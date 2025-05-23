/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#define WT_TXN_NONE 0                /* Beginning of time */
#define WT_TXN_FIRST 1               /* First transaction to run */
#define WT_TXN_MAX (UINT64_MAX - 10) /* End of time */
//__wt_txn_rollback事务回滚时候置位，一个事务中任何一个操作异常，其他写操作op都需要回滚，直接置位为WT_TXN_ABORTED
// 这样当reconcile evict等遇到该标识，都会直接跳过，例如__rec_upd_select
#define WT_TXN_ABORTED UINT64_MAX    /* Update rolled back */

#define WT_TS_NONE 0         /* Beginning of time */
#define WT_TS_MAX UINT64_MAX /* End of time */

/*
 * A list of reasons for returning a rollback error from the API. These reasons can be queried via
 * the session get rollback reason API call. Users of the API could have a dependency on the format
 * of these messages so changing them must be done with care.
 */
//注意，实际上__wt_wiredtiger_error对外的错误码WT_ROLLBACK信息全部是统一的，__wt_txn_rollback_required 
#define WT_TXN_ROLLBACK_REASON_CACHE_OVERFLOW "transaction rolled back because of cache overflow"
#define WT_TXN_ROLLBACK_REASON_CONFLICT "conflict between concurrent operations"
#define WT_TXN_ROLLBACK_REASON_OLDEST_FOR_EVICTION \
    "oldest pinned transaction ID rolled back for eviction"

/* AUTOMATIC FLAG VALUE GENERATION START 0 */
#define WT_TXN_LOG_CKPT_CLEANUP 0x01u
#define WT_TXN_LOG_CKPT_PREPARE 0x02u
#define WT_TXN_LOG_CKPT_START 0x04u
#define WT_TXN_LOG_CKPT_STOP 0x08u
#define WT_TXN_LOG_CKPT_SYNC 0x10u
/* AUTOMATIC FLAG VALUE GENERATION STOP 32 */

/* AUTOMATIC FLAG VALUE GENERATION START 0 */
#define WT_TXN_OLDEST_STRICT 0x1u
#define WT_TXN_OLDEST_WAIT 0x2u
/* AUTOMATIC FLAG VALUE GENERATION STOP 32 */

/* AUTOMATIC FLAG VALUE GENERATION START 0 */
#define WT_TXN_TS_ALREADY_LOCKED 0x1u
#define WT_TXN_TS_INCLUDE_CKPT 0x2u
#define WT_TXN_TS_INCLUDE_OLDEST 0x4u
/* AUTOMATIC FLAG VALUE GENERATION STOP 32 */

typedef enum {
    WT_VISIBLE_FALSE = 0,   /* Not a visible update */
    //__wt_txn_prepare prepare的事务会释放快照，这样就会影响evict和可见性，因此prepare节点释放快照，这样可以减轻内存压力
    // 但是可见性会不一致，因此会增加对WT_PREPARE_INPROGRESS的WT_VISIBLE_PREPARE(见__wt_txn_upd_visible_type)，并增加udp
    // WT_PREPARE_CONFLICT冲突检查(见__wt_txn_read_upd_list_internal)

    
    //赋值参考__wt_txn_upd_visible_type， 如果prepare状态为prepare_state == WT_PREPARE_INPROGRESS，则为这个状态
    //真正生效见__wt_txn_read_upd_list_internal
    WT_VISIBLE_PREPARE = 1, /* Prepared update */
    WT_VISIBLE_TRUE = 2     /* A visible update */
} WT_VISIBLE_TYPE;

/*
 * Transaction ID comparison dealing with edge cases.
 *
 * WT_TXN_ABORTED is the largest possible ID (never visible to a running transaction), WT_TXN_NONE
 * is smaller than any possible ID (visible to all running transactions).
 */
#define WT_TXNID_LE(t1, t2) ((t1) <= (t2))

