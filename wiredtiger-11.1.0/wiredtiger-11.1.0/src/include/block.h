/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

/*
 * WiredTiger's block manager interface.
 */

/*
 * The file's description is written into the first block of the file, which means we can use an
 * offset of 0 as an invalid offset.
 */
#define WT_BLOCK_INVALID_OFFSET 0

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
 Internally, the block manager uses a data structure called an extent list or a WT_EXTLIST to track file usage.
 An extent list consists of a series of extents (or WT_EXT elements). Each extent uses a file offset and size to
 track a portion of the file.
 */
//跳跃表图解参考https://www.jb51.net/article/199510.htm
//__wt_block_ckpt的alloc avail discard为该类型
//__wt_block_alloc中如果需要从avail中获取指定size的ext, 有了size跳跃表，可以方便快速查找指定长度可用的ext
struct __wt_extlist {
    //赋值见__wt_block_extlist_init    
    char *name; /* Name */

    //跳表中所有elem数据字节数总和，参考__block_append  __block_ext_insert中增加extlist中所有ext保护的数据总数，
    //__block_off_remove减少extlist删除的ext数据长度
    uint64_t bytes;   /* Byte count */
    //__block_off_insert->__block_ext_insert和__block_append中分配ext空间向跳跃表中添加elem,计数自增
    //也就是跳跃表中ext的个数,也就是off跳表中的ext个数
    uint32_t entries; /* Entry count */

    //赋值参考__wt_block_extlist_write，也就是跳表持久化到磁盘的核心元数据信息
    //重启的时候通过checkpoint元数据从__wt_block_ckpt_unpack加载起来对应跳表元数据
    uint32_t objectid; /* Written object ID */
    //也就是记录该el跳表元数据记录到的磁盘位置，赋值参考__wt_block_extlist_write， [offset, size]也就是el跳表元数据在磁盘中位置段
    wt_off_t offset;   /* Written extent offset */
    uint32_t checksum; /* Written extent checksum */
    uint32_t size;     /* Written extent size */

    //决定是否需要维护下面的sz跳跃表
    //WT_BLOCK_CKPT.avail或者WT_BLOCK_CKPT.ckpt_avail才会在__wt_block_extlist_init中设置el->track_size为true
    bool track_size; /* Maintain per-size skiplist */

    //__block_append
    WT_EXT *last; /* Cached last element */

    //__block_ext_insert添加ext到跳表中 参考__block_ext_insert __block_off_remove
    WT_EXT *off[WT_SKIP_MAXDEPTH]; /* Size/offset skiplists */
    //只有在track_size为true的时候才生效__block_ext_insert
    //注意WT_SIZE本身内部还包含两个跳跃表，一个跳表存储sz，一个跳表存储相同sz下面拥有的WT_EXT off
    //使用两个跳表的优势是，先找size跳表，在找off跳表会更快, 参考__block_ext_insert __block_off_remove
    WT_SIZE *sz[WT_SKIP_MAXDEPTH];
};

/*
 * WT_EXT --
 *	Encapsulation of an extent, either allocated or freed within the
 * checkpoint.
 */  //__wt_extlist.off成员为该类型
//__block_ext_prealloc中分配空间, 成员赋值参考__block_append
//管理reconcile拆分后的多个chunk磁盘数据，可以由一个ext管理，也可能你由多个ext管理，参考__block_append
struct __wt_ext {
    //该ext在磁盘中的起始位置
    wt_off_t off;  /* Extent's file offset */
    //该ext的数据长度
    wt_off_t size; /* Extent's Size */

    uint8_t depth; /* Skip list depth */

    /*
     * Variable-length array, sized by the number of skiplist elements. The first depth array
     * entries are the address skiplist elements, the second depth array entries are the size
     * skiplist.
     */
    //注意这里是一个动态的数组
    //这里数组大小不为WT_SKIP_MAXDEPTH的原因是，这里是一个两维的跳表，一个对应size  一个对应off
    //参考__block_ext_insert
    //next数组大小实际上是ext->depth*2，next[0-ext->depth]这部分skip depth对应size跳跃表，把所有__wt_size串起来
    //  next[ext->depth, ext->depth*2]这部分skip depth对应Off跳跃表，代表拥有相同size但是off不相同的所有ext通过这里串起来
    WT_EXT *next[0]; /* Offset, size skiplists */
};

