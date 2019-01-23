/*-
 * Copyright (c) 2014-2017 MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

/*
 * WT_DATA_HANDLE_CACHE --
 *	Per-session cache of handles to avoid synchronization when opening
 *	cursors.
 */
//__session_add_dhandle创建空间和赋值
/*session的cahce*/
struct __wt_data_handle_cache {
	WT_DATA_HANDLE *dhandle;

	TAILQ_ENTRY(__wt_data_handle_cache) q;
	TAILQ_ENTRY(__wt_data_handle_cache) hashq;
};

					/* Generations manager */
#define	WT_GEN_CHECKPOINT	0	/* Checkpoint generation */
#define	WT_GEN_EVICT		1	/* Eviction generation */
#define	WT_GEN_HAZARD		2	/* Hazard pointer */
#define	WT_GEN_SPLIT		3	/* Page splits */
#define	WT_GENERATIONS		4	/* Total generation manager entries */


/*
 * WT_HAZARD --
 *	A hazard pointer.
 */
//__wt_hazard_set中赋值
struct __wt_hazard {
	WT_REF *ref;			/* Page reference */
#ifdef HAVE_DIAGNOSTIC
	const char *file;		/* File/line where hazard acquired */
	int	    line;
#endif
};

/* Get the connection implementation for a session */
//返回WT_CONNECTION_IMPL结构     session类型为WT_SESSION_IMPL, iface为WT_SESSION iface类型，connection为WT_CONNECTION *connection;
//WT_CONNECTION_IMPL的第一个成员iface为WT_CONNECTION结构，所以可以将(session)->iface.connection直接转换为WT_CONNECTION_IMPL
#define	S2C(session)	  ((WT_CONNECTION_IMPL *)(session)->iface.connection)

/* Get the btree for a session */ 
//该session操作的是那颗bree树，一个tree数对应磁盘上的一个文件
#define	S2BT(session)	   ((WT_BTREE *)(session)->dhandle->handle)
#define	S2BT_SAFE(session) ((session)->dhandle == NULL ? NULL : S2BT(session))

/*
 * WT_SESSION_IMPL --
 *	Implementation of WT_SESSION.
 */ 
 //创建空间和赋值见__wt_open_internal_session      
 //__wt_connection_impl.sessions数组位该类型  
 //S2C完成session(WT_SESSION_IMPL)到connection(__wt_connection_impl)转换
 //赋值见__open_session  
struct __wt_session_impl {
	WT_SESSION iface; //__open_session中赋值

	void	*lang_private;		/* Language specific private storage */

    //是否已经在使用
	u_int active;			/* Non-zero if the session is in-use */

    //session->name会打印出来，见__wt_verbose  赋值见__wt_open_internal_session
	const char *name;		/* Name */ //可以参考__wt_open_internal_session
	const char *lastop;		/* Last operation */
	//在__wt_connection_impl.sessions数组中的游标 
	uint32_t id;			/* UID, offset in session array */
    //赋值见__wt_event_handler_set
	WT_EVENT_HANDLER *event_handler;/* Application's event handlers */
	
	//说明session->dhandle实际上存储的是当前需要用的dhandle，如果是table,则为__wt_table.iface，如果是file则就为__wt_data_handle 
    //__session_get_dhandle(获取，默认先从hash缓存中获取，没有才创建)  __wt_conn_dhandle_alloc中 __wt_conn_dhandle_find赋值 
	WT_DATA_HANDLE *dhandle;	/* Current data handle */

	/*
	 * Each session keeps a cache of data handles. The set of handles can
	 * grow quite large so we maintain both a simple list and a hash table
	 * of lists. The hash table key is based on a hash of the data handle's
	 * URI.  The hash table list is kept in allocated memory that lives
	 * across session close - so it is declared further down.
	 */
					/* Session handle reference list */		
    //dhandles和下面的dhhash配合，删除dhandle在__session_discard_dhandle中删除，添加在__session_add_dhandle
	TAILQ_HEAD(__dhandles, __wt_data_handle_cache) dhandles;
	//生效见__session_dhandle_sweep
	time_t last_sweep;		/* Last sweep for dead handles */
	struct timespec last_epoch;	/* Last epoch time returned */

					/* Cursors closed with the session */
	TAILQ_HEAD(__cursors, __wt_cursor) cursors;