#define WT_TXNID_LT(t1, t2) ((t1) < (t2))
//该session->id对应的__wt_txn_shared
//这里(s)->id代表session id，txn_shared_list[(s)->id])代表该session id对应的事务id
//WT_SESSION_TXN_SHARED是全局共享的，记录了所有的事务，[]数组每个成员内容代表一个session操作的事务，这里要注意如果是多文档操作，会有多个cursor，这时候在同一个事务中实际上会有多个session对这个多文档事务进行操作，参考src/test/csuit/timestamp_abort
#define WT_SESSION_TXN_SHARED(s)                         \
    (S2C(s)->txn_global.txn_shared_list == NULL ? NULL : \
                                                  &S2C(s)->txn_global.txn_shared_list[(s)->id])

#define WT_SESSION_IS_CHECKPOINT(s) ((s)->id != 0 && (s)->id == S2C(s)->txn_global.checkpoint_id)

/*
 * Perform an operation at the specified isolation level.
 *
 * This is fiddly: we can't cope with operations that begin transactions (leaving an ID allocated),
 * and operations must not move our published snap_min forwards (or updates we need could be freed
 * while this operation is in progress). Check for those cases: the bugs they cause are hard to
 * debug.
 */
//__evict_reconcile
#define WT_WITH_TXN_ISOLATION(s, iso, op)                                       \
    do {                                                                        \
        WT_TXN_ISOLATION saved_iso = (s)->isolation;                            \
        WT_TXN_ISOLATION saved_txn_iso = (s)->txn->isolation;                   \
        WT_TXN_SHARED *txn_shared = WT_SESSION_TXN_SHARED(s);                   \
        WT_TXN_SHARED saved_txn_shared = *txn_shared;                           \
        (s)->txn->forced_iso++;                                                 \
        (s)->isolation = (s)->txn->isolation = (iso);                           \
        op;                                                                     \
        (s)->isolation = saved_iso;                                             \
        (s)->txn->isolation = saved_txn_iso;                                    \
        WT_ASSERT((s), (s)->txn->forced_iso > 0);                               \
        (s)->txn->forced_iso--;                                                 \
        WT_ASSERT((s),                                                          \
          txn_shared->id == saved_txn_shared.id &&                              \
            (txn_shared->metadata_pinned == saved_txn_shared.metadata_pinned || \
              saved_txn_shared.metadata_pinned == WT_TXN_NONE) &&               \
            (txn_shared->pinned_id == saved_txn_shared.pinned_id ||             \
              saved_txn_shared.pinned_id == WT_TXN_NONE));                      \
        txn_shared->metadata_pinned = saved_txn_shared.metadata_pinned;         \
        txn_shared->pinned_id = saved_txn_shared.pinned_id;                     \
    } while (0)

//__wt_txn_global.checkpoint_txn_shared 和 __wt_txn_global.txn_shared_list为该类型
//checkpoint_txn_shared整个结构体赋值见__checkpoint_prepare

//每个成员赋值的地方实际上在WT_WITH_TXN_ISOLATION
struct __wt_txn_shared {
    WT_CACHE_LINE_PAD_BEGIN
    //赋值参考__wt_txn_id_alloc，也就是txn_global->current-1
    //代表事务id
    //在__txn_remove_from_global_table中置为WT_TXN_NONE
    volatile uint64_t id;
    //普通事务: __txn_get_snapshot_int中对txn_global.txn_shared_list[session->id]赋值, 代表该session所能看到的最小事务id,如果获取快照时候还没有其他事务，则为txn_global->current
    //checkpoint事务: __checkpoint_prepare中对checkpoint_txn_shared.pinned_id赋值，也就是做checkpoint时候snapshot[]中的最小事务id
    //普通事务WT_ISO_READ_UNCOMMITTED方式: __wt_txn_cursor_op中对txn_global.txn_shared_list[session->id]赋值
    volatile uint64_t pinned_id;
    //赋值见__txn_get_snapshot_int中对txn_global.txn_shared_list[session->id]，记录的是checkpoint的id
    //WT_ISO_READ_UNCOMMITTED方式__wt_txn_cursor_op中对txn_global.txn_shared_list[session->id]赋值
    volatile uint64_t metadata_pinned;

