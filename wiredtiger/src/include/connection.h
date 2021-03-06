/*-
 * Copyright (c) 2014-2017 MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

/*
 * Default hash table size; we don't need a prime number of buckets
 * because we always use a good hash function.
 */
#define	WT_HASH_ARRAY_SIZE	512

/*******************************************
 * Global per-process structure.
 *******************************************/
/*
 * WT_PROCESS --
 *	Per-process information for the library.
 */
//__wt_process全局变量为该类型
//成员在__wt_global_once中赋值
struct __wt_process {
	WT_SPINLOCK spinlock;		/* Per-process spinlock */

					/* Locked: connection queue */
    //__wt_global_once中初始化赋值，wiredtiger_open添加新的conn到该队列
	TAILQ_HEAD(__wt_connection_impl_qh, __wt_connection_impl) connqh;
	WT_CACHE_POOL *cache_pool;

					/* Checksum function */
#define	__wt_checksum(chunk, len)	__wt_process.checksum(chunk, len)
//__wt_checksum_init中赋值
	uint32_t (*checksum)(const void *, size_t);
};
extern WT_PROCESS __wt_process;

/*
 * WT_KEYED_ENCRYPTOR --
 *	An list entry for an encryptor with a unique (name, keyid).
 */
struct __wt_keyed_encryptor {
	const char *keyid;		/* Key id of encryptor */
	int owned;			/* Encryptor needs to be terminated */
	size_t size_const;		/* The result of the sizing callback */
	WT_ENCRYPTOR *encryptor;	/* User supplied callbacks */
					/* Linked list of encryptors */
	TAILQ_ENTRY(__wt_keyed_encryptor) hashq;
	TAILQ_ENTRY(__wt_keyed_encryptor) q;
};

/*
 * WT_NAMED_COLLATOR --
 *	A collator list entry
 */
struct __wt_named_collator {
	const char *name;		/* Name of collator */
	WT_COLLATOR *collator;		/* User supplied object */
	TAILQ_ENTRY(__wt_named_collator) q;	/* Linked list of collators */
};

/*
 * WT_NAMED_COMPRESSOR --
 *	A compressor list entry
 */
struct __wt_named_compressor {
	const char *name;		/* Name of compressor */
	WT_COMPRESSOR *compressor;	/* User supplied callbacks */
					/* Linked list of compressors */
	TAILQ_ENTRY(__wt_named_compressor) q;
};

/*
 * WT_NAMED_DATA_SOURCE --
 *	A data source list entry
 */ //创建空间赋值见__conn_add_data_source
struct __wt_named_data_source {
	const char *prefix;		/* Name of data source */
	WT_DATA_SOURCE *dsrc;		/* User supplied callbacks */
					/* Linked list of data sources */
	TAILQ_ENTRY(__wt_named_data_source) q;
};

/*
 * WT_NAMED_ENCRYPTOR --
 *	An encryptor list entry
 */
struct __wt_named_encryptor {
	const char *name;		/* Name of encryptor */
	WT_ENCRYPTOR *encryptor;	/* User supplied callbacks */
					/* Locked: list of encryptors by key */
	TAILQ_HEAD(__wt_keyedhash, __wt_keyed_encryptor)
				keyedhashqh[WT_HASH_ARRAY_SIZE];
	TAILQ_HEAD(__wt_keyed_qh, __wt_keyed_encryptor) keyedqh;
					/* Linked list of encryptors */
	TAILQ_ENTRY(__wt_named_encryptor) q;
};

/*
 * WT_NAMED_EXTRACTOR --
 *	An extractor list entry
 */
struct __wt_named_extractor {
	const char *name;		/* Name of extractor */
	WT_EXTRACTOR *extractor;		/* User supplied object */
	TAILQ_ENTRY(__wt_named_extractor) q;	/* Linked list of extractors */
};

/*
 * Allocate some additional slots for internal sessions so the user cannot
 * configure too few sessions for us to run.
 */
