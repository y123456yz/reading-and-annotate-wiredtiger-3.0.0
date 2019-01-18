/*-
 * Copyright (c) 2014-2017 MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#define	WT_THREAD_PAUSE		10	/* Thread pause timeout in seconds */

/*
 * WT_THREAD --
 *	Encapsulation of a thread that belongs to a thread group.
 */
//线程相关，参考__thread_group_resize
struct __wt_thread {
	WT_SESSION_IMPL *session;
	u_int id; //在WT_THREAD_GROUP->threads[]中的位置
	wt_thread_t tid;

	/*
	 * WT_THREAD and thread-group function flags, merged because
	 * WT_THREAD_PANIC_FAIL appears in both groups.
	 */
#define	WT_THREAD_ACTIVE	0x01	/* thread is active or paused */
#define	WT_THREAD_CAN_WAIT	0x02	/* WT_SESSION_CAN_WAIT */
#define	WT_THREAD_LOOKASIDE	0x04	/* open lookaside cursor */
#define	WT_THREAD_PANIC_FAIL	0x08	/* panic if the thread fails */
#define	WT_THREAD_RUN		0x10	/* thread is running */
	uint32_t flags;

	/*
	 * Condition signalled when a thread becomes active.  Paused
	 * threads wait on this condition.
	 */
	WT_CONDVAR      *pause_cond;

	/* The check function used by all threads. */
	bool (*chk_func)(WT_SESSION_IMPL *session);
	/* The runner function used by all threads. */
	//__thread_run中运行
	int (*run_func)(WT_SESSION_IMPL *session, WT_THREAD *context);
	/* The stop function used by all threads. */
	int (*stop_func)(WT_SESSION_IMPL *session, WT_THREAD *context);
};

/*
 * WT_THREAD_GROUP --
 *	Encapsulation of a group of utility threads.
 */
//__wt_connection_impl.evict_threads为该类型，初始化见__wt_thread_group_create
struct __wt_thread_group {
	uint32_t	 alloc;		/* Size of allocated group */
	//创建的可用线程数
	uint32_t	 max;		/* Max threads in group */
	//激活的线程数，也就是处于running运行状态的线程数，参考__thread_group_resize
	uint32_t	 min;		/* Min threads in group */
	//向前线程数  __wt_thread_group_start_one激活一个线程就加1
	uint32_t	 current_threads;/* Number of active threads */

	const char	*name;		/* Name */

	WT_RWLOCK	lock;		/* Protects group changes */

	/*
	 * Condition signalled when wanting to wake up threads that are
	 * part of the group - for example when shutting down. This condition
	 * can also be used by group owners to ensure state changes are noticed.
	 */
	WT_CONDVAR      *wait_cond;

	/*
	 * The threads need to be held in an array of arrays, not an array of
	 * structures because the array is reallocated as it grows, which
	 * causes threads to loose track of their context is realloc moves the
	 * memory.
	 */
	//该线程组对应的线程详细信息都在该数组里面
	WT_THREAD **threads;

	/* The check function used by all threads. */
	bool (*chk_func)(WT_SESSION_IMPL *session);
	/* The runner function used by all threads. */
	int (*run_func)(WT_SESSION_IMPL *session, WT_THREAD *context);
	/* The stop function used by all threads. May be NULL */
	int (*stop_func)(WT_SESSION_IMPL *session, WT_THREAD *context);
};