    /*
     * The first commit or durable timestamp used for this transaction. Determines its position in
     * the durable queue and prevents the all_durable timestamp moving past this point.
     */
    //__wt_txn_publish_durable_timestamp中赋值
    //例如一个事务中可以多次设置commit_timestamp，也就是最早设置durable_timestamp或者commit_timestamp的时间，可以参考timestamp_abort接口
    wt_timestamp_t pinned_durable_timestamp;

    /*
     * The read timestamp used for this transaction. Determines what updates can be read and
     * prevents the oldest timestamp moving past this point.
     mongodb  7.0  WiredTigerBeginTxnBlock::setReadSnapshot->WiredTigerRecoveryUnit::setTimestamp接口调用，设置
     快照读，一条数据删了后，一定时间内可以读到数据 5.0版本 minSnapshotHistoryWindowInSeconds   
     读取历史数据 timestamp参考https://www.mongodb.com/zh-cn/docs/manual/reference/read-concern-snapshot/atClusterTime的允许值取决于minSnapshotHistoryWindowInSeconds参数。
     db.runCommand( {find: "test4", filter: {"_id" : ObjectId("6414462353e5946d312f98d0")}, readConcern: {level: "snapshot",  atClusterTime: Timestamp(1679050275, 1)}})
     
     readConcern: {level: "snapshot",  atClusterTime: Timestamp(1679050275, 1)}这里的时间就是session设置的read_timestamp

     */
    //__wt_txn_set_read_timestamp  read_timestamp必须比oldest timestamp大，否则设置不成功
    //read_timestamp会影响__wt_txn_visible事务可见性，同时会影响txn_global->pinned_timestamp(__wt_txn_get_pinned_timestamp->__txn_get_read_timestamp)
    wt_timestamp_t read_timestamp;

    volatile uint8_t is_allocating;
    WT_CACHE_LINE_PAD_END
};

//conn.txn_global为该类型
struct __wt_txn_global {
    //初始值为1，参考__wt_txn_global_init, __wt_txn_id_alloc中自增
    //txn_global->current在__txn_get_snapshot_int->__txn_sort_snapshot中会被赋值给txn->snap_max
    volatile uint64_t current; /* Current transaction ID. */

   //非WT_ISO_READ_UNCOMMITTED事务隔离级别:
    //     oldest_id: 也就是当前所有事务的snapshot[]中的快照中除了checkpoint事务id以外的快照最小值, 注意__txn_oldest_scan的oldest id没考虑checkpoint快照id, __wt_txn_oldest_id会考虑checkpoint线程对应事务id
    //     last_running: 也就是当前正在运行的除开checkpoint事务以外的id最小值(不包括checkpoint事务id)
    //     metadata_pinned: 也就是当前所有事务的snapshot[]中的事务(包括checkpoint事务id)的id最小值

   //WT_ISO_READ_UNCOMMITTED事务隔离级别:
    // oldest_id = last_running = metadata_pinned
    
    //__wt_txn_update_oldest更新的几个统计项
    /* The oldest running transaction ID (may race). */
    //__wt_txn_update_oldest中赋值
    volatile uint64_t last_running;
    /*
     * The oldest transaction ID that is not yet visible to some transaction in the system.
     */
    //初始值为1，参考__wt_txn_global_init，__wt_txn_update_oldest中赋值
    // 注意__txn_oldest_scan的oldest id没考虑checkpoint快照id, __wt_txn_oldest_id会考虑checkpoint线程对应事务id
    volatile uint64_t oldest_id;
    //__wt_txn_update_oldest中赋值, 真正生效的地方见__wt_txn_activity_check
    volatile uint64_t metadata_pinned; /* Oldest ID for metadata */