/*
 * WT_SIZE --
 *	Encapsulation of a block size skiplist entry.
 */ //__wt_extlist.sz成员为该类型, 参考__block_ext_insert
struct __wt_size {
    wt_off_t size; /* Size */

    uint8_t depth; /* Skip list depth */

    //链接相同size，但是off起始地址不同的ext对应的跳表，使用两个跳表的优势是，先找size跳表，在找off跳表会更快, 参考__block_ext_insert
    WT_EXT *off[WT_SKIP_MAXDEPTH]; /* Per-size offset skiplist */

    /*
     * We don't use a variable-length array for the size skiplist, we want to be able to use any
     * cached WT_SIZE structure as the head of a list, and we don't know the related WT_EXT
     * structure's depth.
     */
    //链接不同__wt_size的跳表
    WT_SIZE *next[WT_SKIP_MAXDEPTH]; /* Size skiplist */
};

/*
 * WT_EXT_FOREACH --
 *	Walk a block manager skiplist.
 * WT_EXT_FOREACH_OFF --
 *	Walk a block manager skiplist where the WT_EXT.next entries are offset
 * by the depth.
 */
#define WT_EXT_FOREACH(skip, head) \
    for ((skip) = (head)[0]; (skip) != NULL; (skip) = (skip)->next[0])
#define WT_EXT_FOREACH_OFF(skip, head) \
    for ((skip) = (head)[0]; (skip) != NULL; (skip) = (skip)->next[(skip)->depth])

/*
 * Checkpoint cookie: carries a version number as I don't want to rev the schema
 * file version should the default block manager checkpoint format change.
 *
 * Version #1 checkpoint cookie format:
 *	[1] [root addr] [alloc addr] [avail addr] [discard addr]
 *	    [file size] [checkpoint size] [write generation]
 */
#define WT_BM_CHECKPOINT_VERSION 1   /* Checkpoint format version */
#define WT_BLOCK_EXTLIST_MAGIC 71002 /* Identify a list */

/*
 * There are two versions of the extent list blocks: the original, and a second version where
 * current checkpoint information is appended to the avail extent list.
 */
#define WT_BLOCK_EXTLIST_VERSION_ORIG 0 /* Original version */
#define WT_BLOCK_EXTLIST_VERSION_CKPT 1 /* Checkpoint in avail output */

/*
 * Maximum buffer required to store a checkpoint: 1 version byte followed by
 * 14 packed 8B values.
 */
#define WT_BLOCK_CHECKPOINT_BUFFER (1 + 14 * WT_INTPACK64_MAXSIZE)
//官方文档参考https://github.com/wiredtiger/wiredtiger/wiki/Block-Manager-Overview#source-files-in-block-manager
//__wt_block.live为该类型
struct __wt_block_ckpt {
    uint8_t version; /* Version */

//[1702810336:479719][75841:0x7efec67c3800], wt, file:access.wt, WT_SESSION.verify: [WT_VERB_CHECKPOINT][DEBUG_5]:
//access.wt: load: version=1, object ID=0, root=[off: 8192-12288, size: 4096, checksum: 0x7f509c85],
//alloc=[off: 12288-16384, size: 4096, checksum: 0xd8d745a6], avail=[off: 16384-20480, size: 4096, checksum: 0x99ff2904],
//discard=[Empty], file size=73728, checkpoint size=49152

    //__rec_write->__wt_blkcache_write->__bm_checkpoint->__bm_checkpoint
    //如果是因为checkpoint进行reconcile，则写入磁盘后的元数据记录到这几个变量中

    //__wt_rec_row_int把该internal page的ref key及其下面所有子page的磁盘元数据信息写入到一个新的ext持久化
    //配合__wt_ckpt_verbose阅读, 这里存储的就是__wt_rec_row_int封包的internal ref key+该internal page下面的持久化后在磁盘的元数据信息

