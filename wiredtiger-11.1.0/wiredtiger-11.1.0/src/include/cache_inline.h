/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

/*
 * __wt_cache_aggressive --
 *     Indicate if the cache is operating in aggressive mode.
 */
//说明evict page落盘阻塞严重
static inline bool
__wt_cache_aggressive(WT_SESSION_IMPL *session)
{
    return (S2C(session)->cache->evict_aggressive_score >= WT_EVICT_SCORE_CUTOFF);
}

/*
 * __wt_cache_read_gen --
 *     Get the current read generation number.
 全局的read_gen
 */
static inline uint64_t
__wt_cache_read_gen(WT_SESSION_IMPL *session)
{
    //__wt_cache.read_gen代表全局的read_gen，page.read_gen代表指定表的
    return (S2C(session)->cache->read_gen);
}

/*
 * __wt_cache_read_gen_incr --
 *     Increment the current read generation number.
 __evict_pass中调用
 */
static inline void
__wt_cache_read_gen_incr(WT_SESSION_IMPL *session)
{
    ++S2C(session)->cache->read_gen;
}

/*
 * __wt_cache_read_gen_bump --
 *     Update the page's read generation.
//__wt_cache_read_gen_new: 第一次访问该page的时候获取
//__wt_cache_read_gen_bump: 非第一次访问该page的时候获取

 //evict worker线程如果挑选了该page进行reconcile，则调用__wt_cache_read_gen_bump获取一个新的read_gen
 */
static inline void
__wt_cache_read_gen_bump(WT_SESSION_IMPL *session, WT_PAGE *page)
{
    /* Ignore pages set for forcible eviction. */
    if (page->read_gen == WT_READGEN_OLDEST)
        return;

    /* Ignore pages already in the future. */
    if (page->read_gen > __wt_cache_read_gen(session))
        return;

    /*
     * We set read-generations in the future (where "the future" is measured by increments of the
     * global read generation). The reason is because when acquiring a new hazard pointer for a
     * page, we can check its read generation, and if the read generation isn't less than the
     * current global generation, we don't bother updating the page. In other words, the goal is to
     * avoid some number of updates immediately after each update we have to make.
     */

    //__wt_cache_read_gen(session)也就是cache->read_gen
    //server线程每次在逻辑__evict_pass->__wt_cache_read_gen_incr对cache->read_gen自增，最近调用__wt_cache_read_gen_new的时候cache->read_gen
    //  相对越大，最终该page的page->read_gen也会越大，这样间接反应了page近期是否有被用户线程访问

    //这里一次加100，后面100次对该page的访问，不会走到这里，而是通过前面的>大于号比较返回
    page->read_gen = __wt_cache_read_gen(session) + WT_READGEN_STEP;

    //printf("yang test __wt_cache_read_gen_bump..., page:%p, cache gen:%lu, read_gen:%lu\r\n", 
    //   page, __wt_cache_read_gen(session), page->read_gen);
}

/*
 * __wt_cache_read_gen_new --
 *     Get the read generation for a new page in memory.
  //__wt_cache_read_gen_new: 第一次访问该page的时候获取
 //__wt_cache_read_gen_bump: 非第一次访问该page的时候获取

第一次访问该page，则调用__wt_cache_read_gen_new获取read_gen, 每次修改该page或者访问该page的时候都会获取__wt_cache_read_gen_new，这样如果最近一次访问的page就
 */
static inline void
__wt_cache_read_gen_new(WT_SESSION_IMPL *session, WT_PAGE *page)
{
    WT_CACHE *cache;

    cache = S2C(session)->cache;
    //__wt_cache_read_gen(session)也就是cache->read_gen

    //server线程每次在逻辑__evict_pass->__wt_cache_read_gen_incr对cache->read_gen自增，最近调用__wt_cache_read_gen_new的时候cache->read_gen
    //  相对越大，最终该page的page->read_gen也会越大，这样间接反应了page近期是否有被用户线程访问
    page->read_gen = (__wt_cache_read_gen(session) + cache->read_gen_oldest) / 2;

    //printf("yang test __wt_cache_read_gen_new..., page:%p, cache gen:%lu, cache->read_gen_oldest:%lu, read_gen:%lu\r\n", 
    //   page, __wt_cache_read_gen(session), cache->read_gen_oldest, page->read_gen);
}

