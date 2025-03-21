/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

#define WT_MEM_TRANSFER(from_decr, to_incr, len) \
    do {                                         \
        size_t __len = (len);                    \
        (from_decr) += __len;                    \
        (to_incr) += __len;                      \
    } while (0)

/*
 * A note on error handling: main split functions first allocate/initialize new structures; failures
 * during that period are handled by discarding the memory and returning an error code, the caller
 * knows the split didn't happen and proceeds accordingly. Second, split functions update the tree,
 * and a failure in that period is catastrophic, any partial update to the tree requires a panic, we
 * can't recover. Third, once the split is complete and the tree has been fully updated, we have to
 * ignore most errors, the split is complete and correct, callers have to proceed accordingly.
 */
typedef enum {
    WT_ERR_IGNORE, /* Ignore minor errors */
    WT_ERR_PANIC,  /* Panic on all errors */
    WT_ERR_RETURN  /* Clean up and return error */
} WT_SPLIT_ERROR_PHASE;

/*
 * __split_safe_free --
 *     Free a buffer if we can be sure no thread is accessing it, or schedule it to be freed
 *     otherwise.
 */
//添加需要释放的p先添加到stash中存起来
//何时释放该session下所有的stash， 一般有用户线程一次请求完成后指向，例如__wt_txn_commit->__wt_txn_release->__wt_stash_discard
static int
__split_safe_free(WT_SESSION_IMPL *session, uint64_t split_gen, bool exclusive, void *p, size_t s)
{
    /*
     * We have swapped something in a page: it's only safe to free it if we have exclusive access.
     */
    if (exclusive) {
        __wt_overwrite_and_free_len(session, p, s);
        return (0);
    }

    return (__wt_stash_add(session, WT_GEN_SPLIT, split_gen, p, s));
}

#ifdef HAVE_DIAGNOSTIC
/*
 * __split_verify_intl_key_order --
 *     Verify the key order on an internal page after a split.
 */
static void
__split_verify_intl_key_order(WT_SESSION_IMPL *session, WT_PAGE *page)
{
    WT_BTREE *btree;
    WT_ITEM *next, _next, *last, _last, *tmp;
    WT_REF *ref;
    uint64_t recno;
    uint32_t slot;
    int cmp;

    btree = S2BT(session);

    switch (page->type) {
    case WT_PAGE_COL_INT:
        recno = 0; /* Less than any valid record number. */
        WT_INTL_FOREACH_BEGIN (session, page, ref) {
            WT_ASSERT(session, ref->home == page);

            WT_ASSERT(session, ref->ref_recno > recno);
            recno = ref->ref_recno;
        }
        WT_INTL_FOREACH_END;
        break;
    case WT_PAGE_ROW_INT:
        next = &_next;
        WT_CLEAR(_next);
        last = &_last;
        WT_CLEAR(_last);

        slot = 0;
        WT_INTL_FOREACH_BEGIN (session, page, ref) {
            WT_ASSERT(session, ref->home == page);

            /*
             * Don't compare the first slot with any other slot, it's ignored on row-store internal
             * pages.
             */
            __wt_ref_key(page, ref, &next->data, &next->size);
            if (++slot > 2) {
                WT_ASSERT(session, __wt_compare(session, btree->collator, last, next, &cmp) == 0);
                WT_ASSERT(session, cmp < 0);
            }
            tmp = last;
            last = next;
            next = tmp;
        }
        WT_INTL_FOREACH_END;
        break;
    }
}

/*
 * __split_verify_root --
 *     Verify a root page involved in a split.
 */ //HAVE_DIAGNOSTIC的时候才使用
static int
__split_verify_root(WT_SESSION_IMPL *session, WT_PAGE *page)
{
    WT_DECL_RET;
    WT_REF *ref;
    uint32_t read_flags;

    /*
     * Ignore pages not in-memory (deleted, on-disk, being read), there's no in-memory structure to
     * check.
     */
    read_flags = WT_READ_CACHE | WT_READ_NO_EVICT;

    /* The split is complete and live, verify all of the pages involved. */
    __split_verify_intl_key_order(session, page);

    WT_INTL_FOREACH_BEGIN (session, page, ref) {
        /*
         * The page might be in transition, being read or evicted or something else. Acquire a
         * hazard pointer for the page so we know its state.
         */
        //printf("yang test ............__split_verify_root.............................\r\n");
        if ((ret = __wt_page_in(session, ref, read_flags)) == WT_NOTFOUND)
            continue;
        WT_ERR(ret);

        __split_verify_intl_key_order(session, ref->page);

        WT_ERR(__wt_page_release(session, ref, read_flags));
    }
    WT_INTL_FOREACH_END;

    return (0);

err:
    /* Something really bad just happened. */
    WT_RET_PANIC(session, ret, "fatal error during page split");
}
#endif

/*
 * __split_ovfl_key_cleanup --
 *     Handle cleanup for on-page row-store overflow keys.
 */ //ref key释放
static int
__split_ovfl_key_cleanup(WT_SESSION_IMPL *session, WT_PAGE *page, WT_REF *ref)
{
    WT_CELL *cell;
    WT_CELL_UNPACK_KV kpack;
    WT_IKEY *ikey;
    uint32_t cell_offset;

    /* There's a per-page flag if there are any overflow keys at all. */
    if (!F_ISSET_ATOMIC_16(page, WT_PAGE_INTL_OVERFLOW_KEYS))
        return (0);

    /*
     * A key being discarded (page split) or moved to a different page (page deepening) may be an
     * on-page overflow key. Clear any reference to an underlying disk image, and, if the key hasn't
     * been deleted, delete it along with any backing blocks.
     */ //获取ref key
    if ((ikey = __wt_ref_key_instantiated(ref)) == NULL)
        return (0);
    if ((cell_offset = ikey->cell_offset) == 0)
        return (0);

    /* Leak blocks rather than try this twice. */
    ikey->cell_offset = 0;

    cell = WT_PAGE_REF_OFFSET(page, cell_offset);
    __wt_cell_unpack_kv(session, page->dsk, cell, &kpack);
    if (FLD_ISSET(kpack.flags, WT_CELL_UNPACK_OVERFLOW) && kpack.raw != WT_CELL_KEY_OVFL_RM)
        WT_RET(__wt_ovfl_discard(session, page, cell));

    return (0);
}

/*
 * __split_ref_move --
 *     Move a WT_REF from one page to another, including updating accounting information.
 */
static int
__split_ref_move(WT_SESSION_IMPL *session, WT_PAGE *from_home, WT_REF **from_refp, size_t *decrp,
  WT_REF **to_refp, size_t *incrp)
{
    WT_ADDR *addr, *ref_addr;
    WT_CELL_UNPACK_ADDR unpack;
    WT_DECL_RET;
    WT_IKEY *ikey;
    WT_REF *ref;
    size_t size;
    void *key;

    ref = *from_refp;
    addr = NULL;

    /*
     * The from-home argument is the page into which the "from" WT_REF may point, for example, if
     * there's an on-page key the "from" WT_REF references, it will be on the page "from-home".
     *
     * Instantiate row-store keys, and column- and row-store addresses in the WT_REF structures
     * referenced by a page that's being split. The WT_REF structures aren't moving, but the index
     * references are moving from the page we're splitting to a set of new pages, and so we can no
     * longer reference the block image that remains with the page being split.
     *
     * No locking is required to update the WT_REF structure because we're the only thread splitting
     * the page, and there's no way for readers to race with our updates of single pointers. The
     * changes have to be written before the page goes away, of course, our caller owns that
     * problem.
     */
    if (from_home->type == WT_PAGE_ROW_INT) {
        /*
         * Row-store keys: if it's not yet instantiated, instantiate it. If already instantiated,
         * check for overflow cleanup (overflow keys are always instantiated).
         */
        if ((ikey = __wt_ref_key_instantiated(ref)) == NULL) {
            __wt_ref_key(from_home, ref, &key, &size);
            WT_RET(__wt_row_ikey(session, 0, key, size, ref));
            ikey = ref->ref_ikey;
        } else {
            WT_RET(__split_ovfl_key_cleanup(session, from_home, ref));
            *decrp += sizeof(WT_IKEY) + ikey->size;
        }
        *incrp += sizeof(WT_IKEY) + ikey->size;
    }

    /*
     * If there's no address at all (the page has never been written), or the address has already
     * been instantiated, there's no work to do. Otherwise, the address still references a split
     * page on-page cell, instantiate it. We can race with reconciliation and/or eviction of the
     * child pages, be cautious: read the address and verify it, and only update it if the value is
     * unchanged from the original. In the case of a race, the address must no longer reference the
     * split page, we're done.
     */
    WT_ORDERED_READ(ref_addr, ref->addr);
    if (ref_addr != NULL && !__wt_off_page(from_home, ref_addr)) {
        __wt_cell_unpack_addr(session, from_home->dsk, (WT_CELL *)ref_addr, &unpack);
        WT_RET(__wt_calloc_one(session, &addr));
        WT_TIME_AGGREGATE_COPY(&addr->ta, &unpack.ta);
        WT_ERR(__wt_memdup(session, unpack.data, unpack.size, &addr->addr));
        addr->size = (uint8_t)unpack.size;
        switch (unpack.raw) {
        case WT_CELL_ADDR_DEL:
            /* Could only have been fast-truncated if there were no overflow items. */
            addr->type = WT_ADDR_LEAF_NO;
            break;
        case WT_CELL_ADDR_INT:
            addr->type = WT_ADDR_INT;
            break;
        case WT_CELL_ADDR_LEAF:
            addr->type = WT_ADDR_LEAF;
            break;
        case WT_CELL_ADDR_LEAF_NO:
            addr->type = WT_ADDR_LEAF_NO;
            break;
        default:
            WT_ERR(__wt_illegal_value(session, unpack.raw));
        }
        /* If the compare-and-swap is successful, clear addr to skip the free at the end. */
        if (__wt_atomic_cas_ptr(&ref->addr, ref_addr, addr))
            addr = NULL;
    }

    /* And finally, copy the WT_REF pointer itself. */
    *to_refp = ref;
    WT_MEM_TRANSFER(*decrp, *incrp, sizeof(WT_REF));

err:
    if (addr != NULL) {
        __wt_free(session, addr->addr);
        __wt_free(session, addr);
    }
    return (ret);
}

/*
 * __split_ref_final --
 *     Finalize the WT_REF move.
 */
static void
__split_ref_final(WT_SESSION_IMPL *session, uint64_t split_gen, WT_PAGE ***lockedp)
{
    WT_PAGE **locked;
    size_t i;

    /* The parent page's page index has been updated. */
    WT_WRITE_BARRIER();

    if ((locked = *lockedp) == NULL)
        return;
    *lockedp = NULL;

    /*
     * The moved child pages are locked to prevent them from splitting before the parent move
     * completes, unlock them as the final step.
     *
     * Once the split is live, newly created internal pages might be evicted and their WT_REF
     * structures freed. If that happens before all threads exit the index of the page that
     * previously "owned" the WT_REF, a thread might see a freed WT_REF. To ensure that doesn't
     * happen, the created pages are set to the current split generation and so can't be evicted
     * until all readers have left the old generation.
     */
    for (i = 0; locked[i] != NULL; ++i) {
        if (split_gen != 0 && WT_PAGE_IS_INTERNAL(locked[i]))
            locked[i]->pg_intl_split_gen = split_gen;
        WT_PAGE_UNLOCK(session, locked[i]);
    }
    __wt_free(session, locked);
}

/*
 * __split_ref_prepare --
 *     Prepare a set of WT_REFs for a move.
 */
static int
__split_ref_prepare(
  WT_SESSION_IMPL *session, 
  //pindex为新增这一层的的index[]
  WT_PAGE_INDEX *pindex, 
  WT_PAGE ***lockedp, bool skip_first)
{
    WT_DECL_RET;
    WT_PAGE *child, **locked;
    WT_REF *child_ref, *ref;
    size_t alloc, cnt;
    uint32_t i, j;

    *lockedp = NULL;

    locked = NULL;

    /*
     * Update the moved WT_REFs so threads moving through them start looking at the created
     * children's page index information. Because we've not yet updated the page index of the parent
     * page into which we are going to split this subtree, a cursor moving through these WT_REFs
     * will ascend into the created children, but eventually fail as that parent page won't yet know
     * about the created children pages. That's OK, we spin there until the parent's page index is
     * updated.
     *
     * Lock the newly created page to ensure none of its children can split. First, to ensure all of
     * the child pages are updated before any pages can split. Second, to ensure the original split
     * completes before any of the children can split. The latter involves split generations: the
     * original split page has references to these children. If they split immediately, they could
     * free WT_REF structures based on split generations earlier than the split generation we'll
     * eventually choose to protect the original split page's previous page index.
     */
    alloc = cnt = 0;
    for (i = skip_first ? 1 : 0; i < pindex->entries; ++i) {
        //指向下一层
        ref = pindex->index[i];
        child = ref->page;

        //printf("yang test ............__split_ref_prepare...............page type:%u, pindex->entries:%u\r\n", 
         //   child->type, pindex->entries);
        /* Track the locked pages for cleanup. */
        WT_ERR(__wt_realloc_def(session, &alloc, cnt + 2, &locked));
        locked[cnt++] = child;

       // printf("yang test .........__split_ref_prepare...........WT_PAGE_LOCK.....page:%p, %s\r\n", child, __wt_page_type_string(child->type));
        WT_PAGE_LOCK(session, child);

        /* Switch the WT_REF's to their new page. */
        j = 0;
        WT_INTL_FOREACH_BEGIN (session, child, child_ref) {
            child_ref->home = child;
            child_ref->pindex_hint = j++;
        }
        WT_INTL_FOREACH_END;

#ifdef HAVE_DIAGNOSTIC
        WT_WITH_PAGE_INDEX(session, __split_verify_intl_key_order(session, child));
#endif
    }
    *lockedp = locked;
    return (0);

err:
    __split_ref_final(session, 0, &locked);
    return (ret);
}

