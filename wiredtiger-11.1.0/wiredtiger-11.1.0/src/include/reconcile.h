/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

/*
 * WT_REC_KV--
 *	An on-page key/value item we're building.
 */
//参考__rec_cell_build_leaf_key  __wt_rec_cell_build_val
//该结构 = cell + 真实value
struct __wt_rec_kv {
    //真正的key内容
    WT_ITEM buf;  /* Data */
    //头部长度编码后的内容记录到这里面
    WT_CELL cell; /* Cell and cell's length */
    //记录key编码方式，返回编码后的key长度占用字节数
    size_t cell_len;
    //编码后的key占用的总字节数=长度部分+实际内容
    size_t len; /* Total length of cell + data */
};

/*
 * WT_REC_DICTIONARY --
 *  We optionally build a dictionary of values for leaf pages. Where
 * two value cells are identical, only write the value once, the second
 * and subsequent copies point to the original cell. The dictionary is
 * fixed size, but organized in a skip-list to make searches faster.
 */
struct __wt_rec_dictionary {
    uint64_t hash;   /* Hash value */
    uint32_t offset; /* Matching cell */

    u_int depth; /* Skiplist */
    WT_REC_DICTIONARY *next[0];
};

/*
 * WT_REC_CHUNK --
 *	Reconciliation split chunk.
 */
//__wt_reconcile.chunk_A chunk_B cur_ptr为该类型
//__rec_split_chunk_init中初始化
struct __wt_rec_chunk {
    /*
     * The recno and entries fields are the starting record number of the split chunk (for
     * column-store splits), and the number of entries in the split chunk.
     *
     * The key for a row-store page; no column-store key is needed because the page's recno, stored
     * in the recno field, is the column-store key.
     */
    //__wt_rec_split_finish当前reconcile处理的page上面的K和V总数
    uint32_t entries;
    //colum才用，row store不用该字段
    uint64_t recno;
    //对应的ref key，赋值参考__wt_rec_split_init, 实际上就是split的拆分点
    WT_ITEM key;
    //WT_TIME_AGGREGATE_UPDATE中统计赋值
    WT_TIME_AGGREGATE ta;

    /* Saved minimum split-size boundary information. */
    //把cur_ptr第一次接近min_space_avail可用阈值时候的成员数以及min_offset偏移量记录下来，在外层的__rec_split_finish_process_prev使用
    uint32_t min_entries;
    uint64_t min_recno;
    WT_ITEM min_key;
    WT_TIME_AGGREGATE ta_min;

    //__wt_rec_split_crossing_bnd赋值
    //也就是最解决image mem内存中，离min_space_avail最近的游标位置
    //把cur_ptr第一次接近min_space_avail可用阈值时候的成员数以及min_offset偏移量记录下来，在外层的__rec_split_finish_process_prev使用
    size_t min_offset; /* byte offset */

    //磁盘中的数据信息，参考__rec_split_write
    //block size = WT_PAGE_HEADER_SIZE + WT_BLOCK_HEADER_SIZE + 实际数据
    //corrected_page_size
    //r->first_free指向这里面的实际数据位置，也就是(WT_PAGE_HEADER_SIZE + WT_BLOCK_HEADER_SIZE + 实际数据)中的实际数据
    //默认空间大小是按照disk_img_buf_size对齐的，参考__rec_split_chunk_init
    WT_ITEM image; /* disk-image */

    /* For fixed-length column store, track where the time windows start and how many we have. */
    uint32_t aux_start_offset;
    uint32_t auxentries;
};

/*
 * WT_DELETE_HS_UPD --
 *	Update that needs to be deleted from the history store.
 */
struct __wt_delete_hs_upd {
    WT_INSERT *ins; /* Insert list reference */
    WT_ROW *rip;    /* Original on-page reference */
    WT_UPDATE *upd;
    WT_UPDATE *tombstone;
};

/*
 * Reconciliation is the process of taking an in-memory page, walking each entry
 * in the page, building a backing disk image in a temporary buffer representing
 * that information, and writing that buffer to disk.  What could be simpler?
 *
 * WT_RECONCILE --
 *	Information tracking a single page reconciliation.
 */ //WT_SESSION_IMPL.reconcile为该类型    __rec_init分片空间
