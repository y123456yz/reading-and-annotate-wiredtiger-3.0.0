/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#define WT_THREAD_PAUSE 10 /* Thread pause timeout in seconds */

/*
 * WT_THREAD --
 *	Encapsulation of a thread that belongs to a thread group.
 */
struct __wt_thread {
    WT_SESSION_IMPL *session;
    u_int id;
    wt_thread_t tid;

/*
 * WT_THREAD and thread-group function flags, merged because WT_THREAD_PANIC_FAIL appears in both
 * groups.
 */
/* AUTOMATIC FLAG VALUE GENERATION START 0 */
#define WT_THREAD_ACTIVE 0x1u     /* Thread is active or paused */
#define WT_THREAD_CAN_WAIT 0x2u   /* WT_SESSION_CAN_WAIT */
#define WT_THREAD_PANIC_FAIL 0x4u /* Panic if the thread fails */
#define WT_THREAD_RUN 0x8u        /* Thread is running */
                                  /* AUTOMATIC FLAG VALUE GENERATION STOP 32 */
    uint32_t flags;

    /*
     * Condition signalled when a thread becomes active. Paused threads wait on this condition.
     */
    //__thread_run中等待
    WT_CONDVAR *pause_cond;

    /* The check function used by all threads. */
    bool (*chk_func)(WT_SESSION_IMPL *session);
    /* The runner function used by all threads. */
    int (*run_func)(WT_SESSION_IMPL *session, WT_THREAD *context);
    /* The stop function used by all threads. */
    int (*stop_func)(WT_SESSION_IMPL *session, WT_THREAD *context);
};

/*
 * WT_THREAD_GROUP --
 *	Encapsulation of a group of utility threads.
 */
struct __wt_thread_group {
    //总的thread，包括活跃的也包括不活跃的
    uint32_t alloc;           /* Size of allocated group */
    uint32_t max;             /* Max threads in group */
    uint32_t min;             /* Min threads in group */
    //WT_THREAD_ACTIVE状态的evict thread
    uint32_t current_threads; /* Number of active threads */

    const char *name; /* Name */

    WT_RWLOCK lock; /* Protects group changes */

    /*
     * Condition signalled when wanting to wake up threads that are part of the group - for example
     * when shutting down. This condition can also be used by group owners to ensure state changes
     * are noticed.
     */
    //__evict_lru_pages  __wt_cache_eviction_worker中等待信号
    //__evict_lru_walk  __wt_page_evict_urgent __thread_group_shrink发送信号
    WT_CONDVAR *wait_cond;

    /*
     * The threads need to be held in an array of arrays, not an array of structures because the
     * array is reallocated as it grows, which causes threads to loose track of their context is
     * realloc moves the memory.
     */
    WT_THREAD **threads;

    //__wt_evict_thread_chk, __wt_evict_thread_run, __wt_evict_thread_stop));
    //赋值见__wt_evict_create
    /* The check function used by all threads. */
    bool (*chk_func)(WT_SESSION_IMPL *session);
    /* The runner function used by all threads. */
    int (*run_func)(WT_SESSION_IMPL *session, WT_THREAD *context);
    /* The stop function used by all threads. May be NULL */
    int (*stop_func)(WT_SESSION_IMPL *session, WT_THREAD *context);
};