    //重启的时候从__wt_block_checkpoint_load加载起来, root元数据来源见__wt_rec_row_int
    //root page在磁盘的位置和大小
    uint32_t root_objectid;//实际上写死的为WT_TIERED_OBJECTID_NONE 0，参考__wt_blkcache_open
    wt_off_t root_offset; /* The root */
    uint32_t root_checksum, root_size;

/*
There are three extent lists that are maintained per checkpoint:
    alloc: The file ranges allocated for a given checkpoint.
    avail: The file ranges that are unused and available for allocation.
    discard: The file ranges freed in the current checkpoint.
The alloc and discard extent lists are maintained as a skiplist sorted by file offset.
The avail extent list also maintains an extra skiplist sorted by the extent size to aid with allocating new blocks.
*/  


/*
    第一次生成一个midnight的checkpoint
    for (int i = 0; i < 56; i++) {
        snprintf(key_buf, 512, "keyxxxxxxxxxxxxxxxxxxxxxxxxxkeyxxxxxxxxxxxxxxxxxxxxxxxxxkeyxxxxxxxxxxxxxxxxxxxxxxxxxkeyxxxxxxxxxxxxxxxxxxxxxxxxxkeyxxxxxxxxxxxxxxxxxxxxxxxxxkeyxxxxxxxxxxxxxxxxxxxxxxxxxkeyxxxxxxxxxxxxxxxxxxxxxxxxx_keyxxxxxxxxxxxxxxxxxxxxxxxxxkeyxxxxxxxxxxxxxxxxxxxxxxxxxkeyxxxxxxxxxxxxxxxxxxxxxxxxxkeyxxxxxxxxxxxxxxxxxxxxxxxxxkeyxxxxxxxxxxxxxxxxxxxxxxxxxkeyxxxxxxxxxxxxxxxxxxxxxxxxxkeyxxxxxxxxxxxxxxxxxxxxxxxxx_%d", i);
        cursor->set_key(cursor, i);  
        cursor->set_value(cursor, key_buf);
        error_check(cursor->insert(cursor));
    }
    error_check(session->checkpoint(session, "name=midnight")); 
 
                  ext
      文件头    leafpage1      leafpage2      root    el.alloc     el.avail  
    |________|_____________|______________|________|___________|___________|
    0      4096         28672           32768   36864       40960       45056    
    
    第一次checkpoint的结果是: 
    el.alloc磁盘空间[36864, 40960]存储的是: [4096, 36864]上面的root + 所有leaf page的磁盘元数据。 因此只有获取到alloc中的内容就可以间接的获取到root + 所有leafpage
    el.avail这里实际上存储的内容是[36864, 40960]对应的ext, 可以参考__ckpt_update->__wt_block_extlist_write->__wt_block_off_remove_overlap
    el.alloc和el.avail的元数据存储在wiredtiger.wt中的checkpoint=xxxx中，因此通过wiredtiger.wt中的checkpoint=xxxx就可以获取的el.alloc和el.avail，从而可以间接的获取到磁盘上的所有page信息


    第二次新增一条数据，page也就是有修改，生成同名midnight的checkpoint
    cursor->set_key(cursor, 56);  
    cursor->set_value(cursor, "4444444444444444444");
    error_check(cursor->insert(cursor));
    error_check(session->checkpoint(session, "name=midnight")); 

                  ext
      文件头    leafpage1      leafpage2      root    el.alloc     el.avail  leafpage2-new   new root  el.alloc     el.avail
    |________|_____________|______________|________|___________|___________|_______________|_________|___________|___________|
    0      4096         28672           32768   36864       40960       45056            49152    53248       57344       61440
    \                                                                      /\                                                /
     \                                                                    /  \                                              /         
      \                             第一次checkpoint                     /    \               第二次checkpoint             /

      第二次checkpoint后:
      新的el.alloc磁盘空间[53248, 57344]存储的是:  [4096, 28672] = [4096, 28672] + [45056, 53248] = leafpage1[4096, 28672] + leafpage2-new[45056, 49152] + new root[49152, 53248]

      参考demo: debug_checkpoint理解_ex.c
*/


    //alloc内存中只会记录真实数据page，包括root、 internal、leaf page的ext数据，不包括el持久化的ext数据，参考__wt_block_extlist_write

