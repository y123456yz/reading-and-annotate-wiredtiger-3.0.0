/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

/*
 * __rec_key_state_update --
 *     Update prefix and suffix compression based on the last key.
 */
static inline void
__rec_key_state_update(WT_RECONCILE *r, bool ovfl_key)
{
    WT_ITEM *a;

    /*
     * If writing an overflow key onto the page, don't update the "last key" value, and leave the
     * state of prefix compression alone. (If we are currently doing prefix compression, we have a
     * key state which will continue to work, we're just skipping the key just created because it's
     * an overflow key and doesn't participate in prefix compression. If we are not currently doing
     * prefix compression, we can't start, an overflow key doesn't give us any state.)
     *
     * Additionally, if we wrote an overflow key onto the page, turn off the suffix compression of
     * row-store internal node keys. (When we split, "last key" is the largest key on the previous
     * page, and "cur key" is the first key on the next page, which is being promoted. In some cases
     * we can discard bytes from the "cur key" that are not needed to distinguish between the "last
     * key" and "cur key", compressing the size of keys on internal nodes. If we just built an
     * overflow key, we're not going to update the "last key", making suffix compression impossible
     * for the next key. Alternatively, we could remember where the last key was on the page, detect
     * it's an overflow key, read it from disk and do suffix compression, but that's too much work
     * for an unlikely event.)
     *
     * If we're not writing an overflow key on the page, update the last-key value and turn on both
     * prefix and suffix compression.
     */
    if (ovfl_key)
        r->key_sfx_compress = false;
    else {
        a = r->cur;
        r->cur = r->last;
        r->last = a;

        r->key_pfx_compress = r->key_pfx_compress_conf;
        r->key_sfx_compress = r->key_sfx_compress_conf;
    }
}

/*
 * __rec_cell_build_int_key --
 *     Process a key and return a WT_CELL structure and byte string to be stored on a row-store
 *     internal page.
 */
static int
__rec_cell_build_int_key(WT_SESSION_IMPL *session, WT_RECONCILE *r, const void *data, size_t size)
{
    WT_REC_KV *key;

    key = &r->k;

    /* Copy the bytes into the "current" and key buffers. */
    WT_RET(__wt_buf_set(session, r->cur, data, size));
    WT_RET(__wt_buf_set(session, &key->buf, data, size));

    key->cell_len = __wt_cell_pack_int_key(&key->cell, key->buf.size);
    key->len = key->cell_len + key->buf.size;

    return (0);
}

/*
 * __rec_cell_build_leaf_key --
 *     Process a key and return a WT_CELL structure and byte string to be stored on a row-store leaf
 *     page.
 */

//__wt_rec_cell_build_val: value���ݷ�װ��r->v��
//__rec_cell_build_leaf_key: ��key���б�������r->k��, ͬʱr->curָ��data����

//��key���б�������r->k�У����__wt_rec_image_copy�Ķ�
static int
__rec_cell_build_leaf_key(
  WT_SESSION_IMPL *session, WT_RECONCILE *r, const void *data, size_t size, bool *is_ovflp)
{
    WT_BTREE *btree;
    WT_REC_KV *key;
    size_t pfx_max;
    uint8_t pfx;
    const uint8_t *a, *b;

    *is_ovflp = false;

    btree = S2BT(session);
    //��key���б�������r->k��
    key = &r->k;

    pfx = 0;
    if (data == NULL)  //���û��ָ��data����ֱ�Ӹ�ֵcur���ݵ�&r->k
        /*
         * When data is NULL, our caller has a prefix compressed key they can't use (probably
         * because they just crossed a split point). Use the full key saved when last called,
         * instead.
         */
        WT_RET(__wt_buf_set(session, &key->buf, r->cur->data, r->cur->size));
    else {
        /*
         * Save a copy of the key for later reference: we use the full key for prefix-compression
         * comparisons, and if we are, for any reason, unable to use the compressed key we generate.
         */
        //����r->cur�ռ䣬������data���ݵ�r->cur��
        WT_RET(__wt_buf_set(session, r->cur, data, size));

        /*
         * Do prefix compression on the key. We know by definition the previous key sorts before the
         * current key, which means the keys must differ and we just need to compare up to the
         * shorter of the two keys.
         */
        if (r->key_pfx_compress) {
            /*
             * We can't compress out more than 256 bytes, limit the comparison to that.
             */
            pfx_max = UINT8_MAX;
            if (size < pfx_max)
                pfx_max = size;
            if (r->last->size < pfx_max)
                pfx_max = r->last->size;
            for (a = data, b = r->last->data; pfx < pfx_max; ++pfx)
                if (*a++ != *b++)
                    break;

            /*
             * Prefix compression costs CPU and memory when the page is re-loaded, skip unless
             * there's a reasonable gain. Also, if the previous key was prefix compressed, don't
             * increase the prefix compression if we aren't getting a reasonable gain. (Groups of
             * keys with the same prefix can be quickly built without needing to roll forward
             * through intermediate keys or allocating memory so they can be built faster in the
             * future, for that reason try and create big groups of keys with the same prefix.)
             */
            if (pfx < btree->prefix_compression_min)
                pfx = 0;
            else if (r->key_pfx_last != 0 && pfx > r->key_pfx_last &&
              pfx < r->key_pfx_last + WT_KEY_PREFIX_PREVIOUS_MINIMUM)
                pfx = r->key_pfx_last;

            if (pfx != 0)
                WT_STAT_DATA_INCRV(session, rec_prefix_compression, pfx);
        }

        /* Copy the non-prefix bytes into the key buffer. */
        //data������r->k buf�У�Ҳ����key->buf
        WT_RET(__wt_buf_set(session, &key->buf, (uint8_t *)data + pfx, size - pfx));
    }
    r->key_pfx_last = pfx;

    /* Create an overflow object if the data won't fit. */
    if (key->buf.size > btree->maxleafkey) {
        /*
         * Overflow objects aren't prefix compressed -- rebuild any object that was prefix
         * compressed.
         */
        if (pfx == 0) {
            WT_STAT_CONN_DATA_INCR(session, rec_overflow_key_leaf);

            *is_ovflp = true;
            return (__wt_rec_cell_build_ovfl(session, r, key, WT_CELL_KEY_OVFL, NULL, 0));
        }

        //��Ҫǰ׺ѹ���������ݹ���ã��´λ���������pfx == 0{}ѭ������
        //�ݹ����
        return (__rec_cell_build_leaf_key(session, r, NULL, 0, is_ovflp));
    }

    //1. key->buf���ȱ����¼��key->cell�У�ͬʱ���ر�����key buf���ݳ��ȱ����ռ�õ��ֽ���
    key->cell_len = __wt_cell_pack_leaf_key(&key->cell, pfx, key->buf.size);
    //������keyռ�õ����ֽ���=���Ȳ���+ʵ������
    key->len = key->cell_len + key->buf.size;

    return (0);
}

