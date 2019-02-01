/*-
 * Copyright (c) 2014-2017 MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

/*
 * __wt_page_modify_alloc --
 *	Allocate a page's modification structure.
 */ //分配一个modify  赋值给page->modify
int
__wt_page_modify_alloc(WT_SESSION_IMPL *session, WT_PAGE *page)
{
	WT_DECL_RET;
	WT_PAGE_MODIFY *modify;

	WT_RET(__wt_calloc_one(session, &modify));

	/* Initialize the spinlock for the page. */
	WT_ERR(__wt_spin_init(session, &modify->page_lock, "btree page"));

	/*
	 * Multiple threads of control may be searching and deciding to modify
	 * a page.  If our modify structure is used, update the page's memory
	 * footprint, else discard the modify structure, another thread did the
	 * work.
	 */
	//modify赋值给page->modify
	if (__wt_atomic_cas_ptr(&page->modify, NULL, modify))
		__wt_cache_page_inmem_incr(session, page, sizeof(*modify));
	else
err:		__wt_free(session, modify);
	return (ret);
}

/*
 * __wt_row_modify --
 *	Row-store insert, update and delete.
 */
/*row store btree的修改实现，包括:insert, update和delete */
int
__wt_row_modify(WT_SESSION_IMPL *session, WT_CURSOR_BTREE *cbt,
    const WT_ITEM *key, const WT_ITEM *value,
    WT_UPDATE *upd_arg, u_int modify_type, bool exclusive)
{
	WT_DECL_RET;
	WT_INSERT *ins;
	WT_INSERT_HEAD *ins_head, **ins_headp;
	WT_PAGE *page;
	WT_PAGE_MODIFY *mod;
	WT_UPDATE *old_upd, *upd, **upd_entry;
	size_t ins_size, upd_size;
	uint32_t ins_slot;
	u_int i, skipdepth;
	bool logged;

	ins = NULL;
	
	page = cbt->ref->page;
	upd = upd_arg;
	logged = false;

	/* If we don't yet have a modify structure, we'll need one. */
	/*分配并初始化一个page modify对象*/
	//分配一个modify  赋值给page->modify
	WT_RET(__wt_page_modify_init(session, page));
	mod = page->modify;

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

	/*
	 * Modify: allocate an update array as necessary, build a WT_UPDATE
	 * structure, and call a serialized function to insert the WT_UPDATE
	 * structure.
	 *
	 * Insert: allocate an insert array as necessary, build a WT_INSERT
	 * and WT_UPDATE structure pair, and call a serialized function to
	 * insert the WT_INSERT structure.
	 */ /* 修改操作，说明前面的__cursor_row_search key的时候btree中有该key */
	if (cbt->compare == 0) {
		if (cbt->ins == NULL) { //如果打开的cursor，reset后重新打开cursor做update，则ins=null
			/* Allocate an update array as necessary. */
			WT_PAGE_ALLOC_AND_SWAP(session, page,
			    mod->mod_row_update, upd_entry, page->entries);
            /*是在update list当中找到修改的记录，直接获取udpate list当中对应的update对象*/
			/* 为cbt游标分配一个update对象数组,并设置update数组槽位，原子操作性的分配*/
			/* Set the WT_UPDATE array reference. */
			upd_entry = &mod->mod_row_update[cbt->slot];
		} else /* 更新会走这个分支，同一个KV(对应一个WT_INSERT)，key一样，每次update则把新的WT_UPDATE添加到ins->upd头部*/
			upd_entry = &cbt->ins->upd; //如果是update，则新的value直接放入ins->upd头部

		if (upd_arg == NULL) { //update和insert udp_arg都是等于NULL
			/* Make sure the update can proceed. */
			//查询事务生效的地方在这里 
			WT_ERR(__wt_txn_update_check( //检查该session对应的事务id是否可见，不可见直接返回
			    session, old_upd = *upd_entry)); //old_upd现在变为更新前之前的udp了

			/* Allocate a WT_UPDATE structure and transaction ID. */
			WT_ERR(__wt_update_alloc(session,
			    value, &upd, &upd_size, modify_type));  //构建一个新的upd,
			WT_ERR(__wt_txn_modify(session, upd)); //事务相关   
			logged = true;

			/* Avoid WT_CURSOR.update data copy. */
			cbt->modify_update = upd;
		} else {
			upd_size = __wt_update_list_memsize(upd);

			/*
			 * We are restoring updates that couldn't be evicted,
			 * there should only be one update list per key.
			 */
			WT_ASSERT(session, *upd_entry == NULL);

			/*
			 * Set the "old" entry to the second update in the list
			 * so that the serialization function succeeds in
			 * swapping the first update into place.
			 */
			old_upd = *upd_entry = upd->next;
		}

		/*
		 * Point the new WT_UPDATE item to the next element in the list.
		 * If we get it right, the serialization function lock acts as
		 * our memory barrier to flush this write.
		 */
		//添加到
		upd->next = old_upd;

		/* Serialize the update. */
		/*进行串行更新操作， 也就是新的udp放到update链表的最前面，也就是最新的数据*/
		WT_ERR(__wt_update_serial(
		    session, page, upd_entry, &upd, upd_size, exclusive));
	} else {/*没有定位到具体的记录，那么相当于插入一个新的记录行*/
		/*
		 * Allocate the insert array as necessary.
		 *
		 * We allocate an additional insert array slot for insert keys
		 * sorting less than any key on the page.  The test to select
		 * that slot is baroque: if the search returned the first page
		 * slot, we didn't end up processing an insert list, and the
		 * comparison value indicates the search key was smaller than
		 * the returned slot, then we're using the smallest-key insert
		 * slot.  That's hard, so we set a flag.
		 */ /*分配一个insert head数组*/
		 //分配page->entries + 1个WT_INSERT_HEAD，然后赋值给mod->mod_row_insert
		WT_PAGE_ALLOC_AND_SWAP(session, page,
		    mod->mod_row_insert, ins_headp, page->entries + 1); 

		ins_slot = F_ISSET(cbt, WT_CBT_SEARCH_SMALLEST) ?
		    page->entries: cbt->slot; 
		ins_headp = &mod->mod_row_insert[ins_slot];

		/* Allocate the WT_INSERT_HEAD structure as necessary. */
		WT_PAGE_ALLOC_AND_SWAP(session, page, *ins_headp, ins_head, 1);
		ins_head = *ins_headp;

		/* Choose a skiplist depth for this insert. */
		skipdepth = __wt_skip_choose_depth(session);

		/*
		 * Allocate a WT_INSERT/WT_UPDATE pair and transaction ID, and
		 * update the cursor to reference it (the WT_INSERT_HEAD might
		 * be allocated, the WT_INSERT was allocated).
		 */
		//key存入ins
		WT_ERR(__wt_row_insert_alloc(
		    session, key, skipdepth, &ins, &ins_size));
		cbt->ins_head = ins_head;
		cbt->ins = ins;

		if (upd_arg == NULL) { //查询的时候默认为NLL，
		    //value存入upd
			WT_ERR(__wt_update_alloc(session,
			    value, &upd, &upd_size, modify_type));
			WT_ERR(__wt_txn_modify(session, upd));
			logged = true;

			/* Avoid WT_CURSOR.update data copy. */
			cbt->modify_update = upd;
		} else
			upd_size = __wt_update_list_memsize(upd);

        //最终key value都存入ins, key在ins->u, update放入ins->update   
        //只有第一次插入某key的时候才会到这里，如果后期继续更新该key，则在上面的cbt->compare=0中处理，因为
        //外层的__cursor_row_search查找的时候会发现btree中有该key存在，则cbt->compare=0
		ins->upd = upd; //一个新的KV对应一个WT_INSERT
		ins_size += upd_size;
            
        //所有的insert(ins)通过page->mod->mod_row_insert[]跳跃表组织管理起来
		/*
		 * If there was no insert list during the search, the cursor's
		 * information cannot be correct, search couldn't have
		 * initialized it.
		 *
		 * Otherwise, point the new WT_INSERT item's skiplist to the
		 * next elements in the insert list (which we will check are
		 * still valid inside the serialization function).
		 *
		 * The serial mutex acts as our memory barrier to flush these
		 * writes before inserting them into the list.
		 */
		if (cbt->ins_stack[0] == NULL)
			for (i = 0; i < skipdepth; i++) {
				cbt->ins_stack[i] = &ins_head->head[i];
				ins->next[i] = cbt->next_stack[i] = NULL;
			}
		else
			for (i = 0; i < skipdepth; i++)
				ins->next[i] = cbt->next_stack[i];

        //新的WT_INSERT通过跳跃表管理起来
		/* Insert the WT_INSERT structure. */
		WT_ERR(__wt_insert_serial(
		    session, page, cbt->ins_head, cbt->ins_stack,
		    &ins, ins_size, skipdepth, exclusive));
	}

    //日志记录,在该函数外层的 (__curtable_insert __curtable_update等)->CURSOR_UPDATE_API_END(session, ret);中写入日志文件
	if (logged && modify_type != WT_UPDATE_RESERVED)
		WT_ERR(__wt_txn_log_op(session, cbt));

	if (0) {
err:		/*
		 * Remove the update from the current transaction, so we don't
		 * try to modify it on rollback.
		 */
		if (logged)
			__wt_txn_unmodify(session);
		__wt_free(session, ins);
		cbt->ins = NULL;
		if (upd_arg == NULL)
			__wt_free(session, upd);
	}

	return (ret);
}

