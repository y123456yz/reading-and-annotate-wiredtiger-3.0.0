/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

/*
 * __wt_page_modify_alloc --
 *     Allocate a page's modification structure.
 */
int
__wt_page_modify_alloc(WT_SESSION_IMPL *session, WT_PAGE *page)
{
    WT_DECL_RET;
    WT_PAGE_MODIFY *modify;

    WT_RET(__wt_calloc_one(session, &modify));

    /* Initialize the spinlock for the page. */
    WT_ERR(__wt_spin_init(session, &modify->page_lock, "btree page"));

    /*
     * Multiple threads of control may be searching and deciding to modify a page. If our modify
     * structure is used, update the page's memory footprint, else discard the modify structure,
     * another thread did the work.
     */
    if (__wt_atomic_cas_ptr(&page->modify, NULL, modify))
        __wt_cache_page_inmem_incr(session, page, sizeof(*modify));
    else
err:
        __wt_free(session, modify);
    return (ret);
}

/*
 * __wt_row_modify --
 *     Row-store insert, update and delete.
 */

// 普通更新和删除都会走到这里，modify_type标识更新的类型

//__cursor_row_search和__wt_row_modify的关系:
//  __cursor_row_search用于确定K在btree中的位置以及是否存在等
//  __wt_row_modify负责在__cursor_row_search找到的位置把新KV添加到btree合适位置
 
