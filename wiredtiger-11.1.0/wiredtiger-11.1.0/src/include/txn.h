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
//__wt_txn_rollback����ع�ʱ����λ��һ���������κ�һ�������쳣������д����op����Ҫ�ع���ֱ����λΪWT_TXN_ABORTED
// ������reconcile evict�������ñ�ʶ������ֱ������������__rec_upd_select
#define WT_TXN_ABORTED UINT64_MAX    /* Update rolled back */

#define WT_TS_NONE 0         /* Beginning of time */
#define WT_TS_MAX UINT64_MAX /* End of time */

/*
 * A list of reasons for returning a rollback error from the API. These reasons can be queried via
 * the session get rollback reason API call. Users of the API could have a dependency on the format
 * of these messages so changing them must be done with care.
 */
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
//��session->id��Ӧ��__wt_txn_shared
//����(s)->id����session id��txn_shared_list[(s)->id])�����session id��Ӧ������id
//WT_SESSION_TXN_SHARED��ȫ�ֹ���ģ���¼�����е�����[]����ÿ����Ա���ݴ���һ��session��������������Ҫע������Ƕ��ĵ����������ж��cursor����ʱ����ͬһ��������ʵ���ϻ��ж��session��������ĵ�������в������ο�src/test/csuit/timestamp_abort
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

//__wt_txn_global.checkpoint_txn_shared �� __wt_txn_global.txn_shared_listΪ������
//checkpoint_txn_shared�����ṹ�帳ֵ��__checkpoint_prepare

//ÿ����Ա��ֵ�ĵط�ʵ������WT_WITH_TXN_ISOLATION
struct __wt_txn_shared {
    WT_CACHE_LINE_PAD_BEGIN
    //��ֵ�ο�__wt_txn_id_alloc��Ҳ����txn_global->current-1
    //��������id
    volatile uint64_t id;
    //��ͨ����: __txn_get_snapshot_int�ж�txn_global.txn_shared_list[session->id]��ֵ, �����session���ܿ�������С����id,�����ȡ����ʱ��û������������Ϊtxn_global->current
    //checkpoint����: __checkpoint_prepare�ж�checkpoint_txn_shared.pinned_id��ֵ
    //��ͨ����WT_ISO_READ_UNCOMMITTED��ʽ: __wt_txn_cursor_op�ж�txn_global.txn_shared_list[session->id]��ֵ
    volatile uint64_t pinned_id;
    //��ֵ��__txn_get_snapshot_int�ж�txn_global.txn_shared_list[session->id]����¼����checkpoint��id
    //WT_ISO_READ_UNCOMMITTED��ʽ__wt_txn_cursor_op�ж�txn_global.txn_shared_list[session->id]��ֵ
    volatile uint64_t metadata_pinned;

    /*
     * The first commit or durable timestamp used for this transaction. Determines its position in
     * the durable queue and prevents the all_durable timestamp moving past this point.
     */
    //__wt_txn_publish_durable_timestamp�и�ֵ
    //Ҳ������������durable_timestamp����commit_timestamp��ʱ��
    wt_timestamp_t pinned_durable_timestamp;

    /*
     * The read timestamp used for this transaction. Determines what updates can be read and
     * prevents the oldest timestamp moving past this point.
     */
    //__wt_txn_set_read_timestamp
    wt_timestamp_t read_timestamp;

    volatile uint8_t is_allocating;
    WT_CACHE_LINE_PAD_END
};

//conn.txn_globalΪ������
struct __wt_txn_global {
    //��ʼֵΪ1���ο�__wt_txn_global_init, __wt_txn_id_alloc������
    //txn_global->current��__txn_get_snapshot_int->__txn_sort_snapshot�лᱻ��ֵ��txn->snap_max
    volatile uint64_t current; /* Current transaction ID. */

    /* The oldest running transaction ID (may race). */
    //__wt_txn_update_oldest�и�ֵ
    volatile uint64_t last_running;

    /*
     * The oldest transaction ID that is not yet visible to some transaction in the system.
     */
    //��ʼֵΪ1���ο�__wt_txn_global_init��__wt_txn_update_oldest�и�ֵ
    volatile uint64_t oldest_id;

