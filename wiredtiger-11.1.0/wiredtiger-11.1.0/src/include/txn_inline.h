/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

/*
 * __wt_txn_context_prepare_check --
 *     Return an error if the current transaction is in the prepare state.
 */
static inline int
__wt_txn_context_prepare_check(WT_SESSION_IMPL *session)
{
    if (F_ISSET(session->txn, WT_TXN_PREPARE_IGNORE_API_CHECK))
        return (0);
    if (F_ISSET(session->txn, WT_TXN_PREPARE))
        WT_RET_MSG(session, EINVAL, "not permitted in a prepared transaction");
    return (0);
}

/*
 * __wt_txn_context_check --
 *     Complain if a transaction is/isn't running.
 */
static inline int
__wt_txn_context_check(WT_SESSION_IMPL *session, bool requires_txn)
{
    if (requires_txn && !F_ISSET(session->txn, WT_TXN_RUNNING))
        WT_RET_MSG(session, EINVAL, "only permitted in a running transaction");
    if (!requires_txn && F_ISSET(session->txn, WT_TXN_RUNNING))
        WT_RET_MSG(session, EINVAL, "not permitted in a running transaction");
    return (0);
}

/*
 * __wt_txn_err_set --
 *     Set an error in the current transaction.
 */
static inline void
__wt_txn_err_set(WT_SESSION_IMPL *session, int ret)
{
    WT_TXN *txn;

    txn = session->txn;

    /*  Ignore standard errors that don't fail the transaction. */
    if (ret == WT_NOTFOUND || ret == WT_DUPLICATE_KEY || ret == WT_PREPARE_CONFLICT)
        return;

    /* Less commonly, it's not a running transaction. */
    if (!F_ISSET(txn, WT_TXN_RUNNING))
        return;

    /* The transaction has to be rolled back. */
    F_SET(txn, WT_TXN_ERROR);

    /*
     * Check for a prepared transaction, and quit: we can't ignore the error and we can't roll back
     * a prepared transaction.
     */
    if (F_ISSET(txn, WT_TXN_PREPARE))
        WT_IGNORE_RET(__wt_panic(session, ret,
          "transactional error logged after transaction was prepared, failing the system"));
}

/*
 * __wt_txn_op_set_recno --
 *     Set the latest transaction operation with the given recno.
 */
static inline void
__wt_txn_op_set_recno(WT_SESSION_IMPL *session, uint64_t recno)
{
    WT_TXN *txn;
    WT_TXN_OP *op;

    txn = session->txn;

    WT_ASSERT(session, txn->mod_count > 0 && recno != WT_RECNO_OOB);
    op = txn->mod + txn->mod_count - 1;

    if (WT_SESSION_IS_CHECKPOINT(session) || WT_IS_HS(op->btree->dhandle) ||
      WT_IS_METADATA(op->btree->dhandle))
        return;

    WT_ASSERT(session, op->type == WT_TXN_OP_BASIC_COL || op->type == WT_TXN_OP_INMEM_COL);

    /*
     * Copy the recno into the transaction operation structure, so when update is evicted to the
     * history store, we have a chance of finding it again. Even though only prepared updates can be
     * evicted, at this stage we don't know whether this transaction will be prepared or not, hence
     * we are copying the key for all operations, so that we can use this key to fetch the update in
     * case this transaction is prepared.
     */
    op->u.op_col.recno = recno;
}

/*
 * __wt_txn_op_set_key --
 *     Set the latest transaction operation with the given key.
 */
static inline int
__wt_txn_op_set_key(WT_SESSION_IMPL *session, const WT_ITEM *key)
{
    WT_TXN *txn;
    WT_TXN_OP *op;

    txn = session->txn;

    WT_ASSERT(session, txn->mod_count > 0 && key->data != NULL);

    op = txn->mod + txn->mod_count - 1;

    if (WT_SESSION_IS_CHECKPOINT(session) || WT_IS_HS(op->btree->dhandle) ||
      WT_IS_METADATA(op->btree->dhandle))
        return (0);

    WT_ASSERT(session, op->type == WT_TXN_OP_BASIC_ROW || op->type == WT_TXN_OP_INMEM_ROW);

    /*
     * Copy the key into the transaction operation structure, so when update is evicted to the
     * history store, we have a chance of finding it again. Even though only prepared updates can be
     * evicted, at this stage we don't know whether this transaction will be prepared or not, hence
     * we are copying the key for all operations, so that we can use this key to fetch the update in
     * case this transaction is prepared.
     */
    return (__wt_buf_set(session, &op->u.op_row.key, key->data, key->size));
}

/*
 * __txn_resolve_prepared_update --
 *     Resolve a prepared update as committed update.
 //把upd->prepare_state设置为WT_PREPARE_RESOLVED
 */
static inline void
__txn_resolve_prepared_update(WT_SESSION_IMPL *session, WT_UPDATE *upd)
{
    WT_TXN *txn;

    txn = session->txn;
    /*
     * In case of a prepared transaction, the order of modification of the prepare timestamp to
     * commit timestamp in the update chain will not affect the data visibility, a reader will
     * encounter a prepared update resulting in prepare conflict.
     *
     * As updating timestamp might not be an atomic operation, we will manage using state.
     */
    upd->prepare_state = WT_PREPARE_LOCKED;
    WT_WRITE_BARRIER();
    upd->start_ts = txn->commit_timestamp;
    upd->durable_ts = txn->durable_timestamp;
    WT_PUBLISH(upd->prepare_state, WT_PREPARE_RESOLVED);
}

/*
 * __txn_next_op --
 *     Mark a WT_UPDATE object modified by the current transaction.
 //__wt_txn_modify->__txn_next_op获取一个事务的op成员，并对op赋值等
 */
static inline int
__txn_next_op(WT_SESSION_IMPL *session, WT_TXN_OP **opp)
{
    WT_TXN *txn;
    WT_TXN_OP *op;

    *opp = NULL;

    txn = session->txn;

    /*
     * We're about to perform an update. Make sure we have allocated a transaction ID.
     */
    WT_RET(__wt_txn_id_check(session));
    WT_ASSERT(session, F_ISSET(txn, WT_TXN_HAS_ID));

    WT_RET(__wt_realloc_def(session, &txn->mod_alloc, txn->mod_count + 1, &txn->mod));

    op = &txn->mod[txn->mod_count++];
    WT_CLEAR(*op);
    op->btree = S2BT(session);
    (void)__wt_atomic_addi32(&session->dhandle->session_inuse, 1);
    *opp = op;
    return (0);
}

/*
 * __wt_txn_unmodify --
 *     If threads race making updates, they may discard the last referenced WT_UPDATE item while the
 *     transaction is still active. This function removes the last update item from the "log".
 */
//把一个WT_TXN_OP从mod_count[]数组中删除
static inline void
__wt_txn_unmodify(WT_SESSION_IMPL *session)
{
    WT_TXN *txn;
    WT_TXN_OP *op;

    txn = session->txn;
    if (F_ISSET(txn, WT_TXN_HAS_ID)) {
        WT_ASSERT(session, txn->mod_count > 0);
        --txn->mod_count;
        op = txn->mod + txn->mod_count;
        __wt_txn_op_free(session, op);
    }
}

/*
 * __wt_txn_op_delete_apply_prepare_state --
 *     Apply the correct prepare state and the timestamp to the ref and to any updates in the page
 *     del update list.  
 */
static inline void
__wt_txn_op_delete_apply_prepare_state(WT_SESSION_IMPL *session, WT_REF *ref, bool commit)
{
    WT_PAGE_DELETED *page_del;
    WT_TXN *txn;
    WT_UPDATE **updp;
    wt_timestamp_t ts;
    uint8_t prepare_state, previous_state;

    txn = session->txn;

    /* Lock the ref to ensure we don't race with page instantiation. */
    WT_REF_LOCK(session, ref, &previous_state);

    if (commit) {
        ts = txn->commit_timestamp;
        prepare_state = WT_PREPARE_RESOLVED;
    } else {
        ts = txn->prepare_timestamp;
        prepare_state = WT_PREPARE_INPROGRESS;
    }

    /*
     * Timestamps and prepare state are in the page deleted structure for truncates, or in the
     * updates list in the case of instantiated pages. We also need to update any page deleted
     * structure in the ref.
     *
     * Only two cases are possible. First: the state is WT_REF_DELETED. In this case page_del cannot
     * be NULL yet because an uncommitted operation cannot have reached global visibility. (Or at
     * least, global visibility in the sense we need to use it for truncations, in which prepared
     * and uncommitted transactions are not visible.)
     *
     * Otherwise: there is an uncommitted delete operation we're handling, so the page must have
     * been deleted at some point, and the tree can't be readonly. Therefore the page must have been
     * instantiated, the state must be WT_REF_MEM, and there should be an update list in
     * mod->inst_updates. (But just in case, allow the update list to be null.) There might be a
     * non-null page_del structure to update, depending on whether the page has been reconciled
     * since it was deleted and then instantiated.
     */
    if (previous_state != WT_REF_DELETED) {
        WT_ASSERT(session, previous_state == WT_REF_MEM);
        WT_ASSERT(session, ref->page != NULL && ref->page->modify != NULL);
        if ((updp = ref->page->modify->inst_updates) != NULL)
            for (; *updp != NULL; ++updp) {
                (*updp)->start_ts = ts;
                /*
                 * Holding the ref locked means we have exclusive access, so if we are committing we
                 * don't need to use the prepare locked transition state.
                 */
                (*updp)->prepare_state = prepare_state;
                if (commit)
                    (*updp)->durable_ts = txn->durable_timestamp;
            }
    }
    page_del = ref->page_del;
    if (page_del != NULL) {
        page_del->timestamp = ts;
        if (commit)
            page_del->durable_timestamp = txn->durable_timestamp;
        WT_PUBLISH(page_del->prepare_state, prepare_state);
    }

    WT_REF_UNLOCK(ref, previous_state);
}

