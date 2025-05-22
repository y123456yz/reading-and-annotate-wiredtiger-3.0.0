/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

static int __evict_clear_all_walks(WT_SESSION_IMPL *);
static int WT_CDECL __evict_lru_cmp(const void *, const void *);
static int __evict_lru_pages(WT_SESSION_IMPL *, bool);
static int __evict_lru_walk(WT_SESSION_IMPL *);
static int __evict_page(WT_SESSION_IMPL *, bool);
static int __evict_pass(WT_SESSION_IMPL *);
static int __evict_server(WT_SESSION_IMPL *, bool *);
static void __evict_tune_workers(WT_SESSION_IMPL *session);
static int __evict_walk(WT_SESSION_IMPL *, WT_EVICT_QUEUE *);
static int __evict_walk_tree(WT_SESSION_IMPL *, WT_EVICT_QUEUE *, u_int, u_int *);

//大于1说明至少有两个evict线程，其中一个为工作线程
#define WT_EVICT_HAS_WORKERS(s) (S2C(s)->evict_threads.current_threads > 1)

/*
 * __evict_lock_handle_list --
 *     Try to get the handle list lock, with yield and sleep back off. Keep timing statistics
 *     overall.
 */
static int
__evict_lock_handle_list(WT_SESSION_IMPL *session)
{
    WT_CACHE *cache;
    WT_CONNECTION_IMPL *conn;
    WT_DECL_RET;
    WT_RWLOCK *dh_lock;
    u_int spins;

    conn = S2C(session);
    cache = conn->cache;
    dh_lock = &conn->dhandle_lock;

    /*
     * Use a custom lock acquisition back off loop so the eviction server notices any interrupt
     * quickly.
     */
    for (spins = 0; (ret = __wt_try_readlock(session, dh_lock)) == EBUSY && cache->pass_intr == 0;
         spins++) {
        if (spins < WT_THOUSAND)
            __wt_yield();
        else
            __wt_sleep(0, WT_THOUSAND);
    }
    return (ret);
}

/*
 * __evict_entry_priority --
 *     Get the adjusted read generation for an eviction entry.
 evict page评分
 //__evict_lru_walk中使用，评分越高，越不会reconcile到磁盘
 */
static inline uint64_t
__evict_entry_priority(WT_SESSION_IMPL *session, WT_REF *ref)
{
    WT_BTREE *btree;
    WT_PAGE *page;
    uint64_t read_gen;

    btree = S2BT(session);
    page = ref->page;

    /* Any page set to the oldest generation should be discarded. */
    //注意直接评分为WT_READGEN_OLDEST
    //评分很低的说明越久没访问了，直接WT_READGEN_OLDEST表示里面evict,生效见__evict_lru_walk
    if (WT_READGEN_EVICT_SOON(page->read_gen))
        return (WT_READGEN_OLDEST);

    /* Any page from a dead tree is a great choice. */
    if (F_ISSET(btree->dhandle, WT_DHANDLE_DEAD))
        return (WT_READGEN_OLDEST);

    /* Any empty page (leaf or internal), is a good choice. */
    if (__wt_page_is_empty(page))
        return (WT_READGEN_OLDEST);

    /* Any large page in memory is likewise a good choice. */
    if (page->memory_footprint > btree->splitmempage)
        return (WT_READGEN_OLDEST);

    /*
     * The base read-generation is skewed by the eviction priority. Internal pages are also
     * adjusted, we prefer to evict leaf pages.
     */
    if (page->modify != NULL && F_ISSET(S2C(session)->cache, WT_CACHE_EVICT_DIRTY) &&
      !F_ISSET(S2C(session)->cache, WT_CACHE_EVICT_CLEAN))
        read_gen = page->modify->update_txn;
    else
        read_gen = page->read_gen;

    //普通数据表为0，meta元数据文件需要缓存到内存中该值为10000，最大化存入内存中
    read_gen += btree->evict_priority;

#define WT_EVICT_INTL_SKEW 1000
    if (F_ISSET(ref, WT_REF_FLAG_INTERNAL))
        read_gen += WT_EVICT_INTL_SKEW;

    return (read_gen);
}

/*
 * __evict_lru_cmp_debug --
 *     Qsort function: sort the eviction array. Version for eviction debug mode.
 */
static int WT_CDECL
__evict_lru_cmp_debug(const void *a_arg, const void *b_arg)
{
    const WT_EVICT_ENTRY *a, *b;
    uint64_t a_score, b_score;

    a = a_arg;
    b = b_arg;
    a_score = (a->ref == NULL ? UINT64_MAX : 0);
    b_score = (b->ref == NULL ? UINT64_MAX : 0);

    return ((a_score < b_score) ? -1 : (a_score == b_score) ? 0 : 1);
}

/*
 * __evict_lru_cmp --
 *     Qsort function: sort the eviction array.
 参考https://www.cnblogs.com/laizhenghong2012/p/8442270.html
 */
//从小到大排序
static int WT_CDECL
__evict_lru_cmp(const void *a_arg, const void *b_arg)
{
    const WT_EVICT_ENTRY *a, *b;
    uint64_t a_score, b_score;

    a = a_arg;
    b = b_arg;
    a_score = (a->ref == NULL ? UINT64_MAX : a->score);
    b_score = (b->ref == NULL ? UINT64_MAX : b->score);

    return ((a_score < b_score) ? -1 : (a_score == b_score) ? 0 : 1);
}

/*
 * __evict_list_clear --
 *     Clear an entry in the LRU eviction list.
 //标识队列中的这个evict成员为WT_PAGE_EVICT_LRU了
 */
static inline void
__evict_list_clear(WT_SESSION_IMPL *session, WT_EVICT_ENTRY *e)
{
    if (e->ref != NULL) {
        WT_ASSERT(session, F_ISSET_ATOMIC_16(e->ref->page, WT_PAGE_EVICT_LRU));
        F_CLR_ATOMIC_16(e->ref->page, WT_PAGE_EVICT_LRU);
    }
    e->ref = NULL;
    e->btree = WT_DEBUG_POINT;
}

/*
 * __wt_evict_list_clear_page --
 *     Make sure a page is not in the LRU eviction list. This called from the page eviction code to
 *     make sure there is no attempt to evict a child page multiple times.
 */ //在evict队列中摘除该ref对应的page
void
__wt_evict_list_clear_page(WT_SESSION_IMPL *session, WT_REF *ref)
{
    WT_CACHE *cache;
    WT_EVICT_ENTRY *evict;
    uint32_t i, elem, q;
    bool found;

    WT_ASSERT(session, __wt_ref_is_root(ref) || ref->state == WT_REF_LOCKED);

    /* Fast path: if the page isn't on the queue, don't bother searching. */
    if (!F_ISSET_ATOMIC_16(ref->page, WT_PAGE_EVICT_LRU))
        return;

    cache = S2C(session)->cache;
    __wt_spin_lock(session, &cache->evict_queue_lock);

    found = false;
    for (q = 0; q < WT_EVICT_QUEUE_MAX && !found; q++) {
        __wt_spin_lock(session, &cache->evict_queues[q].evict_lock);
        //yang add todo xxxxxxxxxxx ，是不是用evict_candidate会更好
        elem = cache->evict_queues[q].evict_max;
        for (i = 0, evict = cache->evict_queues[q].evict_queue; i < elem; i++, evict++)
            if (evict->ref == ref) {
                found = true;
                __evict_list_clear(session, evict);
                break;
            }
        __wt_spin_unlock(session, &cache->evict_queues[q].evict_lock);
    }
    WT_ASSERT(session, !F_ISSET_ATOMIC_16(ref->page, WT_PAGE_EVICT_LRU));

    __wt_spin_unlock(session, &cache->evict_queue_lock);
}

/*
 * __evict_queue_empty --
 *     Is the queue empty? Note that the eviction server is pessimistic and treats a half full queue
 *     as empty.
 //如果server_check为true，则只要队列中有一半的elem还没有消费处理，则认为是empty的
 //如果server_check为false，当队列中的elem全部消费完了，就认为是empty的
 */
static inline bool
__evict_queue_empty(WT_EVICT_QUEUE *queue,
    //server_check表示是否是evict server主线程
    bool server_check)
{
    uint32_t candidates, used;

    if (queue->evict_current == NULL)
        return (true);

    /* The eviction server only considers half of the candidates. */
    candidates = queue->evict_candidates;
    if (server_check && candidates > 1)
        candidates /= 2;
    used = (uint32_t)(queue->evict_current - queue->evict_queue);
    return (used >= candidates);
}

/*
 * __evict_queue_full --
 *     Is the queue full (i.e., it has been populated with candidates and none of them have been
 *     evicted yet)?
 */
static inline bool
__evict_queue_full(WT_EVICT_QUEUE *queue)
{
    //evict_current指向队列头，并且队列不为空，说明队列中还没有淘汰任何一个elem
    return (queue->evict_current == queue->evict_queue && queue->evict_candidates != 0);
}

/*
 * __wt_evict_server_wake --
 *     Wake the eviction server thread.

//用户线程发送evict_cond信号: __wt_cache_eviction_check->__wt_cache_eviction_worker->__wt_evict_server_wake
//evict server线程发送evict_cond信号: //__evict_walk_tree->__wt_page_evict_urgent->__wt_evict_server_wake
 */
void
__wt_evict_server_wake(WT_SESSION_IMPL *session)
{
    WT_CACHE *cache;
    WT_CONNECTION_IMPL *conn;

    conn = S2C(session);
    cache = conn->cache;

    if (WT_VERBOSE_LEVEL_ISSET(session, WT_VERB_EVICTSERVER, WT_VERBOSE_DEBUG_2)) {
        uint64_t bytes_dirty, bytes_inuse, bytes_max, bytes_updates;

        bytes_inuse = __wt_cache_bytes_inuse(cache);
        bytes_max = conn->cache_size;
        //这里建议把dirty update的信息也打印出来
        bytes_inuse = __wt_cache_bytes_inuse(cache);
        bytes_max = conn->cache_size;
        bytes_dirty = __wt_cache_dirty_inuse(cache);
        bytes_updates = __wt_cache_bytes_updates(cache);
        __wt_verbose_debug2(session, WT_VERB_EVICTSERVER,
          "waking: bytes max: %" PRIu64 ", bytes inuse: %" PRIu64 ", bytes dirty: %" PRIu64
          ", bytes updates: %" PRIu64,
          bytes_max, bytes_inuse, bytes_dirty, bytes_updates);
    }
    //printf("yang test ...........__wt_evict_server_wake...................\r\n");
    //如果只有一个evict线程，这时候__wt_evict_thread_run的__wt_cond_auto_wait就靠这里来触发evict线程继续执行
    __wt_cond_signal(session, cache->evict_cond);
}

/*
 * __wt_evict_thread_chk --
 *     Check to decide if the eviction thread should continue running.
 */
bool
__wt_evict_thread_chk(WT_SESSION_IMPL *session)
{
    return (F_ISSET(S2C(session), WT_CONN_EVICTION_RUN));
}

/*
 * __wt_evict_thread_run --
 *     Entry function for an eviction thread. This is called repeatedly from the thread group code
 *     so it does not need to loop itself.
 */
int
__wt_evict_thread_run(WT_SESSION_IMPL *session, WT_THREAD *thread)
{
    WT_CACHE *cache;
    WT_CONNECTION_IMPL *conn;
    WT_DECL_RET;
    bool did_work, was_intr;

    conn = S2C(session);
    cache = conn->cache;

    /* Mark the session as an eviction thread session. */
    F_SET(session, WT_SESSION_EVICTION);

    /*
     * Cache a history store cursor to avoid deadlock: if an eviction thread marks a file busy and
     * then opens a different file (in this case, the HS file), it can deadlock with a thread
     * waiting for the first file to drain from the eviction queue. See WT-5946 for details.
     */
    WT_ERR(__wt_curhs_cache(session));
    //__wt_evict_thread_run多个evict线程竞争evict_pass_lock锁，只会有一个线程走__evict_server这个分支，其他线程走__evict_lru_pages分支
    if (conn->evict_server_running && __wt_spin_trylock(session, &cache->evict_pass_lock) == 0) {
        /*
         * Cannot use WT_WITH_PASS_LOCK because this is a try lock. Fix when that is supported. We
         * set the flag on both sessions because we may call clear_walk when we are walking with the
         * walk session, locked.
         */
        FLD_SET(session->lock_flags, WT_SESSION_LOCKED_PASS);
        FLD_SET(cache->walk_session->lock_flags, WT_SESSION_LOCKED_PASS);
        ret = __evict_server(session, &did_work);
        FLD_CLR(cache->walk_session->lock_flags, WT_SESSION_LOCKED_PASS);
        FLD_CLR(session->lock_flags, WT_SESSION_LOCKED_PASS);
        was_intr = cache->pass_intr != 0;
        __wt_spin_unlock(session, &cache->evict_pass_lock);
        WT_ERR(ret);

        /*
         * If the eviction server was interrupted, wait until requests have been processed: the
         * system may otherwise be busy so don't go to sleep.
         */
        if (was_intr)
            while (cache->pass_intr != 0 && F_ISSET(conn, WT_CONN_EVICTION_RUN) &&
              F_ISSET(thread, WT_THREAD_RUN))
                __wt_yield();
        else {
            __wt_verbose_debug2(session, WT_VERB_EVICTSERVER, "%s", "__wt_evict_thread_run sleeping");

            /* Don't rely on signals: check periodically. */
            //printf("yang test .............__wt_evict_thread_run.....1..............\r\n");
            __wt_cond_auto_wait(session, cache->evict_cond, did_work, NULL);
           // printf("yang test .............__wt_evict_thread_run.....2..............\r\n");
            __wt_verbose_debug2(session, WT_VERB_EVICTSERVER, "%s", "__wt_evict_thread_run waking");
        }
    } else
        WT_ERR(__evict_lru_pages(session, false));

    if (0) {
err:
        WT_RET_PANIC(session, ret, "cache eviction thread error");
    }
    return (ret);
}

/*
 * __wt_evict_thread_stop --
 *     Shutdown function for an eviction thread.
 */
int
__wt_evict_thread_stop(WT_SESSION_IMPL *session, WT_THREAD *thread)
{
    WT_CACHE *cache;
    WT_CONNECTION_IMPL *conn;
    WT_DECL_RET;

    if (thread->id != 0)
        return (0);

    conn = S2C(session);
    cache = conn->cache;
    /*
     * The only time the first eviction thread is stopped is on shutdown: in case any trees are
     * still open, clear all walks now so that they can be closed.
     */
    WT_WITH_PASS_LOCK(session, ret = __evict_clear_all_walks(session));
    WT_ERR(ret);
    /*
     * The only cases when the eviction server is expected to stop are when recovery is finished,
     * when the connection is closing or when an error has occurred and connection panic flag is
     * set.
     */
    WT_ASSERT(session, F_ISSET(conn, WT_CONN_CLOSING | WT_CONN_PANIC | WT_CONN_RECOVERING));

    /* Clear the eviction thread session flag. */
    F_CLR(session, WT_SESSION_EVICTION);

    __wt_verbose(session, WT_VERB_EVICTSERVER, "%s", "cache eviction thread exiting");

    if (0) {
err:
        WT_RET_PANIC(session, ret, "cache eviction thread error");
    }
    return (ret);
}

/*
 * __evict_server --
 *     Thread to evict pages from the cache.

//evict worker线程: __wt_evict_thread_run->__evict_server->__evict_pass->__evict_lru_walk->__evict_walk->__evict_walk_tree
//worker线程: __wt_evict_thread_run->__evict_lru_pages->__evict_page

 //__wt_evict_thread_run多个evict线程竞争evict_pass_lock锁，只会有一个线程走__evict_server这个分支，其他线程走__evict_lru_pages分支

 */