/*
 * __split_root --
 *     Split the root page in-memory, deepening the tree.

root page最开始随着数据写入root index[]下面leaf page增加到185后，root page总内存超过btree->maxmempage大小
这时候__split_internal_should_split满足条件，root page开始拆分，拆分过程是增加一层internal page, meig
                       root page                                第一层:1个root page
                      /     |     \
                    /       |       \
                  /         |         \
       leaf-1 page      .........   leaf-10002 page              第二层:10002个leaf page

                            |
                            |
                            |
                            |
                            |
                           \|/
                         root page                                 第一层:1个root page,index[]大小101
                         /   |      \
                       /     |       \
                     /       |         \
         internal-1 page   .......    internal-101 page             第二层:101个internal page，前面99个internal page的index[]大小100，最后一个internal page的index[]大小102
          /      \           |           /    \
         /        \          |          /       \
        /          \      .......      /          \
 leaf-1 page    leaf-100 page   leaf-9900 page   leaf-10002 page    第三层:10001个leaf page (每组100个，最后一组102个)

 */
static int
__split_root(WT_SESSION_IMPL *session, WT_PAGE *root)
{
    WT_BTREE *btree;
    WT_DECL_RET;
    WT_PAGE *child, **locked;
    WT_PAGE_INDEX *alloc_index, *child_pindex, *pindex;
    WT_REF **alloc_refp, **child_refp, *ref, **root_refp;
    WT_SPLIT_ERROR_PHASE complete;
    size_t child_incr, root_decr, root_incr, size;
    uint64_t split_gen;
    uint32_t children, chunk, i, j, remain;
    uint32_t slots;
    void *p;

    WT_STAT_CONN_DATA_INCR(session, cache_eviction_deepen);
    WT_STAT_CONN_DATA_INCR(session, cache_eviction_split_internal);

    btree = S2BT(session);
    alloc_index = NULL;
    locked = NULL;
    root_decr = root_incr = 0;
    complete = WT_ERR_RETURN;

    /* Mark the root page dirty. */
    //标识这个root page有修改
    WT_RET(__wt_page_modify_init(session, root));
    __wt_page_modify_set(session, root);

    /*
     * Our caller is holding the root page locked to single-thread splits, which means we can safely
     * look at the page's index without setting a split generation.
     */
    //获取root page下面所有的子page index
    pindex = WT_INTL_INDEX_GET_SAFE(root);

    /*
     * Decide how many child pages to create, then calculate the standard chunk and whatever
     * remains. Sanity check the number of children: the decision to split matched to the
     * deepen-per-child configuration might get it wrong.
     */
    //计算出有多少个child， 每个child包含多少个chunk，最后一个child的entries数要加上平均后遗留的entries
    //例如当前pindex数组大小1000，也就是root下面有999个子page，则可以拆分为999/100=9组(每组100个page) + 1组(99个page)
    children = pindex->entries / btree->split_deepen_per_child;
    if (children < 10) {
        if (pindex->entries < WT_INTERNAL_SPLIT_MIN_KEYS)
            return (__wt_set_return(session, EBUSY));
        children = 10;
    }
    //分组后每组包含的page，也就是前面距离中的100(每组100个page)
    chunk = pindex->entries / children;
    //最后一组存储最后的那几个page，也就是前面举例中的99(99个page)
    remain = pindex->entries - chunk * (children - 1);

    //yang test .........__split_root.....................children:10, chunk:18, remain:23
    //printf("yang test .........__split_root.....................children:%d, chunk:%d, remain:%d\r\n",
    //    (int)children, (int)chunk, (int)remain);

    //10011 root page elements, splitting into 100 children
    __wt_verbose(session, WT_VERB_SPLIT,
      "%p: %" PRIu32 " root page elements, splitting into %" PRIu32 " children", (void *)root,
      pindex->entries, children);

    /*
     * Allocate a new WT_PAGE_INDEX and set of WT_REF objects to be inserted into the root page,
     * replacing the root's page-index.
     */
    //注释中新增层的internal ref数组空间分配，总计100组个第二层children
    size = sizeof(WT_PAGE_INDEX) + children * sizeof(WT_REF *);
    WT_ERR(__wt_calloc(session, 1, size, &alloc_index));
    root_incr += size;
    alloc_index->index = (WT_REF **)(alloc_index + 1);
    alloc_index->entries = children;
    alloc_refp = alloc_index->index;
    for (i = 0; i < children; alloc_refp++, ++i)
        WT_ERR(__wt_calloc_one(session, alloc_refp));
    root_incr += children * sizeof(WT_REF);

    /* Allocate child pages, and connect them into the new page index. */
    //新增这一层的page空间创建及从ref空间赋值
    for (root_refp = pindex->index, alloc_refp = alloc_index->index, i = 0; i < children; ++i) {
        slots = i == children - 1 ? remain : chunk;
        //printf("yang test................__split_root....................type:%u\r\n", root->type); //打印出来的值为6，也就是WT_PAGE_ROW_INT
        //child为internal page
        WT_ERR(__wt_page_alloc(session, root->type, slots, false, &child));

        /*
         * Initialize the page's child reference; we need a copy of the page's key.
         */
        //说明是新增的第二层
        ref = *alloc_refp++;
        ref->home = root;//指向root
        ref->page = child;
        ref->addr = NULL;
        //该internal page的key拷贝到该internal ref
        if (root->type == WT_PAGE_ROW_INT) {
            __wt_ref_key(root, *root_refp, &p, &size);
            WT_ERR(__wt_row_ikey(session, 0, p, size, ref));
            root_incr += sizeof(WT_IKEY) + size;
        } else
            ref->ref_recno = (*root_refp)->ref_recno;
        F_SET(ref, WT_REF_FLAG_INTERNAL);
        WT_REF_SET_STATE(ref, WT_REF_MEM);

        /*
         * Initialize the child page. Block eviction in newly created pages and mark them dirty.
         */
        child->pg_intl_parent_ref = ref;
        WT_ERR(__wt_page_modify_init(session, child));
        __wt_page_modify_set(session, child);

        /*
         * The newly allocated child's page index references the same structures as the root. (We
         * cannot move WT_REF structures, threads may be underneath us right now changing the
         * structure state.) However, if the WT_REF structures reference on-page information, we
         * have to fix that, because the disk image for the page that has a page index entry for the
         * WT_REF is about to change.
         */
        //指向新增层的下一层
        child_pindex = WT_INTL_INDEX_GET_SAFE(child);
        child_incr = 0;
        //以注释的图形化为例，slot也就是每个组的leaf page个数，前面的99组slot=100，第100组slots为102
        // 也就是实现注释图形化中的第2层和第3层的映射，实现ref内存拷贝
        for (child_refp = child_pindex->index, j = 0; j < slots; ++child_refp, ++root_refp, ++j)
            WT_ERR(__split_ref_move(session, root, root_refp, &root_decr, child_refp, &child_incr));

        __wt_cache_page_inmem_incr(session, child, child_incr);
    }
    WT_ASSERT(session, alloc_refp - alloc_index->index == (ptrdiff_t)alloc_index->entries);
    WT_ASSERT(session, root_refp - pindex->index == (ptrdiff_t)pindex->entries);

    /*
     * Flush our writes and start making real changes to the tree, errors are fatal.
     */
    WT_PUBLISH(complete, WT_ERR_PANIC);

    /* Prepare the WT_REFs for the move. */
    //也就是锁住新增层的所有page
    WT_ERR(__split_ref_prepare(session, alloc_index, &locked, false));

    /* Encourage a race */
    __wt_timing_stress(session, WT_TIMING_STRESS_SPLIT_1, NULL);

    /*
     * Confirm the root page's index hasn't moved, then update it, which makes the split visible to
     * threads descending the tree.
     */
    WT_ASSERT(session, WT_INTL_INDEX_GET_SAFE(root) == pindex);
    WT_INTL_INDEX_SET(root, alloc_index);
    alloc_index = NULL;

    /* Encourage a race */
    __wt_timing_stress(session, WT_TIMING_STRESS_SPLIT_2, NULL);

    /*
     * Mark the root page with the split generation.
     *
     * Note: as the root page cannot currently be evicted, the root split generation isn't ever
     * used. That said, it future proofs eviction and isn't expensive enough to special-case.
     */
    WT_FULL_BARRIER();
    split_gen = __wt_gen(session, WT_GEN_SPLIT);
    root->pg_intl_split_gen = split_gen;

    /* Finalize the WT_REF move. */
    __split_ref_final(session, split_gen, &locked);

#ifdef HAVE_DIAGNOSTIC
    WT_WITH_PAGE_INDEX(session, ret = __split_verify_root(session, root));
    WT_ERR(ret);
#endif

    /* The split is complete and verified, ignore benign errors. */
    complete = WT_ERR_IGNORE;

    /*
     * We can't free the previous root's index, there may be threads using it. Add to the session's
     * discard list, to be freed once we know no threads can still be using it.
     *
     * This change requires care with error handling: we have already updated the page with a new
     * index. Even if stashing the old value fails, we don't roll back that change, because threads
     * may already be using the new index.
     */
    size = sizeof(WT_PAGE_INDEX) + pindex->entries * sizeof(WT_REF *);
    WT_TRET(__split_safe_free(session, split_gen, false, pindex, size));
    root_decr += size;

    /* Adjust the root's memory footprint. */
    __wt_cache_page_inmem_incr(session, root, root_incr);
    __wt_cache_page_inmem_decr(session, root, root_decr);

    __wt_gen_next(session, WT_GEN_SPLIT, NULL);
err:
    __split_ref_final(session, 0, &locked);

    switch (complete) {
    case WT_ERR_RETURN:
        __wt_free_ref_index(session, root, alloc_index, true);
        break;
    case WT_ERR_IGNORE:
        if (ret != WT_PANIC) {
            if (ret != 0)
                __wt_err(session, ret,
                  "ignoring not-fatal error during root page split to deepen the tree");
            ret = 0;
            break;
        }
    /* FALLTHROUGH */
    case WT_ERR_PANIC:
        ret = __wt_panic(session, ret, "fatal error during root page split to deepen the tree");
        break;
    }
    return (ret);
}

/*
 * __split_parent_discard_ref --
 *     Worker routine to discard WT_REFs for the split-parent function.
 //老ref在__split_parent->__split_parent_discard_ref释放，老page在__split_multi->__wt_page_out释放
 */ //ref及对page资源释放
static int
__split_parent_discard_ref(WT_SESSION_IMPL *session, WT_REF *ref, WT_PAGE *parent, size_t *decrp,
  uint64_t split_gen, bool exclusive)
{
    WT_DECL_RET;
    WT_IKEY *ikey;
    size_t size;

    /*
     * Row-store trees where the old version of the page is being discarded: the previous parent
     * page's key for this child page may have been an on-page overflow key. In that case, if the
     * key hasn't been deleted, delete it now, including its backing blocks. We are exchanging the
     * WT_REF that referenced it for the split page WT_REFs and their keys, and there's no longer
     * any reference to it. Done after completing the split (if we failed, we'd leak the underlying
     * blocks, but the parent page would be unaffected).
     */
    if (parent->type == WT_PAGE_ROW_INT) {
        WT_TRET(__split_ovfl_key_cleanup(session, parent, ref));
        ikey = __wt_ref_key_instantiated(ref);
        if (ikey != NULL) {
            size = sizeof(WT_IKEY) + ikey->size;
            WT_TRET(__split_safe_free(session, split_gen, exclusive, ikey, size));
            *decrp += size;
        }
    }

    /* Free any backing fast-truncate memory. */
    __wt_free(session, ref->page_del);

    /* Free the backing block and address. */
    //printf("yang test ...................__split_parent_discard_ref............ \r\n");
    WT_TRET(__wt_ref_block_free(session, ref));

    /*
     * Set the WT_REF state. It may be possible to immediately free the WT_REF, so this is our last
     * chance.
     */
    WT_REF_SET_STATE(ref, WT_REF_SPLIT);

    WT_TRET(__split_safe_free(session, split_gen, exclusive, ref, sizeof(WT_REF)));
    *decrp += sizeof(WT_REF);

    return (ret);
}

/*
 * __split_parent --
 *     Resolve a multi-page split, inserting new information into the parent.
 //ref记录split之前的page,ref_new为split后的page, ref_new是一个数组，代表从一个ref拆分到了new_entries个ref
 */