/*
 * __wt_cache_stuck --
 *     Indicate if the cache is stuck (i.e., not making progress).
 说明evict server线程阻塞严重
 */
static inline bool
__wt_cache_stuck(WT_SESSION_IMPL *session)
{
    WT_CACHE *cache;

    cache = S2C(session)->cache;
    return (
      cache->evict_aggressive_score == WT_EVICT_SCORE_MAX && F_ISSET(cache, WT_CACHE_EVICT_HARD));
}

/*
 * __wt_page_evict_soon --
 *     Set a page to be evicted as soon as possible.
 */
static inline void
__wt_page_evict_soon(WT_SESSION_IMPL *session, WT_REF *ref)
{
    WT_UNUSED(session);

    ref->page->read_gen = WT_READGEN_OLDEST;
}

/*
 * __wt_page_dirty_and_evict_soon --
 *     Mark a page dirty and set it to be evicted as soon as possible.
 */
static inline int
__wt_page_dirty_and_evict_soon(WT_SESSION_IMPL *session, WT_REF *ref)
{
    WT_RET(__wt_page_modify_init(session, ref->page));
    __wt_page_modify_set(session, ref->page);
    __wt_page_evict_soon(session, ref);

    return (0);
}

/*
 * __wt_cache_pages_inuse --
 *     Return the number of pages in use.
 */
static inline uint64_t
__wt_cache_pages_inuse(WT_CACHE *cache)
{
    return (cache->pages_inmem - cache->pages_evicted);
}

/*
 * __wt_cache_bytes_plus_overhead --
 *     Apply the cache overhead to a size in bytes.
 assume the heap allocator overhead is the specified percentage, and adjust the cache usage by that amount (for example, if there is 10GB of data in cache, a percentage of 10 means WiredTiger treats this as 11GB). This value is configurable because different heap allocators have different overhead and different workloads will have different heap allocation sizes and patterns, therefore applications may need to adjust this value based on allocator choice and behavior in measured workloads.
//实际上真实使用内存会增加一个百分比，因为每种内存分配器都会有额外的开销
 */
static inline uint64_t
__wt_cache_bytes_plus_overhead(WT_CACHE *cache, uint64_t sz)
{
    if (cache->overhead_pct != 0)
        sz += (sz * (uint64_t)cache->overhead_pct) / 100;

    return (sz);
}

/*
 * __wt_cache_bytes_inuse --
 *     Return the number of bytes in use.
 */
static inline uint64_t
__wt_cache_bytes_inuse(WT_CACHE *cache)
{
    return (__wt_cache_bytes_plus_overhead(cache, cache->bytes_inmem));
}

/*
 * __wt_cache_dirty_inuse --
 *     Return the number of dirty bytes in use.
 */
static inline uint64_t
__wt_cache_dirty_inuse(WT_CACHE *cache)
{
    return (
      __wt_cache_bytes_plus_overhead(cache, cache->bytes_dirty_intl + cache->bytes_dirty_leaf));
}

/*
 * __wt_cache_dirty_leaf_inuse --
 *     Return the number of dirty bytes in use by leaf pages.
 */
static inline uint64_t
__wt_cache_dirty_leaf_inuse(WT_CACHE *cache)
{
    return (__wt_cache_bytes_plus_overhead(cache, cache->bytes_dirty_leaf));
}

/*
 * __wt_cache_bytes_updates --
 *     Return the number of bytes in use for updates.
 */
static inline uint64_t
__wt_cache_bytes_updates(WT_CACHE *cache)
{
    return (__wt_cache_bytes_plus_overhead(cache, cache->bytes_updates));
}

/*
 * __wt_cache_bytes_image --
 *     Return the number of page image bytes in use.
 */
static inline uint64_t
__wt_cache_bytes_image(WT_CACHE *cache)
{
    return (
      __wt_cache_bytes_plus_overhead(cache, cache->bytes_image_intl + cache->bytes_image_leaf));
}

/*
 * __wt_cache_bytes_other --
 *     Return the number of bytes in use not for page images.
 */
static inline uint64_t
__wt_cache_bytes_other(WT_CACHE *cache)
{
    uint64_t bytes_other;

    /*
     * Reads can race with changes to the values, so check that the calculation doesn't go negative.
     */
    bytes_other =
      __wt_safe_sub(cache->bytes_inmem, cache->bytes_image_intl + cache->bytes_image_leaf);
    return (__wt_cache_bytes_plus_overhead(cache, bytes_other));
}