static int
__evict_server(WT_SESSION_IMPL *session, bool *did_work)
{
    struct timespec now;
    WT_CACHE *cache;
    WT_CONNECTION_IMPL *conn;
    WT_DECL_RET;
    uint64_t time_diff_ms;

    /* Assume there has been no progress. */
    *did_work = false;

    //printf("yang test ............__evict_server.................. \r\n");
    conn = S2C(session);
    cache = conn->cache;

    /* Evict pages from the cache as needed. */
    WT_RET(__evict_pass(session));

    if (!F_ISSET(conn, WT_CONN_EVICTION_RUN) || cache->pass_intr != 0)
        return (0);

    if (!__wt_cache_stuck(session)) {
        /*
         * Try to get the handle list lock: if we give up, that indicates a session is waiting for
         * us to clear walks. Do that as part of a normal pass (without the handle list lock) to
         * avoid deadlock.
         */
        if ((ret = __evict_lock_handle_list(session)) == EBUSY)
            return (0);
        WT_RET(ret);

        /*
         * Clear the walks so we don't pin pages while asleep, otherwise we can block applications
         * evicting large pages.
         */
        //实际上就是把遍历到的表的记录位置置为NULL, btree->evict_ref = NULL,同时让当前evict_ref记录的page直接evict落盘
        ret = __evict_clear_all_walks(session);

        __wt_readunlock(session, &conn->dhandle_lock);
        WT_RET(ret);

        /* Make sure we'll notice next time we're stuck. */
        cache->last_eviction_progress = 0;
        return (0);
    }

    /* Track if work was done. */
    //如果evict server线程两轮运行期间的evict落盘的page数没有变化，说明evict阻塞了
    *did_work = cache->eviction_progress != cache->last_eviction_progress;
    cache->last_eviction_progress = cache->eviction_progress;

    /* Eviction is stuck, check if we have made progress. */
    if (*did_work) {
#if !defined(HAVE_DIAGNOSTIC)
        /* Need verbose check only if not in diagnostic build */
        if (WT_VERBOSE_ISSET(session, WT_VERB_EVICT_STUCK))
#endif
            __wt_epoch(session, &cache->stuck_time);
        return (0);
    }

    //说明evict阻塞了

#if !defined(HAVE_DIAGNOSTIC)
    /* Need verbose check only if not in diagnostic build */
    if (!WT_VERBOSE_ISSET(session, WT_VERB_EVICT_STUCK))
        return (0);
#endif
    /*
     * If we're stuck for 5 minutes in diagnostic mode, or the verbose evict_stuck flag is
     * configured, log the cache and transaction state.
     *
     * If we're stuck for 5 minutes in diagnostic mode, give up.
     *
     * We don't do this check for in-memory workloads because application threads are not blocked by
     * the cache being full. If the cache becomes full of clean pages, we can be servicing reads
     * while the cache appears stuck to eviction.
     */
    if (F_ISSET(conn, WT_CONN_IN_MEMORY))
        return (0);

    __wt_epoch(session, &now);

#define WT_CACHE_STUCK_TIMEOUT_MS 300000
    time_diff_ms = WT_TIMEDIFF_MS(now, cache->stuck_time);

#ifdef HAVE_DIAGNOSTIC
    /* Enable extra logs 20ms before timing out. */
    if (time_diff_ms > WT_CACHE_STUCK_TIMEOUT_MS - 20) {
        //printf("yang test ........__evict_server.............WT_VERB_EVICT:%d\r\n", S2C(session)->verbose[WT_VERB_EVICT]);
        if (S2C(session)->verbose[WT_VERB_EVICT] > WT_VERBOSE_DEBUG_1)
            WT_SET_VERBOSE_LEVEL(session, WT_VERB_EVICT, WT_VERBOSE_DEBUG_1);
        if (S2C(session)->verbose[WT_VERB_EVICTSERVER] > WT_VERBOSE_DEBUG_1)
            WT_SET_VERBOSE_LEVEL(session, WT_VERB_EVICTSERVER, WT_VERBOSE_DEBUG_1);
        if (S2C(session)->verbose[WT_VERB_EVICT_STUCK] > WT_VERBOSE_DEBUG_1)
            WT_SET_VERBOSE_LEVEL(session, WT_VERB_EVICT_STUCK, WT_VERBOSE_DEBUG_1);
    }
#endif

    if (time_diff_ms > WT_CACHE_STUCK_TIMEOUT_MS) {
#ifdef HAVE_DIAGNOSTIC
        __wt_err(session, ETIMEDOUT, "Cache stuck for too long, giving up");
        WT_RET(__wt_verbose_dump_txn(session, "__evict_server"));
        WT_RET(__wt_verbose_dump_cache(session));
        return (__wt_set_return(session, ETIMEDOUT));
#else
        if (WT_VERBOSE_ISSET(session, WT_VERB_EVICT_STUCK)) {
            WT_RET(__wt_verbose_dump_txn(session, "__evict_server"));
            WT_RET(__wt_verbose_dump_cache(session));

            /* Reset the timer. */
            __wt_epoch(session, &cache->stuck_time);
        }
#endif
    }
    return (0);
}

/*
 * __wt_evict_create --
 *     Start the eviction server.
 */
int
__wt_evict_create(WT_SESSION_IMPL *session)
{
    WT_CONNECTION_IMPL *conn;
    uint32_t session_flags;

    conn = S2C(session);

    /*
     * In case recovery has allocated some transaction IDs, bump to the current state. This will
     * prevent eviction threads from pinning anything as they start up and read metadata in order to
     * open cursors.
     */
    WT_RET(__wt_txn_update_oldest(session, WT_TXN_OLDEST_STRICT | WT_TXN_OLDEST_WAIT));

    WT_ASSERT(session, conn->evict_threads_min > 0);
    /* Set first, the thread might run before we finish up. */
    F_SET(conn, WT_CONN_EVICTION_RUN);

    /*
     * Create the eviction thread group. Set the group size to the maximum allowed sessions.
     */
    session_flags = WT_THREAD_CAN_WAIT | WT_THREAD_PANIC_FAIL;
    WT_RET(__wt_thread_group_create(session, &conn->evict_threads, "eviction-server",
      conn->evict_threads_min, conn->evict_threads_max, session_flags, __wt_evict_thread_chk,
      __wt_evict_thread_run, __wt_evict_thread_stop));

/*
 * Ensure the cache stuck timer is initialized when starting eviction.
 */
#if !defined(HAVE_DIAGNOSTIC)
    /* Need verbose check only if not in diagnostic build */
    if (WT_VERBOSE_ISSET(session, WT_VERB_EVICTSERVER))
#endif
        __wt_epoch(session, &conn->cache->stuck_time);

    /*
     * Allow queues to be populated now that the eviction threads are running.
     */
    conn->evict_server_running = true;

    return (0);
}

/*
 * __wt_evict_destroy --
 *     Destroy the eviction threads.
 */
int
__wt_evict_destroy(WT_SESSION_IMPL *session)
{
    WT_CONNECTION_IMPL *conn;

    conn = S2C(session);

    /* We are done if the eviction server didn't start successfully. */
    if (!conn->evict_server_running)
        return (0);

    /* Wait for any eviction thread group changes to stabilize. */
    __wt_writelock(session, &conn->evict_threads.lock);

    /*
     * Signal the threads to finish and stop populating the queue.
     */
    F_CLR(conn, WT_CONN_EVICTION_RUN);
    conn->evict_server_running = false;
    __wt_evict_server_wake(session);

    __wt_verbose(session, WT_VERB_EVICTSERVER, "%s", "waiting for helper threads");

    /*
     * We call the destroy function still holding the write lock. It assumes it is called locked.
     */
    WT_RET(__wt_thread_group_destroy(session, &conn->evict_threads));

    return (0);
}

/*
 * __evict_update_work --
 *     Configure eviction work state.
 //根据内存使用情况计算cache->flags值，确保总内存、脏数据、update使用情况，任一个状态置位都返回true
 */
static bool
__evict_update_work(WT_SESSION_IMPL *session)
{
    WT_BTREE *hs_tree;
    WT_CACHE *cache;
    WT_CONNECTION_IMPL *conn;
    double dirty_target, dirty_trigger, target, trigger, updates_target, updates_trigger;
    uint64_t bytes_dirty, bytes_inuse, bytes_max, bytes_updates;
    uint32_t flags;

    conn = S2C(session);
    cache = conn->cache;

    dirty_target = __wt_eviction_dirty_target(cache);
    dirty_trigger = cache->eviction_dirty_trigger;
    target = cache->eviction_target;
    trigger = cache->eviction_trigger;
    updates_target = cache->eviction_updates_target;
    updates_trigger = cache->eviction_updates_trigger;

    /* Build up the new state. */
    flags = 0;

    if (!F_ISSET(conn, WT_CONN_EVICTION_RUN)) {
        cache->flags = 0;
        return (false);
    }

    if (!__evict_queue_empty(cache->evict_urgent_queue, false))
        LF_SET(WT_CACHE_EVICT_URGENT);

    /*
     * TODO: We are caching the cache usage values associated with the history store because the
     * history store dhandle isn't always available to eviction. Keeping potentially out-of-date
     * values could lead to surprising bugs in the future.
     */
    if (F_ISSET(conn, WT_CONN_HS_OPEN) && __wt_hs_get_btree(session, &hs_tree) == 0) {
        cache->bytes_hs = hs_tree->bytes_inmem;
        cache->bytes_hs_dirty = hs_tree->bytes_dirty_intl + hs_tree->bytes_dirty_leaf;
    }

    /*
     * If we need space in the cache, try to find clean pages to evict.
     *
     * Avoid division by zero if the cache size has not yet been set in a shared cache.
     */
    bytes_max = conn->cache_size + 1;
    bytes_inuse = __wt_cache_bytes_inuse(cache);
    
    //判断cache内存使用占比是否超过了总内存的eviction_trigger(默认95%)
    if (__wt_eviction_clean_needed(session, NULL))
        LF_SET(WT_CACHE_EVICT_CLEAN | WT_CACHE_EVICT_CLEAN_HARD);
    //总内存消耗在eviction_target--eviction_trigger之间
    else if (bytes_inuse > (target * bytes_max) / 100)
        LF_SET(WT_CACHE_EVICT_CLEAN);

    bytes_dirty = __wt_cache_dirty_leaf_inuse(cache);
    //leaf page脏数据内存超过了总内存eviction_dirty_trigger(默认20%)
    if (__wt_eviction_dirty_needed(session, NULL))
        LF_SET(WT_CACHE_EVICT_DIRTY | WT_CACHE_EVICT_DIRTY_HARD);
    else if (bytes_dirty > (uint64_t)(dirty_target * bytes_max) / 100)
        LF_SET(WT_CACHE_EVICT_DIRTY);

    bytes_updates = __wt_cache_bytes_updates(cache);
    if (__wt_eviction_updates_needed(session, NULL))
        LF_SET(WT_CACHE_EVICT_UPDATES | WT_CACHE_EVICT_UPDATES_HARD);
    else if (bytes_updates > (uint64_t)(updates_target * bytes_max) / 100)
        LF_SET(WT_CACHE_EVICT_UPDATES);

    /*
     * If application threads are blocked by the total volume of data in cache, try dirty pages as
     * well.
     */
    if (__wt_cache_aggressive(session) && LF_ISSET(WT_CACHE_EVICT_CLEAN_HARD))
        LF_SET(WT_CACHE_EVICT_DIRTY);

 //   printf("yang test ..........__evict_update_work............target:%d, trigger:%d, %lu\r\n",
  //      (int)dirty_target, (int)dirty_trigger, (uint64_t)((dirty_target + dirty_trigger) * bytes_max));

    /*
     * Scrub dirty pages and keep them in cache if we are less than half way to the clean, dirty or
     * updates triggers.
     例如如果已使用内存占比总内存不超过(target + trigger)配置的一半，则设置标识WT_CACHE_EVICT_SCRUB，说明reconcile的适合可以内存拷贝一份page数据存入image
     //最终该标识影响reconcile持久化的时候释放需要拷贝一份持久化的page内容到内存中
     */
    //yang test ................... bytes_max:1048577, target:80.000000, trigger:95.000000
    //printf("yang test ................... bytes_max:%lu, target:%f, trigger:%f\r\n",  bytes_max, target, trigger);
    if (bytes_inuse < (uint64_t)((target + trigger) * bytes_max) / 200) {
       //__wt_verbose(
       // session, WT_VERB_EVICT, "yang test ....2...__evict_update_work.........bytes_inuse:%lu, %lu", bytes_inuse,
       //     (uint64_t)((target + trigger) * bytes_max) / 200);
            
        if (bytes_dirty < (uint64_t)((dirty_target + dirty_trigger) * bytes_max) / 200 &&
          bytes_updates < (uint64_t)((updates_target + updates_trigger) * bytes_max) / 200)
            LF_SET(WT_CACHE_EVICT_SCRUB);
    } else
        LF_SET(WT_CACHE_EVICT_NOKEEP);

    if (FLD_ISSET(conn->debug_flags, WT_CONN_DEBUG_UPDATE_RESTORE_EVICT)) {
        LF_SET(WT_CACHE_EVICT_SCRUB);
        LF_CLR(WT_CACHE_EVICT_NOKEEP);
    }

    /*
     * With an in-memory cache, we only do dirty eviction in order to scrub pages.
     */
    if (F_ISSET(conn, WT_CONN_IN_MEMORY)) {
        if (LF_ISSET(WT_CACHE_EVICT_CLEAN))
            LF_SET(WT_CACHE_EVICT_DIRTY);
        if (LF_ISSET(WT_CACHE_EVICT_CLEAN_HARD))
            LF_SET(WT_CACHE_EVICT_DIRTY_HARD);
        LF_CLR(WT_CACHE_EVICT_CLEAN | WT_CACHE_EVICT_CLEAN_HARD);
    }

    /* Update the global eviction state. */
    cache->flags = flags;

    return (F_ISSET(cache, WT_CACHE_EVICT_ALL | WT_CACHE_EVICT_URGENT));
}

/*
 * __evict_pass --
 *     Evict pages from memory.

 //evict worker线程: __wt_evict_thread_run->__evict_server->__evict_pass->__evict_lru_walk->__evict_walk->__evict_walk_tree
//worker线程: __wt_evict_thread_run->__evict_lru_pages->__evict_page

 __evict_server
 */
static int
__evict_pass(WT_SESSION_IMPL *session)
{
    WT_CACHE *cache;
    WT_CONNECTION_IMPL *conn;
    WT_TXN_GLOBAL *txn_global;
    uint64_t eviction_progress, oldest_id, prev_oldest_id;
    uint64_t time_now, time_prev;
    u_int loop;

    conn = S2C(session);
    cache = conn->cache;
    txn_global = &conn->txn_global;
    time_prev = 0; /* [-Wconditional-uninitialized] */

    /* Track whether pages are being evicted and progress is made. */
    eviction_progress = cache->eviction_progress;
    prev_oldest_id = txn_global->oldest_id;

    /* Evict pages from the cache. */
    for (loop = 0; cache->pass_intr == 0; loop++) {
        time_now = __wt_clock(session);
        if (loop == 0)
            time_prev = time_now;

        //自动动态调整evict线程数
        __evict_tune_workers(session);
        /*
         * Increment the shared read generation. Do this occasionally even if eviction is not
         * currently required, so that pages have some relative read generation when the eviction
         * server does need to do some work.
         */
        __wt_cache_read_gen_incr(session);
        ++cache->evict_pass_gen;

        /*
         * Update the oldest ID: we use it to decide whether pages are candidates for eviction.
         * Without this, if all threads are blocked after a long-running transaction (such as a
         * checkpoint) completes, we may never start evicting again.
         *
         * Do this every time the eviction server wakes up, regardless of whether the cache is full,
         * to prevent the oldest ID falling too far behind. Don't wait to lock the table: with
         * highly threaded workloads, that creates a bottleneck.
         */
        WT_RET(__wt_txn_update_oldest(session, WT_TXN_OLDEST_STRICT));

        //根据内存使用情况计算cache->flags值，确保总内存、脏数据、update使用情况，任一个状态置位都返回true
        if (!__evict_update_work(session))
            break;

        __wt_verbose_debug2(session, WT_VERB_EVICTSERVER,
          "Eviction pass with: Max: %" PRIu64 " In use: %" PRIu64 " Dirty: %" PRIu64
          " Updates: %" PRIu64,
          conn->cache_size, cache->bytes_inmem, cache->bytes_dirty_intl + cache->bytes_dirty_leaf,
          cache->bytes_updates);

        if (F_ISSET(cache, WT_CACHE_EVICT_ALL))
            WT_RET(__evict_lru_walk(session));

        /*
         * If the queue has been empty recently, keep queuing more pages to evict. If the rate of
         * queuing pages is high enough, this score will go to zero, in which case the eviction
         * server might as well help out with eviction.
         *
         * Also, if there is a single eviction server thread with no workers, it must service the
         * urgent queue in case all application threads are busy.
         */
        //只配置有一个evict线程并且紧急队列不为空或者队列空的评分很低，则由自己进行真正的evict reconcile
        if (!WT_EVICT_HAS_WORKERS(session) &&
          (cache->evict_empty_score < WT_EVICT_SCORE_CUTOFF ||
            !__evict_queue_empty(cache->evict_urgent_queue, false)))
            WT_RET(__evict_lru_pages(session, true));

        if (cache->pass_intr != 0)
            break;

        /*
         * If we're making progress, keep going; if we're not making any progress at all, mark the
         * cache "stuck" and go back to sleep, it's not something we can fix.
         *
         * We check for progress every 20ms, the idea being that the aggressive score will reach 10
         * after 200ms if we aren't making progress and eviction will start considering more pages.
         * If there is still no progress after 2s, we will treat the cache as stuck and start
         * rolling back transactions and writing updates to the history store table.
         */
        //判断本轮page挑选期间是否有page落盘成功
        if (eviction_progress == cache->eviction_progress) {
            if (WT_CLOCKDIFF_MS(time_now, time_prev) >= 20 && F_ISSET(cache, WT_CACHE_EVICT_HARD)) {
                if (cache->evict_aggressive_score < 100)
                    ++cache->evict_aggressive_score;
                oldest_id = txn_global->oldest_id;

                if (prev_oldest_id == oldest_id && txn_global->current != oldest_id &&
                  cache->evict_aggressive_score < 100)
                    ++cache->evict_aggressive_score;
                time_prev = time_now;
                prev_oldest_id = oldest_id;
            }

            /*
             * Keep trying for long enough that we should be able to evict a page if the server
             * isn't interfering.
             */
            //2秒钟内，我们进入这里等待evict cond被唤醒
            if (loop < 100 || cache->evict_aggressive_score < 100) {
                /*
                 * Back off if we aren't making progress: walks hold the handle list lock, blocking
                 * other operations that can free space in cache, such as LSM discarding handles.
                 *
                 * Allow this wait to be interrupted (e.g. if a checkpoint completes): make sure we
                 * wait for a non-zero number of microseconds).
                 */
                WT_STAT_CONN_INCR(session, cache_eviction_server_slept);
                //printf("yang test .............__evict_pass.............1......\r\n");
                __wt_cond_wait(session, cache->evict_cond, WT_THOUSAND, NULL);
                //printf("yang test .............__evict_pass..............2.....\r\n");
                continue;
            }

            //如果evict_aggressive_score到达100，说明经历了100个20S的该循环中通过__evict_lru_walk挑选evict page
            // 这时候在外层__evict_server->__wt_cache_stuck判断中进行回滚操作

            //如果持续2秒没有page淘汰到磁盘，则直接跳过本轮evict page挑选
            WT_STAT_CONN_INCR(session, cache_eviction_slow);
            __wt_verbose(session, WT_VERB_EVICTSERVER, "%s", "unable to reach eviction goal");
            break;
        }

        //如果有page实现了evict，则阻塞评分减少
        if (cache->evict_aggressive_score > 0)
            --cache->evict_aggressive_score;

        loop = 0;
        eviction_progress = cache->eviction_progress;
    }
    return (0);
}

