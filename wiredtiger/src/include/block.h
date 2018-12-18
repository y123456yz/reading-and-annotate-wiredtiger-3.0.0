/*-
 * Copyright (c) 2014-2017 MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

/*
 * WiredTiger's block manager interface.
 */

/*
 * The file's description is written into the first block of the file, which
 * means we can use an offset of 0 as an invalid offset.
 */
#define	WT_BLOCK_INVALID_OFFSET		0

/*
 * The block manager maintains three per-checkpoint extent lists:
 *	alloc:	 the extents allocated in this checkpoint
 *	avail:	 the extents available for allocation
 *	discard: the extents freed in this checkpoint
 *
 * An extent list is based on two skiplists: first, a by-offset list linking
 * WT_EXT elements and sorted by file offset (low-to-high), second, a by-size
 * list linking WT_SIZE elements and sorted by chunk size (low-to-high).
 *
 * Additionally, each WT_SIZE element on the by-size has a skiplist of its own,
 * linking WT_EXT elements and sorted by file offset (low-to-high).  This list
 * has an entry for extents of a particular size.
 *
 * The trickiness is each individual WT_EXT element appears on two skiplists.
 * In order to minimize allocation calls, we allocate a single array of WT_EXT
 * pointers at the end of the WT_EXT structure, for both skiplists, and store
 * the depth of the skiplist in the WT_EXT structure.  The skiplist entries for
 * the offset skiplist start at WT_EXT.next[0] and the entries for the size
 * skiplist start at WT_EXT.next[WT_EXT.depth].
 *
 * One final complication: we only maintain the per-size skiplist for the avail
 * list, the alloc and discard extent lists are not searched based on size.
 */

/*
 * WT_EXTLIST --
 *	An extent list.
 */
struct __wt_extlist {
	char *name;				/* Name */
    //__block_ext_insert  __block_append中赋值   ext中存放数据的总长度
	uint64_t bytes;				/* Byte count */
	uint32_t entries;			/* Entry count */

	wt_off_t offset;			/* Written extent offset */
	uint32_t checksum;			/* Written extent checksum */
	uint32_t size;				/* Written extent size */

	bool	 track_size;			/* Maintain per-size skiplist */

    //指向最新的ext,见__block_append
	WT_EXT	*last;				/* Cached last element */

	WT_EXT	*off[WT_SKIP_MAXDEPTH];		/* Size/offset skiplists */
	WT_SIZE *sz[WT_SKIP_MAXDEPTH];
};

/*
 * WT_EXT --
 *	Encapsulation of an extent, either allocated or freed within the
 * checkpoint.
 */ //__block_ext_alloc中分配空间  
struct __wt_ext { //每个ext对应磁盘上对应表文件从off开始的size字节数据
    //也就是新的size数据从off位置写入
	wt_off_t  off;				/* Extent's file offset */
	//该ext中需要写的数据长度，见__block_append
	wt_off_t  size;				/* Extent's Size */

	uint8_t	 depth;				/* Skip list depth */

	/*
	 * Variable-length array, sized by the number of skiplist elements.
	 * The first depth array entries are the address skiplist elements,
	 * the second depth array entries are the size skiplist.
	 */
	WT_EXT	*next[0];			/* Offset, size skiplists */
};

/*
 * WT_SIZE --
 *	Encapsulation of a block size skiplist entry.
 */
struct __wt_size {
	wt_off_t size;				/* Size */

	uint8_t	 depth;				/* Skip list depth */

	WT_EXT	*off[WT_SKIP_MAXDEPTH];		/* Per-size offset skiplist */

	/*
	 * We don't use a variable-length array for the size skiplist, we want
	 * to be able to use any cached WT_SIZE structure as the head of a list,
	 * and we don't know the related WT_EXT structure's depth.
	 */
	WT_SIZE *next[WT_SKIP_MAXDEPTH];	/* Size skiplist */
};

