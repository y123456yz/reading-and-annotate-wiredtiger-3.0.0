/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"
//判断dhandle这个表是否可以从
#define WT_DHANDLE_CAN_DISCARD(dhandle)                                                            \
    (!F_ISSET(dhandle, WT_DHANDLE_EXCLUSIVE | WT_DHANDLE_OPEN) && (dhandle)->session_inuse == 0 && \
      (dhandle)->session_ref == 0)

/*
 * __sweep_mark --
 *     Mark idle handles with a time of death, and note if we see dead handles.
 */
static void
__sweep_mark(WT_SESSION_IMPL *session, uint64_t now)
{
    WT_CONNECTION_IMPL *conn;
    WT_DATA_HANDLE *dhandle;

    conn = S2C(session);

    TAILQ_FOREACH (dhandle, &conn->dhqh, q) {
       // printf("[DEBUG] __sweep_mark: dhandle=%p, name:%s, session_ref=%d\n", (void*)dhandle, dhandle->name, (int)dhandle->session_ref);
        if (WT_IS_METADATA(dhandle))
            continue;

        /*
         * There are some internal increments of the in-use count such as eviction. Don't keep
         * handles alive because of those cases, but if we see multiple cursors open, clear the time
         * of death.
         */
        if (dhandle->session_inuse > 1)
            dhandle->timeofdeath = 0;

        /*
         * If the handle is open exclusive or currently in use, or the time of death is already set,
         * move on.
         */
        if (F_ISSET(dhandle, WT_DHANDLE_EXCLUSIVE) || dhandle->session_inuse > 0 ||
          dhandle->timeofdeath != 0)
            continue;

        dhandle->timeofdeath = now;
        //"connection sweep time-of-death sets" 这里代表更新dhandle时间的次数，这并不代表这些更新了timeofdeath的dhanle就是dead的handle
        WT_STAT_CONN_INCR(session, dh_sweep_tod);
    }
}

/*
 * __sweep_close_dhandle_locked --
 *     Close write-locked dhandle.  
 */
static int
__sweep_close_dhandle_locked(WT_SESSION_IMPL *session)
{
    WT_BTREE *btree;
    WT_DATA_HANDLE *dhandle;

    dhandle = session->dhandle;
    btree = WT_DHANDLE_BTREE(dhandle) ? dhandle->handle : NULL;

    /* This method expects dhandle write lock. */
    WT_ASSERT(session, FLD_ISSET(dhandle->lock_flags, WT_DHANDLE_LOCK_WRITE));

    /* Only sweep clean trees. */
    if (btree != NULL && btree->modified) {
      //  printf("yang test .....1......__sweep_close_dhandle_locked......btree:%p.......\r\n", btree);
        return (0);
    }
    
    /*
     * Mark the handle dead and close the underlying handle.
     *
     * For btree handles, closing the handle decrements the open file count, meaning the close loop
     * won't overrun the configured minimum.
     */
   // printf("yang test ......2.....__sweep_close_dhandle_locked.............\r\n");
    return (__wt_conn_dhandle_close(session, false, true));
}

/*
 * __sweep_expire_one --
 *     Mark a single handle dead.
 */
static int
__sweep_expire_one(WT_SESSION_IMPL *session)
{
    WT_DECL_RET;

    /*
     * Acquire an exclusive lock on the handle and mark it dead.
     *
     * The close would require I/O if an update cannot be written (updates in a no-longer-referenced
     * file might not yet be globally visible if sessions have disjoint sets of files open). In that
     * case, skip it: we'll retry the close the next time, after the transaction state has
     * progressed.
     *
     * We don't set WT_DHANDLE_EXCLUSIVE deliberately, we want opens to block on us and then retry
     * rather than returning an EBUSY error to the application. This is done holding the handle list
     * lock so that connection-level handle searches never need to retry.
     */
    WT_WITH_DHANDLE_WRITE_LOCK_NOWAIT(session, ret, ret = __sweep_close_dhandle_locked(session));

    return (ret);
}

/*
 * __sweep_expire --
 *     Mark trees dead if they are clean and haven't been accessed recently, until we have reached
 *     the configured minimum number of handles.

sweep线程:
 __sweep_expire: 选择不被任何session引用并且该表不活跃的表，通过__sweep_expire->__sweep_expire_one->__sweep_close_dhandle_locked->__wt_conn_dhandle_close标记为dead
 __sweep_discard_trees: close所有dead的表资源
 __sweep_remove_handles: 从conn.dhqh和conn.dhhash中删除所有没有被任何session引用的表，也就是不活跃的表从conn.dhqh和dhhash中移除


 
__sweep_expire: 选择不被任何session引用并且该表不活跃的表，通过__sweep_expire->__sweep_expire_one->__sweep_close_dhandle_locked->__wt_conn_dhandle_close标记为dead
 */