/*
 * __evict_clear_walk --
 *     Clear a single walk point.
 //实际上就是把遍历到的表的记录位置置为NULL, btree->evict_ref = NULL,同时让当前evict_ref记录的page直接evict落盘
 */
static int
__evict_clear_walk(WT_SESSION_IMPL *session)
{
    WT_BTREE *btree;
    WT_CACHE *cache;
    WT_DECL_RET;
    WT_REF *ref;

    btree = S2BT(session);
    cache = S2C(session)->cache;

    WT_ASSERT(session, FLD_ISSET(session->lock_flags, WT_SESSION_LOCKED_PASS));
    if (session->dhandle == cache->walk_tree)
        cache->walk_tree = NULL;

    if ((ref = btree->evict_ref) == NULL)
        return (0);

    WT_STAT_CONN_INCR(session, cache_eviction_walks_abandoned);
    WT_STAT_DATA_INCR(session, cache_eviction_walks_abandoned);

    /*
     * Clear evict_ref before releasing it in case that forces eviction (we assert that we never try
     * to evict the current eviction walk point).
     */
    //evict后台线程就找不到这个btree点位了
    btree->evict_ref = NULL;

    WT_WITH_DHANDLE(cache->walk_session, session->dhandle,
      (ret = __wt_page_release(cache->walk_session, ref, WT_READ_NO_EVICT)));
    return (ret);
}

/*
 * __evict_clear_all_walks --
 *     Clear the eviction walk points for all files a session is waiting on.
 */
static int
__evict_clear_all_walks(WT_SESSION_IMPL *session)
{
    WT_CONNECTION_IMPL *conn;
    WT_DATA_HANDLE *dhandle;
    WT_DECL_RET;

    conn = S2C(session);

    TAILQ_FOREACH (dhandle, &conn->dhqh, q)
        if (WT_DHANDLE_BTREE(dhandle))
            WT_WITH_DHANDLE(session, dhandle, WT_TRET(__evict_clear_walk(session)));
    return (ret);
}

/*
 * __wt_evict_file_exclusive_on --
 *     Get exclusive eviction access to a file and discard any of the file's blocks queued for
 *     eviction.

 例如做checkpoint，则需要把evict server挑选的page 队列释放掉，因为checkpoint会做一次全量的落盘
 //__wt_session_lock_checkpoint中__wt_evict_file_exclusive_on和__wt_evict_file_exclusive_off配对使用
 */
int
__wt_evict_file_exclusive_on(WT_SESSION_IMPL *session)
{
    WT_BTREE *btree;
    WT_CACHE *cache;
    WT_DECL_RET;
    WT_EVICT_ENTRY *evict;
    u_int i, elem, q;

    btree = S2BT(session);
    cache = S2C(session)->cache;

    /* Hold the walk lock to turn off eviction. */
    __wt_spin_lock(session, &cache->evict_walk_lock);
    //说明已经有其他线程(例如checkpoint线程在__wt_session_lock_checkpoint中也会有这个操作)操作不让evict线程进行evict操作了，这时候就直接返回，
    if (++btree->evict_disabled > 1) {
        __wt_spin_unlock(session, &cache->evict_walk_lock);
        return (0);
    }

    /*
     * Ensure no new pages from the file will be queued for eviction after this point, then clear
     * any existing LRU eviction walk for the file.
     */
    (void)__wt_atomic_addv32(&cache->pass_intr, 1);
    WT_WITH_PASS_LOCK(session, ret = __evict_clear_walk(session));
    (void)__wt_atomic_subv32(&cache->pass_intr, 1);
    WT_ERR(ret);

    /*
     * The eviction candidate list might reference pages from the file, clear it. Hold the evict
     * lock to remove queued pages from a file.
     */
    __wt_spin_lock(session, &cache->evict_queue_lock);

    //该表已经被选入到evict server紧急队列的page从队列中清理掉
    for (q = 0; q < WT_EVICT_QUEUE_MAX; q++) {
        __wt_spin_lock(session, &cache->evict_queues[q].evict_lock);
        elem = cache->evict_queues[q].evict_max;
        for (i = 0, evict = cache->evict_queues[q].evict_queue; i < elem; i++, evict++)
            if (evict->btree == btree)
                __evict_list_clear(session, evict);
        __wt_spin_unlock(session, &cache->evict_queues[q].evict_lock);
    }

    __wt_spin_unlock(session, &cache->evict_queue_lock);

    /*
     * We have disabled further eviction: wait for concurrent LRU eviction activity to drain.
     */
    //等待其他正在对该btree做evict操作的线程完成相关evict操作
    while (btree->evict_busy > 0)
        __wt_yield();

    if (0) {
err:
        --btree->evict_disabled;
    }
    __wt_spin_unlock(session, &cache->evict_walk_lock);
    return (ret);
}

/*
 * __wt_evict_file_exclusive_off --
 *     Release exclusive eviction access to a file.
  //__wt_session_lock_checkpoint中__wt_evict_file_exclusive_on和__wt_evict_file_exclusive_off配对使用
 */
void
__wt_evict_file_exclusive_off(WT_SESSION_IMPL *session)
{
    WT_BTREE *btree;

    btree = S2BT(session);

    /*
     * We have seen subtle bugs with multiple threads racing to turn eviction on/off. Make races
     * more likely in diagnostic builds.
     */
    WT_DIAGNOSTIC_YIELD;

/*
 * Atomically decrement the evict-disabled count, without acquiring the eviction walk-lock. We can't
 * acquire that lock here because there's a potential deadlock. When acquiring exclusive eviction
 * access, we acquire the eviction walk-lock and then the cache's pass-intr lock. The current
 * eviction implementation can hold the pass-intr lock and call into this function (see WT-3303 for
 * the details), which might deadlock with another thread trying to get exclusive eviction access.
 */
#if defined(HAVE_DIAGNOSTIC)
    {
        int32_t v;

        WT_ASSERT(session, btree->evict_ref == NULL);
        v = __wt_atomic_subi32(&btree->evict_disabled, 1);
        WT_ASSERT(session, v >= 0);
    }
#else
    (void)__wt_atomic_subi32(&btree->evict_disabled, 1);
#endif
}

#define EVICT_TUNE_BATCH 1 /* Max workers to add each period */
                           /*
                            * Data points needed before deciding if we should keep adding workers or
                            * settle on an earlier value.
                            */
#define EVICT_TUNE_DATAPT_MIN 8
#define EVICT_TUNE_PERIOD 60 /* Tune period in milliseconds */

/*
 * We will do a fresh re-tune every that many milliseconds to adjust to significant phase changes.
 */
#define EVICT_FORCE_RETUNE 25000

/*
 * __evict_tune_workers --
 *     Find the right number of eviction workers. Gradually ramp up the number of workers increasing
 *     the number in batches indicated by the setting above. Store the number of workers that gave
 *     us the best throughput so far and the number of data points we have tried. Every once in a
 *     while when we have the minimum number of data points we check whether the eviction throughput
 *     achieved with the current number of workers is the best we have seen so far. If so, we will
 *     keep increasing the number of workers. If not, we are past the infliction point on the
 *     eviction throughput curve. In that case, we will set the number of workers to the best
 *     observed so far and settle into a stable state.
 //自动动态调整evict线程数
 */
static void
__evict_tune_workers(WT_SESSION_IMPL *session)
{
    struct timespec current_time;
    WT_CACHE *cache;
    WT_CONNECTION_IMPL *conn;
    uint64_t delta_msec, delta_pages;
    uint64_t eviction_progress, eviction_progress_rate, time_diff;
    int32_t cur_threads, i, target_threads, thread_surplus;

    conn = S2C(session);
    cache = conn->cache;

    /*
     * If we have a fixed number of eviction threads, there is no value in calculating if we should
     * do any tuning.
     */
    if (conn->evict_threads_max == conn->evict_threads_min)
        return;

    __wt_epoch(session, &current_time);
    time_diff = WT_TIMEDIFF_MS(current_time, cache->evict_tune_last_time);

    /*
     * If we have reached the stable state and have not run long enough to surpass the forced
     * re-tuning threshold, return.
     */
    if (cache->evict_tune_stable) {
        if (time_diff < EVICT_FORCE_RETUNE)
            return;

        /*
         * Stable state was reached a long time ago. Let's re-tune. Reset all the state.
         */
        cache->evict_tune_stable = false;
        cache->evict_tune_last_action_time.tv_sec = 0;
        cache->evict_tune_progress_last = 0;
        cache->evict_tune_num_points = 0;
        cache->evict_tune_progress_rate_max = 0;

        /* Reduce the number of eviction workers by one */
        thread_surplus =
          (int32_t)conn->evict_threads.current_threads - (int32_t)conn->evict_threads_min;

        if (thread_surplus > 0) {
            __wt_thread_group_stop_one(session, &conn->evict_threads);
            WT_STAT_CONN_INCR(session, cache_eviction_worker_removed);
        }
        WT_STAT_CONN_INCR(session, cache_eviction_force_retune);
    } else if (time_diff < EVICT_TUNE_PERIOD)
        /*
         * If we have not reached stable state, don't do anything unless enough time has passed
         * since the last time we have taken any action in this function.
         */
        return;

    /*
     * Measure the evicted progress so far. Eviction rate correlates to performance, so this is our
     * metric of success.
     */
    eviction_progress = cache->eviction_progress;

    /*
     * If we have recorded the number of pages evicted at the end of the previous measurement
     * interval, we can compute the eviction rate in evicted pages per second achieved during the
     * current measurement interval. Otherwise, we just record the number of evicted pages and
     * return.
     */
    if (cache->evict_tune_progress_last == 0)
        goto done;

    delta_msec = WT_TIMEDIFF_MS(current_time, cache->evict_tune_last_time);
    delta_pages = eviction_progress - cache->evict_tune_progress_last;
    eviction_progress_rate = (delta_pages * WT_THOUSAND) / delta_msec;
    cache->evict_tune_num_points++;

    /*
     * Keep track of the maximum eviction throughput seen and the number of workers corresponding to
     * that throughput.
     */
    if (eviction_progress_rate > cache->evict_tune_progress_rate_max) {
        cache->evict_tune_progress_rate_max = eviction_progress_rate;
        cache->evict_tune_workers_best = conn->evict_threads.current_threads;
    }

    /*
     * Compare the current number of data points with the number needed variable. If they are equal,
     * we will check whether we are still going up on the performance curve, in which case we will
     * increase the number of needed data points, to provide opportunity for further increasing the
     * number of workers. Or we are past the inflection point on the curve, in which case we will go
     * back to the best observed number of workers and settle into a stable state.
     */
    if (cache->evict_tune_num_points >= cache->evict_tune_datapts_needed) {
        if (cache->evict_tune_workers_best == conn->evict_threads.current_threads &&
          conn->evict_threads.current_threads < conn->evict_threads_max) {
            /*
             * Keep adding workers. We will check again at the next check point.
             */
            cache->evict_tune_datapts_needed += WT_MIN(EVICT_TUNE_DATAPT_MIN,
              (conn->evict_threads_max - conn->evict_threads.current_threads) / EVICT_TUNE_BATCH);
        } else {
            /*
             * We are past the inflection point. Choose the best number of eviction workers observed
             * and settle into a stable state.
             */
            thread_surplus = (int32_t)conn->evict_threads.current_threads -
              (int32_t)cache->evict_tune_workers_best;

            for (i = 0; i < thread_surplus; i++) {
                __wt_thread_group_stop_one(session, &conn->evict_threads);
                WT_STAT_CONN_INCR(session, cache_eviction_worker_removed);
            }
            cache->evict_tune_stable = true;
            goto done;
        }
    }

    /*
     * If we have not added any worker threads in the past, we set the number of data points needed
     * equal to the number of data points that we must accumulate before deciding if we should keep
     * adding workers or settle on a previously tried stable number of workers.
     */
    if (cache->evict_tune_last_action_time.tv_sec == 0)
        cache->evict_tune_datapts_needed = EVICT_TUNE_DATAPT_MIN;

    if (F_ISSET(cache, WT_CACHE_EVICT_ALL)) {
        cur_threads = (int32_t)conn->evict_threads.current_threads;
        target_threads = WT_MIN(cur_threads + EVICT_TUNE_BATCH, (int32_t)conn->evict_threads_max);
        /*
         * Start the new threads.
         */
        for (i = cur_threads; i < target_threads; ++i) {
            __wt_thread_group_start_one(session, &conn->evict_threads, false);
            WT_STAT_CONN_INCR(session, cache_eviction_worker_created);
            __wt_verbose(session, WT_VERB_EVICTSERVER, "%s", "__evict_tune_workers added worker thread");
        }
        cache->evict_tune_last_action_time = current_time;
    }

done:
    cache->evict_tune_last_time = current_time;
    cache->evict_tune_progress_last = eviction_progress;
}

/*
 * __evict_lru_pages --
 *     Get pages from the LRU queue to evict.
 //evict worker线程: __wt_evict_thread_run->__evict_server->__evict_pass->__evict_lru_walk->__evict_walk->__evict_walk_tree
//worker线程: __wt_evict_thread_run->__evict_lru_pages->__evict_page

 //__wt_evict_thread_run多个evict线程竞争evict_pass_lock锁，只会有一个线程走__evict_server这个分支，其他线程走__evict_lru_pages分支
 //从evict对了获取需要evict page进行淘汰
 */
static int
__evict_lru_pages(WT_SESSION_IMPL *session, 
    //标识是否是evict主线程
    bool is_server)
{
    WT_CONNECTION_IMPL *conn;
    WT_DECL_RET;
    WT_TRACK_OP_DECL;

    WT_TRACK_OP_INIT(session);
    conn = S2C(session);
    //WT_RET(__wt_msg(session, "yang test .............................__evict_lru_pages...................."));

    /*
     * Reconcile and discard some pages: EBUSY is returned if a page fails eviction because it's
     * unavailable, continue in that case.
     */
    while (F_ISSET(conn, WT_CONN_EVICTION_RUN) && ret == 0)
        if ((ret = __evict_page(session, is_server)) == EBUSY)
            ret = 0;

    /* If any resources are pinned, release them now. */
    WT_TRET(__wt_session_release_resources(session));

    /* If a worker thread found the queue empty, pause. */
    if (ret == WT_NOTFOUND && !is_server && F_ISSET(conn, WT_CONN_EVICTION_RUN)) {
        __wt_cond_wait(session, conn->evict_threads.wait_cond, 10000, NULL);
    }
    WT_TRACK_OP_END(session);
    return (ret == WT_NOTFOUND ? 0 : ret);
}