#define	WT_EXTRA_INTERNAL_SESSIONS	20

/*
 * WT_CONN_CHECK_PANIC --
 *	Check if we've panicked and return the appropriate error.
 */
#define	WT_CONN_CHECK_PANIC(conn)					\
	(F_ISSET(conn, WT_CONN_PANIC) ? WT_PANIC : 0)
#define	WT_SESSION_CHECK_PANIC(session)					\
	WT_CONN_CHECK_PANIC(S2C(session))

/*
 * Macros to ensure the dhandle is inserted or removed from both the
 * main queue and the hashed queue.
 */
#define	WT_CONN_DHANDLE_INSERT(conn, dhandle, bucket) do {		\
	WT_ASSERT(session,						\
	    F_ISSET(session, WT_SESSION_LOCKED_HANDLE_LIST_WRITE));	\
	TAILQ_INSERT_HEAD(&(conn)->dhqh, dhandle, q);			\
	TAILQ_INSERT_HEAD(&(conn)->dhhash[bucket], dhandle, hashq);	\
	++(conn)->dhandle_count;					\
} while (0)

#define	WT_CONN_DHANDLE_REMOVE(conn, dhandle, bucket) do {		\
	WT_ASSERT(session,						\
	    F_ISSET(session, WT_SESSION_LOCKED_HANDLE_LIST_WRITE));	\
	TAILQ_REMOVE(&(conn)->dhqh, dhandle, q);			\
	TAILQ_REMOVE(&(conn)->dhhash[bucket], dhandle, hashq);		\
	--(conn)->dhandle_count;					\
} while (0)

/*
 * Macros to ensure the block is inserted or removed from both the
 * main queue and the hashed queue.
 */
#define	WT_CONN_BLOCK_INSERT(conn, block, bucket) do {			\
	TAILQ_INSERT_HEAD(&(conn)->blockqh, block, q);			\
	TAILQ_INSERT_HEAD(&(conn)->blockhash[bucket], block, hashq);	\
} while (0)

#define	WT_CONN_BLOCK_REMOVE(conn, block, bucket) do {			\
	TAILQ_REMOVE(&(conn)->blockqh, block, q);			\
	TAILQ_REMOVE(&(conn)->blockhash[bucket], block, hashq);		\
} while (0)

/*
 * WT_CONNECTION_IMPL --
 *	Implementation of WT_CONNECTION
 */
//S2C完成session(WT_SESSION_IMPL)到connection(__wt_connection_impl)转换
//成员初始化见__wt_connection_init, 一些成员通过解析wiredtiger_open配置项赋值
struct __wt_connection_impl {  
	WT_CONNECTION iface; //赋值参考 wiredtiger_open

	/* For operations without an application-supplied session */
	//__wt_connection_init中初始化，default_session最开始指向dummy_session，并通过__wt_connection_init初始化，
	//配置解析完毕后，在__wt_connection_open中重新赋值
	WT_SESSION_IMPL *default_session;
	//wiredtiger_dummy_session_init中初始化
	WT_SESSION_IMPL  dummy_session;

    //赋值见wiredtiger_open中的conn->cfg = merge_cfg;
	const char *cfg;		/* Connection configuration */

	WT_SPINLOCK api_lock;		/* Connection API spinlock */
	WT_SPINLOCK checkpoint_lock;	/* Checkpoint spinlock */
	WT_SPINLOCK fh_lock;		/* File handle queue spinlock */
	WT_SPINLOCK metadata_lock;	/* Metadata update spinlock */
	WT_SPINLOCK reconfig_lock;	/* Single thread reconfigure */
	WT_SPINLOCK schema_lock;	/* Schema operation spinlock */
	//WT_WITH_TABLE_READ_LOCK  WT_WITH_TABLE_WRITE_LOCK
	WT_RWLOCK table_lock;		/* Table list lock */
	WT_SPINLOCK turtle_lock;	/* Turtle file spinlock */
	WT_RWLOCK dhandle_lock;		/* Data handle list lock */