/*
 * __wt_bulk_insert_row --
 *     Row-store bulk insert.
 */
int
__wt_bulk_insert_row(WT_SESSION_IMPL *session, WT_CURSOR_BULK *cbulk)
{
    WT_BTREE *btree;
    WT_CURSOR *cursor;
    WT_RECONCILE *r;
    WT_REC_KV *key, *val;
    WT_TIME_WINDOW tw;
    bool ovfl_key;

    //printf("yang test .....................__wt_bulk_insert_row.........................\r\n");
    r = cbulk->reconcile;
    btree = S2BT(session);
    cursor = &cbulk->cbt.iface;
    WT_TIME_WINDOW_INIT(&tw);

    key = &r->k;
    val = &r->v;
    WT_RET(__rec_cell_build_leaf_key(session, r, /* Build key cell */
      cursor->key.data, cursor->key.size, &ovfl_key));
    if (cursor->value.size == 0)
        val->len = 0;
    else
        WT_RET(__wt_rec_cell_build_val(session, r, cursor->value.data, /* Build value cell */
          cursor->value.size, &tw, 0));

    /* Boundary: split or write the page. */
    if (WT_CROSSING_SPLIT_BND(r, key->len + val->len)) {
        /*
         * Turn off prefix compression until a full key written to the new page, and (unless already
         * working with an overflow key), rebuild the key without compression.
         */
        if (r->key_pfx_compress_conf) {
            r->key_pfx_compress = false;
            r->key_pfx_last = 0;
            if (!ovfl_key)
                WT_RET(__rec_cell_build_leaf_key(session, r, NULL, 0, &ovfl_key));
        }

        WT_RET(__wt_rec_split_crossing_bnd(session, r, key->len + val->len));
    }

    /* Copy the key/value pair onto the page. */
    __wt_rec_image_copy(session, r, key);
    if (val->len == 0)
        r->any_empty_value = true;
    else {
        r->all_empty_value = false;
        if (btree->dictionary)
            WT_RET(__wt_rec_dict_replace(session, r, &tw, 0, val));
        __wt_rec_image_copy(session, r, val);
    }
    WT_TIME_AGGREGATE_UPDATE(session, &r->cur_ptr->ta, &tw);

    /* Update compression state. */
    __rec_key_state_update(r, ovfl_key);

    return (0);
}

/*
 * __rec_row_merge --
 *     Merge in a split page.

 //checkpoint����:
 //    __checkpoint_tree->__wt_sync_file��ע���������ֻ��Ѳ�ֺ��Ԫ���ݼ�¼��multi_next��multi[multi_next]�У����ǲ���ͨ��__evict_page_dirty_update�͸�page����
 //    ����ͨ��__wt_rec_row_int->__rec_row_merge�л�ȡmulti[multi_next]���г־û�
 //evict reconcile����:
 //    __evict_reconcile: �����һ��page����split_size���Ϊ���chunkд�����, ��ز�ֺ��Ԫ���ݼ�¼��multi_next��multi[multi_next]��
 //    __evict_page_dirty_update: ��__evict_reconcile��ֺ��multi[multi_next]��Ӧ����multi_next��page�����º͸�page����
 */
//__wt_rec_row_int->__rec_row_merge
//��internal page��Ӧ��page��ref key�͸�internal page��Ӧ��page�Ѿ��־û��������ϵ�extԪ������Ϣ��Ϣ�ϲ���һ��chunk image��
static int
__rec_row_merge(WT_SESSION_IMPL *session, WT_RECONCILE *r, WT_PAGE *page)
{
    WT_ADDR *addr;
    WT_MULTI *multi;
    WT_PAGE_MODIFY *mod;
    WT_REC_KV *key, *val;
    uint32_t i;

    mod = page->modify;

    key = &r->k;
    val = &r->v;

  //  printf("yang test .....................__rec_row_merge........mod->mod_multi_entries:%d\n", (int)mod->mod_multi_entries);
    /* For each entry in the split array... */
    for (multi = mod->mod_multi, i = 0; i < mod->mod_multi_entries; ++multi, ++i) {
        /* Build the key and value cells. */
        //��ֵķֽ��key�洢��r->key�У�Ҳ����internal key
        WT_RET(__rec_cell_build_int_key(
          session, r, WT_IKEY_DATA(multi->key.ikey), r->cell_zero ? 1 : multi->key.ikey->size));
        r->cell_zero = false;

        //Ҳ���ǲ�ֺ󲢳־û������̵�extԪ������Ϣ�����洢��r->v��
        addr = &multi->addr;
        __wt_rec_cell_build_addr(session, r, addr, NULL, WT_RECNO_OOB, NULL);

        /* Boundary: split or write the page. */
        if (__wt_rec_need_split(r, key->len + val->len))
            WT_RET(__wt_rec_split_crossing_bnd(session, r, key->len + val->len));

        /* Copy the key and value onto the page. */
        //����r->k��r->v���ݵ�r->first_free��Ӧ�ռ�
        __wt_rec_image_copy(session, r, key);
        __wt_rec_image_copy(session, r, val);
        WT_TIME_AGGREGATE_MERGE(session, &r->cur_ptr->ta, &addr->ta);

        /* Update compression state. */
        __rec_key_state_update(r, false);
    }
    return (0);
}

/*
 * __wt_rec_row_int --
 *     Reconcile a row-store internal page.

//internal page�־û���ext����: __reconcile->__wt_rec_row_int->__wt_rec_split_finish->__rec_split_write->__rec_write
//    ->__wt_blkcache_write->__bm_checkpoint->__bm_checkpoint

//leaf page�־û���ext����: __reconcile->__wt_rec_row_leaf->__wt_rec_split_finish->__rec_split_write->__rec_write
//    ->__wt_blkcache_write->__bm_write->__wt_block_write

//__wt_block_checkpoint->__ckpt_process����checkpoint���Ԫ���ݳ־û�
//__wt_meta_checkpoint��ȡcheckpoint��Ϣ��Ȼ��__wt_block_checkpoint_load����checkpoint���Ԫ����
//__btree_preload->__wt_blkcache_readѭ���������������ݼ���

  //checkpoint����:
 //    __checkpoint_tree->__wt_sync_file��ע���������ֻ��Ѳ�ֺ��Ԫ���ݼ�¼��multi_next��multi[multi_next]�У����ǲ���ͨ��__evict_page_dirty_update�͸�page����
 //    ����ͨ��__wt_rec_row_int->__rec_row_merge�л�ȡmulti[multi_next]���г־û�
 //evict reconcile����:
 //    __evict_reconcile: �����һ��page����split_size���Ϊ���chunkд�����, ��ز�ֺ��Ԫ���ݼ�¼��multi_next��multi[multi_next]��
 //    __evict_page_dirty_update: ��__evict_reconcile��ֺ��multi[multi_next]��Ӧ����multi_next��page�����º͸�page����
 */

