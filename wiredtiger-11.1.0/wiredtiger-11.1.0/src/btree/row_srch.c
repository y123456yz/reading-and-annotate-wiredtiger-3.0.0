/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

/*
 * __search_insert_append --
 *     Fast append search of a row-store insert list, creating a skiplist stack as we go.
 */ 
//__search_insert_append: 如果srch_key比调表中最大的key大，则记录最末尾KV的位置, 如果跳跃表上面还没有KV，则直接返回啥也不做
//__wt_search_insert: leaf page对应ins_head跳跃表上查找srch_key在跳跃表中的位置记录到cbt->next_stack[] cbt->ins_stack[]等中
//__wt_row_modify: 真正把KV添加到跳跃表中


////如果srch_key比调表中最大的key大，则记录最末尾KV的位置, 如果跳跃表上面还没有KV，则直接返回啥也不做
static inline int
__search_insert_append(WT_SESSION_IMPL *session, WT_CURSOR_BTREE *cbt, WT_INSERT_HEAD *ins_head,
  WT_ITEM *srch_key, bool *donep)
{
    WT_BTREE *btree;
    WT_COLLATOR *collator;
    WT_INSERT *ins;
    WT_ITEM key;
    int cmp, i;

    *donep = 0;

    btree = S2BT(session);
    collator = btree->collator;
    
    //ins为该page下跳跃表最后一个节点为NULL，说明该page跳跃表中还没有KV，第一个写入该跳跃表这里会直接返回
    if ((ins = WT_SKIP_LAST(ins_head)) == NULL) {
        return (0);
    }
    /*
     * Since the head of the skip list doesn't get mutated within this function, the compiler may
     * move this assignment above within the loop below if it needs to (and may read a different
     * value on each loop due to other threads mutating the skip list).
     *
     * Place a read barrier here to avoid this issue.
     */
    WT_READ_BARRIER();
    key.data = WT_INSERT_KEY(ins);
    key.size = WT_INSERT_KEY_SIZE(ins);

    WT_RET(__wt_compare(session, collator, srch_key, &key, &cmp));
    if (cmp >= 0) {//如果srch_key比调表中最大的key大，则记录最末尾KV的位置
        /*
         * !!!
         * We may race with another appending thread.
         *
         * To catch that case, rely on the atomic pointer read above
         * and set the next stack to NULL here.  If we have raced with
         * another thread, one of the next pointers will not be NULL by
         * the time they are checked against the next stack inside the
         * serialized insert function.
         */
        for (i = WT_SKIP_MAXDEPTH - 1; i >= 0; i--) {
            cbt->ins_stack[i] = (i == 0) ?
              &ins->next[0] :
              (ins_head->tail[i] != NULL) ? &ins_head->tail[i]->next[i] : &ins_head->head[i];
            cbt->next_stack[i] = NULL;
        }
        cbt->compare = -cmp;
        cbt->ins = ins;
        cbt->ins_head = ins_head;

        /*
         * If we find an exact match, copy the key into the temporary buffer, our callers expect to
         * find it there.
         */
        if (cbt->compare == 0) {
            cbt->tmp->data = WT_INSERT_KEY(cbt->ins);
            cbt->tmp->size = WT_INSERT_KEY_SIZE(cbt->ins);
        }

        *donep = 1;
    }
    return (0);
}

/*
 * __wt_search_insert --
 *     Search a row-store insert list, creating a skiplist stack as we go.
 */ 
