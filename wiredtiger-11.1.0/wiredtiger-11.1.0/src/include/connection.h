/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

/*******************************************
 * Global per-process structure.
 *******************************************/

/*
 * WT_PROCESS --
 *	Per-process information for the library.
 */ //__wt_process全局变量
struct __wt_process {
    WT_SPINLOCK spinlock; /* Per-process spinlock */

    /* Locked: connection queue */
    TAILQ_HEAD(__wt_connection_impl_qh, __wt_connection_impl) connqh;

/* Checksum functions */
//也就是wiredtiger_crc32c_func
#define __wt_checksum(chunk, len) __wt_process.checksum(chunk, len)
    uint32_t (*checksum)(const void *, size_t);

#define WT_TSC_DEFAULT_RATIO 1.0
    double tsc_nsec_ratio; /* rdtsc ticks to nanoseconds */
    bool use_epochtime;    /* use expensive time */

    bool fast_truncate_2022; /* fast-truncate fix run-time configuration */

    //shared_cache方式才会使用，mongodb的cache_size配置不会使用cache_pool
    WT_CACHE_POOL *cache_pool; /* shared cache information */

    /*
     * WT_CURSOR.modify operations set unspecified bytes to space in 'S' format and to a nul byte in
     * all other formats. It makes it easier to debug format test program stress failures if strings
     * are printable and don't require encoding to trace them in the log; this is a hook that allows
     * format to set the modify pad byte to a printable character.
     */
    uint8_t modify_pad_byte;
};
extern WT_PROCESS __wt_process;

/*
 * WT_BUCKET_STORAGE --
 *	A list entry for a storage source with a unique name (bucket, prefix).
 */
struct __wt_bucket_storage {
    const char *bucket;                /* Bucket name */
    const char *bucket_prefix;         /* Bucket prefix */
    const char *cache_directory;       /* Locally cached file location */
    int owned;                         /* Storage needs to be terminated */
    uint64_t retain_secs;              /* Tiered period */
    const char *auth_token;            /* Tiered authentication cookie */
    WT_FILE_SYSTEM *file_system;       /* File system for bucket */
    WT_STORAGE_SOURCE *storage_source; /* Storage source callbacks */
    /* Linked list of bucket storage entries */
    TAILQ_ENTRY(__wt_bucket_storage) hashq;
    TAILQ_ENTRY(__wt_bucket_storage) q;

/* AUTOMATIC FLAG VALUE GENERATION START 0 */
#define WT_BUCKET_FREE 0x1u
    /* AUTOMATIC FLAG VALUE GENERATION STOP 32 */
    uint32_t flags;
};

/* Call a function with the bucket storage and its associated file system. */
#define WT_WITH_BUCKET_STORAGE(bsto, s, e)                                  \
    do {                                                                    \
        WT_BUCKET_STORAGE *__saved_bstorage = (s)->bucket_storage;          \
        (s)->bucket_storage = ((bsto) == NULL ? S2C(s)->bstorage : (bsto)); \
        e;                                                                  \
        (s)->bucket_storage = __saved_bstorage;                             \
    } while (0)

/*
 * WT_KEYED_ENCRYPTOR --
 *	A list entry for an encryptor with a unique (name, keyid).
 */
struct __wt_keyed_encryptor {
    //赋值见__wt_encryptor_config
    const char *keyid;       /* Key id of encryptor */
    int owned;               /* Encryptor needs to be terminated */
    size_t size_const;       /* The result of the sizing callback */
    WT_ENCRYPTOR *encryptor; /* User supplied callbacks */
    /* Linked list of encryptors */
    TAILQ_ENTRY(__wt_keyed_encryptor) hashq;
    TAILQ_ENTRY(__wt_keyed_encryptor) q;
};

/*
 * WT_NAMED_COLLATOR --
 *	A collator list entry
 */ //__wt_connection_impl.collqh为该类型，该列表中的成员
struct __wt_named_collator {
    const char *name;                   /* Name of collator */
    //赋值参考__conn_add_collator
    WT_COLLATOR *collator;              /* User supplied object */
    TAILQ_ENTRY(__wt_named_collator) q; /* Linked list of collators */
};

/*
 * WT_NAMED_COMPRESSOR --
 *	A compressor list entry
 */ //__wt_connection_impl.compqh为该类型，该列表中的成员
struct __wt_named_compressor {
    const char *name;          /* Name of compressor */
    //默认snappy，初始化参考snappy_extension_init
    WT_COMPRESSOR *compressor; /* User supplied callbacks */
    /* Linked list of compressors */
    TAILQ_ENTRY(__wt_named_compressor) q;
};

/*
 * WT_NAMED_DATA_SOURCE --
 *	A data source list entry
 */ //__wt_connection_impl.dsrcqh为该类型，该列表中的成员
struct __wt_named_data_source {
    const char *prefix;   /* Name of data source */
    WT_DATA_SOURCE *dsrc; /* User supplied callbacks */
    /* Linked list of data sources */
    TAILQ_ENTRY(__wt_named_data_source) q;
};

/*
 * WT_NAMED_ENCRYPTOR --
 *	An encryptor list entry
 */ //__wt_connection_impl.encryptqh为该类型，该列表中的成员
