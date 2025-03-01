/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */
//"WiredTiger"只是记录wiredtiger版本信息，实际上没啥用
#define WT_WIREDTIGER "WiredTiger"        /* Version file */
#define WT_SINGLETHREAD "WiredTiger.lock" /* Locking file */
//当创建WiredTiger数据库时，传递给wiredtiger_open的配置字符串被保存到名为WiredTiger的WiredTiger主目录文件中。
//该配置文件将在随后打开数据库时读取。 参考https://source.wiredtiger.com/develop/database_config.html
//记录wiredtiger_open函数的配置参数信息
#define WT_BASECONFIG "WiredTiger.basecfg"         /* Base configuration */
#define WT_BASECONFIG_SET "WiredTiger.basecfg.set" /* Base config temp */

//https://source.wiredtiger.com/develop/database_config.html#config_order
#define WT_USERCONFIG "WiredTiger.config" /* User configuration */

/*
 * Backup related WiredTiger files.
 */ //WT_BACKUP_TMP文件  __backup_start，最终在__backup_start结尾被更名为WT_METADATA_BACKUP，里面存储的是备份表的元数据信息
#define WT_BACKUP_TMP "WiredTiger.backup.tmp"  /* Backup tmp file */ //__backup_start中创建该文件
#define WT_EXPORT_BACKUP "WiredTiger.export"   /* Export backup file */
//存在"WiredTiger.backup"文件说明是hotback文件，这里面的内容也就是，这样就可以通过该文件实现热备数据的恢复
//恢复热备数据对应的元数据接口__metadata_load_hot_backup
#define WT_METADATA_BACKUP "WiredTiger.backup" /* Hot backup file */
#define WT_LOGINCR_BACKUP "WiredTiger.ibackup" /* Log incremental backup */
#define WT_LOGINCR_SRC "WiredTiger.isrc"       /* Log incremental source */

//明文得文件，不需要解析，cat即可，存放wiredtiger.wt元数据信息
#define WT_METADATA_TURTLE "WiredTiger.turtle"         /* Metadata metadata */
#define WT_METADATA_TURTLE_SET "WiredTiger.turtle.set" /* Turtle temp file */

#define WT_METADATA_URI "metadata:"           /* Metadata alias */
//WiredTiger.wt中的元数据捕获有关用户数据库的重要信息。元数据包括跟踪基本信息，例如数据库中存在的文件和表、它们的相关配置以
//及最新的检查点。检查点信息告诉WiredTiger在访问文件时到哪里查找根页面和树的所有其他部分。WiredTiger中的主要元数据存储在
//WiredTiger.wt中  参考https://source.wiredtiger.com/develop/arch-metadata.html
#define WT_METAFILE "WiredTiger.wt"           /* Metadata table */
#define WT_METAFILE_SLVG "WiredTiger.wt.orig" /* Metadata copy */
#define WT_METAFILE_URI "file:WiredTiger.wt"  /* Metadata table URI */

/*
Starting in MongoDB 5.0, you can use the minSnapshotHistoryWindowInSeconds parameter to specify how long WiredTiger 
  keeps the snapshot history.

Increasing the value of minSnapshotHistoryWindowInSeconds increases disk usage because the server must maintain 
  the history of older modified values within the specified time window. The amount of disk space used depends 
  on your workload, with higher volume workloads requiring more disk space.

MongoDB maintains the snapshot history in the WiredTigerHS.wt file, located in your specifie
*/
//Snapshot History Retention,MongoDB maintains the snapshot history in the WiredTigerHS.wt file
//参考https://www.mongodb.com/docs/manual/core/wiredtiger/
#define WT_HS_FILE "WiredTigerHS.wt"     /* History store table */
#define WT_HS_URI "file:WiredTigerHS.wt" /* History store table URI */

