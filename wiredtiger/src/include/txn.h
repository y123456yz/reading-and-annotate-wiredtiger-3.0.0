/*-
 * Copyright (c) 2014-2017 MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */
//事务ID范围默认在WT_TXN_NONE和WT_TXN_ABORTED之间，特殊情况就是下面这几个值
//表示该事务是已提交的，已结束，注意:为该状态的session,有可能是在做checkpoint操作，参考__checkpoint_prepare
#define	WT_TXN_NONE	0		/* No txn running in a session. */
//初始状态
#define	WT_TXN_FIRST	1		/* First transaction to run. */
//表示该事务是回滚了的
#define	WT_TXN_ABORTED	UINT64_MAX	/* Update rolled back, ignore. */

/*
 * Transaction ID comparison dealing with edge cases.
 *
 * WT_TXN_ABORTED is the largest possible ID (never visible to a running
 * transaction), WT_TXN_NONE is smaller than any possible ID (visible to all
 * running transactions).
 */
#define	WT_TXNID_LE(t1, t2)						\
	((t1) <= (t2))

#define	WT_TXNID_LT(t1, t2)						\
	((t1) < (t2))

//当前session正在处理的事务的状态信息
#define	WT_SESSION_TXN_STATE(s) (&S2C(s)->txn_global.states[(s)->id])

#define	WT_SESSION_IS_CHECKPOINT(s)					\
	((s)->id != 0 && (s)->id == S2C(s)->txn_global.checkpoint_id)

/*
 * Perform an operation at the specified isolation level.
 *
 * This is fiddly: we can't cope with operations that begin transactions
 * (leaving an ID allocated), and operations must not move our published
 * snap_min forwards (or updates we need could be freed while this operation is
 * in progress).  Check for those cases: the bugs they cause are hard to debug.
 */
#define	WT_WITH_TXN_ISOLATION(s, iso, op) do {				\
	WT_TXN_ISOLATION saved_iso = (s)->isolation;		        \
	WT_TXN_ISOLATION saved_txn_iso = (s)->txn.isolation;		\
	WT_TXN_STATE *txn_state = WT_SESSION_TXN_STATE(s);		\
	WT_TXN_STATE saved_state = *txn_state;				\
	(s)->txn.forced_iso++;						\
	(s)->isolation = (s)->txn.isolation = (iso);			\
	op;								\
	(s)->isolation = saved_iso;					\
	(s)->txn.isolation = saved_txn_iso;				\
	WT_ASSERT((s), (s)->txn.forced_iso > 0);                        \
	(s)->txn.forced_iso--;						\
	WT_ASSERT((s), txn_state->id == saved_state.id &&		\
	    (txn_state->metadata_pinned == saved_state.metadata_pinned ||\
	    saved_state.metadata_pinned == WT_TXN_NONE) &&		\
	    (txn_state->pinned_id == saved_state.pinned_id ||		\
	    saved_state.pinned_id == WT_TXN_NONE));			\
	txn_state->metadata_pinned = saved_state.metadata_pinned;	\
	txn_state->pinned_id = saved_state.pinned_id;			\
} while (0)

//有名快照，参考__wt_txn_named_snapshot_begin
struct __wt_named_snapshot {//该结构对应的数据最终存入到__wt_txn_global.snapshot队列
	const char *name;

	TAILQ_ENTRY(__wt_named_snapshot) q;

	uint64_t id, pinned_id, snap_min, snap_max;
	uint64_t *snapshot;
	uint32_t snapshot_count;
};

//__wt_txn_global.states  记录各个session的事务id信息
//通过WT_SESSION_TXN_STATE取值
struct __wt_txn_state {
	WT_CACHE_LINE_PAD_BEGIN
	volatile uint64_t id; /* 执行事务的事务ID，赋值见__wt_txn_id_alloc */
	//比txn_global->current小的最小id，也就是离oldest id最接近的未提交事务id
	volatile uint64_t pinned_id; //赋值见__wt_txn_get_snapshot
	//表示当前session正在做checkpoint操作，也就是当前session做checkpoint的id，
	//注意:为该状态的session,有可能是在做checkpoint操作，参考__checkpoint_prepare
	volatile uint64_t metadata_pinned; //赋值见__wt_txn_get_snapshot 

