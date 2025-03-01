/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#define WT_RECNO_OOB 0 /* Illegal record number */

/* AUTOMATIC FLAG VALUE GENERATION START 0 */
#define WT_READ_CACHE 0x0001u
#define WT_READ_IGNORE_CACHE_SIZE 0x0002u
#define WT_READ_NOTFOUND_OK 0x0004u
#define WT_READ_NO_GEN 0x0008u
#define WT_READ_NO_SPLIT 0x0010u
#define WT_READ_NO_WAIT 0x0020u
#define WT_READ_PREV 0x0040u
#define WT_READ_RESTART_OK 0x0080u
#define WT_READ_SKIP_DELETED 0x0100u
#define WT_READ_SKIP_INTL 0x0200u
#define WT_READ_TRUNCATE 0x0400u
#define WT_READ_VISIBLE_ALL 0x0800u
#define WT_READ_WONT_NEED 0x1000u
/* AUTOMATIC FLAG VALUE GENERATION STOP 32 */

/* AUTOMATIC FLAG VALUE GENERATION START 0 */
#define WT_REC_APP_EVICTION_SNAPSHOT 0x001u
#define WT_REC_CALL_URGENT 0x002u
#define WT_REC_CHECKPOINT 0x004u
#define WT_REC_CHECKPOINT_RUNNING 0x008u
#define WT_REC_CLEAN_AFTER_REC 0x010u
//__evict_reconcile  __wt_evict_file中置位
#define WT_REC_EVICT 0x020u
//__evict_reconcile中如果是leaf page设置该标识, __wt_sync_file也会设置
#define WT_REC_HS 0x040u
#define WT_REC_IN_MEMORY 0x080u
//__evict_update_work 例如如果已使用内存占比总内存不超过(target 80% + trigger 95%)配置的一半，则设置标识WT_CACHE_EVICT_SCRUB，
//  说明reconcile的适合可以内存拷贝一份page数据存入image

//一般reconcile都会拥有该标识，见__evict_reconcile
#define WT_REC_SCRUB 0x100u
#define WT_REC_VISIBILITY_ERR 0x200u
#define WT_REC_VISIBLE_ALL 0x400u
/* AUTOMATIC FLAG VALUE GENERATION STOP 32 */

/*
 * WT_PAGE_HEADER --
 *	Blocks have a common header, a WT_PAGE_HEADER structure followed by a
 * block-manager specific structure.
 1. A page header followed by a series of key/value pairs. The page header is broken into several sub-headers.
 2. The page header is followed by a "block header". In WiredTiger each page is a block, and it is possible to plug
     in different "block managers" that manage the transition of pages to and from disk. The default block header is
     defined in src/include/block.h in the __wt_block_header structure.

     Pages consist of a header (WT_PAGE_HEADER and WT_BLOCK_HEADER) followed by a variable number of cells, which encode keys,
 values or addresses (see cell.h). The page header WT_PAGE_HEADER consists of information such as the column-store record number,
 write generation (required for ordering pages in time), in-memory size, cell count (the number of cells on the page), data length
 (the overflow record length), and the page type. This is immediately followed by the block header WT_BLOCK_HEADER which contains
 block-manager specific information such as flags and version.
 参考reconcile官方文档:https://github.com/wiredtiger/wiredtiger/wiki/Reconciliation-overview
 */

//WT_PAGE_HEADER(__wt_page_header)在__rec_split_write_header中完成赋值，对应内存位置见WT_BLOCK_HEADER_REF
//WT_BLOCK_HEADER(__wt_block_header)赋值在写磁盘完成后，在__block_write_off赋值,对应内存偏移见WT_PAGE_HEADER_BYTE_SIZE
//如果配置了压缩，则__wt_page_header记录的是压缩前的数据信息,__wt_block_header记录的是压缩后的数据信息

//wt文件头部4K magic  checksum检查在__desc_read

 
//一个page数据在磁盘中连续空间内容: WT_BLOCK_HEADER + __wt_page_header +  WT_CELL
//分配空间和赋值可以参考__wt_rec_cell_build_ovfl  __rec_split_write_header
//page->dsk为该类型
struct __wt_page_header {
    /*
     * The record number of the first record of the page is stored on disk so we can figure out
     * where the column-store leaf page fits into the key space during salvage.
     */ //A uint64_t record number, used by column stores (since they don't maintain keys internally)
    uint64_t recno; /* 00-07: column-store starting recno */ //行存储该内容为WT_RECNO_OOB

    /*
     * We maintain page write-generations in the non-transactional case as that's how salvage can
     * determine the most recent page between pages overlapping the same key range.
     */
    //__rec_set_page_write_gen
    uint64_t write_gen; /* 08-15: write generation */

    /*
     * The page's in-memory size isn't rounded or aligned, it's the actual number of bytes the
     * disk-image consumes when instantiated in memory.
     */
    //header + data总长度，代表该page在磁盘上面的长度，
    uint32_t mem_size; /* 16-19: in-memory page size */

    union {
        //__wt_rec_split_finish当前reconcile处理的page上面的K和V总数
        uint32_t entries; /* 20-23: number of cells on page */
        uint32_t datalen; /* 20-23: overflow data length */
    } u;

    //也就是对应page类型，例如WT_PAGE_ROW_LEAF WT_PAGE_BLOCK_MANAGER(表示checkpoint的ext)
    uint8_t type; /* 24: page type */

/*
 * No automatic generation: flag values cannot change, they're written to disk.
 */
#define WT_PAGE_COMPRESSED 0x01u   /* Page is compressed on disk */
#define WT_PAGE_EMPTY_V_ALL 0x02u  /* Page has all zero-length values */
#define WT_PAGE_EMPTY_V_NONE 0x04u /* Page has no zero-length values */
#define WT_PAGE_ENCRYPTED 0x08u    /* Page is encrypted on disk */
#define WT_PAGE_UNUSED 0x10u       /* Historic lookaside store page updates, no longer used */
#define WT_PAGE_FT_UPDATE 0x20u    /* Page contains updated fast-truncate information */
    uint8_t flags;                 /* 25: flags */

    /* A byte of padding, positioned to be added to the flags. */
    uint8_t unused; /* 26: unused padding */

#define WT_PAGE_VERSION_ORIG 0 /* Original version */
#define WT_PAGE_VERSION_TS 1   /* Timestamps added */
    uint8_t version;           /* 27: version */
};
/*
 * WT_PAGE_HEADER_SIZE is the number of bytes we allocate for the structure: if the compiler inserts
 * padding it will break the world.
 */ 
//也就是上面的__wt_page_header头部结构长度  
#define WT_PAGE_HEADER_SIZE 28

/*
 * __wt_page_header_byteswap --
 *     Handle big- and little-endian transformation of a page header.
 */
static inline void
__wt_page_header_byteswap(WT_PAGE_HEADER *dsk)
{
#ifdef WORDS_BIGENDIAN
    dsk->recno = __wt_bswap64(dsk->recno);
    dsk->write_gen = __wt_bswap64(dsk->write_gen);
    dsk->mem_size = __wt_bswap32(dsk->mem_size);
    dsk->u.entries = __wt_bswap32(dsk->u.entries);
#else
    WT_UNUSED(dsk);
#endif
}


//WT_PAGE_HEADER(__wt_page_header)在__rec_split_write_header中完成赋值，对应内存位置见WT_BLOCK_HEADER_REF
//WT_BLOCK_HEADER(__wt_block_header)赋值在写磁盘完成后，在__block_write_off赋值,对应内存偏移见WT_PAGE_HEADER_BYTE_SIZE
//如果配置了压缩，则__wt_page_header记录的是压缩前的数据信息,__wt_block_header记录的是压缩后的数据信息

/*
 * The block-manager specific information immediately follows the WT_PAGE_HEADER structure.
 */ //也就是上面的__wt_page_header头部结构长度
#define WT_BLOCK_HEADER_REF(dsk) ((void *)((uint8_t *)(dsk) + WT_PAGE_HEADER_SIZE))

/*
 * WT_PAGE_HEADER_BYTE --
 * WT_PAGE_HEADER_BYTE_SIZE --
 *	The first usable data byte on the block (past the combined headers).
 */

//wt文件头部4K magic  checksum检查在__desc_read

 
//WT_PAGE_HEADER(__wt_page_header)在__rec_split_write_header中完成赋值，对应内存位置见WT_BLOCK_HEADER_REF
//WT_BLOCK_HEADER(__wt_block_header)赋值在写磁盘完成后，在__block_write_off赋值,对应内存偏移见WT_PAGE_HEADER_BYTE_SIZE
//如果配置了压缩，则__wt_page_header记录的是压缩前的数据信息,__wt_block_header记录的是压缩后的数据信息

//block header(WT_BLOCK_HEADER_SIZE) + page header(WT_PAGE_HEADER_SIZE) 
#define WT_PAGE_HEADER_BYTE_SIZE(btree) ((u_int)(WT_PAGE_HEADER_SIZE + (btree)->block_header)) 
//dsk开始跳过block header(WT_BLOCK_HEADER_SIZE) + page header(WT_PAGE_HEADER_SIZE) ,也就是这个page对应的实际数据起始地址
#define WT_PAGE_HEADER_BYTE(btree, dsk) \
    ((void *)((uint8_t *)(dsk) + WT_PAGE_HEADER_BYTE_SIZE(btree)))

/*
 * WT_ADDR --
 *	An in-memory structure to hold a block's location.
 */
//保存chunk->image写入磁盘时候的元数据信息(objectid offset size  checksum)
//赋值见__rec_split_write  
//参考__wt_multi_to_ref, 存的是该ref page写入磁盘的ext元数据信息(objectid offset size  checksum)
//__wt_multi.addr, __wt_ref.addr为该类型，参考__wt_multi_to_ref
struct __wt_addr {
    //赋值见__rec_split_write,真实来源实际上在__rec_row_leaf_insert  __wt_rec_row_leaf->WT_TIME_AGGREGATE_UPDATE中统计赋值
    //最终会赋值给父page的index[]数组下面ref->addr.ta，并通过internal page在__wt_rec_cell_build_addr函数中持久化到磁盘
    WT_TIME_AGGREGATE ta;

    //保存chunk->image写入磁盘时候的元数据信息(objectid offset size  checksum), __rec_split_write中赋值
    uint8_t *addr; /* Block-manager's cookie */
    //__wt_multi_to_ref
    uint8_t size;  /* Block-manager's cookie length */

#define WT_ADDR_INT 1     /* Internal page */
#define WT_ADDR_LEAF 2    /* Leaf page */ //溢出的KEY，也就是大key,
#define WT_ADDR_LEAF_NO 3 /* Leaf page, no overflow */
    //__wt_multi_to_ref
    uint8_t type; //和WT_RECONCILE.ovfl_items对应

    /*
     * If an address is both as an address for the previous and the current multi-block
     * reconciliations, that is, a block we're writing matches the block written the last time, it
     * will appear in both the current boundary points as well as the page modification's list of
     * previous blocks. The reuse flag is how we know that's happening so the block is treated
     * correctly (not free'd on error, for example).
     */
    uint8_t reuse;
};

/*
 * Overflow tracking for reuse: When a page is reconciled, we write new K/V overflow items. If pages
 * are reconciled multiple times, we need to know if we've already written a particular overflow
 * record (so we don't write it again), as well as if we've modified an overflow record previously
 * written (in which case we want to write a new record and discard blocks used by the previously
 * written record). Track overflow records written for the page, storing the values in a skiplist
 * with the record's value as the "key".
 */
struct __wt_ovfl_reuse {
    uint32_t value_offset; /* Overflow value offset */
    uint32_t value_size;   /* Overflow value size */
    uint8_t addr_offset;   /* Overflow addr offset */
    uint8_t addr_size;     /* Overflow addr size */

/*
 * On each page reconciliation, we clear the entry's in-use flag, and reset it as the overflow
 * record is re-used. After reconciliation completes, unused skiplist entries are discarded, along
 * with their underlying blocks.
 *
 * On each page reconciliation, set the just-added flag for each new skiplist entry; if
 * reconciliation fails for any reason, discard the newly added skiplist entries, along with their
 * underlying blocks.
 */
/* AUTOMATIC FLAG VALUE GENERATION START 0 */
#define WT_OVFL_REUSE_INUSE 0x1u
#define WT_OVFL_REUSE_JUST_ADDED 0x2u
    /* AUTOMATIC FLAG VALUE GENERATION STOP 8 */
    uint8_t flags;

/*
 * The untyped address immediately follows the WT_OVFL_REUSE structure, the untyped value
 * immediately follows the address.
 */
#define WT_OVFL_REUSE_ADDR(p) ((void *)((uint8_t *)(p) + (p)->addr_offset))
#define WT_OVFL_REUSE_VALUE(p) ((void *)((uint8_t *)(p) + (p)->value_offset))