					/* Connection queue */
	TAILQ_ENTRY(__wt_connection_impl) q;
					/* Cache pool queue */
	TAILQ_ENTRY(__wt_connection_impl) cpq;

	const char *home;		/* Database home */
	//从error_prefix配置项中解析获取，见wiredtiger_open
	const char *error_prefix;	/* Database error prefix */
	int is_new;			/* Connection created database */

    //兼容版本信息
	uint16_t compat_major;		/* Compatibility major version */
	uint16_t compat_minor;		/* Compatibility minor version */

	WT_EXTENSION_API extension_api;	/* Extension API */

					/* Configuration */ //初始化赋值见__wt_conn_config_init
	const WT_CONFIG_ENTRY **config_entries;

	void  **foc;			/* Free-on-close array */
	size_t  foc_cnt;		/* Array entries */
	size_t  foc_size;		/* Array size */

    //对应WiredTiger.lock文件锁，赋值见__conn_single
	WT_FH *lock_fh;			/* Lock file handle */

	/*
	 * The connection keeps a cache of data handles. The set of handles
	 * can grow quite large so we maintain both a simple list and a hash
	 * table of lists. The hash table key is based on a hash of the table
	 * URI.
	 */
					/* Locked: data handle hash array */
	//__wt_connection_init  __open_session中初始化赋值，
	//添加在WT_CONN_DHANDLE_INSERT，删除在WT_CONN_DHANDLE_REMOVE，查找__wt_conn_dhandle_find
	TAILQ_HEAD(__wt_dhhash, __wt_data_handle) dhhash[WT_HASH_ARRAY_SIZE]; //hash桶用于快速查找，参考__wt_conn_btree_apply
					/* Locked: data handle list */ //参考__wt_conn_btree_apply，通过这里可以获取到所有的tree
	TAILQ_HEAD(__wt_dhandle_qh, __wt_data_handle) dhqh; //dhqh和dhhash一起配合  这个是链表，所有的dhandle都可以通过遍历链表得到

	//lsm hander树，参考__lsm_tree_find， 插入lsm tree节点见__lsm_tree_open  删除lsm节点见__lsm_tree_discard
					/* Locked: LSM handle list. */
	TAILQ_HEAD(__wt_lsm_qh, __wt_lsm_tree) lsmqh;
					/* Locked: file list */
	//存放file handle的hash表，见__handle_search，用于存储各种打开的文件，见__handle_search
	TAILQ_HEAD(__wt_fhhash, __wt_fh) fhhash[WT_HASH_ARRAY_SIZE];
	TAILQ_HEAD(__wt_fh_qh, __wt_fh) fhqh;
					/* Locked: library list */
	TAILQ_HEAD(__wt_dlh_qh, __wt_dlh) dlhqh;

	WT_SPINLOCK block_lock;		/* Locked: block manager list */
	//插入WT_CONN_BLOCK_INSERT  删除WT_CONN_BLOCK_REMOVE  __wt_block_open
	TAILQ_HEAD(__wt_blockhash, __wt_block) blockhash[WT_HASH_ARRAY_SIZE];
	TAILQ_HEAD(__wt_block_qh, __wt_block) blockqh;

	u_int dhandle_count;		/* Locked: handles in the queue */
	u_int open_btree_count;		/* Locked: open writable btree count */
	uint32_t next_file_id;		/* Locked: file ID counter */
	//__handle_search中自增
	uint32_t open_file_count;	/* Atomic: open file handle count */
	uint32_t open_cursor_count;	/* Atomic: open cursor handle count */

