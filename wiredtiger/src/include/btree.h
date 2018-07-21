/*-
 * Copyright (c) 2014-2017 MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

/*
 * Supported btree formats: the "current" version is the maximum supported
 * major/minor versions.
 */
#define	WT_BTREE_MAJOR_VERSION_MIN	1	/* Oldest version supported */
#define	WT_BTREE_MINOR_VERSION_MIN	1

#define	WT_BTREE_MAJOR_VERSION_MAX	1	/* Newest version supported */
#define	WT_BTREE_MINOR_VERSION_MAX	1

/*
 * The maximum btree leaf and internal page size is 512MB (2^29).  The limit
 * is enforced in software, it could be larger, specifically, the underlying
 * default block manager can support 4GB (2^32).  Currently, the maximum page
 * size must accommodate our dependence on the maximum page size fitting into
 * a number of bits less than 32; see the row-store page key-lookup functions
 * for the magic.
 */
#define	WT_BTREE_PAGE_SIZE_MAX		(512 * WT_MEGABYTE)

/*
 * The length of variable-length column-store values and row-store keys/values
 * are stored in a 4B type, so the largest theoretical key/value item is 4GB.
 * However, in the WT_UPDATE structure we use the UINT32_MAX size as a "deleted"
 * flag, and second, the size of an overflow object is constrained by what an
 * underlying block manager can actually write.  (For example, in the default
 * block manager, writing an overflow item includes the underlying block's page
 * header and block manager specific structure, aligned to an allocation-sized
 * unit).  The btree engine limits the size of a single object to (4GB - 1KB);
 * that gives us additional bytes if we ever want to store a structure length
 * plus the object size in 4B, or if we need additional flag values.  Attempts
 * to store large key/value items in the tree trigger an immediate check to the
 * block manager, to make sure it can write the item.  Storing 4GB objects in a
 * btree borders on clinical insanity, anyway.
 *
 * Record numbers are stored in 64-bit unsigned integers, meaning the largest
 * record number is "really, really big".
 */
#define	WT_BTREE_MAX_OBJECT_SIZE	((uint32_t)(UINT32_MAX - 1024))

/*
 * A location in a file is a variable-length cookie, but it has a maximum size
 * so it's easy to create temporary space in which to store them.  (Locations
 * can't be much larger than this anyway, they must fit onto the minimum size
 * page because a reference to an overflow page is itself a location.)
 */
#define	WT_BTREE_MAX_ADDR_COOKIE	255	/* Maximum address cookie */

/* Evict pages if we see this many consecutive deleted records. */
#define	WT_BTREE_DELETE_THRESHOLD	1000

/*
 * Minimum size of the chunks (in percentage of the page size) a page gets split
 * into during reconciliation.
 */
#define	WT_BTREE_MIN_SPLIT_PCT		50

/*
 * WT_BTREE --
 *	A btree handle.
 __wt_conn_dhandle_alloc中分配空间
 btree结构  S2BT(session)完成session到btree的转换   成员赋值__btree_conf
 btree对应的文件见__wt_block_open
 */
struct __wt_btree {
    //赋值见__wt_btree_open
	WT_DATA_HANDLE *dhandle;
    /*checkpoint信息结构指针*/
	WT_CKPT	  *ckpt;		/* Checkpoint information */

    //见__btree_conf
	enum {	BTREE_COL_FIX=1,	/* Fixed-length column store */  /*列式定长存储*/
		BTREE_COL_VAR=2,	/* Variable-length column store */   /*列式变长存储*/
		BTREE_ROW=3		/* Row-store */                     /*行式存储*/
	} type;				/* Type */

	const char *key_format;		/* Key format */
	const char *value_format;	/* Value format */
	/*定长field的长度*/
	uint8_t bitcnt;			/* Fixed-length field size in bits */
    /*行存储时的比较器*/
	WT_COLLATOR *collator;		/* Row-store comparator */
	/*如果这个值为1，表示比较器需要进行free*/
	int collator_owned;		/* The collator needs to be freed */

