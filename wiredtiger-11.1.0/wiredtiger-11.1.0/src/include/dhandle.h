/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

/*
 * Helpers for calling a function with a data handle in session->dhandle then restoring afterwards.
 */
#define WT_WITH_DHANDLE(s, d, e)                        \
    do {                                                \
        WT_DATA_HANDLE *__saved_dhandle = (s)->dhandle; \
        (s)->dhandle = (d);                             \
        e;                                              \
        (s)->dhandle = __saved_dhandle;                 \
    } while (0)

#define WT_WITH_BTREE(s, b, e) WT_WITH_DHANDLE(s, (b)->dhandle, e)

/* Call a function without the caller's data handle, restore afterwards. */
#define WT_WITHOUT_DHANDLE(s, e) WT_WITH_DHANDLE(s, NULL, e)

/*
 * Call a function with the caller's data handle, restore it afterwards in case it is overwritten.
 */
#define WT_SAVE_DHANDLE(s, e) WT_WITH_DHANDLE(s, (s)->dhandle, e)

/* Check if a handle is inactive. */
#define WT_DHANDLE_INACTIVE(dhandle) \
    (F_ISSET(dhandle, WT_DHANDLE_DEAD) || !F_ISSET(dhandle, WT_DHANDLE_EXCLUSIVE | WT_DHANDLE_OPEN))

/* Check if a handle could be reopened. */
#define WT_DHANDLE_CAN_REOPEN(dhandle) \
    (!F_ISSET(dhandle, WT_DHANDLE_DEAD | WT_DHANDLE_DROPPED) && F_ISSET(dhandle, WT_DHANDLE_OPEN))

/* The metadata cursor's data handle. */
#define WT_SESSION_META_DHANDLE(s) (((WT_CURSOR_BTREE *)((s)->meta_cursor))->dhandle)

////__wt_cursor_cache  __session_find_shared_dhandle调用
//dhandle可以被一个session缓存起来，也可以被全局conn缓存起来
#define WT_DHANDLE_ACQUIRE(dhandle) (void)__wt_atomic_add32(&(dhandle)->session_ref, 1)

#define WT_DHANDLE_RELEASE(dhandle) (void)__wt_atomic_sub32(&(dhandle)->session_ref, 1)

#define WT_DHANDLE_NEXT(session, dhandle, head, field)                                     \
    do {                                                                                   \
        WT_ASSERT(session, FLD_ISSET(session->lock_flags, WT_SESSION_LOCKED_HANDLE_LIST)); \
        if ((dhandle) == NULL)                                                             \
            (dhandle) = TAILQ_FIRST(head);                                                 \
        else {                                                                             \
            WT_DHANDLE_RELEASE(dhandle);                                                   \
            (dhandle) = TAILQ_NEXT(dhandle, field);                                        \
        }                                                                                  \
        if ((dhandle) != NULL)                                                             \
            WT_DHANDLE_ACQUIRE(dhandle);                                                   \
    } while (0)

#define WT_DHANDLE_IS_CHECKPOINT(dhandle) ((dhandle)->checkpoint != NULL)

/*
 * WT_WITH_DHANDLE_WRITE_LOCK_NOWAIT --
 *	Try to acquire write lock for the session's current dhandle, perform an operation, drop the
 *  lock.
 */
#define WT_WITH_DHANDLE_WRITE_LOCK_NOWAIT(session, ret, op)               \
    do {                                                                  \
        if (((ret) = __wt_session_dhandle_try_writelock(session)) == 0) { \
            op;                                                           \
            __wt_session_dhandle_writeunlock(session);                    \
        }                                                                 \
    } while (0)

/*
 * WT_DATA_HANDLE --
 参考http://source.wiredtiger.com/11.1.0/arch-dhandle.html
 *	A handle for a generic named data source.
 */ //__wt_connection_impl.dhhash这个hash桶中存储该成员    //WT_SESSION_IMPL.dhandles为该类型
//__wt_conn_dhandle_alloc中分配节点内存    一个__wt_data_handle实际上对应一个BTREE，通过BTREE btree = (WT_BTREE *)dhandle->handle;获取

//__wt_table.iface接口为该类型，mongodb表table对应handle __wt_table
struct __wt_data_handle {
    WT_RWLOCK rwlock; /* Lock for shared/exclusive ops */

    //也就说uri  tabale:表名
    const char *name;         /* Object name as a URI */
    uint64_t name_hash;       /* Hash of name */
    const char *checkpoint;   /* Checkpoint name (or NULL) */
    int64_t checkpoint_order; /* Checkpoint order number, when applicable */
    //该tree对应的配置文件，赋值参考__conn_dhandle_config_set，实际上是从WiredTiger.wt文件中读取的
    const char **cfg;         /* Configuration information */
    const char *meta_base;    /* Base metadata configuration */
    size_t meta_base_length;  /* Base metadata length */
#ifdef HAVE_DIAGNOSTIC
    const char *orig_meta_base; /* Copy of the base metadata configuration */
#endif
    /*
     * Sessions holding a connection's data handle will have a non-zero reference count; sessions
     * using a connection's data handle will have a non-zero in-use count. Instances of cached
     * cursors referencing the data handle appear in session_cache_ref.
     */
    //如果一个cursor被close掉，__wt_cursor_cache中会把该cursor添加到cursor_cache[]中，同时会自增session_ref，也就是代表添加到cursor_cache中的cursor数
    //Both these counters are incremented by the session as the cursor is opened on this dhandle. session_inuse is decremented when the operation completes and the cursor is closed.
    uint32_t session_ref;          /* Sessions referencing this handle */
    //当前正在用的，cursor关闭后会自减
    //session_inuse is a count of the number of cursors opened and operating on this dhandle
    //__wt_cursor_dhandle_incr_use中自增
    int32_t session_inuse;         /* Sessions using this handle */
    uint32_t excl_ref;             /* Refs of handle by excl_session */
    //赋值见__sweep_mark
    uint64_t timeofdeath;          /* Use count went to 0 */
    //代表是否独占dhandle,dhandle加写锁后就代表独占
    WT_SESSION_IMPL *excl_session; /* Session with exclusive use, if any */