/*
 * __wt_session_can_wait --
 *     Return if a session available for a potentially slow operation.
 */
static inline bool
__wt_session_can_wait(WT_SESSION_IMPL *session)
{
    /*
     * Return if a session available for a potentially slow operation; for example, used by the
     * block manager in the case of flushing the system cache.
     */
    if (!F_ISSET(session, WT_SESSION_CAN_WAIT))
        return (false);

    /*
     * LSM sets the "ignore cache size" flag when holding the LSM tree lock, in that case, or when
     * holding the schema lock, we don't want this thread to block for eviction.
     */
    return (!(F_ISSET(session, WT_SESSION_IGNORE_CACHE_SIZE) ||
      FLD_ISSET(session->lock_flags, WT_SESSION_LOCKED_SCHEMA)));
}

/*
 * __wt_eviction_clean_needed --
 *     Return if an application thread should do eviction due to the total volume of data in cache.
//判断cache内存使用占比是否超过了总内存的eviction_trigger(默认95%)
//pct_fullp返回以使用内存占用总内存的百分比的分子部分，例如假设已使用内存暂避99%，则pct_fullp为99
 */
static inline bool
__wt_eviction_clean_needed(WT_SESSION_IMPL *session, double *pct_fullp)
{
    WT_CACHE *cache;
    uint64_t bytes_inuse, bytes_max;

    cache = S2C(session)->cache;

    /*
     * Avoid division by zero if the cache size has not yet been set in a shared cache.
     */
    bytes_max = S2C(session)->cache_size + 1;
    bytes_inuse = __wt_cache_bytes_inuse(cache);

    if (pct_fullp != NULL)
        *pct_fullp = ((100.0 * bytes_inuse) / bytes_max);

    return (bytes_inuse > (cache->eviction_trigger * bytes_max) / 100);
}

/*
 * __wt_eviction_dirty_target --
 *     Return the effective dirty target (including checkpoint scrubbing).
 */
static inline double
__wt_eviction_dirty_target(WT_CACHE *cache)
{
    double dirty_target, scrub_target;

    dirty_target = cache->eviction_dirty_target;
    scrub_target = cache->eviction_scrub_target;

    return (scrub_target > 0 && scrub_target < dirty_target ? scrub_target : dirty_target);
}

/*
 * __wt_eviction_dirty_needed --
 *     Return if an application thread should do eviction due to the total volume of dirty data in
 *     cache.
 */
//判断cache dirty内存使用占比是否超过了总内存的eviction_trigger(默认95%)
//pct_fullp返回以使用内存占用总内存的百分比的分子部分，例如假设已使用内存暂避99%，则pct_fullp为99
static inline bool
__wt_eviction_dirty_needed(WT_SESSION_IMPL *session, double *pct_fullp)
{
    WT_CACHE *cache;
    uint64_t bytes_dirty, bytes_max;

    cache = S2C(session)->cache;

    /*
     * Avoid division by zero if the cache size has not yet been set in a shared cache.
     */
    bytes_dirty = __wt_cache_dirty_leaf_inuse(cache);
    bytes_max = S2C(session)->cache_size + 1;

    if (pct_fullp != NULL)
        *pct_fullp = (100.0 * bytes_dirty) / bytes_max;

    return (bytes_dirty > (uint64_t)(cache->eviction_dirty_trigger * bytes_max) / 100);
}

/*
 * __wt_eviction_updates_needed --
 *     Return if an application thread should do eviction due to the total volume of updates in
 *     cache.
 */
//判断cache update内存使用占比是否超过了总内存的eviction_updates_trigger(默认95%)
//pct_fullp返回以使用内存占用总内存的百分比的分子部分，例如假设已使用内存暂避99%，则pct_fullp为99
static inline bool
__wt_eviction_updates_needed(WT_SESSION_IMPL *session, double *pct_fullp)
{
    WT_CACHE *cache;
    uint64_t bytes_max, bytes_updates;

    cache = S2C(session)->cache;

    /*
     * Avoid division by zero if the cache size has not yet been set in a shared cache.
     */
    bytes_max = S2C(session)->cache_size + 1;
    bytes_updates = __wt_cache_bytes_updates(cache);

    if (pct_fullp != NULL)
        *pct_fullp = (100.0 * bytes_updates) / bytes_max;

    return (bytes_updates > (uint64_t)(cache->eviction_updates_trigger * bytes_max) / 100);
}