/*
 * __evict_lru_walk --
 *     Add pages to the LRU queue to be evicted from cache.
 //evict worker线程(挑选的page入队): __wt_evict_thread_run->__evict_server->__evict_pass->__evict_lru_walk->__evict_walk->__evict_walk_tree
//worker线程(挑选的page出队进行真正evict): __wt_evict_thread_run->__evict_lru_pages->__evict_page->__evict_get_ref(从队列选page进行evict)
 WT_CACHE_EVICT_ALL满足任何一个的时候才会进来

 为何这里挑选page入队到queue没有加锁，原因是使用了原子操作，参考__evict_push_candidate
 */
static int
__evict_lru_walk(WT_SESSION_IMPL *session)
{
    WT_CACHE *cache;
    WT_DECL_RET;
    WT_EVICT_QUEUE *queue, *other_queue;
    WT_TRACK_OP_DECL;
    uint64_t read_gen_oldest;
    uint32_t candidates, entries;//, used;

    WT_TRACK_OP_INIT(session);
    cache = S2C(session)->cache;

    //printf("yang test .....11..........__evict_lru_walk...............verbos evictserver:%d\r\n", S2C(session)->verbose[WT_VERB_EVICTSERVER]);
    /* Age out the score of how much the queue has been empty recently. */
    if (cache->evict_empty_score > 0)
        --cache->evict_empty_score;

    /* Fill the next queue (that isn't the urgent queue). */
    //__evict_lru_walk挑选的需要evict的page(不包括urgent)添加到fill队列，queue也就是evict_queues[0]
    //这里也就是如果上一轮用的是evict_queues[0]用来存放遍历获取的需要evict的page，那这一轮就用evict_queues[1]来存
    // 如果上一轮用的是evict_queues[1]用来存放遍历获取的需要evict的page，那这一轮就用evict_queues[0]来存
    queue = cache->evict_fill_queue;
    other_queue = cache->evict_queues + (1 - (queue - cache->evict_queues));
    //evict_fill_queue的作用其实就一个，记录本轮evict server用的是queue还是other_queue，这样下一轮的时候就用另一个queue
    // 例如如果上一轮用的是evict_queues[1]用来存放遍历获取的需要evict的page，那这一轮就用evict_queues[0]来存
    cache->evict_fill_queue = other_queue;  

    //printf("yang test .......__evict_lru_walk..........queue:%p, other_queue:%p, evict_fill_queue:%p\r\n",
    //    queue, other_queue, cache->evict_fill_queue);


    /* If this queue is full, try the other one. */
    //evict_queues[0]满了，但是evict_queues[1]还没满，则使用evict_queues[1]
    if (__evict_queue_full(queue) && !__evict_queue_full(other_queue)) {
        queue = other_queue;
    }
    /*
     * If both queues are full and haven't been empty on recent refills, we're done.
     */
    //evict_queues[0]和evict_queues[1]都满了，并且得分较低，说明近期一直满，则不进行后续的扫描需要evict的page来进行入队操作
    if (__evict_queue_full(queue) && cache->evict_empty_score < WT_EVICT_SCORE_CUTOFF) {
        //printf("yang test .......__evict_lru_walk................cache->evict_empty_score:%d\r\n",
        //    (int)cache->evict_empty_score);
        goto err; //yang add todo xxxxxxxxxxxxx 这里最好加个统计，代表队列一直满，或者增加cache_eviction_queue_not_empty
    }

    /*
     * If the queue we are filling is empty, pages are being requested faster than they are being
     * queued.
     */
    //消费速度比入队速度更快，则增加evict_empty_score评分
    if (__evict_queue_empty(queue, false) && __evict_queue_empty(cache->evict_urgent_queue, false)) {
        if (F_ISSET(cache, WT_CACHE_EVICT_HARD))
            cache->evict_empty_score =
              WT_MIN(cache->evict_empty_score + WT_EVICT_SCORE_BUMP, WT_EVICT_SCORE_MAX);
        WT_STAT_CONN_INCR(session, cache_eviction_queue_empty);
    } else
        WT_STAT_CONN_INCR(session, cache_eviction_queue_not_empty);

    //used = (uint32_t)(queue->evict_entries - queue->evict_current);
    /*
     * Get some more pages to consider for eviction.
     *
     * If the walk is interrupted, we still need to sort the queue: the next walk assumes there are
     * no entries beyond WT_EVICT_WALK_BASE.
     //__evict_walk_tree在指定表中查找，__evict_walk遍历所有表查找候选page加入queue队列
     */
    //if (used < WT_EVICT_WALK_BASE) {
        if ((ret = __evict_walk(cache->walk_session, queue)) == EBUSY)
            ret = 0;
        WT_ERR_NOTFOUND_OK(ret, false);
   // }


    //普通queue需要进行下面的处理，evict_urgent_queue下面啥也没做，实际上__wt_page_evict_urgent入队后啥也可以不用做


    /* Sort the list into LRU order and restart. */
    //队列加锁，注意只锁本队列加锁，因为evict worker也会消费该队列进行reconcile
    //注意实际上在不会加锁，因为是原子操作，这样可以提高性能，参考__evict_push_candidate
    __wt_spin_lock(session, &queue->evict_lock);

    /*
     * We have locked the queue: in the (unusual) case where we are filling the current queue, mark
     * it empty so that subsequent requests switch to the other queue.
     */
    //这里清除队列的消费位置是因为后面会对该队列重新排序，排序后evict_current会从队列头部开始重新消费
    if (queue == cache->evict_current_queue)
        queue->evict_current = NULL;

    entries = queue->evict_entries;
    /*
     * Style note: __wt_qsort is a macro that can leave a dangling else. Full curly braces are
     * needed here for the compiler.
     */
    //对队列中的每一个entriy成员评分进行从小到大排序
    if (F_ISSET(cache, WT_CACHE_EVICT_DEBUG_MODE)) {
        __wt_qsort(queue->evict_queue, entries, sizeof(WT_EVICT_ENTRY), __evict_lru_cmp_debug);
    } else {
        __wt_qsort(queue->evict_queue, entries, sizeof(WT_EVICT_ENTRY), __evict_lru_cmp);
    }

    //队列前面可能有消费了的page，因此队列头部可能有很多空page, 排序后，这些空page就到了队列末尾处，需要拆剪掉
    /* Trim empty entries from the end. */
    while (entries > 0 && queue->evict_queue[entries - 1].ref == NULL)
        --entries;

    /*
     * If we have more entries than the maximum tracked between walks, clear them. Do this before
     * figuring out how many of the entries are candidates so we never end up with more candidates
     * than entries.
     */
    while (entries > WT_EVICT_WALK_BASE)
        __evict_list_clear(session, &queue->evict_queue[--entries]);

    queue->evict_entries = entries;

    if (entries == 0) {
        /*
         * If there are no entries, there cannot be any candidates. Make sure application threads
         * don't read past the end of the candidate list, or they may race with the next walk.
         */
        queue->evict_candidates = 0;
        queue->evict_current = NULL;
        __wt_spin_unlock(session, &queue->evict_lock);
        goto err;
    }

    /* Decide how many of the candidates we're going to try and evict. */
    if (__wt_cache_aggressive(session)) //很久都没有reconcile page到磁盘了，则选择队列所有page为候选page
        queue->evict_candidates = entries;
    else {
        /*
         * Find the oldest read generation apart that we have in the queue, used to set the initial
         * value for pages read into the system. The queue is sorted, find the first "normal"
         * generation.
         */
        read_gen_oldest = WT_READGEN_START_VALUE;
        //candidates也就是需要紧急的page, 也就是评分低于WT_READGEN_START_VALUE的page需要进入候选队列
        // 评分越低说明最近越没有被访问
        for (candidates = 0; candidates < entries; ++candidates) {
            read_gen_oldest = queue->evict_queue[candidates].score;
            //也就是评分为WT_READGEN_NOTSET，或者>=WT_READGEN_START_VALUE
            //也就是这一批page中最久没有访问的page对应的read gen
            if (!WT_READGEN_EVICT_SOON(read_gen_oldest))
                break;
        }
      //  printf("yang test ...................entries:%d, candidates:%d, read_gen_oldest:%d\r\n",
       //     (int)entries, (int)candidates, (int)read_gen_oldest);

        /*
         * Take all candidates if we only gathered pages with an oldest
         * read generation set.
         *
         * We normally never take more than 50% of the entries but if
         * 50% of the entries were at the oldest read generation, take
         * all of them.
         */
        //需要立马释放evict_queue[]队列中所有的page都需要立马reconcile
        if (WT_READGEN_EVICT_SOON(read_gen_oldest))
            queue->evict_candidates = entries;
        else if (candidates > entries / 2) //有一大半需要reconcile
            //yang add todo xxxxxxxxxx  这里是不是应该更新cache->read_gen_oldest  read_gen_oldest是从一个queue中取的，另一个queue中也可能有数据，这样的read_gen_oldest会不会不准???
            queue->evict_candidates = candidates;
        else {//小于一半的page需要reconcile
            /*
             * Take all of the urgent pages plus a third of ordinary candidates (which could be
             * expressed as WT_EVICT_WALK_INCR / WT_EVICT_WALK_BASE). In the steady state, we want
             * to get as many candidates as the eviction walk adds to the queue.
             *
             * That said, if there is only one entry, which is normal when populating an empty file,
             * don't exclude it.
             */
            //紧急candidates+1/3的普通候选page，+1的目的是考虑普通候选队列只有1个page的情况
            //yang add todo xxxxxxxxxxxxx   这里最好是MIN(queue->evict_candidates, entries / 2), 否则这个分支candidates比上面分支还大
            queue->evict_candidates = 1 + candidates + ((entries - candidates) - 1) / 3;

            //也就是候选page中第一个>=WT_READGEN_START_VALUE的page的评分
            cache->read_gen_oldest = read_gen_oldest;
        }
    }

    WT_STAT_CONN_INCRV(session, cache_eviction_pages_queued_post_lru, queue->evict_candidates);
    //重新排序，并且去除了末尾无用的elem后，重新指向队列头部
    queue->evict_current = queue->evict_queue;
    __wt_spin_unlock(session, &queue->evict_lock);

    /*
     * Signal any application or helper threads that may be waiting to help with eviction.
     */
   // printf("yang test ....................__evict_lru_walk........end\r\n");
    //通知工作线程开始消费处理
    __wt_cond_signal(session, S2C(session)->evict_threads.wait_cond);

err:
    WT_TRACK_OP_END(session);
    return (ret);
}

/*
 * __evict_walk_choose_dhandle --
 *     Randomly select a dhandle for the next eviction walk
 //从dhhash桶中随机选择一个表
 //yang add todo xxxxxxxxxxxxxxxxxxxxxxxxx   不活跃的表会不会被选择?
 */
static void
__evict_walk_choose_dhandle(WT_SESSION_IMPL *session, WT_DATA_HANDLE **dhandle_p)
{
    WT_CONNECTION_IMPL *conn;
    WT_DATA_HANDLE *dhandle;
    u_int dh_bucket_count, rnd_bucket, rnd_dh;

    conn = S2C(session);

    WT_ASSERT(session, __wt_rwlock_islocked(session, &conn->dhandle_lock));

#undef RANDOM_DH_SELECTION_ENABLED
//默认不定义这个，因此是轮询方式
#ifdef RANDOM_DH_SELECTION_ENABLED  //随机选一个表
    *dhandle_p = NULL;

   // printf("yang test ....1.....__evict_walk_choose_dhandle....................\r\n");
    /*
     * If we don't have many dhandles, most hash buckets will be empty. Just pick a random dhandle
     * from the list in that case.
     */
    //
    if (conn->dhandle_count < conn->dh_hash_size / 4) {
        rnd_dh = __wt_random(&session->rnd) % conn->dhandle_count;
        dhandle = TAILQ_FIRST(&conn->dhqh);
        for (; rnd_dh > 0; rnd_dh--)
            dhandle = TAILQ_NEXT(dhandle, q);
        *dhandle_p = dhandle;
        return;
    }

    /*
     * Keep picking up a random bucket until we find one that is not empty.
     */
    //随机选一个桶中不为空的桶，确定挑选的桶下标
    do {
        //代表hash桶下标
        rnd_bucket = __wt_random(&session->rnd) & (conn->dh_hash_size - 1);
        //dh_bucket_count代表rnd_bucket这个桶中拥有的elem个数
    } while ((dh_bucket_count = conn->dh_bucket_count[rnd_bucket]) == 0);

    /* We can't pick up an empty bucket with a non zero bucket count. */
    WT_ASSERT(session, !TAILQ_EMPTY(&conn->dhhash[rnd_bucket]));

    /* Pick a random dhandle in the chosen bucket. */
    //随机从dhhash[rnd_bucket]这个桶中后去第rnd_dh个elem
    rnd_dh = __wt_random(&session->rnd) % dh_bucket_count;
    dhandle = TAILQ_FIRST(&conn->dhhash[rnd_bucket]);
    for (; rnd_dh > 0; rnd_dh--)
        dhandle = TAILQ_NEXT(dhandle, hashq);
#else  //轮询链表中的表
    /* Just step through dhandles. */
    dhandle = *dhandle_p;
    if (dhandle != NULL)
        dhandle = TAILQ_NEXT(dhandle, q);
    if (dhandle == NULL)
        dhandle = TAILQ_FIRST(&conn->dhqh);

    WT_UNUSED(dh_bucket_count);
    WT_UNUSED(rnd_bucket);
    WT_UNUSED(rnd_dh);
  //  printf("yang test ....2.....__evict_walk_choose_dhandle....................\r\n");
#endif

    *dhandle_p = dhandle;
}

/*
 * __evict_walk --
 *     Fill in the array by walking the next set of pages.
  //evict worker线程: __wt_evict_thread_run->__evict_server->__evict_pass->__evict_lru_walk->__evict_walk->__evict_walk_tree
  //worker线程: __wt_evict_thread_run->__evict_lru_pages->__evict_page

  //__evict_walk_tree在指定表中查找，__evict_walk遍历所有表查找候选page加入queue中
 */