    /*btree索引文件ID,主要用于redo log的推演*/
	uint32_t id;			/* File ID, for logging */
    /*行存储时的key前缀范围长度*/
	uint32_t key_gap;		/* Row-store prefix key gap */

	uint32_t allocsize;		/* Allocation size */
	uint32_t maxintlpage;		/* Internal page max size */
	uint32_t maxintlkey;		/* Internal page max key size */
	uint32_t maxleafpage;		/* Leaf page max size */
	uint32_t maxleafkey;		/* Leaf page max key size */
	uint32_t maxleafvalue;		/* Leaf page max value size */
	uint64_t maxmempage;		/* In-memory page max size */
	uint64_t splitmempage;		/* In-memory split trigger size */

#define	WT_ASSERT_COMMIT_TS_ALWAYS	0x0001
#define	WT_ASSERT_COMMIT_TS_NEVER	0x0002
#define	WT_ASSERT_READ_TS_ALWAYS	0x0004
#define	WT_ASSERT_READ_TS_NEVER		0x0008
	uint32_t assert_flags;		/* Debugging assertion information */

    /*key值的霍夫曼编码*/
	void *huffman_key;		/* Key huffman encoding */
	/*value值的霍夫曼编码*/
	void *huffman_value;		/* Value huffman encoding */

    /*checksum开关*/
	enum {	CKSUM_ON=1,		/* On */
		CKSUM_OFF=2,		/* Off */
		CKSUM_UNCOMPRESSED=3	/* Uncompressed blocks only */
	} checksum;			/* Checksum configuration */

	/*
	 * Reconciliation...
	 */
	u_int dictionary;		/* Dictionary slots */
	bool  internal_key_truncate;	/* Internal key truncate */
	/*前缀压缩开关*/
	bool  prefix_compression;	/* Prefix compression */
	u_int prefix_compression_min;	/* Prefix compression min */

#define	WT_SPLIT_DEEPEN_MIN_CHILD_DEF	10000
    /*页split时最少的entry个数*/
	u_int split_deepen_min_child;	/* Minimum entries to deepen tree */
#define	WT_SPLIT_DEEPEN_PER_CHILD_DEF	100
    /*页slpit时btree层增加的平均entry个数*/
	u_int split_deepen_per_child;	/* Entries per child when deepened */
	int   split_pct;		/* Split page percent */

    /*页数据压缩器*/
	WT_COMPRESSOR *compressor;	/* Page compressor */
	WT_KEYED_ENCRYPTOR *kencryptor;	/* Page encryptor */

	WT_RWLOCK ovfl_lock;		/* Overflow lock */

    /*树的最大层数*/
	int	maximum_depth;		/* Maximum tree depth during search */
	u_int	rec_multiblock_max;	/* Maximum blocks written for a page */

    /*列式存储时最后的记录序号*/
	uint64_t last_recno;		/* Column-store last record number */

    /*btree root的根节点句柄*/
	WT_REF	root;			/* Root page reference */
	/*btree修改标示*/
	bool	modified;		/* If the tree ever modified */
	uint8_t	original;		/* Newly created: bulk-load possible
					   (want a bool but needs atomic cas) */

	bool lookaside_entries;		/* Has entries in the lookaside table */
	bool lsm_primary;		/* Handle is/was the LSM primary */

    /*block manager句柄*/
	WT_BM	*bm;			/* Block manager reference */
	/*block头长度，=WT_PAGE_HEADER_BYTE_SIZE*/
	u_int	 block_header;		/* WT_PAGE_HEADER_BYTE_SIZE */

	uint64_t write_gen;		/* Write generation */
	uint64_t rec_max_txn;		/* Maximum txn seen (clean trees) */
	WT_DECL_TIMESTAMP(rec_max_timestamp)