/*
 * WT_EXT_FOREACH --
 *	Walk a block manager skiplist.
 * WT_EXT_FOREACH_OFF --
 *	Walk a block manager skiplist where the WT_EXT.next entries are offset
 * by the depth.
 */
#define	WT_EXT_FOREACH(skip, head)					\
	for ((skip) = (head)[0];					\
	    (skip) != NULL; (skip) = (skip)->next[0])
#define	WT_EXT_FOREACH_OFF(skip, head)					\
	for ((skip) = (head)[0];					\
	    (skip) != NULL; (skip) = (skip)->next[(skip)->depth])

/*
 * Checkpoint cookie: carries a version number as I don't want to rev the schema
 * file version should the default block manager checkpoint format change.
 *
 * Version #1 checkpoint cookie format:
 *	[1] [root addr] [alloc addr] [avail addr] [discard addr]
 *	    [file size] [checkpoint size] [write generation]
 */
#define	WT_BM_CHECKPOINT_VERSION	1	/* Checkpoint format version */
#define	WT_BLOCK_EXTLIST_MAGIC		71002	/* Identify a list */
//读取checkpoint文件赋值见__wt_block_checkpoint_load  __block_buffer_to_ckpt
//__wt_block.live成员
struct __wt_block_ckpt { //__block_buffer_to_ckpt
	uint8_t	 version;			/* Version */

	wt_off_t root_offset;			/* The root */
	uint32_t root_checksum, root_size;

	WT_EXTLIST alloc;			/* Extents allocated */
	WT_EXTLIST avail;			/* Extents available */
	WT_EXTLIST discard;			/* Extents discarded */

	wt_off_t   file_size;			/* Checkpoint file size */
	uint64_t   ckpt_size;			/* Checkpoint byte count */

	WT_EXTLIST ckpt_avail;			/* Checkpoint free'd extents */

	/*
	 * Checkpoint archive: the block manager may potentially free a lot of
	 * memory from the allocation and discard extent lists when checkpoint
	 * completes.  Put it off until the checkpoint resolves, that lets the
	 * upper btree layer continue eviction sooner.
	 */
	WT_EXTLIST ckpt_alloc;			/* Checkpoint archive */
	WT_EXTLIST ckpt_discard;		/* Checkpoint archive */
};

/*
 * WT_BM --
 *	Block manager handle, references a single checkpoint in a file.
 */