//��Ӧ��root����internal page�������̼�__wt_meta_checkpoint  __btree_preload

//__reconcile->__wt_rec_row_int
//__wt_rec_row_int�Ѹ�internal page��ref key��������������page�Ĵ���Ԫ������Ϣд�뵽һ���µ�ext�־û�(__rec_write->__wt_blkcache_write->__bm_checkpoint->__bm_checkpoint)

//���Բο���__wt_sync_file�л�������е�btree���̣���__wt_sync_file����α���btree�ģ�Ȼ���ߵ�����
/*
                        root page                           (root page ext�־û�__wt_rec_row_int)
                        /         \
                      /             \
                    /                 \
       internal-1 page             internal-2 page          (internal page ext�־û�__wt_rec_row_int)
         /      \                      /    \
        /        \                    /       \
       /          \                  /          \
leaf-1 page    leaf-2 page    leaf3 page      leaf4 page    (leaf page ext�־û�__wt_rec_row_leaf)

������һ�����ı���˳��: leaf1->leaf2->internal1->leaf3->leaf4->internal2->root

//�������ͼ���Կ�����internal page(root+internal1+internal2)�ܹ������ߵ�����, internal1��¼leaf1��leaf2��pageԪ����[ref key, leaf page ext addrԪ����]
//  internal2��¼leaf3��leaf4��pageԪ����[ref key, leaf page ext addrԪ����],
//  root��¼internal1��internal2��Ԫ����[ref key, leaf page ext addrԪ����],
*/