/*
 * __wt_btree_dominating_cache --
 *     Return if a single btree is occupying at least half of any of our target's cache usage.
 //确保某个表是否占用了一半触发evict条件的一半的内存(包括dirty update cache中的任何一种)
 */
static inline bool
__wt_btree_dominating_cache(WT_SESSION_IMPL *session, WT_BTREE *btree)
{
    WT_CACHE *cache;
    uint64_t bytes_max;

    cache = S2C(session)->cache;
    bytes_max = S2C(session)->cache_size + 1;

    if (__wt_cache_bytes_plus_overhead(cache, btree->bytes_inmem) >
      (uint64_t)(0.5 * cache->eviction_target * bytes_max) / 100)
        return (true);
    if (__wt_cache_bytes_plus_overhead(cache, btree->bytes_dirty_intl + btree->bytes_dirty_leaf) >
      (uint64_t)(0.5 * cache->eviction_dirty_target * bytes_max) / 100)
        return (true);
    if (__wt_cache_bytes_plus_overhead(cache, btree->bytes_updates) >
      (uint64_t)(0.5 * cache->eviction_updates_target * bytes_max) / 100)
        return (true);

    return (false);
}

/*
 * __wt_eviction_needed --
 *     Return if an application thread should do eviction, and the cache full percentage as a
 *     side-effect.
 //判断是否用户线程需要进行evict操作
 */
static inline bool
__wt_eviction_needed(WT_SESSION_IMPL *session, bool busy, bool readonly, double *pct_fullp)
{
    WT_CACHE *cache;
    double pct_dirty, pct_full, pct_updates;
    bool clean_needed, dirty_needed, updates_needed;

    cache = S2C(session)->cache;

    /*
     * If the connection is closing we do not need eviction from an application thread. The eviction
     * subsystem is already closed.
     */
    if (F_ISSET(S2C(session), WT_CONN_CLOSING))
        return (false);

    //判断cache内存使用占比是否超过了总内存的eviction_trigger(默认95%)
    //pct_full返回以使用内存占用总内存的百分比的分子部分，例如假设已使用内存暂避99%，则pct_full为99
    clean_needed = __wt_eviction_clean_needed(session, &pct_full);
    if (readonly) {
        dirty_needed = updates_needed = false;
        pct_dirty = pct_updates = 0.0;
    } else {
        //判断cache dirty内存使用占比是否超过了总内存的eviction_trigger(默认95%)
        //pct_fullp返回以使用内存占用总内存的百分比的分子部分，例如假设已使用内存暂避99%，则pct_fullp为99
        dirty_needed = __wt_eviction_dirty_needed(session, &pct_dirty);
        //判断cache update内存使用占比是否超过了总内存的eviction_updates_trigger(默认95%)
        //pct_fullp返回以使用内存占用总内存的百分比的分子部分，例如假设已使用内存暂避99%，则pct_fullp为99
        updates_needed = __wt_eviction_updates_needed(session, &pct_updates);
    }

    /*
     * Calculate the cache full percentage; anything over the trigger means we involve the
     * application thread.
     */
    //也就是计算
    if (pct_fullp != NULL)
        //pct_fullp大于100说明，至少有一个超过了用户线程evict的阈值
        *pct_fullp = WT_MAX(0.0,
          100.0 -
            WT_MIN(
              WT_MIN(cache->eviction_trigger - pct_full, cache->eviction_dirty_trigger - pct_dirty),
              cache->eviction_updates_trigger - pct_updates));

    /*
     * Only check the dirty trigger when the session is not busy.
     *
     * In other words, once we are pinning resources, try to finish the operation as quickly as
     * possible without exceeding the cache size. The next transaction in this session will not be
     * able to start until the cache is under the limit.
     */
    return (clean_needed || updates_needed || (!busy && dirty_needed));
}

/*
 * __wt_cache_full --
 *     Return if the cache is at (or over) capacity.
 */
static inline bool
__wt_cache_full(WT_SESSION_IMPL *session)
{
    WT_CACHE *cache;
    WT_CONNECTION_IMPL *conn;

    conn = S2C(session);
    cache = conn->cache;

    return (__wt_cache_bytes_inuse(cache) >= conn->cache_size);
}