//__search_insert_append: 如果srch_key比调表中最大的key大，则记录最末尾KV的位置, 如果跳跃表上面还没有KV，则直接返回啥也不做
//__wt_search_insert: leaf page对应ins_head跳跃表上查找srch_key在跳跃表中的位置记录到cbt->next_stack[] cbt->ins_stack[]等中
//__wt_row_modify: 真正把KV添加到跳跃表中
int
__wt_row_modify(WT_CURSOR_BTREE *cbt, const WT_ITEM *key, const WT_ITEM *value, WT_UPDATE *upd_arg,
  //普通更新和删除都会走到这里，modify_type标识更新的类型
  u_int modify_type, 
  bool exclusive
#ifdef HAVE_DIAGNOSTIC
  ,
  bool restore
#endif
)
{
    WT_DECL_RET;
    WT_INSERT *ins;
    WT_INSERT_HEAD *ins_head, **ins_headp;
    WT_PAGE *page;
    WT_PAGE_MODIFY *mod;
    WT_SESSION_IMPL *session;
    WT_UPDATE *last_upd, *old_upd, *upd, **upd_entry;
    wt_timestamp_t prev_upd_ts;
    size_t ins_size, upd_size;
    uint32_t ins_slot;
    u_int i, skipdepth;
    bool added_to_txn, inserted_to_update_chain;

    ins = NULL;
    page = cbt->ref->page;
    session = CUR2S(cbt);
    last_upd = NULL;
    upd = upd_arg;
    prev_upd_ts = WT_TS_NONE;
    added_to_txn = inserted_to_update_chain = false;
    upd_size = 0;

    /*
     * We should have one of the following:
     * - A full update list to instantiate.
     * - An update to append to the existing update list.
     * - A key/value pair to create an update with and append to the update list.
     * - A key with no value to create a reserved or tombstone update to append to the update list.
     *
     * A "full update list" is distinguished from "an update" by checking whether it has a "next"
     * update. The modify type should only be set if no update list provided.
     */
    WT_ASSERT(session,
      ((modify_type == WT_UPDATE_RESERVE || modify_type == WT_UPDATE_TOMBSTONE) && value == NULL &&
        upd_arg == NULL) ||
        (!(modify_type == WT_UPDATE_RESERVE || modify_type == WT_UPDATE_TOMBSTONE) &&
          ((value == NULL && upd_arg != NULL) || (value != NULL && upd_arg == NULL))));
    WT_ASSERT(session, upd_arg == NULL || modify_type == WT_UPDATE_INVALID);

    /* If we don't yet have a modify structure, we'll need one. */
    WT_RET(__wt_page_modify_init(session, page));
    mod = page->modify;

    /*
     * Modify: allocate an update array as necessary, build a WT_UPDATE structure, and call a
     * serialized function to insert the WT_UPDATE structure.
     *
     * Insert: allocate an insert array as necessary, build a WT_INSERT and WT_UPDATE structure
     * pair, and call a serialized function to insert the WT_INSERT structure.
     */

    if (cbt->compare == 0) {
        //找出需要update或者remove的K对应的update位置，也就是修改前的value upd
        if (cbt->ins == NULL) {
            /* Allocate an update array as necessary. */
            WT_PAGE_ALLOC_AND_SWAP(session, page, mod->mod_row_update, upd_entry, page->entries);

            /* Set the WT_UPDATE array reference. */
            upd_entry = &mod->mod_row_update[cbt->slot];
        } else
            upd_entry = &cbt->ins->upd;

        if (upd_arg == NULL) {
            /* Make sure the modify can proceed. */
            WT_ERR(
              //获取该K的链表上的第一个V对应udp，并做一些检查，同时获取这个V的ts
              
              //如果udp链表上面的第一个有效V已经标记为WT_UPDATE_TOMBSTONE，则直接返回WT_NOTFOUND，这个删除V的type WT_UPDATE_TOMBSTONE写入在下面实现
              __wt_txn_modify_check(session, cbt, old_upd = *upd_entry, &prev_upd_ts, modify_type));

            /* Allocate a WT_UPDATE structure and transaction ID. */
            //如果value为NULL，也就是WT_CURSOR->remove操作删除一个key的时候，实际上是生成一个新的udp,udp的value长度为0,
            //  下次在__wt_txn_modify_check检查的适合如果发现该udp type为WT_UPDATE_TOMBSTONE，则回直接在__wt_txn_modify_check返回WT_NOTFOUND
            WT_ERR(__wt_upd_alloc(session, value, modify_type, &upd, &upd_size));
            //把生成新的udp之前的上一个V的更新时间戳保存起来
            upd->prev_durable_ts = prev_upd_ts;
            WT_ERR(__wt_txn_modify(session, upd));
            added_to_txn = true;

            /* Avoid WT_CURSOR.update data copy. */
            //记录当前正在操作的key的upd value信息到cbt->modify_update
            __wt_upd_value_assign(cbt->modify_update, upd);
        } else {
            /*
             * We only update history store records in three cases:
             *  1) Delete the record with a tombstone with WT_TS_NONE.
             *  2) Update the record's stop time point if the prepared update written to the data
             * store is committed.
             *  3) Reinsert an update that has been deleted by a prepared rollback.
             */
            WT_ASSERT(session,
              !WT_IS_HS(S2BT(session)->dhandle) ||
                (*upd_entry == NULL ||
                  ((*upd_entry)->type == WT_UPDATE_TOMBSTONE &&
                    (*upd_entry)->txnid == WT_TXN_NONE && (*upd_entry)->start_ts == WT_TS_NONE)) ||
                (upd_arg->type == WT_UPDATE_TOMBSTONE && upd_arg->start_ts == WT_TS_NONE &&
                  upd_arg->next == NULL) ||
                (upd_arg->type == WT_UPDATE_TOMBSTONE && upd_arg->next != NULL &&
                  upd_arg->next->type == WT_UPDATE_STANDARD && upd_arg->next->next == NULL));

            upd_size = __wt_update_list_memsize(upd);

            /* If there are existing updates, append them after the new updates. */
            for (last_upd = upd; last_upd->next != NULL; last_upd = last_upd->next)
                ;
            last_upd->next = *upd_entry;

            /*
             * If we restore an update chain in update restore eviction, there should be no update
             * on the existing update chain.
             */
            WT_ASSERT(session, !restore || *upd_entry == NULL);

            /*
             * We can either put multiple new updates or a single update on the update chain.
             *
             * Set the "old" entry to the second update in the list so that the serialization
             * function succeeds in swapping the first update into place.
             */
            if (upd->next != NULL)
                *upd_entry = upd->next;
            old_upd = *upd_entry;
        }

        /*
         * Point the new WT_UPDATE item to the next element in the list. If we get it right, the
         * serialization function lock acts as our memory barrier to flush this write.
         */
        //不管是remove还是update，都是新生成一个udp，新生成的upd next指向老的upd
        upd->next = old_upd;

        /* Serialize the update. */
        //也就是cbt->ins->upd---->udp, udp->next----------->old_upd
        //也就是在cbt->ins->upd链表头部增加一个前面新创建的udp
        WT_ERR(__wt_update_serial(session, cbt, page, upd_entry, &upd, upd_size, exclusive));
    } else {
        /*
         * Allocate the insert array as necessary.
         *
         * We allocate an additional insert array slot for insert keys sorting less than any key on
         * the page. The test to select that slot is baroque: if the search returned the first page
         * slot, we didn't end up processing an insert list, and the comparison value indicates the
         * search key was smaller than the returned slot, then we're using the smallest-key insert
         * slot. That's hard, so we set a flag.
         */
        WT_PAGE_ALLOC_AND_SWAP(session, page, mod->mod_row_insert, ins_headp, page->entries + 1);
        ins_slot = F_ISSET(cbt, WT_CBT_SEARCH_SMALLEST) ? page->entries : cbt->slot;
        ins_headp = &mod->mod_row_insert[ins_slot];

        /* Allocate the WT_INSERT_HEAD structure as necessary. */
        WT_PAGE_ALLOC_AND_SWAP(session, page, *ins_headp, ins_head, 1);
        ins_head = *ins_headp;

        /* Choose a skiplist depth for this insert. */
        //跳跃表level随机生成
        skipdepth = __wt_skip_choose_depth(session);

        /*
         * Allocate a WT_INSERT/WT_UPDATE pair and transaction ID, and update the cursor to
         * reference it (the WT_INSERT_HEAD might be allocated, the WT_INSERT was allocated).
         */
        //给该key分配对应WT_INSERT空间
        //KV中的key对应WT_INSERT，value对应WT_UPDATE
        WT_ERR(__wt_row_insert_alloc(session, key, skipdepth, &ins, &ins_size));
        cbt->ins_head = ins_head;
        cbt->ins = ins;

        if (upd_arg == NULL) {//value空间分配,
            //__wt_upd_alloc分配WT_UPDATE空间
            WT_ERR(__wt_upd_alloc(session, value, modify_type, &upd, &upd_size));
            WT_ERR(__wt_txn_modify(session, upd));
            added_to_txn = true;

            /* Avoid a data copy in WT_CURSOR.update. */
            __wt_upd_value_assign(cbt->modify_update, upd);
        } else {
            /*
             * We either insert a tombstone with a standard update or only a standard update to the
             * history store if we write a prepared update to the data store.
             */
            WT_ASSERT(session,
              !WT_IS_HS(S2BT(session)->dhandle) ||
                (upd_arg->type == WT_UPDATE_TOMBSTONE && upd_arg->next != NULL &&
                  upd_arg->next->type == WT_UPDATE_STANDARD && upd_arg->next->next == NULL) ||
                (upd_arg->type == WT_UPDATE_STANDARD && upd_arg->next == NULL));

            upd_size = __wt_update_list_memsize(upd);
        }

        ins->upd = upd;
        //整个KV消耗的总内存,K对应的V有多了一个upd
        ins_size += upd_size;

        /*
         * If there was no insert list during the search, the cursor's information cannot be
         * correct, search couldn't have initialized it.
         *
         * Otherwise, point the new WT_INSERT item's skiplist to the next elements in the insert
         * list (which we will check are still valid inside the serialization function).
         *
         * The serial mutex acts as our memory barrier to flush these writes before inserting them
         * into the list.
         */
        if (cbt->ins_stack[0] == NULL)
            for (i = 0; i < skipdepth; i++) {
                cbt->ins_stack[i] = &ins_head->head[i];
                ins->next[i] = cbt->next_stack[i] = NULL;
            }
        else
            for (i = 0; i < skipdepth; i++)
                ins->next[i] = cbt->next_stack[i];

        /* Insert the WT_INSERT structure. */
        WT_ERR(__wt_insert_serial(
          session, page, cbt->ins_head, cbt->ins_stack, &ins, ins_size, skipdepth, exclusive));
    }

    inserted_to_update_chain = true;

    /*
     * If the update was successful, add it to the in-memory log.
     *
     * We will enter this code if we are doing cursor operations (upd_arg == NULL). We may fail
     * here. However, we have already successfully inserted the updates to the update chain. In this
     * case, don't free the allocated memory in error handling. Leave them to the rollback logic to
     * do the cleanup.
     *
     * If we are calling for internal purposes (upd_arg != NULL), we skip this code. Therefore, we
     * cannot fail after we have inserted the updates to the update chain. The caller of this
     * function can safely free the updates if it receives an error return.
     */
    if (added_to_txn && modify_type != WT_UPDATE_RESERVE) {
        if (__wt_log_op(session))
            WT_ERR(__wt_txn_log_op(session, cbt));

        /*
         * Set the key in the transaction operation to be used in case this transaction is prepared
         * to retrieve the update corresponding to this operation.
         */
        WT_ERR(__wt_txn_op_set_key(session, key));
    }

    if (0) {
err:
        /*
         * Let the rollback logic to do the cleanup if we have inserted the update to the update
         * chain.
         */
        if (!inserted_to_update_chain) {
            /*
             * Remove the update from the current transaction, don't try to modify it on rollback.
             */
            if (added_to_txn)
                __wt_txn_unmodify(session);

            /* Free any allocated insert list object. */
            __wt_free(session, ins);

            cbt->ins = NULL;

            /* Discard any allocated update, unless we failed after linking it into page memory. */
            if (upd_arg == NULL)
                __wt_free(session, upd);

            /*
             * When prepending a list of updates to an update chain, we link them together; sever
             * that link so our callers list doesn't point into page memory.
             */
            if (last_upd != NULL)
                last_upd->next = NULL;
        }
    }

    return (ret);
}

