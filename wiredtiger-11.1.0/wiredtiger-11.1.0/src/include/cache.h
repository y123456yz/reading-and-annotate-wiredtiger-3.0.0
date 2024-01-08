/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

/*
 * Helper: in order to read without any calls to eviction, we have to ignore the cache size and
 * disable splits.
 */
#define WT_READ_NO_EVICT (WT_READ_IGNORE_CACHE_SIZE | WT_READ_NO_SPLIT)

/*
 * Tuning constants: I hesitate to call this tuning, but we want to review some number of pages from
 * each file's in-memory tree for each page we evict.
 */
#define WT_EVICT_MAX_TREES 1000 /* Maximum walk points */
#define WT_EVICT_WALK_BASE 300  /* Pages tracked across file visits */
#define WT_EVICT_WALK_INCR 100  /* Pages added each walk */

/*
 * WT_EVICT_ENTRY --
 *	Encapsulation of an eviction candidate.
 */
//赋值参考__evict_push_candidate
struct __wt_evict_entry {
    WT_BTREE *btree; /* Enclosing btree object */
    WT_REF *ref;     /* Page to flush/evict */
    //该page评分__evict_entry_priority
    uint64_t score;  /* Relative eviction priority */
};

//WT_CACHE.evict_queues[]数组为该类型，evict_queues[0]代表当前evict的page对应的队列，即evict_current_queue
//，evict_queues[1]代表当前evict_other_queue，evict_queues[2]代表evict_urgent_queue
//赋值见__wt_cache_create
#define WT_EVICT_QUEUE_MAX 3    /* Two ordinary queues plus urgent */
#define WT_EVICT_URGENT_QUEUE 2 /* Urgent queue index */

/*
 * WT_EVICT_QUEUE --
 *	Encapsulation of an eviction candidate queue.
 Eviction is managed using WT_EVICT_QUEUE structures, each of which contains a list of WT_EVICT_ENTRY structures.
//WT_CACHE.evict_queues[]数组为该类型，evict_queues[0]代表当前evict的page对应的队列，即evict_current_queue
//，evict_queues[1]代表当前evict_other_queue，evict_queues[2]代表evict_urgent_queue
 */
struct __wt_evict_queue {
    WT_SPINLOCK evict_lock;        /* Eviction LRU queue */
    //每个evict_queues[]中包含evict_slots个成员，一次性提前分配cache->evict_slots个WT_EVICT_ENTRY空间，赋值见__wt_cache_create
    //evict_queue是一个数组
    WT_EVICT_ENTRY *evict_queue;   /* LRU pages being tracked */
    //代表当前消费到了evict_queues[]数组的那个位置
    WT_EVICT_ENTRY *evict_current; /* LRU current page to be evicted */
    //队列中总的候选page
    uint32_t evict_candidates;     /* LRU list pages to evict */
    uint32_t evict_entries;        /* LRU entries in the queue */
    //记录队列中历史最大elem个数
    volatile uint32_t evict_max;   /* LRU maximum eviction slot used */
};

/* Cache operations. */
typedef enum __wt_cache_op {
    //__checkpoint_tree
    WT_SYNC_CHECKPOINT,
    WT_SYNC_CLOSE,
    WT_SYNC_DISCARD,
    WT_SYNC_WRITE_LEAVES
} WT_CACHE_OP;

#define WT_HS_FILE_MIN (100 * WT_MEGABYTE)

/*
 * WiredTiger cache structure.
 Internally, WiredTiger's cache state is represented by the WT_CACHE structure, which contains counters and
 parameter settings for tracking cache usage and controlling eviction policy. The WT_CACHE also includes state
 WiredTiger uses to track the progress of eviction. There is a single WT_CACHE for each connection, accessed via
 the WT_CONNECTION_IMPL structure.

//每个connection对应一个WT_CACHE(__wt_cache)及WT_CONNECTION_IMPL(__wt_connection_impl)
//WT_CONNECTION_IMPL->cache为该结构
 */
struct __wt_cache {
    /*
     * Different threads read/write pages to/from the cache and create pages in the cache, so we
     * cannot know precisely how much memory is in use at any specific time. However, even though
     * the values don't have to be exact, they can't be garbage, we track what comes in and what
     * goes out and calculate the difference as needed.
     */
    //所有page相关的数据统计
    uint64_t bytes_dirty_intl; /* Bytes/pages currently dirty */
    uint64_t bytes_dirty_leaf;
    uint64_t bytes_dirty_total;
    uint64_t bytes_evict;      /* Bytes/pages discarded by eviction */
    uint64_t bytes_image_intl; /* Bytes of disk images (internal) */
    //在磁盘中的leaf数据有多少 __wt_cache_page_image_incr
    uint64_t bytes_image_leaf; /* Bytes of disk images (leaf) */
    uint64_t bytes_inmem;      /* Bytes/pages in memory */
    uint64_t bytes_internal;   /* Bytes of internal pages */
    uint64_t bytes_read;       /* Bytes read into memory */
    uint64_t bytes_updates;    /* Bytes of updates to pages */
    uint64_t bytes_written;