    WT_DATA_SOURCE *dsrc; /* Data source for this handle */
    //可以通过该handle找到数据源，例如BTREE btree = (WT_BTREE *)dhandle->handle;
    //btree内存分配__wt_conn_dhandle_alloc, btree的打开初始化在__wt_conn_dhandle_open->__wt_btree_open
    void *handle;         /* Generic handle */

    //参考__wt_conn_dhandle_alloc
    enum {
        WT_DHANDLE_TYPE_BTREE,  //file:
        WT_DHANDLE_TYPE_TABLE,  //table:
        WT_DHANDLE_TYPE_TIERED,
        WT_DHANDLE_TYPE_TIERED_TREE
    } type;

#define WT_DHANDLE_BTREE(dhandle) \
    ((dhandle)->type == WT_DHANDLE_TYPE_BTREE || (dhandle)->type == WT_DHANDLE_TYPE_TIERED)

    bool compact_skip; /* If the handle failed to compact */

    /*
     * Data handles can be closed without holding the schema lock; threads walk the list of open
     * handles, operating on them (checkpoint is the best example). To avoid sources disappearing
     * underneath checkpoint, lock the data handle when closing it.
     */
    WT_SPINLOCK close_lock; /* Lock to close the handle */

    /* Data-source statistics */
    WT_DSRC_STATS *stats[WT_COUNTER_SLOTS];
    WT_DSRC_STATS *stat_array;

/*
 * Flags values over 0xfff are reserved for WT_BTREE_*. This lets us combine the dhandle and btree
 * flags when we need, for example, to pass both sets in a function call.
 */
/* AUTOMATIC FLAG VALUE GENERATION START 0 */
//__wt_conn_dhandle_close
#define WT_DHANDLE_DEAD 0x001u         /* Dead, awaiting discard */
//__wt_session_lock_checkpoint
#define WT_DHANDLE_DISCARD 0x002u      /* Close on release */
#define WT_DHANDLE_DISCARD_KILL 0x004u /* Mark dead on release */
#define WT_DHANDLE_DROPPED 0x008u      /* Handle is dropped */
//如果该表的page都已经evict完了，则标记该表已经evicet完成  __evict_walk_tree中赋值  __session_dhandle_sweep中真正使用
#define WT_DHANDLE_EVICTED 0x010u      /* Btree is evicted (advisory) */
//独占访问 //__wt_curfile_open   __wt_session_lock_dhandle
//代表是否独占dhandle,dhandle加写锁后就代表独占,也就是只有当前session一个cursor在访问这个btree
#define WT_DHANDLE_EXCLUSIVE 0x020u    /* Exclusive access */ //
//也就是WT_HS_URI文件
#define WT_DHANDLE_HS 0x040u           /* History store table */
//__wt_conn_dhandle_alloc  说明对应元数据文件"file:WiredTiger.wt"
#define WT_DHANDLE_IS_METADATA 0x080u  /* Metadata handle */
#define WT_DHANDLE_LOCK_ONLY 0x100u    /* Handle only used as a lock */
//__wt_conn_dhandle_open中调用__wt_btree_open创建btree，然后赋值该状态 初始值
#define WT_DHANDLE_OPEN 0x200u         /* Handle is open */
                                       /* AUTOMATIC FLAG VALUE GENERATION STOP 12 */
    uint32_t flags;

/* AUTOMATIC FLAG VALUE GENERATION START 0 */
//默认配置assert=(commit_timestamp=none,durable_timestamp=none,read_timestamp=none,write_timestamp=off)
//  所有标识都不会置位
#define WT_DHANDLE_TS_ASSERT_READ_ALWAYS 0x1u /* Assert read always checking. */
#define WT_DHANDLE_TS_ASSERT_READ_NEVER 0x2u  /* Assert read never checking. */
#define WT_DHANDLE_TS_NEVER 0x4u              /* Handle never using timestamps checking. */
#define WT_DHANDLE_TS_ORDERED 0x8u            /* Handle using ordered timestamps checking. */
                                              /* AUTOMATIC FLAG VALUE GENERATION STOP 16 */
    //从默认配置看，这里状态会为0
    uint16_t ts_flags;

/* AUTOMATIC FLAG VALUE GENERATION START 0 */
#define WT_DHANDLE_LOCK_WRITE 0x1u /* Write lock is acquired. */
                                   /* AUTOMATIC FLAG VALUE GENERATION STOP 16 */
    uint16_t lock_flags;

    TAILQ_ENTRY(__wt_data_handle) q; //yang add  挪动了位置
    TAILQ_ENTRY(__wt_data_handle) hashq;
};