/*
 * __wt_cache_hs_dirty --
 *     Return if a major portion of the cache is dirty due to history store content.
 */
static inline bool
__wt_cache_hs_dirty(WT_SESSION_IMPL *session)
{
    WT_CACHE *cache;
    WT_CONNECTION_IMPL *conn;
    uint64_t bytes_max;
    conn = S2C(session);
    cache = conn->cache;
    bytes_max = S2C(session)->cache_size;

    return (__wt_cache_bytes_plus_overhead(cache, cache->bytes_hs_dirty) >=
      ((uint64_t)(cache->eviction_dirty_trigger * bytes_max) / 100));
}

/*
 * __wt_cache_eviction_check --
 *     Evict pages if the cache crosses its boundaries.
 //检查节点已使用内存、脏数据、update数据百分比，判断是否需要用户线程、evict线程进行evict处理

 //判断该现场是否需要做evict操作，如果dirty高该现场也会参与evict操作
 */
static inline int
__wt_cache_eviction_check(WT_SESSION_IMPL *session, bool busy, bool readonly, bool *didworkp)
{
    WT_BTREE *btree;
    WT_TXN_GLOBAL *txn_global;
    WT_TXN_SHARED *txn_shared;
    double pct_full;

    if (didworkp != NULL)
        *didworkp = false;

    /* Eviction causes reconciliation. So don't evict if we can't reconcile */
    if (F_ISSET(session, WT_SESSION_NO_RECONCILE))
        return (0);

    /* If the transaction is prepared don't evict. */
    if (F_ISSET(session->txn, WT_TXN_PREPARE))
        return (0);

    /*
     * If the transaction is a checkpoint cursor transaction, don't try to evict. Because eviction
     * keeps the current transaction snapshot, and the snapshot in a checkpoint cursor transaction
     * can be (and likely is) very old, we won't be able to see anything current to evict and won't
     * be able to accomplish anything useful.
     */
    if (F_ISSET(session->txn, WT_TXN_IS_CHECKPOINT))
        return (0);

    /*
     * If the current transaction is keeping the oldest ID pinned, it is in the middle of an
     * operation. This may prevent the oldest ID from moving forward, leading to deadlock, so only
     * evict what we can. Otherwise, we are at a transaction boundary and we can work harder to make
     * sure there is free space in the cache.
     */
    txn_global = &S2C(session)->txn_global;
    txn_shared = WT_SESSION_TXN_SHARED(session);
    busy = busy || txn_shared->id != WT_TXN_NONE || session->nhazard > 0 ||
      (txn_shared->pinned_id != WT_TXN_NONE && txn_global->current != txn_global->oldest_id);

    /*
     * LSM sets the "ignore cache size" flag when holding the LSM tree lock, in that case, or when
     * holding the handle list, schema or table locks (which can block checkpoints and eviction),
     * don't block the thread for eviction.
     */
    if (F_ISSET(session, WT_SESSION_IGNORE_CACHE_SIZE) ||
      FLD_ISSET(session->lock_flags,
        WT_SESSION_LOCKED_HANDLE_LIST | WT_SESSION_LOCKED_SCHEMA | WT_SESSION_LOCKED_TABLE))
        return (0);

    /* In memory configurations don't block when the cache is full. */
    if (F_ISSET(S2C(session), WT_CONN_IN_MEMORY))
        return (0);

    /*
     * Threads operating on cache-resident trees are ignored because they're not contributing to the
     * problem. We also don't block while reading metadata because we're likely to be holding some
     * other resources that could block checkpoints or eviction.
     */
    btree = S2BT_SAFE(session);
    if (btree != NULL && (F_ISSET(btree, WT_BTREE_IN_MEMORY) || WT_IS_METADATA(session->dhandle)))
        return (0);

    /* Check if eviction is needed. */
    //判断是否用户线程需要进行evict操作，pct_full大于100说明，至少有一个超过了用户线程evict的阈值
    if (!__wt_eviction_needed(session, busy, readonly, &pct_full))
        return (0);

    //到这里说明需要用户线程进行evict操作
    /*
     * Some callers (those waiting for slow operations), will sleep if there was no cache work to
     * do. After this point, let them skip the sleep.
     */
    if (didworkp != NULL)
        *didworkp = true;

    //需要用户线程进行evict操作
    return (__wt_cache_eviction_worker(session, busy, readonly, pct_full));
}
