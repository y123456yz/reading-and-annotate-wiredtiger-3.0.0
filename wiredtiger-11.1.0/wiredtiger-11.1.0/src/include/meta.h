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
 */
#define WT_BACKUP_TMP "WiredTiger.backup.tmp"  /* Backup tmp file */
#define WT_EXPORT_BACKUP "WiredTiger.export"   /* Export backup file */
//存在"WiredTiger.backup"文件说明是hotback文件
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

//Snapshot History Retention,MongoDB maintains the snapshot history in the WiredTigerHS.wt file
//参考https://www.mongodb.com/docs/manual/core/wiredtiger/
#define WT_HS_FILE "WiredTigerHS.wt"     /* History store table */
#define WT_HS_URI "file:WiredTigerHS.wt" /* History store table URI */

#define WT_SYSTEM_PREFIX "system:"                               /* System URI prefix */
#define WT_SYSTEM_CKPT_TS "checkpoint_timestamp"                 /* Checkpoint timestamp name */
#define WT_SYSTEM_CKPT_URI "system:checkpoint"                   /* Checkpoint timestamp URI */
#define WT_SYSTEM_OLDEST_TS "oldest_timestamp"                   /* Oldest timestamp name */
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
struct __wt_blkincr {
    const char *id_str;   /* User's name for this backup. */
    uint64_t granularity; /* Granularity of this backup. */
/* AUTOMATIC FLAG VALUE GENERATION START 0 */
#define WT_BLKINCR_FULL 0x1u  /* There is no checkpoint, always do full file */
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
//__ckpt_load_blk_mods中赋值使用
struct __wt_block_mods {
    const char *id_str;

    WT_ITEM bitstring;
    uint64_t nbits; /* Number of bits in bitstring */

    uint64_t offset; /* Zero bit offset for bitstring */
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
 */ //每生成一个checkpoint就会自增，见__meta_ckptlist_allocate_new_ckpt  __wt_meta_ckptlist_to_meta
#define WT_CHECKPOINT "WiredTigerCheckpoint"
#define WT_CKPT_FOREACH(ckptbase, ckpt) for ((ckpt) = (ckptbase); (ckpt)->name != NULL; ++(ckpt))
#define WT_CKPT_FOREACH_NAME_OR_ORDER(ckptbase, ckpt) \
    for ((ckpt) = (ckptbase); (ckpt)->name != NULL || (ckpt)->order != 0; ++(ckpt))

//__wt_meta_ckptlist_get_from_config中分配空间, 实例重启的时候通过__wt_btree_open->__wt_meta_checkpoint加载checkpoint元数据 
//__ckpt_last也可以解析获取

//__wt_btree.ckpt为该类型
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
    //每生成一个checkpoint就会自增，见__meta_ckptlist_allocate_new_ckpt  __wt_meta_ckptlist_to_meta
    int64_t order; /* Checkpoint order */
    //也就是checkpoint开始时间session->current_ckpt_sec，参考__meta_ckptlist_allocate_new_ckpt
    uint64_t sec; /* Wall clock time */

    uint64_t size; /* Checkpoint size */

    //__wt_checkpoint_tree_reconcile_update赋值
    uint64_t write_gen;     /* Write generation */
    uint64_t run_write_gen; /* Runtime write generation. */

    //赋值参考__wt_meta_block_metadata
    char *block_metadata;   /* Block-stored metadata */
    //把所有checkpoint核心元数据: 【root持久化元数据(包括internal ref key+所有leafpage ext) + alloc跳表持久化到磁盘的核心元数据信息
    //  +avail跳表持久化到磁盘的核心元数据信息】转换为wiredtiger.wt中对应的checkpoint=xxx字符串
    //转换后的字符串存储到block_checkpoint中，赋值参考__ckpt_update
    char *block_checkpoint; /* Block-stored checkpoint */

    WT_BLOCK_MODS backup_blocks[WT_BLKINCR_MAX];

    //__wt_checkpoint_tree_reconcile_update赋值
    WT_TIME_AGGREGATE ta; /* Validity window */

    //wiredtiger.wt中的checkpoint.addr中的原始checkpoint二进制元数据信息
    WT_ITEM addr; /* Checkpoint cookie string */
    //赋值参考__ckpt_update
    //封装所有checkpoint核心元数据: root持久化元数据(包括internal ref key+所有leafpage ext) + alloc跳表持久化到磁盘的核心元数据信息+avail跳表持久化到磁盘的核心元数据信息
    //重启的时候在__ckpt_load中加载，存储addr二进制解析后的数据，见__ckpt_load

    //内容输出可以参考 list_print_checkpoint  ../../../wt  list -c file:access.wt命令可以可视化输出raw内容
    //也可以参考__wt_ckpt_verbose
    WT_ITEM raw;  /* Checkpoint cookie raw */

    //创建空间及初始化__ckpt_extlist_read, 类型为WT_BLOCK_CKPT
    //ext list的真实数据ext内存元数据存储在这里__ckpt_extlist_read
    //bpriv代表上一次该表checkpoint的元数据信息通过__ckpt_extlist_read加载存到这里，在第一次和第二次两次checkpoint期间如果有page有修改，则把这期间修改
    //的page与第一次的checkpoint在__ckpt_process中做merge
    void *bpriv; /* Block manager private */

/* AUTOMATIC FLAG VALUE GENERATION START 0 */
//表示这是新的checkpoint
#define WT_CKPT_ADD 0x01u        /* Checkpoint to be added */
#define WT_CKPT_BLOCK_MODS 0x02u /* Return list of modified blocks */
//__drop
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