	uint64_t checkpoint_gen;	/* Checkpoint generation */
	volatile enum {
		WT_CKPT_OFF, WT_CKPT_PREPARE, WT_CKPT_RUNNING
	} checkpointing;		/* Checkpoint in progress */

	uint64_t    bytes_inmem;	/* Cache bytes in memory. */
	uint64_t    bytes_dirty_intl;	/* Bytes in dirty internal pages. */
	uint64_t    bytes_dirty_leaf;	/* Bytes in dirty leaf pages. */

	/*
	 * We flush pages from the tree (in order to make checkpoint faster),
	 * without a high-level lock.  To avoid multiple threads flushing at
	 * the same time, lock the tree.
	 */
	WT_SPINLOCK	flush_lock;	/* Lock to flush the tree's pages */

	/*
	 * All of the following fields live at the end of the structure so it's
	 * easier to clear everything but the fields that persist.
	 */
#define	WT_BTREE_CLEAR_SIZE	(offsetof(WT_BTREE, evict_ref))

	/*
	 * Eviction information is maintained in the btree handle, but owned by
	 * eviction, not the btree code.
	 */
	WT_REF	   *evict_ref;		/* Eviction thread's location */
	uint64_t    evict_priority;	/* Relative priority of cached pages */
	u_int	    evict_walk_period;	/* Skip this many LRU walks */
	u_int	    evict_walk_saved;	/* Saved walk skips for checkpoints */
	u_int	    evict_walk_skips;	/* Number of walks skipped */
	int32_t	    evict_disabled;	/* Eviction disabled count */
	bool	    evict_disabled_open;/* Eviction disabled on open */
	volatile uint32_t evict_busy;	/* Count of threads in eviction */
	enum {				/* Start position for eviction walk */
		WT_EVICT_WALK_NEXT,
		WT_EVICT_WALK_PREV,
		WT_EVICT_WALK_RAND_NEXT,
		WT_EVICT_WALK_RAND_PREV
	} evict_start_type;

	/*
	 * Flag values up to 0xff are reserved for WT_DHANDLE_XXX.
	 */
#define	WT_BTREE_ALTER		0x000100 /* Handle is for alter */
#define	WT_BTREE_BULK		0x000200 /* Bulk-load handle */
#define	WT_BTREE_CLOSED		0x000400 /* Handle closed */
#define	WT_BTREE_IGNORE_CACHE	0x000800 /* Cache-resident object */
#define	WT_BTREE_IN_MEMORY	0x001000 /* Cache-resident object */
#define	WT_BTREE_LOOKASIDE	0x002000 /* Look-aside table */
#define	WT_BTREE_NO_CHECKPOINT	0x004000 /* Disable checkpoints */
#define	WT_BTREE_NO_LOGGING	0x008000 /* Disable logging */
#define	WT_BTREE_REBALANCE	0x010000 /* Handle is for rebalance */
#define	WT_BTREE_SALVAGE	0x020000 /* Handle is for salvage */
#define	WT_BTREE_SKIP_CKPT	0x040000 /* Handle skipped checkpoint */
#define	WT_BTREE_UPGRADE	0x080000 /* Handle is for upgrade */
#define	WT_BTREE_VERIFY		0x100000 /* Handle is for verify */
	uint32_t flags;
};

/* Flags that make a btree handle special (not for normal use). */
#define	WT_BTREE_SPECIAL_FLAGS	 					\
	(WT_BTREE_ALTER | WT_BTREE_BULK | WT_BTREE_REBALANCE |		\
	WT_BTREE_SALVAGE | WT_BTREE_UPGRADE | WT_BTREE_VERIFY)

/*
 * WT_SALVAGE_COOKIE --
 *	Encapsulation of salvage information for reconciliation.
 */
struct __wt_salvage_cookie {
	uint64_t missing;			/* Initial items to create */
	uint64_t skip;				/* Initial items to skip */
	uint64_t take;				/* Items to take */

	bool	 done;				/* Ignore the rest */
};