	WT_CACHE_LINE_PAD_END
};

//一个conn中包含多个session,每个session有一个对应的事务txn信息
//该结构用于全局事务管理，__wt_connection_impl.txn_global  该全局锁针对整个conn
struct __wt_txn_global {
    // 全局写事务ID产生种子,一直递增  __wt_txn_id_alloc总自增 
	volatile uint64_t current;	/* Current transaction ID. */

	/* The oldest running transaction ID (may race). */
	
	volatile uint64_t last_running;
	/*
	 * The oldest transaction ID that is not yet visible to some
	 * transaction in the system.
	 */ //系统中最早产生且还在执行(也就是还未提交)的写事务ID，赋值见__wt_txn_update_oldest
	 //未提交事务中最小的一个事务id，只有小于该值的id事务才是可见的，见__txn_visible_all_id
	volatile uint64_t oldest_id; //赋值见__wt_txn_update_oldest

    /* timestamp相关的赋值见__wt_txn_global_set_timestamp __wt_txn_commit*/
	//WT_DECL_TIMESTAMP(commit_timestamp)
	//实际上是所有session对应事务中commit_timestamp最大的，见__wt_txn_commit
	//生效判断在__wt_txn_update_pinned_timestamp->__txn_global_query_timestamp，实际上是通过影响pinned_timestamp(__wt_txn_visible_all)来影响可见性的
	wt_timestamp_t commit_timestamp;
	/*
    WiredTiger 提供设置 oldest timestamp 的功能，允许由 MongoDB 来设置该时间戳，含义是Read as of a timestamp 
    不会提供更小的时间戳来进行一致性读，也就是说，WiredTiger 无需维护 oldest timestamp 之前的所有历史版本。
    MongoDB 层需要频繁（及时）更新 oldest timestamp，避免让 WT cache 压力太大。
	*/
	//WT_DECL_TIMESTAMP(oldest_timestamp)
	//WT_DECL_TIMESTAMP(pinned_timestamp)
	/*
	例如有多个线程，每个线程的session在调用该函数进行oldest_timestamp设置，则txn_global->oldest_timestamp
	是这些设置中的最大值，参考__wt_txn_global_set_timestamp
	*/ //设置检查见__wt_timestamp_validate  
	//生效判断在__wt_txn_update_pinned_timestamp->__txn_global_query_timestamp，实际上是通过影响pinned_timestamp(__wt_txn_visible_all)来影响可见性的
	wt_timestamp_t oldest_timestamp; //举例使用可以参考thread_ts_run
	//生效见__wt_txn_visible_all
	wt_timestamp_t pinned_timestamp; //赋值见__wt_txn_update_pinned_timestamp
	/*
    4.0 版本实现了存储引擎层的回滚机制，当复制集节点需要回滚时，直接调用 WiredTiger 接口，将数据回滚到
    某个稳定版本（实际上就是一个 Checkpoint），这个稳定版本则依赖于 stable timestamp。WiredTiger 会确保 
    stable timestamp 之后的数据不会写到 Checkpoint里，MongoDB 根据复制集的同步状态，当数据已经同步到大多
    数节点时（Majority commited），会更新 stable timestamp，因为这些数据已经提交到大多数节点了，一定不
    会发生 ROLLBACK，这个时间戳之前的数据就都可以写到 Checkpoint 里了。
	*/
	//WT_DECL_TIMESTAMP(stable_timestamp) //举例使用可以参考thread_ts_run
	/*
	例如有多个线程，每个线程的session在调用该函数进行stable_timestamp设置，则txn_global->stable_timestamp
	是这些设置中的最大值，参考__wt_txn_global_set_timestamp
	*/ 
	//赋值检查见__wt_timestamp_validate
	//生效判断在__wt_txn_update_pinned_timestamp->__txn_global_query_timestamp，实际上是通过影响pinned_timestamp(__wt_txn_visible_all)来影响可见性的
	wt_timestamp_t stable_timestamp; //赋值通过mongodb调用__conn_set_timestamp->__wt_txn_global_set_timestamp实现
	bool has_commit_timestamp;
	bool has_oldest_timestamp;
	bool has_pinned_timestamp; //为true表示pinned_timestamp等于oldest_timestamp，见__wt_txn_update_pinned_timestamp
	bool has_stable_timestamp;
	bool oldest_is_pinned; //pinned_timestamp就是oldest_timestamp
	bool stable_is_pinned;