static int
__evict_walk(WT_SESSION_IMPL *session, WT_EVICT_QUEUE *queue)
{
    WT_BTREE *btree;
    WT_CACHE *cache;
    WT_CONNECTION_IMPL *conn;
    WT_DATA_HANDLE *dhandle;
    WT_DECL_RET;
    WT_TRACK_OP_DECL;
    u_int loop_count, max_entries, retries, slot, start_slot;
    u_int total_candidates;
    bool dhandle_locked, incr;

    WT_TRACK_OP_INIT(session);

    conn = S2C(session);
    cache = conn->cache;
    btree = NULL;
    dhandle = NULL;
    dhandle_locked = incr = false;
    retries = 0;

    /*
     * Set the starting slot in the queue and the maximum pages added per walk.
     */
    //新挑选的page添加到slot这个位置
    start_slot = slot = queue->evict_entries;
    //从一个表中一次扫描的需要evict的page数量
    //也就是把queue队列数组填满，还差多少个entry, 队列数组大小为evict_slots，队列中没有evict reconcile的page数不能超过该值
    max_entries = WT_MIN(slot + WT_EVICT_WALK_INCR, cache->evict_slots);

    /*
     * Another pathological case: if there are only a tiny number of candidate pages in cache, don't
     * put all of them on one queue.
     */
    //如果内存中page数很少，就没必要全部候选进行evict
    //如果total_candidates page数量很少，比max_entries还少，如果没有触发dirty，total选择所有page，如果触发了dirty，total选择dirty page
    total_candidates = (u_int)(F_ISSET(cache, WT_CACHE_EVICT_CLEAN | WT_CACHE_EVICT_UPDATES) ?
        __wt_cache_pages_inuse(cache) :
        cache->pages_dirty_leaf);
    max_entries = WT_MIN(max_entries, 1 + total_candidates / 2);

retry: //注意这里有retry
    loop_count = 0;
   // printf("yang test .............__evict_walk...slot:%d, max_entries:%d, conn->dhandle_count:%d, flags:%u\r\n",
    //    (int)slot, (int)max_entries, (int)conn->dhandle_count, conn->flags);
    //有可能都是小表，这时候要遍历获取max_entries个需要evict的page可能要跨越多个表，这时候loop_count代表跨越的表个数
    while (slot < max_entries && loop_count++ < conn->dhandle_count) {
        /* We're done if shutting down or reconfiguring. */
        if (F_ISSET(conn, WT_CONN_CLOSING) || F_ISSET(conn, WT_CONN_RECONFIGURING)) {
            break;
        }

        /*
         * If another thread is waiting on the eviction server to clear the walk point in a tree,
         * give up.
         */
     //例如其他线程做checkpoint，则需要把evict server挑选的page 队列释放掉，因为checkpoint会做一次全量的落盘, 用pass_intr标记其他线程正在
    //对该表做checkpoint，因此evict server可以不用在挑选page入队了
        if (cache->pass_intr != 0)
            WT_ERR(EBUSY);

        /*
         * Lock the dhandle list to find the next handle and bump its reference count to keep it
         * alive while we sweep.
         */
        //对dhandle加锁
        if (!dhandle_locked) {
            WT_ERR(__evict_lock_handle_list(session));
            dhandle_locked = true;
        }

        //dhandle为NULL，说明是本轮的第一个表
        if (dhandle == NULL) {
            /*
             * On entry, continue from wherever we got to in the scan last time through. If we don't
             * have a saved handle, pick one randomly from the list.
             */
            if ((dhandle = cache->walk_tree) != NULL)
                cache->walk_tree = NULL;
            else
                 //从dhhash桶中随机选择一个表
                __evict_walk_choose_dhandle(session, &dhandle);
        } else {
            //需要把上一轮的表的session inuse统计恢复减1
            if (incr) {
                WT_ASSERT(session, dhandle->session_inuse > 0);
                (void)__wt_atomic_subi32(&dhandle->session_inuse, 1);
                incr = false;
                cache->walk_tree = NULL;
            }

            //重新选一个新表出来
            __evict_walk_choose_dhandle(session, &dhandle);
        }

        /* If we couldn't find any dhandle, we're done. */
        if (dhandle == NULL)
            break;

        /* Ignore non-btree handles, or handles that aren't open. */
        if (!WT_DHANDLE_BTREE(dhandle) || !F_ISSET(dhandle, WT_DHANDLE_OPEN))
            continue;

        /* Skip files that don't allow eviction. */
        btree = dhandle->handle;
        if (btree->evict_disabled > 0)
            continue;

        /*
         * Skip files that are checkpointing if we are only looking for dirty pages.
          WT_CACHE_EVICT_ALL满足任何一个的时候才会进来，所以这里可以确定是dirty
         */
        if (WT_BTREE_SYNCING(btree) &&
          !F_ISSET(cache, WT_CACHE_EVICT_CLEAN | WT_CACHE_EVICT_UPDATES))
            continue;

        /*
         * Skip files that are configured to stick in cache until we become aggressive.
         *
         * If the file is contributing heavily to our cache usage then ignore the "stickiness" of
         * its pages.
         */
        if (btree->evict_priority != 0 && !__wt_cache_aggressive(session) &&
          //确保某个表是否占用了一半触发evict条件的一半的内存(包括dirty update cache中的任何一种)
          !__wt_btree_dominating_cache(session, btree))
            continue;

        /*
         * Skip files if we have too many active walks.
         *
         * This used to be limited by the configured maximum number of hazard pointers per session.
         * Even though that ceiling has been removed, we need to test eviction with huge numbers of
         * active trees before allowing larger numbers of hazard pointers in the walk session.
         */
        if (btree->evict_ref == NULL && session->nhazard > WT_EVICT_MAX_TREES)
            continue;

        /*
         * If we are filling the queue, skip files that haven't been useful in the past.
     evict_walk_period在evict_walk_period赋值， 在__evict_walk生效，表示在下一轮对所有表遍历的时候，当前btree少遍历evict_walk_period个page
     因为当前btree遍历效果不佳
     yang add todo xxxxxx 这里是不是检查一下表数量比较好? 假设只有一个表则没表要skip
         */
        if (btree->evict_walk_period != 0 && btree->evict_walk_skips++ < btree->evict_walk_period)
            continue;
        btree->evict_walk_skips = 0;

        (void)__wt_atomic_addi32(&dhandle->session_inuse, 1);
        incr = true;
        //释放锁
        __wt_readunlock(session, &conn->dhandle_lock);
        dhandle_locked = false;

        /*
         * Re-check the "no eviction" flag, used to enforce exclusive access when a handle is being
         * closed.
         *
         * Only try to acquire the lock and simply continue if we fail; the lock is held while the
         * thread turning off eviction clears the tree's current eviction point, and part of the
         * process is waiting on this thread to acknowledge that action.
         *
         * If a handle is being discarded, it will still be marked open, but won't have a root page.
         */
        if (btree->evict_disabled == 0 && !__wt_spin_trylock(session, &cache->evict_walk_lock)) {
            if (btree->evict_disabled == 0 && btree->root.page != NULL) {
                /*
                 * Remember the file to visit first, next loop.
                 */
                cache->walk_tree = dhandle;
                // 从btree的btree->evict_ref位置遍历查找需要evict的page添加到到evict_queues[WT_EVICT_URGENT_QUEUE]队列和queue队列
                WT_WITH_DHANDLE(
                  //slot返回代表挑选出来入队的需要evict的page数，这些page入队到queue队列的slot位置
                  //slotp是一个累加的值
                  session, dhandle, ret = __evict_walk_tree(session, queue, max_entries, &slot));

                WT_ASSERT(session, __wt_session_gen(session, WT_GEN_SPLIT) == 0);
            }
            __wt_spin_unlock(session, &cache->evict_walk_lock);
            WT_ERR(ret);
            /*
             * If there is a checkpoint thread gathering handles, which means it is holding the
             * schema lock, then there is often contention on the evict walk lock with that thread.
             * If eviction is not in aggressive mode, sleep a bit to give the checkpoint thread a
             * chance to gather its handles.
             */
            if (F_ISSET(conn, WT_CONN_CKPT_GATHER) && !__wt_cache_aggressive(session)) {
                __wt_sleep(0, 10);
                WT_STAT_CONN_INCR(session, cache_eviction_walk_sleeps);
            }
        }
    }

    if (incr) {
        WT_ASSERT(session, dhandle->session_inuse > 0);
        (void)__wt_atomic_subi32(&dhandle->session_inuse, 1);
        incr = false;
    }

    /*
     * Repeat the walks a few times if we don't find enough pages. Give up when we have some
     * candidates and we aren't finding more.
     */
    if (slot < max_entries &&
      (retries < 2 ||
        (retries < WT_RETRY_MAX && (slot == queue->evict_entries || slot > start_slot)))) {
        start_slot = slot;
        ++retries;
        //printf("yang test ..........__evict_walk.....retries:%u, slot:%u, start_slot:%u, queue->evict_entriesr:%u\r\n",
        //   retries, slot, start_slot, queue->evict_entries);
        goto retry;
    }

err:
    if (dhandle_locked)
        __wt_readunlock(session, &conn->dhandle_lock);

    if (incr) {
        WT_ASSERT(session, dhandle->session_inuse > 0);
        (void)__wt_atomic_subi32(&dhandle->session_inuse, 1);
    }

    /*
     * If we didn't find any entries on a walk when we weren't interrupted, let our caller know.
     */
    //一轮扫描下来队列中的elem个数没有变化，则返回NOTFOUND
    if (queue->evict_entries == slot && cache->pass_intr == 0)
        ret = WT_NOTFOUND;

    queue->evict_entries = slot;
    WT_TRACK_OP_END(session);
    return (ret);
}

/*
 * __evict_push_candidate --
 *     Initialize a WT_EVICT_ENTRY structure with a given page.
 //__evict_push_candidate: page入队到queue中，queue可以是紧急队列，也可以是普通队列

 
 //WT_EVICT_ENTRY evict结构对应成员赋值
 //向queue队列入队的时候为什么可以不用加锁，这里用原子操作规避了对队列进行加锁
 //这里实际上队列每个elem是提取分配好的，直接对该elem赋值即可
 */
static bool
__evict_push_candidate(
  WT_SESSION_IMPL *session, WT_EVICT_QUEUE *queue, WT_EVICT_ENTRY *evict, WT_REF *ref)
{
    uint16_t orig_flags, new_flags;
    u_int slot;

    /*
     * Threads can race to queue a page (e.g., an ordinary LRU walk can race with a page being
     * queued for urgent eviction).
     */
    orig_flags = new_flags = ref->page->flags_atomic;
    //原子操作来规避队列加锁
    FLD_SET(new_flags, WT_PAGE_EVICT_LRU);
    if (orig_flags == new_flags ||
      !__wt_atomic_cas16(&ref->page->flags_atomic, orig_flags, new_flags))
        return (false);

    /* Keep track of the maximum slot we are using. */
    //evict在evict_queue数组中的index位置
    slot = (u_int)(evict - queue->evict_queue);
    if (slot >= queue->evict_max)
        queue->evict_max = slot + 1;

    if (evict->ref != NULL)
        __evict_list_clear(session, evict);

    evict->btree = S2BT(session);
    evict->ref = ref;
    //printf("yang test .........__evict_push_candidate...1...., btree->splitmempage:%lu, page read_gen:%lu\r\n", 
    //    S2BT(session)->splitmempage, ref->page->read_gen);
    evict->score = __evict_entry_priority(session, ref);
    //printf("yang test .........__evict_push_candidate...2....,page:%p, page size:%lu, evict->score:%lu\r\n", 
    //    ref->page, ref->page->memory_footprint, evict->score);
    
    /* Adjust for size when doing dirty eviction. */
    //如果脏数据过多，评分还要考虑当前的page内存占用，如果page占用内存越少，这里评分也就会越高，这样的目录是尽量让占用内存越高的page优先被evict worker淘汰
    if (F_ISSET(S2C(session)->cache, WT_CACHE_EVICT_DIRTY) && evict->score != WT_READGEN_OLDEST &&
      evict->score != UINT64_MAX && !__wt_page_is_modified(ref->page))
        evict->score += WT_MEGABYTE - WT_MIN(WT_MEGABYTE, ref->page->memory_footprint);

    return (true);
}

/*
 * __evict_walk_target --
 *     Calculate how many pages to queue for a given tree.
 //计算需要进行evict入队的page数量
 */
static uint32_t
__evict_walk_target(WT_SESSION_IMPL *session)
{
    WT_CACHE *cache;
    uint64_t btree_inuse, bytes_per_slot, cache_inuse;
    uint32_t target_pages, target_pages_clean, target_pages_dirty, target_pages_updates;

    cache = S2C(session)->cache;
    target_pages_clean = target_pages_dirty = target_pages_updates = 0;

/*
 * The minimum number of pages we should consider per tree.
 */
#define MIN_PAGES_PER_TREE 10

    /*
     * The target number of pages for this tree is proportional to the space it is taking up in
     * cache. Round to the nearest number of slots so we assign all of the slots to a tree filling
     * 99+% of the cache (and only have to walk it once).
     */
    if (F_ISSET(cache, WT_CACHE_EVICT_CLEAN)) {
        //计算一个表的内存使用量
        btree_inuse = __wt_btree_bytes_evictable(session);
        cache_inuse = __wt_cache_bytes_inuse(cache);
        bytes_per_slot = 1 + cache_inuse / cache->evict_slots;
        target_pages_clean = (uint32_t)((btree_inuse + bytes_per_slot / 2) / bytes_per_slot);
    }

    if (F_ISSET(cache, WT_CACHE_EVICT_DIRTY)) {
        btree_inuse = __wt_btree_dirty_leaf_inuse(session);
        cache_inuse = __wt_cache_dirty_leaf_inuse(cache);
        bytes_per_slot = 1 + cache_inuse / cache->evict_slots;
        target_pages_dirty = (uint32_t)((btree_inuse + bytes_per_slot / 2) / bytes_per_slot);
    }

    if (F_ISSET(cache, WT_CACHE_EVICT_UPDATES)) {
        btree_inuse = __wt_btree_bytes_updates(session);
        cache_inuse = __wt_cache_bytes_updates(cache);
        bytes_per_slot = 1 + cache_inuse / cache->evict_slots;
        target_pages_updates = (uint32_t)((btree_inuse + bytes_per_slot / 2) / bytes_per_slot);
    }

    target_pages = WT_MAX(target_pages_clean, target_pages_dirty);
    target_pages = WT_MAX(target_pages, target_pages_updates);

    /*
     * Walk trees with a small fraction of the cache in case there are so many trees that none of
     * them use enough of the cache to be allocated slots. Only skip a tree if it has no bytes of
     * interest.
     */
    if (target_pages == 0) {
        btree_inuse = F_ISSET(cache, WT_CACHE_EVICT_CLEAN | WT_CACHE_EVICT_UPDATES) ?
          __wt_btree_bytes_evictable(session) :
          __wt_btree_dirty_leaf_inuse(session);

        if (btree_inuse == 0)
            return (0);
    }

    /*
     * There is some cost associated with walking a tree. If we're going to visit this tree, always
     * look for a minimum number of pages.
     */
    if (target_pages < MIN_PAGES_PER_TREE)
        target_pages = MIN_PAGES_PER_TREE;

    /* If the tree is dead, take a lot of pages. */
    if (F_ISSET(session->dhandle, WT_DHANDLE_DEAD))
        target_pages *= 10;

    return (target_pages);
}

/*
 * __evict_walk_tree --
 *     Get a few page eviction candidates from a single underlying file.
 //evict worker线程: __evict_server->__evict_pass->__evict_lru_walk->__evict_walk->__evict_walk_tree
 //__evict_walk_tree在指定表中查找，__evict_walk遍历所有表查找候选page
 从btree的btree->evict_ref位置遍历查找需要evict的page添加到到evict_queues[WT_EVICT_URGENT_QUEUE]队列和queue队列
 */