struct __wt_named_encryptor {
    const char *name;        /* Name of encryptor */
    WT_ENCRYPTOR *encryptor; /* User supplied callbacks */
    /* Locked: list of encryptors by key */
    TAILQ_HEAD(__wt_keyedhash, __wt_keyed_encryptor) * keyedhashqh;
    TAILQ_HEAD(__wt_keyed_qh, __wt_keyed_encryptor) keyedqh;
    /* Linked list of encryptors */
    TAILQ_ENTRY(__wt_named_encryptor) q;
};

/*
 * WT_NAMED_EXTRACTOR --
 *	An extractor list entry
 */ //__wt_connection_impl.extractorqh为该类型，该列表中的成员
struct __wt_named_extractor {
    const char *name;                    /* Name of extractor */
    WT_EXTRACTOR *extractor;             /* User supplied object */
    TAILQ_ENTRY(__wt_named_extractor) q; /* Linked list of extractors */
};

/*
 * WT_NAMED_STORAGE_SOURCE --
 *	A storage source list entry
 */ //__wt_connection_impl.storagesrcqh为该类型，该列表中的成员
struct __wt_named_storage_source {
    const char *name;                  /* Name of storage source */
    WT_STORAGE_SOURCE *storage_source; /* User supplied callbacks */
    TAILQ_HEAD(__wt_buckethash, __wt_bucket_storage) * buckethashqh;
    TAILQ_HEAD(__wt_bucket_qh, __wt_bucket_storage) bucketqh;
    /* Linked list of storage sources */
    TAILQ_ENTRY(__wt_named_storage_source) q;
};

/*
 * WT_NAME_FLAG --
 *	Simple structure for name and flag configuration searches
 */ //参考__wt_verbose_config
struct __wt_name_flag {
    const char *name;
    uint64_t flag;
};

/*
 * WT_CONN_CHECK_PANIC --
 *	Check if we've panicked and return the appropriate error.
 */
#define WT_CONN_CHECK_PANIC(conn) (F_ISSET(conn, WT_CONN_PANIC) ? WT_PANIC : 0)
#define WT_SESSION_CHECK_PANIC(session) WT_CONN_CHECK_PANIC(S2C(session))

/*
 * Macros to ensure the dhandle is inserted or removed from both the main queue and the hashed
 * queue. WT_CONN_DHANDLE_INSERT把表对应handle添加到队列， WT_CONN_DHANDLE_REMOVE把表对应handle从队列移除
 */
#define WT_CONN_DHANDLE_INSERT(conn, dhandle, bucket)                                            \
    do {                                                                                         \
        WT_ASSERT(session, FLD_ISSET(session->lock_flags, WT_SESSION_LOCKED_HANDLE_LIST_WRITE)); \
        TAILQ_INSERT_HEAD(&(conn)->dhqh, dhandle, q);                                            \
        TAILQ_INSERT_HEAD(&(conn)->dhhash[bucket], dhandle, hashq);                              \
        ++(conn)->dh_bucket_count[bucket];                                                       \
        ++(conn)->dhandle_count;                                                                 \
    } while (0)

#define WT_CONN_DHANDLE_REMOVE(conn, dhandle, bucket)                                            \
    do {                                                                                         \
        WT_ASSERT(session, FLD_ISSET(session->lock_flags, WT_SESSION_LOCKED_HANDLE_LIST_WRITE)); \
        TAILQ_REMOVE(&(conn)->dhqh, dhandle, q);                                                 \
        TAILQ_REMOVE(&(conn)->dhhash[bucket], dhandle, hashq);                                   \
        --(conn)->dh_bucket_count[bucket];                                                       \
        --(conn)->dhandle_count;                                                                 \
    } while (0)

/*
 * Macros to ensure the block is inserted or removed from both the main queue and the hashed queue.
 */
#define WT_CONN_BLOCK_INSERT(conn, block, bucket)                    \
    do {                                                             \
        TAILQ_INSERT_HEAD(&(conn)->blockqh, block, q);               \
        TAILQ_INSERT_HEAD(&(conn)->blockhash[bucket], block, hashq); \
    } while (0)

#define WT_CONN_BLOCK_REMOVE(conn, block, bucket)               \
    do {                                                        \
        TAILQ_REMOVE(&(conn)->blockqh, block, q);               \
        TAILQ_REMOVE(&(conn)->blockhash[bucket], block, hashq); \
    } while (0)

/*
 * WT_CONN_HOTBACKUP_START --
 *	Macro to set connection data appropriately for when we commence hot backup.
 */
#define WT_CONN_HOTBACKUP_START(conn)                        \
    do {                                                     \
        (conn)->hot_backup_start = (conn)->ckpt_most_recent; \
        (conn)->hot_backup_list = NULL;                      \
    } while (0)

/*
 * WT_BACKUP_TARGET --
 *	A target URI entry indicating this URI should be restored during a partial backup.
 */
struct __wt_backup_target {
    const char *name; /* File name */

    uint64_t name_hash;                    /* hash of name */
    TAILQ_ENTRY(__wt_backup_target) hashq; /* internal hash queue */
};
typedef TAILQ_HEAD(__wt_backuphash, __wt_backup_target) WT_BACKUPHASH;

/*
 * WT_CONNECTION_IMPL --
 *	Implementation of WT_CONNECTION

 Internally, WiredTiger's cache state is represented by the WT_CACHE structure, which contains counters and parameter
 settings for tracking cache usage and controlling eviction policy. The WT_CACHE also includes state WiredTiger uses
 to track the progress of eviction. There is a single WT_CACHE for each connection, accessed via the WT_CONNECTION_IMPL
 structure.

 //每个connection对应一个WT_CACHE(__wt_cache)及WT_CONNECTION_IMPL(__wt_connection_impl)
 */