    //一般在事务提交的时候赋值，参考__wt_txn_commit(赋值为当前事务提交时候最大的commit_timestamp)，或者用户主动__wt_txn_global_set_timestamp设置  
    //回滚__rollback_to_stable赋值
    wt_timestamp_t durable_timestamp;
    //赋值参考__txn_checkpoint，也就是txn_global->checkpoint_timestamp，也就是上一次做checkpoint的时间
    //txn_global->checkpoint_timestamp只在checkpoint期间记录，checkpoint完成后会清0，last_ckpt_timestamp会保存上次做checkpoint
    //时候的timestamp, txn_global->checkpoint_timestamp表示当前正在做checkpoint的timestamp
    wt_timestamp_t last_ckpt_timestamp;
    //也就是txn_global->checkpoint_timestamp，一般就是stable_timestamp
    wt_timestamp_t meta_ckpt_timestamp;
    
    //__recovery_set_oldest_timestamp  __conn_set_timestamp->__wt_txn_global_set_timestamp
    //stable_timestamp  oldest_timestamp这几个全局timestamp通过__conn_set_timestamp接口设置，pinned_timestamp在__conn_set_timestamp中
    // 设置stable_timestamp  oldest_timestamp的时候，会自动得到pinned_timestamp
    wt_timestamp_t oldest_timestamp;
    //__wt_txn_update_pinned_timestamp中赋值
    wt_timestamp_t pinned_timestamp;
    //赋值见__recovery_set_checkpoint_timestamp
    wt_timestamp_t recovery_timestamp;
    //__wt_txn_global_set_timestamp中赋值，数据永远不会被回滚的应用程序时间
    wt_timestamp_t stable_timestamp;

    
    wt_timestamp_t version_cursor_pinned_timestamp;
    bool has_durable_timestamp;
    //__wt_txn_global_set_timestamp中置为true, has_oldest_timestamp与oldest_is_pinned状态相反
    bool has_oldest_timestamp;
    //__wt_txn_update_pinned_timestamp中置为true
    bool has_pinned_timestamp;
    //__wt_txn_global_set_timestamp中置为true, has_stable_timestamp与stable_is_pinned状态相反
    bool has_stable_timestamp;
    //__wt_txn_global_set_timestamp中置为false，has_oldest_timestamp与oldest_is_pinned状态相反
    bool oldest_is_pinned;
    //__wt_txn_global_set_timestamp中置为false，has_stable_timestamp与stable_is_pinned状态相反
    bool stable_is_pinned;

    /* Protects the active transaction states. */
    //__wt_txn_global_init中初始化
    WT_RWLOCK rwlock;

    /* Protects logging, checkpoints and transaction visibility. */
    WT_RWLOCK visibility_rwlock;

    /*
     * Track information about the running checkpoint. The transaction snapshot used when
     * checkpointing are special. Checkpoints can run for a long time so we keep them out of regular
     * visibility checks. Eviction and checkpoint operations know when they need to be aware of
     * checkpoint transactions.
     *
     * We rely on the fact that (a) the only table a checkpoint updates is the metadata; and (b)
     * once checkpoint has finished reading a table, it won't revisit it.
     */
    //__txn_checkpoint_wrapper中赋值
    volatile bool checkpoint_running;    /* Checkpoint running */
    volatile bool checkpoint_running_hs; /* Checkpoint running and processing history store file */
    //checkpoint事务id赋值见__checkpoint_prepare，
    volatile uint32_t checkpoint_id;     /* Checkpoint's session ID */
    //checkpoint_txn_shared整个结构体赋值见__checkpoint_prepare
    //代表正在做checkpoint的事务信息
    WT_TXN_SHARED checkpoint_txn_shared; /* Checkpoint's txn shared state */
    //赋值参考__checkpoint_prepare，开始checkpoint的时候赋值为txn_global->stable_timestamp，如果checkpoint完成后，在__wt_txn_release_snapshot中设置checkpoint_timestamp为WT_TS_NONE
    //真正生效的地方见__wt_txn_pinned_timestamp
    wt_timestamp_t checkpoint_timestamp; /* Checkpoint's timestamp */

    volatile uint64_t debug_ops;       /* Debug mode op counter */
    //debug_mode.rollback_error配置来模拟回滚，该配置标识没debug_rollback次operation有一次需要回滚
    uint64_t debug_rollback;           /* Debug mode rollback */


    //成员赋值参考WT_WITH_TXN_ISOLATION

