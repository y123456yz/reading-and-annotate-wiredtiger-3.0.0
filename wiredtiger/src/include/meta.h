/*-
 * Copyright (c) 2014-2017 MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

/*
WiredTiger.basecfg存储基本配置信息
WiredTiger.lock用于防止多个进程连接同一个Wiredtiger数据库
table*.wt存储各个tale（数据库中的表）的数据
WiredTiger.wt是特殊的table，用于存储所有其他table的元数据信息
WiredTiger.turtle存储WiredTiger.wt的元数据信息
journal存储Write ahead log
*/

//记录版本信息，内容见__conn_single
#define	WT_WIREDTIGER		"WiredTiger"		/* Version file */
//文件锁，初始赋值见__conn_single
#define	WT_SINGLETHREAD		"WiredTiger.lock"	/* Locking file */

//写入内容在__conn_write_base_config，如果config_base使能，则会读取配置文件使用见__conn_config_file
#define	WT_BASECONFIG		"WiredTiger.basecfg"	/* Base configuration */
#define	WT_BASECONFIG_SET	"WiredTiger.basecfg.set"/* Base config temp */

//如果目录下有该文件，则会读取改文件内容作为配置文件 见__conn_config_file
#define	WT_USERCONFIG		"WiredTiger.config"	/* User configuration */

//创建和赋值见__backup_start  backup相关
#define	WT_BACKUP_TMP		"WiredTiger.backup.tmp"	/* Backup tmp file */
#define	WT_METADATA_BACKUP	"WiredTiger.backup"	/* Hot backup file */
#define	WT_INCREMENTAL_BACKUP	"WiredTiger.ibackup"	/* Incremental backup */
#define	WT_INCREMENTAL_SRC	"WiredTiger.isrc"	/* Incremental source */

//内容默认在__wt_turtle_read中构造，创建文件和写入内容在__wt_turtle_update
#define	WT_METADATA_TURTLE	"WiredTiger.turtle"	/* Metadata metadata */
//turtle更新的时候临时文件，见__wt_turtle_update
#define	WT_METADATA_TURTLE_SET	"WiredTiger.turtle.set"	/* Turtle temp file */

#define	WT_METADATA_URI		"metadata:"		/* Metadata alias */
//该文件在__create_file中创建
#define	WT_METAFILE		"WiredTiger.wt"		/* Metadata table */
#define	WT_METAFILE_URI		"file:WiredTiger.wt"	/* Metadata table URI */

//__wt_las_create中创建
#define	WT_LAS_URI		"file:WiredTigerLAS.wt"	/* Lookaside table URI*/

/*
 * Optimize comparisons against the metafile URI, flag handles that reference
 * the metadata file.
 */
#define	WT_IS_METADATA(dh)      F_ISSET((dh), WT_DHANDLE_IS_METADATA)
#define	WT_METAFILE_ID		0			/* Metadata file ID */

#define	WT_METADATA_VERSION	"WiredTiger version"	/* Version keys */
#define	WT_METADATA_VERSION_STR	"WiredTiger version string"

/*
 * WT_WITH_TURTLE_LOCK --
 *	Acquire the turtle file lock, perform an operation, drop the lock.
 */
#define	WT_WITH_TURTLE_LOCK(session, op) do {				\
	WT_ASSERT(session, !F_ISSET(session, WT_SESSION_LOCKED_TURTLE));\
	WT_WITH_LOCK_WAIT(session,					\
	    &S2C(session)->turtle_lock, WT_SESSION_LOCKED_TURTLE, op);	\
} while (0)

/*
 * WT_CKPT --
 *	Encapsulation of checkpoint information, shared by the metadata, the
 * btree engine, and the block manager.
 */
#define	WT_CHECKPOINT		"WiredTigerCheckpoint"
#define	WT_CKPT_FOREACH(ckptbase, ckpt)					\
	for ((ckpt) = (ckptbase); (ckpt)->name != NULL; ++(ckpt))

/*checkpoint信息结构指针, __ckpt_load中获取配置的checkPoint信息填充该结构 */
struct __wt_ckpt {
	char	*name;				/* Name or NULL */

	WT_ITEM  addr;				/* Checkpoint cookie string */
	WT_ITEM  raw;				/* Checkpoint cookie raw */

	int64_t	 order;				/* Checkpoint order */

	uintmax_t sec;				/* Timestamp */

	uint64_t ckpt_size;			/* Checkpoint size */

	uint64_t write_gen;			/* Write generation */

	void	*bpriv;				/* Block manager private */

#define	WT_CKPT_ADD	0x01			/* Checkpoint to be added */
#define	WT_CKPT_DELETE	0x02			/* Checkpoint to be deleted */
#define	WT_CKPT_FAKE	0x04			/* Checkpoint is a fake */
#define	WT_CKPT_UPDATE	0x08			/* Checkpoint requires update */
	uint32_t flags;
};