static int
__split_parent(WT_SESSION_IMPL *session, 
    //ref对应需要split的leaf page
    WT_REF *ref, 
    WT_REF **ref_new, uint32_t new_entries,
  size_t parent_incr,
  bool exclusive,
  //除了internal leaf为false，其他都为true
  bool discard)
{
    WT_BTREE *btree;
    WT_DECL_ITEM(scr);
    WT_DECL_RET;
    WT_PAGE *parent;
    WT_PAGE_INDEX *alloc_index, *pindex;
    WT_REF **alloc_refp, *next_ref;
    WT_SPLIT_ERROR_PHASE complete;
    size_t parent_decr, size;
    uint64_t split_gen;
    uint32_t deleted_entries, parent_entries, result_entries;
    uint32_t *deleted_refs;
    uint32_t hint, i, j;
    bool empty_parent;
    WT_PAGE *page;

    for (uint32_t ii = 0; ii < new_entries; ii++) {
        page = ref_new[ii]->page;
        //if (page == NULL)
        //    printf("yang test .....__split_parent.................page:%p\r\n", page);
        if (page && false)
            __wt_verbose(
                session, WT_VERB_EVICT, "__split_parent,page%u: %p (%s) memory_footprint:%d, page->dsk->mem_size:%d, entries:%d", ii, (void *)page, 
                __wt_page_type_string(page->type), (int)page->memory_footprint, page->dsk ? (int)page->dsk->mem_size : 0, (int)page->entries);
    }

    btree = S2BT(session);
    //也就是ref的父节点
    parent = ref->home;

    alloc_index = pindex = NULL;
    parent_decr = 0;
    deleted_refs = NULL;
    empty_parent = false;
    complete = WT_ERR_RETURN;

    /* Mark the page dirty. */
    WT_RET(__wt_page_modify_init(session, parent));
    __wt_page_modify_set(session, parent);

    /*
     * We've locked the parent, which means it cannot split (which is the only reason to worry about
     * split generation values).
     */
    pindex = WT_INTL_INDEX_GET_SAFE(parent);
    parent_entries = pindex->entries;

    /*
     * Remove any refs to deleted pages while we are splitting, we have the internal page locked
     * down and are copying the refs into a new page-index array anyway.
     *
     * We can't do this if there is a sync running in the tree in another session: removing the refs
     * frees the blocks for the deleted pages, which can corrupt the free list calculated by the
     * sync.
     */
    deleted_entries = 0;
    //删除该parent下面因为本session做checkpoint标识为WT_REF_DELETED的page
    if (!WT_BTREE_SYNCING(btree) || WT_SESSION_BTREE_SYNC(session))
        for (i = 0; i < parent_entries; ++i) {
            next_ref = pindex->index[i];
            WT_ASSERT(session, next_ref->state != WT_REF_SPLIT);

            /*
             * Protect against including the replaced WT_REF in the list of deleted items. Also, in
             * VLCS, avoid dropping the leftmost page even if it's deleted, because the namespace
             * gap that produces causes search to fail. (For other gaps, search just takes the next
             * page to the left; but for the leftmost page in an internal page that doesn't work
             * unless we update the internal page's start recno on the fly and restart the search,
             * which seems like asking for trouble.)
             */
            //ref对应parent下面需要delete的page添加到deleted_refs中记录下来，在后面进行真正的page释放
            if (next_ref != ref && next_ref->state == WT_REF_DELETED &&
              (btree->type != BTREE_COL_VAR || i != 0) &&
              __wt_delete_page_skip(session, next_ref, true) &&
              WT_REF_CAS_STATE(session, next_ref, WT_REF_DELETED, WT_REF_LOCKED)) {
                if (scr == NULL)
                    WT_ERR(__wt_scr_alloc(session, 10 * sizeof(uint32_t), &scr));
                WT_ERR(__wt_buf_grow(session, scr, (deleted_entries + 1) * sizeof(uint32_t)));

                //需要清理的page添加到该数组中
                deleted_refs = scr->mem;
                deleted_refs[deleted_entries++] = i;
            }
        }

    /*
     * The final entry count is the original count, where one entry will be replaced by some number
     * of new entries, and some number will be deleted.
     */
    //剔除parent下面需要删除的page后的page数
    result_entries = (parent_entries + (new_entries - 1)) - deleted_entries;

    /*
     * If there are no remaining entries on the parent, give up, we can't leave an empty internal
     * page. Mark it to be evicted soon and clean up any references that have changed state.
     */
    if (result_entries == 0) {
        empty_parent = true;

        if (!__wt_ref_is_root(parent->pg_intl_parent_ref))
            //不是root节点
            __wt_page_evict_soon(session, parent->pg_intl_parent_ref);

        //如果ref对应父节点ref是root节点
        goto err;
    }

    /*
     * Allocate and initialize a new page index array for the parent, then copy references from the
     * original index array, plus references from the newly created split array, into place.
     *
     * Update the WT_REF's page-index hint as we go. This can race with a thread setting the hint
     * based on an older page-index, and the change isn't backed out in the case of an error, so
     * there ways for the hint to be wrong; OK because it's just a hint.
     */
    size = sizeof(WT_PAGE_INDEX) + result_entries * sizeof(WT_REF *);
    WT_ERR(__wt_calloc(session, 1, size, &alloc_index));
    parent_incr += size;
    //指向真实的index数组地址
    alloc_index->index = (WT_REF **)(alloc_index + 1);
    //数组大小
    alloc_index->entries = result_entries;
    for (alloc_refp = alloc_index->index, hint = i = 0; i < parent_entries; ++i) {
        next_ref = pindex->index[i];

        //需要拆分的就是这个ref, 一个ref拆分为new_entries个
        if (next_ref == ref) {
            for (j = 0; j < new_entries; ++j) {
                ref_new[j]->home = parent;
                ref_new[j]->pindex_hint = hint++;
                *alloc_refp++ = ref_new[j];
                //if (ref_new[j]->page == NULL)
                 //   printf("yang test .....__split_parent........2.........page:%p\r\n", ref_new[j]->page);
            }
            continue;
        }

        /* Skip refs we have marked for deletion. */
        if (deleted_entries != 0) {
            for (j = 0; j < deleted_entries; ++j)
                if (deleted_refs[j] == i)
                    break;
            if (j < deleted_entries)
                continue;
        }

        next_ref->pindex_hint = hint++;
        *alloc_refp++ = next_ref;
    }

    /* Check we filled in the expected number of entries. */
    WT_ASSERT(session, alloc_refp - alloc_index->index == (ptrdiff_t)result_entries);

    /* Start making real changes to the tree, errors are fatal. */
    WT_NOT_READ(complete, WT_ERR_PANIC);

    /* Encourage a race */
    __wt_timing_stress(session, WT_TIMING_STRESS_SPLIT_3, NULL);

    /*
     * Confirm the parent page's index hasn't moved then update it, which makes the split visible to
     * threads descending the tree.
     */
    WT_ASSERT(session, WT_INTL_INDEX_GET_SAFE(parent) == pindex);
    //设置父节点的index数组为新的alloc_index
    WT_INTL_INDEX_SET(parent, alloc_index);
    alloc_index = NULL;

    /* Encourage a race */
    __wt_timing_stress(session, WT_TIMING_STRESS_SPLIT_4, NULL);

    /*
     * Get a generation for this split, mark the page. This must be after the new index is swapped
     * into place in order to know that no readers with the new generation will look at the old
     * index.
     */
    WT_FULL_BARRIER();
    split_gen = __wt_gen(session, WT_GEN_SPLIT);
    //记录该internal page分裂的次数  __split_parent中赋值
    parent->pg_intl_split_gen = split_gen;

#ifdef HAVE_DIAGNOSTIC
    WT_WITH_PAGE_INDEX(session, __split_verify_intl_key_order(session, parent));
#endif

    /* The split is complete and verified, ignore benign errors. */
    complete = WT_ERR_IGNORE;

    /*
     * The new page index is in place. Threads cursoring in the tree are blocked because the WT_REF
     * being discarded (if any), and deleted WT_REFs (if any) are in a locked state. Changing the
     * locked state to split unblocks those threads and causes them to re-calculate their position
     * based on the just-updated parent page's index. The split state doesn't lock the WT_REF.addr
     * information which is read by cursor threads in some tree-walk cases: free the WT_REF we were
     * splitting and any deleted WT_REFs we found, modulo the usual safe free semantics, then reset
     * the WT_REF state.
     */
    //老的ref需要释放资源，因为已经被新的ref_new给替代了，当一个线程evict的时候ref处于WT_REF_LOCKED，其他线程都需要
    //  在__wt_page_in_func因为WT_REF_LOCKED状态延迟等待，所以这里ref可以直接释放，因为被新的ref替代掉了
    if (discard) {
        //要么conn close，或者ref状态为WT_REF_LOCKED
        WT_ASSERT(session, exclusive || ref->state == WT_REF_LOCKED);
        WT_TRET(
          //老的ref需要释放资源，因为已经被新的ref_new给替代了

          //老ref在__split_parent->__split_parent_discard_ref释放，老page在__split_multi->__wt_page_out释放
          __split_parent_discard_ref(session, ref, parent, &parent_decr, split_gen, exclusive));
    }

    //把上层parent page下面所有的需要删除的page清理掉
    for (i = 0; i < deleted_entries; ++i) {
        next_ref = pindex->index[deleted_refs[i]];
        WT_ASSERT(session, next_ref->state == WT_REF_LOCKED);
        WT_TRET(__split_parent_discard_ref(
          session, next_ref, parent, &parent_decr, split_gen, exclusive));
    }

    /*
     * !!!
     * The original WT_REF has now been freed, we can no longer look at it.
     */

    /*
     * Don't cache the change: not required for correctness, but stops threads spinning on incorrect
     * page references.
     */
    WT_FULL_BARRIER();

    /*
     * We can't free the previous page index, there may be threads using it. Add it to the session
     * discard list, to be freed when it's safe.
     */
    size = sizeof(WT_PAGE_INDEX) + pindex->entries * sizeof(WT_REF *);
    WT_TRET(__split_safe_free(session, split_gen, exclusive, pindex, size));
    parent_decr += size;

    /* Adjust the parent's memory footprint. */
    __wt_cache_page_inmem_incr(session, parent, parent_incr);
    __wt_cache_page_inmem_decr(session, parent, parent_decr);

    /*
     * We've discarded the WT_REFs and swapping in a new page index released the page for eviction;
     * we can no longer look inside the WT_REF or the page, be careful logging the results.
     */
    __wt_verbose(session, WT_VERB_SPLIT,
      "%p: split into parent, %" PRIu32 "->%" PRIu32 ", %" PRIu32 " deleted", (void *)ref,
      parent_entries, result_entries, deleted_entries);

    //split gen自增，表示做了一次split操作，表示对所有page总共做了多少次split
    __wt_gen_next(session, WT_GEN_SPLIT, NULL);
err:
    /*
     * A note on error handling: if we completed the split, return success, nothing really bad can
     * have happened, and our caller has to proceed with the split.
     */
    switch (complete) {
    case WT_ERR_RETURN:
        /* Unlock WT_REFs locked because they were in a deleted state. */
        for (i = 0; i < deleted_entries; ++i) {
            next_ref = pindex->index[deleted_refs[i]];
            WT_ASSERT(session, next_ref->state == WT_REF_LOCKED);
            WT_REF_SET_STATE(next_ref, WT_REF_DELETED);
        }

        __wt_free_ref_index(session, NULL, alloc_index, false);
        /*
         * The split couldn't proceed because the parent would be empty, return EBUSY so our caller
         * knows to unlock the WT_REF that's being deleted, but don't be noisy, there's nothing
         * wrong.
         */
        if (empty_parent)
            ret = __wt_set_return(session, EBUSY);
        break;
    case WT_ERR_IGNORE:
        if (ret != WT_PANIC) {
            if (ret != 0)
                __wt_err(session, ret, "ignoring not-fatal error during parent page split");
            ret = 0;
            break;
        }
    /* FALLTHROUGH */
    case WT_ERR_PANIC:
        ret = __wt_panic(session, ret, "fatal error during parent page split");
        break;
    }
    __wt_scr_free(session, &scr);
    return (ret);
}

/*
 * __split_internal --
 *     Split an internal page into its parent.

 root page最开始随着数据写入root index[]下面leaf page增加到185后，root page总内存超过btree->maxmempage大小
这时候__split_internal_should_split满足条件，root page开始拆分，拆分过程是增加一层internal page, meig
                       root page                               第一层:1个root page
                      /     |     \
                    /       |       \
                  /         |         \
       leaf-1 page      .........   leaf-185 page              第二层:185个root page

                            |
                            |               \
                            | ---------------  root page内存超限的拆分过程
                            |               /
                            |
                           \|/
                         root page                              第一层:1个root page,index[]大小10
                         /   |      \
                       /     |       \
                     /       |         \
         internal-1 page   .......    internal-10 page          第二层:10个internal page，前面9个internal page的index[]大小18，最后一个internal page的index[]大小23
          /      \           |           /      \
         /        \          |          /         \
        /          \       .......     /            \
 leaf-1 page ...leaf-18 page   leaf-162 page  .... leaf-185 page    第三层:拆分完成后internal[1-9]每个包含18个子page, internal-10包含23个子page
                             |
                             |
                             |
                             |                   \
                             |-------------------- internal(非root page)的拆分过程, 假设internal-10下面的子page增长过多，这时候internal-10内存超过btree->maxmempage大小
                             |                   /
                             |
                             |
                            \|/
                           root page                              第一层:1个root page,index[]大小10
                         /   |        \
                       /     |         \
                     /       |          \
         internal-1 page   .......     internal-10 page          第二层:10个internal page，前面9个internal page的index[]大小18，最后一个internal page的index[]大小23
          /      \           |             /      \
         /        \          |            /         \
        /          \       .......       /  ........ \
 leaf-1 page .... leaf-18 page      leaf-162 page  .... leaf-347(internal-10下面的page从最开始的23个增加到185个) ，这时候internal-10内存超过btree->maxmempage大小

                             |
                             |                   \
                             |-------------------- internal-10超限，开始拆分
                             |                   /
                             |
                             |
                            \|/
                           root  page
                           / |    \              \                                           \
                         /   |     \                \                                          \
                        /    |     \                   \                                         \
                       /     |      \                    \                                         \
                     /       |       \(复用拆分前page)      \                                       \
         internal-1 page   .......  internal-10 page       internal-11 page   ................. internal-19 page        第2层: 从10个增加到19个inter page,为什么是增加9个，原因是复用了原来的internal-11
          /      \           |          /      \                     /   \                          /    \
         /        \          |         /        \                   /      \                       /      \
        /          \       .......    /          \                 /        \                     /        \
 leaf-1 page .... leaf-18 page    leaf-162 page   leaf-180      leaf-198   leaf-198 ......   leaf-329     leaf-347      第3层: 之前internal 10page下面的185个page拆分到了新的10个internal page下面
 */