	WT_CURSOR_BACKUP *bkp_cursor;	/* Hot backup cursor */

	WT_COMPACT_STATE *compact;	/* Compaction information */
	enum { WT_COMPACT_NONE=0,
	    WT_COMPACT_RUNNING, WT_COMPACT_SUCCESS } compact_state;

	WT_CURSOR	*las_cursor;	/* Lookaside table cursor */

    //赋值见__wt_metadata_cursor
	WT_CURSOR *meta_cursor;		/* Metadata file */
	void	  *meta_track;		/* Metadata operation tracking */
	void	  *meta_track_next;	/* Current position */
	void	  *meta_track_sub;	/* Child transaction / save point */
	size_t	   meta_track_alloc;	/* Currently allocated */
	int	   meta_track_nest;	/* Nesting level of meta transaction */
#define	WT_META_TRACKING(session)	((session)->meta_track_next != NULL)

	/* Current rwlock for callback. */
	WT_RWLOCK *current_rwlock;
	uint8_t current_rwticket;

    //指向WT_ITEM*指针数组，数组大小为scratch_alloc
	WT_ITEM	**scratch;		/* Temporary memory for any function */
	u_int	  scratch_alloc;	/* Currently allocated */
	//表示上面的scratch空间中，剩下的可重用空间大小，例如scratch分配了1K字节空间大小的item,当释放该item后，表示可重用scratch空间为1K
	size_t	  scratch_cached;	/* Scratch bytes cached */
#ifdef HAVE_DIAGNOSTIC
	/*
	 * Variables used to look for violations of the contract that a
	 * session is only used by a single session at once.
	 */
	volatile uintmax_t api_tid;
	volatile uint32_t api_enter_refcnt;
	/*
	 * It's hard to figure out from where a buffer was allocated after it's
	 * leaked, so in diagnostic mode we track them; DIAGNOSTIC can't simply
	 * add additional fields to WT_ITEM structures because they are visible
	 * to applications, create a parallel structure instead.
	 */
	struct __wt_scratch_track {
		const char *file;	/* Allocating file, line */
		int line;
	} *scratch_track;
#endif

	WT_ITEM err;			/* Error buffer */

    //默认WT_ISO_READ_COMMITTED，见__open_session
	WT_TXN_ISOLATION isolation; 
	//赋值见__wt_txn_init   //session->txn  conn->txn_global的关系可以参考__wt_txn_am_oldest 
	//表示该session当前持有的事务id
	WT_TXN	txn; 			/* Transaction state */
#define	WT_SESSION_BG_SYNC_MSEC		1200000
	WT_LSN	bg_sync_lsn;		/* Background sync operation LSN. */
	//该session对应的有效cursor数
	u_int	ncursors;		/* Count of active file cursors. */

    //赋值见__wt_block_ext_prealloc
	void	*block_manager;		/* Block-manager support */
	//赋值见__wt_block_ext_prealloc
	int	(*block_manager_cleanup)(WT_SESSION_IMPL *);

					/* Checkpoint handles */
	//赋值见__wt_checkpoint_get_handles
	//数组对象，数组成员个数ckpt_handle_next，可以参考__checkpoint_apply
	WT_DATA_HANDLE **ckpt_handle;	/* Handle list */ 
	u_int   ckpt_handle_next;	/* Next empty slot */
	size_t  ckpt_handle_allocated;	/* Bytes allocated */

	/*
	 * Operations acting on handles.
	 *
	 * The preferred pattern is to gather all of the required handles at
	 * the beginning of an operation, then drop any other locks, perform
	 * the operation, then release the handles.  This cannot be easily
	 * merged with the list of checkpoint handles because some operations
	 * (such as compact) do checkpoints internally.
	 */
	WT_DATA_HANDLE **op_handle;	/* Handle list */
	u_int   op_handle_next;		/* Next empty slot */
	size_t  op_handle_allocated;	/* Bytes allocated */

    //赋值见__wt_reconcile
	void	*reconcile;		/* Reconciliation support */
	int	(*reconcile_cleanup)(WT_SESSION_IMPL *);

	/* Sessions have an associated statistics bucket based on its ID. */
	//__open_session中赋值
	u_int	stat_bucket;		/* Statistics bucket offset */