	/*
	 * WiredTiger allocates space for 50 simultaneous sessions (threads of
	 * control) by default.  Growing the number of threads dynamically is
	 * possible, but tricky since server threads are walking the array
	 * without locking it.
	 *
	 * There's an array of WT_SESSION_IMPL pointers that reference the
	 * allocated array; we do it that way because we want an easy way for
	 * the server thread code to avoid walking the entire array when only a
	 * few threads are running.
	 */
	//__wt_connection_open中分配空间，这是个session数组，赋值__open_session 
	WT_SESSION_IMPL	*sessions;	/* Session reference */
	//wiredtiger_open中赋值
	uint32_t	 session_size;	/* Session array size */
	//当前正在使用的session数目，有效session数  赋值见__open_session
	uint32_t	 session_cnt;	/* Session count */

    //scratch空间上限，生效见__wt_scr_free    
    //赋值见wiredtiger_open
	size_t     session_scratch_max;	/* Max scratch memory per session */

    //__wt_cache_create中分配空间
	WT_CACHE  *cache;		/* Page cache */
	volatile uint64_t cache_size;	/* Cache size (either statically
					   configured or the current size
					   within a cache pool). */
    //session->txn  conn->txn_global的关系可以参考__wt_txn_am_oldest
	WT_TXN_GLOBAL txn_global;	/* Global transaction state */

	WT_RWLOCK hot_backup_lock;	/* Hot backup serialization */
	bool hot_backup;		/* Hot backup in progress */
	char **hot_backup_list;		/* Hot backup file list */

	WT_SESSION_IMPL *ckpt_session;	/* Checkpoint thread session */
	wt_thread_t	 ckpt_tid;	/* Checkpoint thread */
	bool		 ckpt_tid_set;	/* Checkpoint thread set */
	WT_CONDVAR	*ckpt_cond;	/* Checkpoint wait mutex */
#define	WT_CKPT_LOGSIZE(conn)	((conn)->ckpt_logsize != 0)
	wt_off_t	 ckpt_logsize;	/* Checkpoint log size period */
	bool		 ckpt_signalled;/* Checkpoint signalled */

	uint64_t  ckpt_usecs;		/* Checkpoint timer */
	uint64_t  ckpt_time_max;	/* Checkpoint time min/max */
	uint64_t  ckpt_time_min;
	uint64_t  ckpt_time_recent;	/* Checkpoint time recent/total */
	uint64_t  ckpt_time_total;

	/* Checkpoint stats and verbosity timers */
	//赋值见__txn_checkpoint  赋值见__txn_checkpoint
	struct timespec ckpt_timer_start; //checkpoint开始时间
	struct timespec ckpt_timer_scrub_end; //做checkpoint的时候，需要系统有足够的内存，如果内存不够需要等待evict淘汰page释放出足够的内存

	/* Checkpoint progress message data */
	uint64_t ckpt_progress_msg_count;
	uint64_t ckpt_write_bytes;
	uint64_t ckpt_write_pages;

	uint32_t stat_flags;		/* Options declared in flags.py */

					/* Connection statistics */
	//__wt_stat_connection_init中初始化  //记录各种状态信息
	WT_CONNECTION_STATS *stats[WT_COUNTER_SLOTS];  
	WT_CONNECTION_STATS *stat_array;

    //分配空间及赋值见__async_start
	WT_ASYNC	*async;		/* Async structure */
	//赋值见__async_start
	bool		 async_cfg;	/* Global async configuration */
	//async.ops_max配置
	uint32_t	 async_size;	/* Async op array size */
	//async.threads中配置
	uint32_t	 async_workers;	/* Number of async workers */

	WT_LSM_MANAGER	lsm_manager;	/* LSM worker thread information */

	WT_KEYED_ENCRYPTOR *kencryptor;	/* Encryptor for metadata and log */

	bool		 evict_server_running;/* Eviction server operating */

    //__wt_evict_create中赋值
	WT_THREAD_GROUP  evict_threads; 
	//线程组中总的线程数  __cache_config_local  
	//最大并发的evict线程数 生效参考__wt_evict_create
	uint32_t	 evict_threads_max;/* Max eviction threads */
	//线程组中活跃线程数 __cache_config_local
	//最少并发的evict线程数  生效参考__wt_evict_create
	uint32_t	 evict_threads_min;/* Min eviction threads */