/*
 * __wt_row_insert_alloc --
 *     Row-store insert: allocate a WT_INSERT structure and fill it in.
 跳跃表图解参考https://www.jb51.net/article/199510.htm
 */
//可以配合https://www.jb51.net/article/199510.htm 图片阅读
int
__wt_row_insert_alloc(WT_SESSION_IMPL *session, const WT_ITEM *key, u_int skipdepth,
  WT_INSERT **insp, size_t *ins_sizep)
{
    WT_INSERT *ins;
    size_t ins_size;

    /*
     * Allocate the WT_INSERT structure, next pointers for the skip list, and room for the key. Then
     * copy the key into place.
     */
    //WT_INSERT头部+level空间+真实数据key
    ins_size = sizeof(WT_INSERT) + skipdepth * sizeof(WT_INSERT *) + key->size;
    WT_RET(__wt_calloc(session, 1, ins_size, &ins));

    ins->u.key.offset = WT_STORE_SIZE(ins_size - key->size);
    WT_INSERT_KEY_SIZE(ins) = WT_STORE_SIZE(key->size);
    memcpy(WT_INSERT_KEY(ins), key->data, key->size);

    *insp = ins;
    if (ins_sizep != NULL)
        *ins_sizep = ins_size;
    return (0);
}

/*
 * __wt_update_obsolete_check --
 *     Check for obsolete updates and force evict the page if the update list is too long.
 //释放链表上已过时的udp，注意这里面只做了内存计数的减法，真正是在调用该函数的地方释放过时udp
 */