    //ext信息打印可以配合__wt_ckpt_verbose阅读
    //重启的时候通过checkpoint元数据从__wt_block_ckpt_unpack加载起来对应跳表元数据
    WT_EXTLIST alloc;   /* Extents allocated */ //__wt_block_alloc中分配ext空间，添加到alloc跳跃表中
    //从alloc跳跃表中被删除的offset对应的ext重新添加到avail中，代表的实际上就是磁盘碎片，也就是可重用的空间，参考__wt_block_off_free
    //__wt_block_alloc中如果需要从avail中获取指定size的ext, 有了size跳跃表，可以方便快速查找指定长度可用的ext
    WT_EXTLIST avail;   /* Extents available */
    //从alloc跳跃表中删除某个范围的ext，如果alloc跳跃表中没找到，则这个要删除范围对应的ext添加到discard跳跃表中，参考__wt_block_off_free
    //例如对某个表连续做了两次checkpoint，两次checkpoint期间，某个page做了修改，则第二次得checkpoint会对修改得page重新生成一个ext,并标识
    //  这个修改得page在第一次checkpoint的对应ext为discard，也就是discard对应ext记录的是第二次checkpoint相比第一次checkpoint修改的page对应的ext
    WT_EXTLIST discard; /* Extents discarded */

    //赋值参考__ckpt_update， 也就是block->size，也就是做checkpoint时候的文件大小
    //重启的时候从checkpoint元数据中获取，赋值见__block_ckpt_unpack
    wt_off_t file_size; /* Checkpoint file size */
    //重启的时候从checkpoint元数据中获取，赋值见__block_ckpt_unpack
    //赋值见__ckpt_process，
    //ckpt_size实际上就是真实ext数据空间=file_size - avail空间(也就是磁盘碎片)
    uint64_t ckpt_size; /* Checkpoint byte count */

    //分配空间__ckpt_proces, 赋值见__ckpt_extlist_fblocks
    //把需要释放的checkpoint对应的el再磁盘上的ext元数据添加到ckpt_avail   
    WT_EXTLIST ckpt_avail; /* Checkpoint free'd extents */
    /*
     * Checkpoint archive: the block manager may potentially free a lot of memory from the
     * allocation and discard extent lists when checkpoint completes. Put it off until the
     * checkpoint resolves, that lets the upper btree layer continue eviction sooner.
     */

    //alloc、discard临时赋值给ckpt_alloc、ckpt_discard，因为alloc discard跳表上有很多ext，遍历是否内存非常耗时，因此在外层释放，避免lock过程太长
    //参考__ckpt_process
    WT_EXTLIST ckpt_alloc;   /* Checkpoint archive */
    WT_EXTLIST ckpt_discard; /* Checkpoint archive */
};

/*
 * WT_BM --
 *	Block manager handle, references a single checkpoint in a file.
 */