/*
 * __wt_txn_op_delete_commit_apply_timestamps --
 *     Apply the correct start and durable timestamps to any updates in the page del update list.
 */
static inline void
__wt_txn_op_delete_commit_apply_timestamps(WT_SESSION_IMPL *session, WT_REF *ref)
{
    WT_PAGE_DELETED *page_del;
    WT_TXN *txn;
    WT_UPDATE **updp;
    uint8_t previous_state;

    txn = session->txn;

    /* Lock the ref to ensure we don't race with page instantiation. */
    WT_REF_LOCK(session, ref, &previous_state);

    /*
     * Timestamps are in the page deleted structure for truncates, or in the updates in the case of
     * instantiated pages. We also need to update any page deleted structure in the ref. Both commit
     * and durable timestamps need to be updated.
     *
     * Only two cases are possible. First: the state is WT_REF_DELETED. In this case page_del cannot
     * be NULL yet because an uncommitted operation cannot have reached global visibility. (Or at
     * least, global visibility in the sense we need to use it for truncations, in which prepared
     * and uncommitted transactions are not visible.)
     *
     * Otherwise: there is an uncommitted delete operation we're handling, so the page must have
     * been deleted at some point, and the tree can't be readonly. Therefore the page must have been
     * instantiated, the state must be WT_REF_MEM, and there should be an update list in
     * mod->inst_updates. (But just in case, allow the update list to be null.) There might be a
     * non-null page_del structure to update, depending on whether the page has been reconciled
     * since it was deleted and then instantiated.
     */
    if (previous_state != WT_REF_DELETED) {
        WT_ASSERT(session, previous_state == WT_REF_MEM);
        WT_ASSERT(session, ref->page != NULL && ref->page->modify != NULL);
        if ((updp = ref->page->modify->inst_updates) != NULL)
            for (; *updp != NULL; ++updp) {
                (*updp)->start_ts = txn->commit_timestamp;
                (*updp)->durable_ts = txn->durable_timestamp;
            }
    }
    page_del = ref->page_del;
    if (page_del != NULL && page_del->timestamp == WT_TS_NONE) {
        page_del->timestamp = txn->commit_timestamp;
        page_del->durable_timestamp = txn->durable_timestamp;
    }

    WT_REF_UNLOCK(ref, previous_state);
}

/*
 * __wt_txn_op_set_timestamp --
 *     Decide whether to copy a commit timestamp into an update. If the op structure doesn't have a
 *     populated update or ref field or is in prepared state there won't be any check for an
 *     existing timestamp.
 */
static inline void
__wt_txn_op_set_timestamp(WT_SESSION_IMPL *session, WT_TXN_OP *op)
{
    WT_BTREE *btree;
    WT_TXN *txn;
    WT_UPDATE *upd;

    btree = op->btree;
    txn = session->txn;
    //printf("yang test .....__wt_txn_op_set_timestamp..1........");

    /*
     * Updates without a commit time and logged objects don't have timestamps, and only the most
     * recently committed data matches files on disk.
     */
    //只有设置了commit_timestamp并且没有启用WAL功能，timestamp功能才会有效
     
    //一定要设置commit_timestamp timestamp功能才会有效, 如果没有设置commit_timestamp则直接返回，就不会进入下面的流程
    
    //如果session设置了commit_timestamp则__wt_txn_commit的时候会再次进入这里面走后面的流程
    if (!F_ISSET(txn, WT_TXN_HAS_TS_COMMIT))
        return;
    //如果启用了WAL功能，则timestamp功能失效
    //mongodb 副本集普通数据表的log是关闭了的，oplog表的log功能是打开的
    if (F_ISSET(btree, WT_BTREE_LOGGED))
        return;

    //printf("yang test .....__wt_txn_op_set_timestamp..2........");
    if (F_ISSET(txn, WT_TXN_PREPARE)) {
        /*
         * We have a commit timestamp for a prepare transaction, this is only possible as part of a
         * transaction commit call.
         */
        if (op->type == WT_TXN_OP_REF_DELETE)
            __wt_txn_op_delete_apply_prepare_state(session, op->u.ref, true);
        else {
            upd = op->u.op_upd;

            /* Resolve prepared update to be committed update. */
            __txn_resolve_prepared_update(session, upd);
        }
    } else {
        if (op->type == WT_TXN_OP_REF_DELETE)
            __wt_txn_op_delete_commit_apply_timestamps(session, op->u.ref);
        else {
            /*
             * The timestamp is in the update for operations other than truncate. Both commit and
             * durable timestamps need to be updated.
             */
            upd = op->u.op_upd;
            //mongodb普通表log.enabled为false,只有oplog表为true，因此这里mongodb只有oplog表才会记录start_ts和durable_ts
            if (upd->start_ts == WT_TS_NONE) {//如果没有设置commit_timestamp则不会进入这里流程，在前面WT_TXN_HAS_TS_COMMIT会直接返回，所以ts都会为0
                //注意只有关闭了oplog(log=(enabled=false))功能的表才会有upd timestamp功能
                //这两个成员真正生效是如果表没有启用log功能，则reconcile的时候会同时把start ts等信息写入磁盘，生效地方见WT_TIME_WINDOW_SET_START和WT_TIME_WINDOW_SET_STOP
                upd->start_ts = txn->commit_timestamp;
                upd->durable_ts = txn->durable_timestamp;
                {//yang add change 
                    char ts_string[2][WT_TS_INT_STRING_SIZE];
                    __wt_verbose(session, WT_VERB_TIMESTAMP, "start_ts %s, durable_ts: %s, %s", 
                        __wt_timestamp_to_string(upd->start_ts, ts_string[0]), 
                        __wt_timestamp_to_string(upd->durable_ts, ts_string[1]), 
                        "__wt_txn_op_set_timestamp");
                }
            }
        }
    }
}

/*
 * __wt_txn_modify --
 *     Mark a WT_UPDATE object modified by the current transaction.
 //获取一个事务op并对相关成员赋值  __wt_row_modify->__wt_txn_modify->__txn_next_op
  //只有key存在时候得modify才会进来，insert不会进来
 */
static inline int
__wt_txn_modify(WT_SESSION_IMPL *session, WT_UPDATE *upd)
{
    WT_TXN *txn;
    WT_TXN_OP *op;

    txn = session->txn;

    if (F_ISSET(txn, WT_TXN_READONLY)) {
        if (F_ISSET(txn, WT_TXN_IGNORE_PREPARE))
            WT_RET_MSG(
              session, ENOTSUP, "Transactions with ignore_prepare=true cannot perform updates");
        WT_RET_MSG(session, WT_ROLLBACK, "Attempt to update in a read-only transaction");
    }

    //获取一个事务op
    WT_RET(__txn_next_op(session, &op));
    if (F_ISSET(session, WT_SESSION_LOGGING_INMEM)) {
        if (op->btree->type == BTREE_ROW)
            op->type = WT_TXN_OP_INMEM_ROW;
        else
            op->type = WT_TXN_OP_INMEM_COL;
    } else {
        if (op->btree->type == BTREE_ROW)
            op->type = WT_TXN_OP_BASIC_ROW;
        else
            op->type = WT_TXN_OP_BASIC_COL;
    }
    op->u.op_upd = upd;

    /* History store bypasses transactions, transaction modify should never be called on it. */
    WT_ASSERT(session, !WT_IS_HS((S2BT(session))->dhandle));

    upd->txnid = session->txn->id;
    __wt_txn_op_set_timestamp(session, op);

    return (0);
}

/*
 * __wt_txn_modify_page_delete --
 *     Remember a page truncated by the current transaction.
 */
static inline int
__wt_txn_modify_page_delete(WT_SESSION_IMPL *session, WT_REF *ref)
{
    WT_DECL_RET;
    WT_TXN *txn;
    WT_TXN_OP *op;

    txn = session->txn;

    WT_RET(__txn_next_op(session, &op));
    op->type = WT_TXN_OP_REF_DELETE;
    op->u.ref = ref;

    /*
     * This access to the WT_PAGE_DELETED structure is safe; caller has the WT_REF locked, and in
     * fact just allocated the structure to fill in.
     */
    ref->page_del->txnid = txn->id;
    __wt_txn_op_set_timestamp(session, op);

    if (__wt_log_op(session))
        WT_ERR(__wt_txn_log_op(session, NULL));
    return (0);

err:
    __wt_txn_unmodify(session);
    return (ret);
}

/*
 * __wt_txn_oldest_id --
 *     Return the oldest transaction ID that has to be kept for the current tree.
 注意__txn_oldest_scan的oldest id没考虑checkpoint快照id, __wt_txn_oldest_id会考虑checkpoint线程对应事务id
 */