    //赋值见__statlog_config
#define	WT_STATLOG_FILENAME	"WiredTigerStat.%d.%H"
	WT_SESSION_IMPL *stat_session;	/* Statistics log session */
	wt_thread_t	 stat_tid;	/* Statistics log thread */
	bool		 stat_tid_set;	/* Statistics log thread set */
	WT_CONDVAR	*stat_cond;	/* Statistics log wait mutex */
	const char	*stat_format;	/* Statistics log timestamp format */
	WT_FSTREAM	*stat_fs;	/* Statistics log stream */
	/* Statistics log json table printing state flag */
	bool		 stat_json_tables;
	char		*stat_path;	/* Statistics log path format */
	char	       **stat_sources;	/* Statistics log list of objects */
	const char	*stat_stamp;	/* Statistics log entry timestamp */
	uint64_t	 stat_usecs;	/* Statistics log period */

    //WT_CONN_LOG_ARCHIVE等
	uint32_t	 log_flags;	/* Global logging configuration */
	WT_CONDVAR	*log_cond;	/* Log server wait mutex */
	WT_SESSION_IMPL *log_session;	/* Log server session */
	wt_thread_t	 log_tid;	/* Log server thread */
	bool		 log_tid_set;	/* Log server thread set */
	WT_CONDVAR	*log_file_cond;	/* Log file thread wait mutex */
	WT_SESSION_IMPL *log_file_session;/* Log file thread session */
	wt_thread_t	 log_file_tid;	/* Log file thread */
	bool		 log_file_tid_set;/* Log file thread set */
	WT_CONDVAR	*log_wrlsn_cond;/* Log write lsn thread wait mutex */
	WT_SESSION_IMPL *log_wrlsn_session;/* Log write lsn thread session */
	wt_thread_t	 log_wrlsn_tid;	/* Log write lsn thread */
	bool		 log_wrlsn_tid_set;/* Log write lsn thread set */
	WT_LOG		*log;		/* Logging structure */
	/*读取日志是否进行压缩项目*/
	WT_COMPRESSOR	*log_compressor;/* Logging compressor */
	uint32_t	 log_cursors;	/* Log cursor count */
	/*获得日志文件最大空间大小*/
	wt_off_t	 log_file_max;	/* Log file max size */
	/* 日志文件存放的路径，__wt_log_open中打开 */
	const char	*log_path;	/* Logging path format */
	uint32_t	 log_prealloc;	/* Log file pre-allocation */
	//__logmgr_sync_cfg中配置解析  会赋值给__wt_txn.txn_logsync
	uint32_t	 txn_logsync;	/* Log sync configuration */

	WT_SESSION_IMPL *meta_ckpt_session;/* Metadata checkpoint session */

	/*
	 * Is there a data/schema change that needs to be the part of a
	 * checkpoint.
	 */
	bool modified;

	WT_SESSION_IMPL *sweep_session;	   /* Handle sweep session */
	wt_thread_t	 sweep_tid;	   /* Handle sweep thread */
	int		 sweep_tid_set;	   /* Handle sweep thread set */
	WT_CONDVAR      *sweep_cond;	   /* Handle sweep wait mutex */

	//一下三个配置项赋值见__wt_sweep_config
	uint64_t         sweep_idle_time;  /* Handle sweep idle time */
	//生效见__session_dhandle_sweep
	uint64_t         sweep_interval;   /* Handle sweep interval */
	uint64_t         sweep_handles_min;/* Handle sweep minimum open */

	/* Set of btree IDs not being rolled back */
	uint8_t *stable_rollback_bitstring;
	uint32_t stable_rollback_maxfile;

					/* Locked: collator list */
	TAILQ_HEAD(__wt_coll_qh, __wt_named_collator) collqh;

					/* Locked: compressor list */
	TAILQ_HEAD(__wt_comp_qh, __wt_named_compressor) compqh;