static int
__split_internal(WT_SESSION_IMPL *session, WT_PAGE *parent, WT_PAGE *page)
{
    WT_BTREE *btree;
    WT_DECL_RET;
    WT_PAGE *child, **locked;
    WT_PAGE_INDEX *alloc_index, *child_pindex, *pindex, *replace_index;
    WT_REF **alloc_refp, **child_refp, *page_ref, **page_refp, *ref;
    WT_SPLIT_ERROR_PHASE complete;
    size_t child_incr, page_decr, page_incr, parent_incr, size;
    uint64_t split_gen;
    uint32_t children, chunk, i, j, remain;
    uint32_t slots;
    void *p;

    WT_STAT_CONN_DATA_INCR(session, cache_eviction_split_internal);

    /* Mark the page dirty. */
    WT_RET(__wt_page_modify_init(session, page));
    __wt_page_modify_set(session, page);

    btree = S2BT(session);
    alloc_index = replace_index = NULL;
    //也就是需要拆分的page ref
    page_ref = page->pg_intl_parent_ref;
    locked = NULL;
    page_decr = page_incr = parent_incr = 0;
    complete = WT_ERR_RETURN;

    /*
     * Our caller is holding the page locked to single-thread splits, which means we can safely look
     * at the page's index without setting a split generation.
     */
    //需要拆分的page的index[], 也就是需要拆分的page下面的所有子page数组
    pindex = WT_INTL_INDEX_GET_SAFE(page);

    /*
     * Decide how many child pages to create, then calculate the standard chunk and whatever
     * remains. Sanity check the number of children: the decision to split matched to the
     * deepen-per-child configuration might get it wrong.
     */
    children = pindex->entries / btree->split_deepen_per_child;
    if (children < 10) {
        if (pindex->entries < WT_INTERNAL_SPLIT_MIN_KEYS)
            return (__wt_set_return(session, EBUSY));
        children = 10;//WT_SPLIT_DEEPEN_MIN_CREATE_CHILD_PAGES
    }
    //例如需要拆分的page下面有185个子page，则这185个子page可以细分位10组，第1-9组各包含18个，第10个page包含23个
    chunk = pindex->entries / children;  //第1-9组各包含18个，
    remain = pindex->entries - chunk * (children - 1); //第10组page包含23个

    //yang test .........__split_internal.....................children:10, chunk:18, remain:23
    // printf("yang test .........__split_internal.....................children:%d, chunk:%d, remain:%d\r\n",
    //    (int)children, (int)chunk, (int)remain);
    __wt_verbose(session, WT_VERB_SPLIT,
      "%p: %" PRIu32 " internal page elements, splitting %" PRIu32 " children into parent %p",
      (void *)page, pindex->entries, children, (void *)parent);

    /*
     * Ideally, we'd discard the original page, but that's hard since other threads of control are
     * using it (for example, if eviction is walking the tree and looking at the page.) Instead,
     * perform a right-split, moving all except the first chunk of the page's WT_REF objects to new
     * pages.
     *
     * Create and initialize a replacement WT_PAGE_INDEX for the original page.
     */
    //分配一个chunk大小的index[]，命名位replace_index
    size = sizeof(WT_PAGE_INDEX) + chunk * sizeof(WT_REF *);
    WT_ERR(__wt_calloc(session, 1, size, &replace_index));
    page_incr += size;
    replace_index->index = (WT_REF **)(replace_index + 1);
    replace_index->entries = chunk;
    //replace_index这个index[]存储需要拆分的page index的前面chunk个
    for (page_refp = pindex->index, i = 0; i < chunk; ++i)
        replace_index->index[i] = *page_refp++;
    //到这里后，page_refp指向需要拆分的page的第chunk个子page，也就是跳过分组后的第1组

    /*
     * Allocate a new WT_PAGE_INDEX and set of WT_REF objects to be inserted into the page's parent,
     * replacing the page's page-index.
     *
     * The first slot of the new WT_PAGE_INDEX is the original page WT_REF. The remainder of the
     * slots are allocated WT_REFs.
     */
    //分配一个children大小的index[]，命名位alloc_index
    //alloc_index[0]指向原来需要拆分的page的第一组子page[0-chunk], alloc_index[1-children]指向其余children-1组子page
    size = sizeof(WT_PAGE_INDEX) + children * sizeof(WT_REF *);//这里分配的是ref指针空间
    WT_ERR(__wt_calloc(session, 1, size, &alloc_index));
    parent_incr += size;
    alloc_index->index = (WT_REF **)(alloc_index + 1);
    alloc_index->entries = children;
    alloc_refp = alloc_index->index;

    //alloc_index这个index[]的第0个ref指向需要拆分的这个page ref
    *alloc_refp++ = page_ref;
    //注意这里从1开始，分配剩余的1到children个ref的真实空间
    for (i = 1; i < children; ++alloc_refp, ++i)
        WT_ERR(__wt_calloc_one(session, alloc_refp));
    parent_incr += children * sizeof(WT_REF);

    /* Allocate child pages, and connect them into the new page index. */
    //到这里后，page_refp指向需要拆分的page的第chunk个子page, 也就是跳过了需要拆分page的第一组子page
    WT_ASSERT(session, page_refp == pindex->index + chunk);
    for (alloc_refp = alloc_index->index + 1, i = 1; i < children; ++i) {
        slots = i == children - 1 ? remain : chunk;
        //printf("yang test.......................................__split_internal....................\r\n");
        WT_ERR(__wt_page_alloc(session, page->type, slots, false, &child));

        /*
         * Initialize the page's child reference; we need a copy of the page's key.
         */
        //这里可以看出新增的ref page的父节点实际上指向需要拆分的page的父节点
        ref = *alloc_refp++;
        ref->home = parent;
        ref->page = child;
        ref->addr = NULL;
        if (page->type == WT_PAGE_ROW_INT) {
            //新的page ref对应的key赋值位需要拆分的page key
            __wt_ref_key(page, *page_refp, &p, &size);
            //设置ref对应ref_ikey为分组拆分点的ref key
            WT_ERR(__wt_row_ikey(session, 0, p, size, ref));
            parent_incr += sizeof(WT_IKEY) + size;
        } else
            ref->ref_recno = (*page_refp)->ref_recno;
        F_SET(ref, WT_REF_FLAG_INTERNAL);
        WT_REF_SET_STATE(ref, WT_REF_MEM);

        /*
         * Initialize the child page. Block eviction in newly created pages and mark them dirty.
         */
        child->pg_intl_parent_ref = ref;
        WT_ERR(__wt_page_modify_init(session, child));
        __wt_page_modify_set(session, child);

        /*
         * The newly allocated child's page index references the same structures as the parent. (We
         * cannot move WT_REF structures, threads may be underneath us right now changing the
         * structure state.) However, if the WT_REF structures reference on-page information, we
         * have to fix that, because the disk image for the page that has a page index entry for the
         * WT_REF is about to be discarded.
         */
        child_pindex = WT_INTL_INDEX_GET_SAFE(child);
        child_incr = 0;
        //也就是把需要拆分的这个internal page的第n组重新挂载到父page
        for (child_refp = child_pindex->index, j = 0; j < slots; ++child_refp, ++page_refp, ++j)
            WT_ERR(__split_ref_move(session, page, page_refp, &page_decr, child_refp, &child_incr));

        __wt_cache_page_inmem_incr(session, child, child_incr);
    }
    WT_ASSERT(session, alloc_refp - alloc_index->index == (ptrdiff_t)alloc_index->entries);
    WT_ASSERT(session, page_refp - pindex->index == (ptrdiff_t)pindex->entries);

    /*
     * Flush our writes and start making real changes to the tree, errors are fatal.
     */
    WT_PUBLISH(complete, WT_ERR_PANIC);

    /* Prepare the WT_REFs for the move. */
    WT_ERR(__split_ref_prepare(session, alloc_index, &locked, true));

    /* Encourage a race */
    __wt_timing_stress(session, WT_TIMING_STRESS_SPLIT_5, NULL);

    /* Split into the parent. */
    //需要拆分的page按照chunk大小分组为children个组，没组对应一个alloc_index(也就是一个internal page), 然后对page的父
    //  page对应index[]数组进行扩容，然后挂载alloc_index数组到父page的index[]上面
    WT_ERR(__split_parent(
      session, page_ref, alloc_index->index, alloc_index->entries, parent_incr, false, false));

    /*
     * Confirm the page's index hasn't moved, then update it, which makes the split visible to
     * threads descending the tree.
     */
    WT_ASSERT(session, WT_INTL_INDEX_GET_SAFE(page) == pindex);
    WT_INTL_INDEX_SET(page, replace_index);
   
    /* Encourage a race */
    __wt_timing_stress(session, WT_TIMING_STRESS_SPLIT_6, NULL);

    /*
     * Get a generation for this split, mark the parent page. This must be after the new index is
     * swapped into place in order to know that no readers with the new generation will look at the
     * old index.
     */
    WT_FULL_BARRIER();
    split_gen = __wt_gen(session, WT_GEN_SPLIT);
    page->pg_intl_split_gen = split_gen;

    /* Finalize the WT_REF move. */
    __split_ref_final(session, split_gen, &locked);

#ifdef HAVE_DIAGNOSTIC
    WT_WITH_PAGE_INDEX(session, __split_verify_intl_key_order(session, parent));
    WT_WITH_PAGE_INDEX(session, __split_verify_intl_key_order(session, page));
#endif

    /* The split is complete and verified, ignore benign errors. */
    complete = WT_ERR_IGNORE;

    /*
     * We don't care about the page-index we allocated, all we needed was the array of WT_REF
     * structures, which has now been split into the parent page.
     */
    __wt_free(session, alloc_index);

    /*
     * We can't free the previous page's index, there may be threads using it. Add to the session's
     * discard list, to be freed once we know no threads can still be using it.
     *
     * This change requires care with error handling, we've already updated the parent page. Even if
     * stashing the old value fails, we don't roll back that change, because threads may already be
     * using the new parent page.
     */
    size = sizeof(WT_PAGE_INDEX) + pindex->entries * sizeof(WT_REF *);
    WT_TRET(__split_safe_free(session, split_gen, false, pindex, size));
    page_decr += size;

    /* Adjust the page's memory footprint. */
    __wt_cache_page_inmem_incr(session, page, page_incr);
    __wt_cache_page_inmem_decr(session, page, page_decr);

    __wt_gen_next(session, WT_GEN_SPLIT, NULL);
err:
    __split_ref_final(session, 0, &locked);

    switch (complete) {
    case WT_ERR_RETURN:
        /*
         * The replace-index variable is the internal page being split's new page index, referencing
         * the first chunk of WT_REFs that aren't being moved to other pages. Those WT_REFs survive
         * the failure, they're referenced from the page's current index. Simply free that memory,
         * but nothing it references.
         */
        __wt_free(session, replace_index);

        /*
         * The alloc-index variable is the array of new WT_REF entries intended to be inserted into
         * the page being split's parent.
         *
         * Except for the first slot (the original page's WT_REF), it's an array of newly allocated
         * combined WT_PAGE_INDEX and WT_REF structures, each of which references a newly allocated
         * (and modified) child page, each of which references an index of WT_REFs from the page
         * being split. Free everything except for slot 1 and the WT_REFs in the child page indexes.
         *
         * First, skip slot 1. Second, we want to free all of the child pages referenced from the
         * alloc-index array, but we can't just call the usual discard function because the WT_REFs
         * referenced by the child pages remain referenced by the original page, after error. For
         * each entry, free the child page's page index (so the underlying page-free function will
         * ignore it), then call the general-purpose discard function.
         */
        if (alloc_index == NULL)
            break;
        alloc_refp = alloc_index->index;
        *alloc_refp++ = NULL;
        for (i = 1; i < children; ++alloc_refp, ++i) {
            ref = *alloc_refp;
            if (ref == NULL || ref->page == NULL)
                continue;

            child = ref->page;
            child_pindex = WT_INTL_INDEX_GET_SAFE(child);
            __wt_free(session, child_pindex);
            WT_INTL_INDEX_SET(child, NULL);
        }
        __wt_free_ref_index(session, page, alloc_index, true);
        break;
    case WT_ERR_IGNORE:
        if (ret != WT_PANIC) {
            if (ret != 0)
                __wt_err(session, ret, "ignoring not-fatal error during internal page split");
            ret = 0;
            break;
        }
    /* FALLTHROUGH */
    case WT_ERR_PANIC:
        ret = __wt_panic(session, ret, "fatal error during internal page split");
        break;
    }
    return (ret);
}

