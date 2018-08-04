/*-
 * Copyright (c) 2014-2017 MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#define	MAX_ASYNC_SLEEP_USECS	100000	/* Maximum sleep waiting for work */
#define	MAX_ASYNC_YIELD		200	/* Maximum number of yields for work */

#define	O2C(op)	((WT_CONNECTION_IMPL *)(op)->iface.connection)
#define	O2S(op)								\
    (((WT_CONNECTION_IMPL *)(op)->iface.connection)->default_session)
/*
 * WT_ASYNC_FORMAT --
 *	The URI/config/format cache.
 */ //赋值见__async_get_format
struct __wt_async_format {
	TAILQ_ENTRY(__wt_async_format) q;
	const char	*config;
	uint64_t	cfg_hash;		/* Config hash */
	const char	*uri;
	uint64_t	uri_hash;		/* URI hash */
	const char	*key_format;
	const char	*value_format;
};

//__wt_async_op_enqueue

#define	WT_ASYNCOP_ENQUEUED	0	/* Placed on the work queue */
#define	WT_ASYNCOP_FREE		1	/* Able to be allocated to user */
#define	WT_ASYNCOP_READY	2	/* Allocated, ready for user to use */
#define	WT_ASYNCOP_WORKING	3	/* Operation in progress by worker */

/*
 * WT_ASYNC_OP_IMPL --
 *	Implementation of the WT_ASYNC_OP.
 */
//__wt_async_new_op中赋值
struct __wt_async_op_impl {
	WT_ASYNC_OP	iface;

	WT_ASYNC_CALLBACK	*cb;

	uint32_t	internal_id;	/* Array position id. */
	uint64_t	unique_id;	/* Unique identifier. */

    //赋值见__async_get_format
	WT_ASYNC_FORMAT *format;	/* Format structure */

	uint32_t	state;
    //赋值__async_op_wrap
	WT_ASYNC_OPTYPE	optype;		/* Operation type */
};

/*
 * Definition of the async subsystem.
 */
struct __wt_async {
	/*
	 * Ops array protected by the ops_lock.
	 */
	WT_SPINLOCK		 ops_lock;      /* Locked: ops array */
	//数组成员初始化__async_op_init  __async_new_op_alloc中查找使用
	WT_ASYNC_OP_IMPL	 *async_ops;	/* Async ops */
#define	OPS_INVALID_INDEX	0xffffffff
    //记录下次从async_ops[]数组中查找可用op的位置
	uint32_t		 ops_index;	/* Active slot index */
	uint64_t		 op_id;		/* Unique ID counter */
	//数组大小async_qsize  op队列，一个数组  参考__wt_async_op_enqueue  __async_op_dequeue
	WT_ASYNC_OP_IMPL	 **async_queue;	/* Async ops work queue */
	//conn->async_size + 2 见__wt_async_op_init
	uint32_t		 async_qsize;	/* Async work queue size */
	/*
	 * We need to have two head and tail values.  All but one is
	 * maintained as an ever increasing value to ease wrap around.
	 *
	 * alloc_head: the next one to allocate for producers.
	 * head: the current head visible to consumers.
	 * head is always <= alloc_head.
	 * alloc_tail: the next slot for consumers to dequeue.
	 * alloc_tail is always <= head.
	 * tail_slot: the last slot consumed.
	 * A producer may need wait for tail_slot to advance.
	 */ //赋值见__wt_async_op_enqueue  没插入一个KV就增1
	uint64_t		 alloc_head;	/* Next slot to enqueue */
	//赋值见__wt_async_op_enqueue
	uint64_t		 head;		/* Head visible to worker */
	uint64_t		 alloc_tail;	/* Next slot to dequeue */
	//async_queue[]上进行消费的队列，赋值见__async_op_dequeue
	uint64_t		 tail_slot;	/* Worker slot consumed */

	TAILQ_HEAD(__wt_async_format_qh, __wt_async_format) formatqh;
	//当前队列中op数  __wt_async_op_enqueue  __async_op_dequeue
	uint32_t		 cur_queue;	/* Currently enqueued */
	//队列中op数最多的时候为多少 __wt_async_op_enqueue
	uint32_t		 max_queue;	/* Maximum enqueued */
    
    //赋值为下面的WT_ASYNC_FLUSH_NONE等
	uint32_t	 	 flush_state;

	/* Notify any waiting threads when flushing is done. */
	WT_CONDVAR		*flush_cond;
	WT_ASYNC_OP_IMPL	 flush_op;	/* Special flush op */
	uint32_t		 flush_count;	/* Worker count */
	uint64_t		 flush_gen;	/* Flush generation number */

#define	WT_ASYNC_MAX_WORKERS	20
    //赋值见__async_start
	WT_SESSION_IMPL		*worker_sessions[WT_ASYNC_MAX_WORKERS];
					/* Async worker threads */
	wt_thread_t		 worker_tids[WT_ASYNC_MAX_WORKERS];

	uint32_t		 flags;	/* Currently unused. */
};

#define	WT_ASYNC_FLUSH_NONE		0	/* No flush in progress */
#define	WT_ASYNC_FLUSH_COMPLETE		1	/* Notify flush caller done */
#define	WT_ASYNC_FLUSH_IN_PROGRESS	2	/* Prevent other callers */
#define	WT_ASYNC_FLUSHING		3	/* Notify workers */


/*
 * WT_ASYNC_CURSOR --
 *	Async container for a cursor.  Each async worker thread
 *	has a cache of async cursors to reuse for operations.
 */
struct __wt_async_cursor {
	TAILQ_ENTRY(__wt_async_cursor) q;	/* Worker cache */
	uint64_t	cfg_hash;		/* Config hash */
	uint64_t	uri_hash;		/* URI hash */
	WT_CURSOR	*c;			/* WT cursor */
};

/*
 * WT_ASYNC_WORKER_STATE --
 *	State for an async worker thread.
 */
struct __wt_async_worker_state {
	uint32_t	id;
	TAILQ_HEAD(__wt_cursor_qh, __wt_async_cursor)	cursorqh;
	uint32_t	num_cursors;
};