static int
__sweep_expire(WT_SESSION_IMPL *session, uint64_t now)
{
    WT_CONNECTION_IMPL *conn;
    WT_DATA_HANDLE *dhandle;
    WT_DECL_RET;

    conn = S2C(session);

    //__wt_verbose_dump_sessions  __wt_verbose_dump_handles
   // (void)conn->iface.debug_info(&conn->iface, "cursors, handles");
    TAILQ_FOREACH (dhandle, &conn->dhqh, q) {
       // printf("[DEBUG] __sweep_expire: dhandle=%p, name:%s, session_ref=%d\n", (void*)dhandle, dhandle->name, (int)dhandle->session_ref);
        /*
         * Ignore open files once the btree file count is below the minimum number of handles.
         */
        if (conn->open_btree_count < conn->sweep_handles_min)
            break;

        //printf("yang test ....11111....__sweep_expire..........name:%s %p, Checkpoint: %s, session inuse:%d, ref session count:%u,    %d, %d, %d\r\n", 
        //    dhandle->name, dhandle, dhandle->checkpoint, dhandle->session_inuse, dhandle->session_ref,
        //    (int)(now - dhandle->timeofdeath), (int)conn->sweep_idle_time, F_ISSET(dhandle, WT_DHANDLE_OPEN));
        
        if (WT_IS_METADATA(dhandle) || !F_ISSET(dhandle, WT_DHANDLE_OPEN) ||
          dhandle->session_inuse != 0 || dhandle->timeofdeath == 0 ||
          now - dhandle->timeofdeath <= conn->sweep_idle_time)
            continue;

        //到这里说明该dhandle已经长时间不活跃了
        /*
         * For tables, we need to hold the table lock to avoid racing with cursor opens.
         */
        if (dhandle->type == WT_DHANDLE_TYPE_TABLE)
            WT_WITH_TABLE_WRITE_LOCK(
              session, WT_WITH_DHANDLE(session, dhandle, ret = __sweep_expire_one(session)));
        else
            WT_WITH_DHANDLE(session, dhandle, ret = __sweep_expire_one(session));
        WT_RET_BUSY_OK(ret);

        //printf("yang test ....22222....__sweep_expire..........name:%s %p, Checkpoint: %s, session inuse:%d, ref session count:%u,    %d, %d, %d\r\n", 
        //    dhandle->name, dhandle, dhandle->checkpoint, dhandle->session_inuse, dhandle->session_ref,
        //    (int)(now - dhandle->timeofdeath), (int)conn->sweep_idle_time, F_ISSET(dhandle, WT_DHANDLE_OPEN));
    }
   // (void)conn->iface.debug_info(&conn->iface, "cursors, handles");
    printf("__sweep_expire 222222222222222222222222222 \n\n\n\n\n");
    
    return (0);
}

/*
 * __sweep_discard_trees --
 *     Discard pages from dead trees.

 sweep线程:
  __sweep_expire: 选择不被任何session引用并且该表不活跃的表，通过__sweep_expire->__sweep_expire_one->__sweep_close_dhandle_locked->__wt_conn_dhandle_close标记为dead
  __sweep_discard_trees: close所有dead的表资源
  __sweep_remove_handles: 从conn.dhqh和conn.dhhash中删除所有没有被任何session引用的表，也就是不活跃的表从conn.dhqh和dhhash中移除
 

 从内存中清除close dead的表，通过dead_handlesp返回close掉的表个数  
 */