    WT_OVFL_REUSE *next[0]; /* Forward-linked skip list */
};

/*
 * History store table support: when a page is being reconciled for eviction and has updates that
 * might be required by earlier readers in the system, the updates are written into the history
 * store table, and restored as necessary if the page is read.
 *
 * The first part of the key is comprised of a file ID, record key (byte-string for row-store,
 * record number for column-store) and timestamp. This allows us to search efficiently for a given
 * record key and read timestamp combination. The last part of the key is a monotonically increasing
 * counter to keep the key unique in the case where we have multiple transactions committing at the
 * same timestamp.
 * The value is the WT_UPDATE structure's:
 * 	- stop timestamp
 * 	- durable timestamp
 *	- update type
 *	- value.
 *
 * As the key for the history store table is different for row- and column-store, we store both key
 * types in a WT_ITEM, building/parsing them in the code, because otherwise we'd need two
 * history store files with different key formats. We could make the history store table's key
 * standard by moving the source key into the history store table value, but that doesn't make the
 * coding any simpler, and it makes the history store table's value more likely to overflow the page
 * size when the row-store key is relatively large.
 *
 * Note that we deliberately store the update type as larger than necessary (8 bytes vs 1 byte).
 * We've done this to leave room in case we need to store extra bit flags in this value at a later
 * point. If we need to store more information, we can potentially tack extra information at the end
 * of the "value" buffer and then use bit flags within the update type to determine how to interpret
 * it.
 *
 * We also configure a larger than default internal page size to accommodate for larger history
 * store keys. We do that to reduce the chances of having to create overflow keys on the page.
 */
#ifdef HAVE_BUILTIN_EXTENSION_SNAPPY
#define WT_HS_COMPRESSOR "snappy"
#else
#define WT_HS_COMPRESSOR "none"
#endif
#define WT_HS_KEY_FORMAT WT_UNCHECKED_STRING(IuQQ)
#define WT_HS_VALUE_FORMAT WT_UNCHECKED_STRING(QQQu)
#define WT_HS_CONFIG                                                   \
    "key_format=" WT_HS_KEY_FORMAT ",value_format=" WT_HS_VALUE_FORMAT \
    ",block_compressor=" WT_HS_COMPRESSOR                              \
    ",internal_page_max=16KB"                                          \
    ",leaf_value_max=64MB"                                             \
    ",prefix_compression=false"

/*
 * WT_SAVE_UPD --
 *	Unresolved updates found during reconciliation.
 */
//复制参考__rec_update_save
struct __wt_save_upd {
    WT_INSERT *ins; /* Insert list reference */
    WT_ROW *rip;    /* Original on-page reference */
    //如果upd_select->upd链表第一个udp是删除操作，指向这个删除udp
    WT_UPDATE *onpage_upd;
    //如果upd_select->upd链表第一个udp是删除操作，指向这个删除udp
    WT_UPDATE *onpage_tombstone;
    //赋值参考__rec_update_save
    //如果supd_restore为true说明存在全局id不可见的udp，如果supd_restore为false说明ins->udp
    //链表上面都是全局id可见的udp链表，但是存在timestamp不是全局可见的udp
    bool restore; /* Whether to restore this saved update chain */
};

/*
 * WT_MULTI --
 *	Replacement block information used during reconciliation.
 */ //一个page拆分为多个page时候用到，可以参考__rec_split_dump_keys的遍历,__rec_split_write这里创建空间和赋值
//管理磁盘page image的内存元数据信息，一个__wt_multi对应一个pag 磁盘image
struct __wt_multi {
    /*
     * Block's key: either a column-store record number or a row-store variable length byte string.
     */
    //page拆分后对应的ref key，参考__rec_split_dump_keys的打印，赋值的地方在__rec_split_write
    union {
        uint64_t recno;
        WT_IKEY *ikey;
    } key;

    /*
     * A disk image that may or may not have been written, used to re-instantiate the page in
     * memory.
     */
    //通过reconcile把一个chunk数据写入磁盘后，会拷贝一份到disk_image, 参考__rec_split_write
    //也就是disk_image内存一个完整的chunk(WT_PAGE_HEADER_SIZE + WT_BLOCK_HEADER_SIZE + 实际数据)
    //最终在__wt_page_inmem中赋值给page->dsk, 然后在__wt_multi_to_ref->__split_multi_inmem->__wt_page_inmem->__inmem_row_leaf解析
    //磁盘page->dsk中的K和V地址信息存入到page->pg_row, 最后在__wt_multi_to_ref中释放disk_image

    //从上面的备注可以看出，reconcile一个multi对应数据写入磁盘后，会在拷贝一份到disk_image中，最终在__wt_page_inmem中赋值给page->dsk
    //然后在__inmem_row_leaf解析出磁盘上的K和V地址保存到page->pg_row[]中, 最后释放disk_image空间，也就是disk_image只是一个临时
    //变量保存写入磁盘的所有封包数据，最终目的是为了获取page磁盘上K或者V数据保存到page->pg_row[]数组中


    //__evict_update_work 例如如果已使用内存占比总内存不超过(target 80% + trigger 95%)配置的一半，则设置标识WT_CACHE_EVICT_SCRUB，
    //  说明reconcile的时候可以内存拷贝一份page数据存入image, 一般代表内存比较充足，因此可以拷贝一份到image，最终被赋值给
    //  内存中的split的page的page->dsk,参考__wt_multi_to_ref->__split_multi_inmem->__wt_page_inmem

    //如果内存很紧张，则reconcile的时候会持久化到磁盘，这时候disk_image为NULL，split拆分后的ref->page为NULL，这时候page生成依靠用户线程
    //  在__page_read中读取磁盘数据到page中完成ref->page内存空间生成
    void *disk_image;

    /*
     * List of unresolved updates. Updates are either a row-store insert or update list, or
     * column-store insert list. When creating history store records, there is an additional value,
     * the committed item's transaction information.
     *
     * If there are unresolved updates, the block wasn't written and there will always be a disk
     * image.
     */
    //赋值参考__rec_supd_move  __rec_update_save
    //记录这个reconcile的page上面存在的不可见的udp链表信息，这里是一个数组，最终会通过__rec_hs_wrapup把这些不可见的upd记录到wiredtigerHs.wt文件中
    WT_SAVE_UPD *supd;
    uint32_t supd_entries;
    //说明有不是全局id可见的udp,赋值参考__rec_update_save
    bool supd_restore; /* Whether to restore saved update chains to this page */

    /*
     * Disk image was written: address, size and checksum. On subsequent reconciliations of this
     * page, we avoid writing the block if it's unchanged by comparing size and checksum; the reuse
     * flag is set when the block is unchanged and we're reusing a previous address.
     */
    //保存chunk->image写入磁盘时候的元数据信息(objectid offset size  checksum)
    //赋值见__rec_split_write
    WT_ADDR addr;
    //也就是一个chunk的大小
    uint32_t size;
    uint32_t checksum;
};

/*
 * WT_OVFL_TRACK --
 *  Overflow record tracking for reconciliation. We assume overflow records are relatively rare,
 * so we don't allocate the structures to track them until we actually see them in the data.
 */
struct __wt_ovfl_track {
    /*
     * Overflow key/value address/byte-string pairs we potentially reuse each time we reconcile the
     * page.
     */
    //可以参考__wt_ovfl_reuse_search
    WT_OVFL_REUSE *ovfl_reuse[WT_SKIP_MAXDEPTH];

    /*
     * Overflow key/value addresses to be discarded from the block manager after reconciliation
     * completes successfully.
     */
    WT_CELL **discard;
    size_t discard_entries;
    size_t discard_allocated;
};

/*
 * WT_PAGE_MODIFY --
 *	When a page is modified, there's additional information to maintain.
 参考官方文档https://source.wiredtiger.com/develop/arch-cache.html
 */
//__wt_page.modify 赋值见__wt_page_modify_init
//例如leaf page中跳跃表中节点内容都在该modify中, mod相关空间释放见__free_page_modify
struct __wt_page_modify {
    /* The first unwritten transaction ID (approximate). */
    uint64_t first_dirty_txn;

    /* The transaction state last time eviction was attempted. */
    //__reconcile_save_evict_state中赋值
    uint64_t last_evict_pass_gen;
    uint64_t last_eviction_id;
    wt_timestamp_t last_eviction_timestamp;

#ifdef HAVE_DIAGNOSTIC
    /* Check that transaction time moves forward. */
    uint64_t last_oldest_id;
#endif

    /* Avoid checking for obsolete updates during checkpoints. */
    uint64_t obsolete_check_txn;
    wt_timestamp_t obsolete_check_timestamp;

    /* The largest transaction seen on the page by reconciliation. */
    uint64_t rec_max_txn;
    wt_timestamp_t rec_max_timestamp;

    /* The largest update transaction ID (approximate). */
    uint64_t update_txn;

    /* Dirty bytes added to the cache. */
    //__wt_cache_page_inmem_incr
    size_t bytes_dirty;
    size_t bytes_updates;

    /*
     * When pages are reconciled, the result is one or more replacement blocks. A replacement block
     * can be in one of two states: it was written to disk, and so we have a block address, or it
     * contained unresolved modifications and we have a disk image for it with a list of those
     * unresolved modifications. The former is the common case: we only build lists of unresolved
     * modifications when we're evicting a page, and we only expect to see unresolved modifications
     * on a page being evicted in the case of a hot page that's too large to keep in memory as it
     * is. In other words, checkpoints will skip unresolved modifications, and will write the blocks
     * rather than build lists of unresolved modifications.
     *
     * Ugly union/struct layout to conserve memory, we never have both a replace address and
     * multiple replacement blocks.
     */
    union {
        struct { /* Single, written replacement block */
            WT_ADDR replace;

            /*
             * A disk image that may or may not have been written, used to re-instantiate the page
             * in memory.
             */
            void *disk_image;
        } r;
#undef mod_replace
#define mod_replace u1.r.replace
#undef mod_disk_image
#define mod_disk_image u1.r.disk_image

        //管理磁盘page image的内存元数据信息，一个__wt_multi对应一个pag 磁盘image
        struct {//注意__wt_reconcile.multi和__wt_page_modify.mod_multi_entries.multi的区别联系
            //__rec_write_wrapup中赋值
            WT_MULTI *multi;        /* Multiple replacement blocks */
            //__rec_write_wrapup中赋值
            uint32_t multi_entries; /* Multiple blocks element count */
        } m;
#undef mod_multi
//赋值参考__rec_write_wrapup， 一次reconcile结束后，reconcile的multi信息转存到mod->mod_multi中
#define mod_multi u1.m.multi
#undef mod_multi_entries
//赋值参考__rec_write_wrapup，一次reconcile结束后，reconcile的multi信息转存到mod->mod_multi中, 也就是r->multi_next数
#define mod_multi_entries u1.m.multi_entries
    } u1;

    /*
     * Internal pages need to be able to chain root-page splits and have a special transactional
     * eviction requirement. Column-store leaf pages need update and append lists.
     *
     * Ugly union/struct layout to conserve memory, a page is either a leaf page or an internal
     * page.
     */
    union {
        struct {
            /*
             * When a root page splits, we create a new page and write it; the new page can also
             * split and so on, and we continue this process until we write a single replacement
             * root page. We use the root split field to track the list of created pages so they can
             * be discarded when no longer needed.
             */
            WT_PAGE *root_split; /* Linked list of root split pages */
        } intl;
#undef mod_root_split
#define mod_root_split u2.intl.root_split
        struct {
            /*
             * Appended items to column-stores. Actual appends to the tree only happen on the last
             * page, but gaps created in the namespace by truncate operations can result in the
             * append lists of other pages becoming populated.
             */
            WT_INSERT_HEAD **append;

            /*
             * Updated items in column-stores: variable-length RLE entries can expand to multiple
             * entries which requires some kind of list we can expand on demand. Updated items in
             * fixed-length files could be done based on an WT_UPDATE array as in row-stores, but
             * there can be a very large number of bits on a single page, and the cost of the
             * WT_UPDATE array would be huge.
             */
            WT_INSERT_HEAD **update;

            /*
             * Split-saved last column-store page record. If a fixed-length column-store page is
             * split, we save the first record number moved so that during reconciliation we know
             * the page's last record and can write any implicitly created deleted records for the
             * page. No longer used by VLCS.
             */
            uint64_t split_recno;
        } column_leaf;
#undef mod_col_append
#define mod_col_append u2.column_leaf.append
#undef mod_col_update
#define mod_col_update u2.column_leaf.update
#undef mod_col_split_recno
#define mod_col_split_recno u2.column_leaf.split_recno
        struct {
            /* Inserted items for row-store. */
            //WT_PAGE_ALLOC_AND_SWAP __wt_leaf_page_can_split分配空间
            WT_INSERT_HEAD **insert;