//__wt_btree_open->__wt_blkcache_open分片空间，__bm_method_set中进行接口定义
//__wt_btree.bm为该类型
struct __wt_bm {
    /* Methods */
    int (*addr_invalid)(WT_BM *, WT_SESSION_IMPL *, const uint8_t *, size_t);
    //__bm_addr_string
    int (*addr_string)(WT_BM *, WT_SESSION_IMPL *, WT_ITEM *, const uint8_t *, size_t);
    //__bm_block_header
    u_int (*block_header)(WT_BM *);
    //__wt_blkcache_write中执行，__bm_checkpoint
    int (*checkpoint)(WT_BM *, WT_SESSION_IMPL *, WT_ITEM *, WT_CKPT *, bool);
    int (*checkpoint_last)(WT_BM *, WT_SESSION_IMPL *, char **, char **, WT_ITEM *);
    //__bm_checkpoint_load
    int (*checkpoint_load)(
      WT_BM *, WT_SESSION_IMPL *, const uint8_t *, size_t, uint8_t *, size_t *, bool);
    //__bm_checkpoint_resolve
    int (*checkpoint_resolve)(WT_BM *, WT_SESSION_IMPL *, bool);
    //__bm_checkpoint_start
    int (*checkpoint_start)(WT_BM *, WT_SESSION_IMPL *);
    int (*checkpoint_unload)(WT_BM *, WT_SESSION_IMPL *);
    int (*close)(WT_BM *, WT_SESSION_IMPL *);
    int (*compact_end)(WT_BM *, WT_SESSION_IMPL *);
    int (*compact_page_rewrite)(WT_BM *, WT_SESSION_IMPL *, uint8_t *, size_t *, bool *);
    int (*compact_page_skip)(WT_BM *, WT_SESSION_IMPL *, const uint8_t *, size_t, bool *);
    int (*compact_skip)(WT_BM *, WT_SESSION_IMPL *, bool *);
    void (*compact_progress)(WT_BM *, WT_SESSION_IMPL *, u_int *);
    int (*compact_start)(WT_BM *, WT_SESSION_IMPL *);
    int (*corrupt)(WT_BM *, WT_SESSION_IMPL *, const uint8_t *, size_t);
    //__bm_free
    int (*free)(WT_BM *, WT_SESSION_IMPL *, const uint8_t *, size_t);
    bool (*is_mapped)(WT_BM *, WT_SESSION_IMPL *);
    int (*map_discard)(WT_BM *, WT_SESSION_IMPL *, void *, size_t);
    //__bm_read
    int (*read)(WT_BM *, WT_SESSION_IMPL *, WT_ITEM *, const uint8_t *, size_t);
    int (*salvage_end)(WT_BM *, WT_SESSION_IMPL *);
    //__bm_salvage_next
    int (*salvage_next)(WT_BM *, WT_SESSION_IMPL *, uint8_t *, size_t *, bool *);
    //__bm_salvage_start
    int (*salvage_start)(WT_BM *, WT_SESSION_IMPL *);
    //__bm_salvage_valid
    int (*salvage_valid)(WT_BM *, WT_SESSION_IMPL *, uint8_t *, size_t, bool);
    int (*size)(WT_BM *, WT_SESSION_IMPL *, wt_off_t *);
    //__bm_stat
    int (*stat)(WT_BM *, WT_SESSION_IMPL *, WT_DSRC_STATS *stats);
    int (*switch_object)(WT_BM *, WT_SESSION_IMPL *, uint32_t);
    //__bm_sync,  __wt_sync_file __wt_checkpoint_sync调用
    int (*sync)(WT_BM *, WT_SESSION_IMPL *, bool);
    //__bm_verify_addr
    int (*verify_addr)(WT_BM *, WT_SESSION_IMPL *, const uint8_t *, size_t);
    int (*verify_end)(WT_BM *, WT_SESSION_IMPL *);
    //__bm_verify_start
    int (*verify_start)(WT_BM *, WT_SESSION_IMPL *, WT_CKPT *, const char *[]);
    //__bm_write
    int (*write)(WT_BM *, WT_SESSION_IMPL *, WT_ITEM *, uint8_t *, size_t *, bool, bool);
    //__bm_write_size
    int (*write_size)(WT_BM *, WT_SESSION_IMPL *, size_t *);

    //__wt_block_open
    WT_BLOCK *block; /* Underlying file */

    //只有WT_BTREE_READONLY状态设置，才会进行map，赋值见__bm_checkpoint_load
    void *map; /* Mapped region */
    size_t maplen;
    void *mapped_cookie;

    /*
     * There's only a single block manager handle that can be written, all others are checkpoints.
     */
    bool is_live; /* The live system */
};

/*
 * WT_BLOCK --
 *	Block manager handle, references a single file.
 */
//分配空间和赋值见__wt_block_open， btree->bm  __wt_bm.block为该类型
struct __wt_block {
    //例如access.wt
    const char *name;  /* Name */
    //__wt_block_open中赋值 //实际上写死的为WT_TIERED_OBJECTID_NONE 0，参考__wt_blkcache_open
    uint32_t objectid; /* Object id */
    uint32_t ref;      /* References */

//   TAILQ_ENTRY(__wt_block) q;     /* Linked list of handles */
//   TAILQ_ENTRY(__wt_block) hashq; /* Hashed list of handles */
    bool linked;

    WT_SPINLOCK cache_lock;   /* Block cache layer lock */
    WT_BLOCK **related;       /* Related objects */
    size_t related_allocated; /* Size of related object array */
    u_int related_next;       /* Next open slot */

    WT_FH *fh;            /* Backing file handle */
    //代表当前已经写入到文件末尾位置，也就是文件大小, 没写入一块数据就自增，参考__block_extend
    wt_off_t size;        /* File size */
    //file_extend配置，默认为0，所以extend_size也就是是当前block size
    wt_off_t extend_size; /* File extended size */
    //file_extend配置，默认为0
    wt_off_t extend_len;  /* File extend chunk size */

    bool close_on_checkpoint;   /* Close the handle after the next checkpoint */
    bool created_during_backup; /* Created during incremental backup */