#define WT_SYSTEM_PREFIX "system:"                               /* System URI prefix */
//也就是最近一次做checkpoint的stable_timestamp
#define WT_SYSTEM_CKPT_TS "checkpoint_timestamp"                 /* Checkpoint timestamp name */
#define WT_SYSTEM_CKPT_URI "system:checkpoint"                   /* Checkpoint timestamp URI */
#define WT_SYSTEM_OLDEST_TS "oldest_timestamp"                   /* Oldest timestamp name */
//也就是oldest_timestamp与stable_timestamp的最小timestamp
#define WT_SYSTEM_OLDEST_URI "system:oldest"                     /* Oldest timestamp URI */
#define WT_SYSTEM_TS_TIME "checkpoint_time"                      /* Checkpoint wall time */
#define WT_SYSTEM_TS_WRITE_GEN "write_gen"                       /* Checkpoint write generation */
#define WT_SYSTEM_CKPT_SNAPSHOT "snapshots"                      /* List of snapshots */
#define WT_SYSTEM_CKPT_SNAPSHOT_MIN "snapshot_min"               /* Snapshot minimum */
#define WT_SYSTEM_CKPT_SNAPSHOT_MAX "snapshot_max"               /* Snapshot maximum */
#define WT_SYSTEM_CKPT_SNAPSHOT_COUNT "snapshot_count"           /* Snapshot count */
#define WT_SYSTEM_CKPT_SNAPSHOT_URI "system:checkpoint_snapshot" /* Checkpoint snapshot URI */
#define WT_SYSTEM_CKPT_SNAPSHOT_TIME "checkpoint_time"           /* Checkpoint wall time */
#define WT_SYSTEM_CKPT_SNAPSHOT_WRITE_GEN "write_gen"            /* Checkpoint write generation */
#define WT_SYSTEM_BASE_WRITE_GEN_URI "system:checkpoint_base_write_gen" /* Base write gen URI */
#define WT_SYSTEM_BASE_WRITE_GEN "base_write_gen"                       /* Base write gen name */

/* Check whether a string is a legal URI for a btree object */
#define WT_BTREE_PREFIX(str) (WT_PREFIX_MATCH(str, "file:") || WT_PREFIX_MATCH(str, "tiered:"))

/*
 * Optimize comparisons against the metafile URI, flag handles that reference the metadata file.
 */
//__wt_conn_dhandle_alloc  说明对应元数据文件"file:WiredTiger.wt"
#define WT_IS_METADATA(dh) F_ISSET((dh), WT_DHANDLE_IS_METADATA)
#define WT_METAFILE_ID 0 /* Metadata file ID */

#define WT_METADATA_COMPAT "Compatibility version"
#define WT_METADATA_VERSION "WiredTiger version" /* Version keys */
#define WT_METADATA_VERSION_STR "WiredTiger version string"

/*
 * As a result of a data format change WiredTiger is not able to start on versions below 3.2.0, as
 * it will write out a data format that is not readable by those versions. These version numbers
 * provide such mechanism.
 */
#define WT_MIN_STARTUP_VERSION ((WT_VERSION){3, 2, 0}) /* Minimum version we can start on. */

/*
 * WT_WITH_TURTLE_LOCK --
 *	Acquire the turtle file lock, perform an operation, drop the lock.
 */
#define WT_WITH_TURTLE_LOCK(session, op)                                                      \
    do {                                                                                      \
        WT_ASSERT(session, !FLD_ISSET(session->lock_flags, WT_SESSION_LOCKED_TURTLE));        \
        WT_WITH_LOCK_WAIT(session, &S2C(session)->turtle_lock, WT_SESSION_LOCKED_TURTLE, op); \
    } while (0)

/*
 * Block based incremental backup structure. These live in the connection.
 */
#define WT_BLKINCR_MAX 2
//conn->incr_backups[i]  __wt_cursor_backup.incr_src为该类型
struct __wt_blkincr {
    const char *id_str;   /* User's name for this backup. */
    uint64_t granularity; /* Granularity of this backup. */
/* AUTOMATIC FLAG VALUE GENERATION START 0 */
//说明wiredtiger.turtle中没有wiredtiger.wt这个K的checkpoint元数据或者没有wiredtiger.wt这个K信息
#define WT_BLKINCR_FULL 0x1u  /* There is no checkpoint, always do full file */
//"incremental.src_id"配置后会置位
#define WT_BLKINCR_INUSE 0x2u /* This entry is active */
#define WT_BLKINCR_VALID 0x4u /* This entry is valid */
                              /* AUTOMATIC FLAG VALUE GENERATION STOP 8 */
    uint8_t flags;
};

/*
 * Block modifications from an incremental identifier going forward.
 */
/*
 * At the default granularity, this is enough for blocks in a 2G file.
 */
#define WT_BLOCK_MODS_LIST_MIN 128 /* Initial bits for bitmap. */

//__wt_ckpt.backup_blocks成员为该类型