    //һ���������ύ��ʱ��ֵ���ο�__wt_txn_commit�������û�����__wt_txn_global_set_timestamp����  
    //�ع�__rollback_to_stable��ֵ
    wt_timestamp_t durable_timestamp;
    wt_timestamp_t last_ckpt_timestamp;
    wt_timestamp_t meta_ckpt_timestamp;
    //__recovery_set_oldest_timestamp  __conn_set_timestamp->__wt_txn_global_set_timestamp
    wt_timestamp_t oldest_timestamp;
    //__wt_txn_update_pinned_timestamp�и�ֵ
    wt_timestamp_t pinned_timestamp;
    wt_timestamp_t recovery_timestamp;
    //__recovery_txn_setup_initial_state
    wt_timestamp_t stable_timestamp;
    wt_timestamp_t version_cursor_pinned_timestamp;
    bool has_durable_timestamp;
    //__wt_txn_global_set_timestamp����Ϊtrue, has_oldest_timestamp��oldest_is_pinned״̬�෴
    bool has_oldest_timestamp;
    //__wt_txn_update_pinned_timestamp����Ϊtrue
    bool has_pinned_timestamp;
    //__wt_txn_global_set_timestamp����Ϊtrue, has_stable_timestamp��stable_is_pinned״̬�෴
    bool has_stable_timestamp;
    //__wt_txn_global_set_timestamp����Ϊfalse��has_oldest_timestamp��oldest_is_pinned״̬�෴
    bool oldest_is_pinned;
    //__wt_txn_global_set_timestamp����Ϊfalse��has_stable_timestamp��stable_is_pinned״̬�෴
    bool stable_is_pinned;

    /* Protects the active transaction states. */
    //__wt_txn_global_init�г�ʼ��
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
    //__txn_checkpoint_wrapper�и�ֵ
    volatile bool checkpoint_running;    /* Checkpoint running */
    volatile bool checkpoint_running_hs; /* Checkpoint running and processing history store file */
    //checkpoint����id��ֵ��__checkpoint_prepare��
    volatile uint32_t checkpoint_id;     /* Checkpoint's session ID */
    //checkpoint_txn_shared�����ṹ�帳ֵ��__checkpoint_prepare
    //����������checkpoint��������Ϣ
    WT_TXN_SHARED checkpoint_txn_shared; /* Checkpoint's txn shared state */
    wt_timestamp_t checkpoint_timestamp; /* Checkpoint's timestamp */

    volatile uint64_t debug_ops;       /* Debug mode op counter */
    //debug_mode.rollback_error������ģ��ع��������ñ�ʶûdebug_rollback��operation��һ����Ҫ�ع�
    uint64_t debug_rollback;           /* Debug mode rollback */
    //__wt_txn_update_oldest�и�ֵ
    volatile uint64_t metadata_pinned; /* Oldest ID for metadata */

    //��Ա��ֵ�ο�WT_WITH_TXN_ISOLATION

    //�����ϵĳ�Ա��Ϊconn->session_cnt���ο�__txn_get_snapshot_int
    //����ռ���估�����С��ֵ�ο�__wt_txn_global_init������ÿ����Աid��Ӧ����session->id

    WT_TXN_SHARED *txn_shared_list; /* Per-session shared transaction states */
};

//������뼶��
typedef enum __wt_txn_isolation {
    //������Ч��__cursor_page_pinned
    //__wt_txn_begin->__wt_txn_get_snapshot( ע������ֻ��WT_ISO_SNAPSHOT���ȡ���գ�WT_ISO_READ_COMMITTED��WT_ISO_READ_UNCOMMITTED�����ȡ����)
    //__wt_txn_cursor_op->__wt_txn_get_snapshot(�������WT_ISO_READ_COMMITTED��WT_ISO_SNAPSHOT���ȡ����)

    //WT_ISO_READ_COMMITTED��WT_ISO_SNAPSHOT�������ǣ��ڶ���ʾbegin_transaction�ķ�ʽ��ʼ�����ʱ��WT_ISO_READ_COMMITTED��__wt_txn_get_snapshot����
    // �����ڽӿڲ���(����__curfile_search->__wt_txn_cursor_op->__wt_txn_get_snapshot)��ʱ����ɣ���WT_ISO_SNAPSHOT��begin_transaction(__wt_txn_begin->__wt_txn_get_snapshot)
    // ��ʱ�����ɣ�����test_txn20����ʱ��WT_ISO_READ_COMMITTED��ʽ����֮ǰ��update���ύ���ύ��ɺ�update��Ӧ������������������update�����ύ��WT_ISO_READ_COMMITTED
    // ��ʽ�޷���ȡupdate�Ŀ��ա�
    WT_ISO_READ_COMMITTED,
    //WT_ISO_READ_COMMITTED: ֻ��__wt_txn_cursor_op->__wt_txn_get_snapshot���̻�ȡ����
    //WT_ISO_SNAPSHOT: __wt_txn_begin->__wt_txn_get_snapshot��__wt_txn_cursor_op->__wt_txn_get_snapshot�����ȡ����
    //WT_ISO_READ_UNCOMMITTED: __wt_txn_begin->__wt_txn_get_snapshot��__wt_txn_cursor_op->__wt_txn_get_snapshot�������ȡ����
    //__txn_visible_id��Ҳ��ֱ�ӷ���true
    WT_ISO_READ_UNCOMMITTED,
     //������Ч��__wt_txn_begin(��ʾbegin txn)��__wt_txn_cursor_op(��ʽbegin txn)��__wt_txn_get_snapshot
     // ������__wt_txn_visible_id_snapshot��Ҫ�жϵ�ǰ�������������Ƿ�ɼ�

     //WT_ISO_READ_COMMITTED: ֻ��__wt_txn_cursor_op->__wt_txn_get_snapshot���̻�ȡ����
    //WT_ISO_SNAPSHOT: __wt_txn_begin->__wt_txn_get_snapshot��__wt_txn_cursor_op->__wt_txn_get_snapshot�����ȡ����
    //WT_ISO_READ_UNCOMMITTED: __wt_txn_begin->__wt_txn_get_snapshot��__wt_txn_cursor_op->__wt_txn_get_snapshot�������ȡ����
    WT_ISO_SNAPSHOT
} WT_TXN_ISOLATION;