static inline uint64_t
__wt_txn_oldest_id(WT_SESSION_IMPL *session)
{
    WT_CONNECTION_IMPL *conn;
    WT_TXN_GLOBAL *txn_global;
    uint64_t checkpoint_pinned, oldest_id, recovery_ckpt_snap_min;

    conn = S2C(session);
    txn_global = &conn->txn_global;

    /*
     * The metadata is tracked specially because of optimizations for checkpoints.
     */
    if (session->dhandle != NULL && WT_IS_METADATA(session->dhandle))
        return (txn_global->metadata_pinned);

    /*
     * Take a local copy of these IDs in case they are updated while we are checking visibility. The
     * read of the transaction ID pinned by a checkpoint needs to be carefully ordered: if a
     * checkpoint is starting and we have to start checking the pinned ID, we take the minimum of it
     * with the oldest ID, which is what we want. The logged tables are excluded as part of RTS, so
     * there is no need of holding their oldest_id
     */
    WT_ORDERED_READ(oldest_id, txn_global->oldest_id);

    if (!F_ISSET(conn, WT_CONN_RECOVERING) || session->dhandle == NULL ||
      F_ISSET(S2BT(session), WT_BTREE_LOGGED)) {
        /*
         * Checkpoint transactions often fall behind ordinary application threads. If there is an
         * active checkpoint, keep changes until checkpoint is finished.
         */
        checkpoint_pinned = txn_global->checkpoint_txn_shared.pinned_id;
        if (checkpoint_pinned == WT_TXN_NONE || WT_TXNID_LT(oldest_id, checkpoint_pinned))
            return (oldest_id);
        return (checkpoint_pinned);
    } else {
        /*
         * Recovered checkpoint snapshot rarely fall behind ordinary application threads. Keep the
         * changes until the recovery is finished.
         */
        recovery_ckpt_snap_min = conn->recovery_ckpt_snap_min;
        if (recovery_ckpt_snap_min == WT_TXN_NONE || WT_TXNID_LT(oldest_id, recovery_ckpt_snap_min))
            return (oldest_id);
        return (recovery_ckpt_snap_min);
    }
}

/*
 * __wt_txn_pinned_timestamp --
 *     Get the first timestamp that has to be kept for the current tree.
 */
static inline void
__wt_txn_pinned_timestamp(WT_SESSION_IMPL *session, wt_timestamp_t *pinned_tsp)
{
    WT_TXN_GLOBAL *txn_global;
    wt_timestamp_t checkpoint_ts, pinned_ts;

    *pinned_tsp = WT_TS_NONE;

    txn_global = &S2C(session)->txn_global;

    /*
     * There is no need to go further if no pinned timestamp has been set yet.
     */
    if (!txn_global->has_pinned_timestamp)
        return;

    /* If we have a version cursor open, use the pinned timestamp when it is opened. */
    if (S2C(session)->version_cursor_count > 0) {
        *pinned_tsp = txn_global->version_cursor_pinned_timestamp;
        return;
    }

    *pinned_tsp = pinned_ts = txn_global->pinned_timestamp;

    /*
     * The read of checkpoint timestamp needs to be carefully ordered: it needs to be after we have
     * read the pinned timestamp and the checkpoint generation, otherwise, we may read earlier
     * checkpoint timestamp before the checkpoint generation that is read resulting more data being
     * pinned. If a checkpoint is starting and we have to use the checkpoint timestamp, we take the
     * minimum of it with the oldest timestamp, which is what we want.
     */
    WT_READ_BARRIER();
    checkpoint_ts = txn_global->checkpoint_timestamp;

    if (checkpoint_ts != 0 && checkpoint_ts < pinned_ts)
        *pinned_tsp = checkpoint_ts;
}

/*
 * __txn_visible_all_id --
 *     Check if a given transaction ID is "globally visible". This is, if all sessions in the system
 *     will see the transaction ID including the ID that belongs to a running checkpoint.
 判断事务id是否对当前所有session可见

__wt_txn_visible:  id对应udp是否对当前session可见
__txn_visible_all_id:  id对应udp是否对所有session可见
 */
static inline bool
__txn_visible_all_id(WT_SESSION_IMPL *session, uint64_t id)
{
    WT_TXN *txn;
    uint64_t oldest_id;

    txn = session->txn;

    /* Make sure that checkpoint cursor transactions only read checkpoints, except for metadata. */
    WT_ASSERT(session,
      (session->dhandle != NULL && WT_IS_METADATA(session->dhandle)) ||
        WT_READING_CHECKPOINT(session) == F_ISSET(session->txn, WT_TXN_IS_CHECKPOINT));

    /*
     * When reading from a checkpoint, all readers use the same snapshot, so a transaction is
     * globally visible if it is visible in that snapshot. Note that this can cause things that were
     * not globally visible yet when the checkpoint is taken to become globally visible in the
     * checkpoint. This is expected (it is like all the old running transactions exited) -- but note
     * that it's important that the inverse change (something globally visible when the checkpoint
     * was taken becomes not globally visible in the checkpoint) never happen as this violates basic
     * assumptions about visibility. (And, concretely, it can cause stale history store entries to
     * come back to life and produce wrong answers.)
     *
     * Note: we use the transaction to check this rather than testing WT_READING_CHECKPOINT because
     * reading the metadata while working with a checkpoint cursor will borrow the transaction; it
     * then ends up using it to read a non-checkpoint tree. This is believed to be ok because the
     * metadata is always read-uncommitted, but we want to still use the checkpoint-cursor
     * visibility logic. Using the regular visibility logic with a checkpoint cursor transaction can
     * be logically invalid (it is possible that way for something to be globally visible but
     * specifically invisible) and also can end up comparing transaction ids from different database
     * opens.
     */
    if (F_ISSET(session->txn, WT_TXN_IS_CHECKPOINT))
        return (__wt_txn_visible_id_snapshot(
          id, txn->snap_min, txn->snap_max, txn->snapshot, txn->snapshot_count));
    //这里一定要__wt_txn_update_oldest更新后的事务id才会可见
    oldest_id = __wt_txn_oldest_id(session);

    return (WT_TXNID_LT(id, oldest_id));
}

/*
 * __wt_txn_visible_all --
 *     Check whether a given time window is either globally visible or obsolete. For global
 *     visibility checks, the commit times are checked against the oldest possible readers in the
 *     system. If all possible readers could always see the time window - it is globally visible.
 *     For obsolete checks callers should generally pass in the durable timestamp, since it is
 *     guaranteed to be newer than or equal to the commit time, and content needs to be retained
 *     (not become obsolete) until both the commit and durable times are obsolete. If the commit
 *     time is used for this check, it's possible that a transaction is committed with a durable
 *     time and made obsolete before it can be included in a checkpoint - which leads to bugs in
 *     checkpoint correctness.
 */
static inline bool
__wt_txn_visible_all(WT_SESSION_IMPL *session, uint64_t id, wt_timestamp_t timestamp)
{
    wt_timestamp_t pinned_ts;

    /*
     * When shutting down, the transactional system has finished running and all we care about is
     * eviction, make everything visible.
     */
    if (F_ISSET(S2C(session), WT_CONN_CLOSING))
        return (true);
        
    // 判断事务id是否对当前所有session可见
    if (!__txn_visible_all_id(session, id))
        return (false);

    /* Timestamp check. */
    if (timestamp == WT_TS_NONE)
        return (true);

    /* Make sure that checkpoint cursor transactions only read checkpoints, except for metadata. */
    WT_ASSERT(session,
      (session->dhandle != NULL && WT_IS_METADATA(session->dhandle)) ||
        WT_READING_CHECKPOINT(session) == F_ISSET(session->txn, WT_TXN_IS_CHECKPOINT));

    /* When reading a checkpoint, use the checkpoint state instead of the current state. */
    if (F_ISSET(session->txn, WT_TXN_IS_CHECKPOINT))
        return (session->txn->checkpoint_oldest_timestamp != WT_TS_NONE &&
          timestamp <= session->txn->checkpoint_oldest_timestamp);

    /* If no oldest timestamp has been supplied, updates have to stay in cache. */
    __wt_txn_pinned_timestamp(session, &pinned_ts);

    return (pinned_ts != WT_TS_NONE && timestamp <= pinned_ts);
}

/*
 * __wt_txn_upd_visible_all --
 *     Is the given update visible to all (possible) readers?
 //用的最多的地方在__wt_update_obsolete_check，用来判断是否可以对历史版本数据从内存清理掉
 
 // 判断upd是否对当前所有session可见，对update影响较大，可以参考__wt_update_obsolete_check, 判断udp数据是否可以释放
 */
static inline bool
__wt_txn_upd_visible_all(WT_SESSION_IMPL *session, WT_UPDATE *upd)
{
    if (upd->prepare_state == WT_PREPARE_LOCKED || upd->prepare_state == WT_PREPARE_INPROGRESS)
        return (false);

    /*
     * This function is used to determine when an update is obsolete: that should take into account
     * the durable timestamp which is greater than or equal to the start timestamp.
     */
    return (__wt_txn_visible_all(session, upd->txnid, upd->durable_ts));
}

/*
 * __wt_txn_upd_value_visible_all --
 *     Is the given update value visible to all (possible) readers?
 */
static inline bool
__wt_txn_upd_value_visible_all(WT_SESSION_IMPL *session, WT_UPDATE_VALUE *upd_value)
{
    WT_ASSERT(session, upd_value->tw.prepare == 0);
    return (upd_value->type == WT_UPDATE_TOMBSTONE ?
        __wt_txn_visible_all(session, upd_value->tw.stop_txn, upd_value->tw.durable_stop_ts) :
        __wt_txn_visible_all(session, upd_value->tw.start_txn, upd_value->tw.durable_start_ts));
}

/*
 * __wt_txn_tw_stop_visible --
 *     Is the given stop time window visible?
 session是否可以访问tw对应事务
 */
static inline bool
__wt_txn_tw_stop_visible(WT_SESSION_IMPL *session, WT_TIME_WINDOW *tw)
{
    return (WT_TIME_WINDOW_HAS_STOP(tw) && !tw->prepare &&
      __wt_txn_visible(session, tw->stop_txn, tw->stop_ts));
}

/*
 * __wt_txn_tw_start_visible --
 *     Is the given start time window visible?
 */
static inline bool
__wt_txn_tw_start_visible(WT_SESSION_IMPL *session, WT_TIME_WINDOW *tw)
{
    /*
     * Check the prepared flag if there is no stop time point or the start and stop time points are
     * from the same transaction.
     */
    return (((WT_TIME_WINDOW_HAS_STOP(tw) &&
               (tw->start_txn != tw->stop_txn || tw->start_ts != tw->stop_ts ||
                 tw->durable_start_ts != tw->durable_stop_ts)) ||
              !tw->prepare) &&
      __wt_txn_visible(session, tw->start_txn, tw->start_ts));
}