struct __wt_reconcile {
    WT_REF *ref; /* Page being reconciled */
    WT_PAGE *page;
    uint32_t flags; /* Caller's configuration */

    /*
     * Track start/stop checkpoint generations to decide if history store table records are correct.
     */
    uint64_t orig_btree_checkpoint_gen;
    uint64_t orig_txn_checkpoint_gen;

    /* Track the oldest running transaction. */
    uint64_t last_running;

    /* Track the oldest running id. This one doesn't consider checkpoint. */
    uint64_t rec_start_oldest_id;

    /* Track the pinned timestamp at the time reconciliation started. */
    wt_timestamp_t rec_start_pinned_ts;

    /* Track the page's maximum transaction/timestamp. */
    uint64_t max_txn;
    wt_timestamp_t max_ts;

    /*
     * When we do not find any update to be written for the whole page, we would like to mark
     * eviction failed in the case of update-restore. There is no progress made by eviction in such
     * a case, the page size stays the same and considering it a success could force the page
     * through eviction repeatedly.
     */
    //标记是否有事务可见的V数据
    bool update_used;

    /*
     * When we can't mark the page clean after reconciliation (for example, checkpoint or eviction
     * found some uncommitted updates), there's a leave-dirty flag.
     */
    bool leave_dirty;

    /*
     * Track if reconciliation has seen any overflow items. If a leaf page with no overflow items is
     * written, the parent page's address cell is set to the leaf-no-overflow type. This means we
     * can delete the leaf page without reading it because we don't have to discard any overflow
     * items it might reference.
     *
     * The test is per-page reconciliation, that is, once we see an overflow item on the page, all
     * subsequent leaf pages written for the page will not be leaf-no-overflow type, regardless of
     * whether or not they contain overflow items. In other words, leaf-no-overflow is not
     * guaranteed to be set on every page that doesn't contain an overflow item, only that if it is
     * set, the page contains no overflow items. XXX This was originally done because raw
     * compression couldn't do better, now that raw compression has been removed, we should do
     * better.
     */
    //标识当前操作的K或者V是否超过page size大小
    bool ovfl_items;

    /*
     * Track if reconciliation of a row-store leaf page has seen empty (zero length) values. We
     * don't write out anything for empty values, so if there are empty values on a page, we have to
     * make two passes over the page when it's read to figure out how many keys it has, expensive in
     * the common case of no empty values and (entries / 2) keys. Likewise, a page with only empty
     * values is another common data set, and keys on that page will be equal to the number of
     * entries. In both cases, set a flag in the page's on-disk header.
     *
     * The test is per-page reconciliation as described above for the overflow-item test.
     */
    bool all_empty_value, any_empty_value;

    /*
     * Reconciliation gets tricky if we have to split a page, which happens when the disk image we
     * create exceeds the page type's maximum disk image size.
     *
     * First, the target size of the page we're building. In FLCS, this is the size of both the
     * primary and auxiliary portions.
     */
    //__wt_rec_split_init
    //默认//= btree->maxmempage_image, 也就是4 * WT_MAX(btree->maxintlpage, btree->maxleafpage);
    uint32_t page_size; /* Page size */

    /*
     * Second, the split size: if we're doing the page layout, split to a smaller-than-maximum page
     * size when a split is required so we don't repeatedly split a packed page.
     */
    //__wt_rec_split_init
    //默认90% * page_size  //reconcile splite的条件
    uint32_t split_size;     /* Split page size */
    //默认50% * page_size
    uint32_t min_split_size; /* Minimum split page size */

    //下面是可用空间，__wt_rec_incr中会减去已写入的KV数据长度