//__wt_cache_pool.cache_pool_qh:为了计算所有conn上面的内存情况及内存压力
////__wt_process.connqh全局变量存储所有分配的conn,确保只有一个conn访问该DB
struct __wt_connection_impl {
    //对应connection的接口回调，参考wiredtiger_open
    WT_CONNECTION iface;

    /* For operations without an application-supplied session */
    //default_session默认等于wiredtiger_open
    WT_SESSION_IMPL *default_session;
    //wiredtiger_dummy_session_init
    WT_SESSION_IMPL dummy_session;

    const char *cfg; /* Connection configuration */

    WT_SPINLOCK api_lock;        /* Connection API spinlock */
    //# Locking statistics相关统计参考WT_SPIN_INIT_TRACKED这里，例如lock_checkpoint_count lock_check point_wait_application lock_checkpoint_wait_internal等
    //相关锁耗时统计见WT_WITH_LOCK_WAIT->__wt_spin_lock_track 
    //在WT_WITH_CHECKPOINT_LOCK获取锁，执行op，然后释放锁
    WT_SPINLOCK checkpoint_lock; /* Checkpoint spinlock */
    WT_SPINLOCK fh_lock;         /* File handle queue spinlock */
    WT_SPINLOCK flush_tier_lock; /* Flush tier spinlock */
    //在WT_WITH_METADATA_LOCK获取锁，执行op，然后释放锁
    WT_SPINLOCK metadata_lock;   /* Metadata update spinlock */
    WT_SPINLOCK reconfig_lock;   /* Single thread reconfigure */
    WT_SPINLOCK schema_lock;     /* Schema operation spinlock */
    //WT_RWLOCK相关的耗时统计见__wt_try_readlock  __wt_readlock  __wt_try_writelock  __wt_writelock
    WT_RWLOCK table_lock;        /* Table list lock */
    WT_SPINLOCK tiered_lock;     /* Tiered work queue spinlock */
    WT_SPINLOCK turtle_lock;     /* Turtle file spinlock */
    //WT_RWLOCK相关的耗时统计见__wt_try_readlock  __wt_readlock  __wt_try_writelock  __wt_writelock
    WT_RWLOCK dhandle_lock;      /* Data handle list lock */

    /* Connection queue */
    TAILQ_ENTRY(__wt_connection_impl) q;
    /* Cache pool queue */
    TAILQ_ENTRY(__wt_connection_impl) cpq;

    //__conn_home中赋值
    const char *home;         /* Database home */
    const char *error_prefix; /* Database error prefix */
    uint64_t dh_hash_size;    /* Data handle hash bucket array size */
    uint64_t hash_size;       /* General hash bucket array size */
    //是不是刚使用/data1/containers/211909456/db目录数据，第一次使用这里为true，如果是mongod关闭进程，重启，这里为false
    int is_new;               /* Connection created database */

    //赋值见__wt_turtle_validate_version，也就是WiredTiger.turtle中的version
    WT_VERSION recovery_version; /* Version of the database being recovered */

#ifndef WT_STANDALONE_BUILD
    bool unclean_shutdown; /* Flag to indicate the earlier shutdown status */
#endif

    //赋值见__wt_conn_compat_config
    WT_VERSION compat_version; /* WiredTiger version for compatibility checks */
    WT_VERSION compat_req_max; /* Maximum allowed version of WiredTiger for compatibility checks */
    WT_VERSION compat_req_min; /* Minimum allowed version of WiredTiger for compatibility checks */

    //__conn_get_extension_api
    WT_EXTENSION_API extension_api; /* Extension API */

    /* Configuration */
    //默认参考__wt_conn_config_init，config_entries中的配置信息
    const WT_CONFIG_ENTRY **config_entries;

    //operation_timeout_ms配置，默认为0, 生效使用在__wt_op_timer_start
    uint64_t operation_timeout_us; /* Maximum operation period before rollback */

    const char *optrack_path;         /* Directory for operation logs */
    WT_FH *optrack_map_fh;            /* Name to id translation file. */
    WT_SPINLOCK optrack_map_spinlock; /* Translation file spinlock. */
    uintmax_t optrack_pid;            /* Cache the process ID. */

    void **foc;      /* Free-on-close array */
    size_t foc_cnt;  /* Array entries */
    size_t foc_size; /* Array size */

    //wirdtiger.lock对应文件锁
    WT_FH *lock_fh; /* Lock file handle */

    /*
     * The connection keeps a cache of data handles. The set of handles can grow quite large so we
     * maintain both a simple list and a hash table of lists. The hash table key is based on a hash
     * of the table URI.
     */
    //__wt_data_handle对应hash桶  __wt_conn_dhandle_alloc中申请__wt_data_handle和WT_TREE添加到hash桶中
    ////__session_get_dhandle->__session_find_shared_dhandle->__wt_conn_dhandle_alloc会同时添
    //加到__wt_connection_impl.dhhash+dhqh和//WT_SESSION_IMPL.dhandles+dhhash