/*
 * __wt_txn_tw_start_visible_all --
 *     Is the given start time window visible to all (possible) readers?
 */
static inline bool
__wt_txn_tw_start_visible_all(WT_SESSION_IMPL *session, WT_TIME_WINDOW *tw)
{
    /*
     * Check the prepared flag if there is no stop time point or the start and stop time points are
     * from the same transaction.
     */
    return (((WT_TIME_WINDOW_HAS_STOP(tw) &&
               (tw->start_txn != tw->stop_txn || tw->start_ts != tw->stop_ts ||
                 tw->durable_start_ts != tw->durable_stop_ts)) ||
              !tw->prepare) &&
      __wt_txn_visible_all(session, tw->start_txn, tw->durable_start_ts));
}

/*
 * __wt_txn_tw_stop_visible_all --
 *     Is the given stop time window visible to all (possible) readers?
 */
static inline bool
__wt_txn_tw_stop_visible_all(WT_SESSION_IMPL *session, WT_TIME_WINDOW *tw)
{
    return (WT_TIME_WINDOW_HAS_STOP(tw) && !tw->prepare &&
      __wt_txn_visible_all(session, tw->stop_txn, tw->durable_stop_ts));
}

/*
 * __wt_txn_visible_id_snapshot --
 *     Is the id visible in terms of the given snapshot?
 判断id是否在本session所拥有的snap[]快照列表中
 */
static inline bool
__wt_txn_visible_id_snapshot(
  uint64_t id, uint64_t snap_min, uint64_t snap_max, uint64_t *snapshot, uint32_t snapshot_count)
{
    bool found;

    /*
     * WT_ISO_SNAPSHOT, WT_ISO_READ_COMMITTED: the ID is visible if it is not the result of a
     * concurrent transaction, that is, if was committed before the snapshot was taken.
     *
     * The order here is important: anything newer than or equal to the maximum ID we saw when
     * taking the snapshot should be invisible, even if the snapshot is empty.
     *
     * Snapshot data:
     *	ids >= snap_max not visible,
     *	ids < snap_min are visible,
     *	everything else is visible unless it is found in the snapshot.
     */
    if (WT_TXNID_LE(snap_max, id))
        return (false);
    if (snapshot_count == 0 || WT_TXNID_LT(id, snap_min))
        return (true);

    //如果snapshot_count不等于0，并且介于snap_min, snap_max之间，如果在snapshot[]数组可以找到则不可访问
    WT_BINARY_SEARCH(id, snapshot, snapshot_count, found);
    return (!found);
}

/*
 * __txn_visible_id --
 *     Can the current transaction see the given ID?
 //当前session是否可以访问id对应事务, 注意这里的id是udp->txnid，它不会因为事务提交而清0，事务提交后只有session->id清0
 */
static inline bool
__txn_visible_id(WT_SESSION_IMPL *session, uint64_t id)
{
    WT_TXN *txn;

    txn = session->txn;

    /* Changes with no associated transaction are always visible. */
    //例如page持久化到磁盘了，然后客户端访问这个数据，这时候会从磁盘加载到内存，该加载page的这些数据txn都为WT_TXN_NONE
    if (id == WT_TXN_NONE) {
        //printf("yang test xxxxxxxxxxxxxxxx __txn_visible_id\r\n");
        return (true);
    }
    /* Nobody sees the results of aborted transactions. */
    if (id == WT_TXN_ABORTED)
        return (false);

    /* Transactions see their own changes. */
    if (id == txn->id)
        return (true);

    /* Read-uncommitted transactions see all other changes. */
    if (txn->isolation == WT_ISO_READ_UNCOMMITTED)
        return (true);

    /* Otherwise, we should be called with a snapshot. */
    WT_ASSERT(session, F_ISSET(txn, WT_TXN_HAS_SNAPSHOT));

    //判断id是否在本session所拥有的snap[]快照列表中, 如果在则不可访问
    return (__wt_txn_visible_id_snapshot(
      id, txn->snap_min, txn->snap_max, txn->snapshot, txn->snapshot_count));
}

/*
 * __wt_txn_visible --
 *     Can the current transaction see the given ID / timestamp?
 //事务提交的时候会在__wt_txn_commit->__wt_txn_release中置session对应事务idWT_SESSION_IMPL->txn->id为WT_TXN_NONE 
//upd->txnid为修改该值对应的事务id(__wt_txn_modify)，该id值不会因为事务提交置为0

注意这里的id为upd->txnid, 可能是其他session做的更新

__wt_txn_visible:  id对应udp是否对当前session可见
__txn_visible_all_id:  id对应udp是否对所有session可见
 */
static inline bool
__wt_txn_visible(WT_SESSION_IMPL *session, uint64_t id, wt_timestamp_t timestamp)
{
    WT_TXN *txn;
    WT_TXN_SHARED *txn_shared;
   // char ts_string[WT_TS_INT_STRING_SIZE];

    txn = session->txn;
    txn_shared = WT_SESSION_TXN_SHARED(session);

    //printf("yang test ...........__wt_txn_visible...session id:%u, session txn id:%lu, session flags:%u, visible txn id:%lu, timestamp:%s\r\n", 
      //  session->id, session->txn->id, session->flags, id, __wt_timestamp_to_string(timestamp, ts_string));
    //当前session是否可以访问id对应事务
    if (!__txn_visible_id(session, id)) {
        //printf("yang test ...........__wt_txn_visible...false\r\n");
        return (false);
    }
    /* Transactions read their writes, regardless of timestamps. */
    //sesion可以访问本事务
    if (F_ISSET(session->txn, WT_TXN_HAS_ID) && id == session->txn->id)
        return (true);

    /* Timestamp check. */
    //__wt_txn_set_read_timestamp中置位，设置了read_timestamp才会有效
    if (!F_ISSET(txn, WT_TXN_SHARED_TS_READ) || timestamp == WT_TS_NONE)
        return (true);

    if (WT_READING_CHECKPOINT(session))
        return (timestamp <= txn->checkpoint_read_timestamp);

    return (timestamp <= txn_shared->read_timestamp);
}

/*
 * __wt_txn_upd_visible_type --
 *     Visible type of given update for the current transaction.
 //当前session对应事务是否可以访问upd->start_ts时间点的upd数据
 */
static inline WT_VISIBLE_TYPE
__wt_txn_upd_visible_type(WT_SESSION_IMPL *session, WT_UPDATE *upd)
{
    uint8_t prepare_state, previous_state;
    bool upd_visible;

    for (;; __wt_yield()) {
        /* Prepare state change is in progress, yield and try again. */
        WT_ORDERED_READ(prepare_state, upd->prepare_state);
        if (prepare_state == WT_PREPARE_LOCKED)
            continue;

        /* Entries in the history store are always visible. */
        if ((WT_IS_HS(session->dhandle) && upd->txnid != WT_TXN_ABORTED &&
              upd->type == WT_UPDATE_STANDARD))
            return (WT_VISIBLE_TRUE);

        upd_visible = __wt_txn_visible(session, upd->txnid, upd->start_ts);

        /*
         * The visibility check is only valid if the update does not change state. If the state does
         * change, recheck visibility.
         */
        previous_state = prepare_state;
        WT_ORDERED_READ(prepare_state, upd->prepare_state);
        if (previous_state == prepare_state)
            break;

        WT_STAT_CONN_INCR(session, prepared_transition_blocked_page);
    }

    //不可见
    if (!upd_visible)
        return (WT_VISIBLE_FALSE);

    //udp可见，需要再做一次WT_PREPARE_INPROGRESS检查，如果upd->prepare_state=WT_PREPARE_INPROGRESS,则直接返回WT_VISIBLE_PREPARE
    //如果是__wt_txn_prepare prepare事务更新的udp，则访问类型为WT_VISIBLE_PREPARE，这样再__wt_txn_read_upd_list_internal中会再次做一次WT_VISIBLE_PREPARE判断
    if (prepare_state == WT_PREPARE_INPROGRESS)
        return (WT_VISIBLE_PREPARE);

    return (WT_VISIBLE_TRUE);
}

/*
 * __wt_txn_upd_visible --
 *     Can the current transaction see the given update.
 __wt_txn_upd_visible: 该事务是否可以修改该udp，是否存在写冲突
 __wt_txn_read_upd_list_internal: 该事务是否可以读该udp 
 
 //当前事务是否可以访问upd这条更新数据
 */
static inline bool
__wt_txn_upd_visible(WT_SESSION_IMPL *session, WT_UPDATE *upd)
{
    return (__wt_txn_upd_visible_type(session, upd) == WT_VISIBLE_TRUE);
}

/*
 * __wt_upd_alloc --
 *     Allocate a WT_UPDATE structure and associated value and fill it in.
 */