    /* Configuration information, set when the file is opened. */
    //https://source.wiredtiger.com/develop/arch-block.html
    //默认为best, block_allocation配置，也就是默认为0
    uint32_t allocfirst; /* Allocation is first-fit */
    //allocation_size配置，默认4K
    //getconf PAGESIZE获取操作系统page
    uint32_t allocsize;  /* Allocation size */
    size_t os_cache;     /* System buffer cache flush max */
    //os_cache_max配置，默认为0， 触发后通过posix_fadvise清理系统中的文件缓存
    size_t os_cache_max;
    //os_cache_dirty_max配置，默认为0, 也就是没写入多少数据，就进行强制__wt_fsync刷盘
    //触发后通过__wt_fsync刷盘
    size_t os_cache_dirty_max;

    u_int block_header; /* Header length */

    /*
     * There is only a single checkpoint in a file that can be written; stored here, only accessed
     * by one WT_BM handle.
     */
    WT_SPINLOCK live_lock; /* Live checkpoint lock */
    //WT_BLOCK->live
    WT_BLOCK_CKPT live;    /* Live checkpoint */  //__wt_block.live
    bool live_open;        /* Live system is open */
    enum {                 /* Live checkpoint status */
        WT_CKPT_NONE = 0,
        //__wt_block_checkpoint_start
        WT_CKPT_INPROGRESS,
        WT_CKPT_PANIC_ON_FAILURE,
        //__wt_block_salvage_start中赋值
        WT_CKPT_SALVAGE
    } ckpt_state;

    WT_CKPT *final_ckpt; /* Final live checkpoint write */

    /* Compaction support */
    int compact_pct_tenths;           /* Percent to compact */
    uint64_t compact_pages_rewritten; /* Pages rewritten */
    uint64_t compact_pages_reviewed;  /* Pages reviewed */
    uint64_t compact_pages_skipped;   /* Pages skipped */

    /* Salvage support */
    wt_off_t slvg_off; /* Salvage file offset */

    /* Verification support */
    bool verify;             /* If performing verification */
    bool verify_layout;      /* Print out file layout information */
    //__wt_block_verify_start strict配置项，默认true
    bool verify_strict;      /* Fail hard on any error */
    wt_off_t verify_size;    /* Checkpoint's file size */
    WT_EXTLIST verify_alloc; /* Verification allocation list */
    //也就是有多少个block->allocsize长度大小
    uint64_t frags;          /* Maximum frags in the file */
    //赋值参考__verify_filefrag_add
    uint8_t *fragfile;       /* Per-file frag tracking list */
    //参考__wt_verify_ckpt_load  //以4092为单位，把verify_alloc中的ext对应的fragckpt[]位置位
    uint8_t *fragckpt;       /* Per-checkpoint frag tracking list */

   //yang add change 挪动位置
   TAILQ_ENTRY(__wt_block) q;     /* Linked list of handles */
   TAILQ_ENTRY(__wt_block) hashq; /* Hashed list of handles */
};

/*
 * WT_BLOCK_DESC --
 *	The file's description.
 The layout of a .wt file consists of a file description WT_BLOCK_DESC which always occupies the first block, followed by a
  set of on-disk pages. The file description contains metadata about the file such as the WiredTiger major and minor version,
  a magic number, and a checksum of the block contents. This information is used to verify that the file is a legitimate WiredTiger
  data file with a compatible WiredTiger version, and that its contents are not corrupted.
 */
//先把该结构写入数据文件最前面 __wt_desc_write， 也就是一个表对应wt文件的头部4K记录的信息，头部异常则读取wt文件会在__desc_read报错
//可以通过./wt -C "verbose=[all:5]" -R  salvage -F file:collection-9--6421956552934611461.wt进行强制数据修复
struct __wt_block_desc {
#define WT_BLOCK_MAGIC 120897 
    uint32_t magic; /* 00-03: Magic number */
#define WT_BLOCK_MAJOR_VERSION 1
    uint16_t majorv; /* 04-05: Major version */
#define WT_BLOCK_MINOR_VERSION 0
    uint16_t minorv; /* 06-07: Minor version */

    uint32_t checksum; /* 08-11: Description block checksum */

    uint32_t unused; /* 12-15: Padding */
};
/*
 * WT_BLOCK_DESC_SIZE is the expected structure size -- we verify the build to ensure the compiler
 * hasn't inserted padding (padding won't cause failure, we reserve the first allocation-size block
 * of the file for this information, but it would be worth investigation, regardless).
 */