    //yang add change，修改位置
    //__wt_rec_split_init初始化，__wt_rec_need_split->WT_CHECK_CROSSING_BND判断是否需要split
    //split_size - WT_PAGE_HEADER_BYTE_SIZE, 除去头部字段的真实可用数据部分
    size_t space_avail;     /* Remaining space in this chunk */
    //min_split_size - WT_PAGE_HEADER_BYTE_SIZE, 除去头部字段的真实可用数据部分
    size_t min_space_avail; /* Remaining space in this chunk to put a minimum size boundary */

    /*
     * We maintain two split chunks in the memory during reconciliation to be written out as pages.
     * As we get to the end of the data, if the last one turns out to be smaller than the minimum
     * split size, we go back into the penultimate chunk and split at this minimum split size
     * boundary. This moves some data from the penultimate chunk to the last chunk, hence increasing
     * the size of the last page written without decreasing the penultimate page size beyond the
     * minimum split size. For this reason, we maintain an expected split percentage boundary and a
     * minimum split percentage boundary.
     *
     * Chunks are referenced by current and previous pointers. In case of a split, previous
     * references the first chunk and current switches to the second chunk. If reconciliation
     * generates more split chunks, the previous chunk is written to the disk and current and
     * previous swap.
     */
    WT_REC_CHUNK chunk_A, //__wt_rec_split_init
                 chunk_B, //__wt_rec_split->__rec_split_chunk_init
                 //实际上指向该page对应的真实磁盘空间，WT_REC_CHUNK.image=WT_PAGE_HEADER_SIZE + WT_BLOCK_HEADER_SIZE + 实际数据
                 //配合__wt_rec_image_copy  __wt_rec_split_init分析
                 *cur_ptr, //赋值参考__wt_rec_split_init，通过cur_ptr变量可以获取该page对应磁盘其实地址信息，通过后面的first_free成员可以获取该page对应磁盘结尾处
                 //赋值参考__wt_rec_split,指向chunk_B，这里用两个chunk的目的是，splite拆分的时候，可以分开，参考__wt_rec_split
                 *prev_ptr;

    //默认值r->page_size按照allocsize对齐的大小
    //WT_ALIGN(WT_MAX(corrected_page_size, r->split_size), btree->allocsize);
    size_t disk_img_buf_size; /* Base size needed for a chunk memory image */

    /*
     * We track current information about the current record number, the number of entries copied
     * into the disk image buffer, where we are in the buffer, how much memory remains, and the
     * current min/max of the timestamps. Those values are packaged here rather than passing
     * pointers to stack locations around the code.
     */
    uint64_t recno;         /* Current record number */
    //__wt_rec_incr计数统计，K 和 V各加1
    uint32_t entries;       /* Current number of entries */
    //r->first_free = WT_PAGE_HEADER_BYTE(btree, r->cur_ptr->image.mem);
    //__wt_rec_image_copy  __wt_rec_split_init中会拷贝数据到该空间
    //跳过PAGE_HEADER及block header，也就是指向真实data, 记录写入的KV数据的末尾处，见__wt_rec_incr
    //最终这部分buf数据会和PAGE_HEADER、block header一起写入磁盘

    //通过cur_ptr变量可以获取该page对应磁盘其实地址信息，通过后面的first_free成员可以获取该page对应磁盘结尾处
    uint8_t *first_free;    /* Current first free byte */

    //__wt_rec_split_init
    //split_size - WT_PAGE_HEADER_BYTE_SIZE, 除去头部字段的真实可用数据部分
    //size_t space_avail;     /* Remaining space in this chunk */
    //min_split_size - WT_PAGE_HEADER_BYTE_SIZE, 除去头部字段的真实可用数据部分
    //size_t min_space_avail; /* Remaining space in this chunk to put a minimum size boundary */

    /*
     * Fixed-length column store divides the disk image into two sections, primary and auxiliary,
     * and we need to track both of them.
     */
    uint32_t aux_start_offset; /* First auxiliary byte */
    uint32_t aux_entries;      /* Current number of auxiliary entries */
    uint8_t *aux_first_free;   /* Current first free auxiliary byte */
    size_t aux_space_avail;    /* Current remaining auxiliary space */