//��������
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
//__txn_next_op�з���ռ�
struct __wt_txn_op {
    WT_BTREE *btree;
    //��ֵ�ο�__wt_txn_modify
    WT_TXN_TYPE type;
    union {
        /* WT_TXN_OP_BASIC_ROW, WT_TXN_OP_INMEM_ROW */
        //�����op_upd
        struct {
            //��ֵ��__wt_txn_modify
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
//��ֵ��__wt_txn_modify
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
#define WT_TXN_OP_KEY_REPEATED 0x1u
    /* AUTOMATIC FLAG VALUE GENERATION STOP 32 */
    uint32_t flags;
};

#define WT_TS_VERBOSE_PREFIX "unexpected timestamp usage: "

/*
 * WT_TXN --
 *	Per-session transaction context.
 __wt_txn_release����س�Աȫ�������ʼ��
 */
struct __wt_txn {
    //ͬһ��session��id��������,��ʼֵ0��session->txn->idʵ���ϵ���txn_global->current-1, 
    // session->txn->idʵ������ȫ�ֵ����ģ���Ҳ��Ϊʲôtxn_global->current��ʼֵΪ1����ֵ��__wt_txn_id_alloc

    //ͬһ��sessionһ������������һ��id,��sesson��һ�������ʱ�򣬻�ͨ��__wt_txn_id_alloc���»�ȡһ��id
    //Ҳ����ÿ��session->txn->id����Ψһ�ģ������ǵ����ģ����ڻ�ȡidʱ���txn_global->current-1, 
    //ע�����������ȫ�ǲ�ѯ�����������������idһֱΪ0

    //�����ύ��ʱ�����__wt_txn_commit->__wt_txn_release����ΪWT_TXN_NONE 
    uint64_t id;

    //������뼶�� __wt_txn_begin���ã�Ĭ��WT_ISO_SNAPSHOT����ֵ��__wt_txn_begin
    WT_TXN_ISOLATION isolation;

    uint32_t forced_iso; /* Isolation is currently forced. */

    /*
     * Snapshot data:
     *	ids >= snap_max are invisible,
     *	ids < snap_min are visible,
     *	everything else is visible unless it is in the snapshot.
     */
    //��ֵ��__txn_sort_snapshot
    uint64_t snap_min, snap_max;
    //����ṹ�������С��__wt_txn_init��ʵ����ָ����Ǳ��ṹ��txn->__snapshot����;
    //�����Ա��ֵ��__txn_get_snapshot_int�������м�¼���ǵ�ǰ����ʹ�õĲ�ͬsession������id
    uint64_t *snapshot;
    //��ֵ��__txn_sort_snapshot
    uint32_t snapshot_count;
    //��ֵ��__wt_txn_begin��Ҳ����conn->txn_logsync(__logmgr_sync_cfg)
    //������__wt_txn_log_commit���øñ��������ύ�����ʱ���Ƿ���Ҫfsyn����flush
    //���ûʹ�ܣ���__wt_txn_commit�л���txn_logsyncΪ0
    uint32_t txn_logsync; /* Log sync configuration */

    /*
     * Timestamp copied into updates created by this transaction.
     *
     * In some use cases, this can be updated while the transaction is running.
     */
    //__wt_txn_set_commit_timestamp�и�ֵ
    wt_timestamp_t commit_timestamp;

    /*
     * Durable timestamp copied into updates created by this transaction. It is used to decide
     * whether to consider this update to be persisted or not by stable checkpoint.
     */
    //__wt_txn_set_commit_timestamp  __wt_txn_set_durable_timestamp�и�ֵ
    wt_timestamp_t durable_timestamp;