/*
 * __wt_row_insert_alloc --
 *	Row-store insert: allocate a WT_INSERT structure and fill it in.
 */
/* 分配一个row insert的WT_INSERT对象，拷贝key到该对象中 */
int
__wt_row_insert_alloc(WT_SESSION_IMPL *session,
    const WT_ITEM *key, u_int skipdepth, WT_INSERT **insp, size_t *ins_sizep)
{
	WT_INSERT *ins;
	size_t ins_size;

	/*
	 * Allocate the WT_INSERT structure, next pointers for the skip list,
	 * and room for the key.  Then copy the key into place.
	 */
	ins_size = sizeof(WT_INSERT) +
	    skipdepth * sizeof(WT_INSERT *) + key->size;
	WT_RET(__wt_calloc(session, 1, ins_size, &ins));

    /*确定key的起始偏移位置*/
	ins->u.key.offset = WT_STORE_SIZE(ins_size - key->size);
	/*设置key的长度*/
	WT_INSERT_KEY_SIZE(ins) = WT_STORE_SIZE(key->size);
	/*key值的拷贝*/
	memcpy(WT_INSERT_KEY(ins), key->data, key->size);

	*insp = ins;
	if (ins_sizep != NULL)
		*ins_sizep = ins_size;
	return (0);
}