//__search_insert_append: 如果srch_key比调表中最大的key大，则记录最末尾KV的位置, 如果跳跃表上面还没有KV，则直接返回啥也不做
//__wt_search_insert: leaf page对应ins_head跳跃表上查找srch_key在跳跃表中的位置记录到cbt->next_stack[] cbt->ins_stack[]等中
//__wt_row_modify: 真正把KV添加到跳跃表中
int
__wt_search_insert(
  WT_SESSION_IMPL *session, WT_CURSOR_BTREE *cbt, WT_INSERT_HEAD *ins_head, WT_ITEM *srch_key)
{
    WT_BTREE *btree;
    WT_COLLATOR *collator;
    WT_INSERT *ins, **insp, *last_ins;
    WT_ITEM key;
    size_t match, skiphigh, skiplow;
    int cmp, i;

    btree = S2BT(session);
    collator = btree->collator;
    cmp = 0; /* -Wuninitialized */

    /*
     * The insert list is a skip list: start at the highest skip level, then go as far as possible
     * at each level before stepping down to the next.
     */
    match = skiphigh = skiplow = 0;
    ins = last_ins = NULL;
    for (i = WT_SKIP_MAXDEPTH - 1, insp = &ins_head->head[i]; i >= 0;) {
        if ((ins = *insp) == NULL) {
            cbt->next_stack[i] = NULL;
            cbt->ins_stack[i--] = insp--;
            continue;
        }

        /*
         * Comparisons may be repeated as we drop down skiplist levels; don't repeat comparisons,
         * they might be expensive.
         */
        if (ins != last_ins) {
            last_ins = ins;
            key.data = WT_INSERT_KEY(ins);
            key.size = WT_INSERT_KEY_SIZE(ins);
            match = WT_MIN(skiplow, skiphigh);
            WT_RET(__wt_compare_skip(session, collator, srch_key, &key, &cmp, &match));
        }

        if (cmp > 0) { /* Keep going at this level */
            insp = &ins->next[i];
            skiplow = match;
        } else if (cmp < 0) { /* Drop down a level */
            cbt->next_stack[i] = ins;
            cbt->ins_stack[i--] = insp--;
            skiphigh = match;
        } else
            for (; i >= 0; i--) {
                cbt->next_stack[i] = ins->next[i];
                cbt->ins_stack[i] = &ins->next[i];
            }
    }

    /*
     * For every insert element we review, we're getting closer to a better choice; update the
     * compare field to its new value. If we went past the last item in the list, return the last
     * one: that is used to decide whether we are positioned in a skiplist.
     */
    cbt->compare = -cmp;
    cbt->ins = (ins != NULL) ? ins : last_ins;
    cbt->ins_head = ins_head;
    return (0);
}

/*
 * __check_leaf_key_range --
 *     Check the search key is in the leaf page's key range.
 */
static inline int
__check_leaf_key_range(
  WT_SESSION_IMPL *session, WT_ITEM *srch_key, WT_REF *leaf, WT_CURSOR_BTREE *cbt)
{
    WT_BTREE *btree;
    WT_COLLATOR *collator;
    WT_ITEM *item;
    WT_PAGE_INDEX *pindex;
    uint32_t indx;
    int cmp;

    btree = S2BT(session);
    collator = btree->collator;
    item = cbt->tmp;

    /*
     * There are reasons we can't do the fast checks, and we continue with the leaf page search in
     * those cases, only skipping the complete leaf page search if we know it's not going to work.
     */
    cbt->compare = 0;

    /*
     * First, confirm we have the right parent page-index slot, and quit if we don't. We don't
     * search for the correct slot, that would make this cheap test expensive.
     */
    WT_INTL_INDEX_GET(session, leaf->home, pindex);
    indx = leaf->pindex_hint;
    if (indx >= pindex->entries || pindex->index[indx] != leaf)
        return (0);

    /*
     * Check if the search key is smaller than the parent's starting key for this page.
     *
     * We can't compare against slot 0 on a row-store internal page because reconciliation doesn't
     * build it, it may not be a valid key.
     */
    if (indx != 0) {
        __wt_ref_key(leaf->home, leaf, &item->data, &item->size);
        WT_RET(__wt_compare(session, collator, srch_key, item, &cmp));
        if (cmp < 0) {
            cbt->compare = 1; /* page keys > search key */
            return (0);
        }
    }

    /*
     * Check if the search key is greater than or equal to the starting key for the parent's next
     * page.
     */
    ++indx;
    if (indx < pindex->entries) {
        __wt_ref_key(leaf->home, pindex->index[indx], &item->data, &item->size);
        WT_RET(__wt_compare(session, collator, srch_key, item, &cmp));
        if (cmp >= 0) {
            cbt->compare = -1; /* page keys < search key */
            return (0);
        }
    }

    return (0);
}

/*
 * __wt_row_search --
 *     Search a row-store tree for a specific key.
 */ //查找srch_key在btree中是否存在，确定该srch_key应该在跳跃表那个位置