static int
__sweep_discard_trees(WT_SESSION_IMPL *session, u_int *dead_handlesp)
{
    WT_CONNECTION_IMPL *conn;
    WT_DATA_HANDLE *dhandle;
    WT_DECL_RET;

    *dead_handlesp = 0;

    conn = S2C(session);

    TAILQ_FOREACH (dhandle, &conn->dhqh, q) {
        //yang add todo xxxxxxxxxxxxxxxx dead_handlesp应该算重了，这里去掉比较好
        if (WT_DHANDLE_CAN_DISCARD(dhandle)) {
            ++*dead_handlesp;
            //printf("yang test ..11111111....__sweep_discard_trees..... handle:%s\r\n", dhandle->name);
        } else {
            //printf("yang test ..22222222....__sweep_discard_trees..... handle:%s\r\n", dhandle->name);
        }

       // printf("[DEBUG] __sweep_discard_trees: 1111 dhandle=%p, name:%s, session_ref=%d\n", (void*)dhandle, dhandle->name, (int)dhandle->session_ref);
        
        //只close dead的表，dhandle如果是"table:"， 则不满足WT_DHANDLE_OPEN，所以下面的dh_sweep_close实际上只会统计file:的
        // 这也是"dh_sweep_close和dh_sweep_remove统计中，百万库表的脚步看这里差了将近一倍"的原因
        if (!F_ISSET(dhandle, WT_DHANDLE_OPEN) || !F_ISSET(dhandle, WT_DHANDLE_DEAD))
            continue;

        printf("[DEBUG] __sweep_discard_trees: 2222 dhandle=%p, name:%s, session_ref=%d\n", (void*)dhandle, dhandle->name, (int)dhandle->session_ref);
        
        /* If the handle is marked dead, flush it from cache. */
        WT_WITH_DHANDLE(session, dhandle, ret = __wt_conn_dhandle_close(session, false, false));

        /* We closed the btree handle. */
        if (ret == 0) {
            //connection sweep dhandles closed
            WT_STAT_CONN_INCR(session, dh_sweep_close);
            ++*dead_handlesp;
            //printf("yang test ..1....__sweep_discard_trees............dead handle:%u\r\n", *dead_handlesp);
        } else {
            //printf("yang test ..2....__sweep_discard_trees............dead handle:%u\r\n", *dead_handlesp);
            WT_STAT_CONN_INCR(session, dh_sweep_ref);
        }
        
        WT_RET_BUSY_OK(ret);
    }
    //printf("yang test ....__sweep_discard_trees............dead_handlesp:%u, conn->dhandle_count:%d\r\n", 
    //    *dead_handlesp, (int)conn->dhandle_count);

    return (0);
}

/*
 * __sweep_remove_one --
 *     Remove a closed handle from the connection list.
 __sweep_server->__sweep_remove_handles->__sweep_remove_one
 
  从conn.dhqh和conn.dhhash中删除session对应的session->dhandle
 */
static int
__sweep_remove_one(WT_SESSION_IMPL *session)
{
    WT_DECL_RET;

    /* Try to get exclusive access. */
    WT_RET(__wt_session_dhandle_try_writelock(session));

    /*
     * If there are no longer any references to the handle in any sessions, attempt to discard it.
     */
    if (!WT_DHANDLE_CAN_DISCARD(session->dhandle))
        WT_ERR(EBUSY);

    ret = __wt_conn_dhandle_discard_single(session, false, true);

    /*
     * If the handle was not successfully discarded, unlock it and don't retry the discard until it
     * times out again.
     */
    if (ret != 0) {
err:
        __wt_session_dhandle_writeunlock(session);
    }

    return (ret);
}