    /*
     * Counters tracking how much time information is included in reconciliation for each page that
     * is written to disk. The number of entries on a page is limited to a 32 bit number so these
     * counters can be too.
     */
    //__rec_cell_tw_stats中对下面的变量进行自增统计
    uint32_t count_durable_start_ts;
    uint32_t count_start_ts;
    uint32_t count_start_txn;
    uint32_t count_durable_stop_ts;
    uint32_t count_stop_ts;
    uint32_t count_stop_txn;
    uint32_t count_prepare;

/* AUTOMATIC FLAG VALUE GENERATION START 0 */
#define WT_REC_TIME_NEWEST_START_DURABLE_TS 0x01u
#define WT_REC_TIME_NEWEST_STOP_DURABLE_TS 0x02u
#define WT_REC_TIME_NEWEST_STOP_TS 0x04u
#define WT_REC_TIME_NEWEST_STOP_TXN 0x08u
#define WT_REC_TIME_NEWEST_TXN 0x10u
#define WT_REC_TIME_OLDEST_START_TS 0x20u
#define WT_REC_TIME_PREPARE 0x40u
    /* AUTOMATIC FLAG VALUE GENERATION STOP 16 */
    uint16_t ts_usage_flags;

    /*
     * Saved update list, supporting WT_REC_HS configurations. While reviewing updates for each
     * page, we save WT_UPDATE lists here, and then move them to per-block areas as the blocks are
     * defined.
     */
    //__rec_update_save中对下面几个值赋值，记录的是存在更新操作的KV中的V的多个版本信息
    WT_SAVE_UPD *supd; /* Saved updates */
    //数组大小
    uint32_t supd_next;
    size_t supd_allocated;
    size_t supd_memsize; /* Size of saved update structures */

    /*
     * List of updates to be deleted from the history store. While reviewing updates for each page,
     * we save the updates that needs to be deleted from history store here, and then delete them
     * after we have built the disk image.
     */
    WT_DELETE_HS_UPD *delete_hs_upd; /* Updates to delete from history store */
    uint32_t delete_hs_upd_next;
    size_t delete_hs_upd_allocated;

    /* List of pages we've written so far. */
    //注意__wt_reconcile.multi和__wt_page_modify.mod_multi_entries.multi的区别联系

    //可以参考__rec_split_dump_keys的遍历,__rec_split_write这里创建空间和赋值
    //__rec_split_write中把chunk数据写入磁盘，并保存chunk->image写入磁盘时候的元数据信息(objectid offset size  checksum)到WT_MULTI中
    WT_MULTI *multi;
    //__rec_split_write中自增, 也就是该page拆分为了多少个新page，可以参考__rec_split_dump_keys的打印
    uint32_t multi_next;
    size_t multi_allocated;

    /*
     * Root pages are written when wrapping up the reconciliation, remember the image we're going to
     * write.
     */
    WT_ITEM *wrapup_checkpoint;
    bool wrapup_checkpoint_compressed;

    /*
     * We don't need to keep the 0th key around on internal pages, the search code ignores them as
     * nothing can sort less by definition. There's some trickiness here, see the code for comments
     * on how these fields work.
     */
    bool cell_zero; /* Row-store internal page 0th key */

    /*
     * We calculate checksums to find previously written identical blocks, but once a match fails
     * during an eviction, there's no point trying again.
     */
    bool evict_matching_checksum_failed;

    WT_REC_DICTIONARY **dictionary;          /* Dictionary */
    u_int dictionary_next, dictionary_slots; /* Next, max entries */
                                             /* Skiplist head. */
    WT_REC_DICTIONARY *dictionary_head[WT_SKIP_MAXDEPTH];

    //经过__rec_cell_build_leaf_key编码后的KV存入这两个遍历
    WT_REC_KV k, v; /* Key/Value being built */

    //__rec_cell_build_leaf_key中会拷贝需要操作的K或者V内容到这个变量
    //__rec_row_leaf_insert中拷贝insert的数据到该cur中
    WT_ITEM *cur, _cur;   /* Key/Value being built */
    WT_ITEM *last, _last; /* Last key/value built */

/* Don't increase key prefix-compression unless there's a significant gain. */
#define WT_KEY_PREFIX_PREVIOUS_MINIMUM 10
    uint8_t key_pfx_last; /* Last prefix compression */