int
__wt_row_search(WT_CURSOR_BTREE *cbt, WT_ITEM *srch_key, bool insert, WT_REF *leaf, bool leaf_safe,
  bool *leaf_foundp)
{
    WT_BTREE *btree;
    WT_COLLATOR *collator;
    WT_DECL_RET;
    WT_INSERT_HEAD *ins_head;
    WT_ITEM *item;
    WT_PAGE *page;
    WT_PAGE_INDEX *pindex, *parent_pindex;
    WT_REF *current, *descent;
    WT_ROW *rip;
    WT_SESSION_IMPL *session;
    size_t match, skiphigh, skiplow;
    uint32_t base, indx, limit, read_flags;
    int cmp, depth;
    bool append_check, descend_right, done;

    session = CUR2S(cbt);
    btree = S2BT(session);
    collator = btree->collator;
    item = cbt->tmp;
    current = NULL;

    /*
     * Assert the session and cursor have the right relationship (not search specific, but search is
     * a convenient place to check given any operation on a cursor will likely search a page).
     */
    WT_ASSERT(session, session->dhandle == cbt->dhandle);

    __cursor_pos_clear(cbt);

    /*
     * In some cases we expect we're comparing more than a few keys with matching prefixes, so it's
     * faster to avoid the memory fetches by skipping over those prefixes. That's done by tracking
     * the length of the prefix match for the lowest and highest keys we compare as we descend the
     * tree. The high boundary is reset on each new page, the lower boundary is maintained.
     */
    skiplow = 0;

    /*
     * If a cursor repeatedly appends to the tree, compare the search key against the last key on
     * each internal page during insert before doing the full binary search.
     *
     * Track if the descent is to the right-side of the tree, used to set the cursor's append
     * history.
     */
    //如果一个cursor多次insert，第二次insert开始就会append_check=true
    append_check = insert && cbt->append_tree;
    descend_right = true;

    /*
     * We may be searching only a single leaf page, not the full tree. In the normal case where we
     * are searching a tree, check the page's parent keys before doing the full search, it's faster
     * when the cursor is being re-positioned. Skip that check if we know the page is the right one
     * (for example, when re-instantiating a page in memory, in that case we know the target must be
     * on the current page).
     */
    if (leaf != NULL) {//如果是从指定得leaf page查找
        if (!leaf_safe) {
            WT_RET(__check_leaf_key_range(session, srch_key, leaf, cbt));
            *leaf_foundp = cbt->compare == 0;
            if (!*leaf_foundp)
                return (0);
        }

        current = leaf;
        goto leaf_only;
    }

    if (0) {
restart://表示重新从root page开始查找
        /*
         * Discard the currently held page and restart the search from the root.
         */
        WT_RET(__wt_page_release(session, current, 0));
        skiplow = 0;
    }

    /* Search the internal pages of the tree. */
    //从根page开始查找
    current = &btree->root;
    //这里从2开始，原因是需要加上leaf page那一层
    for (depth = 2, pindex = NULL;; ++depth) {
        parent_pindex = pindex;
        page = current->page;
        if (page->type != WT_PAGE_ROW_INT)//找到了该srch_key对应的leaf page，直接break跳出循环
            break;

        //internal page才会走下面的流程

        //获取该internal page下面的index ref数组
        WT_INTL_INDEX_GET(session, page, pindex);

        /*
         * Fast-path appends.
         *
         * The 0th key on an internal page is a problem for a couple of reasons. First, we have to
         * force the 0th key to sort less than any application key, so internal pages don't have to
         * be updated if the application stores a new, "smallest" key in the tree. Second,
         * reconciliation is aware of this and will store a byte of garbage in the 0th key, so the
         * comparison of an application key and a 0th key is meaningless (but doing the comparison
         * could still incorrectly modify our tracking of the leading bytes in each key that we can
         * skip during the comparison). For these reasons, special-case the 0th key, and never pass
         * it to a collator.
         */
       // printf("yang test ...................................append_check:%d\r\n", append_check);
        if (append_check) {
            descent = pindex->index[pindex->entries - 1];
            //该internale page下面只有1个leaf page,那么我们数据肯定需要到这个leaf page
            if (pindex->entries == 1)
                goto append;

            //如果该internale page下面不止一个leaf page，则取该internale page下面最右边的leaf page
           
            //拷贝ref->ref_ikey到item
            __wt_ref_key(page, descent, &item->data, &item->size);
            WT_ERR(__wt_compare(session, collator, srch_key, item, &cmp));
             //目的是判断该srch_key是否是该btree中最大的key, 如果是最大的key,则直接追加到该leaf page即可
            if (cmp >= 0)
                goto append;

            //说明这个srch_key不比btree中的最大key大，则需要通过后面的二分查找从internal page中找到该srch_key应该属于那个leaf page
            /* A failed append check turns off append checks. */
            append_check = false;
        }

        /*
         * Binary search of an internal page. There are three versions (keys with no
         * application-specified collation order, in long and short versions, and keys with an
         * application-specified collation order), because doing the tests and error handling inside
         * the loop costs about 5%.
         *
         * Reference the comment above about the 0th key: we continue to special-case it.
         */
        base = 1;
        limit = pindex->entries - 1;
        //小key比较，二分快速查找
        if (collator == NULL && srch_key->size <= WT_COMPARE_SHORT_MAXLEN)
            for (; limit != 0; limit >>= 1) {
                indx = base + (limit >> 1);
                descent = pindex->index[indx];
                __wt_ref_key(page, descent, &item->data, &item->size);

                cmp = __wt_lex_compare_short(srch_key, item);
                if (cmp > 0) {
                    base = indx + 1;
                    --limit;
                } else if (cmp == 0)
                    goto descend;
            }
        else if (collator == NULL) {//大key比较
            /*
             * Reset the skipped prefix counts; we'd normally expect the parent's skipped prefix
             * values to be larger than the child's values and so we'd only increase them as we walk
             * down the tree (in other words, if we can skip N bytes on the parent, we can skip at
             * least N bytes on the child). However, if a child internal page was split up into the
             * parent, the child page's key space will have been truncated, and the values from the
             * parent's search may be wrong for the child. We only need to reset the high count
             * because the split-page algorithm truncates the end of the internal page's key space,
             * the low count is still correct.
             */
            skiphigh = 0;

            for (; limit != 0; limit >>= 1) {
                indx = base + (limit >> 1);
                descent = pindex->index[indx];
                __wt_ref_key(page, descent, &item->data, &item->size);

                match = WT_MIN(skiplow, skiphigh);
                cmp = __wt_lex_compare_skip(srch_key, item, &match);
                if (cmp > 0) {
                    skiplow = match;
                    base = indx + 1;
                    --limit;
                } else if (cmp < 0)
                    skiphigh = match;
                else
                    goto descend;
            }
        } else
        //自定义比较器走这里
            for (; limit != 0; limit >>= 1) {
                indx = base + (limit >> 1);
                descent = pindex->index[indx];
                __wt_ref_key(page, descent, &item->data, &item->size);

                WT_ERR(__wt_compare(session, collator, srch_key, item, &cmp));
                if (cmp > 0) {
                    base = indx + 1;
                    --limit;
                } else if (cmp == 0)
                    goto descend;
            }

        /*
         * Set the slot to descend the tree: descent was already set if there was an exact match on
         * the page, otherwise, base is the smallest index greater than key, possibly one past the
         * last slot.
         */
        descent = pindex->index[base - 1];

        /*
         * If we end up somewhere other than the last slot, it's not a right-side descent.
         */
        //说明srch_key不在BTREE中所有leaf page的最右边
        if (pindex->entries != base)
            descend_right = false;

       // printf("yang test ........entries:%u.....base:%u\r\n", pindex->entries, base);
        /*
         * If on the last slot (the key is larger than any key on the page), check for an internal
         * page split race.
         */
        if (pindex->entries == base) {
append:
            if (__wt_split_descent_race(session, current, parent_pindex))
                //重新从根page查找
                goto restart;
        }

descend:
        /* Encourage races. */
        WT_DIAGNOSTIC_YIELD;

        /*
         * Swap the current page for the child page. If the page splits while we're retrieving it,
         * restart the search at the root. We cannot restart in the "current" page; for example, if
         * a thread is appending to the tree, the page it's waiting for did an insert-split into the
         * parent, then the parent split into its parent, the name space we are searching for may
         * have moved above the current page in the tree.
         *
         * On other error, simply return, the swap call ensures we're holding nothing on failure.
         */
        read_flags = WT_READ_RESTART_OK;
        if (F_ISSET(cbt, WT_CBT_READ_ONCE))
            FLD_SET(read_flags, WT_READ_WONT_NEED);
        //如果BTREE中没数据，这时候写入一条数据，是第一次写入，这里面会创建leaf page存入&descent->page
        //internal page查找的时候都会到这里，这里面可能会创建leaf page

        //current代表当前的internal page, descent代表对应的下一层page，可能是internal page也可能是leaf page
        if ((ret = __wt_page_swap(session, current, descent, read_flags)) == 0) {
            //把该internal page赋值给current，然后继续下一层internal page的遍历
            current = descent;
            continue;
        }
    
        if (ret == WT_RESTART) //重新从根page查找
            goto restart; //?????????????????? 有没有可能会形成死循环，反复查找
        return (ret);
    }

    /* Track how deep the tree gets. */
    //获取到leaf page的层数记录下来
    if (depth > btree->maximum_depth)
        btree->maximum_depth = depth;

leaf_only:
    page = current->page;
    //记录srch_key所属的leaf page
    cbt->ref = current;

    /*
     * Clear current now that we have moved the reference into the btree cursor, so that cleanup
     * never releases twice.
     */
    current = NULL;

    /*
     * In the case of a right-side tree descent during an insert, do a fast check for an append to
     * the page, try to catch cursors appending data into the tree.
     *
     * It's tempting to make this test more rigorous: if a cursor inserts randomly into a two-level
     * tree (a root referencing a single child that's empty except for an insert list), the
     * right-side descent flag will be set and this comparison wasted. The problem resolves itself
     * as the tree grows larger: either we're no longer doing right-side descent, or we'll avoid
     * additional comparisons in internal pages, making up for the wasted comparison here.
     * Similarly, the cursor's history is set any time it's an insert and a right-side descent, both
     * to avoid a complicated/expensive test, and, in the case of multiple threads appending to the
     * tree, we want to mark them all as appending, even if this test doesn't work.
     */
    if (insert && descend_right) {
    //descend_right表示查找到的leaf page不在btree leaf page的最右边，在中间或者左边，就没必要append了
        cbt->append_tree = 1;

        //获取最右边的leaf page
        if (page->entries == 0) {
            cbt->slot = WT_ROW_SLOT(page, page->pg_row);

            F_SET(cbt, WT_CBT_SEARCH_SMALLEST);
            ins_head = WT_ROW_INSERT_SMALLEST(page);
        } else {
            cbt->slot = WT_ROW_SLOT(page, page->pg_row + (page->entries - 1));
            ins_head = WT_ROW_INSERT_SLOT(page, cbt->slot);
        }

        ///如果srch_key比调表中最大的key大，则记录最末尾KV的位置, 如果跳跃表上面还没有KV，则直接返回啥也不做
        WT_ERR(__search_insert_append(session, cbt, ins_head, srch_key, &done));
        if (done)
            return (0);
    }

    /*
     * Binary search of an leaf page. There are three versions (keys with no application-specified
     * collation order, in long and short versions, and keys with an application-specified collation
     * order), because doing the tests and error handling inside the loop costs about 5%.
     */
    //也就是二分查找定位到的pg_row数组位置
    base = 0;
    limit = page->entries;
    if (collator == NULL && srch_key->size <= WT_COMPARE_SHORT_MAXLEN)
        for (; limit != 0; limit >>= 1) {
            indx = base + (limit >> 1);
            rip = page->pg_row + indx;
            WT_ERR(__wt_row_leaf_key(session, page, rip, item, true));

            cmp = __wt_lex_compare_short(srch_key, item);
            if (cmp > 0) {
                base = indx + 1;
                --limit;
            } else if (cmp == 0)
                goto leaf_match;
        }
    else if (collator == NULL) {
        /*
         * Reset the skipped prefix counts; we'd normally expect the parent's skipped prefix values
         * to be larger than the child's values and so we'd only increase them as we walk down the
         * tree (in other words, if we can skip N bytes on the parent, we can skip at least N bytes
         * on the child). However, leaf pages at the end of the tree can be extended, causing the
         * parent's search to be wrong for the child. We only need to reset the high count, the page
         * can only be extended so the low count is still correct.
         */
        skiphigh = 0;

        for (; limit != 0; limit >>= 1) {
            indx = base + (limit >> 1);
            rip = page->pg_row + indx;
            WT_ERR(__wt_row_leaf_key(session, page, rip, item, true));

            match = WT_MIN(skiplow, skiphigh);
            cmp = __wt_lex_compare_skip(srch_key, item, &match);
            if (cmp > 0) {
                skiplow = match;
                base = indx + 1;
                --limit;
            } else if (cmp < 0)
                skiphigh = match;
            else
                goto leaf_match;
        }
    } else
        for (; limit != 0; limit >>= 1) {
            indx = base + (limit >> 1);
            rip = page->pg_row + indx;
            WT_ERR(__wt_row_leaf_key(session, page, rip, item, true));

            WT_ERR(__wt_compare(session, collator, srch_key, item, &cmp));
            if (cmp > 0) {
                base = indx + 1;
                --limit;
            } else if (cmp == 0)
                goto leaf_match;
        }

    /*
     * The best case is finding an exact match in the leaf page's WT_ROW array, probable for any
     * read-mostly workload. Check that case and get out fast.
     */
    if (0) {
leaf_match:
        //btree中存在一个完全一样的key
        cbt->compare = 0;
        cbt->slot = WT_ROW_SLOT(page, rip);
        return (0);
    }

    /*
     * We didn't find an exact match in the WT_ROW array.
     *
     * Base is the smallest index greater than key and may be the 0th index or the (last + 1) index.
     * Set the slot to be the largest index less than the key if that's possible (if base is the 0th
     * index it means the application is inserting a key before any key found on the page).
     *
     * It's still possible there is an exact match, but it's on an insert list. Figure out which
     * insert chain to search and then set up the return information assuming we'll find nothing in
     * the insert list (we'll correct as needed inside the search routine, depending on what we
     * find).
     *
     * If inserting a key smaller than any key found in the WT_ROW array, use the extra slot of the
     * insert array, otherwise the insert array maps one-to-one to the WT_ROW array.
     */
   // printf("yang test .......insert:%d......descend_right:%d.....page->entries:%u,....cbt->slot:%u, base:%u\r\n", 
   //        insert, descend_right, page->entries, cbt->slot, base);
    if (base == 0) {
        cbt->compare = 1;
        cbt->slot = 0;

        F_SET(cbt, WT_CBT_SEARCH_SMALLEST);
        ins_head = WT_ROW_INSERT_SMALLEST(page);
    } else {
        cbt->compare = -1;
        cbt->slot = base - 1;

        ins_head = WT_ROW_INSERT_SLOT(page, cbt->slot);
    }

    /* If there's no insert list, we're done. */
    //该page上跳跃表还没有KV数据直接返回
    if (WT_SKIP_FIRST(ins_head) == NULL)
        return (0);

    /*
     * Test for an append first when inserting onto an insert list, try to catch cursors repeatedly
     * inserting at a single point, then search the insert list. If we find an exact match, copy the
     * key into the temporary buffer, our callers expect to find it there.
     */
    if (insert) {//如果srch_key比该page对应跳跃表中最大的key大，则记录最末尾KV的位置, 如果跳跃表上面还没有KV，则直接返回啥也不做
        WT_ERR(__search_insert_append(session, cbt, ins_head, srch_key, &done));
        if (done)
            return (0);
    }

    //leaf page对应ins_head跳跃表上查找srch_key在跳跃表中的位置记录到cbt->next_stack[] cbt->ins_stack[]等中
    WT_ERR(__wt_search_insert(session, cbt, ins_head, srch_key));
    if (cbt->compare == 0) {
        cbt->tmp->data = WT_INSERT_KEY(cbt->ins);
        cbt->tmp->size = WT_INSERT_KEY_SIZE(cbt->ins);
    }
    return (0);

err:
    WT_TRET(__wt_page_release(session, current, 0));
    return (ret);
}