            /* Updated items for row-stores. */
            //pg_row指向磁盘KV相关数据(实际上这里指向的是内存，从磁盘加载的或者从内存写入磁盘的，内存和磁盘都有一份)，mod_row_insert指向内存相关KV数据，mod_row_update记录内存中同一个K的变更过程
            WT_UPDATE **update;
        } row_leaf;
#undef mod_row_insert
////pg_row指向磁盘KV相关数据(实际上这里指向的是内存，从磁盘加载的或者从内存写入磁盘的，内存和磁盘都有一份)，mod_row_insert指向内存相关KV数据，mod_row_update记录内存中同一个K的变更过程

//__wt_row_modify->WT_PAGE_ALLOC_AND_SWAP、__split_insert分配空间
//WT_ROW_INSERT_SLOT获取对应的跳跃表，实际上insert是个数组，数组每个成员对应一个跳跃表，参考__wt_leaf_page_can_split
#define mod_row_insert u2.row_leaf.insert
#undef mod_row_update
////pg_row指向磁盘KV相关数据(实际上这里指向的是内存，从磁盘加载的或者从内存写入磁盘的，内存和磁盘都有一份)，mod_row_insert指向内存相关KV数据，mod_row_update记录内存中同一个K的变更过程
#define mod_row_update u2.row_leaf.update
    } u2;

    /* Overflow record tracking for reconciliation. */
    WT_OVFL_TRACK *ovfl_track;

    /*
     * Page-delete information for newly instantiated deleted pages. The instantiated flag remains
     * set until the page is reconciled successfully; this indicates that the page_del information
     * in the ref remains valid. The update list remains set (if set at all) until the transaction
     * that deleted the page is resolved. These transitions are independent; that is, the first
     * reconciliation can happen either before or after the delete transaction resolves.
     */
    bool instantiated;        /* True if this is a newly instantiated page. */
    WT_UPDATE **inst_updates; /* Update list for instantiated page with unresolved truncate. */

//获取page对应的page_lock锁
#define WT_PAGE_LOCK(s, p) __wt_spin_lock((s), &(p)->modify->page_lock)
#define WT_PAGE_TRYLOCK(s, p) __wt_spin_trylock((s), &(p)->modify->page_lock)
#define WT_PAGE_UNLOCK(s, p) __wt_spin_unlock((s), &(p)->modify->page_lock)
    WT_SPINLOCK page_lock; /* Page's spinlock */

/*
 * The page state is incremented when a page is modified.
 *
 * WT_PAGE_CLEAN --
 *	The page is clean.
 * WT_PAGE_DIRTY_FIRST --
 *	The page is in this state after the first operation that marks a
 *	page dirty, or when reconciliation is checking to see if it has
 *	done enough work to be able to mark the page clean.
 * WT_PAGE_DIRTY --
 *	Two or more updates have been added to the page.
 */
//__wt_page_modify_clear
#define WT_PAGE_CLEAN 0
#define WT_PAGE_DIRTY_FIRST 1
#define WT_PAGE_DIRTY 2
    //标识该page状态，是否是脏page，参考__wt_page_only_modify_set
    uint32_t page_state;

#define WT_PM_REC_EMPTY 1      /* Reconciliation: no replacement */
#define WT_PM_REC_MULTIBLOCK 2 /* Reconciliation: multiple blocks */
//例如一个page有修改，但是修了一点点，这时候一个page完全够了，一般update的适合这种很常见
#define WT_PM_REC_REPLACE 3    /* Reconciliation: single block */
    //复制参考__rec_write_wrapup
    uint8_t rec_result;        /* Reconciliation state */

#define WT_PAGE_RS_RESTORED 0x1
    uint8_t restore_state; /* Created by restoring updates */
};

/*
 * WT_COL_RLE --
 *	Variable-length column-store pages have an array of page entries with
 *	RLE counts greater than 1 when reading the page, so it's not necessary
 *	to walk the page counting records to find a specific entry. We can do a
 *	binary search in this array, then an offset calculation to find the
 *	cell.
 */
//WT_PACKED_STRUCT_BEGIN(__wt_col_rle)
//    uint64_t recno; /* Record number of first repeat. */
//    uint64_t rle;   /* Repeat count. */
//    uint32_t indx;  /* Slot of entry in col_var. */
//WT_PACKED_STRUCT_END

struct __wt_col_rle {
    uint64_t recno; /* Record number of first repeat. */
    uint64_t rle;   /* Repeat count. */
    uint32_t indx;  /* Slot of entry in col_var. */
};

/*
 * WT_PAGE_INDEX --
 *	The page index held by each internal page.
 */
//https://github.com/wiredtiger/wiredtiger/wiki/In-Memory-Tree-Layout
//__wt_page_alloc分配空间，通过WT_INTL_INDEX_GET(session, page, pindex);获取page对应的__wt_page_index
//可以参考__split_parent
struct __wt_page_index {/* Sanity check for a reasonable number of on-page keys. */
#define WT_INTERNAL_SPLIT_MIN_KEYS 100
    //新增层分了多少组，例如10002个子page会默认分10002/100组
    uint32_t entries;
    uint32_t deleted_entries;
    //指向真实的index数组地址
    WT_REF **index;
};

/*
 * WT_COL_VAR_REPEAT --
 *  Variable-length column-store pages have an array of page entries with RLE counts
 * greater than 1 when reading the page, so it's not necessary to walk the page counting
 * records to find a specific entry. We can do a binary search in this array, then an
 * offset calculation to find the cell.
 *
 * It's a separate structure to keep the page structure as small as possible.
 */
struct __wt_col_var_repeat {
    uint32_t nrepeats;     /* repeat slots */
    WT_COL_RLE repeats[0]; /* lookup RLE array */
};

/*
 * WT_COL_FIX_TW_ENTRY --
 *     This is a single entry in the WT_COL_FIX_TW array. It stores the offset from the page's
 * starting recno and the offset into the page to find the value cell containing the time window.
 */
struct __wt_col_fix_tw_entry {
    uint32_t recno_offset;
    uint32_t cell_offset;
};

/*
 * WT_COL_FIX_TW --
 *     Fixed-length column-store pages carry an array of page entries that have time windows. This
 * is built when reading the page to avoid the need to walk the page to find a specific entry. We
 * can do a binary search in this array instead.
 */
struct __wt_col_fix_tw {
    uint32_t numtws;            /* number of time window slots */
    WT_COL_FIX_TW_ENTRY tws[0]; /* lookup array */
};

/* WT_COL_FIX_TW_CELL gets the cell pointer from a WT_COL_FIX_TW_ENTRY. */
#define WT_COL_FIX_TW_CELL(page, entry) ((WT_CELL *)((uint8_t *)(page)->dsk + (entry)->cell_offset))

/*
 * Macro to walk the list of references in an internal page.
 */