// __wt_upd_alloc中插入，如果删除K了，则在__wt_txn_modify_check中返回不存在
//__wt_upd_alloc分配WT_UPDATE空间
static inline int
__wt_upd_alloc(WT_SESSION_IMPL *session, const WT_ITEM *value, u_int modify_type, WT_UPDATE **updp,
  size_t *sizep)
{
    WT_UPDATE *upd;

    *updp = NULL;

    /*
     * The code paths leading here are convoluted: assert we never attempt to allocate an update
     * structure if only intending to insert one we already have, or pass in a value with a type
     * that doesn't support values.
     */
    WT_ASSERT(session, modify_type != WT_UPDATE_INVALID);
    WT_ASSERT(session,
      (value == NULL && (modify_type == WT_UPDATE_RESERVE || modify_type == WT_UPDATE_TOMBSTONE)) ||
        (value != NULL &&
          !(modify_type == WT_UPDATE_RESERVE || modify_type == WT_UPDATE_TOMBSTONE)));

    /*
     * Allocate the WT_UPDATE structure and room for the value, then copy any value into place.
     * Memory is cleared, which is the equivalent of setting:
     *    WT_UPDATE.txnid = WT_TXN_NONE;
     *    WT_UPDATE.durable_ts = WT_TS_NONE;
     *    WT_UPDATE.start_ts = WT_TS_NONE;
     *    WT_UPDATE.prepare_state = WT_PREPARE_INIT;
     *    WT_UPDATE.flags = 0;
     */
    WT_RET(__wt_calloc(session, 1, WT_UPDATE_SIZE + (value == NULL ? 0 : value->size), &upd));
    //如果value为NULL，也就是WT_CURSOR->remove操作删除一个key的时候，实际上是生成一个新的udp,udp的value长度为0
    if (value != NULL && value->size != 0) {
        upd->size = WT_STORE_SIZE(value->size);
        memcpy(upd->data, value->data, value->size);
    }
    upd->type = (uint8_t)modify_type;

    *updp = upd;
    if (sizep != NULL)
        *sizep = WT_UPDATE_MEMSIZE(upd);
    return (0);
}

/*
 * __wt_upd_alloc_tombstone --
 *     Allocate a tombstone update.
 */
static inline int
__wt_upd_alloc_tombstone(WT_SESSION_IMPL *session, WT_UPDATE **updp, size_t *sizep)
{
    return (__wt_upd_alloc(session, NULL, WT_UPDATE_TOMBSTONE, updp, sizep));
}

/*
 * __wt_txn_read_upd_list_internal --
 *     Internal helper function to get the first visible update in a list (or NULL if none are
 *     visible).

  __wt_txn_upd_visible: 该事务是否可以修改该udp，是否存在写冲突
 __wt_txn_read_upd_list_internal: 该事务是否可以读该udp 
 */
//获取udp链表中的更新的数据，获取第一个可访问数据存入cbt->upd_value
static inline int
__wt_txn_read_upd_list_internal(WT_SESSION_IMPL *session, WT_CURSOR_BTREE *cbt, WT_UPDATE *upd,
  WT_UPDATE **prepare_updp, WT_UPDATE **restored_updp)
{
    WT_VISIBLE_TYPE upd_visible;
    uint8_t prepare_state, type;

    if (prepare_updp != NULL)
        *prepare_updp = NULL;
    if (restored_updp != NULL)
        *restored_updp = NULL;
    __wt_upd_value_clear(cbt->upd_value);

    //循环遍历udp链表中的V，获取第一个最新的可访问V
    for (; upd != NULL; upd = upd->next) {
        WT_ORDERED_READ(type, upd->type);
        /* Skip reserved place-holders, they're never visible. */
        if (type == WT_UPDATE_RESERVE)
            continue;

        WT_ORDERED_READ(prepare_state, upd->prepare_state);
        /*
         * If the cursor is configured to ignore tombstones, copy the timestamps from the tombstones
         * to the stop time window of the update value being returned to the caller. Caller can
         * process the stop time window to decide if there was a tombstone on the update chain. If
         * the time window already has a stop time set then we must have seen a tombstone prior to
         * ours in the update list, and therefore don't need to do this again.
         */
        //udp对应key被删除了，并且不是hs表
        if (type == WT_UPDATE_TOMBSTONE && F_ISSET(&cbt->iface, WT_CURSTD_IGNORE_TOMBSTONE) &&
          !WT_TIME_WINDOW_HAS_STOP(&cbt->upd_value->tw)) {
            cbt->upd_value->tw.durable_stop_ts = upd->durable_ts;
            cbt->upd_value->tw.stop_ts = upd->start_ts;
            cbt->upd_value->tw.stop_txn = upd->txnid;
            cbt->upd_value->tw.prepare =
              prepare_state == WT_PREPARE_INPROGRESS || prepare_state == WT_PREPARE_LOCKED;
            continue;
        }

        //当前session可以访问udp对应数据
        upd_visible = __wt_txn_upd_visible_type(session, upd);
        //找到了第一个可访问的V，直接返回
        if (upd_visible == WT_VISIBLE_TRUE)
            break;

        /*
         * Save the prepared update to help us detect if we race with prepared commit or rollback
         * irrespective of update visibility.
         */
        if ((prepare_state == WT_PREPARE_INPROGRESS || prepare_state == WT_PREPARE_LOCKED) &&
          prepare_updp != NULL && *prepare_updp == NULL &&
          F_ISSET(upd, WT_UPDATE_PREPARE_RESTORED_FROM_DS))
            *prepare_updp = upd;

        /*
         * Save the restored update to use it as base value update in case if we need to reach
         * history store instead of on-disk value.
         */
        if (restored_updp != NULL && F_ISSET(upd, WT_UPDATE_RESTORED_FROM_HS) &&
          type == WT_UPDATE_STANDARD) {
            WT_ASSERT(session, *restored_updp == NULL);
            *restored_updp = upd;
        }

        //如果visible类型为WT_VISIBLE_PREPARE，则返回WT_PREPARE_CONFLICT，mongo server收到这个一次就会不停的重试
        //增加WT_PREPARE_CONFLICT的原因是prepare_transaction中释放了快照，因此这里会影响可见性判断，并且会影响evict
        if (upd_visible == WT_VISIBLE_PREPARE) {
            /* Ignore the prepared update, if transaction configuration says so. */
            if (F_ISSET(session->txn, WT_TXN_IGNORE_PREPARE))
                continue;

            return (WT_PREPARE_CONFLICT);
        }
    }

    if (upd == NULL)
        return (0);

    /*
     * Now assign to the update value. If it's not a modify, we're free to simply point the value at
     * the update's memory without owning it. If it is a modify, we need to reconstruct the full
     * update now and make the value own the buffer.
     *
     * If the caller has specifically asked us to skip assigning the buffer, we shouldn't bother
     * reconstructing the modify.
     */
    if (upd->type != WT_UPDATE_MODIFY || cbt->upd_value->skip_buf)
        __wt_upd_value_assign(cbt->upd_value, upd);
    else
        WT_RET(__wt_modify_reconstruct_from_upd_list(session, cbt, upd, cbt->upd_value));
    return (0);
}

/*
 * __wt_txn_read_upd_list --
 *     Get the first visible update in a list (or NULL if none are visible).
 //__cursor_row_prev  __cursor_row_next遍历获取udp这条数据， 
 //确定udp上面的那个V对当前session可见
 */
static inline int
__wt_txn_read_upd_list(WT_SESSION_IMPL *session, WT_CURSOR_BTREE *cbt, WT_UPDATE *upd)
{
    return (__wt_txn_read_upd_list_internal(session, cbt, upd, NULL, NULL));
}

/*
 * __wt_txn_read --
 *     Get the first visible update in a chain. This function will first check the update list
 *     supplied as a function argument. If there is no visible update, it will check the onpage
 *     value for the given key. Finally, if the onpage value is not visible to the reader, the
 *     function will search the history store for a visible update.
 //获取udp链表上面可访问的最新V存入cbt->upd_value
 //__cursor_row_prev  __cursor_row_next遍历获取udp这条数据， 
 */