    //WT_CONN_DHANDLE_INSERT中可以看出，一个是主队列，一个是hash队列, dhqh用于遍历所有活跃表，dhhash用于快速查找指定表，所以这里设计了两个队列
    //WT_CONN_DHANDLE_INSERT把表对应handle添加到队列， WT_CONN_DHANDLE_REMOVE把表对应handle从队列移除
    /* Locked: data handle hash array */
    TAILQ_HEAD(__wt_dhhash, __wt_data_handle) * dhhash;  //dhhash和dhqh的关系，可以参考__evict_walk_choose_dhandle
    /* Locked: data handle list */
    TAILQ_HEAD(__wt_dhandle_qh, __wt_data_handle) dhqh;  //dhhash和dhqh的关系，可以参考__evict_walk_choose_dhandle

    /* Locked: dynamic library handle list */
    TAILQ_HEAD(__wt_dlh_qh, __wt_dlh) dlhqh;
    /* Locked: file list */
    TAILQ_HEAD(__wt_fhhash, __wt_fh) * fhhash;
    TAILQ_HEAD(__wt_fh_qh, __wt_fh) fhqh;

    /* Locked: LSM handle list. */
    TAILQ_HEAD(__wt_lsm_qh, __wt_lsm_tree) lsmqh;
    /* Locked: Tiered system work queue. */
    TAILQ_HEAD(__wt_tiered_qh, __wt_tiered_work_unit) tieredqh;

    WT_SPINLOCK block_lock; /* Locked: block manager list */
    //一个是主队列，一个是hash队列
    TAILQ_HEAD(__wt_blockhash, __wt_block) * blockhash;
    TAILQ_HEAD(__wt_block_qh, __wt_block) blockqh;

    WT_BLKCACHE blkcache; /* Block cache */

    /* Locked: handles in each bucket */
    //connectio相关统计信息 每个桶中的elem个数都记录到这个数组，数组大小dh_hash_size, 可以参考__evict_walk_choose_dhandle
    uint64_t *dh_bucket_count;
    //WT_CONN_DHANDLE_INSERT把表对应handle添加到队列， WT_CONN_DHANDLE_REMOVE把表对应handle从队列移除，实际上代表当前活跃的表
    // 也就是下面这个统计: 
    //    stat_data.py:    DhandleStat('dh_conn_handle_count', 'connection data handles currently active', 'no_clear,no_scale'),
    uint64_t dhandle_count;        /* Locked: handles in the queue */
    u_int open_btree_count;        /* Locked: open writable btree count */
    //下一个新建的.wt文件的文件号__recovery_file_scan
    uint32_t next_file_id;         /* Locked: file ID counter */
    uint32_t open_file_count;      /* Atomic: open file handle count */
    uint32_t open_cursor_count;    /* Atomic: open cursor handle count */
    uint32_t version_cursor_count; /* Atomic: open version cursor count */

    /*
     * WiredTiger allocates space for 50 simultaneous sessions (threads of control) by default.
     * Growing the number of threads dynamically is possible, but tricky since server threads are
     * walking the array without locking it.
     *
     * There's an array of WT_SESSION_IMPL pointers that reference the allocated array; we do it
     * that way because we want an easy way for the server thread code to avoid walking the entire
     * array when only a few threads are running.
     */
    //__wt_connection_open中提前分配内存
    //该conn下面拥有的session总数，可以参考__rollback_to_stable_check
    WT_SESSION_IMPL *sessions; /* Session reference */
    //__conn_session_size中初始化，从配置文件解析后赋值 __conn_session_size
    //节点最多用这么多session
    uint32_t session_size;     /* Session array size */
    //__open_session赋值，包含WT_SESSION_INTERNAL和非WT_SESSION_INTERNAL所有的sessions总和
    uint32_t session_cnt;      /* Session count */

    size_t session_scratch_max; /* Max scratch memory per session */
    //__wt_cache_create
    WT_CACHE *cache;              /* Page cache */
    //__cache_config_loca中解析cacheSize配置
    //yang add todo xxxxxx cache size越大，脏数据比例也会越高，checkpoint耗时也会越长，这期间例如reconfig调整配置也会等待checkpoint完成
    volatile uint64_t cache_size; /* Cache size (either statically
                                     configured or the current size
                                     within a cache pool). */

    WT_TXN_GLOBAL txn_global; /* Global transaction state */

    /*
    Wiredtiger.wt中的下面信息:
    system:checkpoint_snapshot\00
    snapshot_min=1,snapshot_max=1,snapshot_count=0,checkpoint_time=1721461898,write_gen=16\00
    */
    /* Recovery checkpoint snapshot details saved in the metadata file */
    uint64_t recovery_ckpt_snap_min, recovery_ckpt_snap_max;
    uint64_t *recovery_ckpt_snapshot;
    uint32_t recovery_ckpt_snapshot_count;

    WT_RWLOCK hot_backup_lock; /* Hot backup serialization */
    //赋值参考WT_CONN_HOTBACKUP_START
    uint64_t hot_backup_start; /* Clock value of most recent checkpoint needed by hot backup */
    //赋值参考__backup_start
    char **hot_backup_list;    /* Hot backup file list */
    uint32_t *partial_backup_remove_ids; /* Remove btree id list for partial backup */

    //__ckpt_server_start
    WT_SESSION_IMPL *ckpt_session; /* Checkpoint thread session */
    wt_thread_t ckpt_tid;          /* Checkpoint thread */
    bool ckpt_tid_set;             /* Checkpoint thread set */
    WT_CONDVAR *ckpt_cond;         /* Checkpoint wait mutex */
    uint64_t ckpt_most_recent;     /* Clock value of most recent checkpoint */
#define WT_CKPT_LOGSIZE(conn) ((conn)->ckpt_logsize != 0)
    //__ckpt_server_config
    wt_off_t ckpt_logsize; /* Checkpoint log size period */
    bool ckpt_signalled;   /* Checkpoint signalled */