    bool key_pfx_compress;      /* If can prefix-compress next key */
    bool key_pfx_compress_conf; /* If prefix compression configured */
    bool key_sfx_compress;      /* If can suffix-compress next key */
    bool key_sfx_compress_conf; /* If suffix compression configured */

    bool is_bulk_load; /* If it's a bulk load */

    WT_SALVAGE_COOKIE *salvage; /* If it's a salvage operation */

    bool cache_write_hs;      /* Used the history store table */
    bool cache_write_restore; /* Used update/restoration */

    uint8_t tested_ref_state; /* Debugging information */

    /*
     * XXX In the case of a modified update, we may need a copy of the current value as a set of
     * bytes. We call back into the btree code using a fake cursor to do that work. This a layering
     * violation and fragile, we need a better solution.
     */
    WT_CURSOR_BTREE update_modify_cbt;

    /*
     * Variables to track reconciliation calls for pages containing cells with time window values
     * and prepared transactions.
     */
    //__rec_page_time_stats
    bool rec_page_cell_with_ts;
    bool rec_page_cell_with_txn_id;
    bool rec_page_cell_with_prepared_txn;

    /*
     * When removing a key due to a tombstone with a durable timestamp of "none", we also remove the
     * history store contents associated with that key. Keep the pertinent state here: a flag to say
     * whether this is appropriate, and a cached history store cursor for doing it.
     */
    bool hs_clear_on_tombstone;
    WT_CURSOR *hs_cursor;
};

typedef struct {
    //这个是同一个K数据对应的最新的V，参考__rec_upd_select
    WT_UPDATE *upd;       /* Update to write (or NULL) */
    WT_UPDATE *tombstone; /* The tombstone to write (or NULL) */

    //赋值见__rec_fill_tw_from_upd_select
    WT_TIME_WINDOW tw;

    bool upd_saved;       /* An element on the row's update chain was saved */
    bool no_ts_tombstone; /* Tombstone without a timestamp */
} WT_UPDATE_SELECT;

/*
 * WT_CHILD_RELEASE, WT_CHILD_RELEASE_ERR --
 *	Macros to clean up during internal-page reconciliation, releasing the hazard pointer we're
 * holding on a child page.
 */
#define WT_CHILD_RELEASE(session, hazard, ref)                          \
    do {                                                                \
        if (hazard) {                                                   \
            (hazard) = false;                                           \
            WT_TRET(__wt_page_release(session, ref, WT_READ_NO_EVICT)); \
        }                                                               \
    } while (0)
#define WT_CHILD_RELEASE_ERR(session, hazard, ref) \
    do {                                           \
        WT_CHILD_RELEASE(session, hazard, ref);    \
        WT_ERR(ret);                               \
    } while (0)

/*
 * WT_CHILD_MODIFY_STATE --
 *	We review child pages (while holding the child page's WT_REF lock), during internal-page
 * reconciliation. This structure encapsulates the child page's returned information/state.
 */
typedef struct {
    enum {
        WT_CHILD_IGNORE,   /* Ignored child */
        WT_CHILD_MODIFIED, /* Modified child */
        WT_CHILD_ORIGINAL, /* Original child */
        WT_CHILD_PROXY     /* Deleted child: proxy */
    } state;               /* Returned child state */

    WT_PAGE_DELETED del; /* WT_CHILD_PROXY state fast-truncate information */

    bool hazard; /* If currently holding a child hazard pointer */
} WT_CHILD_MODIFY_STATE;

/*
 * Macros from fixed-length entries to/from bytes.
 */
#define WT_COL_FIX_BYTES_TO_ENTRIES(btree, bytes) ((uint32_t)((((bytes)*8) / (btree)->bitcnt)))
#define WT_COL_FIX_ENTRIES_TO_BYTES(btree, entries) \
    ((uint32_t)WT_ALIGN((entries) * (btree)->bitcnt, 8))
