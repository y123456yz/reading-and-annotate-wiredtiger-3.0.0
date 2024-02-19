/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

/*
 * This is an implementation of condition variables that automatically adjust the wait time
 * depending on whether the wake is resulting in useful work.
 */

/*
 * __wt_cond_auto_alloc --
 *     Allocate and initialize an automatically adjusting condition variable.
 */
int
__wt_cond_auto_alloc(
  WT_SESSION_IMPL *session, const char *name, uint64_t min, uint64_t max, WT_CONDVAR **condp)
{
    WT_CONDVAR *cond;

    WT_RET(__wt_cond_alloc(session, name, condp));
    cond = *condp;

    cond->min_wait = min;
    cond->max_wait = max;
    cond->prev_wait = min;

    return (0);
}

/*
 * __wt_cond_auto_wait_signal --
 *     Wait on a mutex, optionally timing out. If we get it before the time out period expires, let
 *     the caller know.
 等待cond信号到来，并动态调整等待时间
 */
void
__wt_cond_auto_wait_signal(WT_SESSION_IMPL *session, WT_CONDVAR *cond, 
  //本次cond等待是直接使用min_wait，还是使用上一次的prev_wait并进行动态调整等待时间
  bool progress,
  bool (*run_func)(WT_SESSION_IMPL *), 
  //本次wait完成后，是否需要把prev_wait恢复为min_wait;
  //如果是因为超时返回signalled会被置位false，同时需要把prev_wait恢复为min_wait;
  bool *signalled)
{
    uint64_t delta, saved_prev_wait;

    /*
     * Catch cases where this function is called with a condition variable that wasn't initialized
     * to do automatic adjustments.
     */
    WT_ASSERT(session, cond->min_wait != 0);

    WT_STAT_CONN_INCR(session, cond_auto_wait);
    if (progress)
        cond->prev_wait = cond->min_wait;
    else {
        delta = WT_MAX(1, (cond->max_wait - cond->min_wait) / 10);
        /*
         * Try to update the previous wait value for the condition variable. There can be multiple
         * threads doing this concurrently, so use atomic operations to make sure the value remains
         * within the bounds of the maximum configured. Don't retry if our update didn't make it in
         * - it's not necessary for the previous wait time to be updated every time.
         */
        WT_ORDERED_READ(saved_prev_wait, cond->prev_wait);
        if (!__wt_atomic_cas64(
              &cond->prev_wait, saved_prev_wait, WT_MIN(cond->max_wait, saved_prev_wait + delta)))
            //auto adjusting condition wait raced to update timeout and skipped updating'
            WT_STAT_CONN_INCR(session, cond_auto_wait_skipped);
    }

    //如果是因为超时返回signalled会被置位false
    __wt_cond_wait_signal(session, cond, cond->prev_wait, run_func, signalled);

    if (progress || *signalled)
        //auto adjusting condition resets
        WT_STAT_CONN_INCR(session, cond_auto_wait_reset);
    //本次wait完成后，如果signalled为TRUE则恢复prev_wait为初始值min_wait
    //说明是因为收到了其他线程发送的cond信号，而不是超时,这时候就需要恢复等待时间为min_wait
    if (*signalled)
        cond->prev_wait = cond->min_wait;
}

/*
 * __wt_cond_auto_wait --
 *     Wait on a mutex, optionally timing out. If we get it before the time out period expires, let
 *     the caller know.
 */
void
__wt_cond_auto_wait(
  WT_SESSION_IMPL *session, WT_CONDVAR *cond, bool progress, bool (*run_func)(WT_SESSION_IMPL *))
{
    bool notused;

    __wt_cond_auto_wait_signal(session, cond, progress, run_func, &notused);
}