#define WT_BLOCK_DESC_SIZE 16

/*
 * __wt_block_desc_byteswap --
 *     Handle big- and little-endian transformation of a description block.
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
 https://github.com/wiredtiger/wiredtiger/wiki/Reconciliation-overview

 The page header is followed by a "block header". In WiredTiger each page is a block, and it is possible
   to plug in different "block managers" that manage the transition of pages to and from disk.

 参考https://source.wiredtiger.com/develop/arch-block.html
 */
//wt文件头部4K magic  checksum检查在__desc_read

//WT_PAGE_HEADER(__wt_page_header)在__rec_split_write_header中完成赋值，对应内存位置见WT_BLOCK_HEADER_REF
//WT_BLOCK_HEADER(__wt_block_header)赋值在写磁盘完成后，在__block_write_off赋值,对应内存偏移见WT_PAGE_HEADER_BYTE_SIZE
//如果配置了压缩，则__wt_page_header记录的是压缩前的数据信息,__wt_block_header记录的是压缩后的数据信息
struct __wt_block_header {
    /*
     * We write the page size in the on-disk page header because it makes salvage easier. (If we
     * don't know the expected page length, we'd have to read increasingly larger chunks from the
     * file until we find one that checksums, and that's going to be harsh given WiredTiger's
     * potentially large page sizes.)
     */
    uint32_t disk_size; /* 00-03: on-disk page size */

    /*
     * Page checksums are stored in two places. First, the page checksum is written within the
     * internal page that references it as part of the address cookie. This is done to improve the
     * chances of detecting not only disk corruption but other bugs (for example, overwriting a page
     * with another valid page image). Second, a page's checksum is stored in the disk header. This
     * is for salvage, so salvage knows it has found a page that may be useful.
     */
    //__block_write_off中赋值
    uint32_t checksum; /* 04-07: checksum */

/*
 * No automatic generation: flag values cannot change, they're written to disk.
 */
#define WT_BLOCK_DATA_CKSUM 0x1u /* Block data is part of the checksum */
    uint8_t flags;               /* 08: flags */

    /*
     * End the structure with 3 bytes of padding: it wastes space, but it leaves the structure
     * 32-bit aligned and having a few bytes to play with in the future can't hurt.
     */
    uint8_t unused[3]; /* 09-11: unused padding */
};
/*
 * WT_BLOCK_HEADER_SIZE is the number of bytes we allocate for the structure: if the compiler
 * inserts padding it will break the world.
 */
//也就是上面的__wt_block_header结构长度，__block_write_off中通过blk填充
#define WT_BLOCK_HEADER_SIZE 12

/*
 * __wt_block_header_byteswap_copy --
 *     Handle big- and little-endian transformation of a header block, copying from a source to a
 *     target.
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
 *     Handle big- and little-endian transformation of a header block.
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
#define WT_BLOCK_HEADER_BYTE_SIZE (WT_PAGE_HEADER_SIZE + WT_BLOCK_HEADER_SIZE)
#define WT_BLOCK_HEADER_BYTE(dsk) ((void *)((uint8_t *)(dsk) + WT_BLOCK_HEADER_BYTE_SIZE))

/*
 * We don't compress or encrypt the block's WT_PAGE_HEADER or WT_BLOCK_HEADER structures because we
 * need both available with decompression or decryption. We use the WT_BLOCK_HEADER checksum and
 * on-disk size during salvage to figure out where the blocks are, and we use the WT_PAGE_HEADER
 * in-memory size during decompression and decryption to know how large a target buffer to allocate.
 * We can only skip the header information when doing encryption, but we skip the first 64B when
 * doing compression; a 64B boundary may offer better alignment for the underlying compression
 * engine, and skipping 64B shouldn't make any difference in terms of compression efficiency.
 */
#define WT_BLOCK_COMPRESS_SKIP 64
#define WT_BLOCK_ENCRYPT_SKIP WT_BLOCK_HEADER_BYTE_SIZE

/*
 * __wt_block_header --
 *     Return the size of the block-specific header.
 */
static inline u_int
__wt_block_header(WT_BLOCK *block)
{
    WT_UNUSED(block);

    return ((u_int)WT_BLOCK_HEADER_SIZE);
}