    //链表上的成员数为conn->session_cnt，参考__txn_get_snapshot_int
    //数组空间分配及数组大小赋值参考__wt_txn_global_init，数组每个成员id对应所有session->id

    WT_TXN_SHARED *txn_shared_list; /* Per-session shared transaction states */
};


/*
脏读：在事物还没有提交前，修改的数据可以被其他事物所看到。
不可重复读：在一个事物中使用相同的条件查询一条数据，前后两次查询所得到的数据不同，这是因为同时其他事物对这条数据进行了修改（已提交事物），第二次查询返回了其他事物修改的数据。
幻读：在一个事物A中使用相同的条件查询了多条数据，同时其他事物添加或删除了符合事物A中查询条件的数据，这时候当事物A再次查询时候会发现数据多了或者少了，与前一次查询的结果不相同。 

不可重复读：是同一条记录（一条数据）的内容被其他事物修改了，关注的是update、delete操作一条数据的操作.
幻读：是查询某个范围（多条数据）的数据行变多或变少了，在于insert、delete的操作。


脏读: WT_ISO_READ_UNCOMMITTED
不可重复读: WT_ISO_READ_UNCOMMITTED、WT_ISO_READ_COMMITTED都可能，WT_ISO_READ_COMMITTED是因为每次读都需要重新获取snapshot
幻读: WT_ISO_READ_UNCOMMITTED、WT_ISO_READ_COMMITTED、WT_ISO_SNAPSHOT都可能，

???? WT_ISO_SNAPSHOT为什么可能有幻读，官方说是可能的，但是这里从原理上讲，因为读的时候事务开始前时候的快照，这个读事务中连续多次读，结果不可能不一样
  是不是官方的文档有误
*/

//事务隔离级别
typedef enum __wt_txn_isolation {
    //真正生效见__cursor_page_pinned
    //__wt_txn_begin->__wt_txn_get_snapshot( 注意这里只有WT_ISO_SNAPSHOT会获取快照，WT_ISO_READ_COMMITTED和WT_ISO_READ_UNCOMMITTED不会获取快照)
    //__wt_txn_cursor_op->__wt_txn_get_snapshot(这个流程WT_ISO_READ_COMMITTED和WT_ISO_SNAPSHOT会获取快照)

    //WT_ISO_READ_COMMITTED和WT_ISO_SNAPSHOT的区别是，在都显示begin_transaction的方式开始事务的时候，WT_ISO_READ_COMMITTED的__wt_txn_get_snapshot快照
    // 生成在接口操作(例如__curfile_search->__wt_txn_cursor_op->__wt_txn_get_snapshot)的时候完成，而WT_ISO_SNAPSHOT在begin_transaction(__wt_txn_begin->__wt_txn_get_snapshot)
    // 的时候生成，所以test_txn20测试时候，WT_ISO_READ_COMMITTED方式由于之前的update已提交，提交完成后update对应事务会清理掉，所以在update事务提交后WT_ISO_READ_COMMITTED
    // 方式无法获取update的快照。
    WT_ISO_READ_COMMITTED,
    //WT_ISO_READ_COMMITTED: 只有__wt_txn_cursor_op->__wt_txn_get_snapshot流程获取快照
    //WT_ISO_SNAPSHOT: __wt_txn_begin->__wt_txn_get_snapshot和__wt_txn_cursor_op->__wt_txn_get_snapshot都会获取快照
    //WT_ISO_READ_UNCOMMITTED: __wt_txn_begin->__wt_txn_get_snapshot和__wt_txn_cursor_op->__wt_txn_get_snapshot都不会获取快照
    //__txn_visible_id中也会直接返回true
    WT_ISO_READ_UNCOMMITTED,
     //真正生效见__wt_txn_begin(显示begin txn)和__wt_txn_cursor_op(隐式begin txn)，__wt_txn_get_snapshot
     // 这样在__wt_txn_visible_id_snapshot需要判断当前事务所有数据是否可见

     //WT_ISO_READ_COMMITTED: 只有__wt_txn_cursor_op->__wt_txn_get_snapshot流程获取快照
    //WT_ISO_SNAPSHOT: __wt_txn_begin->__wt_txn_get_snapshot和__wt_txn_cursor_op->__wt_txn_get_snapshot都会获取快照
    //WT_ISO_READ_UNCOMMITTED: __wt_txn_begin->__wt_txn_get_snapshot和__wt_txn_cursor_op->__wt_txn_get_snapshot都不会获取快照
    WT_ISO_SNAPSHOT
} WT_TXN_ISOLATION;