static inline int
__wt_txn_read(
  WT_SESSION_IMPL *session, WT_CURSOR_BTREE *cbt, WT_ITEM *key, uint64_t recno, WT_UPDATE *upd)
{
    WT_DECL_RET;
    WT_TIME_WINDOW tw;
    WT_UPDATE *prepare_upd, *restored_upd;
    bool have_stop_tw, prepare_retry, read_onpage;

    prepare_upd = restored_upd = NULL;
    read_onpage = prepare_retry = true;

retry:
    //获取udp链表中的更新的数据，获取第一个可访问数据存入cbt->upd_value
    WT_RET(__wt_txn_read_upd_list_internal(session, cbt, upd, &prepare_upd, &restored_upd));
    //这个key的update链表上面有可以访问的value数据
    if (WT_UPDATE_DATA_VALUE(cbt->upd_value) ||
      (cbt->upd_value->type == WT_UPDATE_MODIFY && cbt->upd_value->skip_buf))
        return (0);
    WT_ASSERT(session, cbt->upd_value->type == WT_UPDATE_INVALID);

    //内存中没有可访问的value数据，并且该key也不在磁盘上，则直接返回
    /* If there is no ondisk value, there can't be anything in the history store either. */
    if (cbt->ref->page->dsk == NULL) {
        cbt->upd_value->type = WT_UPDATE_TOMBSTONE;
        return (0);
    }

    /*
     * Skip retrieving the on-disk value when there exists a restored update from history store in
     * the update list. Having a restored update as part of the update list indicates that the
     * existing on-disk value is unstable.
     */
    if (restored_upd != NULL) {
        WT_ASSERT(session, !WT_IS_HS(session->dhandle));
        cbt->upd_value->buf.data = restored_upd->data;
        cbt->upd_value->buf.size = restored_upd->size;
    } else {
        /*
         * When we inspected the update list we may have seen a tombstone leaving us with a valid
         * stop time window, we don't want to overwrite this stop time window.
         */
        have_stop_tw = WT_TIME_WINDOW_HAS_STOP(&cbt->upd_value->tw);

        if (read_onpage) {
            /*
             * We may have raced with checkpoint freeing the overflow blocks. Retry from start and
             * ignore the onpage value the next time. For pages that have remained in memory after a
             * checkpoint, this will lead us to read every key with an overflow removed onpage value
             * twice. However, it simplifies the logic and doesn't depend on the assumption that the
             * cell unpacking code will always return a correct time window even it returns a
             * WT_RESTART error.
             */
            //从磁盘从磁盘的page->pg_row[cbt->slot]cbt->slot位置读取该KV
            ret = __wt_value_return_buf(cbt, cbt->ref, &cbt->upd_value->buf, &tw);
            if (ret == WT_RESTART) {
                read_onpage = false;
                goto retry;
            } else
                WT_RET(ret);

          
            /*
             * If the stop time point is set, that means that there is a tombstone at that time. If
             * it is not prepared and it is visible to our txn it means we've just spotted a
             * tombstone and should return "not found", except scanning the history store during
             * rollback to stable and when we are told to ignore non-globally visible tombstones.
             */
            //磁盘上获取到了该K数据，并且该数据可见
            if (!have_stop_tw && __wt_txn_tw_stop_visible(session, &tw) &&
              !F_ISSET(&cbt->iface, WT_CURSTD_IGNORE_TOMBSTONE)) {
                cbt->upd_value->buf.data = NULL;
                cbt->upd_value->buf.size = 0;
                cbt->upd_value->type = WT_UPDATE_TOMBSTONE;
                WT_TIME_WINDOW_COPY_STOP(&cbt->upd_value->tw, &tw);
                return (0);
            }

            /* Store the stop time pair of the history store record that is returning. */
            if (!have_stop_tw && WT_TIME_WINDOW_HAS_STOP(&tw) && WT_IS_HS(session->dhandle))
                WT_TIME_WINDOW_COPY_STOP(&cbt->upd_value->tw, &tw);

            /*
             * We return the onpage value in the following cases:
             * 1. The record is from the history store.
             * 2. It is visible to the reader.
             */
            if (WT_IS_HS(session->dhandle) || __wt_txn_tw_start_visible(session, &tw)) {
                if (cbt->upd_value->skip_buf) {
                    cbt->upd_value->buf.data = NULL;
                    cbt->upd_value->buf.size = 0;
                }
                cbt->upd_value->type = WT_UPDATE_STANDARD;

                WT_TIME_WINDOW_COPY_START(&cbt->upd_value->tw, &tw);
                return (0);
            }
        }
    }

    /* If there's no visible update in the update chain or ondisk, check the history store file. */
    //内存和磁盘中都找到，则从hs表中查找
    if (F_ISSET(S2C(session), WT_CONN_HS_OPEN) && !F_ISSET(session->dhandle, WT_DHANDLE_HS)) {
        __wt_timing_stress(session, WT_TIMING_STRESS_HS_SEARCH, NULL);
        WT_RET(__wt_hs_find_upd(session, S2BT(session)->id, key, cbt->iface.value_format, recno,
          cbt->upd_value, &cbt->upd_value->buf));
    }

    /*
     * Retry if we race with prepared commit or rollback. If we race with prepared rollback, the
     * value the reader should read may have been removed from the history store and appended to the
     * data store. If we race with prepared commit, imagine a case we read with timestamp 50 and we
     * have a prepared update with timestamp 30 and a history store record with timestamp 20,
     * committing the prepared update will cause the stop timestamp of the history store record
     * being updated to 30 and the reader not seeing it.
     */
    if (prepare_upd != NULL) {
        WT_ASSERT(session, F_ISSET(prepare_upd, WT_UPDATE_PREPARE_RESTORED_FROM_DS));
        if (prepare_retry &&
          (prepare_upd->txnid == WT_TXN_ABORTED ||
            prepare_upd->prepare_state == WT_PREPARE_RESOLVED)) {
            prepare_retry = false;
            /* Clean out any stale value before performing the retry. */
            __wt_upd_value_clear(cbt->upd_value);
            WT_STAT_CONN_DATA_INCR(session, txn_read_race_prepare_update);

            /*
             * When a prepared update/insert is rollback or committed, retrying it again should fix
             * concurrent modification of a prepared update. Other than prepared insert rollback,
             * rest of the cases, the history store update is either added to the end of the update
             * chain or modified to set proper stop timestamp. In all the scenarios, retrying again
             * will work to return a proper update.
             */
            goto retry;
        }
    }

    /* Return invalid not tombstone if nothing is found in history store. */
    WT_ASSERT(session, cbt->upd_value->type != WT_UPDATE_TOMBSTONE);
    return (0);
}

/*
 * __wt_txn_begin --
 *     Begin a transaction.
 //如果一个写入操作__curfile_insert没有显示调用__wt_txn_begin，则会调用CURSOR_UPDATE_API_CALL_BTREE->TXN_API_CALL_NOCONF设置为WT_TXN_AUTOCOMMIT,
//  __wt_txn_autocommit_check中就会自动加上__wt_txn_begin
 */
static inline int
__wt_txn_begin(WT_SESSION_IMPL *session, const char *cfg[])
{
    WT_TXN *txn;

    txn = session->txn;
    txn->isolation = session->isolation;
    txn->txn_logsync = S2C(session)->txn_logsync;
    //yang add todo xxxxxxxxxxxxxxx durable_timestamp和prepare_timestamp这里要不要设置为WT_TS_NONE   ????????????
    txn->commit_timestamp = WT_TS_NONE;
    txn->first_commit_timestamp = WT_TS_NONE;

    WT_ASSERT(session, !F_ISSET(txn, WT_TXN_RUNNING));

    //session事务配置解析
    WT_RET(__wt_txn_config(session, cfg));

    /*
     * Allocate a snapshot if required or update the existing snapshot. Do not update the existing
     * snapshot of autocommit transactions because they are committed at the end of the operation.
     */
    //注意这里只有WT_ISO_SNAPSHOT会获取快照，WT_ISO_READ_COMMITTED和WT_ISO_READ_UNCOMMITTED不会获取快照
    if (txn->isolation == WT_ISO_SNAPSHOT &&
       //注意这里有个!，一般都不会满足这个条件，从而进入下面流程
      !(F_ISSET(txn, WT_TXN_AUTOCOMMIT) && F_ISSET(txn, WT_TXN_HAS_SNAPSHOT))) {
        if (session->ncursors > 0)
            WT_RET(__wt_session_copy_values(session));

        /*
         * Stall here if the cache is completely full. Eviction check can return rollback, but the
         * WT_SESSION.begin_transaction API can't, continue on.
         */
        //printf("yang test ..................__wt_txn_begin...................................\r\n");
        //检查节点已使用内存、脏数据、update数据百分比，判断是否需要用户线程、evict线程进行evict处理
        WT_RET_ERROR_OK(__wt_cache_eviction_check(session, false, true, NULL), WT_ROLLBACK);

        __wt_txn_get_snapshot(session);
    }

    F_SET(txn, WT_TXN_RUNNING);
    if (F_ISSET(S2C(session), WT_CONN_READONLY))
        F_SET(txn, WT_TXN_READONLY);

    return (0);
}

/*
 * __wt_txn_autocommit_check --
 *     If an auto-commit transaction is required, start one.
 //如果一个写入操作__curfile_insert没有显示调用__wt_txn_begin，则会调用CURSOR_UPDATE_API_CALL_BTREE->TXN_API_CALL_NOCONF设置为WT_TXN_AUTOCOMMIT,
//  __wt_txn_autocommit_check中就会自动加上__wt_txn_begin
 */
static inline int
__wt_txn_autocommit_check(WT_SESSION_IMPL *session)
{
    WT_DECL_RET;
    WT_TXN *txn;

    txn = session->txn;
    if (F_ISSET(txn, WT_TXN_AUTOCOMMIT)) {
        //printf("yang test ............__wt_txn_autocommit_check..............\r\n");
        ret = __wt_txn_begin(session, NULL);
        F_CLR(txn, WT_TXN_AUTOCOMMIT);
    }
    return (ret);
}

/*
 * __wt_txn_idle_cache_check --
 *     If there is no transaction active in this thread and we haven't checked if the cache is full,
 *     do it now. If we have to block for eviction, this is the best time to do it.
 */
static inline int
__wt_txn_idle_cache_check(WT_SESSION_IMPL *session)
{
    WT_TXN *txn;
    WT_TXN_SHARED *txn_shared;

    txn = session->txn;
    txn_shared = WT_SESSION_TXN_SHARED(session);

    /*
     * Check the published snap_min because read-uncommitted never sets WT_TXN_HAS_SNAPSHOT. We
     * don't have any transaction information at this point, so assume the transaction will be
     * read-only. The dirty cache check will be performed when the transaction completes, if
     * necessary.
     */
    if (F_ISSET(txn, WT_TXN_RUNNING) && !F_ISSET(txn, WT_TXN_HAS_ID) &&
      txn_shared->pinned_id == WT_TXN_NONE) {
       // printf("yang test ..................__wt_txn_begin...................................\r\n");
        //检查节点已使用内存、脏数据、update数据百分比，判断是否需要用户线程、evict线程进行evict处理
        WT_RET(__wt_cache_eviction_check(session, false, true, NULL));
    }
    return (0);
}

/*
 * __wt_txn_id_alloc --
 *     Allocate a new transaction ID.
 //__wt_txn_id_check->__wt_txn_id_alloc
 */