int
__wt_rec_row_int(WT_SESSION_IMPL *session, WT_RECONCILE *r, WT_PAGE *page)
{
    WT_ADDR *addr;
    WT_BTREE *btree;
    WT_CELL *cell;
    WT_CELL_UNPACK_ADDR *kpack, _kpack, *vpack, _vpack;
    WT_CHILD_MODIFY_STATE cms;
    WT_DECL_RET;
    WT_IKEY *ikey;
    WT_PAGE *child;
    WT_PAGE_DELETED *page_del;
    WT_REC_KV *key, *val;
    WT_REF *ref;
    WT_TIME_AGGREGATE ft_ta, *source_ta, ta;
    size_t size;
    const void *p;

    ref = page->pg_intl_parent_ref;
    if(__wt_ref_is_root(ref))
        WT_RET(__wt_msg(session, "yang test ..............................page is root, page:%p, ref:%p, type:%s",
            page, ref, __wt_page_type_string(page->type)));
    else
        WT_RET(__wt_msg(session, "yang test ..............................page is not root, page:%p, ref:%p, type:%s",
            page, ref, __wt_page_type_string(page->type)));
    btree = S2BT(session);
    child = NULL;
    WT_TIME_AGGREGATE_INIT_MERGE(&ft_ta);

    //printf("yang test ...............__wt_rec_row_int...........Reconcile a row-store internal page.........\r\n");
    key = &r->k;
    kpack = &_kpack;
    WT_CLEAR(*kpack); /* -Wuninitialized */
    val = &r->v;
    vpack = &_vpack;
    WT_CLEAR(*vpack); /* -Wuninitialized */

    ikey = NULL; /* -Wuninitialized */
    cell = NULL;

    WT_RET(__wt_rec_split_init(session, r, page, 0, btree->maxintlpage_precomp, 0));

    /*
     * Ideally, we'd never store the 0th key on row-store internal pages because it's never used
     * during tree search and there's no reason to waste the space. The problem is how we do splits:
     * when we split, we've potentially picked out several "split points" in the buffer which is
     * overflowing the maximum page size, and when the overflow happens, we go back and physically
     * split the buffer, at those split points, into new pages. It would be both difficult and
     * expensive to re-process the 0th key at each split point to be an empty key, so we don't do
     * that. However, we are reconciling an internal page for whatever reason, and the 0th key is
     * known to be useless. We truncate the key to a single byte, instead of removing it entirely,
     * it simplifies various things in other parts of the code (we don't have to special case
     * transforming the page from its disk image to its in-memory version, for example).
     */
    r->cell_zero = true;
    // printf("yang test ......__wt_rec_row_int....1.......page->dsk:%p\r\n", page->dsk);

    do {
        WT_PAGE_INDEX *__pindex;

        WT_INTL_INDEX_GET(session, page, __pindex);
        (void)__pindex->entries;
         //printf("yang test ......__wt_rec_row_int....1.......__pindex size:%u\r\n", __pindex->entries);
    } while(0);

    /* For each entry in the in-memory page... */
    //������ȡinternal page��������������ref
    WT_INTL_FOREACH_BEGIN (session, page, ref) {
        /*
         * There are different paths if the key is an overflow item vs. a straight-forward on-page
         * value. If an overflow item, we would have instantiated it, and we can use that fact to
         * set things up.
         *
         * Note the cell reference and unpacked key cell are available only in the case of an
         * instantiated, off-page key, we don't bother setting them if that's not possible.
         */
        cell = NULL;
        //��ȡref key
        ikey = __wt_ref_key_instantiated(ref);
        //����ǴӴ��̼��صģ���Ҫ����������洢��kpack��
        if (ikey != NULL && ikey->cell_offset != 0) {
            cell = WT_PAGE_REF_OFFSET(page, ikey->cell_offset);
            __wt_cell_unpack_addr(session, page->dsk, cell, kpack);

            /*
             * Historically, we stored overflow cookies on internal pages, discard any underlying
             * blocks. We have a copy to build the key (the key was instantiated when we read the
             * page into memory), they won't be needed in the future as we're rewriting the page.
             */
            if (F_ISSET(kpack, WT_CELL_UNPACK_OVERFLOW) && kpack->raw != WT_CELL_KEY_OVFL_RM)
                WT_ERR(__wt_ovfl_discard_add(session, page, kpack->cell));
        }

        WT_ERR(__wt_rec_child_modify(session, r, ref, &cms));
        addr = ref->addr;
        child = ref->page;

        //printf("yang test ..................__wt_rec_row_int.................cms.state:%u\r\n", cms.state);
        switch (cms.state) {
        case WT_CHILD_IGNORE:
            /*
             * Ignored child.
             */
            WT_CHILD_RELEASE_ERR(session, cms.hazard, ref);
            continue;

        case WT_CHILD_MODIFIED:
            /*
             * Modified child. Empty pages are merged into the parent and discarded.
             */
            switch (child->modify->rec_result) {
            case WT_PM_REC_EMPTY:
                WT_CHILD_RELEASE_ERR(session, cms.hazard, ref);
                continue;
             //checkpoint����:
             //    __checkpoint_tree->__wt_sync_file��ע���������ֻ��Ѳ�ֺ��Ԫ���ݼ�¼��multi_next��multi[multi_next]�У����ǲ���ͨ��__evict_page_dirty_update�͸�page����
             //    ����ͨ��__wt_rec_row_int->__rec_row_merge�л�ȡmulti[multi_next]���г־û�
            case WT_PM_REC_MULTIBLOCK:
                //��internal page��Ӧ��page��ref key�͸�internal page��Ӧ��page�Ѿ��־û��������ϵ�extԪ������Ϣ�ϲ���һ��chunk image��
                //Ȼ��Ѹ�chunk����д��־û���ext
                WT_ERR(__rec_row_merge(session, r, child));
                WT_CHILD_RELEASE_ERR(session, cms.hazard, ref);
                continue;
            case WT_PM_REC_REPLACE:
                /*
                 * If the page is replaced, the page's modify structure has the page's address.
                 */
                addr = &child->modify->mod_replace;
                break;
            default:
                WT_ERR(__wt_illegal_value(session, child->modify->rec_result));
            }
            break;
        case WT_CHILD_ORIGINAL:
            /* Original child. */
            break;
        case WT_CHILD_PROXY:
            /* Fast-delete child where we write a proxy cell. */
            break;
        }

        //printf("yang test .........__wt_rec_row_int.........................\r\n");
        /*
         * Build the value cell, the child page's address. Addr points to an on-page cell or an
         * off-page WT_ADDR structure.
         */
        page_del = NULL;
        if (__wt_off_page(page, addr)) {
            page_del = cms.state == WT_CHILD_PROXY ? &cms.del : NULL;
            //�Ѹ�page��Ӧ�־û��Ĵ���addr WT_ADDR��Ϣ����洢��r->v��
            __wt_rec_cell_build_addr(session, r, addr, NULL, WT_RECNO_OOB, page_del);
            source_ta = &addr->ta;
        } else if (cms.state == WT_CHILD_PROXY) {
            /* Proxy cells require additional information in the address cell. */
            __wt_cell_unpack_addr(session, page->dsk, ref->addr, vpack);
            page_del = &cms.del;
            __wt_rec_cell_build_addr(session, r, NULL, vpack, WT_RECNO_OOB, page_del);
            source_ta = &vpack->ta;
        } else {
            /*
             * The transaction ids are cleared after restart. Repack the cell with new validity
             * information to flush cleared transaction ids.
             */
            WT_ASSERT_ALWAYS(session, cms.state == WT_CHILD_ORIGINAL,
              "Not propagating the original fast-truncate information");
            __wt_cell_unpack_addr(session, page->dsk, ref->addr, vpack);

            /* The proxy cells of fast truncate pages must be handled in the above flows. */
            WT_ASSERT_ALWAYS(session, vpack->type != WT_CELL_ADDR_DEL,
              "Proxy cell is selected with original child image");

            if (F_ISSET(vpack, WT_CELL_UNPACK_TIME_WINDOW_CLEARED)) {
                __wt_rec_cell_build_addr(session, r, NULL, vpack, WT_RECNO_OOB, page_del);
            } else {
                val->buf.data = ref->addr;
                val->buf.size = __wt_cell_total_len(vpack);
                val->cell_len = 0;
                val->len = val->buf.size;
            }
            source_ta = &vpack->ta;
        }

        /*
         * Track the time window. The fast-truncate is a stop time window and has to be considered
         * in the internal page's aggregate information for RTS to find it.
         */
        WT_TIME_AGGREGATE_COPY(&ta, source_ta);
        if (page_del != NULL)
            WT_TIME_AGGREGATE_UPDATE_PAGE_DEL(session, &ft_ta, page_del);
        WT_CHILD_RELEASE_ERR(session, cms.hazard, ref);

        /* Build key cell. Truncate any 0th key, internal pages don't need 0th keys. */
        //��ȡһ��page������ref��keyֵ�ͳ���, ��¼���Ƕ�Ӧpage����Сkey
        __wt_ref_key(page, ref, &p, &size);
        {
            //char * test;
            //WT_ERR(__wt_strndup(session, p, size, &test));
           // printf("yang test .......__wt_rec_row_int..........sizze:%d test:%s ss:%d---%d--\r\n",
            //    (int)size, test, (int)strlen(""), (int)sizeof(""));
        }
        //ȷ��ÿһ��internal page������ߵ�page��Ӧ��ref key="",����д��Ϊ1��1����Ϊ�и�'\0'
        //����root��ref keyҲ��������򣬲ο�__btree_tree_open_empty
        if (r->cell_zero)
            size = 1;
        //�Ѹ�page��Ӧref key�����r->k��
        WT_ERR(__rec_cell_build_int_key(session, r, p, size));
        r->cell_zero = false;

        /* Boundary: split or write the page. */
        //internal pageͨ������������ref key���ж��Ƿ��ڴ泬����Ҫsplit������ͨ��evictд�����
        if (__wt_rec_need_split(r, key->len + val->len))
            //���ճ־û���__ckpt_process
            WT_ERR(__wt_rec_split_crossing_bnd(session, r, key->len + val->len));

        /* Copy the key and value onto the page. */
        //����r->k��r->v���ݵ�r->first_free��Ӧ�ռ�
        __wt_rec_image_copy(session, r, key);
        __wt_rec_image_copy(session, r, val);
        if (page_del != NULL)
            WT_TIME_AGGREGATE_MERGE(session, &r->cur_ptr->ta, &ft_ta);
        WT_TIME_AGGREGATE_MERGE(session, &r->cur_ptr->ta, &ta);

        /* Update compression state. */
        __rec_key_state_update(r, false);
    }
    WT_INTL_FOREACH_END;

    //û�е�__wt_rec_need_split��ֵ��ref keyͨ������־û�������, ���ճ־û���__ckpt_process
    /* Write the remnant page. */
    return (__wt_rec_split_finish(session, r));

err:
    WT_CHILD_RELEASE(session, cms.hazard, ref);
    return (ret);
}

/*
 * __rec_row_zero_len --
 *     Return if a zero-length item can be written.
 */
static bool
__rec_row_zero_len(WT_SESSION_IMPL *session, WT_TIME_WINDOW *tw)
{
    /*
     * The item must be globally visible because we're not writing anything on the page. Don't be
     * tempted to check the time window against the default here - the check is subtly different due
     * to the grouping.
     */
    return (!WT_TIME_WINDOW_HAS_STOP(tw) &&
      ((tw->start_ts == WT_TS_NONE && tw->start_txn == WT_TXN_NONE) ||
        __wt_txn_tw_start_visible_all(session, tw)));
}

/*
 * __rec_row_leaf_insert --
 *     Walk an insert chain, writing K/V pairs.
 */