/*
 * __sweep_remove_handles --
 *     Remove closed handles from the connection list.
sweep线程:
 __sweep_expire: 选择不被任何session引用并且该表不活跃的表，通过__sweep_expire->__sweep_expire_one->__sweep_close_dhandle_locked->__wt_conn_dhandle_close标记为dead
 __sweep_discard_trees: close所有dead的表资源
 __sweep_remove_handles: 从conn.dhqh和conn.dhhash中删除所有没有被任何session引用的表，也就是不活跃的表从conn.dhqh和dhhash中移除


__sweep_server->__sweep_remove_handles
*/
static int
__sweep_remove_handles(WT_SESSION_IMPL *session)
{
    WT_CONNECTION_IMPL *conn;
    WT_DATA_HANDLE *dhandle, *dhandle_tmp;
    WT_DECL_RET;

    conn = S2C(session);
    //__wt_verbose_dump_sessions  __wt_verbose_dump_handles
   // (void)conn->iface.debug_info(&conn->iface, "sessions, cursors, handles");
    TAILQ_FOREACH_SAFE(dhandle, &conn->dhqh, q, dhandle_tmp)
    {
      //  printf("[DEBUG] __sweep_remove_handles: dhandle=%p, name:%s, session_ref=%d\n", (void*)dhandle, dhandle->name, (int)dhandle->session_ref);
        if (WT_IS_METADATA(dhandle))
            continue;

        //if (F_ISSET(dhandle, WT_DHANDLE_EXCLUSIVE | WT_DHANDLE_OPEN))
        // printf("yang test ...1.....__sweep_remove_handles.........name:%s, %d, %d\r\n", dhandle->name, 
        //    (int)dhandle->session_inuse, (int)dhandle->session_ref);

        if (!WT_DHANDLE_CAN_DISCARD(dhandle))   
            continue;

        printf("yang test ......2..__sweep_remove_handles.....name:%s, %p\r\n", dhandle->name, dhandle);
        if (dhandle->type == WT_DHANDLE_TYPE_TABLE)
            WT_WITH_TABLE_WRITE_LOCK(session,
              WT_WITH_HANDLE_LIST_WRITE_LOCK(
                session, WT_WITH_DHANDLE(session, dhandle, ret = __sweep_remove_one(session))));
        else
            WT_WITH_HANDLE_LIST_WRITE_LOCK(
              session, WT_WITH_DHANDLE(session, dhandle, ret = __sweep_remove_one(session)));
        if (ret == 0)
            //'connection sweep dhandles removed from hash list')
            //yang add todo xxxxxxxxxx   dh_sweep_close和dh_sweep_remove统计中，百万库表的脚步看这里差了将近一倍，但是从代码逻辑看应该相等才对
            // 这是因为一个表一般包含"table:"和"file:"两个schema，close只会统计file:，所以只统计一次，但是remove这里会统计"table:"和"file:",所以是两次
            /*
                   dh_sweep_close     "connection sweep dhandles closed" : 185943,
                   dh_sweep_remove     "connection sweep dhandles removed from hash list" : 372219,
              */
            WT_STAT_CONN_INCR(session, dh_sweep_remove);
        else
            WT_STAT_CONN_INCR(session, dh_sweep_ref);
        WT_RET_BUSY_OK(ret);
    }

    //printf("\r\n\r\n\r\n\r\n\r\n");
    //TAILQ_FOREACH_SAFE(dhandle, &conn->dhqh, q, dhandle_tmp) {
    //    printf("yang test ................all dhandle:%s\r\n", dhandle->name);
    //}
    return (ret == EBUSY ? 0 : ret);
}

/*
 * __sweep_server_run_chk --
 *     Check to decide if the sweep server should continue running.
 */
static bool
__sweep_server_run_chk(WT_SESSION_IMPL *session)
{
    return (FLD_ISSET(S2C(session)->server_flags, WT_CONN_SERVER_SWEEP));
}

#include <sys/resource.h>
#include <stdio.h>

static void print_memory_usage(void) {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    printf("@@@@@@@@@@@@@@@@@@@@@@@@@ Memory usage: %ld KB\n", usage.ru_maxrss);
}

/*
 * __sweep_server --
 *     The handle sweep server thread.
 */
static WT_THREAD_RET
__sweep_server(void *arg)
{
    WT_CONNECTION_IMPL *conn;
    WT_DECL_RET;
    WT_SESSION_IMPL *session;
    uint64_t last, now;
    uint64_t sweep_interval;
    u_int dead_handles;
    bool cv_signalled;

    session = arg;
    conn = S2C(session);
    if (FLD_ISSET(conn->timing_stress_flags, WT_TIMING_STRESS_AGGRESSIVE_SWEEP))
        sweep_interval = conn->sweep_interval / 10;
    else
        sweep_interval = conn->sweep_interval;

    /*
     * Sweep for dead and excess handles.
     */
    __wt_seconds(session, &last);
    for (;;) {
        /* Wait until the next event. */
        if (FLD_ISSET(conn->timing_stress_flags, WT_TIMING_STRESS_AGGRESSIVE_SWEEP))
            __wt_cond_wait_signal(session, conn->sweep_cond,
              conn->sweep_interval * 100 * WT_THOUSAND, __sweep_server_run_chk, &cv_signalled);
        else
            __wt_cond_wait_signal(session, conn->sweep_cond, conn->sweep_interval * WT_MILLION,
              __sweep_server_run_chk, &cv_signalled);

        /* Check if we're quitting or being reconfigured. */
        if (!__sweep_server_run_chk(session))
            break;

        __wt_seconds(session, &now);

        /*
         * See if it is time to sweep the data handles. Those are swept less frequently than the
         * history store table by default and the frequency is controlled by a user setting. We want
         * to avoid sweeping while checkpoint is gathering handles. Both need to lock the dhandle
         * list and sweep acquiring that lock can interfere with checkpoint and cause it to take
         * longer. Sweep is an operation that typically has long intervals so skipping some for
         * checkpoint should have little impact.
         */
        if (!cv_signalled && (now - last < sweep_interval))
            continue;
        if (F_ISSET(conn, WT_CONN_CKPT_GATHER)) {
            WT_STAT_CONN_INCR(session, dh_sweep_skip_ckpt);
            continue;
        }

        printf("\r\n\r\n\r\nyang test ...1... sweep_server,   ");
        print_memory_usage();
        WT_STAT_CONN_INCR(session, dh_sweeps);
        /*
         * Mark handles with a time of death, and report whether any handles are marked dead. If
         * sweep_idle_time is 0, handles never become idle.
         */
        if (conn->sweep_idle_time != 0)
            __sweep_mark(session, now);

        /*
         * Close handles if we have reached the configured limit. If sweep_idle_time is 0, handles
         * never become idle.

           __sweep_expire: 选择不被任何session引用并且该表不活跃的表，通过
              __sweep_expire->__sweep_expire_one->__sweep_close_dhandle_locked->__wt_conn_dhandle_close标记为dead
         */
        if (conn->sweep_idle_time != 0 && conn->open_btree_count >= conn->sweep_handles_min)
            WT_ERR(__sweep_expire(session, now));

        // 从内存中清除close dead的表，通过dead_handles返回close掉的表个数
        WT_ERR(__sweep_discard_trees(session, &dead_handles));

        printf("yang test ...2... sweep_server,   ");
        print_memory_usage();
        
        if (dead_handles > 0)
            WT_ERR(__sweep_remove_handles(session));
        
        printf("yang test ...3... sweep_server,   ");
        print_memory_usage();
        /* Remember the last sweep time. */
        last = now;
    }

    if (0) {
err:
        WT_IGNORE_RET(__wt_panic(session, ret, "handle sweep server error"));
    }
    return (WT_THREAD_RET_VALUE);
}