//事务类型
typedef enum __wt_txn_type {
    WT_TXN_OP_NONE = 0,
    WT_TXN_OP_BASIC_COL,
    WT_TXN_OP_BASIC_ROW,
    WT_TXN_OP_INMEM_COL,
    WT_TXN_OP_INMEM_ROW,
    WT_TXN_OP_REF_DELETE,
    WT_TXN_OP_TRUNCATE_COL,
    WT_TXN_OP_TRUNCATE_ROW
} WT_TXN_TYPE;

typedef enum {
    WT_TXN_TRUNC_ALL,
    WT_TXN_TRUNC_BOTH,
    WT_TXN_TRUNC_START,
    WT_TXN_TRUNC_STOP
} WT_TXN_TRUNC_MODE;

/*
 * WT_TXN_OP --
 *	A transactional operation.  Each transaction builds an in-memory array
 *	of these operations as it runs, then uses the array to either write log
 *	records during commit or undo the operations during rollback.
 */
//__txn_next_op中分配空间
struct __wt_txn_op {
    WT_BTREE *btree;
    //赋值参考__wt_txn_modify
    WT_TXN_TYPE type;
    union {
        /* WT_TXN_OP_BASIC_ROW, WT_TXN_OP_INMEM_ROW */
        //下面的op_upd
        struct {
            //赋值见__wt_txn_modify
            WT_UPDATE *upd;
            WT_ITEM key;
        } op_row;

        /* WT_TXN_OP_BASIC_COL, WT_TXN_OP_INMEM_COL */
        struct {
            WT_UPDATE *upd;
            uint64_t recno;
        } op_col;
/*
 * upd is pointing to same memory in both op_row and op_col, so for simplicity just chose op_row upd
 */
#undef op_upd
//赋值见__wt_txn_modify
#define op_upd op_row.upd

        /* WT_TXN_OP_REF_DELETE */
        WT_REF *ref;
        /* WT_TXN_OP_TRUNCATE_COL */
        struct {
            uint64_t start, stop;
        } truncate_col;
        /* WT_TXN_OP_TRUNCATE_ROW */
        struct {
            WT_ITEM start, stop;
            WT_TXN_TRUNC_MODE mode;
        } truncate_row;
    } u;

/* AUTOMATIC FLAG VALUE GENERATION START 0 */
//如果udp链表上面有连续的两个udp的事务id一样(也就是同一个事务连续修改了同一个K)，则会设置WT_TXN_OP_KEY_REPEATED标识 __wt_txn_prepare
#define WT_TXN_OP_KEY_REPEATED 0x1u
    /* AUTOMATIC FLAG VALUE GENERATION STOP 32 */
    uint32_t flags;
};

#define WT_TS_VERBOSE_PREFIX "unexpected timestamp usage: "

/*
 * WT_TXN --
 *	Per-session transaction context.
 __wt_txn_release中相关成员全部清理初始化
 */
struct __wt_txn {
    //同一个session的id是自增的,初始值0，session->txn->id实际上等于txn_global->current-1, 
    // session->txn->id实际上是全局递增的，这也是为什么txn_global->current初始值为1，赋值见__wt_txn_id_alloc

    //同一个session一个完整事务有一个id,该sesson下一个事务的时候，回通过__wt_txn_id_alloc从新获取一个id
    //也就是每个session->txn->id都是唯一的，并且是递增的，等于获取id时候的txn_global->current-1, 
    //注意如果事务中全是查询，则整个事务过程中id一直为0

    //事务提交的时候会在__wt_txn_commit->__wt_txn_release中置为WT_TXN_NONE 
    uint64_t id;