    uint64_t ckpt_apply;      /* Checkpoint handles applied */
    uint64_t ckpt_apply_time; /* Checkpoint applied handles gather time */
    uint64_t ckpt_skip;       /* Checkpoint handles skipped */
    uint64_t ckpt_skip_time;  /* Checkpoint skipped handles gather time */
    //__ckpt_server_config
    uint64_t ckpt_usecs;      /* Checkpoint timer */
    uint64_t ckpt_prep_max;   /* Checkpoint prepare time min/max */
    uint64_t ckpt_prep_min;
    uint64_t ckpt_prep_recent; /* Checkpoint prepare time recent/total */
    uint64_t ckpt_prep_total;
    uint64_t ckpt_time_max; /* Checkpoint time min/max */
    uint64_t ckpt_time_min;
    uint64_t ckpt_time_recent; /* Checkpoint time recent/total */
    uint64_t ckpt_time_total;

    /* Checkpoint stats and verbosity timers */
    struct timespec ckpt_prep_end;
    //__checkpoint_prepare开始时间
    struct timespec ckpt_prep_start;
    struct timespec ckpt_timer_start;
    struct timespec ckpt_timer_scrub_end;

    /* Checkpoint progress message data */
    uint64_t ckpt_progress_msg_count;
    uint64_t ckpt_write_bytes;
    uint64_t ckpt_write_pages;

    /* Checkpoint and incremental backup data */
    //增量备份的粒度，取值"min=4KB,max=2GB"
    //"incremental.granularity"配置
    uint64_t incr_granularity;
    WT_BLKINCR incr_backups[WT_BLKINCR_MAX];

    /* Connection's base write generation. */
    uint64_t base_write_gen;

    /* Last checkpoint connection's base write generation */
    //也就是获取WiredTiger.wt文件中的system:checkpoint_base_write_gen
    uint64_t last_ckpt_base_write_gen;

    //__wt_conn_statistics_config中配置
    uint32_t stat_flags; /* Options declared in flags.py */

    /* Connection statistics */
    uint64_t rec_maximum_seconds; /* Maximum seconds reconciliation took. */
    //配合WT_STAT_CONN_DATA_INCR等阅读
    //__wt_stat_connection_init中赋值，每个指针指向stat_array数组，参考 __wt_stat_connection_init
    WT_CONNECTION_STATS *stats[WT_COUNTER_SLOTS];
    WT_CONNECTION_STATS *stat_array;

    //__wt_capacity_throttle中使用
    WT_CAPACITY capacity;              /* Capacity structure */
    WT_SESSION_IMPL *capacity_session; /* Capacity thread session */
    wt_thread_t capacity_tid;          /* Capacity thread */
    bool capacity_tid_set;             /* Capacity thread set */
    WT_CONDVAR *capacity_cond;         /* Capacity wait mutex */

    WT_LSM_MANAGER lsm_manager; /* LSM worker thread information */

    WT_BUCKET_STORAGE *bstorage;     /* Bucket storage for the connection */
    WT_BUCKET_STORAGE bstorage_none; /* Bucket storage for "none" */

    WT_KEYED_ENCRYPTOR *kencryptor; /* Encryptor for metadata and log */

    //__wt_evict_create创建evict线程完成后置为true
    bool evict_server_running; /* Eviction server operating */

    WT_THREAD_GROUP evict_threads;
    uint32_t evict_threads_max; /* Max eviction threads */
    uint32_t evict_threads_min; /* Min eviction threads */

#define WT_STATLOG_FILENAME "WiredTigerStat.%d.%H"
    WT_SESSION_IMPL *stat_session; /* Statistics log session */
    wt_thread_t stat_tid;          /* Statistics log thread */
    bool stat_tid_set;             /* Statistics log thread set */
    WT_CONDVAR *stat_cond;         /* Statistics log wait mutex */
    const char *stat_format;       /* Statistics log timestamp format */
    WT_FSTREAM *stat_fs;           /* Statistics log stream */
    /* Statistics log json table printing state flag */
    bool stat_json_tables;
    char *stat_path;        /* Statistics log path format */
    char **stat_sources;    /* Statistics log list of objects */
    const char *stat_stamp; /* Statistics log entry timestamp */
    uint64_t stat_usecs;    /* Statistics log period */