/*
 * __wt_update_alloc --
 *	Allocate a WT_UPDATE structure and associated value and fill it in.
 */
/* 分配一个row update的WT_UPDATE对象, value = NULL表示delete操作, 通过updp返回 */
int
__wt_update_alloc(WT_SESSION_IMPL *session, const WT_ITEM *value,
    WT_UPDATE **updp, size_t *sizep, u_int modify_type)
{
	WT_UPDATE *upd;

	*updp = NULL;

	/*
	 * The code paths leading here are convoluted: assert we never attempt
	 * to allocate an update structure if only intending to insert one we
	 * already have.
	 */
	WT_ASSERT(session, modify_type != WT_UPDATE_INVALID);

	/*
	 * Allocate the WT_UPDATE structure and room for the value, then copy
	 * the value into place.
	 */
	if (modify_type == WT_UPDATE_DELETED ||  //删除的话不用拷贝数据了
	    modify_type == WT_UPDATE_RESERVED)
		WT_RET(__wt_calloc(session, 1, WT_UPDATE_SIZE, &upd));
	else {
		WT_RET(__wt_calloc(
		    session, 1, WT_UPDATE_SIZE + value->size, &upd));
		if (value->size != 0) {
			upd->size = WT_STORE_SIZE(value->size);
			memcpy(upd->data, value->data, value->size);
		}
	}
	upd->type = (uint8_t)modify_type;

	*updp = upd;
	*sizep = WT_UPDATE_MEMSIZE(upd);
	return (0);
}

/*
 * __wt_update_obsolete_check --
 *	Check for obsolete updates.
 *//* 检查过期废弃的update，只有下最近一个session能看到的update版本，前面处于rollback的版本作为废弃数据 */
WT_UPDATE *
__wt_update_obsolete_check(
    WT_SESSION_IMPL *session, WT_PAGE *page, WT_UPDATE *upd)
{
	WT_TXN_GLOBAL *txn_global;
	WT_UPDATE *first, *next;
	u_int count;

	txn_global = &S2C(session)->txn_global;

	/*
	 * This function identifies obsolete updates, and truncates them from
	 * the rest of the chain; because this routine is called from inside
	 * a serialization function, the caller has responsibility for actually
	 * freeing the memory.
	 *
	 * Walk the list of updates, looking for obsolete updates at the end.
	 *
	 * Only updates with globally visible, self-contained data can terminate
	 * update chains.
	 */
	for (first = NULL, count = 0; upd != NULL; upd = upd->next, count++) {
		if (upd->txnid == WT_TXN_ABORTED)
			continue;
		if (!__wt_txn_upd_visible_all(session, upd))
			first = NULL;
		else if (first == NULL && WT_UPDATE_DATA_VALUE(upd))
			first = upd;
	}

	/*
	 * We cannot discard this WT_UPDATE structure, we can only discard
	 * WT_UPDATE structures subsequent to it, other threads of control will
	 * terminate their walk in this element.  Save a reference to the list
	 * we will discard, and terminate the list.
	 */
	if (first != NULL &&
	    (next = first->next) != NULL &&
	    __wt_atomic_cas_ptr(&first->next, next, NULL))
		return (next);

	/*
	 * If the list is long, don't retry checks on this page until the
	 * transaction state has moved forwards. This function is used to
	 * trim update lists independently of the page state, ensure there
	 * is a modify structure.
	 */
	if (count > 20 && page->modify != NULL) {
		page->modify->obsolete_check_txn = txn_global->last_running;
#ifdef HAVE_TIMESTAMPS
		if (txn_global->has_pinned_timestamp)
			WT_WITH_TIMESTAMP_READLOCK(session, &txn_global->rwlock,
			    __wt_timestamp_set(
				&page->modify->obsolete_check_timestamp,
				&txn_global->pinned_timestamp));
#endif
	}

	return (NULL);
}

/*
 * __wt_update_obsolete_free --
 *	Free an obsolete update list.
 */
void
__wt_update_obsolete_free(
    WT_SESSION_IMPL *session, WT_PAGE *page, WT_UPDATE *upd)
{
	WT_UPDATE *next;
	size_t size;

	/* Free a WT_UPDATE list. */
	for (size = 0; upd != NULL; upd = next) {
		next = upd->next;
		size += WT_UPDATE_MEMSIZE(upd);
		__wt_free(session, upd);
	}
	if (size != 0)
		__wt_cache_page_inmem_decr(session, page, size);
}
