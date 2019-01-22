/*-
 * Copyright (c) 2014-2017 MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

/*
 * Helpers for calling a function with a data handle in session->dhandle
 * then restoring afterwards.
 */
#define	WT_WITH_DHANDLE(s, d, e) do {					\
	WT_DATA_HANDLE *__saved_dhandle = (s)->dhandle;			\
	(s)->dhandle = (d);						\
	e;								\
	(s)->dhandle = __saved_dhandle;					\
} while (0)

#define	WT_WITH_BTREE(s, b, e)	WT_WITH_DHANDLE(s, (b)->dhandle, e)

/* Call a function without the caller's data handle, restore afterwards. */
#define	WT_WITHOUT_DHANDLE(s, e) WT_WITH_DHANDLE(s, NULL, e)

/*
 * Call a function with the caller's data handle, restore it afterwards in case
 * it is overwritten.
 */
#define	WT_SAVE_DHANDLE(s, e) WT_WITH_DHANDLE(s, (s)->dhandle, e)

/* Check if a handle is inactive. */
#define	WT_DHANDLE_INACTIVE(dhandle)					\
	(F_ISSET(dhandle, WT_DHANDLE_DEAD) ||				\
	!F_ISSET(dhandle, WT_DHANDLE_EXCLUSIVE | WT_DHANDLE_OPEN))

/* The metadata cursor's data handle. */
#define	WT_SESSION_META_DHANDLE(s)					\
	(((WT_CURSOR_BTREE *)((s)->meta_cursor))->btree->dhandle)

#define	WT_DHANDLE_ACQUIRE(dhandle)					\
    (void)__wt_atomic_add32(&(dhandle)->session_ref, 1)

#define	WT_DHANDLE_RELEASE(dhandle)					\
    (void)__wt_atomic_sub32(&(dhandle)->session_ref, 1)

#define	WT_DHANDLE_NEXT(session, dhandle, head, field) do {		\
	WT_ASSERT(session, F_ISSET(session, WT_SESSION_LOCKED_HANDLE_LIST));\
	if ((dhandle) == NULL)						\
		(dhandle) = TAILQ_FIRST(head);				\
	else {								\
		    WT_DHANDLE_RELEASE(dhandle);			\
		    (dhandle) = TAILQ_NEXT(dhandle, field);		\
	}								\
	if ((dhandle) != NULL)						\
		    WT_DHANDLE_ACQUIRE(dhandle);			\
} while (0)

/*
 * WT_DATA_HANDLE --
 *	A handle for a generic named data source.
 */
//__wt_conn_dhandle_alloc中创建改结构   __wt_table.iface为该结构
//可以对应table  file  index colgroup等，
//__wt_data_handle_cache.dhandle 
//__session_add_dhandle  __conn_dhandle_get创建空间和赋值
//__wt_conn_dhandle_alloc中创建dhandle，一个dhandle对应一个table或者file，如果是file，则与一个btree(__wt_data_handle.handle)树关联
/*file对应一个tree，也就是__wt_data_handle.handle。如果有多个table创建则会有多个tree及其对应的文件，
也就有多个__wt_data_handle结构，这些handle最终存入session->dhandles队列*/
struct __wt_data_handle {//如果是table,则为__wt_table.iface，如果是file则就为__wt_data_handle 
	WT_RWLOCK rwlock;		/* Lock for shared/exclusive ops */

    //table  index  file colgroup名   如table:access，在__wt_schema_colgroup_source中改为file
	const char *name;		/* Object name as a URI */
	uint64_t name_hash;		/* Hash of name */
	const char *checkpoint;		/* Checkpoint name (or NULL) */
	//__conn_dhandle_config_clear  __conn_dhandle_config_set 分别清除配置和添加配置
	//__conn_dhandle_config_set
	//配置信息，例如table 配置  file 配置  index配置 colgroup配置 
	//对应WT_CONFIG_ENTRY_file_meta配置   __conn_dhandle_config_set赋值
	const char **cfg;		/* Configuration information */

	/*
	 * Sessions caching a connection's data handle will have a non-zero
	 * reference count; sessions using a connection's data handle will
	 * have a non-zero in-use count.
	 */
	uint32_t session_ref;		/* Sessions referencing this handle */
	int32_t	 session_inuse;		/* Sessions using this handle */
	uint32_t excl_ref;		/* Refs of handle by excl_session */
	time_t	 timeofdeath;		/* Use count went to 0 */
	WT_SESSION_IMPL *excl_session;	/* Session with exclusive use, if any */

	WT_DATA_SOURCE *dsrc;		/* Data source for this handle */
	//赋值见__wt_conn_dhandle_alloc,如果是file，则通过这里与btree关联
	//WT_BTREE和__wt_block文件通过__wt_btree_open->__wt_block_manager_open关联
	void *handle;			/* Generic handle */

	enum { //赋值见__wt_conn_dhandle_alloc
		WT_DHANDLE_TYPE_BTREE, //file相关
		WT_DHANDLE_TYPE_TABLE  //table相关
	} type;

	bool compact_skip;		/* If the handle failed to compact */

	/*
	 * Data handles can be closed without holding the schema lock; threads
	 * walk the list of open handles, operating on them (checkpoint is the
	 * best example).  To avoid sources disappearing underneath checkpoint,
	 * lock the data handle when closing it.
	 */
	WT_SPINLOCK	close_lock;	/* Lock to close the handle */

					/* Data-source statistics */
	WT_DSRC_STATS *stats[WT_COUNTER_SLOTS]; //见__wt_stat_dsrc_init初始化
	//见__wt_stat_dsrc_init初始化
	WT_DSRC_STATS *stat_array;
	
	TAILQ_ENTRY(__wt_data_handle) q;
	TAILQ_ENTRY(__wt_data_handle) hashq;

	/* Flags values over 0xff are reserved for WT_BTREE_* */
#define	WT_DHANDLE_DEAD		        0x01	/* Dead, awaiting discard */
#define	WT_DHANDLE_DISCARD	        0x02	/* Close on release */
#define	WT_DHANDLE_DISCARD_KILL		0x04	/* Mark dead on release */
#define	WT_DHANDLE_EXCLUSIVE	        0x08	/* Exclusive access */
//WT_METAFILE_URI文件  set使用见__wt_conn_dhandle_alloc
#define	WT_DHANDLE_IS_METADATA		0x10	/* Metadata handle */
#define	WT_DHANDLE_LOCK_ONLY	        0x20	/* Handle only used as a lock */
#define	WT_DHANDLE_OPEN		        0x40	/* Handle is open */
	uint32_t flags;
};