	uint32_t flags;

	/*
	 * All of the following fields live at the end of the structure so it's
	 * easier to clear everything but the fields that persist.
	 */
#define	WT_SESSION_CLEAR_SIZE	(offsetof(WT_SESSION_IMPL, rnd))

	/*
	 * The random number state persists past session close because we don't
	 * want to repeatedly use the same values for skiplist depth when the
	 * application isn't caching sessions.
	 */
	//__wt_random_init获取随机数赋值给rnd
	WT_RAND_STATE rnd;		/* Random number generation state */

    //数组下标取值参考WT_GEN_CHECKPOINT等
    //conn->generations[]和session->s->generations[]的关系可以参考__wt_gen_oldest，conn包含多个session,因此代表总的
	volatile uint64_t generations[WT_GENERATIONS];

	/*
	 * Session memory persists past session close because it's accessed by
	 * threads of control other than the thread owning the session. For
	 * example, btree splits and hazard pointers can "free" memory that's
	 * still in use. In order to eventually free it, it's stashed here with
	 * with its generation number; when no thread is reading in generation,
	 * the memory can be freed for real.
	 */
	/*
    会话内存在会话关闭后仍然存在，不是马上释放，因为它是由控制线程访问的，而不是由拥有会话的线程访问的。例如，btree分割和
    危险指针可以“释放”仍在使用的内存。当生成中没有线程正在读取时，内存可以真正释放。该结构就是控制合适释放这些内存
	*/
	struct __wt_session_stash {
		struct __wt_stash {
			void	*p;	/* Memory, length */
			size_t	 len;
			uint64_t gen;	/* Generation */
		} *list;
		size_t  cnt;		/* Array entries */
		size_t  alloc;		/* Allocated bytes */
	} stash[WT_GENERATIONS];

	/*
	 * Hazard pointers.
	 *
	 * Hazard information persists past session close because it's accessed
	 * by threads of control other than the thread owning the session.
	 *
	 * Use the non-NULL state of the hazard field to know if the session has
	 * previously been initialized.
	 */
#define	WT_SESSION_FIRST_USE(s)						\
	((s)->hazard == NULL)

					/* Hashed handle reference list array */
	//dhandles和上面的dhhash配合，删除dhandle在__session_discard_dhandle中删除，添加在__session_add_dhandle
	TAILQ_HEAD(__dhandles_hash, __wt_data_handle_cache) *dhhash;


    /*
    Hazard Pointer（风险指针）
    Hazard Pointer是lock-free技术的一种实现方式， 它将我们常用的锁机制问题转换为一个内存管理问题， 
    通常额也能减少程序所等待的时间以及死锁的风险， 并且能够提高性能， 在多线程环境下面，它很好的解决读多写少的问题。 
    基本原理 
    对于一个资源， 建立一个Hazard Pointer List， 每当有线程需要读该资源的时候， 给该链表添加一个节点， 
    当读结束的时候， 删除该节点； 要删除该资源的时候， 判断该链表是不是空， 如不， 表明有线程在读取该资源， 就不能删除。 
    
    
    HazardPointer在WiredTiger中的使用 
    在WiredTiger里， 对于每一个缓存的页， 使用一个Hazard Pointer 来对它管理， 之所以需要这样的管理方式， 是因为， 
    每当读了一个物理页到内存， WiredTiger会把它尽可能的放入缓存， 以备后续的内存访问， 但是同时由一些evict 线程
    在运行，当内存吃紧的时候， evict线程就会按照LRU算法， 将一些不常被访问到的内存页写回磁盘。 
    由于每一个内存页有一个Hazard Point， 在evict的时候， 就可以根据Hazard Pointer List的长度， 来决定是否可以将该
    内存页从缓存中写回磁盘。
    */
	/*
	 * The hazard pointer array grows as necessary, initialize with 250
	 * slots.
	 */
	//下面这几个参数在__open_session中初始化
#define	WT_SESSION_INITIAL_HAZARD_SLOTS	250
	uint32_t   hazard_size;		/* Hazard pointer array slots */
	uint32_t   hazard_inuse;	/* Hazard pointer array slots in-use */
	uint32_t   nhazard;		/* Count of active hazard pointers */
	WT_HAZARD *hazard;		/* Hazard pointer array */
};