//__ckpt_load_blk_mods中从元数据中解析checkpoint_backup_info赋值
//__ckpt_update->(__ckpt_add_blk_mods_ext __ckpt_add_blk_mods_alloc)中赋值
struct __wt_block_mods {
    const char *id_str;

    //checkpoint_backup_info=("ID2"=(id=0,granularity=1048576,nbits=128,offset=0,rename=0,blocks=1f000000000000000000000000000000),
    //    "ID3"=(id=1,granularity=1048576,nbits=128,offset=0,rename=0,blocks=1f000000000000000000000000000000))
    //  这里的blocks就是以granularity为单位的位图，例如这里的blocks=1f000000000000000000000000000000，去掉0也就是1f，对应
    //    位图为11111,也就是从offset=0开始的5个granularity大小为变化的增量数据块
    WT_ITEM bitstring;
    uint64_t nbits; /* Number of bits in bitstring */

    uint64_t offset; /* Zero bit offset for bitstring */
    //incremental.granularity配置，默认16M
    uint64_t granularity;
/* AUTOMATIC FLAG VALUE GENERATION START 0 */
#define WT_BLOCK_MODS_RENAME 0x1u /* Entry is from a rename */
#define WT_BLOCK_MODS_VALID 0x2u  /* Entry is valid */
                                  /* AUTOMATIC FLAG VALUE GENERATION STOP 32 */
    uint32_t flags;
};

/*
 * WT_CKPT --
 *	Encapsulation of checkpoint information, shared by the metadata, the
 * btree engine, and the block manager.
 //默认WT_CHECKPOINT checkpoint就会自增，也就是WiredTigerCheckpoint.x，见__meta_ckptlist_allocate_new_ckpt  __wt_meta_ckptlist_to_meta

//用户不能显示指定checkpoint的名称为WiredTigerCheckpoint开头的，因为__checkpoint_name_ok中有做检查
 */ 

 //每生成一个checkpoint就会自增，见__meta_ckptlist_allocate_new_ckpt  __wt_meta_ckptlist_to_meta
#define WT_CHECKPOINT "WiredTigerCheckpoint"
#define WT_CKPT_FOREACH(ckptbase, ckpt) for ((ckpt) = (ckptbase); (ckpt)->name != NULL; ++(ckpt))
#define WT_CKPT_FOREACH_NAME_OR_ORDER(ckptbase, ckpt) \
    for ((ckpt) = (ckptbase); (ckpt)->name != NULL || (ckpt)->order != 0; ++(ckpt))

//__wt_meta_ckptlist_get_from_config中分配空间, 实例重启的时候通过__wt_btree_open->__wt_meta_checkpoint加载checkpoint元数据 
//__ckpt_last也可以解析获取

//__ckpt_load解析ckpt信息赋值给对应字段
//__wt_btree.ckpt为该类型，对应wiredtiger.wt中一个表(也就是一个btree)的checkpoint信息
struct __wt_ckpt {
    //赋值见__checkpoint_lock_dirty_tree，不指定name，默认为"WiredTigerCheckpoint"
    //wreidtiger.wt中checkpoint=(WiredTigerCheckpoint.1=(addr="018181e4886a50198281e41546bd168381e4fa0c608a808080e22fc0cfc0"
    //这个对应的name就是WiredTigerCheckpoint.1
    char *name; /* Name or NULL */

    /*
     * Each internal checkpoint name is appended with a generation to make it a unique name. We're
     * solving two problems: when two checkpoints are taken quickly, the timer may not be unique
     * and/or we can even see time travel on the second checkpoint if we snapshot the time
     * in-between nanoseconds rolling over. Second, if we reset the generational counter when new
     * checkpoints arrive, we could logically re-create specific checkpoints, racing with cursors
     * open on those checkpoints. I can't think of any way to return incorrect results by racing
     * with those cursors, but it's simpler not to worry about it.
     */
    //默认WT_CHECKPOINT checkpoint就会自增，也就是WiredTigerCheckpoint.x，见__meta_ckptlist_allocate_new_ckpt  __wt_meta_ckptlist_to_meta
    int64_t order; /* Checkpoint order */
    //也就是checkpoint开始时间session->current_ckpt_sec，参考__meta_ckptlist_allocate_new_ckpt
    uint64_t sec; /* Wall clock time */

    uint64_t size; /* Checkpoint size */

