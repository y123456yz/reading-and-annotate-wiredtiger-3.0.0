/*-
 * Copyright (c) 2014-2017 MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

/*
 * __search_insert_append --
 *	Fast append search of a row-store insert list, creating a skiplist stack
 * as we go.
 */
/* 在insert append列表中查找定位key对应的记录, 并构建一个skip list stack 对象返回*/
//insert list定位该KV应该放在什么位置，注意是排好序的跳跃表中
static inline int
__search_insert_append(WT_SESSION_IMPL *session, WT_CURSOR_BTREE *cbt,
    WT_INSERT_HEAD *ins_head, WT_ITEM *srch_key, bool *donep)
{
	WT_BTREE *btree;
	WT_COLLATOR *collator;
	WT_INSERT *ins;
	WT_ITEM key;
	int cmp, i;

	*donep = 0;

	btree = S2BT(session);
	collator = btree->collator;

	if ((ins = WT_SKIP_LAST(ins_head)) == NULL) //下次插入到skip末尾
		return (0);

	/*通过ins获得key的值*/
	key.data = WT_INSERT_KEY(ins);
	key.size = WT_INSERT_KEY_SIZE(ins);

	WT_RET(__wt_compare(session, collator, srch_key, &key, &cmp));
	if (cmp >= 0) {
		/*
		 * !!!
		 * We may race with another appending thread.
		 *
		 * To catch that case, rely on the atomic pointer read above
		 * and set the next stack to NULL here.  If we have raced with
		 * another thread, one of the next pointers will not be NULL by
		 * the time they are checked against the next stack inside the
		 * serialized insert function.
		 */ //cbt->ins_stack[]来定位位置
		for (i = WT_SKIP_MAXDEPTH - 1; i >= 0; i--) {
			cbt->ins_stack[i] = (i == 0) ? &ins->next[0] :
			    (ins_head->tail[i] != NULL) ?
			    &ins_head->tail[i]->next[i] : &ins_head->head[i];
			cbt->next_stack[i] = NULL;
		}

		//获取该kv对应的insert和insert_head，也就是应该在跳跃表中的那个insert位置插入，后面的__cursor_row_modify取这些ins和ins_head指针来做插入操作
		cbt->compare = -cmp;
		cbt->ins = ins;
		cbt->ins_head = ins_head;
		*donep = 1;
	}
	return (0);
}

/*
 * __wt_search_insert --
 *	Search a row-store insert list, creating a skiplist stack as we go.
 */