	WT_SPINLOCK id_lock;

	/* Protects the active transaction states. */
	WT_RWLOCK rwlock;

	/* Protects logging, checkpoints and transaction visibility. */
	WT_RWLOCK visibility_rwlock;

	/* List of transactions sorted by commit timestamp. */
	WT_RWLOCK commit_timestamp_rwlock;
	//时间撮相关的事务都会添加到该链表中，见__wt_txn_set_commit_timestamp
	TAILQ_HEAD(__wt_txn_cts_qh, __wt_txn) commit_timestamph;
	uint32_t commit_timestampq_len; //commit_timestamph队列的成员长度

	/* List of transactions sorted by read timestamp. */
	WT_RWLOCK read_timestamp_rwlock;
	TAILQ_HEAD(__wt_txn_rts_qh, __wt_txn) read_timestamph;
	uint32_t read_timestampq_len;

	/*
	 * Track information about the running checkpoint. The transaction
	 * snapshot used when checkpointing are special. Checkpoints can run
	 * for a long time so we keep them out of regular visibility checks.
	 * Eviction and checkpoint operations know when they need to be aware
	 * of checkpoint transactions.
	 *
	 * We rely on the fact that (a) the only table a checkpoint updates is
	 * the metadata; and (b) once checkpoint has finished reading a table,
	 * it won't revisit it.
	 */
	//说明在做checkpoint过程中，见__txn_checkpoint_wrapper
	volatile bool	  checkpoint_running;	/* Checkpoint running */
	//做checkpoint时候的session对应的id，赋值见__checkpoint_prepare
	volatile uint32_t checkpoint_id;	/* Checkpoint's session ID */
	//做checkpoint所在session的state，赋值见__checkpoint_prepare   生效见__wt_txn_oldest_id
	WT_TXN_STATE	  checkpoint_state;	/* Checkpoint's txn state */
	//做checkpoint所在session的txn，赋值见__checkpoint_prepare
	WT_TXN           *checkpoint_txn;	/* Checkpoint's txn structure */

	volatile uint64_t metadata_pinned;	/* Oldest ID for metadata */

	/* Named snapshot state. */
	WT_RWLOCK nsnap_rwlock;
	volatile uint64_t nsnap_oldest_id;
	TAILQ_HEAD(__wt_nsnap_qh, __wt_named_snapshot) nsnaph;

    //数组，不同session的WT_TXN_STATE记录到该数组对应位置，见WT_SESSION_TXN_STATE
    //存储了所有session的事务id信息，参考__wt_txn_am_oldest
    //程序一起来就会分陪空间，见__wt_txn_global_init，每个session对应的WT_TXN_STATE的赋值在__wt_txn_get_snapshot
	WT_TXN_STATE *states;		/* Per-session transaction states */ 
};

//可以参考https://blog.csdn.net/yuanrxdu/article/details/78339295
/*
可以结合 MongoDB新存储引擎WiredTiger实现(事务篇) https://www.jianshu.com/p/f053e70f9b18参考事务隔离级别
*/
/* wiredtiger 事务隔离类型，生效见__wt_txn_visible->__txn_visible_id */
typedef enum __wt_txn_isolation { //赋值见__wt_txn_config
	WT_ISO_READ_COMMITTED,
	WT_ISO_READ_UNCOMMITTED,
	WT_ISO_SNAPSHOT
} WT_TXN_ISOLATION;