    //事务隔离级别 __wt_txn_begin配置，默认WT_ISO_SNAPSHOT，赋值见__wt_txn_begin
    WT_TXN_ISOLATION isolation;

    uint32_t forced_iso; /* Isolation is currently forced. */

    /*
     * Snapshot data:
     *	ids >= snap_max are invisible,
     *	ids < snap_min are visible,
     *	everything else is visible unless it is in the snapshot.
     */
    //赋值见__txn_sort_snapshot
    uint64_t snap_min, snap_max;
    //数组结构，数组大小见__wt_txn_init，实际上指向的是本结构的txn->__snapshot数组;
    //数组成员赋值见__txn_get_snapshot_int，数组中记录的是当前正在使用的不同session的事务id
    uint64_t *snapshot;
    //赋值见__txn_sort_snapshot
    uint32_t snapshot_count;
    //赋值见__wt_txn_begin，也就是conn->txn_logsync(__logmgr_sync_cfg)
    //最终在__wt_txn_log_commit中用该变量决定提交事务的时候是否需要fsyn或者flush
    //如果没使能，则__wt_txn_commit中会置txn_logsync为0
    uint32_t txn_logsync; /* Log sync configuration */

    /*
     * Timestamp copied into updates created by this transaction.
     *
     * In some use cases, this can be updated while the transaction is running.
     */
    //__wt_txn_set_commit_timestamp中赋值
    //如果只设置commit_timestamp，则commit_timestamp=durable_timestamp=commit_transaction()接口设置的时间
    //如果设置了prepare_timestamp，则commit_timestamp不能小于prepare_timestamp
    wt_timestamp_t commit_timestamp;

    /*
     * Durable timestamp copied into updates created by this transaction. It is used to decide
     * whether to consider this update to be persisted or not by stable checkpoint.
     */
    //__wt_txn_set_commit_timestamp  __wt_txn_set_durable_timestamp中赋值
    //如果只设置commit_timestamp，则commit_timestamp=durable_timestamp=commit_transaction()接口设置的时间
    wt_timestamp_t durable_timestamp;

    /*
     * Set to the first commit timestamp used in the transaction and fixed while the transaction is
     * on the public list of committed timestamps.
     */
    //__wt_txn_set_commit_timestamp中赋值
    //一个事务里面多次设置commit_timestamp，则first_commit_timestamp代表第一次设置的时间
    wt_timestamp_t first_commit_timestamp;

    /*
     * Timestamp copied into updates created by this transaction, when this transaction is prepared.
     */
    //__wt_txn_set_prepare_timestamp中赋值 
    //说明设置了"prepare_timestamp=xxxxx"，这个值必须大于oldest_timestamp
    //真正生效的地方其实在__wt_txn_prepare和__wt_txn_op_delete_apply_prepare_state，最终会影响upd->start_ts
    wt_timestamp_t prepare_timestamp;

    /*
     * Timestamps used for reading via a checkpoint cursor instead of txn_shared->read_timestamp and
     * the current oldest/pinned timestamp, respectively.
     */
    wt_timestamp_t checkpoint_read_timestamp;
    wt_timestamp_t checkpoint_oldest_timestamp;


/*
* For example, a transaction has modified [k,v] as
*  [k, v]  -> [k, u1]   (txn_op : txn_op1)
*  [k, u1] -> [k, u2]   (txn_op : txn_op2)
*  update chain : u2->u1
*  txn_mod      : txn_op1->txn_op2.
*/
    /* Array of modifications by this transaction. */
    //实际上是一个数组，数组大小为下面的mod_count，可以参考__txn_next_op
    WT_TXN_OP *mod;
    size_t mod_alloc;
    //__txn_next_op中自增，__wt_txn_unmodify中自减 
    //代表这个事务中有多少个写请求，也就是上面的mod[]数组大小
    u_int mod_count;
#ifdef HAVE_DIAGNOSTIC
    u_int prepare_count;
#endif

    /* Scratch buffer for in-memory log records. */
    //log=(enabled),默认一般enabled，因此事务会写日志
    WT_ITEM *logrec;