static inline uint64_t
__wt_txn_id_alloc(WT_SESSION_IMPL *session, bool publish)
{
    WT_TXN_GLOBAL *txn_global;
    WT_TXN_SHARED *txn_shared;
    uint64_t id;

    txn_global = &S2C(session)->txn_global;
    txn_shared = WT_SESSION_TXN_SHARED(session);

    /*
     * Allocating transaction IDs involves several steps.
     *
     * Firstly, publish that this transaction is allocating its ID, then publish the transaction ID
     * as the current global ID. Note that this transaction ID might not be unique among threads and
     * hence not valid at this moment. The flag will notify other transactions that are attempting
     * to get their own snapshot for this transaction ID to retry.
     *
     * Then we do an atomic increment to allocate a unique ID. This will give the valid ID to this
     * transaction that we publish to the global transaction table.
     *
     * We want the global value to lead the allocated values, so that any allocated transaction ID
     * eventually becomes globally visible. When there are no transactions running, the oldest_id
     * will reach the global current ID, so we want post-increment semantics. Our atomic add
     * primitive does pre-increment, so adjust the result here.
     *
     * We rely on atomic reads of the current ID to create snapshots, so for unlocked reads to be
     * well defined, we must use an atomic increment here.
     */
    
    if (publish) {//btree都走这里
        WT_PUBLISH(txn_shared->is_allocating, true);
        WT_PUBLISH(txn_shared->id, txn_global->current);
        id = __wt_atomic_addv64(&txn_global->current, 1) - 1;
        session->txn->id = id;
        WT_PUBLISH(txn_shared->id, id);
        WT_PUBLISH(txn_shared->is_allocating, false);
    } else
        id = __wt_atomic_addv64(&txn_global->current, 1) - 1;
        
    //printf("yang test __wt_txn_id_alloc......., sessionid:%u, session_name:%s, publish:%d, txn_global->current:%lu, session->txn->id:%lu, txn_shared->id:%lu\r\n", 
    //    session->id, session->name, publish, txn_global->current, session->txn->id, txn_shared->id);

    return (id);
}

/*
 * __wt_txn_id_check --
 *     A transaction is going to do an update, allocate a transaction ID.
//__wt_txn_log_op  __wt_txn_ts_log->__txn_logrec_init->__wt_txn_id_check
//__wt_txn_modify->__txn_next_op->__wt_txn_id_check
//checkpoint事务生成事务id: __wt_txn_modify->__txn_next_op->__wt_txn_id_check   __checkpoint_prepare->__wt_txn_id_check
 */
static inline int
__wt_txn_id_check(WT_SESSION_IMPL *session)
{
    WT_TXN *txn;

    txn = session->txn;

    WT_ASSERT(session, F_ISSET(txn, WT_TXN_RUNNING));
   // printf("yang test ...................in __wt_txn_id_check..........befor __wt_txn_id_alloc............\r\n");
    //这里就保证了同一个session下的单次事务的id只有一个
    if (F_ISSET(txn, WT_TXN_HAS_ID))
        return (0);

    /*
     * Return error when the transactions with read committed or uncommitted isolation tries to
     * perform any write operation. Don't return an error for any update on metadata because it uses
     * special transaction visibility rules, search and updates on metadata happens in
     * read-uncommitted and read-committed isolation.
     */
    if (session->dhandle != NULL && !WT_IS_METADATA(session->dhandle) &&
      (txn->isolation == WT_ISO_READ_COMMITTED || txn->isolation == WT_ISO_READ_UNCOMMITTED)) {
        WT_ASSERT(session, !F_ISSET(session, WT_SESSION_INTERNAL));
        WT_RET_MSG(session, ENOTSUP,
          "write operations are not supported in read-committed or read-uncommitted transactions.");
    }

    /* If the transaction is idle, check that the cache isn't full. */
    WT_RET(__wt_txn_idle_cache_check(session));

    WT_IGNORE_RET(__wt_txn_id_alloc(session, true));

    /*
     * If we have used 64-bits of transaction IDs, there is nothing more we can do.
     */
    if (txn->id == WT_TXN_ABORTED)
        WT_RET_MSG(session, WT_ERROR, "out of transaction IDs");
    F_SET(txn, WT_TXN_HAS_ID);

    return (0);
}

/*
 * __wt_txn_search_check --
 *     Check if a search by the current transaction violates timestamp rules.
 */
static inline int
__wt_txn_search_check(WT_SESSION_IMPL *session)
{
    WT_TXN *txn;
    uint16_t flags;
    const char *name;

    txn = session->txn;
    flags = session->dhandle->ts_flags;
    name = session->dhandle->name;

    /* Timestamps are ignored on logged files. */
    //表级log启用，timestamps功能失效
    //mongodb 副本集普通数据表的log是关闭了的，oplog表的log功能是打开的
    if (F_ISSET(S2BT(session), WT_BTREE_LOGGED))
        return (0);

    /* Skip checks during recovery. */
    if (F_ISSET(S2C(session), WT_CONN_RECOVERING))
        return (0);

    /* Verify if the table should always or never use a read timestamp. */
    if (LF_ISSET(WT_DHANDLE_TS_ASSERT_READ_ALWAYS) && !F_ISSET(txn, WT_TXN_SHARED_TS_READ)) {
        __wt_err(session, EINVAL,
          "%s: " WT_TS_VERBOSE_PREFIX "read timestamps required and none set", name);
#ifdef HAVE_DIAGNOSTIC
        __wt_abort(session);
#endif
#ifdef WT_STANDALONE_BUILD
        return (EINVAL);
#endif
    }

    if (LF_ISSET(WT_DHANDLE_TS_ASSERT_READ_NEVER) && F_ISSET(txn, WT_TXN_SHARED_TS_READ)) {
        __wt_err(session, EINVAL,
          "%s: " WT_TS_VERBOSE_PREFIX "read timestamps disallowed and one set", name);
#ifdef HAVE_DIAGNOSTIC
        __wt_abort(session);
#endif
#ifdef WT_STANDALONE_BUILD
        return (EINVAL);
#endif
    }
    return (0);
}

/*
 * __wt_txn_modify_block --
 *     Check if the current transaction can modify an item.
 //判断多线程环境中是否有事务冲突
 */
static inline int
__wt_txn_modify_block(
  WT_SESSION_IMPL *session, WT_CURSOR_BTREE *cbt, WT_UPDATE *upd, wt_timestamp_t *prev_tsp)
{
    WT_DECL_ITEM(buf);
    WT_DECL_RET;
    WT_TIME_WINDOW tw;
    WT_TXN *txn;
    uint32_t snap_count;
    char ts_string[WT_TS_INT_STRING_SIZE];
    bool ignore_prepare_set, rollback, tw_found;

    rollback = tw_found = false;
    txn = session->txn;

    /*
     * Always include prepared transactions in this check: they are not supposed to affect
     * visibility for update operations.
     */
    ignore_prepare_set = F_ISSET(txn, WT_TXN_IGNORE_PREPARE);
    F_CLR(txn, WT_TXN_IGNORE_PREPARE);
    //如果session对应事务对udp所有变更数据都不可见，说明冲突了，因为不能修改该K，这时候需要回滚
    //yang add todo xxxxxxxxxxxxxxxx 这里是否可以增加重试机制????

    //写冲突对应的业务感知字符串为"WT_ROLLBACK: conflict between concurrent operations"  可以参考test_txn27.py模拟的写冲突
    for (; upd != NULL && !__wt_txn_upd_visible(session, upd); upd = upd->next) {
        if (upd->txnid != WT_TXN_ABORTED) {
            //ret = __log_slot_close(session, slot, &release, forced);后增加一个延迟 __wt_sleep(0, 30000);//yang add change  即可模拟出来
            __wt_verbose_debug1(session, WT_VERB_TRANSACTION,
              "Conflict with update with txn id %" PRIu64 " at timestamp: %s", upd->txnid,
              __wt_timestamp_to_string(upd->start_ts, ts_string));
            //如果这里是因为快照冲突引起的，这里是不是可以增加重试机制__txn_visible_id(session, id)，并增加重试超时时间配置
            rollback = true;
            break;
        }
    }

    WT_ASSERT(session, upd != NULL || !rollback);

    /*
     * Check conflict against any on-page value if there is no update on the update chain except
     * aborted updates. Otherwise, we would have either already detected a conflict if we saw an
     * uncommitted update or determined that it would be safe to write if we saw a committed update.
     *
     * In the case of row-store we also need to check that the insert list is empty as the existence
     * of it implies there is no on disk value for the given key. However we can still get a
     * time-window from an unrelated on-disk value if we are not careful as the slot can still be
     * set on the cursor b-tree.
     */
    if (!rollback && upd == NULL && (CUR2BT(cbt)->type != BTREE_ROW || cbt->ins == NULL)) {
        // 从磁盘读取&page->pg_row[cbt->slot]位置的数据并获取对应tw
        tw_found = __wt_read_cell_time_window(cbt, &tw);
        if (tw_found) {
            //磁盘上读取的数据中带有tw信息，则判断该session是否可以访问tw对应事务数据
            if (WT_TIME_WINDOW_HAS_STOP(&tw)) {
                rollback = !__wt_txn_tw_stop_visible(session, &tw);
                if (rollback)
                    __wt_verbose_debug1(session, WT_VERB_TRANSACTION,
                      "Conflict with update %" PRIu64 " at stop timestamp: %s", tw.stop_txn,
                      __wt_timestamp_to_string(tw.stop_ts, ts_string));
            } else {
                rollback = !__wt_txn_tw_start_visible(session, &tw);
                if (rollback)
                    __wt_verbose_debug1(session, WT_VERB_TRANSACTION,
                      "Conflict with update %" PRIu64 " at start timestamp: %s", tw.start_txn,
                      __wt_timestamp_to_string(tw.start_ts, ts_string));
            }
        }
    }

    if (rollback) {//回滚操作
        /* Dump information about the txn snapshot. */
        if (WT_VERBOSE_LEVEL_ISSET(session, WT_VERB_TRANSACTION, WT_VERBOSE_DEBUG_1)) {
            WT_ERR(__wt_scr_alloc(session, 1024, &buf));
            WT_ERR(__wt_buf_fmt(session, buf,
              "__wt_txn_modify_block rolback snapshot_min=%" PRIu64 ", snapshot_max=%" PRIu64 ", snapshot_count=%" PRIu32,
              txn->snap_min, txn->snap_max, txn->snapshot_count));
            if (txn->snapshot_count > 0) {
                WT_ERR(__wt_buf_catfmt(session, buf, ", snapshots=["));
                for (snap_count = 0; snap_count < txn->snapshot_count - 1; ++snap_count)
                    WT_ERR(
                      __wt_buf_catfmt(session, buf, "%" PRIu64 ",", txn->snapshot[snap_count]));
                WT_ERR(__wt_buf_catfmt(session, buf, "%" PRIu64 "]", txn->snapshot[snap_count]));
            }
            __wt_verbose_debug1(session, WT_VERB_TRANSACTION, "%s", (const char *)buf->data);
        }

        //回滚统计
        WT_STAT_CONN_DATA_INCR(session, txn_update_conflict);
        //设置回滚原因
        ret = __wt_txn_rollback_required(session, WT_TXN_ROLLBACK_REASON_CONFLICT);
    }

    /*
     * Don't access the update from an uncommitted transaction as it can produce wrong timestamp
     * results.
     */
    if (!rollback && prev_tsp != NULL) {
        if (upd != NULL) {
            /*
             * The durable timestamp must be greater than or equal to the commit timestamp unless it
             * is an in-progress prepared update.
             */
            WT_ASSERT(session,
              upd->durable_ts >= upd->start_ts || upd->prepare_state == WT_PREPARE_INPROGRESS);
            *prev_tsp = upd->durable_ts;
        } else if (tw_found)
            *prev_tsp = WT_TIME_WINDOW_HAS_STOP(&tw) ? tw.durable_stop_ts : tw.durable_start_ts;
    }

    if (ignore_prepare_set)
        F_SET(txn, WT_TXN_IGNORE_PREPARE);

err:
    __wt_scr_free(session, &buf);
    return (ret);
}