    /*
     * History store cache usage. TODO: The values for these variables are cached and potentially
     * outdated.
     */
    uint64_t bytes_hs;       /* History store bytes inmem */
    uint64_t bytes_hs_dirty; /* History store bytes inmem dirty */

    //脏页page数量统计
    uint64_t pages_dirty_intl;
    uint64_t pages_dirty_leaf;
    //内存中挑选出来需要evict的page数
    uint64_t pages_evicted;
    //__wt_page_alloc  内存中的page数  __wt_cache_pages_inuse pages_inmem-pages_evicted=当前内存中正在使用的page数量
    uint64_t pages_inmem;

    //__wt_cache_page_evict中自增，代表evict的page总数
    volatile uint64_t eviction_progress; /* Eviction progress count */
    uint64_t last_eviction_progress;     /* Tracked eviction progress */

    uint64_t app_waits;  /* User threads waited for cache */
    uint64_t app_evicts; /* Pages evicted by user threads */

    //也就是历史最大的page->memory_footprint
    uint64_t evict_max_page_size; /* Largest page seen at eviction */
    struct timespec stuck_time;   /* Stuck time */

    /*
     * Read information.
     */
    uint64_t read_gen;        /* Current page read generation */
    uint64_t read_gen_oldest; /* Oldest read generation the eviction
                               * server saw in its last queue load */
    //__evict_pass中自增，实际上代表进行了多少轮__evict_pass
    uint64_t evict_pass_gen;  /* Number of eviction passes */

    /*
     * Eviction thread information.
     
     //用户线程发送evict_cond信号: __wt_cache_eviction_check->__wt_cache_eviction_worker->__wt_evict_server_wake
     //evict server线程发送evict_cond信号: //__evict_walk_tree->__wt_page_evict_urgent->__wt_evict_server_wake
     */
    //__wt_evict_server_wake发送信号   __wt_evict_thread_run  __evict_pass等待信号
    //"cache eviction server"
    WT_CONDVAR *evict_cond;      /* Eviction server condition */
    WT_SPINLOCK evict_walk_lock; /* Eviction walk location */

    /*
     * Eviction threshold percentages use double type to allow for specifying percentages less than
     * one.
     */
    //赋值参考__cache_config_local
    //eviction_dirty_target=5, eviction_dirty_trigger=20,eviction_target=80,eviction_trigger=95,eviction_updates_target=0,eviction_updates_trigger=0
    double eviction_dirty_target;    /* Percent to allow dirty */
    double eviction_dirty_trigger;   /* Percent to trigger dirty eviction */
    double eviction_trigger;         /* Percent to trigger eviction */
    double eviction_target;          /* Percent to end eviction */
    //如果不配置，默认为cache->eviction_dirty_target / 2;
    double eviction_updates_target;  /* Percent to allow for updates */
    //update trigger不能超过cache->eviction_trigger
    double eviction_updates_trigger; /* Percent of updates to trigger eviction */

    //默认值为1， eviction_checkpoint_target配置
    double eviction_checkpoint_target; /* Percent to reduce dirty
                                        to during checkpoint scrubs */
    double eviction_scrub_target;      /* Current scrub target */
    //cache_overhead=8，默认等于8
    //assume the heap allocator overhead is the specified percentage, and adjust the cache usage by that amount (for example, if there is 10GB of data in cache, a percentage of 10 means WiredTiger treats this as 11GB). This value is configurable because different heap allocators have different overhead and different workloads will have different heap allocation sizes and patterns, therefore applications may need to adjust this value based on allocator choice and behavior in measured workloads.
    u_int overhead_pct;         /* Cache percent adjustment */
    //cache_max_wait_ms配置，默认为0
    //__wt_cache_eviction_worker优先看session维度的cache_max_wait_ms配置，如果session维度没有配置，则以conn维度的cache_max_wait_ms配置为准
    //cache_max_wait_ms配置，默认为0
    uint64_t cache_max_wait_us; /* Maximum time an operation waits for
                                 * space in cache */