/*
 * __split_internal_lock --
 *     Lock an internal page.
 */
//获取page对应的page_lock锁，也就是锁住ref->home这个父internal page, 并返回parent page
static int
__split_internal_lock(WT_SESSION_IMPL *session, WT_REF *ref, bool trylock, WT_PAGE **parentp)
{
    WT_PAGE *parent;

    *parentp = NULL;

    /*
     * A checkpoint reconciling this parent page can deadlock with our split. We have an exclusive
     * page lock on the child before we acquire the page's reconciliation lock, and reconciliation
     * acquires the page's reconciliation lock before it encounters the child's exclusive lock
     * (which causes reconciliation to loop until the exclusive lock is resolved). If we want to
     * split the parent, give up to avoid that deadlock.
     */
    //
    if (!trylock && __wt_btree_syncing_by_other_session(session))
        return (__wt_set_return(session, EBUSY));

    /*
     * Get a page-level lock on the parent to single-thread splits into the page because we need to
     * single-thread sizing/growing the page index. It's OK to queue up multiple splits as the child
     * pages split, but the actual split into the parent has to be serialized. Note we allocate
     * memory inside of the lock and may want to invest effort in making the locked period shorter.
     *
     * We use the reconciliation lock here because not only do we have to single-thread the split,
     * we have to lock out reconciliation of the parent because reconciliation of the parent can't
     * deal with finding a split child during internal page traversal. Basically, there's no reason
     * to use a different lock if we have to block reconciliation anyway.
     */
    for (;;) {
        parent = ref->home;

        /* Encourage races. */
        //随机延迟，默认配置不会sleep
        __wt_timing_stress(session, WT_TIMING_STRESS_SPLIT_7, NULL);

        /* Page locks live in the modify structure. */
        WT_RET(__wt_page_modify_init(session, parent));

        if (trylock) //获取page对应的page_lock锁
            WT_RET(WT_PAGE_TRYLOCK(session, parent));
        else {
        //    printf("yang test ...__split_internal_lock...........WT_PAGE_LOCK.....page:%p %s\r\n", parent, __wt_page_type_string(parent->type));
        
            WT_PAGE_LOCK(session, parent);
        }
        if (parent == ref->home)
            break;//这里break，也就是没有unlock，在外层unlock

        WT_PAGE_UNLOCK(session, parent);
    }

    /*
     * This child has exclusive access to split its parent and the child's existence prevents the
     * parent from being evicted. However, once we update the parent's index, it may no longer refer
     * to the child, and could conceivably be evicted. If the parent page is dirty, our page lock
     * prevents eviction because reconciliation is blocked. However, if the page were clean, it
     * could be evicted without encountering our page lock. That isn't possible because you cannot
     * move a child page and still leave the parent page clean.
     */

    *parentp = parent;
    return (0);
}

/*
 * __split_internal_unlock --
 *     Unlock the parent page.
 */
static void
__split_internal_unlock(WT_SESSION_IMPL *session, WT_PAGE *parent)
{
    WT_PAGE_UNLOCK(session, parent);
}

/*
 * __split_internal_should_split --
 *     Return if we should split an internal page.
internal page(包括root)的split条件，以下满足任何一个即可
条件1: 子page超过10000个
条件2: 子page超过100个并且page->memory_footprint > btree->maxmempage
 */
static bool
__split_internal_should_split(WT_SESSION_IMPL *session, WT_REF *ref)
{
    WT_BTREE *btree;
    WT_PAGE *page;
    WT_PAGE_INDEX *pindex;

    btree = S2BT(session);
    page = ref->page;

    /*
     * Our caller is holding the parent page locked to single-thread splits, which means we can
     * safely look at the page's index without setting a split generation.
     */
    pindex = WT_INTL_INDEX_GET_SAFE(page);

   //yang test .....__split_internal_should_split........entries:185, [32905, 32768]
   // printf("yang test .....__split_internal_should_split........entries:%u, [%d, %d]\r\n",
   //     pindex->entries, (int)page->memory_footprint, (int)btree->maxmempage);
    /* Sanity check for a reasonable number of on-page keys. */
    if (pindex->entries < WT_INTERNAL_SPLIT_MIN_KEYS)
        return (false);

    /*
     * Deepen the tree if the page's memory footprint is larger than the maximum size for a page in
     * memory (presumably putting eviction pressure on the cache).
     */
    if (page->memory_footprint > btree->maxmempage)
        return (true);

    /*
     * Check if the page has enough keys to make it worth splitting. If the number of keys is
     * allowed to grow too large, the cost of splitting into parent pages can become large enough to
     * result in slow operations.
     */
    if (pindex->entries > btree->split_deepen_min_child)
        return (true);

    return (false);
}

/*
 * __split_parent_climb --
 *     Check if we should split up the tree.
 */
static int
__split_parent_climb(WT_SESSION_IMPL *session, 
    //对应的是需要split的leaf page的parent
    WT_PAGE *page)
{
    WT_DECL_RET;
    WT_PAGE *parent;
    WT_REF *ref;

    /*
     * Disallow internal splits during the final pass of a checkpoint. Most splits are already
     * disallowed during checkpoints, but an important exception is insert splits. The danger is an
     * insert split creates a new chunk of the namespace, and then the internal split will move it
     * to a different part of the tree where it will be written; in other words, in one part of the
     * tree we'll skip the newly created insert split chunk, but we'll write it upon finding it in a
     * different part of the tree.
     *
     * Historically we allowed checkpoint itself to trigger an internal split here. That wasn't
     * correct, since if that split climbs the tree above the immediate parent the checkpoint walk
     * will potentially miss some internal pages. This is wrong as checkpoint needs to reconcile the
     * entire internal tree structure. Non checkpoint cursor traversal doesn't care the internal
     * tree structure as they just want to get the next leaf page correctly. Therefore, it is OK to
     * split concurrently to cursor operations.
     */
    if (WT_BTREE_SYNCING(S2BT(session))) {
        __split_internal_unlock(session, page);
        return (0);
    }

    /*
     * Page splits trickle up the tree, that is, as leaf pages grow large enough and are evicted,
     * they'll split into their parent. And, as that parent page grows large enough and is evicted,
     * it splits into its parent and so on. When the page split wave reaches the root, the tree will
     * permanently deepen as multiple root pages are written.
     *
     * However, this only helps if internal pages are evicted (and we resist evicting internal pages
     * for obvious reasons), or if the tree were to be closed and re-opened from a disk image, which
     * may be a rare event.
     *
     * To avoid internal pages becoming too large absent eviction, check parent pages each time
     * pages are split into them. If the page is big enough, either split the page into its parent
     * or, in the case of the root, deepen the tree.
     *
     * Split up the tree.
     */
    for (;;) {
        parent = NULL;
        //ref指向该page自己所属的ref
        ref = page->pg_intl_parent_ref;

        {
            WT_PAGE_INDEX *pindex;

            pindex = WT_INTL_INDEX_GET_SAFE(page);

      __wt_verbose(session, WT_VERB_SPLIT,
          "yang test ......__split_parent_climb............ref:%p, entrys:%d, is root:%d\r\n",
                ref, (int)pindex->entries, __wt_ref_is_root(ref));
        }
        /* If we don't need to split the page, we're done. */
        /*
            internal page(包括root)的split条件，以下满足任何一个即可
            条件1: 子page超过10000个
            条件2: 子page超过100个并且page->memory_footprint > btree->maxmempage
         */
        if (!__split_internal_should_split(session, ref))
            break;

        /*
         * If we've reached the root page, there are no subsequent pages to review, deepen the tree
         * and quit.
         */
        if (__wt_ref_is_root(ref)) {
            ret = __split_root(session, page);
            break;
        }

        /*
         * Lock the parent and split into it, then swap the parent/page locks, lock-coupling up the
         * tree.
         */
        WT_ERR(__split_internal_lock(session, ref, true, &parent));
        ret = __split_internal(session, parent, page);
        __split_internal_unlock(session, page);

        page = parent;
        parent = NULL;
        WT_ERR(ret);
    }

err:
    if (parent != NULL)
        __split_internal_unlock(session, parent);
    __split_internal_unlock(session, page);

    /* A page may have been busy, in which case return without error. */
    switch (ret) {
    case 0:
    case WT_PANIC:
        break;
    case EBUSY:
        ret = 0;
        break;
    default:
        __wt_err(session, ret, "ignoring not-fatal error during parent page split");
        ret = 0;
        break;
    }
    return (ret);
}

/*
 * __split_multi_inmem --
 *     Instantiate a page from a disk image.
 //reconcile拆分后每个multi都需要调用一次，用于隐射内存page和磁盘chunk数据

 //新建一个page， 实现一个multi与这个page的映射，主要是实现磁盘KV数据和page->pg_row的隐射，以及page相关赋值
 */ //__wt_multi_to_ref->__split_multi_inmem
static int
__split_multi_inmem(WT_SESSION_IMPL *session, WT_PAGE *orig, WT_MULTI *multi, WT_REF *ref)
{
    WT_CURSOR_BTREE cbt;
    WT_DECL_ITEM(key);
    WT_DECL_RET;
    WT_PAGE *page;
    WT_PAGE_MODIFY *mod;
    WT_SAVE_UPD *supd;
    WT_UPDATE *prev_onpage, *upd, *tmp;
    uint64_t recno;
    uint32_t i, slot;
    bool prepare;

    /*
     * This code re-creates an in-memory page from a disk image, and adds references to any
     * unresolved update chains to the new page. We get here either because an update could not be
     * written when evicting a page, or eviction chose to keep a page in memory.
     *
     * Reconciliation won't create a disk image with entries the running database no longer cares
     * about (at least, not based on the current tests we're performing), ignore the validity
     * window.
     *
     * Steal the disk image and link the page into the passed-in WT_REF to simplify error handling:
     * our caller will not discard the disk image when discarding the original page, and our caller
     * will discard the allocated page on error, when discarding the allocated WT_REF.
     */
    //为磁盘上面的一个chunk ext分配一个page，并记录该page在磁盘上面的KV总数
    //printf("yang test........__split_multi_inmem.......111.............page->dsk:%p\r\n", multi->disk_image);
    WT_RET(__wt_page_inmem(session, ref, multi->disk_image, WT_PAGE_DISK_ALLOC, &page, &prepare));
    multi->disk_image = NULL; //这里加打印可以看出multi->disk_image地址和page->dsk地址相同
    //printf("yang test........__split_multi_inmem.......222.............page->dsk:%p\r\n", page->dsk);

    /*
     * In-memory databases restore non-obsolete updates directly in this function, don't call the
     * underlying page functions to do it.
     */
    if (prepare && !F_ISSET(S2C(session), WT_CONN_IN_MEMORY))
        WT_RET(__wt_page_inmem_prepare(session, ref));

    /*
     * Put the re-instantiated page in the same LRU queue location as the original page, unless this
     * was a forced eviction, in which case we leave the new page with the read generation unset.
     * Eviction will set the read generation next time it visits this page.
     */
    if (!WT_READGEN_EVICT_SOON(orig->read_gen))
        page->read_gen = orig->read_gen;

    /*
     * If there are no updates to apply to the page, we're done. Otherwise, there are updates we
     * need to restore.
     */
    //该page在磁盘上的update相关多版本信息
    if (multi->supd_entries == 0)
        return (0);
    WT_ASSERT(session, multi->supd_restore);

    if (orig->type == WT_PAGE_ROW_LEAF)
        WT_RET(__wt_scr_alloc(session, 0, &key));

    __wt_btcur_init(session, &cbt);
    __wt_btcur_open(&cbt);

    /* Re-create each modification we couldn't write. */
    for (i = 0, supd = multi->supd; i < multi->supd_entries; ++i, ++supd) {
        /* Ignore update chains that don't need to be restored. */
        if (!supd->restore)
            continue;

        if (supd->ins == NULL) {
            /* Note: supd->ins is never null for column-store. */
            slot = WT_ROW_SLOT(orig, supd->rip);
            upd = orig->modify->mod_row_update[slot];
        } else
            upd = supd->ins->upd;

        /* We shouldn't restore an empty update chain. */
        WT_ASSERT(session, upd != NULL);

        /*
         * Truncate the onpage value and the older versions moved to the history store. We can't
         * truncate the updates for an in memory database as it doesn't support the history store.
         * We can't free the truncated updates here as we may still fail. If we fail, we will append
         * them back to their original update chains. Truncate before we restore them to ensure the
         * size of the page is correct.
         */
        if (supd->onpage_upd != NULL && !F_ISSET(S2C(session), WT_CONN_IN_MEMORY)) {
            /*
             * If there is an on-page tombstone we need to remove it as well while performing update
             * restore eviction.
             */
            tmp = supd->onpage_tombstone != NULL ? supd->onpage_tombstone : supd->onpage_upd;

            /*
             * We have decided to restore this update chain so it must have newer updates than the
             * onpage value on it.
             */
            WT_ASSERT(session, upd != tmp);
            WT_ASSERT(session, F_ISSET(tmp, WT_UPDATE_DS));

            /*
             * Move the pointer to the position before the onpage value and truncate all the updates
             * starting from the onpage value.
             */
            for (prev_onpage = upd; prev_onpage->next != NULL && prev_onpage->next != tmp;
                 prev_onpage = prev_onpage->next)
                ;
            WT_ASSERT(session, prev_onpage->next == tmp);
#ifdef HAVE_DIAGNOSTIC
            /*
             * During update restore eviction we remove anything older than the on-page update,
             * including the on-page update. However it is possible a tombstone is also written as
             * the stop time of the on-page value. To handle this we also need to remove the
             * tombstone from the update chain.
             *
             * This assertion checks that there aren't any unexpected updates between that tombstone
             * and the subsequent value which both make up the on-page value.
             */
            for (; tmp != NULL && tmp != supd->onpage_upd; tmp = tmp->next)
                WT_ASSERT(session, tmp == supd->onpage_tombstone || tmp->txnid == WT_TXN_ABORTED);
#endif
            prev_onpage->next = NULL;
        }

        switch (orig->type) {
        case WT_PAGE_COL_FIX:
        case WT_PAGE_COL_VAR:
            /* Build a key. */
            recno = WT_INSERT_RECNO(supd->ins);

            /* Search the page. */
            WT_ERR(__wt_col_search(&cbt, recno, ref, true, NULL));

            /* Apply the modification. */
#ifdef HAVE_DIAGNOSTIC
            WT_ERR(__wt_col_modify(&cbt, recno, NULL, upd, WT_UPDATE_INVALID, true, true));
#else
            WT_ERR(__wt_col_modify(&cbt, recno, NULL, upd, WT_UPDATE_INVALID, true));
#endif
            break;
        case WT_PAGE_ROW_LEAF:
            /* Build a key. */
            if (supd->ins == NULL)
                WT_ERR(__wt_row_leaf_key(session, orig, supd->rip, key, false));
            else {
                key->data = WT_INSERT_KEY(supd->ins);
                key->size = WT_INSERT_KEY_SIZE(supd->ins);
            }

            /* Search the page. */
            WT_ERR(__wt_row_search(&cbt, key, true, ref, true, NULL));

            /* Apply the modification. */
#ifdef HAVE_DIAGNOSTIC
            WT_ERR(__wt_row_modify(&cbt, key, NULL, upd, WT_UPDATE_INVALID, true, true));
#else
            WT_ERR(__wt_row_modify(&cbt, key, NULL, upd, WT_UPDATE_INVALID, true));
#endif
            break;
        default:
            WT_ERR(__wt_illegal_value(session, orig->type));
        }
    }

    /*
     * When modifying the page we set the first dirty transaction to the last transaction currently
     * running. However, the updates we made might be older than that. Set the first dirty
     * transaction to an impossibly old value so this page is never skipped in a checkpoint.
     */
    mod = page->modify;
    mod->first_dirty_txn = WT_TXN_FIRST;

    /*
     * Restore the previous page's modify state to avoid repeatedly attempting eviction on the same
     * page.
     */
    mod->last_evict_pass_gen = orig->modify->last_evict_pass_gen;
    mod->last_eviction_id = orig->modify->last_eviction_id;
    mod->last_eviction_timestamp = orig->modify->last_eviction_timestamp;
    mod->rec_max_txn = orig->modify->rec_max_txn;
    mod->rec_max_timestamp = orig->modify->rec_max_timestamp;

    /* Add the update/restore flag to any previous state. */
    mod->restore_state = orig->modify->restore_state;
    FLD_SET(mod->restore_state, WT_PAGE_RS_RESTORED);

err:
    /* Free any resources that may have been cached in the cursor. */
    WT_TRET(__wt_btcur_close(&cbt, true));

    __wt_scr_free(session, &key);
    return (ret);
}