//__wt_rec_row_leaf->__rec_row_leaf_insert->__wt_rec_image_copy: ����page�ڴ沿��KV���ݵ�r->first_free��Ӧ�ڴ�ռ�, ��������Ŀռ����ݳ���һ����ֵ����ֱ��д�����
//__wt_rec_row_leaf->__wt_rec_image_copy: �������̲���KV���ݵ�r->first_free��Ӧ�ڴ�ռ�, ��������Ŀռ����ݳ���һ����ֵ����ֱ��д�����
static int
__rec_row_leaf_insert(WT_SESSION_IMPL *session, WT_RECONCILE *r, WT_INSERT *ins)
{
    WT_BTREE *btree;
    WT_CURSOR_BTREE *cbt;
    WT_REC_KV *key, *val;
    WT_TIME_WINDOW tw;
    WT_UPDATE *upd;
    WT_UPDATE_SELECT upd_select;
    bool ovfl_key;

    btree = S2BT(session);

    cbt = &r->update_modify_cbt;
    cbt->iface.session = (WT_SESSION *)session;

    key = &r->k;
    val = &r->v;

    upd = NULL;

    for (; ins != NULL; ins = WT_SKIP_NEXT(ins)) {//������Ծ��
         //��ȡ��ins��Ӧ��update���������µ�V
         //��ȡ��ins��Ӧ��update���������µ�V�洢��upd_select�� ���������ʷ�汾�洢��r->supd
        WT_RET(__wt_rec_upd_select(session, r, ins, NULL, NULL, &upd_select));
        if ((upd = upd_select.upd) == NULL) {//upd��¼���Ǹ�key��Ӧ��value�޸��б�һ�㲻�����
            /*
             * In cases where a page has grown so large we are trying to force evict it (there is
             * content, but none of the content can be evicted), we set up fake split points, to
             * allow the page to use update restore eviction and be split into multiple reasonably
             * sized pages. Check if we are in this situation. The call to split with zero
             * additional size is odd, but split takes into account saved updates in a special way
             * for this case already.
             */
           // printf("yang test .......__rec_row_leaf_insert.......update no no date.................\r\n");
            if (!upd_select.upd_saved || !__wt_rec_need_split(r, 0))
                continue;

            //�������ݵ�r->cur��
            WT_RET(__wt_buf_set(session, r->cur, WT_INSERT_KEY(ins), WT_INSERT_KEY_SIZE(ins)));
            WT_RET(__wt_rec_split_crossing_bnd(session, r, 0));

            /*
             * Turn off prefix and suffix compression until a full key is written into the new page.
             */
            r->key_pfx_compress = r->key_sfx_compress = false;
            r->key_pfx_last = 0;
            continue;
        }

        /*
         * If we've selected an update, it should be flagged as being destined for the data store.
         *
         * If not, it's either because we're not doing a history store reconciliation or because the
         * update is globally visible (in which case, subsequent updates become irrelevant for
         * reconciliation).
         */
        WT_ASSERT(session,
          F_ISSET(upd, WT_UPDATE_DS) || !F_ISSET(r, WT_REC_HS) ||
            __wt_txn_tw_start_visible_all(session, &upd_select.tw));

        WT_TIME_WINDOW_COPY(&tw, &upd_select.tw);

        switch (upd->type) {
        case WT_UPDATE_MODIFY:
            /*
             * Impossible slot, there's no backing on-page item.
             */
            cbt->slot = UINT32_MAX;
            WT_RET(__wt_modify_reconstruct_from_upd_list(session, cbt, upd, cbt->upd_value));
            __wt_value_return(cbt, cbt->upd_value);
            WT_RET(__wt_rec_cell_build_val(
              session, r, cbt->iface.value.data, cbt->iface.value.size, &tw, 0));
            break;
        case WT_UPDATE_STANDARD:
            if (upd->size == 0 && WT_TIME_WINDOW_IS_EMPTY(&tw))
                val->len = 0;
            else
                /* Take the value from the update. */
                //value���ݷ�װ��r->v��
                WT_RET(__wt_rec_cell_build_val(session, r, upd->data, upd->size, &tw, 0));
            break;
        case WT_UPDATE_TOMBSTONE:
            continue;
        default:
            WT_RET(__wt_illegal_value(session, upd->type));
        }
        /* Build key cell. */
        //��key���б�������r->k��
        WT_RET(__rec_cell_build_leaf_key(
          session, r, WT_INSERT_KEY(ins), WT_INSERT_KEY_SIZE(ins), &ovfl_key));

        /* Boundary: split or write the page. */
        //������KV�ӽ����ͻᳬ��һ��WT_RECONCILE�Ŀ��ô��̿ռ䣬���̿ռ䲻���ã�����Ҫ�����KV��ǰ��һ���Ѿ�������r->first_free�ռ��������д�����
        if (__wt_rec_need_split(r, key->len + val->len)) {
          //����һ���������Σ���һ���ǿ��ÿռ�ӽ�min_space_avail���ڶ����ǿ��ÿռ�ֱ�ӽӽ�space_avail
            /*
             * Turn off prefix compression until a full key written to the new page, and (unless
             * already working with an overflow key), rebuild the key without compression.
             */
            if (r->key_pfx_compress_conf) {
                r->key_pfx_compress = false;
                r->key_pfx_last = 0;
                if (!ovfl_key)
                    WT_RET(__rec_cell_build_leaf_key(session, r, NULL, 0, &ovfl_key));
            }
           // printf("yang test .........__rec_row_leaf_insert.....__wt_rec_need_split.......__wt_rec_split_crossing_bnd.........................\r\n");
            //Ҳ���ǰѵ�ǰ���ڲ�����page���Ѿ��������������ֵ�����ݷ���prev_ptr,prev_ptr����д�������, ��ǰcur_ptrָ���µĸɾ�image�ռ�

            //key->len + val->len��ʾ��KV��ǰ��û����ӵ�r->first_free�ռ���
            WT_RET(__wt_rec_split_crossing_bnd(session, r, key->len + val->len));
        }

        /* Copy the key/value pair onto the page. */
        //����key���ݵ�r->first_free��Ӧ�ռ�
        __wt_rec_image_copy(session, r, key);
        if (val->len == 0 && __rec_row_zero_len(session, &tw))//˵��valueΪ��
            r->any_empty_value = true;
        else {
            r->all_empty_value = false;
            if (btree->dictionary)//row�洢����
                WT_RET(__wt_rec_dict_replace(session, r, &tw, 0, val));
            //����val���ݵ�r->first_free��Ӧ�ռ�
            __wt_rec_image_copy(session, r, val);
        }
        //tw��Ϣ��ֵ��r������reconcileд�����
        WT_TIME_AGGREGATE_UPDATE(session, &r->cur_ptr->ta, &tw);

        /* Update compression state. */
        __rec_key_state_update(r, ovfl_key);
    }

    return (0);
}