    /*
     * Eviction thread tuning information.
     */
    uint32_t evict_tune_datapts_needed;          /* Data needed to tune */
    struct timespec evict_tune_last_action_time; /* Time of last action */
    struct timespec evict_tune_last_time;        /* Time of last check */
    uint32_t evict_tune_num_points;              /* Number of values tried */
    uint64_t evict_tune_progress_last;           /* Progress counter */
    uint64_t evict_tune_progress_rate_max;       /* Max progress rate */
    bool evict_tune_stable;                      /* Are we stable? */
    uint32_t evict_tune_workers_best;            /* Best performing value */

    /*
     * Pass interrupt counter.
     */
    volatile uint32_t pass_intr; /* Interrupt eviction pass. */

    /*
     * LRU eviction list information.
     */
    WT_SPINLOCK evict_pass_lock;   /* Eviction pass lock */
    //对应session名为"evict pass"  __evict_walk __evict_walk_tree使用该session
    WT_SESSION_IMPL *walk_session; /* Eviction pass session */
    //记录当前正在挑选那个表上的page进行evict操作，赋值见__evict_walk
    WT_DATA_HANDLE *walk_tree;     /* LRU walk current tree */

    WT_SPINLOCK evict_queue_lock; /* Eviction current queue lock */
    //WT_CACHE.evict_queues[]数组为该类型，evict_queues[0]代表当前evict的page对应的队列，即evict_current_queue和evict_fill_queue
    //，evict_queues[1]代表当前evict_other_queue，evict_queues[2]代表evict_urgent_queue
    //赋值见__wt_cache_create
    WT_EVICT_QUEUE evict_queues[WT_EVICT_QUEUE_MAX];
    WT_EVICT_QUEUE *evict_current_queue; /* LRU current queue in use */
    //__evict_lru_walk挑选的需要evict的page(不包括urgent)添加到fill队列
    WT_EVICT_QUEUE *evict_fill_queue;    /* LRU next queue to fill.
                                            This is usually the same as the
                                            "other" queue but under heavy
                                            load the eviction server will
                                            start filling the current queue
                                            before it switches. */
    WT_EVICT_QUEUE *evict_other_queue;   /* LRU queue not in use */
    WT_EVICT_QUEUE *evict_urgent_queue;  /* LRU urgent queue */
    //__wt_cache_create中初始化赋值为WT_EVICT_WALK_BASE + WT_EVICT_WALK_INCR;
    uint32_t evict_slots;                /* LRU list eviction slots */

#define WT_EVICT_SCORE_BUMP 10
#define WT_EVICT_SCORE_CUTOFF 10
#define WT_EVICT_SCORE_MAX 100
    /*
     * Score of how aggressive eviction should be about selecting eviction candidates. If eviction
     * is struggling to make progress, this score rises (up to a maximum of 100), at which point the
     * cache is "stuck" and transactions will be rolled back.
     */
    uint32_t evict_aggressive_score;

    /*
     * Score of how often LRU queues are empty on refill. This score varies between 0 (if the queue
     * hasn't been empty for a long time) and 100 (if the queue has been empty the last 10 times we
     * filled up.
     */
    //赋值见__evict_lru_walk，这个值代表队列中是否为空的占比，是经常为空(100)还是经常不位空(0)
    //评分越高说明消费速度比入队速度更快
    //__evict_lru_walk中赋值
    uint32_t evict_empty_score;

    uint32_t hs_fileid; /* History store table file ID */

    /*
     * The "history_activity" verbose messages are throttled to once per checkpoint. To accomplish
     * this we track the checkpoint generation for the most recent read and write verbose messages.
     */
    uint64_t hs_verb_gen_read;
    uint64_t hs_verb_gen_write;