//赋值见__bm_method_set  __wt_block_manager_open
struct __wt_bm {//bm代表block manage
						/* Methods */
	int (*addr_invalid)
	    (WT_BM *, WT_SESSION_IMPL *, const uint8_t *, size_t);
	int (*addr_string)
	    (WT_BM *, WT_SESSION_IMPL *, WT_ITEM *, const uint8_t *, size_t);
	u_int (*block_header)(WT_BM *);
	int (*checkpoint)
	    (WT_BM *, WT_SESSION_IMPL *, WT_ITEM *, WT_CKPT *, bool);
	int (*checkpoint_load)(WT_BM *, WT_SESSION_IMPL *,
	    const uint8_t *, size_t, uint8_t *, size_t *, bool);
	int (*checkpoint_resolve)(WT_BM *, WT_SESSION_IMPL *, bool);
	int (*checkpoint_start)(WT_BM *, WT_SESSION_IMPL *);
	int (*checkpoint_unload)(WT_BM *, WT_SESSION_IMPL *);
	int (*close)(WT_BM *, WT_SESSION_IMPL *);
	int (*compact_end)(WT_BM *, WT_SESSION_IMPL *);
	int (*compact_page_skip)
	    (WT_BM *, WT_SESSION_IMPL *, const uint8_t *, size_t, bool *);
	int (*compact_skip)(WT_BM *, WT_SESSION_IMPL *, bool *);
	int (*compact_start)(WT_BM *, WT_SESSION_IMPL *);
	int (*free)(WT_BM *, WT_SESSION_IMPL *, const uint8_t *, size_t);
	bool (*is_mapped)(WT_BM *, WT_SESSION_IMPL *);
	int (*map_discard)(WT_BM *, WT_SESSION_IMPL *, void *, size_t);
	int (*preload)(WT_BM *, WT_SESSION_IMPL *, const uint8_t *, size_t);
	int (*read)
	    (WT_BM *, WT_SESSION_IMPL *, WT_ITEM *, const uint8_t *, size_t);
	int (*salvage_end)(WT_BM *, WT_SESSION_IMPL *);
	int (*salvage_next)
	    (WT_BM *, WT_SESSION_IMPL *, uint8_t *, size_t *, bool *);
	int (*salvage_start)(WT_BM *, WT_SESSION_IMPL *);
	int (*salvage_valid)
	    (WT_BM *, WT_SESSION_IMPL *, uint8_t *, size_t, bool);
	int (*size)(WT_BM *, WT_SESSION_IMPL *, wt_off_t *);
	int (*stat)(WT_BM *, WT_SESSION_IMPL *, WT_DSRC_STATS *stats);
	int (*sync)(WT_BM *, WT_SESSION_IMPL *, bool);
	int (*verify_addr)(WT_BM *, WT_SESSION_IMPL *, const uint8_t *, size_t);
	int (*verify_end)(WT_BM *, WT_SESSION_IMPL *);
	int (*verify_start)
	    (WT_BM *, WT_SESSION_IMPL *, WT_CKPT *, const char *[]);
	int (*write) (WT_BM *,
	    WT_SESSION_IMPL *, WT_ITEM *, uint8_t *, size_t *, bool, bool);
	int (*write_size)(WT_BM *, WT_SESSION_IMPL *, size_t *);

    //__bm_checkpoint_load中做mmap操作
	WT_BLOCK *block;			/* Underlying file */

    //mmap文件内存映射对应的地址和文件长度，见__bm_checkpoint_load
	void	*map;				/* Mapped region */
	size_t	 maplen;
	void	*mapped_cookie;

	/*
	 * There's only a single block manager handle that can be written, all
	 * others are checkpoints.
	 */ //__bm_checkpoint_load中赋值
	bool is_live;				/* The live system */
};

/*
 * WT_BLOCK --
 *	Block manager handle, references a single file.
 */
//block创建见__wt_block_open  
//__bm_checkpoint_load中做mmap操作  
//见__wt_block_alloc   
//WT_BTREE和__wt_block文件通过__wt_btree_open->__wt_block_manager_open关联
struct __wt_block {//一个xx.wt表文件对应一个block
    //一个文件对应一个block,见__wt_block_open
	const char *name;		/* Name */
	uint64_t name_hash;		/* Hash of name */

	WT_FH	*fh;			/* Backing file handle */
	wt_off_t size;			/* File size */
	wt_off_t extend_size;		/* File extended size */
	wt_off_t extend_len;		/* File extend chunk size */

	/* Configuration information, set when the file is opened. */
	/*从文件开始处进行写的标识*/	
	uint32_t allocfirst;		/* Allocation is first-fit */
	/*文件写入对齐的长度*/
	uint32_t allocsize;		/* Allocation size */
	/*当前block中在os page cache中的数据字节数*/
	size_t	 os_cache;		/* System buffer cache flush max */
	/*操作系统对文件最大的page cache的字节数  os_cache_max配置 */
	size_t	 os_cache_max;
	/*当前脏数据的字节数*/
	size_t	 os_cache_dirty;	/* System buffer cache write max */
	size_t	 os_cache_dirty_max;  /*允许最大的脏数据字节数*/
	
    /*block header的长度*/
	u_int	 block_header;		/* Header length */