/*
 * __split_multi_inmem_final --
 *     Discard moved update lists from the original page and free the updates written to the data
 *     store and the history store.
 */
static void
__split_multi_inmem_final(WT_SESSION_IMPL *session, WT_PAGE *orig, WT_MULTI *multi)
{
    WT_SAVE_UPD *supd;
    WT_UPDATE **tmp;
    uint32_t i, slot;

    /* If we have saved updates, we must have decided to restore them to the new page. */
    WT_ASSERT(session, multi->supd_entries == 0 || multi->supd_restore);

    /*
     * We successfully created new in-memory pages. For error-handling reasons, we've left the
     * update chains referenced by both the original and new pages. We're ready to discard the
     * original page, terminate the original page's reference to any update list we moved and free
     * the updates written to the data store and the history store.
     */
    for (i = 0, supd = multi->supd; i < multi->supd_entries; ++i, ++supd) {
        /* We have finished restoration. Discard the update chains that aren't restored. */
        if (!supd->restore)
            continue;

        if (supd->ins == NULL) {
            /* Note: supd->ins is never null for column-store. */
            slot = WT_ROW_SLOT(orig, supd->rip);
            orig->modify->mod_row_update[slot] = NULL;
        } else
            supd->ins->upd = NULL;

        /*
         * Free the updates written to the data store and the history store when there exists an
         * onpage value. It is possible that there can be an onpage tombstone without an onpage
         * value when the tombstone is globally visible. Do not free them here as it is possible
         * that the globally visible tombstone is already freed as part of update obsolete check.
         */
        if (supd->onpage_upd != NULL && !F_ISSET(S2C(session), WT_CONN_IN_MEMORY)) {
            tmp = supd->onpage_tombstone != NULL ? &supd->onpage_tombstone : &supd->onpage_upd;
            __wt_free_update_list(session, tmp);
            supd->onpage_tombstone = supd->onpage_upd = NULL;
        }
    }
}

/*
 * __split_multi_inmem_fail --
 *     Discard allocated pages after failure and append the onpage values back to the original
 *     update chains.
 */
static void
__split_multi_inmem_fail(WT_SESSION_IMPL *session, WT_PAGE *orig, WT_MULTI *multi, WT_REF *ref)
{
    WT_SAVE_UPD *supd;
    WT_UPDATE *upd, *tmp;
    uint32_t i, slot;

    if (!F_ISSET(S2C(session), WT_CONN_IN_MEMORY))
        /* Append the onpage values back to the original update chains. */
        for (i = 0, supd = multi->supd; i < multi->supd_entries; ++i, ++supd) {
            /*
             * We don't need to do anything for update chains that are not restored, or restored
             * without an onpage value.
             */
            if (!supd->restore || supd->onpage_upd == NULL)
                continue;

            if (supd->ins == NULL) {
                /* Note: supd->ins is never null for column-store. */
                slot = WT_ROW_SLOT(orig, supd->rip);
                upd = orig->modify->mod_row_update[slot];
            } else
                upd = supd->ins->upd;

            WT_ASSERT(session, upd != NULL);
            tmp = supd->onpage_tombstone != NULL ? supd->onpage_tombstone : supd->onpage_upd;
            for (; upd->next != NULL && upd->next != tmp; upd = upd->next)
                ;
            if (upd->next == NULL)
                upd->next = tmp;
        }

    /*
     * We failed creating new in-memory pages. For error-handling reasons, we've left the update
     * chains referenced by both the original and new pages. Discard the newly allocated WT_REF
     * structures and their pages (setting a flag so the discard code doesn't discard the updates on
     * the page).
     *
     * Our callers allocate WT_REF arrays, then individual WT_REFs, check for uninitialized
     * information.
     */
    if (ref != NULL) {
        if (ref->page != NULL)
            F_SET_ATOMIC_16(ref->page, WT_PAGE_UPDATE_IGNORE);
        __wt_free_ref(session, ref, orig->type, true);
    }
}

/*
 * __wt_multi_to_ref --
 *     Move a multi-block entry into a WT_REF structure.
 */
//__wt_evict->__evict_page_dirty_update->__wt_split_multi->__split_multi_lock->__split_multi->__wt_multi_to_ref
//新建一个ref， 实现一个multi与这个ref page的映射，主要是实现磁盘KV数据和page->pg_row的隐射，以及page相关赋值
int
__wt_multi_to_ref(WT_SESSION_IMPL *session, WT_PAGE *page, WT_MULTI *multi, WT_REF **refp,
  size_t *incrp, bool closing)
{
    WT_ADDR *addr;
    WT_IKEY *ikey;
    WT_REF *ref;

    /* There can be an address or a disk image or both. */
    WT_ASSERT(session, multi->addr.addr != NULL || multi->disk_image != NULL);

    /* If closing the file, there better be an address. */
    WT_ASSERT(session, !closing || multi->addr.addr != NULL);

    /* If closing the file, there better not be any saved updates. */
    WT_ASSERT(session, !closing || multi->supd == NULL);

    /* If we don't have a disk image, we can't restore the saved updates. */
    WT_ASSERT(
      session, multi->disk_image != NULL || (multi->supd_entries == 0 && !multi->supd_restore));

    /* Verify any disk image we have. */
    WT_ASSERT(session,
      multi->disk_image == NULL ||
        //disk_image不为NULL，则说明持久化的page内存中有一份一模一样的
        __wt_verify_dsk_image(session, "[page instantiate]", multi->disk_image, 0, &multi->addr,
          WT_VRFY_DISK_EMPTY_PAGE_OK) == 0);

    /* Allocate an underlying WT_REF. */
    WT_RET(__wt_calloc_one(session, refp));
    ref = *refp;
    if (incrp)
        *incrp += sizeof(WT_REF);

    /*
     * Set the WT_REF key before (optionally) building the page, underlying column-store functions
     * need the page's key space to search it.
     */
    switch (page->type) {
    case WT_PAGE_ROW_INT:
    case WT_PAGE_ROW_LEAF:
        ikey = multi->key.ikey;
        WT_RET(__wt_row_ikey(session, 0, WT_IKEY_DATA(ikey), ikey->size, ref));
        if (incrp)
            *incrp += sizeof(WT_IKEY) + ikey->size;
        break;
    default:
        ref->ref_recno = multi->key.recno;
        break;
    }

    switch (page->type) {
    case WT_PAGE_COL_INT:
    case WT_PAGE_ROW_INT:
        F_SET(ref, WT_REF_FLAG_INTERNAL);
        break;
    default:
        F_SET(ref, WT_REF_FLAG_LEAF);
        break;
    }

    /*
     * If there's an address, the page was written, set it.
     *
     * Copy the address: we could simply take the buffer, but that would complicate error handling,
     * freeing the reference array would have to avoid freeing the memory, and it's not worth the
     * confusion.
     */
    //创建新ref，新ref->addr指向磁盘上的chunk数据
    //reconcile后page数据已经持久化，这时候page不在内存中，记录已持久化的page元数据addr信息，这时候ref->page都为NULL
    if (multi->addr.addr != NULL) {
        WT_RET(__wt_calloc_one(session, &addr));
        ref->addr = addr;
        WT_TIME_AGGREGATE_COPY(&addr->ta, &multi->addr.ta);
        //拷贝page对应的磁盘元数据信息,multi->addr保存chunk->image写入磁盘时候的元数据信息(objectid offset size  checksum)
        WT_RET(__wt_memdup(session, multi->addr.addr, multi->addr.size, &addr->addr));
        addr->size = multi->addr.size;
        addr->type = multi->addr.type;

        //表示该page对应数据在磁盘中，注意在下面的__split_multi_inmem后会置为WT_REF_MEM
        WT_REF_SET_STATE(ref, WT_REF_DISK);

        //这里打印出的page为NULL
        //printf("yang test ......111...........__wt_multi_to_ref..................ref:%p, page:%p stat:%u\r\n",
        //    ref, ref->page, ref->state);
    }

    /*
     * If we have a disk image and we're not closing the file, re-instantiate the page.
     *
     * Discard any page image we don't use.
     如果内存比较充足，创建page空间，这时候也会拷贝一份磁盘数据到内存page->pg_row[]中
     */

    //如果内存很紧张，则reconcile的时候会持久化到磁盘，这时候disk_image为NULL，split拆分后的ref->page为NULL，这时候page生成依靠用户线程
    //  在__page_read中读取磁盘数据到page中完成ref->page内存空间生成
    if (multi->disk_image != NULL && !closing) {//yang add todo xxxx   如果是closin __rec_split_write中是否有必要分配内存拷贝chunk数据到disk_image
         //新建一个page， 实现一个multi与这个page的映射，主要是实现磁盘KV数据和page->pg_row的隐射，以及page相关赋值
        WT_RET(__split_multi_inmem(session, page, multi, ref));

        //当reconcile evict拆分page为多个，并且写入磁盘ext，这时候page状态进入WT_REF_DISK, 当unpack解包获取到该ext的所有K或者V在相比ext头部
        //偏移量后，重新置为WT_REF_MEM状态，表示我们已经获取到ext中包含的所有K和V磁盘元数据地址存储到了内存pg_row中
        WT_REF_SET_STATE(ref, WT_REF_MEM);
    }

  //  printf("yang test ...222..............__wt_multi_to_ref..................ref:%p, page:%p stat:%u\r\n",
   //         ref, ref->page, ref->state);
    //在__split_multi_inmem->__wt_page_inmem中赋值给了page->dsk, 并在置为disk_image=NULL， 所以指向的内存空间实际上被page->dsk继承了
    __wt_free(session, multi->disk_image);

    return (0);
}

