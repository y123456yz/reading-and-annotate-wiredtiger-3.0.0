/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */
//"WiredTiger"ֻ�Ǽ�¼wiredtiger�汾��Ϣ��ʵ����ûɶ��
#define WT_WIREDTIGER "WiredTiger"        /* Version file */
#define WT_SINGLETHREAD "WiredTiger.lock" /* Locking file */
//������WiredTiger���ݿ�ʱ�����ݸ�wiredtiger_open�������ַ��������浽��ΪWiredTiger��WiredTiger��Ŀ¼�ļ��С�
//�������ļ������������ݿ�ʱ��ȡ�� �ο�https://source.wiredtiger.com/develop/database_config.html
//��¼wiredtiger_open���������ò�����Ϣ
#define WT_BASECONFIG "WiredTiger.basecfg"         /* Base configuration */
#define WT_BASECONFIG_SET "WiredTiger.basecfg.set" /* Base config temp */

//https://source.wiredtiger.com/develop/database_config.html#config_order
#define WT_USERCONFIG "WiredTiger.config" /* User configuration */

/*
 * Backup related WiredTiger files.
 */
#define WT_BACKUP_TMP "WiredTiger.backup.tmp"  /* Backup tmp file */
#define WT_EXPORT_BACKUP "WiredTiger.export"   /* Export backup file */
//����"WiredTiger.backup"�ļ�˵����hotback�ļ�
#define WT_METADATA_BACKUP "WiredTiger.backup" /* Hot backup file */
#define WT_LOGINCR_BACKUP "WiredTiger.ibackup" /* Log incremental backup */
#define WT_LOGINCR_SRC "WiredTiger.isrc"       /* Log incremental source */

//���ĵ��ļ�������Ҫ������cat���ɣ����wiredtiger.wtԪ������Ϣ
#define WT_METADATA_TURTLE "WiredTiger.turtle"         /* Metadata metadata */
#define WT_METADATA_TURTLE_SET "WiredTiger.turtle.set" /* Turtle temp file */

#define WT_METADATA_URI "metadata:"           /* Metadata alias */
//WiredTiger.wt�е�Ԫ���ݲ����й��û����ݿ����Ҫ��Ϣ��Ԫ���ݰ������ٻ�����Ϣ���������ݿ��д��ڵ��ļ��ͱ����ǵ����������
//�����µļ��㡣������Ϣ����WiredTiger�ڷ����ļ�ʱ��������Ҹ�ҳ������������������֡�WiredTiger�е���ҪԪ���ݴ洢��
//WiredTiger.wt��  �ο�https://source.wiredtiger.com/develop/arch-metadata.html
#define WT_METAFILE "WiredTiger.wt"           /* Metadata table */
#define WT_METAFILE_SLVG "WiredTiger.wt.orig" /* Metadata copy */
#define WT_METAFILE_URI "file:WiredTiger.wt"  /* Metadata table URI */

//Snapshot History Retention,MongoDB maintains the snapshot history in the WiredTigerHS.wt file
//�ο�https://www.mongodb.com/docs/manual/core/wiredtiger/
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
//__wt_conn_dhandle_alloc  ˵����ӦԪ�����ļ�"file:WiredTiger.wt"
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
//__ckpt_load_blk_mods�и�ֵʹ��
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
 */ //ÿ����һ��checkpoint�ͻ���������__meta_ckptlist_allocate_new_ckpt  __wt_meta_ckptlist_to_meta
#define WT_CHECKPOINT "WiredTigerCheckpoint"
#define WT_CKPT_FOREACH(ckptbase, ckpt) for ((ckpt) = (ckptbase); (ckpt)->name != NULL; ++(ckpt))
#define WT_CKPT_FOREACH_NAME_OR_ORDER(ckptbase, ckpt) \
    for ((ckpt) = (ckptbase); (ckpt)->name != NULL || (ckpt)->order != 0; ++(ckpt))

//__wt_meta_ckptlist_get_from_config�з���ռ�, ʵ��������ʱ��ͨ��__wt_btree_open->__wt_meta_checkpoint����checkpointԪ���� 
//__ckpt_lastҲ���Խ�����ȡ

//__wt_btree.ckptΪ������
struct __wt_ckpt {
    //��ֵ��__checkpoint_lock_dirty_tree����ָ��name��Ĭ��Ϊ"WiredTigerCheckpoint"
    //wreidtiger.wt��checkpoint=(WiredTigerCheckpoint.1=(addr="018181e4886a50198281e41546bd168381e4fa0c608a808080e22fc0cfc0"
    //�����Ӧ��name����WiredTigerCheckpoint.1
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
    //ÿ����һ��checkpoint�ͻ���������__meta_ckptlist_allocate_new_ckpt  __wt_meta_ckptlist_to_meta
    int64_t order; /* Checkpoint order */
    //Ҳ����checkpoint��ʼʱ��session->current_ckpt_sec���ο�__meta_ckptlist_allocate_new_ckpt
    uint64_t sec; /* Wall clock time */

    uint64_t size; /* Checkpoint size */

    //__wt_checkpoint_tree_reconcile_update��ֵ
    uint64_t write_gen;     /* Write generation */
    uint64_t run_write_gen; /* Runtime write generation. */

    //��ֵ�ο�__wt_meta_block_metadata
    char *block_metadata;   /* Block-stored metadata */
    //������checkpoint����Ԫ����: ��root�־û�Ԫ����(����internal ref key+����leafpage ext) + alloc����־û������̵ĺ���Ԫ������Ϣ
    //  +avail����־û������̵ĺ���Ԫ������Ϣ��ת��Ϊwiredtiger.wt�ж�Ӧ��checkpoint=xxx�ַ���
    //ת������ַ����洢��block_checkpoint�У���ֵ�ο�__ckpt_update
    char *block_checkpoint; /* Block-stored checkpoint */

    WT_BLOCK_MODS backup_blocks[WT_BLKINCR_MAX];

    //__wt_checkpoint_tree_reconcile_update��ֵ
    WT_TIME_AGGREGATE ta; /* Validity window */

    //wiredtiger.wt�е�checkpoint.addr�е�ԭʼcheckpoint������Ԫ������Ϣ
    WT_ITEM addr; /* Checkpoint cookie string */
    //��ֵ�ο�__ckpt_update
    //��װ����checkpoint����Ԫ����: root�־û�Ԫ����(����internal ref key+����leafpage ext) + alloc����־û������̵ĺ���Ԫ������Ϣ+avail����־û������̵ĺ���Ԫ������Ϣ
    //������ʱ����__ckpt_load�м��أ��洢addr�����ƽ���������ݣ���__ckpt_load

    //����������Բο� list_print_checkpoint  ../../../wt  list -c file:access.wt������Կ��ӻ����raw����
    //Ҳ���Բο�__wt_ckpt_verbose
    WT_ITEM raw;  /* Checkpoint cookie raw */

    //�����ռ估��ʼ��__ckpt_extlist_read, ����ΪWT_BLOCK_CKPT
    //ext list����ʵ����ext�ڴ�Ԫ���ݴ洢������__ckpt_extlist_read
    //bpriv������һ�θñ�checkpoint��Ԫ������Ϣͨ��__ckpt_extlist_read���ش浽����ڵ�һ�κ͵ڶ�������checkpoint�ڼ������page���޸ģ�������ڼ��޸�
    //��page���һ�ε�checkpoint��__ckpt_process����merge
    void *bpriv; /* Block manager private */

/* AUTOMATIC FLAG VALUE GENERATION START 0 */
//��ʾ�����µ�checkpoint
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