    /*
     * Set to the first commit timestamp used in the transaction and fixed while the transaction is
     * on the public list of committed timestamps.
     */
    //__wt_txn_set_commit_timestamp�и�ֵ
    //һ����������������commit_timestamp����first_commit_timestamp�����һ�����õ�ʱ��
    wt_timestamp_t first_commit_timestamp;

    /*
     * Timestamp copied into updates created by this transaction, when this transaction is prepared.
     */
    //__wt_txn_set_prepare_timestamp�и�ֵ
    wt_timestamp_t prepare_timestamp;

    /*
     * Timestamps used for reading via a checkpoint cursor instead of txn_shared->read_timestamp and
     * the current oldest/pinned timestamp, respectively.
     */
    wt_timestamp_t checkpoint_read_timestamp;
    wt_timestamp_t checkpoint_oldest_timestamp;

    /* Array of modifications by this transaction. */
    //ʵ������һ�����飬�����СΪ�����mod_count�����Բο�__txn_next_op
    WT_TXN_OP *mod;
    size_t mod_alloc;
    //__txn_next_op��������__wt_txn_unmodify���Լ� 
    //��������������ж��ٸ�д����Ҳ���������mod[]�����С
    u_int mod_count;
#ifdef HAVE_DIAGNOSTIC
    u_int prepare_count;
#endif

    /* Scratch buffer for in-memory log records. */
    //log=(enabled),Ĭ��һ��enabled����������д��־
    WT_ITEM *logrec;

    /* Checkpoint status. */
    WT_LSN ckpt_lsn;
    uint32_t ckpt_nsnapshot;
    WT_ITEM *ckpt_snapshot;
    bool full_ckpt;

    /* Timeout */
    //operation_timeout_ms���ã�Ĭ��0
    uint64_t operation_timeout_us;

    //�ع�ԭ��ֵ��__wt_txn_rollback_required
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

//���һ��д�����__curfile_insertû����ʾ����__wt_txn_begin��������CURSOR_UPDATE_API_CALL_BTREE->TXN_API_CALL_NOCONF����ΪWT_TXN_AUTOCOMMIT,
//  __wt_txn_autocommit_check�оͻ��Զ�����__wt_txn_begin
/* AUTOMATIC FLAG VALUE GENERATION START 0 */
#define WT_TXN_AUTOCOMMIT 0x00001u
//__wt_txn_err_set������,һ���������κ�һ��д����ʧ�ܣ�����Ѹô������session->txn��״̬��λ��״̬
#define WT_TXN_ERROR 0x00002u
//__wt_txn_id_check�и�һ�������session������һ��id
#define WT_TXN_HAS_ID 0x00004u
//__txn_sort_snapshot����λ��__wt_txn_release_snapshot������ñ�ʶ
#define WT_TXN_HAS_SNAPSHOT 0x00008u
//__wt_txn_set_commit_timestamp����λ
#define WT_TXN_HAS_TS_COMMIT 0x00010u
//__wt_txn_set_durable_timestamp����λ
#define WT_TXN_HAS_TS_DURABLE 0x00020u
//__wt_txn_set_prepare_timestamp����λ
#define WT_TXN_HAS_TS_PREPARE 0x00040u
//ignore_prepare���ã�Ĭ�ϲ����ò�����λ
#define WT_TXN_IGNORE_PREPARE 0x00080u
//__wt_txn_init_checkpoint_cursor���ã�����checkpoint�α꣬�ο�__curfile_check_cbt_txn
#define WT_TXN_IS_CHECKPOINT 0x00100u
//__wt_txn_prepare����λ
#define WT_TXN_PREPARE 0x00200u
#define WT_TXN_PREPARE_IGNORE_API_CHECK 0x00400u
#define WT_TXN_READONLY 0x00800u
//__wt_txn_begin����λ
#define WT_TXN_RUNNING 0x01000u
//__wt_txn_publish_durable_timestamp����λ
#define WT_TXN_SHARED_TS_DURABLE 0x02000u
//__wt_txn_set_read_timestamp����λ
#define WT_TXN_SHARED_TS_READ 0x04000u
#define WT_TXN_SYNC_SET 0x08000u
#define WT_TXN_TS_NOT_SET 0x10000u
#define WT_TXN_TS_ROUND_PREPARED 0x20000u
#define WT_TXN_TS_ROUND_READ 0x40000u
#define WT_TXN_UPDATE 0x80000u
    /* AUTOMATIC FLAG VALUE GENERATION STOP 32 */
    uint32_t flags;

    /*
     * Zero or more bytes of value (the payload) immediately follows the WT_TXN structure. We use a
     * C99 flexible array member which has the semantics we want.
     */
    uint64_t __snapshot[];
};