	/*
	 * There is only a single checkpoint in a file that can be written.  The
	 * information could logically live in the WT_BM structure, but then we
	 * would be re-creating it every time we opened a new checkpoint and I'd
	 * rather not do that.  So, it's stored here, only accessed by one WT_BM
	 * handle.
	 */
	/*对live的保护锁*/
	WT_SPINLOCK	live_lock;	/* Live checkpoint lock */
	/*checkpoint的详细信息*/
	WT_BLOCK_CKPT	live;		/* Live checkpoint */
#ifdef HAVE_DIAGNOSTIC
	bool		live_open;	/* Live system is open */
#endif

	/* Live checkpoint status */
	enum { 
	    WT_CKPT_NONE=0, 
	    WT_CKPT_INPROGRESS, //__wt_block_checkpoint_start中赋值
	    WT_CKPT_PANIC_ON_FAILURE, 
	    WT_CKPT_SALVAGE 
    } ckpt_state;

				/* Compaction support */
	int	 compact_pct_tenths;	/* Percent to compact */
	uint64_t compact_pages_reviewed;/* Pages reviewed */
	uint64_t compact_pages_skipped;	/* Pages skipped */
	uint64_t compact_pages_written;	/* Pages rewritten */

				/* Salvage support */
	wt_off_t	slvg_off;	/* Salvage file offset */

				/* Verification support */
	bool	   verify;		/* If performing verification */
	bool	   verify_layout;	/* Print out file layout information */
	bool	   verify_strict;	/* Fail hard on any error */
	wt_off_t   verify_size;		/* Checkpoint's file size */
	WT_EXTLIST verify_alloc;	/* Verification allocation list */
	uint64_t   frags;		/* Maximum frags in the file */
	uint8_t   *fragfile;		/* Per-file frag tracking list */
	uint8_t   *fragckpt;		/* Per-checkpoint frag tracking list */

    //yang change
	/* A list of block manager handles, sharing a file descriptor. */
	uint32_t ref;			/* References */
	TAILQ_ENTRY(__wt_block) q;	/* Linked list of handles */
	TAILQ_ENTRY(__wt_block) hashq;	/* Hashed list of handles */
};

/*
 * WT_BLOCK_DESC --
 *	The file's description.
 */
struct __wt_block_desc {
#define	WT_BLOCK_MAGIC		120897
	uint32_t magic;			/* 00-03: Magic number */
#define	WT_BLOCK_MAJOR_VERSION	1
	uint16_t majorv;		/* 04-05: Major version */
#define	WT_BLOCK_MINOR_VERSION	0
	uint16_t minorv;		/* 06-07: Minor version */

	uint32_t checksum;		/* 08-11: Description block checksum */

	uint32_t unused;		/* 12-15: Padding */
};
/*
 * WT_BLOCK_DESC_SIZE is the expected structure size -- we verify the build to
 * ensure the compiler hasn't inserted padding (padding won't cause failure,
 * we reserve the first allocation-size block of the file for this information,
 * but it would be worth investigation, regardless).
 */
#define	WT_BLOCK_DESC_SIZE		16

/*
 * __wt_block_desc_byteswap --
 *	Handle big- and little-endian transformation of a description block.
 */
static inline void
__wt_block_desc_byteswap(WT_BLOCK_DESC *desc)
{
#ifdef WORDS_BIGENDIAN
	desc->magic = __wt_bswap32(desc->magic);
	desc->majorv = __wt_bswap16(desc->majorv);
	desc->minorv = __wt_bswap16(desc->minorv);
	desc->checksum = __wt_bswap32(desc->checksum);
#else
	WT_UNUSED(desc);
#endif
}

/*
 * WT_BLOCK_HEADER --
 *	Blocks have a common header, a WT_PAGE_HEADER structure followed by a
 * block-manager specific structure: WT_BLOCK_HEADER is WiredTiger's default.
 */ //赋值见__block_write_off