    WT_SESSION_IMPL *tiered_session; /* Tiered thread session */
    wt_thread_t tiered_tid;          /* Tiered thread */
    bool tiered_tid_set;             /* Tiered thread set */
    WT_CONDVAR *flush_cond;          /* Flush wait mutex */
    WT_CONDVAR *tiered_cond;         /* Tiered wait mutex */
    uint64_t tiered_interval;        /* Tiered work interval */
    bool tiered_server_running;      /* Internal tiered server operating */
    bool flush_ckpt_complete;        /* Checkpoint after flush completed */
    uint64_t flush_most_recent;      /* Clock value of last flush_tier */
    uint32_t flush_state;            /* State of last flush tier */
    wt_timestamp_t flush_ts;         /* Timestamp of most recent flush_tier */

/* AUTOMATIC FLAG VALUE GENERATION START 0 */
//mongodb 配置log=(enabled=true,archive=true,path=journal,compressor=snappy)
#define WT_CONN_LOG_CONFIG_ENABLED 0x001u  /* Logging is configured */
#define WT_CONN_LOG_DEBUG_MODE 0x002u      /* Debug-mode logging enabled */
#define WT_CONN_LOG_DOWNGRADED 0x004u      /* Running older version */
//FLD_ISSET(conn->log_flags, WT_CONN_LOG_ENABLED)  F_ISSET(btree, WT_BTREE_LOGGED) 
//WT_CONN_LOG_ENABLED代表全局log是否开启，WT_BTREE_LOGGED代表table表级是否开启日志功能
#define WT_CONN_LOG_ENABLED 0x008u         /* Logging is enabled */
//说明有WiredTigerLog.xxxx文件，也就是有wal日志，赋值见__wt_log_open
#define WT_CONN_LOG_EXISTED 0x010u         /* Log files found */
#define WT_CONN_LOG_FORCE_DOWNGRADE 0x020u /* Force downgrade */
#define WT_CONN_LOG_RECOVER_DIRTY 0x040u   /* Recovering unclean */
#define WT_CONN_LOG_RECOVER_DONE 0x080u    /* Recovery completed */
//"log=(recover=error)"配置则会置位该标识，然后__wt_log_needs_recovery中做检查，判断释放需要recover，如果需要recover则wt需要带上-R(也就是配置"log=(recover=on)")进行数据恢复
//否则__wt_log_needs_recovery外层会直接抛异常
#define WT_CONN_LOG_RECOVER_ERR 0x100u     /* Error if recovery required */
#define WT_CONN_LOG_RECOVER_FAILED 0x200u  /* Recovery failed */
//默认为ture, log.remove配置，值默认为true
#define WT_CONN_LOG_REMOVE 0x400u          /* Removal is enabled */
//默认false zero_fill=false
#define WT_CONN_LOG_ZERO_FILL 0x800u       /* Manually zero files */
                                           /* AUTOMATIC FLAG VALUE GENERATION STOP 32 */

    //journal日志相关，默认mongod配置log=(enabled=true,archive=true,path=journal,compressor=snappy)
    //默认使能为WT_CONN_LOG_ENABLED，参考__wt_logmgr_create
    uint32_t log_flags;                    /* Global logging configuration */
    //__log_server线程等待该信号，__log_newfile  __wt_log_ckpt  __log_write_internal发送信号
    //默认50ms __log_server线程进行一次强制刷盘，见__wt_logmgr_open
    WT_CONDVAR *log_cond;                  /* Log server wait mutex */
    WT_SESSION_IMPL *log_session;          /* Log server session */
    wt_thread_t log_tid;                   /* Log server thread */
    bool log_tid_set;                      /* Log server thread set */
    WT_CONDVAR *log_file_cond;             /* Log file thread wait mutex */
    WT_SESSION_IMPL *log_file_session;     /* Log file thread session */
    wt_thread_t log_file_tid;              /* Log file thread */
    bool log_file_tid_set;                 /* Log file thread set */
    //__log_wrlsn_server write lsn线程等待该cond，然后__wt_log_wrlsn把日志写入磁盘，__log_wait_for_earlier_slot发送信号
    //wait一次最多等待10ms左右，见__wt_logmgr_open   
    WT_CONDVAR *log_wrlsn_cond;            /* Log write lsn thread wait mutex */
    WT_SESSION_IMPL *log_wrlsn_session;    /* Log write lsn thread session */
    wt_thread_t log_wrlsn_tid;             /* Log write lsn thread */
    bool log_wrlsn_tid_set;                /* Log write lsn thread set */
    //__wt_logmgr_create
    WT_LOG *log;                           /* Logging structure */
    //journal日志压缩算法
    WT_COMPRESSOR *log_compressor;         /* Logging compressor */
    uint32_t log_cursors;                  /* Log cursor count */
    //"log.os_cache_dirty_pct"配置，默认值为0
    wt_off_t log_dirty_max;                /* Log dirty system cache max size */
    //log.file_max配置  file_max=100MB
    wt_off_t log_file_max;                 /* Log file max size */
    //"log.force_write_wait"配置，默认值为0
    uint32_t log_force_write_wait;         /* Log force write wait configuration */
/*
[user_00@xxx /data2/containers/175591266/db]$ cd journal/
[user_00@xxx /data2/containers/175591266/db/journal]$ ls
WiredTigerLog.0000047087  WiredTigerPreplog.0000039400
[user_00@xxx /data2/containers/175591266/db/journal]$
*/ //journal日志目录
    const char *log_path;                  /* Logging path format */
    //"log.prealloc"配置，默认true，初始值1，见__wt_logmgr_config，
    //一次性创建log_prealloc个WiredTigerPreplog.xxxxx文件，见__log_prealloc_once
    uint32_t log_prealloc;                 /* Log file pre-allocation */
    uint16_t log_req_max;                  /* Max required log version */
    uint16_t log_req_min;                  /* Min required log version */
    //transaction_sync配置，参考__logmgr_sync_cfg，
    //最终在__wt_txn_begin中被赋值给txn->txn_logsync使用
    uint32_t txn_logsync;                  /* Log sync configuration */

    WT_SESSION_IMPL *meta_ckpt_session; /* Metadata checkpoint session */

    /*
     * Is there a data/schema change that needs to be the part of a checkpoint.
     */
    bool modified;