/* 为row store方式检索insert list,并构建一个skip list stack，确定ins_head */
int
__wt_search_insert(WT_SESSION_IMPL *session,
    WT_CURSOR_BTREE *cbt, WT_INSERT_HEAD *ins_head, WT_ITEM *srch_key)
{
	WT_BTREE *btree;
	WT_COLLATOR *collator;
	WT_INSERT *ins, **insp, *last_ins;
	WT_ITEM key;
	size_t match, skiphigh, skiplow;
	int cmp, i;

	btree = S2BT(session);
	collator = btree->collator;
	cmp = 0;				/* -Wuninitialized */

	/*
	 * The insert list is a skip list: start at the highest skip level, then
	 * go as far as possible at each level before stepping down to the next.
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
		 * Comparisons may be repeated as we drop down skiplist levels;
		 * don't repeat comparisons, they might be expensive.
		 */
		if (ins != last_ins) { //查找key
			last_ins = ins;
			key.data = WT_INSERT_KEY(ins);
			key.size = WT_INSERT_KEY_SIZE(ins);
			match = WT_MIN(skiplow, skiphigh);
			WT_RET(__wt_compare_skip(
			    session, collator, srch_key, &key, &cmp, &match));
		}

		if (cmp > 0) {			/* Keep going at this level */
			insp = &ins->next[i];
			skiplow = match;
		} else if (cmp < 0) {		/* Drop down a level */
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
	 * For every insert element we review, we're getting closer to a better
	 * choice; update the compare field to its new value.  If we went past
	 * the last item in the list, return the last one: that is used to
	 * decide whether we are positioned in a skiplist.
	 */
	cbt->compare = -cmp;
	//记录下这个找到的位置，例如serch操作查找某个key的value的时候，就从ins取，见__wt_btcur_search __wt_row_modify
	cbt->ins = (ins != NULL) ? ins : last_ins;
	cbt->ins_head = ins_head;
	return (0);
}

/*
 * __check_leaf_key_range --
 *	Check the search key is in the leaf page's key range.
 */
//
static inline int
__check_leaf_key_range(WT_SESSION_IMPL *session,
    WT_ITEM *srch_key, WT_REF *leaf, WT_CURSOR_BTREE *cbt)
{
	WT_BTREE *btree;
	WT_COLLATOR *collator;
	WT_ITEM *item;
	WT_PAGE_INDEX *pindex;
	uint32_t indx;
	int cmp;
    printf("yang test ......d........... 11 __check_leaf_key_range\r\n");

	btree = S2BT(session);
	collator = btree->collator;
	item = cbt->tmp;

	/*
	 * There are reasons we can't do the fast checks, and we continue with
	 * the leaf page search in those cases, only skipping the complete leaf
	 * page search if we know it's not going to work.
	 */
	cbt->compare = 0;

	/*
	 * First, confirm we have the right parent page-index slot, and quit if
	 * we don't. We don't search for the correct slot, that would make this
	 * cheap test expensive.
	 */
	WT_INTL_INDEX_GET(session, leaf->home, pindex);
	indx = leaf->pindex_hint;//先找到该page的父节点的_index，做检查，因为父page的pindex->index[indx]应该要指向该leaf page
	if (indx >= pindex->entries || pindex->index[indx] != leaf) //
		return (0);

	/*
	 * Check if the search key is smaller than the parent's starting key for
	 * this page.
	 *
	 * We can't compare against slot 0 on a row-store internal page because
	 * reconciliation doesn't build it, it may not be a valid key.
	 */
	if (indx != 0) {
		__wt_ref_key(leaf->home, leaf, &item->data, &item->size); //获取该leaf page上所有的key空间
        __wt_verbose(session, WT_VERB_API, "__check_leaf_key_range 1, key:%s, value:%s", item->data, item->size);
		WT_RET(__wt_compare(session, collator, srch_key, item, &cmp));
		if (cmp < 0) {
			cbt->compare = 1;	/* page keys > search key */
			return (0);
		}
	}

	/*
	 * Check if the search key is greater than or equal to the starting key
	 * for the parent's next page.
	 */
	++indx;
	if (indx < pindex->entries) {
	    __wt_verbose(session, WT_VERB_API, "__check_leaf_key_range 2, key:%s, value:%s", item->data, item->size);
		__wt_ref_key(
		    leaf->home, pindex->index[indx], &item->data, &item->size);
		WT_RET(__wt_compare(session, collator, srch_key, item, &cmp));
		if (cmp >= 0) {
			cbt->compare = -1;	/* page keys < search key */
			return (0);
		}
	}

	return (0);
}

/*
 * __wt_row_search --
 *	Search a row-store tree for a specific key.
/* 用指定的key在ref对应的page进行查找定位，存储方式为row store方式 */
int
__wt_row_search(WT_SESSION_IMPL *session,
    WT_ITEM *srch_key, WT_REF *leaf, WT_CURSOR_BTREE *cbt,
    bool insert, bool restore)
{
	WT_BTREE *btree;
	WT_COLLATOR *collator;
	WT_DECL_RET;
	WT_INSERT_HEAD *ins_head;
	WT_ITEM *item;
	WT_PAGE *page;
	WT_PAGE_INDEX *pindex, *parent_pindex;
	WT_REF *current, *descent;
	WT_ROW *rip; //当找到该key后，确定该KV在page->pg_row的位置
	size_t match, skiphigh, skiplow;
	uint32_t base, indx, limit;
	int cmp, depth;
	bool append_check, descend_right, done;

	btree = S2BT(session);
	collator = btree->collator;
	item = cbt->tmp;
	current = NULL;
	
    /* btree cursor 复位 */
	__cursor_pos_clear(cbt);

	/*
	 * In some cases we expect we're comparing more than a few keys with
	 * matching prefixes, so it's faster to avoid the memory fetches by
	 * skipping over those prefixes. That's done by tracking the length of
	 * the prefix match for the lowest and highest keys we compare as we
	 * descend the tree.
	 */
	skiphigh = skiplow = 0;

	/*
	 * If a cursor repeatedly appends to the tree, compare the search key
	 * against the last key on each internal page during insert before
	 * doing the full binary search.
	 *
	 * Track if the descent is to the right-side of the tree, used to set
	 * the cursor's append history.
	 */
	append_check = insert && cbt->append_tree;
	descend_right = true;

	/*
	 * We may be searching only a single leaf page, not the full tree. In
	 * the normal case where we are searching a tree, check the page's
	 * parent keys before doing the full search, it's faster when the
	 * cursor is being re-positioned.  Skip this if the page is being
	 * re-instantiated in memory.
	 */
	//直接在该leaf page中查找
	if (leaf != NULL) { /*如果BTREE SPLITS, 只能检索单个叶子节点, 而不能检索整个树*/
		if (!restore) {
			WT_RET(__check_leaf_key_range(
			    session, srch_key, leaf, cbt));
			if (cbt->compare != 0) {
				/*
				 * !!!
				 * WT_CURSOR.search_near uses the slot value to
				 * decide if there was an on-page match.
				 */
				cbt->slot = 0;
				return (0);
			}
		}

		current = leaf;
		goto leaf_only;
	}

	if (0) {
restart:	/*
		 * Discard the currently held page and restart the search from
		 * the root.
		 */
		WT_RET(__wt_page_release(session, current, 0));
		skiphigh = skiplow = 0;
	}

    /*
注意:如果KV较少，则一个leaf page就可以存储所有的KV，其树结构如下
          root page(internal page类型)
          /
         /
        /
     leaf page(该leaf page存储了所有这些少量的KV)

    上面的模型serach kv的时候，一次性就可以定位到leaf page
    当该leaf page随着KV越来越多，消耗的page内存超过一定比例(split_pct)，就开始进行分裂
    当所有的tree树使用的内存超过cacheSizeGB，则会触发evict线程淘汰处理，把一部分page写入磁盘，参考http://www.mongoing.com/archives/3675
    */

    //没有指定leaf page则查找整课树
	/* Search the internal pages of the tree. */
	current = &btree->root;
	/*对internal page做检索定位*/
	for (depth = 2, pindex = NULL;; ++depth) {
		parent_pindex = pindex;
		page = current->page;
		/*已经到叶子节点了，退出对叶子节点做检索  root page和internale page实际上都是WT_PAGE_ROW_INT*/
		if (page->type != WT_PAGE_ROW_INT) //只遍历internal page下的leaf page
			break;

        //获取internal page对应的pindex
		WT_INTL_INDEX_GET(session, page, pindex);

		/*
		 * Fast-path appends.
		 *
		 * The 0th key on an internal page is a problem for a couple of
		 * reasons.  First, we have to force the 0th key to sort less
		 * than any application key, so internal pages don't have to be
		 * updated if the application stores a new, "smallest" key in
		 * the tree.  Second, reconciliation is aware of this and will
		 * store a byte of garbage in the 0th key, so the comparison of
		 * an application key and a 0th key is meaningless (but doing
		 * the comparison could still incorrectly modify our tracking
		 * of the leading bytes in each key that we can skip during the
		 * comparison). For these reasons, special-case the 0th key, and
		 * never pass it to a collator.
		 */
		/* Fast-path appends. 追加式插入，只需要定位最后一个entry,判断最后一个entry是否包含了插入KEY的值范围，
		如果是，直接进入下一层 */
		if (append_check) {
			descent = pindex->index[pindex->entries - 1];
            /*只有一个孩子，直接深入下一级孩子节点做检索*/
			if (pindex->entries == 1)
				goto append;

			//获取page对应的key存储到item中
			__wt_ref_key(page, descent, &item->data, &item->size);
			__wt_verbose(session, WT_VERB_API, "__wt_row_search 1, key:%s, value:%s", item->data, item->size);
			WT_ERR(__wt_compare(
			    session, collator, srch_key, item, &cmp));
			if (cmp >= 0)
				goto append;

			/* A failed append check turns off append checks. */
			append_check = false;
		}

		/*
		 * Binary search of an internal page. There are three versions
		 * (keys with no application-specified collation order, in long
		 * and short versions, and keys with an application-specified
		 * collation order), because doing the tests and error handling
		 * inside the loop costs about 5%.
		 *
		 * Reference the comment above about the 0th key: we continue to
		 * special-case it.
		 */
        //参考《MySQL技术内幕:InnoDB存储引擎(第2版)》 索引与算法更好理解
		 
		base = 1;
		limit = pindex->entries - 1;
		/*用二分法进行内部索引页内定位,定位到key对应的leaf page*/
		if (collator == NULL && //没有指定collator比较方法
		/*key范围增量比较,防止比较过程运算过多*/
		    srch_key->size <= WT_COMPARE_SHORT_MAXLEN) {
			for (; limit != 0; limit >>= 1) {
				indx = base + (limit >> 1);
				descent = pindex->index[indx];
				__wt_ref_key(
				    page, descent, &item->data, &item->size);
                __wt_verbose(session, WT_VERB_API, "__wt_row_search 1, key:%s, value:%s", item->data, item->size);
				cmp = __wt_lex_compare_short(srch_key, item);
				if (cmp > 0) { //srch_key > item对应的key，说明srch_key需要从下一个indx对应的page中查找
					base = indx + 1; //
					--limit;
				} else if (cmp == 0) //相等，说明应该在该index对应的page中继续查找
					goto descend;
			} 
		} else if (collator == NULL) { /*用二分法进行内部索引页内定位,定位到key对应的leaf page*/
			/*
			 * Reset the skipped prefix counts; we'd normally expect
			 * the parent's skipped prefix values to be larger than
			 * the child's values and so we'd only increase them as
			 * we walk down the tree (in other words, if we can skip
			 * N bytes on the parent, we can skip at least N bytes
			 * on the child). However, if a child internal page was
			 * split up into the parent, the child page's key space
			 * will have been truncated, and the values from the
			 * parent's search may be wrong for the child. We only
			 * need to reset the high count because the split-page
			 * algorithm truncates the end of the internal page's
			 * key space, the low count is still correct. We also
			 * don't need to clear either count when transitioning
			 * to a leaf page, a leaf page's key space can't change
			 * in flight.
			 */
			skiphigh = 0;
            /*用二分法进行内部索引页内定位,定位到key对应的leaf page*/

			for (; limit != 0; limit >>= 1) {
				indx = base + (limit >> 1);
				descent = pindex->index[indx];
				__wt_ref_key(
				    page, descent, &item->data, &item->size);
                __wt_verbose(session, WT_VERB_API, "__wt_row_search 1, key:%s, value:%s", item->data, item->size);

				match = WT_MIN(skiplow, skiphigh);
				cmp = __wt_lex_compare_skip(
				    srch_key, item, &match);
				if (cmp > 0) {
					skiplow = match;
					base = indx + 1;
					--limit;
				} else if (cmp < 0)
					skiphigh = match;
				else //相等，说明应该在该index对应的page中继续查找
					goto descend;
			}
		} else
			for (; limit != 0; limit >>= 1) { /*通过collator来比较*/
				indx = base + (limit >> 1);
				descent = pindex->index[indx];
				__wt_ref_key(
				    page, descent, &item->data, &item->size);
                __wt_verbose(session, WT_VERB_API, "__wt_row_search 1, key:%s, value:%s", item->data, item->size);
				WT_ERR(__wt_compare(
				    session, collator, srch_key, item, &cmp));
				if (cmp > 0) {
					base = indx + 1;
					--limit;
				} else if (cmp == 0) //相等，说明应该在该index对应的page中继续查找
					goto descend;
			}

		/*
		 * Set the slot to descend the tree: descent was already set if
		 * there was an exact match on the page, otherwise, base is the
		 * smallest index greater than key, possibly one past the last
		 * slot.
		 */ /*定位到存储key的范围page ref*/
		descent = pindex->index[base - 1];
		/*
		 * If we end up somewhere other than the last slot, it's not a
		 * right-side descent.
		 */
		if (pindex->entries != base)
			descend_right = false;

		/*
		 * If on the last slot (the key is larger than any key on the
		 * page), check for an internal page split race.
		 */
		if (pindex->entries == base) {
append:			if (__wt_split_descent_race(
			    session, current, parent_pindex)) {
				    goto restart;
				}
		}

descend:	/*
		 * Swap the current page for the child page. If the page splits
		 * while we're retrieving it, restart the search at the root.
		 * We cannot restart in the "current" page; for example, if a
		 * thread is appending to the tree, the page it's waiting for
		 * did an insert-split into the parent, then the parent split
		 * into its parent, the name space we are searching for may have
		 * moved above the current page in the tree.
		 *
		 * On other error, simply return, the swap call ensures we're
		 * holding nothing on failure.
		 */
		 /*进行下一级页读取，如果有限制，先从内存中淘汰正在操作的page,如果正要读取的page在splits,
		 那么我们从新检索当前(current)的page*/
		 //这里面会调用__wt_page_alloc分配page leaf
		if ((ret = __wt_page_swap( //current和descent交换，下次循环就从子ref对应的page descent = pindex->index[base - 1]开始
		    session, current, descent, WT_READ_RESTART_OK)) == 0) {
			current = descent; 
			continue;
		}
        
		if (ret == WT_RESTART) /*读取失败，从新再试*/
			goto restart;
		return (ret);
	}

	/* Track how deep the tree gets. */
	/*检索超过了btree的最大层级，那么扩大最大层级限制*/
	if (depth > btree->maximum_depth)
		btree->maximum_depth = depth;

leaf_only: //到叶子节点了
	page = current->page;
	cbt->ref = current; //current指向了leaf page，每个leaf page多个KV，继续在leaf page查找

	/*
	 * Clear current now that we have moved the reference into the btree
	 * cursor, so that cleanup never releases twice.
	 */
	current = NULL;

	/*
	 * In the case of a right-side tree descent during an insert, do a fast
	 * check for an append to the page, try to catch cursors appending data
	 * into the tree.
	 *
	 * It's tempting to make this test more rigorous: if a cursor inserts
	 * randomly into a two-level tree (a root referencing a single child
	 * that's empty except for an insert list), the right-side descent flag
	 * will be set and this comparison wasted.  The problem resolves itself
	 * as the tree grows larger: either we're no longer doing right-side
	 * descent, or we'll avoid additional comparisons in internal pages,
	 * making up for the wasted comparison here.  Similarly, the cursor's
	 * history is set any time it's an insert and a right-side descent,
	 * both to avoid a complicated/expensive test, and, in the case of
	 * multiple threads appending to the tree, we want to mark them all as
	 * appending, even if this test doesn't work.
	 */
	if (insert && descend_right) {
		cbt->append_tree = 1;

		if (page->entries == 0) {
			cbt->slot = WT_ROW_SLOT(page, page->pg_row);

			F_SET(cbt, WT_CBT_SEARCH_SMALLEST);
			ins_head = WT_ROW_INSERT_SMALLEST(page);
		} else {
			cbt->slot = WT_ROW_SLOT(page,
			    page->pg_row + (page->entries - 1));

			ins_head = WT_ROW_INSERT_SLOT(page, cbt->slot);
		}
		WT_ERR(__search_insert_append(
		    session, cbt, ins_head, srch_key, &done));
		if (done) //已经定位到srch_key要插入的位置，直接返回  
			return (0);
	}

	/*
	 * Binary search of an leaf page. There are three versions (keys with
	 * no application-specified collation order, in long and short versions,
	 * and keys with an application-specified collation order), because
	 * doing the tests and error handling inside the loop costs about 5%.
	 */
	//匹配查找，找到leaf中有该key，则cbt->slot代表该key在pg_row[]中的位置
	//没查到要么limit为0，要么查找遍了limit范围也没找到  
	base = 0;
	limit = page->entries;//二分查找的最大边界

	//注意leaf查找是根据pg_row中查找的, pg_row中存储的是已经罗盘的KV(因为每个page不可能全部都在内存，内存没那么多)
	//先从pg_row对应的磁盘找对应的key
	if (collator == NULL && srch_key->size <= WT_COMPARE_SHORT_MAXLEN)
		for (; limit != 0; limit >>= 1) {
			indx = base + (limit >> 1); //右移一位代表除2，用于二分查找
			rip = page->pg_row + indx; 
			WT_ERR( /*获取row store的叶子节ref的KEY值*/
			    __wt_row_leaf_key(session, page, rip, item, true));

			cmp = __wt_lex_compare_short(srch_key, item);
			if (cmp > 0) { //srch_key比item大，则继续查找
				base = indx + 1;
				--limit;
			} else if (cmp == 0)
				goto leaf_match;
		}
	else if (collator == NULL)
		for (; limit != 0; limit >>= 1) {
			indx = base + (limit >> 1);
			rip = page->pg_row + indx;
			WT_ERR(
			    __wt_row_leaf_key(session, page, rip, item, true));

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
	else
		for (; limit != 0; limit >>= 1) {
			indx = base + (limit >> 1);
			rip = page->pg_row + indx;
			WT_ERR(
			    __wt_row_leaf_key(session, page, rip, item, true));

			WT_ERR(__wt_compare(
			    session, collator, srch_key, item, &cmp));
			if (cmp > 0) {
				base = indx + 1;
				--limit;
			} else if (cmp == 0)
				goto leaf_match;
		}

	/*
	 * The best case is finding an exact match in the leaf page's WT_ROW
	 * array, probable for any read-mostly workload.  Check that case and
	 * get out fast.
	 */
	if (0) {
leaf_match:	cbt->compare = 0; //该key在磁盘上通过这里返回，如果不在磁盘，在内存中则在后面返回
		cbt->slot = WT_ROW_SLOT(page, rip); //在page->pg_row对应的磁盘中，找到了leaf中有该key
		return (0);
	}

	/*
	 * We didn't find an exact match in the WT_ROW array.
	 *
	 * Base is the smallest index greater than key and may be the 0th index
	 * or the (last + 1) index.  Set the slot to be the largest index less
	 * than the key if that's possible (if base is the 0th index it means
	 * the application is inserting a key before any key found on the page).
	 *
	 * It's still possible there is an exact match, but it's on an insert
	 * list.  Figure out which insert chain to search and then set up the
	 * return information assuming we'll find nothing in the insert list
	 * (we'll correct as needed inside the search routine, depending on
	 * what we find).
	 *
	 * If inserting a key smaller than any key found in the WT_ROW array,
	 * use the extra slot of the insert array, otherwise the insert array
	 * maps one-to-one to the WT_ROW array.
	 */
	//可能KV还没有罗盘，则会记录page内存中，也就是到mod_row_insert[]对应的数组链中(买个KV对应一个WT_INSERT，并由跳跃表管理起来)
	//在叶子节点的第几个index上面
	if (base == 0) {
		cbt->compare = 1;
		cbt->slot = 0;

		F_SET(cbt, WT_CBT_SEARCH_SMALLEST);
		//(page)->modify->mod_row_insert[(page)->entries])
		ins_head = WT_ROW_INSERT_SMALLEST(page);
	} else { //说明查找了整个page->entries也没找到，则下次直接从base个后面插入
		cbt->compare = -1;
		cbt->slot = base - 1;

        //(page)->modify->mod_row_insert[slot])
		ins_head = WT_ROW_INSERT_SLOT(page, cbt->slot);
	}

	/* If there's no insert list, we're done. */
	if (WT_SKIP_FIRST(ins_head) == NULL) //如果该leaf page还没有KV(如第一次忘wiredtiger中insert kv)一般从这里返回 
		return (0);

    /*
    下面确定该KEY应该放到管理WT_INSERT(即一个KV)的跳跃表中的什么位置
    */
    
	/*
	 * Test for an append first when inserting onto an insert list, try to
	 * catch cursors repeatedly inserting at a single point.
	 */
	if (insert) {
		WT_ERR(__search_insert_append(
		    session, cbt, ins_head, srch_key, &done));
		if (done) //已经确定好新的insert kv对应在跳跃表中的位置，则返回
			return (0);
	}

    //更新删除等操作，确定该更新操作对应的kv在跳跃表中的位置
	WT_ERR(__wt_search_insert(session, cbt, ins_head, srch_key)); 

	return (0);

err:	WT_TRET(__wt_page_release(session, current, 0));
	return (ret);
}