    //__wt_checkpoint_tree_reconcile_update赋值
    uint64_t write_gen;     /* Write generation */
    uint64_t run_write_gen; /* Runtime write generation. */

    //赋值参考__wt_meta_block_metadata，实际上就是该btree表的
    char *block_metadata;   /* Block-stored metadata */
    //把所有checkpoint核心元数据: [root持久化元数据 + alloc跳表持久化到磁盘的核心元数据信息 +avail跳表持久化到磁盘的核心元数据信息]转换为wiredtiger.wt中对应的checkpoint=xxx字符串
    //转换后的字符串存储到block_checkpoint中，赋值参考__ckpt_update
    char *block_checkpoint; /* Block-stored checkpoint */ //也就是wiredtiger.wt文件中的checkpoint=(xxxxx)信息记录到这里

    //__ckpt_load_blk_mods中从元数据中解析checkpoint_backup_info赋值
    //__ckpt_update->(__ckpt_add_blk_mods_ext __ckpt_add_blk_mods_alloc)中赋值
    //__wt_ckpt.backup_blocks
    WT_BLOCK_MODS backup_blocks[WT_BLKINCR_MAX];

    //__wt_checkpoint_tree_reconcile_update赋值
    WT_TIME_AGGREGATE ta; /* Validity window */

    //wiredtiger.wt中的checkpoint.addr中的原始checkpoint二进制元数据信息
    //赋值见__wt_meta_ckptlist_to_meta,也就是checkpoint=(midnight=(addr="xxxxx"))中addr后的内容，内容来源实际上在__wt_block_ckpt_pack
    WT_ITEM addr; /* Checkpoint cookie string */
    //赋值参考__ckpt_update
    //封装所有checkpoint核心元数据: root持久化元数据(包括internal ref key+所有leafpage ext) + alloc跳表持久化到磁盘的核心元数据信息+avail跳表持久化到磁盘的核心元数据信息
    //重启的时候在__ckpt_load中加载，存储addr二进制解析后的数据，见__ckpt_load

    //内容输出可以参考 list_print_checkpoint  ../../../wt  list -c file:access.wt命令可以可视化输出raw内容
    //也可以参考__wt_ckpt_verbose
    WT_ITEM raw;  /* Checkpoint cookie raw */

    //创建空间及初始化__ckpt_extlist_read, 真正使用在__ckpt_extlist_read， WT_BLOCK_CKPT类型
    //ext list的真实数据ext内存元数据存储在这里__ckpt_extlist_read
    //bpriv代表上一次该表checkpoint的元数据信息通过__ckpt_extlist_read加载存到这里，在第一次和第二次两次checkpoint期间如果有page有修改，则把这期间修改
    //的page与第一次的checkpoint在__ckpt_process中做merge
    void *bpriv; /* Block manager private */

/* AUTOMATIC FLAG VALUE GENERATION START 0 */
//表示这是新的checkpoint   __meta_ckptlist_allocate_new_ckpt->__meta_blk_mods_load置位
#define WT_CKPT_ADD 0x01u        /* Checkpoint to be added */
//代表backup期间修改的block，__meta_blk_mods_load中置位
#define WT_CKPT_BLOCK_MODS 0x02u /* Return list of modified blocks */
//__drop  __drop_from  __drop_to置位，标识需要删除
//__drop，如果ckptbase链表中已经存在已有得checkpoint name，说明之前的相同checkpoint name对应的checkpoint需要删除，需要删除的checkpoint保存到drop_list，
//  同时设置WT_CKPT_DELETE标识, 等待__ckpt_process做真正的删除
#define WT_CKPT_DELETE 0x04u     /* Checkpoint to be deleted */
#define WT_CKPT_FAKE 0x08u       /* Checkpoint is a fake */
#define WT_CKPT_UPDATE 0x10u     /* Checkpoint requires update */
                                 /* AUTOMATIC FLAG VALUE GENERATION STOP 32 */
    uint32_t flags;
};

/*
 * WT_CKPT_SNAPSHOT --
 *     Snapshot and timestamp information associated with a checkpoint.
 */
struct __wt_ckpt_snapshot {
    uint64_t ckpt_id;
    uint64_t oldest_ts;
    uint64_t stable_ts;
    uint64_t snapshot_write_gen;
    uint64_t snapshot_min;
    uint64_t snapshot_max;
    uint64_t *snapshot_txns;
    uint32_t snapshot_count;
};