/*
 * __split_insert --
 *     Split a page's last insert list entries into a separate page.
 官方split文档说明: https://github.com/wiredtiger/wiredtiger/wiki/In-memory-Page-Splits
 把一个ref page拆分为2个page，拆分点为page最大的key，假设这个page最大的为maxkey，则page拆分后
 的两个page分别为page1=[XXXX, maxkey的前一个K], page2=[maxkey]
 */
//__split_insert_lock
static int
__split_insert(WT_SESSION_IMPL *session, WT_REF *ref)
{
    WT_DECL_RET;
    WT_INSERT *ins, **insp, *moved_ins, *prev_ins;
    WT_INSERT_HEAD *ins_head, *tmp_ins_head;
    WT_PAGE *page, *right;
    WT_REF *child, *split_ref[2] = {NULL, NULL};
    size_t key_size, page_decr, parent_incr, right_incr;
    uint8_t type;
    int i;
    void *key;
    #ifdef HAVE_DIAGNOSTIC
    WT_DBG *ds, _ds;
    #endif

    WT_STAT_CONN_DATA_INCR(session, cache_inmem_split);

    page = ref->page;
    right = NULL;
    page_decr = parent_incr = right_incr = 0;
    type = page->type;

    /*
     * Assert splitting makes sense; specifically assert the page is dirty, we depend on that,
     * otherwise the page might be evicted based on its last reconciliation which no longer matches
     * reality after the split.
     */
    //page是否可以拆分
    WT_ASSERT(session, __wt_leaf_page_can_split(session, page));
    //page中有脏数据
    WT_ASSERT(session, __wt_page_is_modified(page));

    //原来的page设置该标识
    F_SET_ATOMIC_16(page, WT_PAGE_SPLIT_INSERT); /* Only split in-memory once. */

    /* Find the last item on the page. */
    //也就是获取跳表中最上层的链表指针，这样可以快速获取该page的最后一个节点
    if (type == WT_PAGE_ROW_LEAF)
        ins_head = page->entries == 0 ? WT_ROW_INSERT_SMALLEST(page) :
                                        WT_ROW_INSERT_SLOT(page, page->entries - 1);
    else
        ins_head = WT_COL_APPEND(page);

    //获取跳跃表中的最后一个成员KV, 也就是该page最大得K
    //下面的新page，也就是righ page中拆分后只存储这一个KEY
    moved_ins = WT_SKIP_LAST(ins_head);

    /*
     * The first page in the split is almost identical to the current page, but we have to create a
     * replacement WT_REF, the original WT_REF will be set to split status and eventually freed.
     */
    //创建一个新的ref, 并赋值为原始ref相关值，ref[0]指向原来的page, ref[1]指向拆分后的创建的新page
    WT_ERR(__wt_calloc_one(session, &split_ref[0]));
    parent_incr += sizeof(WT_REF);
    child = split_ref[0];
    //split_ref[0]指向原来的page
    child->page = ref->page;
    child->home = ref->home;
    child->pindex_hint = ref->pindex_hint;
    F_SET(child, WT_REF_FLAG_LEAF);
    child->state = WT_REF_MEM; /* Visible as soon as the split completes. */
    child->addr = ref->addr;
    if (type == WT_PAGE_ROW_LEAF) {
        //获取一个page所属ref的key值和长度
        __wt_ref_key(ref->home, ref, &key, &key_size);
        WT_ERR(__wt_row_ikey(session, 0, key, key_size, child));
        parent_incr += sizeof(WT_IKEY) + key_size;
    } else
        child->ref_recno = ref->ref_recno;

    /*
     * The address has moved to the replacement WT_REF. Make sure it isn't freed when the original
     * ref is discarded.
     */
    ref->addr = NULL;

    /* The second page in the split is a new WT_REF/page pair. */
    //获取一个新page通过right返回
    WT_ERR(__wt_page_alloc(session, type, 0, false, &right));

    /*
     * The new page is dirty by definition, plus column-store splits update the page-modify
     * structure, so create it now.
     */
    WT_ERR(__wt_page_modify_init(session, right));
     //标记该right这个page及其所在的btree有修改操作
    __wt_page_modify_set(session, right);

    if (type == WT_PAGE_ROW_LEAF) {
        WT_ERR(__wt_calloc_one(session, &right->modify->mod_row_insert));
        WT_ERR(__wt_calloc_one(session, &right->modify->mod_row_insert[0]));
    } else {
        WT_ERR(__wt_calloc_one(session, &right->modify->mod_col_append));
        WT_ERR(__wt_calloc_one(session, &right->modify->mod_col_append[0]));
    }
    right_incr += sizeof(WT_INSERT_HEAD);
    right_incr += sizeof(WT_INSERT_HEAD *);

    //创建ref2,ref[0]指向原来的page, ref[1]指向拆分后的创建的新page
    WT_ERR(__wt_calloc_one(session, &split_ref[1]));
    parent_incr += sizeof(WT_REF);
    child = split_ref[1];
    //split_ref[1]指向新的page
    child->page = right;
    F_SET(child, WT_REF_FLAG_LEAF);
    child->state = WT_REF_MEM; /* Visible as soon as the split completes. */
    if (type == WT_PAGE_ROW_LEAF) {
        WT_ERR(__wt_row_ikey(
          session, 0, WT_INSERT_KEY(moved_ins), WT_INSERT_KEY_SIZE(moved_ins), child));
        parent_incr += sizeof(WT_IKEY) + WT_INSERT_KEY_SIZE(moved_ins);
    } else
        child->ref_recno = WT_INSERT_RECNO(moved_ins);

#ifdef HAVE_DIAGNOSTIC
    ds = &_ds;
    WT_ERR(__debug_config(session, ds, NULL));
    WT_RET(__debug_item_key(ds, "\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\nyang test __split_insert:", WT_INSERT_KEY(moved_ins), WT_INSERT_KEY_SIZE(moved_ins)));
#endif

    /*
     * Allocation operations completed, we're going to split.
     *
     * Record the fixed-length column-store split page record, used in reconciliation.
     */
    if (type == WT_PAGE_COL_FIX) {
        WT_ASSERT(session, page->modify->mod_col_split_recno == WT_RECNO_OOB);
        page->modify->mod_col_split_recno = child->ref_recno;
    }

    /*
     * Calculate how much memory we're moving: figure out how deep the skip list stack is for the
     * element we are moving, and the memory used by the item's list of updates.
     */
    //也就是确认跳跃表中最后一个moved_ins成员对应的辅助WT_INSERT指针数组有多少个，参考https://www.jb51.net/article/199510.htm
    for (i = 0; i < WT_SKIP_MAXDEPTH && ins_head->tail[i] == moved_ins; ++i)
        ;
    //计数从ref[0]对应原始page移动到ref[1]对应拆分后新增的page的内存
    WT_MEM_TRANSFER(page_decr, right_incr, sizeof(WT_INSERT) + (size_t)i * sizeof(WT_INSERT *));
    if (type == WT_PAGE_ROW_LEAF)
        WT_MEM_TRANSFER(page_decr, right_incr, WT_INSERT_KEY_SIZE(moved_ins));
    WT_MEM_TRANSFER(page_decr, right_incr, __wt_update_list_memsize(moved_ins->upd));

    /*
     * Move the last insert list item from the original page to the new page.
     *
     * First, update the item to the new child page. (Just append the entry for simplicity, the
     * previous skip list pointers originally allocated can be ignored.)
     */
    //需要删除的moved_ins节点添加到第二个page
    tmp_ins_head = type == WT_PAGE_ROW_LEAF ? right->modify->mod_row_insert[0] :
                                              right->modify->mod_col_append[0];
    tmp_ins_head->head[0] = tmp_ins_head->tail[0] = moved_ins;

    /*
     * Remove the entry from the orig page (i.e truncate the skip list).
     * Following is an example skip list that might help.
     *
     *               __
     *              |c3|
     *               |
     *   __		 __    __
     *  |a2|--------|c2|--|d2|
     *   |		 |	|
     *   __		 __    __	   __
     *  |a1|--------|c1|--|d1|--------|f1|
     *   |		 |	|	   |
     *   __    __    __    __    __    __
     *  |a0|--|b0|--|c0|--|d0|--|e0|--|f0|
     *
     *   From the above picture.
     *   The head array will be: a0, a1, a2, c3, NULL
     *   The tail array will be: f0, f1, d2, c3, NULL
     *   We are looking for: e1, d2, NULL
     *   If there were no f1, we'd be looking for: e0, NULL
     *   If there were an f2, we'd be looking for: e0, d1, d2, NULL
     *
     *   The algorithm does:
     *   1) Start at the top of the head list.
     *   2) Step down until we find a level that contains more than one
     *      element.
     *   3) Step across until we reach the tail of the level.
     *   4) If the tail is the item being moved, remove it.
     *   5) Drop down a level, and go to step 3 until at level 0.
     */
    //从原page对应跳跃表中摘除moved_ins节点, moved_ins是跳跃表中最大的一个K，因此只需要摘除改跳跃表每层指向该节点的指针即可
    prev_ins = NULL; /* -Wconditional-uninitialized */
    for (i = WT_SKIP_MAXDEPTH - 1, insp = &ins_head->head[i]; i >= 0; i--, insp--) {
        /* Level empty, or a single element. */
        //跳跃表中没有elem，或者只有一个elem
        if (ins_head->head[i] == NULL || ins_head->head[i] == ins_head->tail[i]) {
            /* Remove if it is the element being moved. */
            //把moved_ins从原来的ins_head跳跃表中移除
            if (ins_head->head[i] == moved_ins)
                ins_head->head[i] = ins_head->tail[i] = NULL;
            continue;
        }

        //遍历地i层，直到改层末尾
        for (ins = *insp; ins != ins_head->tail[i]; ins = ins->next[i])
            //记录下这一层指向的最后一个elem的前一个elem, 间接说明这一层至少2个elem
            prev_ins = ins;

        /*
         * Update the stack head so that we step down as far to the right as possible. We know that
         * prev_ins is valid since levels must contain at least two items to be here.
         */
        insp = &prev_ins->next[i];
        //如果该层的最后一个指针指向了该成员，则让next指向null， 同时更改该层的tail到上一个节点
        if (ins == moved_ins) {
            /* Remove the item being moved. */
            WT_ASSERT(session, ins_head->head[i] != moved_ins);
            WT_ASSERT(session, prev_ins->next[i] == moved_ins);
            *insp = NULL;
            ins_head->tail[i] = prev_ins;
        }
    }

#ifdef HAVE_DIAGNOSTIC
    /*
     * Verify the moved insert item appears nowhere on the skip list.
     */
    for (i = WT_SKIP_MAXDEPTH - 1, insp = &ins_head->head[i]; i >= 0; i--, insp--)
        for (ins = *insp; ins != NULL; ins = ins->next[i])
            WT_ASSERT(session, ins != moved_ins);
#endif

    /*
     * We perform insert splits concurrently with checkpoints, where the requirement is a checkpoint
     * must include either the original page or both new pages. The page we're splitting is dirty,
     * but that's insufficient: set the first dirty transaction to an impossibly old value so this
     * page is not skipped by a checkpoint.
     */
    page->modify->first_dirty_txn = WT_TXN_FIRST;

    /*
     * We modified the page above, which will have set the first dirty transaction to the last
     * transaction current running. However, the updates we installed may be older than that. Set
     * the first dirty transaction to an impossibly old value so this page is never skipped in a
     * checkpoint.
     */
    right->modify->first_dirty_txn = WT_TXN_FIRST;

    /*
     * Update the page accounting.
     */
    //源page上面的内存减少
    __wt_cache_page_inmem_decr(session, page, page_decr);
    //新page计数
    __wt_cache_page_inmem_incr(session, right, right_incr);

    /*
     * The act of splitting into the parent releases the pages for eviction; ensure the page
     * contents are consistent.
     */
    WT_WRITE_BARRIER();

    /*
     * Split into the parent.
     */
    printf("yang test ....................split_ref[0].page:%p, split_ref[1].page:%p\r\n", 
        split_ref[0]->page, split_ref[1]->page);
    if ((ret = __split_parent(session, ref, split_ref, 2, parent_incr, false, true)) == 0)
        return (0);

    //__split_parent异常，下面需要释放申请的资源信息

    /*
     * Failure.
     *
     * Reset the fixed-length column-store split page record.
     */
    if (type == WT_PAGE_COL_FIX)
        page->modify->mod_col_split_recno = WT_RECNO_OOB;

    /*
     * Clear the allocated page's reference to the moved insert list element so it's not freed when
     * we discard the page.
     *
     * Move the element back to the original page list. For simplicity, the previous skip list
     * pointers originally allocated can be ignored, just append the entry to the end of the level 0
     * list. As before, we depend on the list having multiple elements and ignore the edge cases
     * small lists have.
     */
    //
    if (type == WT_PAGE_ROW_LEAF)
        right->modify->mod_row_insert[0]->head[0] = right->modify->mod_row_insert[0]->tail[0] =
          NULL;
    else
        right->modify->mod_col_append[0]->head[0] = right->modify->mod_col_append[0]->tail[0] =
          NULL;

    //移除的这个KV需要恢复到源page对应跳跃表末尾

    //跳跃表中已有的最末尾的数据的next指向moved_ins
    ins_head->tail[0]->next[0] = moved_ins;
    //跳跃表的第0层的tail直接指向该节点
    ins_head->tail[0] = moved_ins;

    /* Fix up accounting for the page size. */
    __wt_cache_page_inmem_incr(session, page, page_decr);
    
err:
    if (split_ref[0] != NULL) {
        /*
         * The address was moved to the replacement WT_REF, restore it.
         */
        ref->addr = split_ref[0]->addr;

        if (type == WT_PAGE_ROW_LEAF)
            __wt_free(session, split_ref[0]->ref_ikey);
        __wt_free(session, split_ref[0]);
    }
    if (split_ref[1] != NULL) {
        if (type == WT_PAGE_ROW_LEAF)
            __wt_free(session, split_ref[1]->ref_ikey);
        __wt_free(session, split_ref[1]);
    }
    if (right != NULL) {
        /*
         * We marked the new page dirty; we're going to discard it, but first mark it clean and fix
         * up the cache statistics.
         */
        __wt_page_modify_clear(session, right);
        __wt_page_out(session, &right);
    }
    return (ret);
}