/*
 * WT_TXN_OP --
 *	A transactional operation.  Each transaction builds an in-memory array
 *	of these operations as it runs, then uses the array to either write log
 *	records during commit or undo the operations during rollback.
 */ //赋值见__wt_txn_modify  用于记录各种操作
 //WT_TXN和__wt_txn_op在__txn_next_op中关联起来    __wt_txn.mod数组成员
struct __wt_txn_op {
	uint32_t fileid; //赋值见__txn_next_op  对应btree id
	enum {
		WT_TXN_OP_BASIC,
		WT_TXN_OP_BASIC_TS,
		WT_TXN_OP_INMEM,
		WT_TXN_OP_REF,
		WT_TXN_OP_TRUNCATE_COL,
		WT_TXN_OP_TRUNCATE_ROW
	} type; //赋值见__wt_txn_modify
	union { 
		/* WT_TXN_OP_BASIC, WT_TXN_OP_INMEM */
		WT_UPDATE *upd;
		/* WT_TXN_OP_REF */
		WT_REF *ref;
		/* WT_TXN_OP_TRUNCATE_COL */
		struct {
			uint64_t start, stop;
		} truncate_col;
		/* WT_TXN_OP_TRUNCATE_ROW */
		struct {
			WT_ITEM start, stop;
			enum {
				WT_TXN_TRUNC_ALL,
				WT_TXN_TRUNC_BOTH,
				WT_TXN_TRUNC_START,
				WT_TXN_TRUNC_STOP
			} mode;
		} truncate_row;
	} u; //和每个key的update链关联起来，见__wt_txn_modify
};

/*
 * WT_TXN --
 *	Per-session transaction context.
 */
//WT_SESSION_IMPL.txn成员为该类型
//WT_TXN和__wt_txn_op在__txn_next_op中关联起来
struct __wt_txn {//WT_SESSION_IMPL.txn成员，每个session都有对应的txn
    //本次事务的全局唯一的ID，用于标示事务修改数据的版本号，也就是多个session都会有不同的id，每个事务有一个非重复id，见__wt_txn_id_alloc
	uint64_t id; /*事务ID*/ //赋值见__wt_txn_id_alloc

    //生效见__txn_visible_id */
	WT_TXN_ISOLATION isolation; /*隔离级别*/ //赋值见__wt_txn_config

	uint32_t forced_iso;	/* Isolation is currently forced. */

	/*
	 * Snapshot data:
	 *	ids < snap_min are visible,
	 *	ids > snap_max are invisible,
	 *	everything else is visible unless it is in the snapshot.
	 */ //这个范围内的事务表示当前系统中正在操作的事务，参考https://blog.csdn.net/yuanrxdu/article/details/78339295
	uint64_t snap_min, snap_max;
	//系统事务对象数组，保存系统中所有的事务对象,保存的是正在执行事务的区间的事务对象序列
	//当前事务开始或者操作时刻其他正在执行且并未提交的事务集合,用于事务隔离
	uint64_t *snapshot; //snapshot数组，对应__wt_txn_init   数组内容默认在__txn_sort_snapshot中按照id排序
	uint32_t snapshot_count; //txn->snapshot数组中有多少个成员
	//生效见__wt_txn_log_commit  //来源在__logmgr_sync_cfg中配置解析  赋值见__wt_txn_begin
	uint32_t txn_logsync;	/* Log sync configuration */

	/*
	 * Timestamp copied into updates created by this transaction.
	 *
	 * In some use cases, this can be updated while the transaction is
	 * running.
	 */
	//WT_DECL_TIMESTAMP(commit_timestamp)
/*
真正生效是在__wt_txn_commit中影响全局txn_global->commit_timestamp，
以及在__wt_txn_set_commit_timestamp中影响全局队列txn_global->commit_timestamph
最终因为影响全局commit_timestamp和commit_timestamph从而影响__wt_txn_update_pinned_timestamp->
__txn_global_query_timestamp，实际上是通过影响pinned_timestamp(__wt_txn_visible_all)来影响可见性的
*/
	wt_timestamp_t commit_timestamp;  

