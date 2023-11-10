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
//�ο�__rec_cell_build_leaf_key  __wt_rec_cell_build_val
//�ýṹ = cell + ��ʵvalue
struct __wt_rec_kv {
    //������key����
    WT_ITEM buf;  /* Data */
    //ͷ�����ȱ��������ݼ�¼��������
    WT_CELL cell; /* Cell and cell's length */
    //��¼key���뷽ʽ�����ر�����key����ռ���ֽ���
    size_t cell_len;
    //������keyռ�õ����ֽ���=���Ȳ���+ʵ������
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
//__wt_reconcile.chunk_A chunk_B cur_ptrΪ������
//__rec_split_chunk_init�г�ʼ��
struct __wt_rec_chunk {
    /*
     * The recno and entries fields are the starting record number of the split chunk (for
     * column-store splits), and the number of entries in the split chunk.
     *
     * The key for a row-store page; no column-store key is needed because the page's recno, stored
     * in the recno field, is the column-store key.
     */
    //__wt_rec_split_finish��ǰreconcile�����page�����K��V����
    uint32_t entries;
    //colum���ã�row store���ø��ֶ�
    uint64_t recno;
    //��Ӧ��ref key����ֵ�ο�__wt_rec_split_init, ʵ���Ͼ���split�Ĳ�ֵ�
    WT_ITEM key;
    //WT_TIME_AGGREGATE_UPDATE��ͳ�Ƹ�ֵ
    WT_TIME_AGGREGATE ta;

    /* Saved minimum split-size boundary information. */
    //��cur_ptr��һ�νӽ�min_space_avail������ֵʱ��ĳ�Ա���Լ�min_offsetƫ������¼������������__rec_split_finish_process_prevʹ��
    uint32_t min_entries;
    uint64_t min_recno;
    WT_ITEM min_key;
    WT_TIME_AGGREGATE ta_min;

    //__wt_rec_split_crossing_bnd��ֵ
    //Ҳ��������image mem�ڴ��У���min_space_avail������α�λ��
    //��cur_ptr��һ�νӽ�min_space_avail������ֵʱ��ĳ�Ա���Լ�min_offsetƫ������¼������������__rec_split_finish_process_prevʹ��
    size_t min_offset; /* byte offset */

    //�����е�������Ϣ���ο�__rec_split_write
    //block size = WT_PAGE_HEADER_SIZE + WT_BLOCK_HEADER_SIZE + ʵ������
    //corrected_page_size
    //r->first_freeָ���������ʵ������λ�ã�Ҳ����(WT_PAGE_HEADER_SIZE + WT_BLOCK_HEADER_SIZE + ʵ������)�е�ʵ������
    //Ĭ�Ͽռ��С�ǰ���disk_img_buf_size����ģ��ο�__rec_split_chunk_init
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
 */ //WT_SESSION_IMPL.reconcileΪ������    __rec_init��Ƭ�ռ�
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
    //����Ƿ�������ɼ���V����
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
    //��ʶ��ǰ������K����V�Ƿ񳬹�page size��С
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
    //Ĭ��//= btree->maxmempage_image, Ҳ����4 * WT_MAX(btree->maxintlpage, btree->maxleafpage);
    uint32_t page_size; /* Page size */

    /*
     * Second, the split size: if we're doing the page layout, split to a smaller-than-maximum page
     * size when a split is required so we don't repeatedly split a packed page.
     */
    //__wt_rec_split_init
    //Ĭ��90% * page_size  //reconcile splite������
    uint32_t split_size;     /* Split page size */
    //Ĭ��50% * page_size
    uint32_t min_split_size; /* Minimum split page size */

    //�����ǿ��ÿռ䣬__wt_rec_incr�л��ȥ��д���KV���ݳ���

    //yang add change���޸�λ��
    //__wt_rec_split_init��ʼ����__wt_rec_need_split->WT_CHECK_CROSSING_BND�ж��Ƿ���Ҫsplit
    //split_size - WT_PAGE_HEADER_BYTE_SIZE, ��ȥͷ���ֶε���ʵ�������ݲ���
    size_t space_avail;     /* Remaining space in this chunk */
    //min_split_size - WT_PAGE_HEADER_BYTE_SIZE, ��ȥͷ���ֶε���ʵ�������ݲ���
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
                 //ʵ����ָ���page��Ӧ����ʵ���̿ռ䣬WT_REC_CHUNK.image=WT_PAGE_HEADER_SIZE + WT_BLOCK_HEADER_SIZE + ʵ������
                 //���__wt_rec_image_copy  __wt_rec_split_init����
                 *cur_ptr, //��ֵ�ο�__wt_rec_split_init��ͨ��cur_ptr�������Ի�ȡ��page��Ӧ������ʵ��ַ��Ϣ��ͨ�������first_free��Ա���Ի�ȡ��page��Ӧ���̽�β��
                 //��ֵ�ο�__wt_rec_split,ָ��chunk_B������������chunk��Ŀ���ǣ�splite��ֵ�ʱ�򣬿��Էֿ����ο�__wt_rec_split
                 *prev_ptr;

    //Ĭ��ֵr->page_size����allocsize����Ĵ�С
    //WT_ALIGN(WT_MAX(corrected_page_size, r->split_size), btree->allocsize);
    size_t disk_img_buf_size; /* Base size needed for a chunk memory image */

    /*
     * We track current information about the current record number, the number of entries copied
     * into the disk image buffer, where we are in the buffer, how much memory remains, and the
     * current min/max of the timestamps. Those values are packaged here rather than passing
     * pointers to stack locations around the code.
     */
    uint64_t recno;         /* Current record number */
    //__wt_rec_incr����ͳ�ƣ�K �� V����1
    uint32_t entries;       /* Current number of entries */
    //r->first_free = WT_PAGE_HEADER_BYTE(btree, r->cur_ptr->image.mem);
    //__wt_rec_image_copy  __wt_rec_split_init�л´�����ݵ��ÿռ�
    //����PAGE_HEADER��block header��Ҳ����ָ����ʵdata, ��¼д���KV���ݵ�ĩβ������__wt_rec_incr
    //�����ⲿ��buf���ݻ��PAGE_HEADER��block headerһ��д�����

    //ͨ��cur_ptr�������Ի�ȡ��page��Ӧ������ʵ��ַ��Ϣ��ͨ�������first_free��Ա���Ի�ȡ��page��Ӧ���̽�β��
    uint8_t *first_free;    /* Current first free byte */

    //__wt_rec_split_init
    //split_size - WT_PAGE_HEADER_BYTE_SIZE, ��ȥͷ���ֶε���ʵ�������ݲ���
    //size_t space_avail;     /* Remaining space in this chunk */
    //min_split_size - WT_PAGE_HEADER_BYTE_SIZE, ��ȥͷ���ֶε���ʵ�������ݲ���
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
    //__rec_cell_tw_stats�ж�����ı�����������ͳ��
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
    //__rec_update_save�ж����漸��ֵ��ֵ����¼���Ǵ��ڸ��²�����KV�е�V�Ķ���汾��Ϣ
    WT_SAVE_UPD *supd; /* Saved updates */
    //�����С
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
    //ע��__wt_reconcile.multi��__wt_page_modify.mod_multi_entries.multi��������ϵ

    //���Բο�__rec_split_dump_keys�ı���,__rec_split_write���ﴴ���ռ�͸�ֵ
    //__rec_split_write�а�chunk����д����̣�������chunk->imageд�����ʱ���Ԫ������Ϣ(objectid offset size  checksum)��WT_MULTI��
    WT_MULTI *multi;
    //__rec_split_write������, Ҳ���Ǹ�page���Ϊ�˶��ٸ���page�����Բο�__rec_split_dump_keys�Ĵ�ӡ
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

    //����__rec_cell_build_leaf_key������KV��������������
    WT_REC_KV k, v; /* Key/Value being built */

    //__rec_cell_build_leaf_key�л´����Ҫ������K����V���ݵ��������
    //__rec_row_leaf_insert�п���insert�����ݵ���cur��
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
    //�����ͬһ��K���ݶ�Ӧ�����µ�V���ο�__rec_upd_select
    WT_UPDATE *upd;       /* Update to write (or NULL) */
    WT_UPDATE *tombstone; /* The tombstone to write (or NULL) */

    //��ֵ��__rec_fill_tw_from_upd_select
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