/*
 * __wt_txn_modify_check --
 *     Check if the current transaction can modify an item.
 __wt_upd_alloc中插入，如果删除K了，则在__wt_txn_modify_check中返回不存在
 */
static inline int
__wt_txn_modify_check(WT_SESSION_IMPL *session, WT_CURSOR_BTREE *cbt, WT_UPDATE *upd,
  wt_timestamp_t *prev_tsp, u_int modify_type)
{
    WT_TXN *txn;
    WT_TXN_GLOBAL *txn_global;

    txn = session->txn;

    /*
     * Check if this operation is permitted, skipping if transaction isolation is not snapshot or
     * operating on the metadata table.
     */
    //判断本session对udp是否有可见性，如果不可见则代表冲突了
    if (txn->isolation == WT_ISO_SNAPSHOT && !WT_IS_METADATA(cbt->dhandle))
        WT_RET(__wt_txn_modify_block(session, cbt, upd, prev_tsp));

    /*
     * Prepending a tombstone to another tombstone indicates remove of a non-existent key and that
     * isn't permitted, return a WT_NOTFOUND error.
     */
    //udp多版本链表中找出第一个不为WT_TXN_ABORTED或者WT_UPDATE_TOMBSTONE的V
    //这里可以看出如果udp链表上面的第一个有效V已经标记为WT_UPDATE_TOMBSTONE，则直接返回WT_NOTFOUND
    if (modify_type == WT_UPDATE_TOMBSTONE) {
        /* Loop until a valid update is found. */
        while (upd != NULL && upd->txnid == WT_TXN_ABORTED)
            upd = upd->next;

        //yang add ?????????????????? 这个最新的删除的K和空的V，什么适合从insert跳表摘除并释放空间呢
        //reconcile的时候__split_multi_inmem_final  __wt_page_out中会释放
        if (upd != NULL && upd->type == WT_UPDATE_TOMBSTONE)
            return (WT_NOTFOUND);
    }

    /* Everything is OK, optionally rollback for testing (skipping metadata operations). */
    if (!WT_IS_METADATA(cbt->dhandle)) {
        txn_global = &S2C(session)->txn_global;
         //debug_mode.rollback_error配置来模拟回滚，该配置标识没debug_rollback次operation有一次需要回滚
        if (txn_global->debug_rollback != 0 &&
          ++txn_global->debug_ops % txn_global->debug_rollback == 0)
            return (__wt_txn_rollback_required(session, "debug mode simulated conflict"));
    }
    return (0);
}

/*
 * __wt_txn_read_last --
 *     Called when the last page for a session is released.
 */
static inline void
__wt_txn_read_last(WT_SESSION_IMPL *session)
{
    WT_TXN *txn;

    txn = session->txn;

    /*
     * Release the snap_min ID we put in the global table.
     *
     * If the isolation has been temporarily forced, don't touch the snapshot here: it will be
     * restored by WT_WITH_TXN_ISOLATION.
     */
    if ((!F_ISSET(txn, WT_TXN_RUNNING) || txn->isolation != WT_ISO_SNAPSHOT) &&
      txn->forced_iso == 0)
        __wt_txn_release_snapshot(session);
}

/*
 * __wt_txn_cursor_op --
 *     Called for each cursor operation.
 curosr读写都会调用__wt_cursor_func_init->__wt_txn_cursor_op
 从这里可以看出，如果是WT_ISO_READ_COMMITTED每次读写都会重新获取快照
 */
static inline void
__wt_txn_cursor_op(WT_SESSION_IMPL *session)
{
    WT_TXN *txn;
    WT_TXN_GLOBAL *txn_global;
    WT_TXN_SHARED *txn_shared;

    txn = session->txn;
    txn_global = &S2C(session)->txn_global;
    txn_shared = WT_SESSION_TXN_SHARED(session);

    /*
     * We are about to read data, which means we need to protect against
     * updates being freed from underneath this cursor. Read-uncommitted
     * isolation protects values by putting a transaction ID in the global
     * table to prevent any update that we are reading from being freed.
     * Other isolation levels get a snapshot to protect their reads.
     *
     * !!!
     * Note:  We are updating the global table unprotected, so the global
     * oldest_id may move past our snap_min if a scan races with this value
     * being published. That said, read-uncommitted operations always see
     * the most recent update for each record that has not been aborted
     * regardless of the snap_min value published here.  Even if there is a
     * race while publishing this ID, it prevents the oldest ID from moving
     * further forward, so that once a read-uncommitted cursor is
     * positioned on a value, it can't be freed.
     */
    if (txn->isolation == WT_ISO_READ_UNCOMMITTED) {//如果是WT_ISO_READ_UNCOMMITTED，不会获取当前其他事务快照信息
        if (txn_shared->pinned_id == WT_TXN_NONE)
            txn_shared->pinned_id = txn_global->last_running;
        if (txn_shared->metadata_pinned == WT_TXN_NONE)
            txn_shared->metadata_pinned = txn_shared->pinned_id;
    } else if (!F_ISSET(txn, WT_TXN_HAS_SNAPSHOT)) {//说明是WT_ISO_READ_COMMITTED, 
    //配合__wt_txn_begin阅读，如果是WT_ISO_READ_COMMITTED,则在__wt_txn_begin不会获取快照，而是在这里获取快照
        //printf("yang test ..............__wt_txn_cursor_op...........\r\n");
        //如果没有显示的定义txn begain，直接写入，则会进入这里
        // 从这里可以看出，如果是WT_ISO_READ_COMMITTED每次读写都会重新获取快照
        __wt_txn_get_snapshot(session);
    }
}

/*
 * __wt_txn_activity_check --
 *     Check whether there are any running transactions.
 //判断当前是否有在running的事务
 */
static inline int
__wt_txn_activity_check(WT_SESSION_IMPL *session, bool *txn_active)
{
    WT_TXN_GLOBAL *txn_global;

    txn_global = &S2C(session)->txn_global;

    /*
     * Default to true - callers shouldn't rely on this if an error is returned, but let's give them
     * deterministic behavior if they do.
     */
    *txn_active = true;

    /*
     * Ensure the oldest ID is as up to date as possible so we can use a simple check to find if
     * there are any running transactions.
     */
    WT_RET(__wt_txn_update_oldest(session, WT_TXN_OLDEST_STRICT | WT_TXN_OLDEST_WAIT));

    *txn_active = (txn_global->oldest_id != txn_global->current ||
      txn_global->metadata_pinned != txn_global->current);

    return (0);
}

/*
 * __wt_upd_value_assign --
 *     Point an update value at a given update. We're specifically not getting the value to own the
 *     memory since this exists in an update list somewhere.
 */
static inline void
__wt_upd_value_assign(WT_UPDATE_VALUE *upd_value, WT_UPDATE *upd)
{
    if (!upd_value->skip_buf) {
        upd_value->buf.data = upd->data;
        upd_value->buf.size = upd->size;
    }
    if (upd->type == WT_UPDATE_TOMBSTONE) {
        upd_value->tw.durable_stop_ts = upd->durable_ts;
        upd_value->tw.stop_ts = upd->start_ts;
        upd_value->tw.stop_txn = upd->txnid;
        upd_value->tw.prepare =
          upd->prepare_state == WT_PREPARE_INPROGRESS || upd->prepare_state == WT_PREPARE_LOCKED;
    } else {
        upd_value->tw.durable_start_ts = upd->durable_ts;
        upd_value->tw.start_ts = upd->start_ts;
        upd_value->tw.start_txn = upd->txnid;
        upd_value->tw.prepare =
          upd->prepare_state == WT_PREPARE_INPROGRESS || upd->prepare_state == WT_PREPARE_LOCKED;
    }
    upd_value->type = upd->type;
}

/*
 * __wt_upd_value_clear --
 *     Clear an update value to its defaults.
 */
static inline void
__wt_upd_value_clear(WT_UPDATE_VALUE *upd_value)
{
    /*
     * Make sure we don't touch the memory pointers here. If we have some allocated memory, that
     * could come in handy next time we need to write to the buffer.
     */
    upd_value->buf.data = NULL;
    upd_value->buf.size = 0;
    WT_TIME_WINDOW_INIT(&upd_value->tw);
    upd_value->type = WT_UPDATE_INVALID;
}