static int
__evict_walk_tree(WT_SESSION_IMPL *session, WT_EVICT_QUEUE *queue, u_int max_entries,
    //slotp返回代表挑选出来入队的需要evict的page数
    u_int *slotp)
{
    WT_BTREE *btree;
    WT_CACHE *cache;
    WT_CONNECTION_IMPL *conn;
    WT_DECL_RET;
    WT_EVICT_ENTRY *end, *evict, *start;
    WT_PAGE *last_parent, *page;
    WT_REF *ref;

    uint64_t internal_pages_already_queued, internal_pages_queued, internal_pages_seen;
    uint64_t min_pages, pages_already_queued,
        //pages_seen代表遍历的循环次数   pages_queued代表选择出来需要后台evict的page数
        pages_seen, pages_queued,
        refs_walked;
    uint32_t read_flags, remaining_slots, target_pages, walk_flags;
    int restarts;
    bool give_up, modified, urgent_queued, want_page;

   // printf("yang test .....................__evict_walk_tree.....................\r\n");

    conn = S2C(session);
    btree = S2BT(session);
    cache = conn->cache;
    last_parent = NULL;
    restarts = 0;
    give_up = urgent_queued = false;

    /*
     * Figure out how many slots to fill from this tree. Note that some care is taken in the
     * calculation to avoid overflow.
     */
    //记录本轮需要入队的起始地址
    start = queue->evict_queue + *slotp;
    //queue->evict_queue队列中剩余可用节点数
    remaining_slots = max_entries - *slotp;
    if (btree->evict_walk_progress >= btree->evict_walk_target) {
         //计算需要进行evict入队的page数量
        btree->evict_walk_target = __evict_walk_target(session);
        //printf("yang test ....................... btree->evict_walk_target:%u\r\n", btree->evict_walk_target);
        btree->evict_walk_progress = 0;
    }

    //还剩余多少page需要入队
    target_pages = btree->evict_walk_target - btree->evict_walk_progress;

    if (target_pages > remaining_slots)
        target_pages = remaining_slots;

    /*
     * Reduce the number of pages to be selected from btrees other than the history store (HS) if
     * the cache pressure is high and HS content dominates the cache. Evicting unclean non-HS pages
     * can generate even more HS content and will not help with the cache pressure, and will
     * probably just amplify it further.
     */
    if (!WT_IS_HS(btree->dhandle) && __wt_cache_hs_dirty(session)) {
        /* If target pages are less than 10, keep it like that. */
        if (target_pages >= 10) {
            target_pages = target_pages / 10;
            WT_STAT_CONN_DATA_INCR(session, cache_eviction_target_page_reduced);
        }
    }

    /* If we don't want any pages from this tree, move on. */
    if (target_pages == 0)
        return (0);

    /*
     * These statistics generate a histogram of the number of pages targeted for eviction each
     * round. The range of values here start at MIN_PAGES_PER_TREE as this is the smallest number of
     * pages we can target, unless there are fewer slots available. The aim is to cover the likely
     * ranges of target pages in as few statistics as possible to reduce the overall overhead.
     */
    if (target_pages < MIN_PAGES_PER_TREE) {
        WT_STAT_CONN_INCR(session, cache_eviction_target_page_lt10);
        WT_STAT_DATA_INCR(session, cache_eviction_target_page_lt10);
    } else if (target_pages < 32) {
        WT_STAT_CONN_INCR(session, cache_eviction_target_page_lt32);
        WT_STAT_DATA_INCR(session, cache_eviction_target_page_lt32);
    } else if (target_pages < 64) {
        WT_STAT_CONN_INCR(session, cache_eviction_target_page_lt64);
        WT_STAT_DATA_INCR(session, cache_eviction_target_page_lt64);
    } else if (target_pages < 128) {
        WT_STAT_CONN_INCR(session, cache_eviction_target_page_lt128);
        WT_STAT_DATA_INCR(session, cache_eviction_target_page_lt128);
    } else {
        WT_STAT_CONN_INCR(session, cache_eviction_target_page_ge128);
        WT_STAT_DATA_INCR(session, cache_eviction_target_page_ge128);
    }

    //应该在队列中的结束位置
    end = start + target_pages;

    /*
     * Examine at least a reasonable number of pages before deciding whether to give up. When we are
     * only looking for dirty pages, search the tree for longer.
     */
    min_pages = 10 * (uint64_t)target_pages;
    if (!F_ISSET(cache, WT_CACHE_EVICT_DIRTY | WT_CACHE_EVICT_UPDATES)) //说明满足clean条件，因为走到该函数至少要满足一个条件
        WT_STAT_CONN_INCR(session, cache_eviction_target_strategy_clean);
    else if (!F_ISSET(cache, WT_CACHE_EVICT_UPDATES)) {
        min_pages *= 10;
        WT_STAT_CONN_INCR(session, cache_eviction_target_strategy_dirty);
    }

    if (F_ISSET(cache, WT_CACHE_EVICT_DIRTY) && F_ISSET(cache, WT_CACHE_EVICT_UPDATES))
        //这里应该是update，为了避免运行disk，我这里就不改了，已经提交PR到官网
        WT_STAT_CONN_INCR(session, cache_eviction_target_strategy_both_clean_and_dirty);

    if (btree->evict_ref == NULL) {
        WT_STAT_CONN_INCR(session, cache_eviction_walk_from_root);
        WT_STAT_DATA_INCR(session, cache_eviction_walk_from_root);
    } else {
        WT_STAT_CONN_INCR(session, cache_eviction_walk_saved_pos);
        WT_STAT_DATA_INCR(session, cache_eviction_walk_saved_pos);
    }

    walk_flags = WT_READ_CACHE | WT_READ_NO_EVICT | WT_READ_NO_GEN | WT_READ_NO_WAIT;
    if (!F_ISSET(session->txn, WT_TXN_HAS_SNAPSHOT))
        walk_flags |= WT_READ_VISIBLE_ALL;

    /*
     * Choose a random point in the tree if looking for candidates in a tree with no starting point
     * set. This is mostly aimed at ensuring eviction fairly visits all pages in trees with a lot of
     * in-cache content.
     */
    switch (btree->evict_start_type) {
    case WT_EVICT_WALK_NEXT:
        break;
    case WT_EVICT_WALK_PREV:
        FLD_SET(walk_flags, WT_READ_PREV);
        break;
    case WT_EVICT_WALK_RAND_PREV:
        FLD_SET(walk_flags, WT_READ_PREV);
    /* FALLTHROUGH */
    case WT_EVICT_WALK_RAND_NEXT:
        read_flags = WT_READ_CACHE | WT_READ_NO_EVICT | WT_READ_NO_GEN | WT_READ_NO_WAIT |
          WT_READ_NOTFOUND_OK | WT_READ_RESTART_OK;
        if (btree->evict_ref == NULL) {
            for (;;) {
                /* Ensure internal pages indexes remain valid */
                WT_WITH_PAGE_INDEX(
                  session, ret = __wt_random_descent(session, &btree->evict_ref, read_flags));
                if (ret != WT_RESTART)
                    break;
                WT_STAT_CONN_INCR(session, cache_eviction_walk_restart);
                WT_STAT_DATA_INCR(session, cache_eviction_walk_restart);
            }
            WT_RET_NOTFOUND_OK(ret);
        }
        break;
    }

    /*
     * Get some more eviction candidate pages, starting at the last saved point. Clear the saved
     * point immediately, we assert when discarding pages we're not discarding an eviction point, so
     * this clear must be complete before the page is released.
     */
    ref = btree->evict_ref;
    btree->evict_ref = NULL;

    /*
     * !!! Take care terminating this loop.
     *
     * Don't make an extra call to __wt_tree_walk after we hit the end of a
     * tree: that will leave a page pinned, which may prevent any work from
     * being done.
     *
     * Once we hit the page limit, do one more step through the walk in
     * case we are appending and only the last page in the file is live.
     */
    internal_pages_already_queued = internal_pages_queued = internal_pages_seen = 0;
    for (evict = start, pages_already_queued = pages_queued = pages_seen = refs_walked = 0;
         evict < end && (ret == 0 || ret == WT_NOTFOUND);
         last_parent = ref == NULL ? NULL : ref->home,
        //refp返回找到的下一个page，walkcntp代表CacheStat('cache_eviction_walk', 'pages walked for eviction'),
        //注意walkcntp是保留原值进行增加
        ret = __wt_tree_walk_count(session, &ref, &refs_walked, walk_flags)) {
        /*
         * Check whether we're finding a good ratio of candidates vs pages seen. Some workloads
         * create "deserts" in trees where no good eviction candidates can be found. Abandon the
         * walk if we get into that situation.
         */
        give_up = !__wt_cache_aggressive(session) && !WT_IS_HS(btree->dhandle) &&
          pages_seen > min_pages &&
          (pages_queued == 0 || (pages_seen / pages_queued) > (min_pages / target_pages));
        //如果发现选择脏数据的效果不是很好，则考虑反方向查找
        if (give_up) {
            /*
             * Try a different walk start point next time if a walk gave up.
             */
            //找到的候选page有点难，我们可以反方向找一下试试
            switch (btree->evict_start_type) {
            case WT_EVICT_WALK_NEXT:
                btree->evict_start_type = WT_EVICT_WALK_PREV;
                break;
            case WT_EVICT_WALK_PREV:
                btree->evict_start_type = WT_EVICT_WALK_RAND_PREV;
                break;
            case WT_EVICT_WALK_RAND_PREV:
                btree->evict_start_type = WT_EVICT_WALK_RAND_NEXT;
                break;
            case WT_EVICT_WALK_RAND_NEXT:
                btree->evict_start_type = WT_EVICT_WALK_NEXT;
                break;
            }

            /*
             * We differentiate the reasons we gave up on this walk and increment the stats
             * accordingly.
             */
            if (pages_queued == 0) {
                //扫描了一批page，但是一个候选page都没有
                WT_STAT_CONN_INCR(session, cache_eviction_walks_gave_up_no_targets);
                WT_STAT_DATA_INCR(session, cache_eviction_walks_gave_up_no_targets);
            } else {//找到少量候选索引
                WT_STAT_CONN_INCR(session, cache_eviction_walks_gave_up_ratio);
                WT_STAT_DATA_INCR(session, cache_eviction_walks_gave_up_ratio);
            }
            break;
        }

        if (ref == NULL) {
            WT_STAT_CONN_INCR(session, cache_eviction_walks_ended);
            WT_STAT_DATA_INCR(session, cache_eviction_walks_ended);

            if (++restarts == 2) {
                WT_STAT_CONN_INCR(session, cache_eviction_walks_stopped);
                WT_STAT_DATA_INCR(session, cache_eviction_walks_stopped);
                break;
            }
            WT_STAT_CONN_INCR(session, cache_eviction_walks_started);
            continue;
        }

        //__wt_tree_walk_count扫描了多少个page了
        ++pages_seen;

        /* Ignore root pages entirely. */
        if (__wt_ref_is_root(ref)) {
            internal_pages_seen++;
            continue;
        }
        page = ref->page;
        modified = __wt_page_is_modified(page);
        //也就是第几轮__evict_pass的时候该page被后台evict server线程选中淘汰的
        page->evict_pass_gen = cache->evict_pass_gen;

        /* Count internal pages seen. */
        //这一轮挑选evict page，扫描了多少internal page
        if (F_ISSET(ref, WT_REF_FLAG_INTERNAL))
            internal_pages_seen++;

        /* Use the EVICT_LRU flag to avoid putting pages onto the list multiple times. */
        //page已经在队列中
        if (F_ISSET_ATOMIC_16(page, WT_PAGE_EVICT_LRU)) {
            pages_already_queued++;
            if (F_ISSET(ref, WT_REF_FLAG_INTERNAL))
                internal_pages_already_queued++;
            continue;
        }

        /* Don't queue dirty pages in trees during checkpoints. */
        //正在做checkpoint，跳过
        //其他线程在做checkpoint,那么这个page会通过checkpoint进行evict，不用单独通过evict server线程进行入队处理了
        if (modified && WT_BTREE_SYNCING(btree))
            continue;

        //其他线程没有在做checkpoint，或者其他线程在做checkpoint但是该page没有被修改，可以继续往下走

        /*
         * It's possible (but unlikely) to visit a page without a read generation, if we race with
         * the read instantiating the page. Set the page's read generation here to ensure a bug
         * doesn't somehow leave a page without a read generation.
         */
        //如果read_gen没有设置，则给挑选出来的page read_gen赋值给page->read_gen，会在后面的__evict_push_candidate根据read_gen对page进行评分
        if (page->read_gen == WT_READGEN_NOTSET) {
            //printf("yang test ..........__evict_walk_tree...__wt_cache_read_gen_new........page:%p\r\n", page);
            __wt_cache_read_gen_new(session, page);
        }
        
        /* Pages being forcibly evicted go on the urgent queue. */
        //printf("yang test ................... %d, %d, %d\r\n", (int)modified, (int)page->read_gen, (int)page->memory_footprint);
        if (modified &&
          (page->read_gen == WT_READGEN_OLDEST || page->memory_footprint >= btree->splitmempage)) {
            WT_STAT_CONN_INCR(session, cache_eviction_pages_queued_oldest);
            //该page内存超限了，需要进行紧急evict
            //需要进行紧急evict的page添加到evict_queues[WT_EVICT_URGENT_QUEUE]队列
            if (__wt_page_evict_urgent(session, ref))
                urgent_queued = true;
            continue;
        }

        /*
         * If history store dirty content is dominating the cache, we want to prioritize evicting
         * history store pages over other btree pages. This helps in keeping cache contents below
         * the configured cache size during checkpoints where reconciling non-HS pages can generate
         * significant amount of HS dirty content very quickly.
         */
        if (WT_IS_HS(btree->dhandle) && __wt_cache_hs_dirty(session)) {
            WT_STAT_CONN_INCR(session, cache_eviction_pages_queued_urgent_hs_dirty);
            //需要进行紧急evict的page添加到evict_queues[WT_EVICT_URGENT_QUEUE]队列
            if (__wt_page_evict_urgent(session, ref))
                urgent_queued = true;
            continue;
        }

        /* Pages that are empty or from dead trees are fast-tracked. */
        if (__wt_page_is_empty(page) || F_ISSET(session->dhandle, WT_DHANDLE_DEAD))
            goto fast;

        /*
         * Do not evict a clean metadata page that contains historical data needed to satisfy a
         * reader. Since there is no history store for metadata, we won't be able to serve an older
         * reader if we evict this page.
         */
        if (WT_IS_METADATA(session->dhandle) && F_ISSET(cache, WT_CACHE_EVICT_CLEAN_HARD) &&
          F_ISSET(ref, WT_REF_FLAG_LEAF) && !modified && page->modify != NULL &&
          !__wt_txn_visible_all(
            session, page->modify->rec_max_txn, page->modify->rec_max_timestamp))
            continue;

        /* Skip pages we don't want. */
        //总内存消耗过高，则选择没有修改的page
        want_page = (F_ISSET(cache, WT_CACHE_EVICT_CLEAN) && !modified) ||
          //cache中dirty过高，有修改的page一般都是dirty page引起，因此选择dirty page
          (F_ISSET(cache, WT_CACHE_EVICT_DIRTY) && modified) ||
          //说明主要有update引起，则选择有modify成员的page
          (F_ISSET(cache, WT_CACHE_EVICT_UPDATES) && page->modify != NULL);
        if (!want_page)
            continue;

        /*
         * Don't attempt eviction of internal pages with children in cache (indicated by seeing an
         * internal page that is the parent of the last page we saw).
         *
         * Also skip internal page unless we get aggressive, the tree is idle (indicated by the tree
         * being skipped for walks), or we are in eviction debug mode. The goal here is that if
         * trees become completely idle, we eventually push them out of cache completely.
         */
        if (!F_ISSET(cache, WT_CACHE_EVICT_DEBUG_MODE) && F_ISSET(ref, WT_REF_FLAG_INTERNAL)) {
            if (page == last_parent)
                continue;
            if (btree->evict_walk_period == 0 && !__wt_cache_aggressive(session))
                continue;
        }

        /* If eviction gets aggressive, anything else is fair game. */
        if (__wt_cache_aggressive(session))
            goto fast;

        /*
         * If the global transaction state hasn't changed since the last time we tried eviction,
         * it's unlikely we can make progress. Similarly, if the most recent update on the page is
         * not yet globally visible, eviction will fail. This heuristic avoids repeated attempts to
         * evict the same page.
         */
        if (!__wt_page_evict_retry(session, page) ||
          //说明page上可能还有很多未提交的事务在使用这个page，因此跳过这个evict
          //这里可以看出如果该page最近一次更改的事务id不是全局可见，则不允许evict     
          //????????????? 这里为什么不考虑__wt_txn_visible_all中的pinned_timestamp时间戳判断,事务id全局可见，时间戳不是全局可见，可以做evict????
          (modified && page->modify->update_txn >= conn->txn_global.last_running))
            continue;

fast:
        /* If the page can't be evicted, give up. */
        if (!__wt_page_can_evict(session, ref, NULL))
            continue;

        WT_ASSERT(session, evict->ref == NULL);
        //不需要紧急evict的page添加到queue中,evict为选择的page在queue中的位置
        if (!__evict_push_candidate(session, queue, evict, ref))
            continue;
        ++evict;
        ++pages_queued;
        ++btree->evict_walk_progress;

        /* Count internal pages queued. */
        if (F_ISSET(ref, WT_REF_FLAG_INTERNAL))
            internal_pages_queued++;

        //打印evict主线程挑选出来需要evict的page
        __wt_verbose(session, WT_VERB_EVICTSERVER, "__evict_walk_tree select: %p, size %zu, type:%s", (void *)page,
          page->memory_footprint, __wt_page_type_string(page->type));
    }
    WT_RET_NOTFOUND_OK(ret);

    //从这里可以看出，slotp是一个累加的值
    *slotp += (u_int)(evict - start);
    //挑选的入队的总page数
    WT_STAT_CONN_INCRV(session, cache_eviction_pages_queued, (u_int)(evict - start));

    __wt_verbose_debug2(session, WT_VERB_EVICTSERVER, "__evict_walk_tree, %s walk: seen %" PRIu64
        ", queued %" PRIu64 ", min_pages %" PRIu64 ", target_pages %" PRIu32,
      session->dhandle->name, pages_seen, pages_queued, min_pages, target_pages);

    /*
     * If we couldn't find the number of pages we were looking for, skip the tree next time.
     evict_walk_period在evict_walk_period赋值， 在__evict_walk生效，表示在下一轮对所有表遍历的时候，当前btree少遍历evict_walk_period个page
     因为当前btree遍历效果不佳，每次效果不佳，下一轮该表跳过的page就*2, 效果逐渐好转后下一轮skip/2
     */

    if (pages_queued < target_pages / 2 && !urgent_queued)
        btree->evict_walk_period = WT_MIN(WT_MAX(1, 2 * btree->evict_walk_period), 100);
    else if (pages_queued == target_pages) {
        btree->evict_walk_period = 0;
        /*
         * If there's a chance the Btree was fully evicted, update the evicted flag in the handle.
         */
        //如果该表的page都已经evict完了，则标记该表已经evicet完成
        if (__wt_btree_bytes_evictable(session) == 0)
            F_SET(session->dhandle, WT_DHANDLE_EVICTED);
    } else if (btree->evict_walk_period > 0)
        btree->evict_walk_period /= 2;

    /*
     * Give up the walk occasionally.
     *
     * If we happen to end up on the root page or a page requiring urgent eviction, clear it. We
     * have to track hazard pointers, and the root page complicates that calculation.
     *
     * Likewise if we found no new candidates during the walk: there is no point keeping a page
     * pinned, since it may be the only candidate in an idle tree.
     *
     * If we land on a page requiring forced eviction, or that isn't an ordinary in-memory page,
     * move until we find an ordinary page: we should not prevent exclusive access to the page until
     * the next walk.
     */
    if (ref != NULL) {
        if (__wt_ref_is_root(ref) || evict == start || give_up ||
          ref->page->memory_footprint >= btree->splitmempage) {
            if (restarts == 0)
                WT_STAT_CONN_INCR(session, cache_eviction_walks_abandoned);
            WT_RET(__wt_page_release(cache->walk_session, ref, walk_flags));
            ref = NULL;
        } else
            while (ref != NULL &&
              (ref->state != WT_REF_MEM || WT_READGEN_EVICT_SOON(ref->page->read_gen)))
                //注意walkcntp是保留原值进行增加
                WT_RET_NOTFOUND_OK(__wt_tree_walk_count(session, &ref, &refs_walked, walk_flags));
        btree->evict_ref = ref;
    }

    WT_STAT_CONN_INCRV(session, cache_eviction_walk, refs_walked);
    WT_STAT_CONN_DATA_INCRV(session, cache_eviction_pages_seen, pages_seen);
    //扫描的page中已经入队的page数(包括internal和leaf page)
    WT_STAT_CONN_INCRV(session, cache_eviction_pages_already_queued, pages_already_queued);
    //本轮遍历扫描的internal page数
    WT_STAT_CONN_INCRV(session, cache_eviction_internal_pages_seen, internal_pages_seen);
    //表示本轮遍历前之前已经进入了队列的internal page数
    WT_STAT_CONN_INCRV(
      session, cache_eviction_internal_pages_already_queued, internal_pages_already_queued);
    //本次遍历选择进入队列的internage
    WT_STAT_CONN_INCRV(session, cache_eviction_internal_pages_queued, internal_pages_queued);
    //上面的cache_eviction_pages_queued-internal_pages_queued可以满足这个要求
    WT_STAT_CONN_DATA_INCR(session, cache_eviction_walk_passes);
    return (0);
}