/*
 * __split_insert_lock --
 *     Split a page's last insert list entries into a separate page.
 */
static int
__split_insert_lock(WT_SESSION_IMPL *session, WT_REF *ref)
{
    WT_DECL_RET;
    WT_PAGE *parent;

    /* Lock the parent page, then proceed with the insert split. */
    WT_RET(__split_internal_lock(session, ref, true, &parent));
    if ((ret = __split_insert(session, ref)) != 0) {
        __split_internal_unlock(session, parent);
        return (ret);
    }

    /*
     * Split up through the tree as necessary; we're holding the original parent page locked, note
     * the functions we call are responsible for releasing that lock.
     */
    return (__split_parent_climb(session, parent));
}

/*
 * __wt_split_insert --
 *     Split a page's last insert list entries into a separate page.
 */

//__wt_evict: inmem_split，内存中的page进行拆分，拆分后的还是在内存中不会写入磁盘，对应__wt_split_insert(split-insert)打印
//__evict_page_dirty_update(__evict_reconcile): 对page拆分为多个page后写入磁盘中,对应__wt_split_multi(split-multi)打印
//__evict_page_dirty_update(__evict_reconcile): __wt_split_reverse(reverse-split)打印
//__evict_page_dirty_update(__evict_reconcile):__wt_split_rewrite(split-rewrite)打印

//__evict_force_check中通过page消耗的内存，决定走内存split evict还是reconcile evict
int
__wt_split_insert(WT_SESSION_IMPL *session, WT_REF *ref)
{
    WT_DECL_RET;

    __wt_verbose(session, WT_VERB_SPLIT, "%p: split-insert", (void *)ref);

    /*
     * Set the session split generation to ensure underlying code isn't surprised by internal page
     * eviction, then proceed with the insert split.
     */
    WT_WITH_PAGE_INDEX(session, ret = __split_insert_lock(session, ref));
    return (ret);
}

/*
 * __split_multi --
 *     Split a page into multiple pages.
 */
//__wt_evict->__evict_page_dirty_update->__wt_split_multi->__split_multi_lock->__split_multi
static int
__split_multi(WT_SESSION_IMPL *session, WT_REF *ref, bool closing)
{
    WT_DECL_RET;
    WT_PAGE *page;
    WT_PAGE_MODIFY *mod;
    WT_REF **ref_new;
    size_t parent_incr;
    uint32_t i, new_entries;

    WT_STAT_CONN_DATA_INCR(session, cache_eviction_split_leaf);

    page = ref->page;
    mod = page->modify;
    new_entries = mod->mod_multi_entries;

    parent_incr = 0;

    /*
     * Convert the split page's multiblock reconciliation information into an array of page
     * reference structures.
     */
    /* 一拆多后，构建新得ref[]及对应page，并和拆分前的page的父page关联 */
    //new_entries个新ref

    //这里要注意: 一个大page拆分的过程是先拆分为多个page(mod->mod_multi_entries)通过reconcile持久化, 然后在进行内存page的拆分
    // 所以这里每一个拆分后的内存page都会隐射一个磁盘page
    WT_RET(__wt_calloc_def(session, new_entries, &ref_new));
    for (i = 0; i < new_entries; ++i)
        WT_ERR( //为每一个page指向reconcile拆分后的磁盘元数据
        //新建一个ref， 实现一个multi与这个ref的映射，如果内存充足，还实现磁盘KV数据和page->pg_row的隐射，以及page相关赋值
          __wt_multi_to_ref(session, page, &mod->mod_multi[i], &ref_new[i], &parent_incr, closing));

    /*
     * Split into the parent; if we're closing the file, we hold it exclusively.
     */
    //该page拆分为multi_next个ref后，重新构建父parent的index索引数组
    WT_ERR(__split_parent(session, ref, ref_new, new_entries, parent_incr, closing, true));

    /* 下面逻辑主要是释放老page(也就是拆分前的page)的内存回收 */
    /*
     * The split succeeded, we can no longer fail.
     *
     * Finalize the move, discarding moved update lists from the original page.
     */
    for (i = 0; i < new_entries; ++i)
        __split_multi_inmem_final(session, page, &mod->mod_multi[i]);

    /*
     * Page with changes not written in this reconciliation is not marked as clean, do it now, then
     * discard the page.
     */
    __wt_page_modify_clear(session, page);
    //page空间释，包括WT_PAGE_MODIFY page->modify相关空间释放，包括mod_row_insert mod_row_update mod_multi等，以及page->dsk等
    //老的page在这里释放，前面已经生成了新page挂找到ref_new[]上，因此这里需要把老的ref释放掉

    //老ref在__split_parent->__split_parent_discard_ref释放，老page在__split_multi->__wt_page_out释放
    __wt_page_out(session, &page);

    if (0) {
err:
        for (i = 0; i < new_entries; ++i)
            __split_multi_inmem_fail(session, page, &mod->mod_multi[i], ref_new[i]);
        /*
         * Mark the page dirty to ensure it is reconciled again as we free the split disk images if
         * we fail to instantiate any of them into memory.
         */
        __wt_page_modify_set(session, page);
    }

    __wt_free(session, ref_new);
    return (ret);
}

/*
 * __split_multi_lock --
 *     Split a page into multiple pages.
 */
//__wt_evict->__evict_page_dirty_update->__wt_split_multi->__split_multi_lock
static int
__split_multi_lock(WT_SESSION_IMPL *session, WT_REF *ref, int closing)
{
    WT_DECL_RET;
    WT_PAGE *parent;

    /* Lock the parent page, then proceed with the split. */
    //这里为什么对需要split的parent加锁，而需要拆分的page没用加锁，原因是如果该page正在进行split,则其他线程通过__wt_page_in_func
    //  的WT_REF_LOCKED来延迟阻塞等待该split完成，等待的ref也就变为拆分后的ref数组的ref_new[0]，原来的ref是保留的，只是ref对应page发生了变化
    
    //获取page对应的page_lock锁，也就是锁住ref->home这个internal page, 并返回parent page
    WT_RET(__split_internal_lock(session, ref, false, &parent));
    //注意这里是对需要拆分的ref的父index[]元数据更新，父page index[]生成新的元数据
    if ((ret = __split_multi(session, ref, closing)) != 0 || closing) {
        __split_internal_unlock(session, parent);
        return (ret);
    }

    /*
     * Split up through the tree as necessary; we're holding the original parent page locked, note
     * the functions we call are responsible for releasing that lock.
     */
    //这里是对parent处理
    return (__split_parent_climb(session, parent));
}

/*
 * __wt_split_multi --
 *     Split a page into multiple pages.
 */
//__wt_evict: inmem_split，内存中的page进行拆分，拆分后的还是在内存中不会写入磁盘，对应__wt_split_insert(split-insert)打印
//__evict_page_dirty_update(__evict_reconcile): 对page拆分为多个page后写入磁盘中,对应__wt_split_multi(split-multi)打印
//__evict_page_dirty_update(__evict_reconcile): __wt_split_reverse(reverse-split)打印
//__evict_page_dirty_update(__evict_reconcile):__wt_split_rewrite(split-rewrite)打印

//__wt_evict->__evict_page_dirty_update->__wt_split_multi
int
__wt_split_multi(WT_SESSION_IMPL *session, WT_REF *ref, int closing)
{
    WT_DECL_RET;
   // uint64_t time_start, time_stop;

   // time_start = __wt_clock(session);
    __wt_verbose(session, WT_VERB_SPLIT, "%p: split-multi", (void *)ref);

    /*
     * Set the session split generation to ensure underlying code isn't surprised by internal page
     * eviction, then proceed with the split.
     */
    WT_WITH_PAGE_INDEX(session, ret = __split_multi_lock(session, ref, closing));

  //  time_stop = __wt_clock(session);
    
   // printf("yang test ..........__wt_split_multi...............time:%d us\r\n", (int)WT_CLOCKDIFF_US(time_stop, time_start));

    return (ret);
}

/*
 * __split_reverse --
 *     Reverse split (rewrite a parent page's index to reflect an empty page).
 */
static int
__split_reverse(WT_SESSION_IMPL *session, WT_REF *ref)
{
    WT_DECL_RET;
    WT_PAGE *parent;

    /* Lock the parent page, then proceed with the reverse split. */
    WT_RET(__split_internal_lock(session, ref, false, &parent));
    ret = __split_parent(session, ref, NULL, 0, 0, false, true);
    __split_internal_unlock(session, parent);
    return (ret);
}

/*
 * __wt_split_reverse --
 *     Reverse split (rewrite a parent page's index to reflect an empty page).
 */
//__wt_evict: inmem_split，内存中的page进行拆分，拆分后的还是在内存中不会写入磁盘，对应__wt_split_insert(split-insert)打印
//__evict_page_dirty_update(__evict_reconcile): 对page拆分为多个page后写入磁盘中,对应__wt_split_multi(split-multi)打印
//__evict_page_dirty_update(__evict_reconcile): __wt_split_reverse(reverse-split)打印
//__evict_page_dirty_update(__evict_reconcile):__wt_split_rewrite(split-rewrite)打印

int
__wt_split_reverse(WT_SESSION_IMPL *session, WT_REF *ref)
{
    WT_DECL_RET;

    __wt_verbose(session, WT_VERB_SPLIT, "%p: reverse-split", (void *)ref);

    /*
     * Set the session split generation to ensure underlying code isn't surprised by internal page
     * eviction, then proceed with the reverse split.
     */
    WT_WITH_PAGE_INDEX(session, ret = __split_reverse(session, ref));
    return (ret);
}

/*
 * __wt_split_rewrite --
 *     Rewrite an in-memory page with a new version.
 */
//__wt_evict: inmem_split，内存中的page进行拆分，拆分后的还是在内存中不会写入磁盘，对应__wt_split_insert(split-insert)打印
//__evict_page_dirty_update(__evict_reconcile): 对page拆分为多个page后写入磁盘中,对应__wt_split_multi(split-multi)打印
//__evict_page_dirty_update(__evict_reconcile): __wt_split_reverse(reverse-split)打印
//__evict_page_dirty_update(__evict_reconcile):__wt_split_rewrite(split-rewrite)打印

int
__wt_split_rewrite(WT_SESSION_IMPL *session, WT_REF *ref, WT_MULTI *multi)
{
    WT_DECL_RET;
    WT_PAGE *page;
    WT_REF *new;

    page = ref->page;

    __wt_verbose(session, WT_VERB_SPLIT, "%p: split-rewrite", (void *)ref);

    /*
     * This isn't a split: a reconciliation failed because we couldn't write something, and in the
     * case of forced eviction, we need to stop this page from being such a problem. We have
     * exclusive access, rewrite the page in memory. The code lives here because the split code
     * knows how to re-create a page in memory after it's been reconciled, and that's exactly what
     * we want to do.
     *
     * Build the new page.
     *
     * Allocate a WT_REF, the error path calls routines that free memory. The only field we need to
     * set is the record number, as it's used by the search routines.
     */
    WT_RET(__wt_calloc_one(session, &new));
    new->ref_recno = ref->ref_recno;

    WT_ERR(__split_multi_inmem(session, page, multi, new));

    /*
     * The rewrite succeeded, we can no longer fail.
     *
     * Finalize the move, discarding moved update lists from the original page.
     */
    __split_multi_inmem_final(session, page, multi);

    /*
     * Discard the original page.
     *
     * Pages with unresolved changes are not marked clean during reconciliation, do it now.
     *
     * Don't count this as eviction making progress, we did a one-for-one rewrite of a page in
     * memory, typical in the case of cache pressure unless the cache is configured for scrub and
     * page doesn't have any skipped updates.
     */
    __wt_page_modify_clear(session, page);
    if (!F_ISSET(S2C(session)->cache, WT_CACHE_EVICT_SCRUB) || multi->supd_restore)
        F_SET_ATOMIC_16(page, WT_PAGE_EVICT_NO_PROGRESS);
    __wt_ref_out(session, ref);

    /* Swap the new page into place. */
    ref->page = new->page;

    WT_REF_SET_STATE(ref, WT_REF_MEM);

    __wt_free(session, new);
    return (0);

err:
    __split_multi_inmem_fail(session, page, multi, new);
    return (ret);
}