//遍历获取internal page所包含的所有子ref
#define WT_INTL_FOREACH_BEGIN(session, page, ref)                                    \
    do {                                                                             \
        WT_PAGE_INDEX *__pindex;                                                     \
        WT_REF **__refp;                                                             \
        uint32_t __entries;                                                          \
        WT_INTL_INDEX_GET(session, page, __pindex);                                  \
        for (__refp = __pindex->index, __entries = __pindex->entries; __entries > 0; \
             --__entries) {                                                          \
            (ref) = *__refp++;

#undef pg_intl_parent_ref
//指向该page的ref，参考图形化https://github.com/wiredtiger/wiredtiger/wiki/In-Memory-Tree-Layout
#define pg_intl_parent_ref u.intl.parent_ref
#undef pg_intl_split_gen
//记录该internal page分裂的次数  __split_parent中赋值
#define pg_intl_split_gen u.intl.split_gen

/*
 * WT_PAGE --
 *	The WT_PAGE structure describes the in-memory page information.
 */ //__wt_page_alloc分片空间和赋值 __wt_page_out对page进行空间释放
struct __wt_page {
    /* Per page-type information. */
    union {
        /*
         * Internal pages (both column- and row-store).
         *
         * In-memory internal pages have an array of pointers to child
         * structures, maintained in collated order.
         *
         * Multiple threads of control may be searching the in-memory
         * internal page and a child page of the internal page may
         * cause a split at any time.  When a page splits, a new array
         * is allocated and atomically swapped into place.  Threads in
         * the old array continue without interruption (the old array is
         * still valid), but have to avoid racing.  No barrier is needed
         * because the array reference is updated atomically, but code
         * reading the fields multiple times would be a very bad idea.
         * Specifically, do not do this:
         *	WT_REF **refp = page->u.intl__index->index;
         *	uint32_t entries = page->u.intl__index->entries;
         *
         * The field is declared volatile (so the compiler knows not to
         * read it multiple times), and we obscure the field name and
         * use a copy macro in all references to the field (so the code
         * doesn't read it multiple times).
         */
        //配合图形化阅读https://github.com/wiredtiger/wiredtiger/wiki/In-Memory-Tree-Layout
        struct {
            //指向父节点的page
            WT_REF *parent_ref; /* Parent reference */
            uint64_t split_gen; /* Generation of last split */

            //index ref数组
            //WT_PAGE_INDEX *volatile __index; /* Collated children */
            WT_PAGE_INDEX *__index; /* Collated children */
        } intl;

/*
 * Macros to copy/set the index because the name is obscured to ensure the field isn't read multiple
 * times.
 *
 * There are two versions of WT_INTL_INDEX_GET because the session split generation is usually set,
 * but it's not always required: for example, if a page is locked for splitting, or being created or
 * destroyed.
 */
//参考BTREE数图形化 https://github.com/wiredtiger/wiredtiger/wiki/In-Memory-Tree-Layout
//为何是safe安全的，可以参考WT_ENTER_PAGE_INDEX  WT_LEAVE_PAGE_INDEX  WT_WITH_PAGE_INDEX
//之类可以验证一下，假设某个用户线程操作的page超过限制，在evict中会splite重新生成修改index，这时候用户线程的evict逻辑中我们sleep，
//evict server线程会怎么样?
#define WT_INTL_INDEX_GET_SAFE(page) ((page)->u.intl.__index)
#define WT_INTL_INDEX_GET(session, page, pindex)                          \
    do {                                                                  \
        WT_ASSERT(session, __wt_session_gen(session, WT_GEN_SPLIT) != 0); \
        (pindex) = WT_INTL_INDEX_GET_SAFE(page);                          \
    } while (0)

//page的__index数组赋值，见__wt_page_alloc
#define WT_INTL_INDEX_SET(page, v)      \
    do {                                \
        WT_WRITE_BARRIER();             \
        ((page)->u.intl.__index) = (v); \
    } while (0)

#define WT_INTL_FOREACH_REVERSE_BEGIN(session, page, ref)                                 \
    do {                                                                                  \
        WT_PAGE_INDEX *__pindex;                                                          \
        WT_REF **__refp;                                                                  \
        uint32_t __entries;                                                               \
        WT_INTL_INDEX_GET(session, page, __pindex);                                       \
        for (__refp = __pindex->index + __pindex->entries, __entries = __pindex->entries; \
             __entries > 0; --__entries) {                                                \
            (ref) = *--__refp;
#define WT_INTL_FOREACH_END \
    }                       \
    }                       \
    while (0)

        /* Row-store leaf page. */
        //An array of items that were on the page when it was loaded into memory (cache) from disk.
        //WT_PAGE::row::d aliased as WT_PAGE::pg_row_d
        //指向该page存储的真实数据，见__wt_page_alloc
        WT_ROW *row; /* Key/value pairs */
#undef pg_row
//pg_row指向磁盘KV相关数据WT_ROW_FOREACH遍历获取该page在磁盘的KV数据� 这个数据在内存中，只是从磁盘加载的，在磁盘上也有一份数据
//mod_row_insert指向内存相关KV数据，mod_row_update记录内存中同一个K的变更过程
//指向该page存储的真实数据，pg_row[]数组(数组大小page->entries)空间分配见__wt_page_alloc,保持K或者V的磁盘地址，每个KV赋值参考__inmem_row_leaf
//磁盘上的数据最终有一份完全一样的内存数据存在page->dsk地址开始的内存空间，page->pg_row[]数组每个K或者V成员实际上指向离page->dsk起始位置的距离，参考__wt_cell_unpack_safe
//page->dsk存储ext磁盘头部地址，page->pg_row[]存储实际的K或者V相对头部的距离，通过page->dsk和这个距离就可以确定在磁盘ext中的位置,参考__wt_page_inmem


//场景1:
// 该page没有数据在磁盘上面
//   cbt->slot则直接用WT_ROW_INSERT_SLOT[0]这个跳表，

//场景2:
// 如果磁盘有数据则WT_ROW_INSERT_SMALLEST(mod_row_insert[(page)->entries])表示写入这个page并且K小于该page在
//   磁盘上面最小的K的所有数据通过这个跳表存储起来

//场景3:
// 该page有数据在磁盘上面，例如该page在磁盘上面有两天数据ke1,key2...keyn，新插入keyx>page最大的keyn，则内存会维护一个
//   cbt->slot为磁盘KV总数-1，这样大于该page的所有KV都会添加到WT_ROW_INSERT_SLOT[page->entries - 1]这个跳表上面

//场景4:
// 该page有数据在磁盘上面，例如该page在磁盘上面有两天数据ke1,key2,key3...keyn，新插入keyx大于key2小于key3, key2<keyx>key3，则内存会维护一个
//   cbt->slot为磁盘key2在磁盘中的位置(也就是1，从0开始算)，这样大于该page的所有KV都会添加到WT_ROW_INSERT_SLOT[1]这个跳表

//一个page在磁盘page->pg_row有多少数据(page->pg_row[]数组大小)，就会维护多少个跳表，因为要保证新写入内存的数据和磁盘的数据保持顺序

#define pg_row u.row

        /* Fixed-length column-store leaf page. */
        struct {
            uint8_t *fix_bitf;     /* Values */
            WT_COL_FIX_TW *fix_tw; /* Time window index */
#define WT_COL_FIX_TWS_SET(page) ((page)->u.col_fix.fix_tw != NULL)
        } col_fix;
#undef pg_fix_bitf
#define pg_fix_bitf u.col_fix.fix_bitf
#undef pg_fix_numtws
#define pg_fix_numtws u.col_fix.fix_tw->numtws
#undef pg_fix_tws
#define pg_fix_tws u.col_fix.fix_tw->tws

        /* Variable-length column-store leaf page. */
        struct {
            WT_COL *col_var;            /* Values */
            WT_COL_VAR_REPEAT *repeats; /* Repeats array */
#define WT_COL_VAR_REPEAT_SET(page) ((page)->u.col_var.repeats != NULL)
        } col_var;
#undef pg_var
#define pg_var u.col_var.col_var
#undef pg_var_repeats
#define pg_var_repeats u.col_var.repeats->repeats
#undef pg_var_nrepeats
#define pg_var_nrepeats u.col_var.repeats->nrepeats
    } u;

    /*
     * Page entry count, page-wide prefix information, type and flags are positioned at the end of
     * the WT_PAGE union to reduce cache misses when searching row-store pages.
     *
     * The entries field only applies to leaf pages, internal pages use the page-index entries
     * instead.
     */
    // An internal Btree page will have an array of WT_REF structures.
    //A row-store leaf page will have an array of WT_ROW structures representing the KV pairs stored on the page.
    //赋值见__wt_page_inmem->__wt_page_inmem
    //代表在磁盘pg_row上面的KV总数,可以参考__wt_evict->__evict_page_dirty_update->__wt_split_multi->__split_multi_lock
    //->__split_multi->__wt_multi_to_ref->__split_multi_inmem->__wt_page_inmem->__wt_page_alloc
    //磁盘上pg_row[]中的KV数量
    uint32_t entries; /* Leaf page entries */

    uint32_t prefix_start; /* Best page prefix starting slot */
    uint32_t prefix_stop;  /* Maximum slot to which the best page prefix applies */

#define WT_PAGE_IS_INTERNAL(page) \
    ((page)->type == WT_PAGE_COL_INT || (page)->type == WT_PAGE_ROW_INT)
#define WT_PAGE_INVALID 0       /* Invalid page */
//__wt_block_extlist_write，表示checkpoint的ext
#define WT_PAGE_BLOCK_MANAGER 1 /* Block-manager page */
#define WT_PAGE_COL_FIX 2       /* Col-store fixed-len leaf */
#define WT_PAGE_COL_INT 3       /* Col-store internal page */
#define WT_PAGE_COL_VAR 4       /* Col-store var-length leaf page */
#define WT_PAGE_OVFL 5          /* Overflow page */
#define WT_PAGE_ROW_INT 6       /* Row-store internal page */
#define WT_PAGE_ROW_LEAF 7      /* Row-store leaf page */
    uint8_t type;               /* Page type */

/* AUTOMATIC FLAG VALUE GENERATION START 0 */
#define WT_PAGE_BUILD_KEYS 0x001u         /* Keys have been built in memory */
#define WT_PAGE_COMPACTION_WRITE 0x002u   /* Writing the page for compaction */
#define WT_PAGE_DISK_ALLOC 0x004u         /* Disk image in allocated memory */
#define WT_PAGE_DISK_MAPPED 0x008u        /* Disk image in mapped memory */
//__evict_push_candidate置位，挑选出进入evict队列的page标识
#define WT_PAGE_EVICT_LRU 0x010u          /* Page is on the LRU queue */
#define WT_PAGE_EVICT_NO_PROGRESS 0x020u  /* Eviction doesn't count as progress */
#define WT_PAGE_INTL_OVERFLOW_KEYS 0x040u /* Internal page has overflow keys (historic only) */
//__split_insert置位
#define WT_PAGE_SPLIT_INSERT 0x080u       /* A leaf page was split for append */
#define WT_PAGE_UPDATE_IGNORE 0x100u      /* Ignore updates on page discard */
                                          /* AUTOMATIC FLAG VALUE GENERATION STOP 16 */
    uint16_t flags_atomic;                /* Atomic flags, use F_*_ATOMIC_16 */

    uint8_t unused; /* Unused padding */

    //__wt_cache_page_inmem_decr中做减法，__wt_cache_page_inmem_incr中做加法
    //该page对应的内存，包括写入磁盘的，可以参考__evict_force_check
    size_t memory_footprint; /* Memory attached to the page */

    /* Page's on-disk representation: NULL for pages created in memory. */
    //__split_multi_inmem->__wt_page_inmem可以看出，实际上内存也有一份磁盘完全一样的内存数据，也就是disk_image，最终内存中的数据记录到dsk
    //该page有数据存储在磁盘上，磁盘的结构信息位置, 指向一个磁盘chunk(WT_PAGE_HEADER_SIZE + WT_BLOCK_HEADER_SIZE + 实际数据)的头部WT_PAGE_HEADER_SIZE

    //在__split_multi_inmem->__wt_page_inmem中赋值给了page->dsk, 并在置为disk_image=NULL， 所以指向的内存空间实际上被page->dsk继承了
    //也就是该page在磁盘上的数据同样会存一份到内存中，也就是两份数据

    //随着splite拆分完成，例如从老page拆分为10个新page，老page最终会通过__wt_page_out释放
    //page->dsk存储ext磁盘头部地址，page->pg_row[]存储实际的K或者V相对头部的距离，通过page->dsk和这个距离就可以确定在磁盘ext中的位置,参考__wt_page_inmem
    //__wt_page_out->__wt_overwrite_and_free_len中释放

    //__wt_cache_bytes_image中记录总的image内存消耗，也就是写入ext时候在内存中实际上有同样一份的内存计算统计

    //注意__wt_rec_row_leaf和__wt_rec_row_int的差异，一个是持久化leaf page，一个是持久化internal page
    //  __wt_rec_row_int: 持久化internal page拥有的子ref page key及其所有子page持久化到磁盘的ext(objectid offset size  checksum)元数据信息, 
    //      当重启(__wt_btree_tree_open->__wt_page_inmem)或者读取interanal page数据(__page_read)的时候会把这些持久化的ref page key和所有持久化的page元数据加载到page->dsk中
    //  __wt_rec_row_leaf: 持久化leaf page数据到磁盘，同时把磁盘ext元数据(objectid offset size  checksum)信息添加到ref.addr
    const WT_PAGE_HEADER *dsk; //赋值见__wt_page_inmem，指向磁盘数据

    /* If/when the page is modified, we need lots more information. */
    //__wt_page.modify 赋值见__wt_page_modify_init
    //例如leaf page中跳跃表中节点内容都在该modify中，mod相关空间释放见__free_page_modify，WT_PAGE_MODIFY page->modify相关空间释放，包括mod_row_insert mod_row_update mod_multi等
    WT_PAGE_MODIFY *modify;//__wt_page.modify

    /*
     * !!!
     * This is the 64 byte boundary, try to keep hot fields above here.
     */

/*
 * The page's read generation acts as an LRU value for each page in the
 * tree; it is used by the eviction server thread to select pages to be
 * discarded from the in-memory tree.
 *
 * The read generation is a 64-bit value, if incremented frequently, a
 * 32-bit value could overflow.
 *
 * The read generation is a piece of shared memory potentially read
 * by many threads.  We don't want to update page read generations for
 * in-cache workloads and suffer the cache misses, so we don't simply
 * increment the read generation value on every access.  Instead, the
 * read generation is incremented by the eviction server each time it
 * becomes active.  To avoid incrementing a page's read generation too
 * frequently, it is set to a future point.
 *
 * Because low read generation values have special meaning, and there
 * are places where we manipulate the value, use an initial value well
 * outside of the special range.
 */
//初始值, 例如一个page进行evict的时候，从一个page拆分为多个page,多出来的page的初始值也就是WT_READGEN_NOTSET
#define WT_READGEN_NOTSET 0
//__wt_page_evict_soon  Set a page to be evicted as soon as possible.
#define WT_READGEN_OLDEST 1
#define WT_READGEN_WONT_NEED 2
//满足这个条件在__evict_entry_priority中直接评分为WT_READGEN_OLDEST
//评分在这个范围的page需要立马reconcile，
// 评分越低，说明近期该page越没有被访问，例如2点访问的page1比1点访问的page2的评分会高，参考__wt_cache_read_gen_bump __wt_cache_read_gen_new
/* Any page set to the oldest generation should be discarded. */
#define WT_READGEN_EVICT_SOON(readgen) \
    ((readgen) != WT_READGEN_NOTSET && (readgen) < WT_READGEN_START_VALUE)
#define WT_READGEN_START_VALUE 100
#define WT_READGEN_STEP 100
    //__wt_cache.read_gen代表全局的read_gen，page.read_gen代表指定表的
     //__wt_cache_read_gen_new: 如果read_gen没有设置，则evict server线程在__evict_walk_tree中选出需要evict的page后通过该函数生成page read_gen
    //__wt_cache_read_gen_bump: evict worker或者app用户线程在__wt_cache_read_gen_bump中从队列消费这个page reconcile的时候赋值

    //__wt_page_alloc  __wt_page_evict_soon中初始值WT_READGEN_NOTSET，__wt_cache_read_gen_bump __wt_cache_read_gen_new中修改赋值
    //代表近期是否用户线程有访问该page，如果值越大，说明近期该page被用户线程访问活跃，参考__wt_cache_read_gen_new
    uint64_t read_gen;//evict server线程通过该值对挑选的page进行评分，见__evict_entry_priority

    uint64_t cache_create_gen; /* Page create timestamp */
    //赋值见__evict_walk_tree，也就是第几轮__evict_pass的时候该page被后台evict server线程选中淘汰的
    uint64_t evict_pass_gen;   /* Eviction pass generation */
};

/*
 * WT_PAGE_DISK_OFFSET, WT_PAGE_REF_OFFSET --
 *	Return the offset/pointer of a pointer/offset in a page disk image.
 */
#define WT_PAGE_DISK_OFFSET(page, p) WT_PTRDIFF32(p, (page)->dsk)
#define WT_PAGE_REF_OFFSET(page, o) ((void *)((uint8_t *)((page)->dsk) + (o)))

/*
 * Prepare update states.
 *
 * Prepare update synchronization is based on the state field, which has the
 * following possible states:
 *
 * WT_PREPARE_INIT:
 *	The initial prepare state of either an update or a page_del structure,
 *	indicating a prepare phase has not started yet.
 *	This state has no impact on the visibility of the update's data.
 *
 * WT_PREPARE_INPROGRESS:
 *	Update is in prepared phase.
 *
 * WT_PREPARE_LOCKED:
 *	State is locked as state transition is in progress from INPROGRESS to
 *	RESOLVED. Any reader of the state needs to wait for state transition to
 *	complete.
 *
 * WT_PREPARE_RESOLVED:
 *	Represents the commit state of the prepared update.
 *
 * State Transition:
 * 	From uncommitted -> prepare -> commit:
 * 	INIT --> INPROGRESS --> LOCKED --> RESOLVED
 * 	LOCKED will be a momentary phase during timestamp update.
 *
 * 	From uncommitted -> prepare -> rollback:
 * 	INIT --> INPROGRESS
 * 	Prepare state will not be updated during rollback and will continue to
 * 	have the state as INPROGRESS.
 */
#define WT_PREPARE_INIT              \
    0 /* Must be 0, as structures    \
         will be default initialized \
         with 0. */
//赋值见__page_inmem_prepare_update  __wt_txn_prepare
//__wt_txn_prepare prepare的事务会释放快照，这样就会影响evict和可见性，因此prepare节点释放快照，这样可以减轻内存压力
// 但是可见性会不一致，因此会增加对WT_PREPARE_INPROGRESS的WT_VISIBLE_PREPARE(见__wt_txn_upd_visible_type)，并增加udp
// WT_PREPARE_CONFLICT冲突检查(见__wt_txn_read_upd_list_internal)
#define WT_PREPARE_INPROGRESS 1
//赋值见__txn_resolve_prepared_update
#define WT_PREPARE_LOCKED 2
//赋值见__txn_resolve_prepared_update  把udp链表上面属于本session事务正在操作的udp节点的状态置为WT_PREPARE_RESOLVED
#define WT_PREPARE_RESOLVED 3

/*
 * Page state.
 *
 * Synchronization is based on the WT_REF->state field, which has a number of
 * possible states:
 *
 * WT_REF_DISK:
 *	The initial setting before a page is brought into memory, and set as a
 *	result of page eviction; the page is on disk, and must be read into
 *	memory before use.  WT_REF_DISK has a value of 0 (the default state
 *	after allocating cleared memory).
 *
 * WT_REF_DELETED:
 *	The page is on disk, but has been deleted from the tree; we can delete
 *	row-store and VLCS leaf pages without reading them if they don't
 *	reference overflow items.
 *
 * WT_REF_LOCKED:
 *	Locked for exclusive access.  In eviction, this page or a parent has
 *	been selected for eviction; once hazard pointers are checked, the page
 *	will be evicted.  When reading a page that was previously deleted, it
 *	is locked until the page is in memory and the deletion has been
 *      instantiated with tombstone updates. The thread that set the page to
 *      WT_REF_LOCKED has exclusive access; no other thread may use the WT_REF
 *      until the state is changed.
 *
 * WT_REF_MEM:
 *	Set by a reading thread once the page has been read from disk; the page
 *	is in the cache and the page reference is OK.
 *
 * WT_REF_SPLIT:
 *	Set when the page is split; the WT_REF is dead and can no longer be
 *	used.
 *
 * The life cycle of a typical page goes like this: pages are read into memory
 * from disk and their state set to WT_REF_MEM.  When the page is selected for
 * eviction, the page state is set to WT_REF_LOCKED.  In all cases, evicting
 * threads reset the page's state when finished with the page: if eviction was
 * successful (a clean page was discarded, and a dirty page was written to disk
 * and then discarded), the page state is set to WT_REF_DISK; if eviction failed
 * because the page was busy, page state is reset to WT_REF_MEM.
 *
 * Readers check the state field and if it's WT_REF_MEM, they set a hazard
 * pointer to the page, flush memory and re-confirm the page state.  If the
 * page state is unchanged, the reader has a valid reference and can proceed.
 *
 * When an evicting thread wants to discard a page from the tree, it sets the
 * WT_REF_LOCKED state, flushes memory, then checks hazard pointers.  If a
 * hazard pointer is found, state is reset to WT_REF_MEM, restoring the page
 * to the readers.  If the evicting thread does not find a hazard pointer,
 * the page is evicted.
 */

/*
 * WT_PAGE_DELETED --
 *	Information about how they got deleted for deleted pages. This structure records the
 *      transaction that deleted the page, plus the state the ref was in when the deletion happened.
 *      This structure is akin to an update but applies to a whole page.
 */
struct __wt_page_deleted {
    /*
     * Transaction IDs are set when updates are created (before they become visible) and only change
     * when marked with WT_TXN_ABORTED. Transaction ID readers expect to copy a transaction ID into
     * a local variable and see a stable value. In case a compiler might re-read the transaction ID
     * from memory rather than using the local variable, mark the shared transaction IDs volatile to
     * prevent unexpected repeated/reordered reads.
     */
    volatile uint64_t txnid; /* Transaction ID */

    wt_timestamp_t timestamp; /* Timestamps */
    wt_timestamp_t durable_timestamp;

    /*
     * The prepare state is used for transaction prepare to manage visibility and propagating the
     * prepare state to the updates generated at instantiation time.
     */
    volatile uint8_t prepare_state;

    /*
     * The previous state of the WT_REF; if the fast-truncate transaction is rolled back without the
     * page first being instantiated, this is the state to which the WT_REF returns.
     */
    uint8_t previous_ref_state;

    /*
     * If the fast-truncate transaction has committed. If we're forced to instantiate the page, and
     * the committed flag isn't set, we have to create an update structure list for the transaction
     * to resolve in a subsequent commit. (This is tricky: if the transaction is rolled back, the
     * entire structure is discarded, that is, the flag is set only on commit and not on rollback.)
     */
    bool committed;

    /* Flag to indicate fast-truncate is written to disk. */
    bool selected_for_write;
};

/*
 * WT_REF_HIST --
 *	State information of a ref at a single point in time.
 */
struct __wt_ref_hist {
    WT_SESSION_IMPL *session;
    const char *name;
    const char *func;
    uint32_t time_sec;
    uint16_t line;
    uint16_t state;
};

/*
 * WT_REF --
 *	A single in-memory page and state information.
 //To access a page in the B-Tree, we require a WT_REF which tracks whether the page has or has not been loaded from storage
 //

 //Each page in the cache is accessed via a WT_REF structure. When WiredTiger opens a Btree, it places a WT_REF for the
 //cached root page in the corresponding WT_BTREE structure. A WT_REF can represent either a page in the cache or one that
 //has not been loaded yet. The page itself is represented by a WT_PAGE structure. This includes a pointer to a buffer that
 //contains the on-disk page image (decrypted and uncompressed). It also holds the supplemental structures that WiredTiger
 //uses to access and update the page while it is cached.

 An internal Btree page will have an array of WT_REF structures. A row-store leaf page will have an array of WT_ROW
 structures representing the KV pairs stored on the page.
 BTREE图形化参考https://github.com/wiredtiger/wiredtiger/wiki/In-Memory-Tree-Layout
 */ //btree对应root page,赋值参考__btree_tree_open_empty
struct __wt_ref {
    WT_PAGE *page; /* Page */

    /*
     * When the tree deepens as a result of a split, the home page value changes. Don't cache it, we
     * need to see that change when looking up our slot in the page's index structure.
     */
    WT_PAGE *volatile home;        /* Reference page */
    //代表该ref在父page index[]数组的位置
    volatile uint32_t pindex_hint; /* Reference page index hint */

    uint8_t unused[2]; /* Padding: before the flags field so flags can be easily expanded. */

/*
 * Define both internal- and leaf-page flags for now: we only need one, but it provides an easy way
 * to assert a page-type flag is always set (we allocate WT_REFs in lots of places and it's easy to
 * miss one). If we run out of bits in the flags field, remove the internal flag and rewrite tests
 * depending on it to be "!leaf" instead.
 */
/* AUTOMATIC FLAG VALUE GENERATION START 0 */
#define WT_REF_FLAG_INTERNAL 0x1u /* Page is an internal page */
#define WT_REF_FLAG_LEAF 0x2u     /* Page is a leaf page */
#define WT_REF_FLAG_READING 0x4u  /* Page is being read in */
                                  /* AUTOMATIC FLAG VALUE GENERATION STOP 8 */
    uint8_t flags;

//标识该page数据在disk中,evict把内存page写入磁盘后在__wt_multi_to_ref中置为该状态
#define WT_REF_DISK 0       /* Page is on disk */
//__btree_tree_open_empty创建root page的时候初始化为该值
#define WT_REF_DELETED 1    /* Page is on disk, but deleted */
//evict worker线程在__evict_get_ref中设置page状态为WT_REF_LOCKED 
//用户线程在__wt_page_release_evict中设置page状态为WT_REF_LOCKED 
//在整个__wt_evict期间，所有访问该page的用户线程都会在__wt_page_in_func中因为处于WT_REF_LOCKED而一直等待，因为split会破坏原有的ref page结构，一拆多
#define WT_REF_LOCKED 2     /* Page locked for exclusive access */
//新创建的leaf page就会设置为该状态  __wt_btree_new_leaf_page创建leaf page后，该leaf page对应状态为WT_REF_MEM
//当reconcile evict拆分page为多个，并且写入磁盘ext，这时候page状态进入WT_REF_DISK, 当unpack解包获取到该ext的所有K或者V在相比ext头部
//偏移量后，重新置为WT_REF_MEM状态，表示我们已经获取到ext中包含的所有K和V磁盘元数据地址存储到了内存pg_row中，参考__wt_multi_to_ref
#define WT_REF_MEM 3        /* Page is in cache and valid */

//__split_parent_discard_ref置为该状态，说明该parent ref因为split需要被释放
//__wt_page_release_evict进行split的时候会进入该状态，如果__wt_page_release_evict split成功，则拆分前的ref状态会在
//  __split_multi->__wt_multi_to_ref生成新的ref_new[]，原来的ref会通过__split_safe_free->__wt_stash_add加入等待释放队列并置为WT_REF_SPLIT
//  下一轮while会判断WT_REF_SPLIT然后返回WT_REF_SPLIT，最终在外层__wt_txn_commit->__wt_txn_release->__wt_stash_discard真正释放
#define WT_REF_SPLIT 4      /* Parent page split (WT_REF dead) */
    //WT_REF_SET_STATE WT_REF_CAS_STATE        WT_REF_DISK WT_REF_MEM(磁盘还是内存)赋值
    volatile uint8_t state; /* Page state */

    /*
     * Address: on-page cell if read from backing block, off-page WT_ADDR if instantiated in-memory,
     * or NULL if page created in-memory.
     */
    //通过ref->addr可以判断除该ref对应page是否罗盘了 __wt_ref_block_free
    //例如evict reconcile流程中的__wt_multi_to_ref，指向该page对应的磁盘ext元数据信息WT_ADDR(objectid offset size  checksum)
    //如果是root page，其addr=NULL, 如果page在内存中，例如internal page(不包括root page)或者leaf page在内存中，还没有持久化

    //重启时候从__inmem_row_int中从root元数据ext中加载每个page的磁盘addr信息
//可以参考在__wt_sync_file中会遍历所有的btree过程，看__wt_sync_file是如何遍历btree的，然后走到这里
/*
                        root page
                        /         \
                      /             \
                    /                 \
       internal-1 page             internal-2 page
         /      \                      /    \
        /        \                    /       \
       /          \                  /          \
leaf-1 page    leaf-2 page    leaf3 page      leaf4 page

上面这一棵树的遍历顺序: leaf1->leaf2->internal1->leaf3->leaf4->internal2->root
*/
//从上面的图可以看出，internal page(root+internal1+internal2)总共三次走到这里, internal1记录leaf1和leaf2的page addr元数据[ref key, leaf page ext元数据]
//  internal2记录leaf3和leaf4的page addr元数据[ref key, leaf page ext元数据],
//  root记录internal1和internal2的 addr元数据[ref key, leaf page ext元数据],

    //注意__wt_rec_row_leaf和__wt_rec_row_int的差异，一个是持久化leaf page，一个是持久化internal page
    //  __wt_rec_row_int: 持久化internal page拥有的子ref page key及其所有子page持久化到磁盘的ext(objectid offset size  checksum)元数据信息, 
    //      当重启(__wt_btree_tree_open->__wt_page_inmem)或者读取interanal page数据(__page_read)的时候会把这些持久化的ref page key和所有持久化的page元数据加载到page->dsk中
    //  __wt_rec_row_leaf: 持久化leaf page数据到磁盘，同时把磁盘ext元数据(objectid offset size  checksum)信息添加到ref.addr
    void *addr;//对应WT_ADDR，参考__wt_multi_to_ref, 存的是该ref page写入磁盘的ext元数据信息(objectid offset size  checksum)

    /*
     * The child page's key.  Do NOT change this union without reviewing
     * __wt_ref_key.
     */
    union {
        uint64_t recno; /* Column-store: starting recno */
        void *ikey;     /* Row-store: key */
    } key;
#undef ref_recno
#define ref_recno key.recno
#undef ref_ikey
////WT_REF.ref_ikey ref对应的key，__wt_row_ikey  __wt_ref_key_onpage_set中赋值
//记录的是对应page的最小key
#define ref_ikey key.ikey

    /*
     * Page deletion information, written-to/read-from disk as necessary in the internal page's
     * address cell. (Deleted-address cells are also referred to as "proxy cells".) When a WT_REF
     * first becomes part of a fast-truncate operation, the page_del field is allocated and
     * initialized; it is similar to an update and holds information about the transaction that
     * performed the truncate. It can be discarded and set to NULL when that transaction reaches
     * global visibility.
     *
     * Operations other than truncate that produce deleted pages (checkpoint cleanup, reconciliation
     * as empty, etc.) leave the page_del field NULL as in these cases the deletion is already
     * globally visible.
     *
     * Once the deletion is globally visible, the original on-disk page is no longer needed and can
     * be discarded; this happens the next time the parent page is reconciled, either by eviction or
     * by a checkpoint. The ref remains, however, and still occupies the same key space in the table
     * that it always did.
     *
     * Deleted refs (and thus chunks of the tree namespace) are only discarded at two points: when
     * the parent page is discarded after being evicted, or in the course of internal page splits
     * and reverse splits. Until this happens, the "same" page can be brought back to life by
     * writing to its portion of the key space.
     *
     * A deleted page needs to be "instantiated" (read in from disk and converted to an in-memory
     * page where every item on the page has been individually deleted) if we need to position a
     * cursor on the page, or if we need to visit it for other reasons. Logic exists to avoid that
     * in various common cases (see: __wt_btcur_skip_page, __wt_delete_page_skip) but in many less
     * common situations we proceed with instantiation anyway to avoid multiplying the number of
     * special cases in the system.
     *
     * Common triggers for instantiation include: another thread reading from the page before a
     * truncate commits; an older reader visiting a page after a truncate commits; a thread reading
     * the page via a checkpoint cursor if the truncation wasn't yet globally visible at checkpoint
     * time; a thread reading the page after shutdown and restart under similar circumstances; RTS
     * needing to roll back a committed but unstable truncation (and possibly also updates that
     * occurred before the truncation); and a thread writing to the truncated portion of the table
     * space after the truncation but before the page is completely discarded.
     *
     * If the page must be instantiated for any reason: (1) for each entry on the page a WT_UPDATE
     * is created; (2) the transaction information from page_del is copied to those WT_UPDATE
     * structures (making them a match for the truncate operation), and (3) the WT_REF state
     * switches to WT_REF_MEM.
     *
     * If the fast-truncate operation has not yet committed, an array of references to the WT_UPDATE
     * structures is placed in modify->inst_updates. This is used to find the updates when the
     * operation subsequently resolves. (The page can split, so there needs to be some way to find
     * all of the update structures.)
     *
     * After instantiation, the page_del structure is kept until the instantiated page is next
     * reconciled. This is because in some cases reconciliation of the parent internal page may need
     * to write out a reference to the pre-instantiated on-disk page, at which point the page_del
     * information is needed to build the correct reference.
     *
     * If the ref is in WT_REF_DELETED state, all actions besides checking whether page_del is NULL
     * require that the WT_REF be locked. There are two reasons for this: first, the page might be
     * instantiated at any time, and it is important to not see a partly-completed instantiation;
     * and second, the page_del structure is discarded opportunistically if its transaction is found
     * to be globally visible, so accessing it without locking the ref is unsafe.
     *
     * If the ref is in WT_REF_MEM state because it has been instantiated, the safety requirements
     * are somewhat looser. Checking for an instantiated page by examining modify->instantiated does
     * not require locking. Checking if modify->inst_updates is non-NULL (which means that the
     * truncation isn't committed) also doesn't require locking. In general the page_del structure
     * should not be used after instantiation; exceptions are (a) it is still updated by transaction
     * prepare, commit, and rollback (so that it remains correct) and (b) it is used by internal
     * page reconciliation if that occurs before the instantiated child is itself reconciled. (The
     * latter can only happen if the child is evicted in a fairly narrow time window during a
     * checkpoint.) This still requires locking the ref.
     *
     * It is vital to consider all the possible cases when touching a deleted or instantiated page.
     *
     * There are two major groups of states:
     *
     * 1. The WT_REF state is WT_REF_DELETED. This means the page is deleted and not in memory.
     *    - If the page has no disk address, the ref is a placeholder in the key space and may in
     *      general be discarded at the next opportunity. (Some restrictions apply in VLCS.)
     *    - If the page has a disk address, page_del may be NULL. In this case, the deletion of the
     *      page is globally visible and the on-disk page can be discarded at the next opportunity.
     *    - If the page has a disk address and page_del is not NULL, page_del contains information
     *      about the transaction that deleted the page. It is necessary to lock the ref to read
     *      page_del; at that point (if the state hasn't changed while getting the lock)
     *      page_del->committed can be used to check if the transaction is committed or not.
     *
     * 2. The WT_REF state is WT_REF_MEM. The page is either an ordinary page or an instantiated
     * deleted page.
     *    - If ref->page->modify is NULL, the page is ordinary.
     *    - If ref->page->modify->instantiated is false and ref->page->modify->inst_updates is NULL,
     *      the page is ordinary.
     *    - If ref->page->modify->instantiated is true, the page is instantiated and has not yet
     *      been reconciled. ref->page_del is either NULL (meaning the deletion is globally visible)
     *      or contains information about the transaction that deleted the page. This information is
     *      only meaningful either (a) in relation to the existing on-disk page rather than the in-
     *      memory page (this can be needed to reconcile the parent internal page) or (b) if the
     *      page is clean.
     *    - If ref->page->modify->inst_updates is not NULL, the page is instantiated and the
     *      transaction that deleted it has not resolved yet. The update list is used during commit
     *      or rollback to find the updates created during instantiation.
     *
     * The last two points of group (2) are orthogonal; that is, after instantiation the
     * instantiated flag and page_del structure (on the one hand) and the update list (on the other)
     * are used and discarded independently. The former persists only until the page is first
     * successfully reconciled; the latter persists until the transaction resolves. These events may
     * occur in either order.
     *
     * As described above, in any state in group (1) an access to the page may require it be read
     * into memory, at which point it moves into group (2). Instantiation always sets the
     * instantiated flag to true; the updates list is only created if the transaction has not yet
     * resolved at the point instantiation happens. (The ref is locked in both transaction
     * resolution and instantiation to make sure these events happen in a well-defined order.)
     *
     * Because internal pages with uncommitted (including prepared) deletions are not written to
     * disk, a page instantiated after its parent was read from disk will always have inst_updates
     * set to NULL.
     */
    WT_PAGE_DELETED *page_del; /* Page-delete information for a deleted page. */

#ifdef HAVE_REF_TRACK
/*
 * In DIAGNOSTIC mode we overwrite the WT_REF on free to force failures, but we want to retain ref
 * state history. Don't overwrite these fields.
 */
#define WT_REF_CLEAR_SIZE (offsetof(WT_REF, hist))
#define WT_REF_SAVE_STATE_MAX 3
    /* Capture history of ref state changes. */
    WT_REF_HIST hist[WT_REF_SAVE_STATE_MAX];
    uint64_t histoff;
#define WT_REF_SAVE_STATE(ref, s, f, l)                                   \
    do {                                                                  \
        (ref)->hist[(ref)->histoff].session = session;                    \
        (ref)->hist[(ref)->histoff].name = session->name;                 \
        __wt_seconds32(session, &(ref)->hist[(ref)->histoff].time_sec);   \
        (ref)->hist[(ref)->histoff].func = (f);                           \
        (ref)->hist[(ref)->histoff].line = (uint16_t)(l);                 \
        (ref)->hist[(ref)->histoff].state = (uint16_t)(s);                \
        (ref)->histoff = ((ref)->histoff + 1) % WT_ELEMENTS((ref)->hist); \
    } while (0)
#define WT_REF_SET_STATE(ref, s)                                  \
    do {                                                          \
        WT_REF_SAVE_STATE(ref, s, __PRETTY_FUNCTION__, __LINE__); \
        WT_PUBLISH((ref)->state, s);                              \
    } while (0)
#else
#define WT_REF_CLEAR_SIZE (sizeof(WT_REF))
#define WT_REF_SET_STATE(ref, s) WT_PUBLISH((ref)->state, s)
#endif
};

/*
 * WT_REF_SIZE is the expected structure size -- we verify the build to ensure the compiler hasn't
 * inserted padding which would break the world.
 */
#ifdef HAVE_REF_TRACK
#define WT_REF_SIZE (48 + WT_REF_SAVE_STATE_MAX * sizeof(WT_REF_HIST) + 8)
#else
#define WT_REF_SIZE 48
#endif

/* A macro wrapper allowing us to remember the callers code location */
#define WT_REF_CAS_STATE(session, ref, old_state, new_state) \
    __wt_ref_cas_state_int(session, ref, old_state, new_state, __PRETTY_FUNCTION__, __LINE__)

//WT_REF_LOCK和WT_REF_UNLOCK对应
#define WT_REF_LOCK(session, ref, previous_statep)                             \
    do {                                                                       \
        uint8_t __previous_state;                                              \
        for (;; __wt_yield()) {                                                \
            __previous_state = (ref)->state;                                   \
            if (__previous_state != WT_REF_LOCKED &&                           \
              WT_REF_CAS_STATE(session, ref, __previous_state, WT_REF_LOCKED)) \
                break;                                                         \
        }                                                                      \
        *(previous_statep) = __previous_state;                                 \
    } while (0)

//WT_REF_LOCK和WT_REF_UNLOCK对应
#define WT_REF_UNLOCK(ref, state) WT_REF_SET_STATE(ref, state)

/*
 * WT_ROW --
 * Each in-memory page row-store leaf page has an array of WT_ROW structures:
 * this is created from on-page data when a page is read from the file.  It's
 * sorted by key, fixed in size, and starts with a reference to on-page data.
 *
 * Multiple threads of control may be searching the in-memory row-store pages,
 * and the key may be instantiated at any time.  Code must be able to handle
 * both when the key has not been instantiated (the key field points into the
 * page's disk image), and when the key has been instantiated (the key field
 * points outside the page's disk image).  We don't need barriers because the
 * key is updated atomically, but code that reads the key field multiple times
 * is a very, very bad idea.  Specifically, do not do this:
 *
 *	key = rip->key;
 *	if (key_is_on_page(key)) {
 *		cell = rip->key;
 *	}
 *
 * The field is declared volatile (so the compiler knows it shouldn't read it
 * multiple times), and we obscure the field name and use a copy macro in all
 * references to the field (so the code doesn't read it multiple times), all
 * to make sure we don't introduce this bug (again).
 */
//代表磁盘上面的一条KV数据，参考__debug_page_row_leaf
struct __wt_row { /* On-page key, on-page cell, or off-page WT_IKEY */
    //真正赋值在WT_ROW_KEY_SET
    void *volatile __key;
};
#define WT_ROW_KEY_COPY(rip) ((rip)->__key)
#define WT_ROW_KEY_SET(rip, v) ((rip)->__key) = (void *)(v)

/*
 * WT_ROW_FOREACH --
 *	Walk the entries of an in-memory row-store leaf page.
 */
//pg_row指向磁盘KV相关数据，mod_row_insert指向内存相关KV数据，mod_row_update记录内存中同一个K的变更过程
#define WT_ROW_FOREACH(page, rip, i) \
    for ((i) = (page)->entries, (rip) = (page)->pg_row; (i) > 0; ++(rip), --(i))
#define WT_ROW_FOREACH_REVERSE(page, rip, i)                                             \
    for ((i) = (page)->entries, (rip) = (page)->pg_row + ((page)->entries - 1); (i) > 0; \
         --(rip), --(i))

/*
 * WT_ROW_SLOT --
 *	Return the 0-based array offset based on a WT_ROW reference.
 */
//也就是标识rip对应KV在page内存pg_row中的游标
#define WT_ROW_SLOT(page, rip) ((uint32_t)((rip) - (page)->pg_row))

/*
 * WT_COL -- Each in-memory variable-length column-store leaf page has an array of WT_COL
 * structures: this is created from on-page data when a page is read from the file. It's fixed in
 * size, and references data on the page.
 */
struct __wt_col {
    /*
     * Variable-length column-store data references are page offsets, not pointers (we boldly
     * re-invent short pointers). The trade-off is 4B per K/V pair on a 64-bit machine vs. a single
     * cycle for the addition of a base pointer. The on-page data is a WT_CELL (same as row-store
     * pages).
     *
     * Obscure the field name, code shouldn't use WT_COL->__col_value, the public interface is
     * WT_COL_PTR and WT_COL_PTR_SET.
     */
    uint32_t __col_value;
};

/*
 * WT_COL_PTR, WT_COL_PTR_SET --
 *	Return/Set a pointer corresponding to the data offset. (If the item does
 * not exist on the page, return a NULL.)
 */
#define WT_COL_PTR(page, cip) WT_PAGE_REF_OFFSET(page, (cip)->__col_value)
#define WT_COL_PTR_SET(cip, value) (cip)->__col_value = (value)

/*
 * WT_COL_FOREACH --
 *	Walk the entries of variable-length column-store leaf page.
 */
#define WT_COL_FOREACH(page, cip, i) \
    for ((i) = (page)->entries, (cip) = (page)->pg_var; (i) > 0; ++(cip), --(i))

/*
 * WT_COL_SLOT --
 *	Return the 0-based array offset based on a WT_COL reference.
 */
#define WT_COL_SLOT(page, cip) ((uint32_t)((cip) - (page)->pg_var))

/*
 * WT_IKEY --
 *  Instantiated key: row-store keys are usually prefix compressed or overflow objects.
 *  Normally, a row-store page in-memory key points to the on-page WT_CELL, but in some
 *  cases, we instantiate the key in memory, in which case the row-store page in-memory
 *  key points to a WT_IKEY structure.
 */ //__wt_row_ikey_alloc参考，ref存的真实page上最小数据key
struct __wt_ikey {
    //key长度
    uint32_t size; /* Key length */

    /*
     * If we no longer point to the key's on-page WT_CELL, we can't find its
     * related value.  Save the offset of the key cell in the page.
     *
     * Row-store cell references are page offsets, not pointers (we boldly
     * re-invent short pointers).  The trade-off is 4B per K/V pair on a
     * 64-bit machine vs. a single cycle for the addition of a base pointer.
     */
    //记录K在内存中的位置，参考__wt_row_ikey_alloc
    //不为0则对应磁盘page数据加载到内存中page->dsk的位置
    //表示该ref key对应的ref下面的page已经写入到磁盘或者从磁盘加载的
    uint32_t cell_offset;

/* The key bytes immediately follow the WT_IKEY structure. */
#define WT_IKEY_DATA(ikey) ((void *)((uint8_t *)(ikey) + sizeof(WT_IKEY)))
};

/*
 * WT_UPDATE --
 *
 * Entries on leaf pages can be updated, either modified or deleted. Updates to entries in the
 * WT_ROW and WT_COL arrays are stored in the page's WT_UPDATE array. When the first element on a
 * page is updated, the WT_UPDATE array is allocated, with one slot for every existing element in
 * the page. A slot points to a WT_UPDATE structure; if more than one update is done for an entry,
 * WT_UPDATE structures are formed into a forward-linked list.
  参考官方文档https://source.wiredtiger.com/develop/arch-cache.html

 */ //分配__wt_upd_alloc空间
//分配__wt_upd_alloc空间 //KV中的key对应WT_INSERT，value对应WT_UPDATE(WT_INSERT.upd)
//__wt_insert.upd为该类型,记录的是V的变化过程
struct __wt_update {
    /*
     * Transaction IDs are set when updates are created (before they become visible) and only change
     * when marked with WT_TXN_ABORTED. Transaction ID readers expect to copy a transaction ID into
     * a local variable and see a stable value. In case a compiler might re-read the transaction ID
     * from memory rather than using the local variable, mark the shared transaction IDs volatile to
     * prevent unexpected repeated/reordered reads.
     */

    //事务提交的时候会在__wt_txn_commit->__wt_txn_release中置session对应事务id WT_SESSION_IMPL->txn->id为WT_TXN_NONE 
    //upd->txnid为修改该值对应的事务id(__wt_txn_modify)，该id值不会因为事务提交置为0
     
    //该upd对应的事务id，赋值见__wt_txn_modify,同一个事务中的写操作txnid一样
    volatile uint64_t txnid; /* transaction ID */


    //注意只有关闭了oplog(log=(enabled=false))功能的表才会有upd timestamp功能
    //只有设置了commit_timestamp并且没有启用WAL功能，durable_ts和start_ts功能才会有效，参考__wt_txn_op_set_timestamp
    //__wt_txn_op_set_timestamp记录该操作在事务中提交的时间点durable_timestamp记录到durable_ts中,最终在WT_TIME_WINDOW_SET_START中使用
    wt_timestamp_t durable_ts; /* timestamps */ //如果是prepare_transaction则赋值在__txn_resolve_prepared_update
    //__wt_txn_op_set_timestamp记录该操作在事务中提交的时间点commit_timestamp记录到start_ts中,最终在WT_TIME_WINDOW_SET_STOP中使用
    //注意只有关闭了oplog(log=(enabled=false))功能的表才会有upd timestamp功能
    wt_timestamp_t start_ts; //如果是prepare_transaction则赋值在__txn_resolve_prepared_update  __wt_txn_prepare和__wt_txn_op_delete_apply_prepare_state

    /*
     * The durable timestamp of the previous update in the update chain. This timestamp is used for
     * diagnostic checks only, and could be removed to reduce the size of the structure should that
     * be necessary.
     */
    //上一次更新该key的时间戳
    wt_timestamp_t prev_durable_ts;

    WT_UPDATE *next; /* forward-linked list */

    uint32_t size; /* data length */

#define WT_UPDATE_INVALID 0   /* diagnostic check */
//__wt_hs_insert_updates中设置
#define WT_UPDATE_MODIFY 1    /* partial-update modify value */
//__wt_btcur_reserve中设置，可以先不关注
#define WT_UPDATE_RESERVE 2   /* reserved */
//一般都是这个状态,普通更新
#define WT_UPDATE_STANDARD 3  /* complete value */
//说明是删除操作 __wt_upd_alloc_tombstone创建delete udp
#define WT_UPDATE_TOMBSTONE 4 /* deleted */
    //代表是什么类型的更新，是普通更新还是删除操作，还是部分更新
    uint8_t type;             /* type (one byte to conserve memory) */

/* If the update includes a complete value. */
#define WT_UPDATE_DATA_VALUE(upd) \
    ((upd)->type == WT_UPDATE_STANDARD || (upd)->type == WT_UPDATE_TOMBSTONE)

    /*
     * The update state is used for transaction prepare to manage visibility and transitioning
     * update structure state safely.
     */
    volatile uint8_t prepare_state; /* prepare state */

/* AUTOMATIC FLAG VALUE GENERATION START 0 */
#define WT_UPDATE_DS 0x01u                       /* Update has been written to the data store. */
#define WT_UPDATE_HS 0x02u                       /* Update has been written to history store. */
#define WT_UPDATE_PREPARE_RESTORED_FROM_DS 0x04u /* Prepared update restored from data store. */
//__tombstone_update_alloc中置位
#define WT_UPDATE_RESTORED_FAST_TRUNCATE 0x08u   /* Fast truncate instantiation */
#define WT_UPDATE_RESTORED_FROM_DS 0x10u         /* Update restored from data store. */
#define WT_UPDATE_RESTORED_FROM_HS 0x20u         /* Update restored from history store. */
#define WT_UPDATE_TO_DELETE_FROM_HS 0x40u        /* Update needs to be deleted from history store */
                                                 /* AUTOMATIC FLAG VALUE GENERATION STOP 8 */
    uint8_t flags;

    /*
     * Zero or more bytes of value (the payload) immediately follows the WT_UPDATE structure. We use
     * a C99 flexible array member which has the semantics we want.
     */
    //value数据在这里
    uint8_t data[]; /* start of the data */
};

/*
 * WT_UPDATE_SIZE is the expected structure size excluding the payload data -- we verify the build
 * to ensure the compiler hasn't inserted padding.
 */
#define WT_UPDATE_SIZE 47

/*
 * The memory size of an update: include some padding because this is such a common case that
 * overhead of tiny allocations can swamp our cache overhead calculation.
 */
#define WT_UPDATE_MEMSIZE(upd) WT_ALIGN(WT_UPDATE_SIZE + (upd)->size, 32)

/*
 * WT_UPDATE_VALUE --
 *
 * A generic representation of an update's value regardless of where it exists. This structure is
 * used to represent both in-memory updates and updates that don't exist in an update list such as
 * reconstructed modify updates, updates in the history store and onpage values.
 *
 * The skip buffer flag is an optimization for callers of various read functions to communicate that
 * they just want to check that an update exists and not read its underlying value. This means that
 * the read functions can avoid the performance penalty of reconstructing modifies.
 */ //__wt_cursor_btree.modify_update为该类型
struct __wt_update_value {
    WT_ITEM buf;
    //赋值可以参考__wt_upd_value_assign
    WT_TIME_WINDOW tw;
    uint8_t type;
    bool skip_buf;
};

/*
 * WT_WITH_UPDATE_VALUE_SKIP_BUF --
 *
 * A helper macro to use for calling read functions when we're checking for the existence of a given
 * key. This means that read functions can avoid the performance penalty of reconstructing modifies.
 */
#define WT_WITH_UPDATE_VALUE_SKIP_BUF(op) \
    do {                                  \
        cbt->upd_value->skip_buf = true;  \
        op;                               \
        cbt->upd_value->skip_buf = false; \
    } while (0)

/*
 * WT_MODIFY_UPDATE_MIN/MAX, WT_MODIFY_VECTOR_STACK_SIZE
 *	Limit update chains value to avoid penalizing reads and permit truncation. Having a smaller
 * value will penalize the cases when history has to be maintained, resulting in multiplying cache
 * pressure.
 *
 * When threads race modifying a record, we can end up with more than the usual maximum number of
 * modifications in an update list. We use small vectors of modify updates in a couple of places to
 * avoid heap allocation, add a few additional slots to that array.
 */
#define WT_MODIFY_UPDATE_MIN 10  /* Update count before we bother checking anything else */
#define WT_MODIFY_UPDATE_MAX 200 /* Update count hard limit */
#define WT_UPDATE_VECTOR_STACK_SIZE (WT_MODIFY_UPDATE_MIN + 10)

/*
 * WT_UPDATE_VECTOR --
 * 	A resizable array for storing updates. The allocation strategy is similar to that of
 *	llvm::SmallVector<T> where we keep space on the stack for the regular case but fall back to
 *	dynamic allocation as needed.
 */
struct __wt_update_vector {
    WT_SESSION_IMPL *session;
    WT_UPDATE *list[WT_UPDATE_VECTOR_STACK_SIZE];
    WT_UPDATE **listp;
    size_t allocated_bytes;
    size_t size;
};

/*
 * WT_MODIFY_MEM_FRACTION
 *	Limit update chains to a fraction of the base document size.
 */
#define WT_MODIFY_MEM_FRACTION 10

/*
 * WT_INSERT --
 *
 * Row-store leaf pages support inserts of new K/V pairs. When the first K/V pair is inserted, the
 * WT_INSERT_HEAD array is allocated, with one slot for every existing element in the page, plus one
 * additional slot. A slot points to a WT_INSERT_HEAD structure for the items which sort after the
 * WT_ROW element that references it and before the subsequent WT_ROW element; the skiplist
 * structure has a randomly chosen depth of next pointers in each inserted node.
 *
 * The additional slot is because it's possible to insert items smaller than any existing key on the
 * page: for that reason, the first slot of the insert array holds keys smaller than any other key
 * on the page.
 *
 * In column-store variable-length run-length encoded pages, a single indx entry may reference a
 * large number of records, because there's a single on-page entry representing many identical
 * records. (We don't expand those entries when the page comes into memory, as that would require
 * resources as pages are moved to/from the cache, including read-only files.) Instead, a single
 * indx entry represents all of the identical records originally found on the page.
 *
 * Modifying (or deleting) run-length encoded column-store records is hard because the page's entry
 * no longer references a set of identical items. We handle this by "inserting" a new entry into the
 * insert array, with its own record number. (This is the only case where it's possible to insert
 * into a column-store: only appends are allowed, as insert requires re-numbering subsequent
 * records. Berkeley DB did support mutable records, but it won't scale and it isn't useful enough
 * to re-implement, IMNSHO.)
  跳跃表图解参考https://www.jb51.net/article/199510.htm

 */
//__wt_row_insert_alloc  WT_INSERT头部+level空间+真实数据key
//mod_row_insert为该类型，一个insertKV对应的__wt_insert都在mod_row_insert对应跳跃表中
struct __wt_insert {
    //是一个链表结构，存储插入进来的这个K的多个V版本
    WT_UPDATE *upd; /* value */ //value在这里  __wt_insert.upd为该类型

    union {
        uint64_t recno; /* column-store record number */
        struct {
            //真实key其实地址
            uint32_t offset; /* row-store key data start */
            //key大小
            uint32_t size;   /* row-store key data size */
        } key; //key记录到这里，value在upd中
    } u;

#define WT_INSERT_KEY_SIZE(ins) (((WT_INSERT *)(ins))->u.key.size)
#define WT_INSERT_KEY(ins) ((void *)((uint8_t *)(ins) + ((WT_INSERT *)(ins))->u.key.offset))
#define WT_INSERT_RECNO(ins) (((WT_INSERT *)(ins))->u.recno)

    //数组大小和随机层数相关，参考__wt_row_insert_alloc
    WT_INSERT *next[0]; /* forward-linked skip list */
};

/*
 * Skiplist helper macros.
 */
#define WT_SKIP_FIRST(ins_head) \
    (((ins_head) == NULL) ? NULL : ((WT_INSERT_HEAD *)(ins_head))->head[0])
#define WT_SKIP_LAST(ins_head) \
    (((ins_head) == NULL) ? NULL : ((WT_INSERT_HEAD *)(ins_head))->tail[0])
#define WT_SKIP_NEXT(ins) ((ins)->next[0])
#define WT_SKIP_FOREACH(ins, ins_head) \
    for ((ins) = WT_SKIP_FIRST(ins_head); (ins) != NULL; (ins) = WT_SKIP_NEXT(ins))

/*
 * Atomically allocate and swap a structure or array into place.
 */
#define WT_PAGE_ALLOC_AND_SWAP(s, page, dest, v, count)                      \
    do {                                                                     \
        if (((v) = (dest)) == NULL) {                                        \
            WT_ERR(__wt_calloc_def(s, count, &(v)));                         \
            if (__wt_atomic_cas_ptr(&(dest), NULL, v))                       \
                __wt_cache_page_inmem_incr(s, page, (count) * sizeof(*(v))); \
            else                                                             \
                __wt_free(s, v);                                             \
        }                                                                    \
    } while (0)

/*
 * WT_INSERT_HEAD --
 * 	The head of a skiplist of WT_INSERT items.

 //场景1:
 // 该page没有数据在磁盘上面
 //   cbt->slot则直接用WT_ROW_INSERT_SLOT[0]这个跳表，
 
 //场景2:
 // 如果磁盘有数据则WT_ROW_INSERT_SMALLEST(mod_row_insert[(page)->entries])表示写入这个page并且K小于该page在
 //   磁盘上面最小的K的所有数据通过这个跳表存储起来
 
 //场景3:
 // 该page有数据在磁盘上面，例如该page在磁盘上面有两天数据ke1,key2...keyn，新插入keyx>page最大的keyn，则内存会维护一个
 //   cbt->slot为磁盘KV总数-1，这样大于该page的所有KV都会添加到WT_ROW_INSERT_SLOT[page->entries - 1]这个跳表上面
 
 //场景4:
 // 该page有数据在磁盘上面，例如该page在磁盘上面有两天数据ke1,key2,key3...keyn，新插入keyx大于key2小于key3, key2<keyx>key3，则内存会维护一个
 //   cbt->slot为磁盘key2在磁盘中的位置(也就是1，从0开始算)，这样大于该page的所有KV都会添加到WT_ROW_INSERT_SLOT[1]这个跳表
 
 //一个page在磁盘page->pg_row有多少数据(page->pg_row[]数组大小)，就会维护多少个跳表，因为要保证新写入内存的数据和磁盘的数据保持顺序
 
     配合WT_SKIP_FOREACH   WT_SKIP_LAST     WT_SKIP_FIRST阅读
 跳跃表图解参考https://www.jb51.net/article/199510.htm
 */ //WT_PAGE_ALLOC_AND_SWAP中会分配空间
struct __wt_insert_head {
    WT_INSERT *head[WT_SKIP_MAXDEPTH]; /* first item on skiplists */
    WT_INSERT *tail[WT_SKIP_MAXDEPTH]; /* last item on skiplists */
};

/*
 * The row-store leaf page insert lists are arrays of pointers to structures, and may not exist. The
 * following macros return an array entry if the array of pointers and the specific structure exist,
 * else NULL.
 */

//场景1:
// 该page没有数据在磁盘上面
//   cbt->slot则直接用WT_ROW_INSERT_SLOT[0]这个跳表，

//场景2:
// 如果磁盘有数据则WT_ROW_INSERT_SMALLEST(mod_row_insert[(page)->entries])表示写入这个page并且K小于该page在
//   磁盘上面最小的K的所有数据通过这个跳表存储起来

//场景3:
// 该page有数据在磁盘上面，例如该page在磁盘上面有两天数据ke1,key2...keyn，新插入keyx>page最大的keyn，则内存会维护一个
//   cbt->slot为磁盘KV总数-1，这样大于该page的所有KV都会添加到WT_ROW_INSERT_SLOT[page->entries - 1]这个跳表上面

//场景4:
// 该page有数据在磁盘上面，例如该page在磁盘上面有两天数据ke1,key2,key3...keyn，新插入keyx大于key2小于key3, key2<keyx>key3，则内存会维护一个
//   cbt->slot为磁盘key2在磁盘中的位置(也就是1，从0开始算)，这样大于该page的所有KV都会添加到WT_ROW_INSERT_SLOT[1]这个跳表

//一个page在磁盘page->pg_row有多少数据(page->pg_row[]数组大小)，就会维护多少个跳表，因为要保证新写入内存的数据和磁盘的数据保持顺序


//pg_row指向磁盘KV相关数据，mod_row_insert指向内存相关KV数据，mod_row_update记录内存中同一个K的变更过程
#define WT_ROW_INSERT_SLOT(page, slot)                                  \
    ((page)->modify == NULL || (page)->modify->mod_row_insert == NULL ? \
        NULL :                                                          \
        (page)->modify->mod_row_insert[slot])
#define WT_ROW_INSERT(page, ip) WT_ROW_INSERT_SLOT(page, WT_ROW_SLOT(page, ip))
#define WT_ROW_UPDATE(page, ip)                                         \
    ((page)->modify == NULL || (page)->modify->mod_row_update == NULL ? \
        NULL :                                                          \
        (page)->modify->mod_row_update[WT_ROW_SLOT(page, ip)])
/*
 * WT_ROW_INSERT_SMALLEST references an additional slot past the end of the "one per WT_ROW slot"
 * insert array. That's because the insert array requires an extra slot to hold keys that sort
 * before any key found on the original page.
 */
//pg_row指向磁盘KV相关数据，mod_row_insert指向内存相关KV数据，mod_row_update记录内存中同一个K的变更过程

//说明没有数据在磁盘上面，则写入的数据直接添加到mod_row_insert[0]这个跳表，如果磁盘有数据则WT_ROW_INSERT_SMALLEST表示写入这个page
// 并且K小于该page在磁盘上面最小的K的所有数据通过这个跳表存储起来
#define WT_ROW_INSERT_SMALLEST(page)                                    \
    ((page)->modify == NULL || (page)->modify->mod_row_insert == NULL ? \
        NULL :                                                          \
        (page)->modify->mod_row_insert[(page)->entries])

/*
 * The column-store leaf page update lists are arrays of pointers to structures, and may not exist.
 * The following macros return an array entry if the array of pointers and the specific structure
 * exist, else NULL.
 */
#define WT_COL_UPDATE_SLOT(page, slot)                                  \
    ((page)->modify == NULL || (page)->modify->mod_col_update == NULL ? \
        NULL :                                                          \
        (page)->modify->mod_col_update[slot])
#define WT_COL_UPDATE(page, ip) WT_COL_UPDATE_SLOT(page, WT_COL_SLOT(page, ip))

/*
 * WT_COL_UPDATE_SINGLE is a single WT_INSERT list, used for any fixed-length column-store updates
 * for a page.
 */
#define WT_COL_UPDATE_SINGLE(page) WT_COL_UPDATE_SLOT(page, 0)

/*
 * WT_COL_APPEND is an WT_INSERT list, used for fixed- and variable-length appends.
 */
#define WT_COL_APPEND(page)                                             \
    ((page)->modify == NULL || (page)->modify->mod_col_append == NULL ? \
        NULL :                                                          \
        (page)->modify->mod_col_append[0])

/* WT_COL_FIX_FOREACH_BITS walks fixed-length bit-fields on a disk page. */
#define WT_COL_FIX_FOREACH_BITS(btree, dsk, v, i)                            \
    for ((i) = 0,                                                            \
        (v) = (i) < (dsk)->u.entries ?                                       \
           __bit_getv(WT_PAGE_HEADER_BYTE(btree, dsk), 0, (btree)->bitcnt) : \
           0;                                                                \
         (i) < (dsk)->u.entries; ++(i),                                      \
        (v) = (i) < (dsk)->u.entries ?                                       \
           __bit_getv(WT_PAGE_HEADER_BYTE(btree, dsk), i, (btree)->bitcnt) : \
           0)

/*
 * FLCS pages with time information have a small additional header after the main page data that
 * holds a version number and cell count, plus the byte offset to the start of the cell data. The
 * latter values are limited by the page size, so need only be 32 bits. One hopes we'll never need
 * 2^32 versions.
 *
 * This struct is the in-memory representation. The number of entries is the number of time windows
 * (there are twice as many cells) and the offsets is from the beginning of the page. The space
 * between the empty offset and the data offset is not used and is expected to be zeroed.
 *
 * This structure is only used when handling on-disk pages; once the page is read in, one should
 * instead use the time window index in the page structure, which is a different type found above.
 */
struct __wt_col_fix_auxiliary_header {
    uint32_t version;
    uint32_t entries;
    uint32_t emptyoffset;
    uint32_t dataoffset;
};

/*
 * The on-disk auxiliary header uses a 1-byte version (the header must always begin with a nonzero
 * byte) and packed integers for the entry count and offset. To make the size of the offset entry
 * predictable (rather than dependent on the total page size) and also as small as possible, we
 * store the distance from the auxiliary data. To avoid complications computing the offset, we
 * include the offset's own storage space in the offset, and to make things simpler all around, we
 * include the whole auxiliary header in the offset; that is, the position of the auxiliary data is
 * computed as the position of the start of the auxiliary header plus the decoded stored offset.
 *
 * Both the entry count and the offset are limited to 32 bits because pages may not exceed 4G, so
 * their maximum encoded lengths are 5 each, so the maximum size of the on-disk header is 11 bytes.
 * It can be as small as 3 bytes, though.
 *
 * We reserve 7 bytes for the header on a full page (not 11) because on a full page the encoded
 * offset is the reservation size, and 7 encodes in one byte. This is enough for all smaller pages:
 * obviously if there's at least 4 extra bytes in the bitmap space any header will fit (4 + 7 = 11)
 * and if there's less the encoded offset is less than 11, which still encodes to one byte.
 */

#define WT_COL_FIX_AUXHEADER_RESERVATION 7
#define WT_COL_FIX_AUXHEADER_SIZE_MAX 11

/* Values for ->version. Version 0 never appears in an on-disk header. */
#define WT_COL_FIX_VERSION_NIL 0 /* Original page format with no timestamp data */
#define WT_COL_FIX_VERSION_TS 1  /* Upgraded format with cells carrying timestamp info */

/*
 * Manage split generation numbers. Splits walk the list of sessions to check when it is safe to
 * free structures that have been replaced. We also check that list periodically (e.g., when
 * wrapping up a transaction) to free any memory we can.
 *
 * Before a thread enters code that will examine page indexes (which are swapped out by splits), it
 * publishes a copy of the current split generation into its session. Don't assume that threads
 * never re-enter this code: if we already have a split generation, leave it alone. If our caller is
 * examining an index, we don't want the oldest split generation to move forward and potentially
 * free it.
 */
//用户保护page index, WT_INTL_INDEX_GET_SAFE
#define WT_ENTER_PAGE_INDEX(session)                                         \
    do {                                                                     \
        uint64_t __prev_split_gen = __wt_session_gen(session, WT_GEN_SPLIT); \
        if (__prev_split_gen == 0)                                           \
            __wt_session_gen_enter(session, WT_GEN_SPLIT);

#define WT_LEAVE_PAGE_INDEX(session)                   \
    if (__prev_split_gen == 0)                         \
        __wt_session_gen_leave(session, WT_GEN_SPLIT); \
    }                                                  \
    while (0)

//保证安全操作page index
//用户保护page index, WT_INTL_INDEX_GET_SAFE
#define WT_WITH_PAGE_INDEX(session, e) \
    WT_ENTER_PAGE_INDEX(session);      \
    (e);                               \
    WT_LEAVE_PAGE_INDEX(session)

/*
 * WT_VERIFY_INFO -- A structure to hold all the information related to a verify operation.
 */ //__wt_verify_dsk_image
struct __wt_verify_info {
    WT_SESSION_IMPL *session;

    const char *tag;           /* Identifier included in error messages */
    const WT_PAGE_HEADER *dsk; /* The disk header for the page being verified */
    WT_ADDR *page_addr;        /* An item representing a page entry being verified */
    size_t page_size;
    uint32_t cell_num; /* The current cell offset being verified */
    uint64_t recno;    /* The current record number in a column store page */

/* AUTOMATIC FLAG VALUE GENERATION START 0 */
#define WT_VRFY_DISK_CONTINUE_ON_FAILURE 0x1u
#define WT_VRFY_DISK_EMPTY_PAGE_OK 0x2u
    /* AUTOMATIC FLAG VALUE GENERATION STOP 32 */
    uint32_t flags;
};