struct __wt_block_header {
	/*
	 * We write the page size in the on-disk page header because it makes
	 * salvage easier.  (If we don't know the expected page length, we'd
	 * have to read increasingly larger chunks from the file until we find
	 * one that checksums, and that's going to be harsh given WiredTiger's
	 * potentially large page sizes.)
	 */
	uint32_t disk_size;		/* 00-03: on-disk page size */

	/*
	 * Page checksums are stored in two places.  First, the page checksum
	 * is written within the internal page that references it as part of
	 * the address cookie.  This is done to improve the chances of detecting
	 * not only disk corruption but other bugs (for example, overwriting a
	 * page with another valid page image).  Second, a page's checksum is
	 * stored in the disk header.  This is for salvage, so salvage knows it
	 * has found a page that may be useful.
	 */
	uint32_t checksum;		/* 04-07: checksum */

#define	WT_BLOCK_DATA_CKSUM	0x01	/* Block data is part of the checksum */
	uint8_t flags;			/* 08: flags */

	/*
	 * End the structure with 3 bytes of padding: it wastes space, but it
	 * leaves the structure 32-bit aligned and having a few bytes to play
	 * with in the future can't hurt.
	 */
	uint8_t unused[3];		/* 09-11: unused padding */
};
/*
 * WT_BLOCK_HEADER_SIZE is the number of bytes we allocate for the structure: if
 * the compiler inserts padding it will break the world.
 */
#define	WT_BLOCK_HEADER_SIZE		12

/*
 * __wt_block_header_byteswap_copy --
 *	Handle big- and little-endian transformation of a header block,
 * copying from a source to a target.
 */
static inline void
__wt_block_header_byteswap_copy(WT_BLOCK_HEADER *from, WT_BLOCK_HEADER *to)
{
	*to = *from;
#ifdef WORDS_BIGENDIAN
	to->disk_size = __wt_bswap32(from->disk_size);
	to->checksum = __wt_bswap32(from->checksum);
#endif
}

/*
 * __wt_block_header_byteswap --
 *	Handle big- and little-endian transformation of a header block.
 */
static inline void
__wt_block_header_byteswap(WT_BLOCK_HEADER *blk)
{
#ifdef WORDS_BIGENDIAN
	__wt_block_header_byteswap_copy(blk, blk);
#else
	WT_UNUSED(blk);
#endif
}

/*
 * WT_BLOCK_HEADER_BYTE
 * WT_BLOCK_HEADER_BYTE_SIZE --
 *	The first usable data byte on the block (past the combined headers).
 */
#define	WT_BLOCK_HEADER_BYTE_SIZE					\
	(WT_PAGE_HEADER_SIZE + WT_BLOCK_HEADER_SIZE)
#define	WT_BLOCK_HEADER_BYTE(dsk)					\
	((void *)((uint8_t *)(dsk) + WT_BLOCK_HEADER_BYTE_SIZE))

/*
 * We don't compress or encrypt the block's WT_PAGE_HEADER or WT_BLOCK_HEADER
 * structures because we need both available with decompression or decryption.
 * We use the WT_BLOCK_HEADER checksum and on-disk size during salvage to
 * figure out where the blocks are, and we use the WT_PAGE_HEADER in-memory
 * size during decompression and decryption to know how large a target buffer
 * to allocate. We can only skip the header information when doing encryption,
 * but we skip the first 64B when doing compression; a 64B boundary may offer
 * better alignment for the underlying compression engine, and skipping 64B
 * shouldn't make any difference in terms of compression efficiency.
 */
#define	WT_BLOCK_COMPRESS_SKIP	64
#define	WT_BLOCK_ENCRYPT_SKIP	WT_BLOCK_HEADER_BYTE_SIZE

/*
 * __wt_block_header --
 *	Return the size of the block-specific header.
 */
static inline u_int
__wt_block_header(WT_BLOCK *block)
{
	WT_UNUSED(block);

	return ((u_int)WT_BLOCK_HEADER_SIZE);
}