/*
 * __wt_sweep_config --
 *     Pull out sweep configuration settings
 */
int
__wt_sweep_config(WT_SESSION_IMPL *session, const char *cfg[])
{
    WT_CONFIG_ITEM cval;
    WT_CONNECTION_IMPL *conn;

    conn = S2C(session);

    /*
     * A non-zero idle time is incompatible with in-memory, and the default is non-zero; set the
     * in-memory configuration idle time to zero.
     */
    conn->sweep_idle_time = 0;
    WT_RET(__wt_config_gets(session, cfg, "in_memory", &cval)); 
    if (cval.val == 0) {
        WT_RET(__wt_config_gets(session, cfg, "file_manager.close_idle_time", &cval));
        conn->sweep_idle_time = (uint64_t)cval.val;
    }

    WT_RET(__wt_config_gets(session, cfg, "file_manager.close_scan_interval", &cval));
    conn->sweep_interval = (uint64_t)cval.val;

    WT_RET(__wt_config_gets(session, cfg, "file_manager.close_handle_minimum", &cval));
    conn->sweep_handles_min = (uint64_t)cval.val;

    return (0);
}

/*
 * __wt_sweep_create --
 *     Start the handle sweep thread.
 */
int
__wt_sweep_create(WT_SESSION_IMPL *session)
{
    WT_CONNECTION_IMPL *conn;
    uint32_t session_flags;

    conn = S2C(session);

    /* Set first, the thread might run before we finish up. */
    FLD_SET(conn->server_flags, WT_CONN_SERVER_SWEEP);

    /*
     * Handle sweep does enough I/O it may be called upon to perform slow operations for the block
     * manager. Sweep should not block due to the cache being full.
     */
    session_flags = WT_SESSION_CAN_WAIT | WT_SESSION_IGNORE_CACHE_SIZE;
    WT_RET(__wt_open_internal_session(
      conn, "sweep-server", true, session_flags, 0, &conn->sweep_session));
    session = conn->sweep_session;

    WT_RET(__wt_cond_alloc(session, "handle sweep server", &conn->sweep_cond));

    WT_RET(__wt_thread_create(session, &conn->sweep_tid, __sweep_server, session));
    conn->sweep_tid_set = 1;

    return (0);
}

/*
 * __wt_sweep_destroy --
 *     Destroy the handle-sweep thread.
 */
int
__wt_sweep_destroy(WT_SESSION_IMPL *session)
{
    WT_CONNECTION_IMPL *conn;
    WT_DECL_RET;

    conn = S2C(session);

    FLD_CLR(conn->server_flags, WT_CONN_SERVER_SWEEP);
    if (conn->sweep_tid_set) {
        __wt_cond_signal(session, conn->sweep_cond);
        WT_TRET(__wt_thread_join(session, &conn->sweep_tid));
        conn->sweep_tid_set = 0;
    }
    __wt_cond_destroy(session, &conn->sweep_cond);

    if (conn->sweep_session != NULL) {
        WT_TRET(__wt_session_close_internal(conn->sweep_session));

        conn->sweep_session = NULL;
    }

    return (ret);
}