    //Sweep-Server Dhandle Sweep参考http://source.wiredtiger.com/11.1.0/arch-dhandle.html#dhandle_data_handle_creation
    WT_SESSION_IMPL *sweep_session; /* Handle sweep session */
    wt_thread_t sweep_tid;          /* Handle sweep thread */
    int sweep_tid_set;              /* Handle sweep thread set */
    WT_CONDVAR *sweep_cond;         /* Handle sweep wait mutex */
    //mongod默认配置file_manager=(close_idle_time=600,close_scan_interval=10,close_handle_minimum=2000)
    //db.adminCommand({setParameter:1, wiredTigerEngineRuntimeConfig:'file_manager=(close_handle_minimum=100)'})
    //close_idle_time配置
    uint64_t sweep_idle_time;       /* Handle sweep idle time */
    //close_scan_interval参数配置，代表sweep server线程清理不活跃表的周期
    uint64_t sweep_interval;        /* Handle sweep interval */
    //close_handle_minimum参数配置
    uint64_t sweep_handles_min;     /* Handle sweep minimum open */

    /* Locked: collator list */
    TAILQ_HEAD(__wt_coll_qh, __wt_named_collator) collqh;

    /* Locked: compressor list */
    //__conn_add_compressor中添加
    TAILQ_HEAD(__wt_comp_qh, __wt_named_compressor) compqh;

    /* Locked: data source list */
    //参考__conn_add_data_source
    TAILQ_HEAD(__wt_dsrc_qh, __wt_named_data_source) dsrcqh;

    /* Locked: encryptor list */
    //WT_CONNECTION->add_encryptor注册加密算法到该hash桶中  add_my_encryptors
    WT_SPINLOCK encryptor_lock; /* Encryptor list lock */
    TAILQ_HEAD(__wt_encrypt_qh, __wt_named_encryptor) encryptqh;

    /* Locked: extractor list */
    TAILQ_HEAD(__wt_extractor_qh, __wt_named_extractor) extractorqh;

    /* Locked: storage source list */
    WT_SPINLOCK storage_lock; /* Storage source list lock */
    TAILQ_HEAD(__wt_storage_source_qh, __wt_named_storage_source) storagesrcqh;

    void *lang_private; /* Language specific private storage */

    /* If non-zero, all buffers used for I/O will be aligned to this. */
    size_t buffer_alignment;

    //赋值参考__wt_stash_add
    uint64_t stashed_bytes; /* Atomic: stashed memory statistics */
    uint64_t stashed_objects;
    /* Generations manager */
    //注意conn gen和session gen的区别
    volatile uint64_t generations[WT_GENERATIONS];

    //file_extend配置，默认为0
    wt_off_t data_extend_len; /* file_extend data length */
    //file_extend配置，默认不配置，值为WT_CONFIG_UNSET
    //在__wt_logmgr_config中赋值为conn->log_file_max  //默认100M
    wt_off_t log_extend_len;  /* file_extend log length */

/* AUTOMATIC FLAG VALUE GENERATION START 0 */
#define WT_DIRECT_IO_CHECKPOINT 0x1u /* Checkpoints */
#define WT_DIRECT_IO_DATA 0x2u       /* Data files */
#define WT_DIRECT_IO_LOG 0x4u        /* Log files */
                                     /* AUTOMATIC FLAG VALUE GENERATION STOP 32 */
    uint32_t direct_io;              /* O_DIRECT, FILE_FLAG_NO_BUFFERING */
    uint64_t write_through;          /* FILE_FLAG_WRITE_THROUGH */

    bool mmap;     /* use mmap when reading checkpoints */
    bool mmap_all; /* use mmap for all I/O on data files */
    int page_size; /* OS page size for mmap alignment */

    WT_LSN *debug_ckpt;      /* Debug mode checkpoint LSNs. */
    size_t debug_ckpt_alloc; /* Checkpoint retention allocated. */
    uint32_t debug_ckpt_cnt; /* Checkpoint retention number. */
    uint32_t debug_log_cnt;  /* Log file retention count */

/* AUTOMATIC FLAG VALUE GENERATION START 0 */
#define WT_CONN_DEBUG_CKPT_RETAIN 0x01u
#define WT_CONN_DEBUG_CORRUPTION_ABORT 0x02u
#define WT_CONN_DEBUG_CURSOR_COPY 0x04u
#define WT_CONN_DEBUG_CURSOR_REPOSITION 0x08u
#define WT_CONN_DEBUG_REALLOC_EXACT 0x10u
#define WT_CONN_DEBUG_REALLOC_MALLOC 0x20u
#define WT_CONN_DEBUG_SLOW_CKPT 0x40u
#define WT_CONN_DEBUG_UPDATE_RESTORE_EVICT 0x80u
    /* AUTOMATIC FLAG VALUE GENERATION STOP 16 */
    uint16_t debug_flags;