    /*
     * Cache pool information.
     */
    uint64_t cp_pass_pressure;   /* Calculated pressure from this pass */
    uint64_t cp_quota;           /* Maximum size for this cache */
    uint64_t cp_reserved;        /* Base size for this cache */
    WT_SESSION_IMPL *cp_session; /* May be used for cache management */
    uint32_t cp_skip_count;      /* Post change stabilization */
    wt_thread_t cp_tid;          /* Thread ID for cache pool manager */
    /* State seen at the last pass of the shared cache manager */
    uint64_t cp_saved_app_evicts; /* User eviction count at last review */
    uint64_t cp_saved_app_waits;  /* User wait count at last review */
    uint64_t cp_saved_read;       /* Read count at last review */

/*
 * Flags.
 */
/* AUTOMATIC FLAG VALUE GENERATION START 0 */
#define WT_CACHE_POOL_MANAGER 0x1u /* The active cache pool manager */
#define WT_CACHE_POOL_RUN 0x2u     /* Cache pool thread running */
                                   /* AUTOMATIC FLAG VALUE GENERATION STOP 32 */
    uint32_t pool_flags;           /* Cache pool flags */

/* AUTOMATIC FLAG VALUE GENERATION START 0 */
//超过了eviction_target(默认80%)配置
#define WT_CACHE_EVICT_CLEAN 0x001u        /* Evict clean pages */
//超过了总内存的eviction_trigger(默认95%)
#define WT_CACHE_EVICT_CLEAN_HARD 0x002u   /* Clean % blocking app threads */
#define WT_CACHE_EVICT_DEBUG_MODE 0x004u   /* Aggressive debugging mode */
//脏数据内存超过了总内存eviction_dirty_trigger(默认20%)
#define WT_CACHE_EVICT_DIRTY 0x008u        /* Evict dirty pages */
//leaf page脏数据内存超过了总内存eviction_dirty_trigger(默认20%)
#define WT_CACHE_EVICT_DIRTY_HARD 0x010u   /* Dirty % blocking app threads */
#define WT_CACHE_EVICT_NOKEEP 0x020u       /* Don't add read pages to cache */
//__evict_update_work
#define WT_CACHE_EVICT_SCRUB 0x040u        /* Scrub dirty pages */
#define WT_CACHE_EVICT_UPDATES 0x080u      /* Evict pages with updates */
#define WT_CACHE_EVICT_UPDATES_HARD 0x100u /* Update % blocking app threads */
//evict_urgent_queue不为空，置位该标识
#define WT_CACHE_EVICT_URGENT 0x200u       /* Pages are in the urgent queue */
/* AUTOMATIC FLAG VALUE GENERATION STOP 32 */
#define WT_CACHE_EVICT_ALL (WT_CACHE_EVICT_CLEAN | WT_CACHE_EVICT_DIRTY | WT_CACHE_EVICT_UPDATES)
#define WT_CACHE_EVICT_HARD \
    (WT_CACHE_EVICT_CLEAN_HARD | WT_CACHE_EVICT_DIRTY_HARD | WT_CACHE_EVICT_UPDATES_HARD)
    uint32_t flags;
};

#define WT_WITH_PASS_LOCK(session, op)                                                   \
    do {                                                                                 \
        WT_WITH_LOCK_WAIT(session, &cache->evict_pass_lock, WT_SESSION_LOCKED_PASS, op); \
    } while (0)

/*
 * WT_CACHE_POOL --
 *	A structure that represents a shared cache.

 When shared caching is enabled, WiredTiger creates a cache pool server thread to manage the shared cache. It also
 allocates a global WT_CACHE_POOL structure, which stores settings and statistics for the shared cache. These settings
 include a minimum and maximum cache size for connections participating in the shared cache.
 */
////__wt_process.cache_pool存入这个全局变量成员中
struct __wt_cache_pool {
    WT_SPINLOCK cache_pool_lock;
    WT_CONDVAR *cache_pool_cond;
    const char *name;
    uint64_t size;
    uint64_t chunk;
    uint64_t quota;
    uint64_t currently_used;
    uint32_t refs; /* Reference count for structure. */
    /* Locked: List of connections participating in the cache pool. */
    //为了计算所有conn上面的内存情况及内存压力
    TAILQ_HEAD(__wt_cache_pool_qh, __wt_connection_impl) cache_pool_qh;

    uint8_t pool_managed; /* Cache pool has a manager thread */

/* AUTOMATIC FLAG VALUE GENERATION START 0 */
#define WT_CACHE_POOL_ACTIVE 0x1u /* Cache pool is active */
                                  /* AUTOMATIC FLAG VALUE GENERATION STOP 8 */
    uint8_t flags;
};

/*
 * Optimize comparisons against the history store URI, flag handles that reference the history store
 * file.
 */
#define WT_IS_HS(dh) F_ISSET(dh, WT_DHANDLE_HS)

/* Flags used with __wt_evict */
/* AUTOMATIC FLAG VALUE GENERATION START 0 */
//__wt_evict_file
#define WT_EVICT_CALL_CLOSING 0x1u  /* Closing connection or tree */
#define WT_EVICT_CALL_NO_SPLIT 0x2u /* Splits not allowed */
#define WT_EVICT_CALL_URGENT 0x4u   /* Urgent eviction */
/* AUTOMATIC FLAG VALUE GENERATION STOP 32 */