    //__conn_add_data_source和__wt_conn_remove_data_source对应
					/* Locked: data source list */
	TAILQ_HEAD(__wt_dsrc_qh, __wt_named_data_source) dsrcqh;

					/* Locked: encryptor list */
	WT_SPINLOCK encryptor_lock;	/* Encryptor list lock */
	TAILQ_HEAD(__wt_encrypt_qh, __wt_named_encryptor) encryptqh;

					/* Locked: extractor list */
	TAILQ_HEAD(__wt_extractor_qh, __wt_named_extractor) extractorqh;

	void	*lang_private;		/* Language specific private storage */

	/* If non-zero, all buffers used for I/O will be aligned to this. */
	size_t buffer_alignment; //赋值见wiredtiger_open  字节对其

	uint64_t stashed_bytes;		/* Atomic: stashed memory statistics */
	uint64_t stashed_objects;
					/* Generations manager */
	//__wt_gen_init中赋值初始化  获取参考__wt_gen   __wt_gen_next
	//conn->generations[]和session->s->generations[]的关系可以参考__wt_gen_oldest，conn包含多个session,因此代表总的
	volatile uint64_t generations[WT_GENERATIONS]; //数组下标取值参考WT_GEN_CHECKPOINT等

    //赋值见wiredtiger_open  file_extend配置，
	wt_off_t data_extend_len;	/* file_extend data length */
	wt_off_t log_extend_len;	/* file_extend log length */

#define	WT_DIRECT_IO_CHECKPOINT	0x01	/* Checkpoints */
#define	WT_DIRECT_IO_DATA	0x02	/* Data files */
#define	WT_DIRECT_IO_LOG	0x04	/* Log files */
    //赋值见wiredtiger_open   direct_io配置项指定
	uint32_t direct_io;		/* O_DIRECT, FILE_FLAG_NO_BUFFERING */
    //赋值见wiredtiger_open   write_through配置项指定 
	uint32_t write_through;		/* FILE_FLAG_WRITE_THROUGH */

    //赋值见wiredtiger_open   mmap配置项指定
	bool	 mmap;			/* mmap configuration */
	//赋值为__wt_get_vm_pagesize
	int page_size;			/* OS page size for mmap alignment */
	//__wt_verbose是否打印输出，就是根据这个判断
	//根据配置赋值见__wt_verbose_config
	uint32_t verbose;

	/*
	 * Variable with flags for which subsystems the diagnostic stress timing
	 * delays have been requested.
	 */
	uint32_t timing_stress_flags;

#define	WT_STDERR(s)	(&S2C(s)->wt_stderr)
#define	WT_STDOUT(s)	(&S2C(s)->wt_stdout)
	WT_FSTREAM wt_stderr, wt_stdout;

	/*
	 * File system interface abstracted to support alternative file system
	 * implementations.
	 */
	//__wt_os_posix  __wt_os_inmemory中对file_system赋值
	WT_FILE_SYSTEM *file_system;

    //例如F_SET(conn, WT_CONN_IN_MEMORY);设置，都是根据配置来设置的
	uint32_t flags;
};

#define	WT_CONN_LOG_ARCHIVE		0x001	/* Archive is enabled */
#define	WT_CONN_LOG_DOWNGRADED		0x002	/* Running older version */
//log.enabled是否使能
#define	WT_CONN_LOG_ENABLED		0x004	/* Logging is enabled */  //启用日志功能，赋值见__wt_logmgr_create
#define	WT_CONN_LOG_EXISTED		0x008	/* Log files found */
#define	WT_CONN_LOG_FORCE_DOWNGRADE	0x010	/* Force downgrade */
#define	WT_CONN_LOG_RECOVER_DIRTY	0x020	/* Recovering unclean */
#define	WT_CONN_LOG_RECOVER_DONE	0x040	/* Recovery completed */
#define	WT_CONN_LOG_RECOVER_ERR		0x080	/* Error if recovery required */
#define	WT_CONN_LOG_ZERO_FILL		0x100	/* Manually zero files */