WT_UPDATE *
__wt_update_obsolete_check(
  WT_SESSION_IMPL *session, WT_CURSOR_BTREE *cbt, WT_UPDATE *upd, bool update_accounting)
{
    WT_PAGE *page;
    WT_TXN_GLOBAL *txn_global;
    WT_UPDATE *first, *next;
    size_t size;
    uint64_t oldest, stable;
    u_int count, upd_seen, upd_unstable;

    next = NULL;
    page = cbt->ref->page;
    txn_global = &S2C(session)->txn_global;

    upd_seen = upd_unstable = 0;
    oldest = txn_global->has_oldest_timestamp ? txn_global->oldest_timestamp : WT_TS_NONE;
    stable = txn_global->has_stable_timestamp ? txn_global->stable_timestamp : WT_TS_NONE;
    /*
     * This function identifies obsolete updates, and truncates them from the rest of the chain;
     * because this routine is called from inside a serialization function, the caller has
     * responsibility for actually freeing the memory.
     *
     * Walk the list of updates, looking for obsolete updates at the end.
     *
     * Only updates with globally visible, self-contained data can terminate update chains.
     *  
     * count表示该K对应的v历史版本数量
     */
    for (first = NULL, count = 0; upd != NULL; upd = upd->next, count++) {
        if (upd->txnid == WT_TXN_ABORTED)
            continue;

        //printf("yang test ........................... data:%s\r\n", upd->data);
        ++upd_seen;
        // 判断upd是否对当前所有session可见，这里一般是__wt_txn_update_oldest更新后，__wt_txn_update_oldest之前的事务才全局可见
        if (__wt_txn_upd_visible_all(session, upd)) {
            //first指向K的udp链表上第一个全局可见的V
            if (first == NULL && WT_UPDATE_DATA_VALUE(upd))
                first = upd;
        } else {
            first = NULL;
            /*
             * While we're here, also check for the update being kept only for timestamp history to
             * gauge updates being kept due to history.
             */
            if (upd->start_ts != WT_TS_NONE && upd->start_ts >= oldest && upd->start_ts < stable)
                ++upd_unstable;
        }

        /* Cannot truncate the updates if we need to remove the updates from the history store. */
        if (F_ISSET(upd, WT_UPDATE_TO_DELETE_FROM_HS))
            first = NULL;
    }
    //if (strcmp(session->name, "WT_CURSOR.__curfile_update") == 0)
    //    WT_IGNORE_RET(__wt_msg(session, "yang test ...1...........__wt_update_obsolete_check...%p, %p page:%p..count:%d...page->memory_footprint:%lu\r\n", 
    //        first, first->next, page, (int)count, page->memory_footprint));

    /*
     * We cannot discard this WT_UPDATE structure, we can only discard WT_UPDATE structures
     * subsequent to it, other threads of control will terminate their walk in this element. Save a
     * reference to the list we will discard, and terminate the list.
     */
    //if (first)
    //    printf("yang test ........................... first data:%s\r\n", first->data);

    //if (first && first->next)
    //    printf("yang test ........................... next data:%s\r\n", first->next->data);
                
    //udp链表上first之后的V都过时了，可以清理掉，这里只做了内存使用量减去这些过时的V内存，没用对这些过时的V真正释放内存
    if (first != NULL && (next = first->next) != NULL &&
      //first->next指向Null, next也就指向需要释放的udp链表，这里用原子操作可以避免锁开销
      __wt_atomic_cas_ptr(&first->next, next, NULL)) {
        /*
         * Decrement the dirty byte count while holding the page lock, else we can race with
         * checkpoints cleaning a page.
         */
        if (update_accounting) {
            for (size = 0, upd = next; upd != NULL; upd = upd->next)
                size += WT_UPDATE_MEMSIZE(upd);
            if (size != 0) {
                __wt_cache_page_inmem_decr(session, page, size);
                if (strcmp(session->name, "WT_CURSOR.__curfile_update") == 0)
                    WT_IGNORE_RET(__wt_msg(session, "yang test ...2...........__wt_update_obsolete_check........page->memory_footprint:%lu\r\n", 
                        page->memory_footprint));
            }
        }
    }

    /*
     * Force evict a page when there are more than WT_THOUSAND updates to a single item. Increasing
     * the minSnapshotHistoryWindowInSeconds to 300 introduced a performance regression in which the
     * average number of updates on a single item was approximately 1000 in write-heavy workloads.
     * This is why we use WT_THOUSAND here.
     */
    //如果一个page上任何一个key的历史value超过1000个，也就是对一个key更新了1000次，则需要强制让evict线程淘汰掉该page
    if (count > WT_THOUSAND) {
        WT_STAT_CONN_INCR(session, cache_eviction_force_long_update_list);
        __wt_page_evict_soon(session, cbt->ref);
    }

    //next链表在外层进行真正的内存释放
    if (next != NULL)
        return (next);

    /*
     * If the list is long, don't retry checks on this page until the transaction state has moved
     * forwards. This function is used to trim update lists independently of the page state, ensure
     * there is a modify structure.
     */
    if (count > 20 && page->modify != NULL) {
        //这里增加obsolete_check_txn和obsolete_check_timestamp的目的是避免__wt_update_serial下次再WT_PAGE_TRYLOCK等待锁然后
        //  对该page做__wt_update_obsolete_check检查
        page->modify->obsolete_check_txn = txn_global->last_running;
        if (txn_global->has_pinned_timestamp)
            page->modify->obsolete_check_timestamp = txn_global->pinned_timestamp;
    }

    return (NULL);
}