/*
 * __evict_get_ref --
 *     Get a page for eviction.
//evict worker线程(挑选的page入队): __wt_evict_thread_run->__evict_server->__evict_pass->__evict_lru_walk->__evict_walk->__evict_walk_tree
//worker线程(挑选的page出队进行真正evict): __wt_evict_thread_run->__evict_lru_pages->__evict_page->__evict_get_ref(从队列选page进行evict)
 */
static int
__evict_get_ref(WT_SESSION_IMPL *session,
    //这里is_server为true，说明是evict server线程
    bool is_server,
    WT_BTREE **btreep, WT_REF **refp,
    uint8_t *previous_statep)
{
    WT_CACHE *cache;
    WT_EVICT_ENTRY *evict;
    WT_EVICT_QUEUE *queue, *other_queue, *urgent_queue;
    uint32_t candidates;
    uint8_t previous_state;
    bool is_app, server_only, urgent_ok;

    *btreep = NULL;
    /*
     * It is polite to initialize output variables, but it isn't safe for callers to use the
     * previous state if we don't return a locked ref.
     */
    *previous_statep = WT_REF_MEM;
    *refp = NULL;

    cache = S2C(session)->cache;
    is_app = !F_ISSET(session, WT_SESSION_INTERNAL);
    server_only = is_server && !WT_EVICT_HAS_WORKERS(session);
    /* Application threads do eviction when cache is full of dirty data */
    //1. 说明是evict线程中的worker线程
    //2. evict线程只配置1个
    //3. 用户线程因为dirty过高触发evict
    urgent_ok = (!is_app && !is_server)
        || !WT_EVICT_HAS_WORKERS(session)
          || (is_app && F_ISSET(cache, WT_CACHE_EVICT_DIRTY_HARD));
    urgent_queue = cache->evict_urgent_queue;

    WT_STAT_CONN_INCR(session, cache_eviction_get_ref);

    /* Avoid the LRU lock if no pages are available. */
    //如果is_server=true，说明是evict server线程，也就代表只配置了一个evict线程, 从 __evict_pass->__evict_lru_pages走到这里
    //如果is_server=false，说明是worker线程或者app用户线程，这时候如果三个队列都为空，则直接这里返回
    if (__evict_queue_empty(cache->evict_current_queue, is_server) &&
      __evict_queue_empty(cache->evict_other_queue, is_server) &&
      (!urgent_ok || __evict_queue_empty(urgent_queue, false))) {
        //cache_eviction_get_ref_empty: eviction calls to get a page found queue empty
        //cache_eviction_get_ref_empty2: eviction calls to get a page found queue empty after locking
        WT_STAT_CONN_INCR(session, cache_eviction_get_ref_empty);
       // printf("yang test ............__evict_get_ref...................1......\r\n");
        return (WT_NOTFOUND);
    }

    //普通队列不为空，或者需要紧急处理(urgent_ok=TRUE或者urgent_queue不为空)

    /*
     * The server repopulates whenever the other queue is not full, as long as at least one page has
     * been evicted out of the current queue.
     *
     * Note that there are pathological cases where there are only enough eviction candidates in the
     * cache to fill one queue. In that case, we will continually evict one page and attempt to
     * refill the queues. Such cases are extremely rare in real applications.
     */
    //罕见行为先跳过
    if (is_server && (!urgent_ok || __evict_queue_empty(urgent_queue, false)) &&
      !__evict_queue_full(cache->evict_current_queue) &&
      !__evict_queue_full(cache->evict_fill_queue) &&
      (cache->evict_empty_score > WT_EVICT_SCORE_CUTOFF ||
        __evict_queue_empty(cache->evict_fill_queue, false))) {
      //  printf("yang test ............__evict_get_ref...................1......\r\n");
        return (WT_NOTFOUND);
    }
    //上锁，注意这里是cache维度的全局锁，所有队列都会被影响
    __wt_spin_lock(session, &cache->evict_queue_lock);


    /* Check the urgent queue first. */
    //紧急情况下优先处理紧急队列
    if (urgent_ok && !__evict_queue_empty(urgent_queue, false))
        queue = urgent_queue;
    else {
        /*
         * Check if the current queue needs to change.
         *
         * The server will only evict half of the pages before looking for more, but should only
         * switch queues if there are no other eviction workers.
         */
        queue = cache->evict_current_queue;
        other_queue = cache->evict_other_queue;
        //printf("yang test ....__evict_get_ref.........queue:%p, other_queue:%p\r\n", queue, other_queue);
        //如果evict_current_queue队列为空，但是evict_other_queue队列不为空，则交换这两个队列指向
        if (__evict_queue_empty(queue, server_only) &&
          !__evict_queue_empty(other_queue, server_only)) {
            cache->evict_current_queue = other_queue;
            cache->evict_other_queue = queue;
            //printf("yang test ....__evict_get_ref....switch.....queue:%p, other_queue:%p\r\n", queue, other_queue);
        }
    }

    __wt_spin_unlock(session, &cache->evict_queue_lock);

    /*
     * We got the queue lock, which should be fast, and chose a queue. Now we want to get the lock
     * on the individual queue.
     */
    //对queue队列上锁，这里是queue维度的锁，只锁本queue队列，上面是cache维度的锁
    for (;;) {
        /* Verify there are still pages available. */
        if (__evict_queue_empty(queue, is_server && queue != urgent_queue)) {
            //cache_eviction_get_ref_empty: eviction calls to get a page found queue empty
            //cache_eviction_get_ref_empty2: eviction calls to get a page found queue empty after locking
            WT_STAT_CONN_INCR(session, cache_eviction_get_ref_empty2);
           // printf("yang test ............__evict_get_ref...................3......\r\n");
            return (WT_NOTFOUND);
        }
        if (!is_server)
            //worker线程或者用户线程直接上锁
            __wt_spin_lock(session, &queue->evict_lock);
        else if (__wt_spin_trylock(session, &queue->evict_lock) != 0)
            //evict server线程trylock, 下面的evict_lock锁中的内容运行很快，所以这里evict server线程可以很快获取到锁
            continue;
        break;
    }

    /*
     * Only evict half of the pages before looking for more. The remainder are left to eviction
     * workers (if configured), or application thread if necessary.
     */
    candidates = queue->evict_candidates;
    if (is_server && queue != urgent_queue && candidates > 1)
        candidates /= 2;

    //if (queue == urgent_queue)
       // printf("yang test ............__evict_get_ref.........u-size:%u, urgent_ok:%d\r\n", 
         //   queue->evict_candidates, urgent_ok);
      
    /* Get the next page queued for eviction. */
    for (evict = queue->evict_current;
         evict >= queue->evict_queue && evict < queue->evict_queue + candidates; ++evict) {
       // printf("yang test ............__evict_get_ref........1......ref:%p, %d\r\n", 
          //  evict->ref, evict->ref->state);
        if (evict->ref == NULL)
            continue;
        WT_ASSERT(session, evict->btree != NULL);

        /*
         * Evicting a dirty page in the server thread could stall during a write and prevent
         * eviction from finding new work.
         *
         * However, we can't skip entries in the urgent queue or they may never be found again.
         *
         * Don't force application threads to evict dirty pages if they aren't stalled by the amount
         * of dirty data in cache.
         */
        //如果不紧急，并且只是总内存超限，不进行后面的流程，直接break
        if (!urgent_ok &&
          (is_server || !F_ISSET(cache, WT_CACHE_EVICT_DIRTY_HARD | WT_CACHE_EVICT_UPDATES_HARD)) &&
          __wt_page_is_modified(evict->ref->page)) {
            --evict;
          //  printf("yang test ......xx......__evict_get_ref........1......ref:%p\r\n", evict->ref);
            //yang add todo xxxxxxxxxxxx  走到这里，如果不紧急，则用户线程不进行真正的evict
            break;
        }

        /*
         * Lock the page while holding the eviction mutex to prevent multiple attempts to evict it.
         * For pages that are already being evicted, this operation will fail and we will move on.
         */
        if ((previous_state = evict->ref->state) != WT_REF_MEM ||
          !WT_REF_CAS_STATE(session, evict->ref, previous_state, WT_REF_LOCKED)) {
            __evict_list_clear(session, evict);
           // printf("yang test ............__evict_get_ref.......ss.1......ref:%p  %d\r\n", 
            //    evict->ref, evict->ref->state);
            continue;
        }

        /*
         * Increment the busy count in the btree handle to prevent it from being closed under us.
         */
        (void)__wt_atomic_addv32(&evict->btree->evict_busy, 1);

        //选出的这个evict page记录到这几个变量中返回
        *btreep = evict->btree;
        *refp = evict->ref;
        *previous_statep = previous_state;

        /*
         * Remove the entry so we never try to reconcile the same page on reconciliation error.
         */
         //标识队列中的这个evict成员为WT_PAGE_EVICT_LRU了
        __evict_list_clear(session, evict);
        break;
    }

    /* Move to the next item. */
    if (evict != NULL && evict + 1 < queue->evict_queue + queue->evict_candidates)
        queue->evict_current = evict + 1;
    else /* Clear the current pointer if there are no more candidates. */
        queue->evict_current = NULL;

    __wt_spin_unlock(session, &queue->evict_lock);
   // printf("yang test ............__evict_get_ref...................1......%p\r\n", *refp);

    return (*refp == NULL ? WT_NOTFOUND : 0);
}

/*
 * __evict_page --
 *     Called by both eviction and application threads to evict a page.

  //evict worker线程: __wt_evict_thread_run->__evict_server->__evict_pass->__evict_lru_walk->__evict_walk->__evict_walk_tree
  //worker线程: __wt_evict_thread_run->__evict_lru_pages->__evict_page

  //当一个page消耗内存较高，用户线程主动强制eviect:  __wt_page_in_func->__wt_page_release_evict->__wt_evict
 //当总内存或者脏数据或者update数据超过一定比例，用户线程或者后台线程的evict逻辑: __evict_page->__wt_evict
 //__wt_evict_file->__wt_evict : checkpoint逻辑
 */
static int
__evict_page(WT_SESSION_IMPL *session, 
    //标识是否是evict主线程
    bool is_server)
{
    WT_BTREE *btree;
    WT_CACHE *cache;
    WT_DECL_RET;
    WT_REF *ref;
    WT_TRACK_OP_DECL;
    uint64_t time_start, time_stop;
    uint32_t flags;
    uint8_t previous_state;

    WT_TRACK_OP_INIT(session);
    //printf("yang test ................................__evict_page......1.......................................\r\n");

    //从队列挑选出需要evict进行真正的的evict reconcile落盘
    WT_RET_TRACK(__evict_get_ref(session, is_server, &btree, &ref, &previous_state));
    WT_ASSERT(session, ref->state == WT_REF_LOCKED);
    //printf("yang test ................................__evict_page........22.....................................\r\n");

    cache = S2C(session)->cache;
    time_start = 0;

    flags = 0;

    /*
     * An internal session flags either the server itself or an eviction worker thread.
     */
    if (is_server)
        WT_STAT_CONN_INCR(session, cache_eviction_server_evicting);
    else if (F_ISSET(session, WT_SESSION_INTERNAL))
        WT_STAT_CONN_INCR(session, cache_eviction_worker_evicting);
    else {
        if (__wt_page_is_modified(ref->page))
            WT_STAT_CONN_INCR(session, cache_eviction_app_dirty);
        WT_STAT_CONN_INCR(session, cache_eviction_app);
        cache->app_evicts++;
        time_start = WT_STAT_ENABLED(session) ? __wt_clock(session) : 0;
    }

    /*
     * In case something goes wrong, don't pick the same set of pages every time.
     *
     * We used to bump the page's read generation only if eviction failed, but that isn't safe: at
     * that point, eviction has already unlocked the page and some other thread may have evicted it
     * by the time we look at it.
     */
    //printf("yang test ........__evict_page.....__wt_cache_read_gen_bump.........page:%p\r\n", ref->page);
    __wt_cache_read_gen_bump(session, ref->page);
    //挑选出来的ref page进行evict reconcile落盘操作
    WT_WITH_BTREE(session, btree, ret = __wt_evict(session, ref, previous_state, flags));

    (void)__wt_atomic_subv32(&btree->evict_busy, 1);

    if (time_start != 0) {
        time_stop = __wt_clock(session);
        WT_STAT_CONN_INCRV(session, application_evict_time, WT_CLOCKDIFF_US(time_stop, time_start));
    }
    WT_TRACK_OP_END(session);
    return (ret);
}

/*
 * __wt_cache_eviction_worker --
 *     Worker function for __wt_cache_eviction_check: evict pages if the cache crosses its
 *     boundaries.
 触发后台线程进行evict操作, 例如是总内存用了默认80%就会走到这里，首先触发后台线程进行evict，如果还超过了默认95%,则还会触发后面的用户线程淘汰
 */
//用户线程发送evict_cond信号: __wt_cache_eviction_check->__wt_cache_eviction_worker->__wt_evict_server_wake
//evict server线程发送evict_cond信号: //__evict_walk_tree->__wt_page_evict_urgent->__wt_evict_server_wake