/*
 * __rec_cell_repack --
 *     Repack a cell.
 */
static inline int
__rec_cell_repack(WT_SESSION_IMPL *session, WT_BTREE *btree, WT_RECONCILE *r,
  WT_CELL_UNPACK_KV *vpack, WT_TIME_WINDOW *tw)
{
    WT_DECL_ITEM(tmpval);
    WT_DECL_RET;
    size_t size;
    const void *p;

    WT_ERR(__wt_scr_alloc(session, 0, &tmpval));

    /* If the item is Huffman encoded, decode it. */
    if (btree->huffman_value == NULL) {
        p = vpack->data;
        size = vpack->size;
    } else {
        WT_ERR(
          __wt_huffman_decode(session, btree->huffman_value, vpack->data, vpack->size, tmpval));
        p = tmpval->data;
        size = tmpval->size;
    }
    WT_ERR(__wt_rec_cell_build_val(session, r, p, size, tw, 0));

err:
    __wt_scr_free(session, &tmpval);
    return (ret);
}

/*
 * __wt_rec_row_leaf --
 *     Reconcile a row-store leaf page.
 */

//__ckpt_process����checkpoint���Ԫ���ݳ־û�
//__wt_meta_checkpoint��ȡcheckpoint��Ϣ��Ȼ��__wt_block_checkpoint_load����checkpoint���Ԫ����
//__btree_preload->__wt_blkcache_readѭ���������������ݼ���

//__wt_btree_open->__wt_btree_tree_open->__wt_blkcache_read: ����root addr��ȡ���������echeckpoint avail����alloc��Ծ���е�extԪ���ݵ��ڴ���
//__wt_btree_open->__btree_preload->__wt_blkcache_read: ����ext��Ԫ���ݵ�ַaddr��Ϣ�Ӵ��̶�ȡ��ʵext��buf�ڴ���

//���Բο���__wt_sync_file�л�������е�btree���̣���__wt_sync_file����α���btree�ģ�Ȼ���ߵ�����
/*
                        root page                           (root page ext�־û�__wt_rec_row_int)
                        /         \
                      /             \
                    /                 \
       internal-1 page             internal-2 page          (internal page ext�־û�__wt_rec_row_int)
         /      \                      /    \
        /        \                    /       \
       /          \                  /          \
leaf-1 page    leaf-2 page    leaf3 page      leaf4 page    (leaf page ext�־û�__wt_rec_row_leaf)

������һ�����ı���˳��: leaf1->leaf2->internal1->leaf3->leaf4->internal2->root

//�������ͼ���Կ�����internal page(root+internal1+internal2)�ܹ������ߵ�����, internal1��¼leaf1��leaf2��pageԪ����[ref key, leaf page ext addrԪ����]
//  internal2��¼leaf3��leaf4��pageԪ����[ref key, leaf page ext addrԪ����],
//  root��¼internal1��internal2��Ԫ����[ref key, leaf page ext addrԪ����],
*/