    /* Checkpoint status. */
    WT_LSN ckpt_lsn;
    uint32_t ckpt_nsnapshot;
    WT_ITEM *ckpt_snapshot;
    bool full_ckpt;

    /* Timeout */
    //operation_timeout_ms配置，默认0
    uint64_t operation_timeout_us;

    //回滚原因赋值见__wt_txn_rollback_required
    const char *rollback_reason; /* If rollback, the reason */

/*
 * WT_TXN_HAS_TS_COMMIT --
 *	The transaction has a set commit timestamp.
 * WT_TXN_HAS_TS_DURABLE --
 *	The transaction has an explicitly set durable timestamp (that is, it
 *	hasn't been mirrored from its commit timestamp value).
 * WT_TXN_SHARED_TS_DURABLE --
 *	The transaction has been published to the durable queue. Setting this
 *	flag lets us know that, on release, we need to mark the transaction for
 *	clearing.
 */

//如果一个写入操作__curfile_insert没有显示调用__wt_txn_begin，则会调用CURSOR_UPDATE_API_CALL_BTREE->TXN_API_CALL_NOCONF设置为WT_TXN_AUTOCOMMIT,
//  __wt_txn_autocommit_check中就会自动加上__wt_txn_begin
/* AUTOMATIC FLAG VALUE GENERATION START 0 */
#define WT_TXN_AUTOCOMMIT 0x00001u
//__wt_txn_err_set中设置,一个事务中任何一个写操作失败，都会把该次事务的session->txn的状态置位该状态
#define WT_TXN_ERROR 0x00002u
//__wt_txn_id_check中给一个事务的session生成了一个id
#define WT_TXN_HAS_ID 0x00004u
//__txn_sort_snapshot中置位，__wt_txn_release_snapshot中清理该标识
#define WT_TXN_HAS_SNAPSHOT 0x00008u
//__wt_txn_set_commit_timestamp中置位
#define WT_TXN_HAS_TS_COMMIT 0x00010u
//__wt_txn_set_durable_timestamp中置位
#define WT_TXN_HAS_TS_DURABLE 0x00020u
//__wt_txn_set_prepare_timestamp中置位
#define WT_TXN_HAS_TS_PREPARE 0x00040u
//ignore_prepare配置，默认不启用不会置位
#define WT_TXN_IGNORE_PREPARE 0x00080u
//__wt_txn_init_checkpoint_cursor配置，代表checkpoint游标，参考__curfile_check_cbt_txn
#define WT_TXN_IS_CHECKPOINT 0x00100u
//__wt_txn_prepare中置位，不会显示的清理这个标识位，只会在事务提交或者回滚的时候调用__wt_txn_release对flag清0
#define WT_TXN_PREPARE 0x00200u
#define WT_TXN_PREPARE_IGNORE_API_CHECK 0x00400u
#define WT_TXN_READONLY 0x00800u
//__wt_txn_begin中置位
#define WT_TXN_RUNNING 0x01000u
//__wt_txn_publish_durable_timestamp中置位
#define WT_TXN_SHARED_TS_DURABLE 0x02000u
//__wt_txn_set_read_timestamp中置位，表示该事务有设置read_timestamp
#define WT_TXN_SHARED_TS_READ 0x04000u
#define WT_TXN_SYNC_SET 0x08000u
#define WT_TXN_TS_NOT_SET 0x10000u
#define WT_TXN_TS_ROUND_PREPARED 0x20000u
//也就是是否启用"roundup_timestamps.read"，赋值见__wt_txn_config
#define WT_TXN_TS_ROUND_READ 0x40000u
#define WT_TXN_UPDATE 0x80000u
    /* AUTOMATIC FLAG VALUE GENERATION STOP 32 */
    //事务提交后，__wt_txn_release中直接把这个事务的所有flags清零
    uint32_t flags;

    /*
     * Zero or more bytes of value (the payload) immediately follows the WT_TXN structure. We use a
     * C99 flexible array member which has the semantics we want.
     */
    uint64_t __snapshot[];
};