	/*
	 * Set to the first commit timestamp used in the transaction and fixed
	 * while the transaction is on the public list of committed timestamps.
	 */
	//WT_DECL_TIMESTAMP(first_commit_timestamp)
	// __wt_txn_set_commit_timestamp
	//把本次session的上一次操作的commit_timestamp保存到first_commit_timestamp
	wt_timestamp_t first_commit_timestamp; //有效性检查见__wt_timestamp_validate

	/* Read updates committed as of this timestamp. */
	//生效参考__wt_txn_visible，赋值见__wt_txn_config
	//WT_DECL_TIMESTAMP(read_timestamp)
	//生效判断在__wt_txn_visible
	wt_timestamp_t read_timestamp;  
	TAILQ_ENTRY(__wt_txn) commit_timestampq;
	TAILQ_ENTRY(__wt_txn) read_timestampq;

	/* Array of modifications by this transaction. */
	//一次事务操作里面包含的具体内容通过mod数组存储
	//见__wt_txn_log_op   赋值见__txn_next_op中分配WT_TXN_OP
	 //WT_TXN和__wt_txn_op在__txn_next_op中关联起来
	 //本次事务中已执行的操作列表,用于事务回滚。
	 //在内存中的update结构信息，就是存入该数组对应成员中的
	WT_TXN_OP      *mod;  /* 该mod数组记录了本session对应的事务的所有写操作信息*/  
	
	size_t		mod_alloc; //__txn_next_op
	u_int		mod_count; //见__txn_next_op

	/* Scratch buffer for in-memory log records. */
	//赋值见__txn_logrec_init    https://blog.csdn.net/yuanrxdu/article/details/78339295
	//KV的各种插入  更新 删除操作都会获取一个op，然后在__txn_op_log中把op格式化为指定格式数据后
	//存入logrec中，然后通过__wt_txn_log_commit....__wt_log_fill把op这里面的内容拷贝到slot buffer,
	//然后在__wt_txn_commit->__wt_txn_release->__wt_logrec_free中释放掉该空间(用于重用)，下次新的OP操作
	//会为logrec获取一个新的item，重复该过程。
	WT_ITEM	       *logrec;

	/* Requested notification when transactions are resolved. */
	WT_TXN_NOTIFY *notify;

	/* Checkpoint status. */
	WT_LSN		ckpt_lsn;
	uint32_t	ckpt_nsnapshot;
	WT_ITEM		*ckpt_snapshot;
	bool		full_ckpt;

    //下面的WT_TXN_AUTOCOMMIT等
	uint32_t flags;
};

//上面的__wt_txn.flags中用到
/* txn flags的值类型 */
#define	WT_TXN_AUTOCOMMIT	0x00001
#define	WT_TXN_ERROR		0x00002
#define	WT_TXN_HAS_ID		0x00004
//获取当前系统事务快照后在__wt_txn_get_snapshot->__txn_sort_snapshot中置位，在__wt_txn_release_snapshot中清楚标记
//__txn_sort_snapshot  __wt_txn_named_snapshot_get中置位，__wt_txn_release_snapshot中清除
#define	WT_TXN_HAS_SNAPSHOT	0x00008
//__wt_txn_set_commit_timestamp中置位
#define	WT_TXN_HAS_TS_COMMIT	0x00010
/* Are we using a read timestamp for this checkpoint transaction? */
//__wt_txn_config->__wt_txn_set_read_timestamp中赋值
#define	WT_TXN_HAS_TS_READ	0x00020
#define	WT_TXN_NAMED_SNAPSHOT	0x00040
//__wt_txn_set_commit_timestamp中置位
#define	WT_TXN_PUBLIC_TS_COMMIT	0x00080
#define	WT_TXN_PUBLIC_TS_READ	0x00100
#define	WT_TXN_READONLY		0x00200
#define	WT_TXN_RUNNING		0x00400
#define	WT_TXN_SYNC_SET		0x00800
#define	WT_TXN_TS_COMMIT_ALWAYS	0x01000
#define	WT_TXN_TS_COMMIT_NEVER	0x02000