    /* Verbose settings for our various categories. */
    WT_VERBOSE_LEVEL verbose[WT_VERB_NUM_CATEGORIES];

/* AUTOMATIC FLAG VALUE GENERATION START 0 */
#define WT_JSON_OUTPUT_ERROR 0x1u
#define WT_JSON_OUTPUT_MESSAGE 0x2u
    /* AUTOMATIC FLAG VALUE GENERATION STOP 8 */
    uint8_t json_output; /* Output event handler messages in JSON format. */

/*
 * Variable with flags for which subsystems the diagnostic stress timing delays have been requested.
 */
/* AUTOMATIC FLAG VALUE GENERATION START 0 */
#define WT_TIMING_STRESS_AGGRESSIVE_SWEEP 0x000001u
#define WT_TIMING_STRESS_BACKUP_RENAME 0x000002u
#define WT_TIMING_STRESS_CHECKPOINT_EVICT_PAGE 0x000004u
#define WT_TIMING_STRESS_CHECKPOINT_SLOW 0x000008u
#define WT_TIMING_STRESS_CHECKPOINT_STOP 0x000010u
#define WT_TIMING_STRESS_COMPACT_SLOW 0x000020u
#define WT_TIMING_STRESS_EVICT_REPOSITION 0x000040u
#define WT_TIMING_STRESS_FAILPOINT_EVICTION_FAIL_AFTER_RECONCILIATION 0x000080u
#define WT_TIMING_STRESS_FAILPOINT_HISTORY_STORE_DELETE_KEY_FROM_TS 0x000100u
#define WT_TIMING_STRESS_HS_CHECKPOINT_DELAY 0x000200u
#define WT_TIMING_STRESS_HS_SEARCH 0x000400u
#define WT_TIMING_STRESS_HS_SWEEP 0x000800u
#define WT_TIMING_STRESS_PREPARE_CHECKPOINT_DELAY 0x001000u
#define WT_TIMING_STRESS_SLEEP_BEFORE_READ_OVERFLOW_ONPAGE 0x002000u
#define WT_TIMING_STRESS_SPLIT_1 0x004000u
#define WT_TIMING_STRESS_SPLIT_2 0x008000u
#define WT_TIMING_STRESS_SPLIT_3 0x010000u
#define WT_TIMING_STRESS_SPLIT_4 0x020000u
#define WT_TIMING_STRESS_SPLIT_5 0x040000u
#define WT_TIMING_STRESS_SPLIT_6 0x080000u
#define WT_TIMING_STRESS_SPLIT_7 0x100000u
#define WT_TIMING_STRESS_TIERED_FLUSH_FINISH 0x200000u
    /* AUTOMATIC FLAG VALUE GENERATION STOP 32 */
    //__wt_timing_stress_config配置，如果配置了则不会进行随机sleep, 默认为0
    uint32_t timing_stress_flags;

//__wt_os_stdio
#define WT_STDERR(s) (&S2C(s)->wt_stderr)
#define WT_STDOUT(s) (&S2C(s)->wt_stdout)
//__wt_os_stdio中默认初始化
    WT_FSTREAM wt_stderr, wt_stdout;

    /*
     * File system interface abstracted to support alternative file system implementations.
     */
    //__wt_os_posix中赋值
    WT_FILE_SYSTEM *file_system;

/*
 * Server subsystem flags.
 */
/* AUTOMATIC FLAG VALUE GENERATION START 0 */
#define WT_CONN_SERVER_CAPACITY 0x01u
#define WT_CONN_SERVER_CHECKPOINT 0x02u
#define WT_CONN_SERVER_LOG 0x04u
#define WT_CONN_SERVER_LSM 0x08u
#define WT_CONN_SERVER_STATISTICS 0x10u
#define WT_CONN_SERVER_SWEEP 0x20u
#define WT_CONN_SERVER_TIERED 0x40u
    /* AUTOMATIC FLAG VALUE GENERATION STOP 32 */
    uint32_t server_flags;

/* AUTOMATIC FLAG VALUE GENERATION START 0 */
#define WT_CONN_BACKUP_PARTIAL_RESTORE 0x0000001u
#define WT_CONN_CACHE_CURSORS 0x0000002u
#define WT_CONN_CACHE_POOL 0x0000004u
#define WT_CONN_CKPT_GATHER 0x0000008u
#define WT_CONN_CKPT_SYNC 0x0000010u
#define WT_CONN_CLOSING 0x0000020u
//connect close的时候设置该表示
#define WT_CONN_CLOSING_CHECKPOINT 0x0000040u
#define WT_CONN_CLOSING_NO_MORE_OPENS 0x0000080u
#define WT_CONN_COMPATIBILITY 0x0000100u
#define WT_CONN_DATA_CORRUPTION 0x0000200u
//Check to decide if the eviction thread should continue running.
#define WT_CONN_EVICTION_RUN 0x0000400u
#define WT_CONN_HS_OPEN 0x0000800u
//"incremental.enabled"
#define WT_CONN_INCR_BACKUP 0x0001000u
#define WT_CONN_IN_MEMORY 0x0002000u
//leak_memory配置
#define WT_CONN_LEAK_MEMORY 0x0004000u
#define WT_CONN_LSM_MERGE 0x0008000u
#define WT_CONN_MINIMAL 0x0010000u
#define WT_CONN_OPTRACK 0x0020000u
#define WT_CONN_PANIC 0x0040000u
//readonly配置
#define WT_CONN_READONLY 0x0080000u
#define WT_CONN_READY 0x0100000u
#define WT_CONN_RECONFIGURING 0x0200000u
#define WT_CONN_RECOVERING 0x0400000u
#define WT_CONN_RECOVERY_COMPLETE 0x0800000u
#define WT_CONN_SALVAGE 0x1000000u
#define WT_CONN_TIERED_FIRST_FLUSH 0x2000000u
//存在"WiredTiger.backup"文件说明是hotback文件
#define WT_CONN_WAS_BACKUP 0x4000000u

    /* AUTOMATIC FLAG VALUE GENERATION STOP 32 */
    uint32_t flags;
};