//用户线程调用
int
__wt_cache_eviction_worker(WT_SESSION_IMPL *session, bool busy, bool readonly,
    //pct_full大于100说明，至少有一个超过了用户线程evict的阈值，配合__wt_eviction_needed阅读
    double pct_full)
{
    WT_CACHE *cache;
    WT_CONNECTION_IMPL *conn;
    WT_DECL_RET;
    WT_TRACK_OP_DECL;
    WT_TXN_GLOBAL *txn_global;
    WT_TXN_SHARED *txn_shared;
    uint64_t cache_max_wait_us, initial_progress, max_progress;
    uint64_t elapsed, time_start, time_stop;
    bool app_thread;

    WT_TRACK_OP_INIT(session);
   // printf("yang test ..................__wt_cache_eviction_worker...................................\r\n");

    conn = S2C(session);
    cache = conn->cache;
    time_start = 0;
    txn_global = &conn->txn_global;
    txn_shared = WT_SESSION_TXN_SHARED(session);

    //优先看session维度的cache_max_wait_ms配置，如果session维度没有配置，则以conn维度的cache_max_wait_ms配置为准
    if (session->cache_max_wait_us != 0)
        cache_max_wait_us = session->cache_max_wait_us;
    else
        cache_max_wait_us = cache->cache_max_wait_us;

    /*
     * Before we enter the eviction generation, make sure this session has a cached history store
     * cursor, otherwise we can deadlock with a session wanting exclusive access to a handle: that
     * session will have a handle list write lock and will be waiting on eviction to drain, we'll be
     * inside eviction waiting on a handle list read lock to open a history store cursor.
     */
    WT_ERR(__wt_curhs_cache(session));

    /*
     * It is not safe to proceed if the eviction server threads aren't setup yet.
     */
    if (!conn->evict_server_running || (busy && pct_full < 100.0))
        goto done;

    //走到这里，说明触发后台线程进行evict操作, 例如是总内存用了默认80%就会走到这里，首先触发后台线程进行evict，如果还超过了默认95%,则还会触发后面的用户线程淘汰
    /* Wake the eviction server if we need to do work. */
    __wt_evict_server_wake(session);

    /* Track how long application threads spend doing eviction. */
    app_thread = !F_ISSET(session, WT_SESSION_INTERNAL);
    if (app_thread)
        time_start = __wt_clock(session);

    for (initial_progress = cache->eviction_progress;; ret = 0) {
        /*
         * If eviction is stuck, check if this thread is likely causing problems and should be
         * rolled back. Ignore if in recovery, those transactions can't be rolled back.
         */
        if (!F_ISSET(conn, WT_CONN_RECOVERING) && __wt_cache_stuck(session)) {
            ret = __wt_txn_is_blocking(session);
            if (ret == WT_ROLLBACK) {
                --cache->evict_aggressive_score;
                WT_STAT_CONN_INCR(session, txn_fail_cache);
                __wt_verbose_debug1(
                  session, WT_VERB_TRANSACTION, "__wt_cache_eviction_worker rollback: %s", session->txn->rollback_reason);
            }
            WT_ERR(ret);
        }

        /*
         * Check if we've exceeded our operation timeout, this would also get called from the
         * previous txn is blocking call, however it won't pickup transactions that have been
         * committed or rolled back as their mod count is 0, and that txn needs to be the oldest.
         *
         * Additionally we don't return rollback which could confuse the caller.
         */
        //例如做回滚或者事务提交的时候开始计时，进行evict page的时间不能超过operation_timeout_us
        //yang add todo xxxxxxxxxxxxxxxxxxxxx   operation_timeout_us和cache_max_wait_us配置的区别是啥呢? 不是一样的吗
        if (__wt_op_timer_fired(session))
            break;

        /*
         * Check if we have become busy.
         *
         * If we're busy (because of the transaction check we just did or because our caller is
         * waiting on a longer-than-usual event such as a page read), and the cache level drops
         * below 100%, limit the work to 5 evictions and return. If that's not the case, we can do
         * more.
         */
        if (!busy && txn_shared->pinned_id != WT_TXN_NONE &&
          txn_global->current != txn_global->oldest_id)
            busy = true;
        max_progress = busy ? 5 : 20;

        /* See if eviction is still needed. */
        if (!__wt_eviction_needed(session, busy, readonly, &pct_full) ||
          (pct_full < 100.0 && (cache->eviction_progress > initial_progress + max_progress)))
            break;

        //用户线程通过这里进行evict page操作，这里面有具体是用户线程还是evict线程进行evict的统计
        /* Evict a page. */  //yang add todo xxxxxxxxxxxxxxxxxxxxx   如果读线程触发__evict_page，并返回错误，这里是不是不应该进入err逻辑，进入err逻辑读会直接报错
        switch (ret = __evict_page(session, false)) {
        case 0:
            if (busy)
                goto err;
        /* FALLTHROUGH */
        case EBUSY:
            break;
        case WT_NOTFOUND:
            /* Allow the queue to re-populate before retrying. */
            __wt_cond_wait(session, conn->evict_threads.wait_cond, 10000, NULL);
            cache->app_waits++;
            break;
        default:
            goto err;
        }
        /* Stop if we've exceeded the time out. */
        //配置了cache_max_wait_us，但是这个用户请求因为做evict page操作最终超时了，这时候
        if (time_start != 0 && cache_max_wait_us != 0) {
            time_stop = __wt_clock(session);
            if (session->cache_wait_us + WT_CLOCKDIFF_US(time_stop, time_start) > cache_max_wait_us)
                goto err;
        }
    }

err:
    if (time_start != 0) {//假设上面的__evict_page没有执行，这里就不应该进行application_cache_time统计
        time_stop = __wt_clock(session);
        elapsed = WT_CLOCKDIFF_US(time_stop, time_start);
        //这里需要一个用户线程进行evict的次数统计 application_cache_ops
        WT_STAT_CONN_INCRV(session, application_cache_time, elapsed);
        //参考mongo server的WiredTigerOperationStats::_statNameMap  {WT_STAT_SESSION_CACHE_TIME, std::make_pair("cache"_sd, Section::WAIT)}};
        WT_STAT_SESSION_INCRV(session, cache_time, elapsed);
        session->cache_wait_us += elapsed;
        if (cache_max_wait_us != 0 && session->cache_wait_us > cache_max_wait_us) {
            WT_TRET(__wt_txn_rollback_required(session, WT_TXN_ROLLBACK_REASON_CACHE_OVERFLOW));
            --cache->evict_aggressive_score;
            WT_STAT_CONN_INCR(session, cache_timed_out_ops);
            __wt_verbose_notice(session, WT_VERB_TRANSACTION, "%s", session->txn->rollback_reason);
        }
    }

done:
    WT_TRACK_OP_END(session);

    return (ret);
}

/*
 * __wt_page_evict_urgent --
 *     Set a page to be evicted as soon as possible.
 */  //__evict_walk_tree->__wt_page_evict_urgent->__wt_evict_server_wake

//用户线程发送evict_cond信号: __wt_cache_eviction_check->__wt_cache_eviction_worker->__wt_evict_server_wake
//evict server线程发送evict_cond信号: //__evict_walk_tree->__wt_page_evict_urgent->__wt_evict_server_wake

//需要进行紧急evict的page添加到evict_queues[WT_EVICT_URGENT_QUEUE]队列
bool
__wt_page_evict_urgent(WT_SESSION_IMPL *session, WT_REF *ref)
{
    WT_CACHE *cache;
    WT_EVICT_ENTRY *evict;
    WT_EVICT_QUEUE *urgent_queue;
    WT_PAGE *page;
    bool queued;

    /* Root pages should never be evicted via LRU. */
    WT_ASSERT(session, !__wt_ref_is_root(ref));
    //printf("yang test .....................__wt_page_evict_urgent...............\r\n");

    page = ref->page;
    if (F_ISSET_ATOMIC_16(page, WT_PAGE_EVICT_LRU) || S2BT(session)->evict_disabled > 0)
        return (false);

    /* Append to the urgent queue if we can. */
    cache = S2C(session)->cache;
    urgent_queue = &cache->evict_queues[WT_EVICT_URGENT_QUEUE];
    queued = false;

    __wt_spin_lock(session, &cache->evict_queue_lock);
    if (F_ISSET_ATOMIC_16(page, WT_PAGE_EVICT_LRU) || S2BT(session)->evict_disabled > 0)
        goto done;

    __wt_spin_lock(session, &urgent_queue->evict_lock);
     //如果server_check为true，则只要队列中有一半的elem还没有消费处理，则任务是empty的
    //如果server_check为false，当队列中的elem全部消费完了，就任务是empty的
    if (__evict_queue_empty(urgent_queue, false)) {
        urgent_queue->evict_current = urgent_queue->evict_queue;
        urgent_queue->evict_candidates = 0;
    }

    //从预分配的evict_queue[]数组中获取一个WT_EVICT_ENTRY
    evict = urgent_queue->evict_queue + urgent_queue->evict_candidates;
    //如果预分配的evict_slots个WT_EVICT_ENTRY没有用完，则直接添加到evict_queue[]对应位置
    if (evict < urgent_queue->evict_queue + cache->evict_slots &&
      //这里面使用了原子操作，这样可以避免加锁
      __evict_push_candidate(session, urgent_queue, evict, ref)) {
        ++urgent_queue->evict_candidates;
        queued = true;
    } //yang add todo xxxxxxxxxxxxxxx 当前队列已经用完，这个page会被跳过，这样会不会有问题??
    __wt_spin_unlock(session, &urgent_queue->evict_lock);

done:
    __wt_spin_unlock(session, &cache->evict_queue_lock);
    //需要紧急处理的page，立马通知工作线程处理
    if (queued) {
        WT_STAT_CONN_INCR(session, cache_eviction_pages_queued_urgent);
        if (WT_EVICT_HAS_WORKERS(session)) //如果有2个或者以上的evict线程，则触发工作线程在__evict_lru_pages进行下一轮page evict
            __wt_cond_signal(session, S2C(session)->evict_threads.wait_cond);
        else
            //说明只有一个evict线程，这时候需要通过这里幻想自己的下一轮evict选举和淘汰
            __wt_evict_server_wake(session);
    }

    return (queued);
}

/*
 * __wt_evict_priority_set --
 *     Set a tree's eviction priority.
 */
void
__wt_evict_priority_set(WT_SESSION_IMPL *session, uint64_t v)
{
    S2BT(session)->evict_priority = v;
}

/*
 * __wt_evict_priority_clear --
 *     Clear a tree's eviction priority.
 */
void
__wt_evict_priority_clear(WT_SESSION_IMPL *session)
{
    S2BT(session)->evict_priority = 0;
}

/*
 * __verbose_dump_cache_single --
 *     Output diagnostic information about a single file in the cache.
 */
static int
__verbose_dump_cache_single(WT_SESSION_IMPL *session, uint64_t *total_bytesp,
  uint64_t *total_dirty_bytesp, uint64_t *total_updates_bytesp)
{
    WT_BTREE *btree;
    WT_DATA_HANDLE *dhandle;
    WT_PAGE *page;
    WT_REF *next_walk;
    size_t size;
    uint64_t intl_bytes, intl_bytes_max, intl_dirty_bytes;
    uint64_t intl_dirty_bytes_max, intl_dirty_pages, intl_pages;
    uint64_t leaf_bytes, leaf_bytes_max, leaf_dirty_bytes;
    uint64_t leaf_dirty_bytes_max, leaf_dirty_pages, leaf_pages, updates_bytes;

    intl_bytes = intl_bytes_max = intl_dirty_bytes = 0;
    intl_dirty_bytes_max = intl_dirty_pages = intl_pages = 0;
    leaf_bytes = leaf_bytes_max = leaf_dirty_bytes = 0;
    leaf_dirty_bytes_max = leaf_dirty_pages = leaf_pages = 0;
    updates_bytes = 0;

    dhandle = session->dhandle;
    btree = dhandle->handle;
    WT_RET(__wt_msg(session, "__verbose_dump_cache_single %s(%s%s)%s%s:", dhandle->name,
      WT_DHANDLE_IS_CHECKPOINT(dhandle) ? "checkpoint=" : "",
      WT_DHANDLE_IS_CHECKPOINT(dhandle) ? dhandle->checkpoint : "<live>",
      btree->evict_disabled != 0 ? " eviction disabled" : "",
      btree->evict_disabled_open ? " at open" : ""));

    /*
     * We cannot walk the tree of a dhandle held exclusively because the owning thread could be
     * manipulating it in a way that causes us to dump core. So print out that we visited and
     * skipped it.
     */
    if (F_ISSET(dhandle, WT_DHANDLE_EXCLUSIVE))
        return (__wt_msg(session, " handle opened exclusively, cannot walk tree, skipping"));

    next_walk = NULL;
    while (__wt_tree_walk(session, &next_walk,
             WT_READ_CACHE | WT_READ_NO_EVICT | WT_READ_NO_WAIT | WT_READ_VISIBLE_ALL) == 0 &&
      next_walk != NULL) {
        page = next_walk->page;
        size = page->memory_footprint;

        if (F_ISSET(next_walk, WT_REF_FLAG_INTERNAL)) {
            ++intl_pages;
            intl_bytes += size;
            intl_bytes_max = WT_MAX(intl_bytes_max, size);
            if (__wt_page_is_modified(page)) {
                ++intl_dirty_pages;
                intl_dirty_bytes += size;
                intl_dirty_bytes_max = WT_MAX(intl_dirty_bytes_max, size);
            }
        } else {
            ++leaf_pages;
            leaf_bytes += size;
            leaf_bytes_max = WT_MAX(leaf_bytes_max, size);
            if (__wt_page_is_modified(page)) {
                ++leaf_dirty_pages;
                leaf_dirty_bytes += size;
                leaf_dirty_bytes_max = WT_MAX(leaf_dirty_bytes_max, size);
            }
            if (page->modify != NULL)
                updates_bytes += page->modify->bytes_updates;
        }
    }

    if (intl_pages == 0)
        WT_RET(__wt_msg(session, "__verbose_dump_cache_single internal: 0 pages"));
    else
        WT_RET(
          __wt_msg(session,
            "__verbose_dump_cache_single internal: "
            "%" PRIu64 " pages, %.2f KB, "
            "%" PRIu64 "/%" PRIu64 " clean/dirty pages, "
            "%.2f/%.2f clean / dirty KB, "
            "%.2f KB max page, "
            "%.2f KB max dirty page ",
            intl_pages, (double)intl_bytes / WT_KILOBYTE, intl_pages - intl_dirty_pages,
            intl_dirty_pages, (double)(intl_bytes - intl_dirty_bytes) / WT_KILOBYTE,
            (double)intl_dirty_bytes / WT_KILOBYTE, (double)intl_bytes_max / WT_KILOBYTE,
            (double)intl_dirty_bytes_max / WT_KILOBYTE));
    if (leaf_pages == 0)
        WT_RET(__wt_msg(session, "leaf: 0 pages"));
    else
        WT_RET(
          __wt_msg(session,
            "__verbose_dump_cache_single leaf: "
            "%" PRIu64 " pages, %.2f KB, "
            "%" PRIu64 "/%" PRIu64 " clean/dirty pages, "
            "%.2f /%.2f /%.2f clean/dirty/updates KB, "
            "%.2f KB max page, "
            "%.2f KB max dirty page",
            leaf_pages, (double)leaf_bytes / WT_KILOBYTE, leaf_pages - leaf_dirty_pages,
            leaf_dirty_pages, (double)(leaf_bytes - leaf_dirty_bytes) / WT_KILOBYTE,
            (double)leaf_dirty_bytes / WT_KILOBYTE, (double)updates_bytes / WT_KILOBYTE,
            (double)leaf_bytes_max / WT_KILOBYTE, (double)leaf_dirty_bytes_max / WT_KILOBYTE));

    *total_bytesp += intl_bytes + leaf_bytes;
    *total_dirty_bytesp += intl_dirty_bytes + leaf_dirty_bytes;
    *total_updates_bytesp += updates_bytes;

    return (0);
}

/*
 * __verbose_dump_cache_apply --
 *     Apply dumping cache for all the dhandles.
 */
static int
__verbose_dump_cache_apply(WT_SESSION_IMPL *session, uint64_t *total_bytesp,
  uint64_t *total_dirty_bytesp, uint64_t *total_updates_bytesp)
{
    WT_CONNECTION_IMPL *conn;
    WT_DATA_HANDLE *dhandle;
    WT_DECL_RET;

    conn = S2C(session);
    for (dhandle = NULL;;) {
        WT_DHANDLE_NEXT(session, dhandle, &conn->dhqh, q);
        if (dhandle == NULL)
            break;

        /* Skip if the tree is marked discarded by another thread. */
        if (!WT_DHANDLE_BTREE(dhandle) || !F_ISSET(dhandle, WT_DHANDLE_OPEN) ||
          F_ISSET(dhandle, WT_DHANDLE_DISCARD))
            continue;

        WT_WITH_DHANDLE(session, dhandle,
          ret = __verbose_dump_cache_single(
            session, total_bytesp, total_dirty_bytesp, total_updates_bytesp));
        if (ret != 0)
            WT_RET(ret);
    }
    return (0);
}

/*
 * __wt_verbose_dump_cache --
 *     Output diagnostic information about the cache.
 */
int
__wt_verbose_dump_cache(WT_SESSION_IMPL *session)
{
    WT_CACHE *cache;
    WT_CONNECTION_IMPL *conn;
    WT_DECL_RET;
    double pct;
    uint64_t total_bytes, total_dirty_bytes, total_updates_bytes, cache_bytes_updates;
    bool needed;

    conn = S2C(session);
    cache = conn->cache;
    total_bytes = total_dirty_bytes = total_updates_bytes = cache_bytes_updates = 0;
    pct = 0.0; /* [-Werror=uninitialized] */

    WT_RET(__wt_msg(session, "%s", WT_DIVIDER));
    WT_RET(__wt_msg(session, "cache dump"));

    WT_RET(__wt_msg(session, "cache full: %s", __wt_cache_full(session) ? "yes" : "no"));
    needed = __wt_eviction_clean_needed(session, &pct);
    WT_RET(__wt_msg(session, "cache clean check: %s (%2.3f%%)", needed ? "yes" : "no", pct));
    needed = __wt_eviction_dirty_needed(session, &pct);
    WT_RET(__wt_msg(session, "cache dirty check: %s (%2.3f%%)", needed ? "yes" : "no", pct));
    needed = __wt_eviction_updates_needed(session, &pct);
    WT_RET(__wt_msg(session, "cache updates check: %s (%2.3f%%)", needed ? "yes" : "no", pct));

    WT_WITH_HANDLE_LIST_READ_LOCK(session,
      ret = __verbose_dump_cache_apply(
        session, &total_bytes, &total_dirty_bytes, &total_updates_bytes));
    WT_RET(ret);

    /*
     * Apply the overhead percentage so our total bytes are comparable with the tracked value.
     */
    total_bytes = __wt_cache_bytes_plus_overhead(conn->cache, total_bytes);
    cache_bytes_updates = __wt_cache_bytes_updates(cache);

    WT_RET(__wt_msg(session, "cache dump: total found: %.2f MB vs tracked inuse %.2f MB",
      (double)total_bytes / WT_MEGABYTE, (double)cache->bytes_inmem / WT_MEGABYTE));
    WT_RET(__wt_msg(session, "total dirty bytes: %.2f MB vs tracked dirty %.2f MB",
      (double)total_dirty_bytes / WT_MEGABYTE,
      (double)(cache->bytes_dirty_intl + cache->bytes_dirty_leaf) / WT_MEGABYTE));
    WT_RET(__wt_msg(session, "total updates bytes: %.2f MB vs tracked updates %.2f MB",
      (double)total_updates_bytes / WT_MEGABYTE, (double)cache_bytes_updates / WT_MEGABYTE));

    return (0);
}