int
__wt_rec_row_leaf( 
  WT_SESSION_IMPL *session, WT_RECONCILE *r, WT_REF *pageref, WT_SALVAGE_COOKIE *salvage)
{
    static WT_UPDATE upd_tombstone = {.txnid = WT_TXN_NONE, .type = WT_UPDATE_TOMBSTONE};
    WT_BTREE *btree;
    WT_CELL *cell;
    WT_CELL_UNPACK_KV *kpack, _kpack, *vpack, _vpack;
    WT_CURSOR_BTREE *cbt;
    WT_DECL_ITEM(lastkey);
    WT_DECL_ITEM(tmpkey);
    WT_DECL_RET;
    WT_IKEY *ikey;
    WT_INSERT *ins;
    WT_PAGE *page;
    WT_REC_KV *key, *val;
    WT_ROW *rip;
    WT_TIME_WINDOW *twp;
    WT_UPDATE *upd;
    WT_UPDATE_SELECT upd_select;
    size_t key_size;
    uint64_t slvg_skip;
    uint32_t i;
    uint8_t key_prefix;
    bool dictionary, key_onpage_ovfl, ovfl_key;
    void *copy;
    const void *key_data;

    btree = S2BT(session);
    page = pageref->page;
    twp = NULL;
    upd = NULL;
    slvg_skip = salvage == NULL ? 0 : salvage->skip;

    key = &r->k;
    val = &r->v;
    vpack = &_vpack;

    cbt = &r->update_modify_cbt;
    cbt->iface.session = (WT_SESSION *)session;

    WT_RET(__wt_rec_split_init(session, r, page, 0, btree->maxleafpage_precomp, 0));
   // printf("yang test __wt_rec_row_leaf......btree->maxmempage:%lu, page->memory_footprintmax:%lu, leafpage_precomp:%lu,  split_size:%u, min_split_size:%u\r\n", 
    //    btree->maxmempage, page->memory_footprint, btree->maxleafpage_precomp, r->split_size, r->min_split_size);

   //����1:
   // ��pageû�������ڴ�������
   //   cbt->slot��ֱ����WT_ROW_INSERT_SLOT[0]�������
   
   //����2:
   // ���������������WT_ROW_INSERT_SMALLEST(mod_row_insert[(page)->entries])��ʾд�����page����KС�ڸ�page��
   //   ����������С��K����������ͨ���������洢����
   
   //����3:
   // ��page�������ڴ������棬�����page�ڴ�����������������ke1,key2...keyn���²���keyx>page����keyn�����ڴ��ά��һ��
   //   cbt->slotΪ����KV����-1���������ڸ�page������KV������ӵ�WT_ROW_INSERT_SLOT[page->entries - 1]�����������
   
   //����4:
   // ��page�������ڴ������棬�����page�ڴ�����������������ke1,key2,key3...keyn���²���keyx����key2С��key3, key2<keyx>key3�����ڴ��ά��һ��
   //   cbt->slotΪ����key2�ڴ����е�λ��(Ҳ����1����0��ʼ��)���������ڸ�page������KV������ӵ�WT_ROW_INSERT_SLOT[1]�������
   
   //һ��page�ڴ���page->pg_row�ж�������(page->pg_row[]�����С)���ͻ�ά�����ٸ�������ΪҪ��֤��д���ڴ�����ݺʹ��̵����ݱ���˳��

    /*
     * Write any K/V pairs inserted into the page before the first from-disk key on the page.
     */
    //�����KV������ӵ�r->first_free��Ӧ�ڴ�ռ�
    //ins��ӦWT_ROW_INSERT_SMALLEST(page)�����Ծ���0�㣬�����ò㼴�ɻ�ȡ����Ծ����������
    if ((ins = WT_SKIP_FIRST(WT_ROW_INSERT_SMALLEST(page))) != NULL) {
       // printf("yang test .......11111111111111111111111..............__wt_rec_row_leaf.......................\r\n");
        //__rec_row_leaf_insert: �����KV������ӵ�r->first_free��Ӧ�ڴ�ռ�
        WT_RET(__rec_row_leaf_insert(session, r, ins));
    }
    /*
     * When we walk the page, we store each key we're building for the disk image in the last-key
     * buffer. There's trickiness because it's significantly faster to use a previously built key
     * plus the next key's prefix count to build the next key (rather than to call some underlying
     * function to do it from scratch). In other words, we put each key into the last-key buffer,
     * then use it to create the next key, again storing the result into the last-key buffer. If we
     * don't build a key for any reason (imagine we skip a key because the value was deleted), clear
     * the last-key buffer size so it's not used to fast-path building the next key.
     */
    WT_ERR(__wt_scr_alloc(session, 0, &lastkey));

    /* Temporary buffer in which to instantiate any uninstantiated keys or value items we need. */
    WT_ERR(__wt_scr_alloc(session, 0, &tmpkey));

    /* For each entry in the page... */
    //������page����KV��Ȼ������KV������ӵ�r->first_free��Ӧ�ڴ�ռ�
    WT_ROW_FOREACH (page, rip, i) {
        /*
         * The salvage code, on some rare occasions, wants to reconcile a page but skip some leading
         * records on the page. Because the row-store leaf reconciliation function copies keys from
         * the original disk page, this is non-trivial -- just changing the in-memory pointers isn't
         * sufficient, we have to change the WT_CELL structures on the disk page, too. It's ugly,
         * but we pass in a value that tells us how many records to skip in this case.
         */
        if (slvg_skip != 0) {
            --slvg_skip;
            continue;
        }
        dictionary = false;

        /*
         * Figure out if the key is an overflow key, and in that case unpack the cell, we'll need it
         * later.
         */
        copy = WT_ROW_KEY_COPY(rip);
        __wt_row_leaf_key_info(page, copy, &ikey, &cell, &key_data, &key_size, &key_prefix);
        kpack = NULL;
        if (__wt_cell_type(cell) == WT_CELL_KEY_OVFL) {
            kpack = &_kpack;
            __wt_cell_unpack_kv(session, page->dsk, cell, kpack);
        }

        /* Unpack the on-page value cell. */
        __wt_row_leaf_value_cell(session, page, rip, vpack);

        /* Look for an update. */
        //��ȡ��ins��Ӧ��update���������µ�V
        WT_ERR(__wt_rec_upd_select(session, r, NULL, rip, vpack, &upd_select));
        upd = upd_select.upd;

        /* Take the timestamp from the update or the cell. */
        if (upd == NULL)
            twp = &vpack->tw;
        else
            twp = &upd_select.tw;

        /*
         * If we reconcile an on disk key with a globally visible stop time point and there are no
         * new updates for that key, skip writing that key.
         */
        if (upd == NULL && __wt_txn_tw_stop_visible_all(session, twp))
            upd = &upd_tombstone;

        /* Build value cell. */
        if (upd == NULL) {
            /* Clear the on-disk cell time window if it is obsolete. */
            __wt_rec_time_window_clear_obsolete(session, NULL, vpack, r);

            /*
             * When the page was read into memory, there may not have been a value item.
             *
             * If there was a value item, check if it's a dictionary cell (a copy of another item on
             * the page). If it's a copy, we have to create a new value item as the old item might
             * have been discarded from the page.
             *
             * Repack the cell if we clear the transaction ids in the cell.
             */
            if (vpack->raw == WT_CELL_VALUE_COPY) {
                WT_ERR(__rec_cell_repack(session, btree, r, vpack, twp));

                dictionary = true;
            } else if (F_ISSET(vpack, WT_CELL_UNPACK_TIME_WINDOW_CLEARED)) {
                /*
                 * The transaction ids are cleared after restart. Repack the cell to flush the
                 * cleared transaction ids.
                 */
                if (F_ISSET(vpack, WT_CELL_UNPACK_OVERFLOW)) {
                    r->ovfl_items = true;

                    val->buf.data = vpack->data;
                    val->buf.size = vpack->size;

                    /* Rebuild the cell. */
                    val->cell_len =
                      __wt_cell_pack_ovfl(session, &val->cell, vpack->raw, twp, 0, val->buf.size);
                    val->len = val->cell_len + val->buf.size;
                } else
                    WT_ERR(__rec_cell_repack(session, btree, r, vpack, twp));

                dictionary = true;
            } else {
                val->buf.data = vpack->cell;
                val->buf.size = __wt_cell_total_len(vpack);
                val->cell_len = 0;
                val->len = val->buf.size;

                /* Track if page has overflow items. */
                if (F_ISSET(vpack, WT_CELL_UNPACK_OVERFLOW))
                    r->ovfl_items = true;
            }
        } else {
            /*
             * If we've selected an update, it should be flagged as being destined for the data
             * store.
             *
             * If not, it's either because we're not doing a history store reconciliation or because
             * the update is globally visible (in which case, subsequent updates become irrelevant
             * for reconciliation).
             */
            WT_ASSERT(session,
              F_ISSET(upd, WT_UPDATE_DS) || !F_ISSET(r, WT_REC_HS) ||
                __wt_txn_tw_start_visible_all(session, twp));

            /* The first time we find an overflow record, discard the underlying blocks. */
            if (F_ISSET(vpack, WT_CELL_UNPACK_OVERFLOW) && vpack->raw != WT_CELL_VALUE_OVFL_RM)
                WT_ERR(__wt_ovfl_remove(session, page, vpack));

            switch (upd->type) {
            case WT_UPDATE_MODIFY:
                cbt->slot = WT_ROW_SLOT(page, rip);
                WT_ERR(__wt_modify_reconstruct_from_upd_list(session, cbt, upd, cbt->upd_value));
                __wt_value_return(cbt, cbt->upd_value);
                WT_ERR(__wt_rec_cell_build_val(
                  session, r, cbt->iface.value.data, cbt->iface.value.size, twp, 0));
                dictionary = true;
                break;
            case WT_UPDATE_STANDARD:
                /* Take the value from the update. */
                WT_ERR(__wt_rec_cell_build_val(session, r, upd->data, upd->size, twp, 0));
                /*
                 * When a tombstone without a timestamp is written to disk, remove any historical
                 * versions that are greater in the history store for that key.
                 */
                if (upd_select.no_ts_tombstone && r->hs_clear_on_tombstone) {
                    WT_ERR(__wt_row_leaf_key(session, page, rip, tmpkey, true));
                    WT_ERR(__wt_rec_hs_clear_on_tombstone(session, r, WT_RECNO_OOB, tmpkey, true));
                }
                dictionary = true;
                break;
            case WT_UPDATE_TOMBSTONE:
                /*
                 * If this key/value pair was deleted, we're done.
                 *
                 * Overflow keys referencing discarded values are no longer useful, discard the
                 * backing blocks. Don't worry about reuse, reusing keys from a row-store page
                 * reconciliation seems unlikely enough to ignore.
                 */
                if (kpack != NULL && F_ISSET(kpack, WT_CELL_UNPACK_OVERFLOW) &&
                  kpack->raw != WT_CELL_KEY_OVFL_RM) {
                    /*
                     * Keys are part of the name-space, we can't remove them. If an overflow key was
                     * deleted without ever having been instantiated, instantiate it now so future
                     * searches aren't surprised when it's marked as cleared in the on-disk image.
                     */
                    if (ikey == NULL)
                        WT_ERR(__wt_row_leaf_key(session, page, rip, tmpkey, true));

                    WT_ERR(__wt_ovfl_discard_add(session, page, kpack->cell));
                }

                /*
                 * When a tombstone without a timestamp is written to disk, remove any historical
                 * versions that are greater in the history store for this key.
                 */
                if (upd_select.no_ts_tombstone && r->hs_clear_on_tombstone) {
                    WT_ERR(__wt_row_leaf_key(session, page, rip, tmpkey, true));
                    WT_ERR(__wt_rec_hs_clear_on_tombstone(session, r, WT_RECNO_OOB, tmpkey, false));
                }

                /* Not creating a key so we can't use last-key as a prefix for a subsequent key. */
                lastkey->size = 0;

                /* Proceed with appended key/value pairs. */
                goto leaf_insert;
            default:
                WT_ERR(__wt_illegal_value(session, upd->type));
            }
        }

        /*
         * Build key cell.
         *
         * If the key is an overflow key that hasn't been removed, use the original backing blocks.
         */
        key_onpage_ovfl = kpack != NULL && F_ISSET(kpack, WT_CELL_UNPACK_OVERFLOW) &&
          kpack->raw != WT_CELL_KEY_OVFL_RM;
        if (key_onpage_ovfl) {
            key->buf.data = cell;
            key->buf.size = __wt_cell_total_len(kpack);
            key->cell_len = 0;
            key->len = key->buf.size;
            ovfl_key = true;

            /* Not creating a key so we can't use last-key as a prefix for a subsequent key. */
            lastkey->size = 0;

            /* Track if page has overflow items. */
            r->ovfl_items = true;
        } else {
            /*
             * Get the key from the page or an instantiated key, or inline building the key from a
             * previous key (it's a fast path for simple, prefix-compressed keys), or by building
             * the key from scratch.
             */
            __wt_row_leaf_key_info(page, copy, NULL, &cell, &key_data, &key_size, &key_prefix);
            if (key_data == NULL) {
                if (__wt_cell_type(cell) != WT_CELL_KEY)
                    goto slow;
                kpack = &_kpack;
                __wt_cell_unpack_kv(session, page->dsk, cell, kpack);
                key_data = kpack->data;
                key_size = kpack->size;
                key_prefix = kpack->prefix;
            }

            /*
             * If the key has no prefix count, no prefix compression work is needed; else check for
             * a previously built key big enough cover this key's prefix count, else build from
             * scratch.
             */
            if (key_prefix == 0) {
                lastkey->data = key_data;
                lastkey->size = key_size;
            } else if (lastkey->size >= key_prefix) {
                /*
                 * Grow the buffer as necessary as well as ensure data has been copied into local
                 * buffer space, then append the suffix to the prefix already in the buffer. Don't
                 * grow the buffer unnecessarily or copy data we don't need, truncate the item's
                 * CURRENT data length to the prefix bytes before growing the buffer.
                 */
                lastkey->size = key_prefix;
                WT_ERR(__wt_buf_grow(session, lastkey, key_prefix + key_size));
                memcpy((uint8_t *)lastkey->mem + key_prefix, key_data, key_size);
                lastkey->size = key_prefix + key_size;
            } else {
slow:
                WT_ERR(__wt_row_leaf_key_copy(session, page, rip, lastkey));
            }

            WT_ERR(__rec_cell_build_leaf_key(session, r, lastkey->data, lastkey->size, &ovfl_key));
        }

        /* Boundary: split or write the page. */
        if (__wt_rec_need_split(r, key->len + val->len)) {
            /*
             * If we copied address blocks from the page rather than building the actual key, we
             * have to build the key now because we are about to promote it.
             */
            if (key_onpage_ovfl) {
                WT_ERR(__wt_dsk_cell_data_ref_kv(session, WT_PAGE_ROW_LEAF, kpack, r->cur));
                WT_NOT_READ(key_onpage_ovfl, false);
            }

            /*
             * Turn off prefix compression until a full key written to the new page, and (unless
             * already working with an overflow key), rebuild the key without compression.
             */
            if (r->key_pfx_compress_conf) {
                r->key_pfx_compress = false;
                r->key_pfx_last = 0;
                if (!ovfl_key)
                    WT_ERR(__rec_cell_build_leaf_key(session, r, NULL, 0, &ovfl_key));
            }

            //printf("yang test ......2222222222222222222222........__wt_rec_row_leaf....__wt_rec_split_crossing_bnd........\r\n");
            WT_ERR(__wt_rec_split_crossing_bnd(session, r, key->len + val->len));
        }

        /* Copy the key/value pair onto the page. */
        //����key���ݵ�r->first_free��Ӧ�ڴ�ռ�
        __wt_rec_image_copy(session, r, key);
        if (val->len == 0 && __rec_row_zero_len(session, twp))
            r->any_empty_value = true;
        else {
            r->all_empty_value = false;
            if (dictionary && btree->dictionary)
                WT_ERR(__wt_rec_dict_replace(session, r, twp, 0, val));
            //����value���ݵ�r->first_free��Ӧ�ڴ�ռ�
            __wt_rec_image_copy(session, r, val);
        }
        WT_TIME_AGGREGATE_UPDATE(session, &r->cur_ptr->ta, twp);

        /* Update compression state. */
        __rec_key_state_update(r, ovfl_key);

leaf_insert:
        /* Write any K/V pairs inserted into the page after this key. */
        //�����KV������ӵ�r->first_free��Ӧ�ڴ�ռ�
        if ((ins = WT_SKIP_FIRST(WT_ROW_INSERT(page, rip))) != NULL) {
            //printf("yang test ......333333333333333333333...............__wt_rec_row_leaf.......................\r\n");
            WT_ERR(__rec_row_leaf_insert(session, r, ins));
       }
    } ////������page����KV��Ȼ������KV������ӵ�r->first_free��Ӧ�ڴ�ռ�,����page���ݱ�������˳���forѭ��

    /* Write the remnant page. */
    ret = __wt_rec_split_finish(session, r);

err:
    __wt_scr_free(session, &lastkey);
    __wt_scr_free(session, &tmpkey);
    return (ret);
}
