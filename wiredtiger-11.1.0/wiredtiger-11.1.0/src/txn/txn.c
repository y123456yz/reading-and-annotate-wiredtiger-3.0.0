/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

/*
 * __snapsort_partition --
 *     Custom quick sort partitioning for snapshots.
 */
static uint32_t
__snapsort_partition(uint64_t *array, uint32_t f, uint32_t l, uint64_t pivot)
{
    uint32_t i, j;

    i = f - 1;
    j = l + 1;
    for (;;) {
        while (pivot < array[--j])
            ;
        while (array[++i] < pivot)
            ;
        if (i < j) {
            uint64_t tmp = array[i];
            array[i] = array[j];
            array[j] = tmp;
        } else
            return (j);
    }
}

/*
 * __snapsort_impl --
 *     Custom quick sort implementation for snapshots.
 */
static void
__snapsort_impl(uint64_t *array, uint32_t f, uint32_t l)
{
    while (f + 16 < l) {
        uint64_t v1 = array[f], v2 = array[l], v3 = array[(f + l) / 2];
        uint64_t median =
          v1 < v2 ? (v3 < v1 ? v1 : WT_MIN(v2, v3)) : (v3 < v2 ? v2 : WT_MIN(v1, v3));
        uint32_t m = __snapsort_partition(array, f, l, median);
        __snapsort_impl(array, f, m);
        f = m + 1;
    }
}

/*
 * __snapsort --
 *     Sort an array of transaction IDs.
 */
static void
__snapsort(uint64_t *array, uint32_t size)
{
    __snapsort_impl(array, 0, size - 1);
    WT_INSERTION_SORT(array, size, uint64_t, WT_TXNID_LT);
}

/*
 * __txn_remove_from_global_table --
 *     Remove the transaction id from the global transaction table.
 */
//°Ñtxn_shared_list[]Êý×éÖÐsession¶ÔÓ¦idÖÃÎ»WT_TXN_NONE, Ò²¾ÍÊÇ±¾session¹ÜÀíµÄÊÂÎñidÇå0
static inline void
__txn_remove_from_global_table(WT_SESSION_IMPL *session)
{
#ifdef HAVE_DIAGNOSTIC
    WT_TXN *txn;
    WT_TXN_GLOBAL *txn_global;
    WT_TXN_SHARED *txn_shared;

    txn = session->txn;
    txn_global = &S2C(session)->txn_global;
    txn_shared = WT_SESSION_TXN_SHARED(session);

    WT_ASSERT(session, !WT_TXNID_LT(txn->id, txn_global->last_running));
    WT_ASSERT(session, txn->id != WT_TXN_NONE && txn_shared->id != WT_TXN_NONE);
#else
    WT_TXN_SHARED *txn_shared;

    txn_shared = WT_SESSION_TXN_SHARED(session);
#endif
    WT_PUBLISH(txn_shared->id, WT_TXN_NONE);
}

/*
 * __txn_sort_snapshot --
 *     Sort a snapshot for faster searching and set the min/max bounds.

  __txn_sort_snapshot __txn_get_snapshot_intºÍ__wt_txn_release_snapshot¶ÔÓ¦
  
 //session->txnµÄsnap_minºÍsnap_max¸³Öµ
 //¶Ô¸ÃsessionÄÜ¿´µ½µÄËùÓÐÊÂÎñ°´ÕÕÊÂÎñid½øÐÐÅÅÐò£¬²¢¼ÇÂ¼ÄÜ¿´µ½µÄËùÓÐÊÂÎñidµÄ×îÐ¡ÖµºÍ×î´óÖµ
 */
static void
__txn_sort_snapshot(WT_SESSION_IMPL *session, uint32_t n, uint64_t snap_max)
{
    WT_TXN *txn;

    txn = session->txn;

    if (n > 1)
        __snapsort(txn->snapshot, n);

    txn->snapshot_count = n;
    txn->snap_max = snap_max;
    txn->snap_min =
      (n > 0 && WT_TXNID_LE(txn->snapshot[0], snap_max)) ? txn->snapshot[0] : snap_max;
    F_SET(txn, WT_TXN_HAS_SNAPSHOT);
    WT_ASSERT(session, n == 0 || txn->snap_min != WT_TXN_NONE);
}

/*
 * __wt_txn_release_snapshot --
 *     Release the snapshot in the current transaction.
 //[1701957314:925541][56918:0x7fe1ab82e800], close_ckpt: [WT_VERB_CHECKPOINT_PROGRESS][DEBUG_1]: saving checkpoint snapshot
 //  min: 10005, snapshot max: 10005 snapshot count: 0, oldest timestamp: (0, 0) , meta checkpoint timestamp: (0, 0) base write gen: 1

 __txn_sort_snapshot __txn_get_snapshot_intºÍ__wt_txn_release_snapshot¶ÔÓ¦

 */
void
__wt_txn_release_snapshot(WT_SESSION_IMPL *session)
{
    WT_TXN *txn;
    WT_TXN_GLOBAL *txn_global;
    WT_TXN_SHARED *txn_shared;

    txn = session->txn;
    txn_global = &S2C(session)->txn_global;
    txn_shared = WT_SESSION_TXN_SHARED(session);

    WT_ASSERT(session,
      txn_shared->pinned_id == WT_TXN_NONE || session->txn->isolation == WT_ISO_READ_UNCOMMITTED ||
        !__wt_txn_visible_all(session, txn_shared->pinned_id, WT_TS_NONE));

    txn_shared->metadata_pinned = txn_shared->pinned_id = WT_TXN_NONE;
    F_CLR(txn, WT_TXN_HAS_SNAPSHOT);

    //txn_shared->id=WT_TXN_NONE  ÔÚÍâ²ãµÄ__wt_txn_releaseÖÐµÄ__txn_remove_from_global_tableÖÃÎª0

    /* Clear a checkpoint's pinned ID and timestamp. */
    if (WT_SESSION_IS_CHECKPOINT(session)) {
        txn_global->checkpoint_txn_shared.pinned_id = WT_TXN_NONE;
        txn_global->checkpoint_timestamp = WT_TS_NONE;
    }
}

/*
 * __wt_txn_active --
 *     Check if a transaction is still active. If not, it is either committed, prepared, or rolled
 *     back. It is possible that we race with commit, prepare or rollback and a transaction is still
 *     active before the start of the call is eventually reported as resolved.
 */
//ÅÐ¶Ïtxnid¶ÔÓ¦ÊÂÎñÊÇ·ñÒÑ¾­Ìá½»»òÕß»Ø¹ö»òÕßprepared, Ò²¾ÍÊÇ¸ÃÊÂÎñidÐ¡ÓÚÈ«¾Öoldest_id »òÕß ÊÂÎñid´óÓÚÈ«¾Öoldest_id²¢ÇÒÔÚtxn_shared_listÖÐ¿ÉÒÔÕÒµ½
//txid¶ÔÓ¦ÊÂÎñÊÇ·ñÔÚtxn_shared_listÈ«¾Ö¹²ÏíÊý×éÖÐ¿ÉÒÔÕÒµ½
bool
__wt_txn_active(WT_SESSION_IMPL *session, uint64_t txnid)
{
    WT_CONNECTION_IMPL *conn;
    WT_TXN_GLOBAL *txn_global;
    WT_TXN_SHARED *s;
    uint64_t oldest_id;
    uint32_t i, session_cnt;
    bool active;

    conn = S2C(session);
    txn_global = &conn->txn_global;
    active = true;

    /* We're going to scan the table: wait for the lock. */
    __wt_readlock(session, &txn_global->rwlock);
    oldest_id = txn_global->oldest_id;

    if (WT_TXNID_LT(txnid, oldest_id)) {
        active = false;
        goto done;
    }

    /* Walk the array of concurrent transactions. */
    WT_ORDERED_READ(session_cnt, conn->session_cnt);
    WT_STAT_CONN_INCR(session, txn_walk_sessions);
    for (i = 0, s = txn_global->txn_shared_list; i < session_cnt; i++, s++) {
        WT_STAT_CONN_INCR(session, txn_sessions_walked);
        /* If the transaction is in the list, it is uncommitted. */
        if (s->id == txnid)
            goto done;
    }

    active = false;
done:
    __wt_readunlock(session, &txn_global->rwlock);
    return (active);
}

/*
 * __txn_get_snapshot_int --
 *     Allocate a snapshot, optionally update our shared txn ids.
   __txn_sort_snapshot __txn_get_snapshot_intºÍ__wt_txn_release_snapshot¶ÔÓ¦
 */
static void
__txn_get_snapshot_int(WT_SESSION_IMPL *session, bool publish)
{
    WT_CONNECTION_IMPL *conn;
    WT_TXN *txn;
    WT_TXN_GLOBAL *txn_global;
    WT_TXN_SHARED *s, *txn_shared;
    uint64_t commit_gen, current_id, id, prev_oldest_id, pinned_id;
    uint32_t i, n, session_cnt;

    conn = S2C(session);
    txn = session->txn;
    txn_global = &conn->txn_global;
    txn_shared = WT_SESSION_TXN_SHARED(session);
    n = 0;

    /* Fast path if we already have the current snapshot. */
    //Í¬Ò»¸öÊÂÎñ¿ÉÄÜ¶à´Îµ÷ÓÃ¸Ã½Ó¿Ú£¬Èç¹ûÁ½´Î¼ä¸ôÆÚ¼äsession gen»¹ÊÇºÍÈ«¾Öconn genÒ»Ñù£¬
    //  ËµÃ÷Ã»ÓÐÐÂÔöÆäËûÐÂÊÂÎñ£¬¿ìÕÕsnapshotÃ»ÓÐ±ä»¯£¬ÎÞÐèÖØ¸´×ßÏÂÃæµÄÁ÷³ÌÁË

    //ËµÃ÷¸ÃÊÂÎñµÄÇ°ºóÁ½¸ö¶ÁÐ´²Ù×÷ÆÚ¼äÃ»ÓÐÆäËûÊÂÎñÌá½»__wt_txn_commit£¬Òò´Ë¸ÃÊÂÎñµÄ¿ìÕÕÐÅÏ¢²»»áÓÐ±ä»¯£
    //WT_ISO_READ_COMMITTEDÃ¿´Î¶ÁÐ´¶¼»áÖØÐÂ»ñÈ¡¿ìÕÕ£¬Òò´ËWT_ISO_READ_COMMITTED¸ôÀë¼¶±ðÒ»¸öÊÂÎñÖÐÓÐ¶à¸ö¶ÁÐ´²Ù×÷£¬¾Í»á½øÀ´¶à´Î
    if ((commit_gen = __wt_session_gen(session, WT_GEN_COMMIT)) != 0) {
        if (F_ISSET(txn, WT_TXN_HAS_SNAPSHOT) && commit_gen == __wt_gen(session, WT_GEN_COMMIT))
            return;
        __wt_session_gen_leave(session, WT_GEN_COMMIT);
    }
    //°Ñµ±Ç°µÄÈ«¾Öconn gen¼ÇÂ¼µ½session genÖÐ
    __wt_session_gen_enter(session, WT_GEN_COMMIT);

    /* We're going to scan the table: wait for the lock. */
    __wt_readlock(session, &txn_global->rwlock);

    current_id = pinned_id = txn_global->current;
    prev_oldest_id = txn_global->oldest_id;

    /*
     * Include the checkpoint transaction, if one is running: we should ignore any uncommitted
     * changes the checkpoint has written to the metadata. We don't have to keep the checkpoint's
     * changes pinned so don't go including it in the published pinned ID.
     *
     * We can assume that if a function calls without intention to publish then it is the special
     * case of checkpoint calling it twice. In which case do not include the checkpoint id.
     */
    //ÆÕÍ¨ÊÂÎñ»ñÈ¡¿ìÕÕÊ±ºò£¬Èç¹ûµ±Ç°ÓÐÕýÔÚ×öcheckpointµÄÊÂÎñ,checkpointÏß³Ì»á½øÈëÕâÀïÃæ»ñÈ¡checkpoint¶ÔÓ¦sessionµÄÊÂÎñID
    if ((id = txn_global->checkpoint_txn_shared.id) != WT_TXN_NONE) {
        if (txn->id != id)
            //Ò²¾ÍÊÇ¼ÇÂ¼¶ÔÓ¦µÃcheckpoint idÐÅÏ¢
            txn->snapshot[n++] = id;
        //printf("yang test .........__txn_get_snapshot_int......%d, %lu\r\n", publish, id);    
        if (publish)
            //Ò²¾ÍÊÇµ±Ç°ÊÂÎñ½øÐÐÖÐµÄÊ±ºò£¬ÆäËûÏß³Ì
            txn_shared->metadata_pinned = id;
    }
    
   /* {
        char buf[512];

        
        snprintf(buf, 512, "session:%p, yang test __txn_get_snapshot_int....session id:%u, session txn id:%lu, current_id:%lu, prev_oldest_id:%lu, txn_global->checkpoint_txn_shared.id:%lu\r\n\r\n",
            session, session->id, txn->id, current_id, prev_oldest_id, txn_global->checkpoint_txn_shared.id);
        __wt_verbose_debug1(
                  session, WT_VERB_TRANSACTION, "%s", buf);
    }*/
    
    /* For pure read-only workloads, avoid scanning. */
    if (prev_oldest_id == current_id) {
        pinned_id = current_id;
        /* Check that the oldest ID has not moved in the meantime. */
        WT_ASSERT(session, prev_oldest_id == txn_global->oldest_id);
        goto done;
    }

    /* Walk the array of concurrent transactions. */
    //´ú±íÊµÀýÖÐÍ¬Ê±ÔÚÊ¹ÓÃµÄ×î´ósessionÊýÁ¿
    WT_ORDERED_READ(session_cnt, conn->session_cnt);
    //transaction walk of concurrent sessions
    WT_STAT_CONN_INCR(session, txn_walk_sessions);
    //»ñÈ¡txn_shared_list[]Êý×éÖÐ£¬»ñÈ¡ËùÓÐ´óÓÚprev_oldest_idµÄsessionÊÂÎñ
    //Ò²¾ÍÊÇ»ñÈ¡µ±Ç°session¿ÉÒÔ¿´µ½µÄËùÓÐÆäËûÊÂÎñ(´óÓÚprev_oldest_idµÄsessionÊÂÎñ)
    for (i = 0, s = txn_global->txn_shared_list; i < session_cnt; i++, s++) {
        WT_STAT_CONN_INCR(session, txn_sessions_walked);
        /*
         * Build our snapshot of any concurrent transaction IDs.
         *
         * Ignore:
         *  - Our own ID: we always read our own updates.
         *  - The ID if it is older than the oldest ID we saw. This
         *    can happen if we race with a thread that is allocating
         *    an ID -- the ID will not be used because the thread will
         *    keep spinning until it gets a valid one.
         *  - The ID if it is higher than the current ID we saw. This
         *    can happen if the transaction is already finished. In
         *    this case, we ignore this transaction because it would
         *    not be visible to the current snapshot.
         */
       // printf("yang test ........................i=%u, txn_shared_list[%u].id=%lu\r\n", i, i, s->id);
        //txn_global->txn_shared_list[]Êý×éÖÐ²»ÊÇµ±Ç°session id£¬²¢ÇÒÊÂÎñid²»Îª0£¬
        while (s != txn_shared && (id = s->id) != WT_TXN_NONE && WT_TXNID_LE(prev_oldest_id, id) &&
          WT_TXNID_LT(id, current_id)) {
            /*
             * If the transaction is still allocating its ID, then we spin here until it gets its
             * valid ID.
             */
            WT_READ_BARRIER();
            if (!s->is_allocating) {
                /*
                 * There is still a chance that fetched ID is not valid after ID allocation, so we
                 * check again here. The read of transaction ID should be carefully ordered: we want
                 * to re-read ID from transaction state after this transaction completes ID
                 * allocation.
                 */
                WT_READ_BARRIER();
                //id´ú±íµÄÊÇÊÂÎñid
                if (id == s->id) {
                    //nÒ²¾ÍÊÇµ±Ç°txn_shared_list[]Êý×éÖÐ³ý×Ô¼ºÒÔÍâµÄËùÓÐ´óÓÚprev_oldest_idµÄsessionÊÂÎñ×ÜÊý
                    txn->snapshot[n++] = id;
                    if (WT_TXNID_LT(id, pinned_id))
                        pinned_id = id;
                    break;
                }
            }
            WT_PAUSE();
        }
    }

    /*
     * If we got a new snapshot, update the published pinned ID for this session.
     */
    WT_ASSERT(session, WT_TXNID_LE(prev_oldest_id, pinned_id));
    WT_ASSERT(session, prev_oldest_id == txn_global->oldest_id);
done:
    if (publish)
        txn_shared->pinned_id = pinned_id;
    __wt_readunlock(session, &txn_global->rwlock);
    __txn_sort_snapshot(session, n, current_id);
    {
       // int ret = __wt_verbose_dump_txn(session, "__txn_get_snapshot_int");
      //  (void)(ret);
    }
}

/*
 * __wt_txn_get_snapshot --
 *     Common case, allocate a snapshot and update our shared ids.
  __wt_txn_begin->__wt_txn_get_snapshot( ×¢ÒâÕâÀïÖ»ÓÐWT_ISO_SNAPSHOT»á»ñÈ¡¿ìÕÕ£¬WT_ISO_READ_COMMITTEDºÍWT_ISO_READ_UNCOMMITTED²»»á»ñÈ¡¿ìÕÕ)
  curosr¶ÁÐ´¶¼»áµ÷ÓÃ__wt_cursor_func_init->__wt_txn_cursor_op->__wt_txn_get_snapshot
  //»ñÈ¡ÊÂÎñ¿ìÕÕ£¬Êµ¼ÊÉÏÆÕÍ¨µÄÐ´²Ù×÷Èç¹û²»ÏÔÊ¾Ö¸¶¨transaction_begin  transaction_commitµÈ²Ù×÷£¬Ò²»áµ±ÆÕÍ¨ÊÂÎñÁ÷³Ì´¦Àí
  // 1. __wt_cursor_func_init->__wt_txn_cursor_op->__wt_txn_get_snapshot»ñÈ¡¿ìÕÕ
  // 2. TXN_API_ENDÖÐµ÷ÓÃ__wt_txn_commitÌá½»ÊÂÎñ
 */
void
__wt_txn_get_snapshot(WT_SESSION_IMPL *session)
{
    __txn_get_snapshot_int(session, true);
}

/*
 * __wt_txn_bump_snapshot --
 *     Uncommon case, allocate a snapshot but skip updating our shared ids.
 */
void
__wt_txn_bump_snapshot(WT_SESSION_IMPL *session)
{
    __txn_get_snapshot_int(session, false);
}

/*
 * __txn_oldest_scan --
 *     Sweep the running transactions to calculate the oldest ID required.

oldest ID: 22
durable timestamp: (0, 20)
oldest timestamp: (0, 0)
pinned timestamp: (0, 0)
stable timestamp: (0, 0)
has_durable_timestamp: yes
has_oldest_timestamp: no
has_pinned_timestamp: no
has_stable_timestamp: no
oldest_is_pinned: no
stable_is_pinned: no
checkpoint running: yes
checkpoint generation: 2
checkpoint pinned ID: 22
checkpoint txn ID: 29
session count: 22
Transaction state of active sessions:
session ID: 16, txn ID: 31, pinned ID: 22, metadata pinned ID: 29, name: connection-open-session
transaction id: 31, mod count: 3, snap min: 22, snap max: 31, snapshot count: 5, snapshot: [22, 24, 26, 28, 29], commit_timestamp: (0, 22), durable_timestamp: (0, 22), first_commit_timestamp: (0, 21), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 21), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
session ID: 18, txn ID: 33, pinned ID: 24, metadata pinned ID: 29, name: connection-open-session
transaction id: 33, mod count: 3, snap min: 24, snap max: 33, snapshot count: 5, snapshot: [24, 26, 28, 29, 31], commit_timestamp: (0, 24), durable_timestamp: (0, 24), first_commit_timestamp: (0, 23), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 23), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
session ID: 19, txn ID: 35, pinned ID: 26, metadata pinned ID: 29, name: connection-open-session
transaction id: 35, mod count: 3, snap min: 26, snap max: 35, snapshot count: 5, snapshot: [26, 28, 29, 31, 33], commit_timestamp: (0, 26), durable_timestamp: (0, 26), first_commit_timestamp: (0, 25), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 25), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
session ID: 20, txn ID: 37, pinned ID: 28, metadata pinned ID: 29, name: connection-open-session
transaction id: 37, mod count: 3, snap min: 28, snap max: 37, snapshot count: 5, snapshot: [28, 29, 31, 33, 35], commit_timestamp: (0, 28), durable_timestamp: (0, 28), first_commit_timestamp: (0, 27), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 27), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
//ÕâÀïÎªÊ²Ã´pinned ID: 31£¬ÒòÎªpinned ID²»°üÀ¨ snapshot:[]ÖÐµÄcheckpoint¶ÔÓ¦ÊÂÎñid£¬Òò´ËÐèÒªÌÞ³ý29£¬29ÊÇcheckpointÊÂÎñid
session ID: 21, txn ID: 39, pinned ID: 31, metadata pinned ID: 29, name: connection-open-session
transaction id: 39, mod count: 3, snap min: 29, snap max: 39, snapshot count: 5, snapshot: [29, 31, 33, 35, 37], commit_timestamp: (0, 30), durable_timestamp: (0, 30), first_commit_timestamp: (0, 29), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 29), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
checkpoint session ID: 22, txn ID: 29, pinned ID: 22, metadata pinned ID: 0, name: eviction-server

 
//ÒÔÉÏÃæµÄÕâ¸öÀý×ÓÎªÀý:
//  µ±Ç°ËùÓÐsessionÊÂÎñtxn ID·Ö±ðÊÇ: 31  33  35  37  39,  ÕâÀï¿ÉÒÔ¿´³ölast_running = 31£¬Ò²¾ÍÊÇµ±Ç°ÕýÔÚÔËÐÐµÄËùÓÐÊÂÎñµÄ×îÐ¡ÊÂÎñid
//  metadata pinned ID·Ö±ðÊÇ: 29  29  29  29  29, Ò²¾ÍÊÇ¶ÔÓ¦ÊÂÎñsnapshot[]ÖÐµÄcheckpointÊÂÎñ,  ÕâÀï¿ÉÒÔ¿´³ömetadata_pinned = 29
//  pinned ID·Ö±ðÊÇ: 22 24 26 28 31, Ò²¾ÍÊÇ¶ÔÓ¦ÊÂÎñsnapshot[]ÖÐ³ýÁËcheckpointÊÂÎñÒÔÍâµÄÊÂÎñidµÄ×îÐ¡Öµ, ÕâÀï¿ÉÒÔ¿´³öoldest_id = 22
//  ×îÖÕ: 
//     oldest_id=22,     Ò²¾ÍÊÇµ±Ç°ËùÓÐÊÂÎñµÄsnapshot[]ÖÐµÄ¿ìÕÕÖÐ³ýÁËcheckpointÊÂÎñidÒÔÍâµÄ¿ìÕÕ×îÐ¡Öµ
//     last_running=31,  Ò²¾ÍÊÇµ±Ç°ÕýÔÚÔËÐÐµÄÊÂÎñµÄid×îÐ¡Öµ(²»°üÀ¨checkpointÊÂÎñid)
//     metadata_pinned=22 Ò²¾ÍÊÇµ±Ç°ËùÓÐÊÂÎñµÄsnapshot[]ÖÐµÄÊÂÎñ(°üÀ¨checkpointÊÂÎñid)µÄid×îÐ¡Öµ
 */
static void
__txn_oldest_scan(WT_SESSION_IMPL *session, uint64_t *oldest_idp, uint64_t *last_runningp,
  uint64_t *metadata_pinnedp, WT_SESSION_IMPL **oldest_sessionp)
{
    WT_CONNECTION_IMPL *conn;
    WT_SESSION_IMPL *oldest_session;
    WT_TXN_GLOBAL *txn_global;
    WT_TXN_SHARED *s;
    uint64_t id, last_running, metadata_pinned, oldest_id, prev_oldest_id;
    uint32_t i, session_cnt;

    conn = S2C(session);
    txn_global = &conn->txn_global;
    oldest_session = NULL;

    /* The oldest ID cannot change while we are holding the scan lock. */
    prev_oldest_id = txn_global->oldest_id;
    last_running = oldest_id = txn_global->current;

    //Èç¹ûµ±Ç°ÓÐÆäËûÏß³ÌÕýÔÚ×öcheckpointÔòmetadata_pinnedÎªÕýÔÚ×öcheckpointµÄÏß³Ì¶ÔÓ¦ÊÂÎñid
    //Èç¹ûÓÐÆäËûÏß³ÌÔÚ×öcheckpoint£¬Ôò¿ÉÄÜmetadata_pinned»áÐ¡ÓÚµ±Ç°ËùÓÐÆäËûÊÂÎñµÄ×îÐ¡id
    if ((metadata_pinned = txn_global->checkpoint_txn_shared.id) == WT_TXN_NONE)
        metadata_pinned = oldest_id;

    /* Walk the array of concurrent transactions. */
    WT_ORDERED_READ(session_cnt, conn->session_cnt);
    WT_STAT_CONN_INCR(session, txn_walk_sessions);
    //»ñÈ¡µ±Ç°ËùÓÐÊÂÎñidÖÐµÄ×îÐ¡ÊÂÎñid¸³Öµ¸ølast_running
    for (i = 0, s = txn_global->txn_shared_list; i < session_cnt; i++, s++) {
        WT_STAT_CONN_INCR(session, txn_sessions_walked);
        /* Update the last running transaction ID. */
        //»ñÈ¡ËùÓÐsession¶ÔÓ¦ÊÂÎñidÔÚ[prev_oldest_id, last_running]Çø¼äµÄshared txn

        //ÕâÀïwhileÒ²¾ÍÊÇ»ñÈ¡ËùÓÐÎ´Ìá½»ÊÂÎñÖÐ´óÓÚtxn_global->oldest_idµÄ×îÐ¡ÊÂÎñid¸³Öµ¸ølast_running
        while ((id = s->id) != WT_TXN_NONE && WT_TXNID_LE(prev_oldest_id, id) &&
          //»¹ÓÐ±Èlast_running¸üÐ¡µÄÊÂÎñid
          WT_TXNID_LT(id, last_running)) {
            /*
             * If the transaction is still allocating its ID, then we spin here until it gets its
             * valid ID.
             */
            WT_READ_BARRIER();
            if (!s->is_allocating) {
                /*
                 * There is still a chance that fetched ID is not valid after ID allocation, so we
                 * check again here. The read of transaction ID should be carefully ordered: we want
                 * to re-read ID from transaction state after this transaction completes ID
                 * allocation.
                 */
                WT_READ_BARRIER();
                if (id == s->id) {
                    last_running = id;
                    break;
                }
            }
            WT_PAUSE();
        }

        /* Update the metadata pinned ID. */
        //Ò²¾ÍÊÇËùÓÐsession¶ÔÓ¦ÊÂÎñÖÐ´óÓÚtxn_global->currentµÄ×îÐ¡ÊÂÎñmetadata_pinned¸³Öµ 
        //ÊÂÎñ½øÐÐÖÐµÄÊ±ºò¿ÉÄÜcheckpointÏß³ÌÔÚ×öcheckpoint£¬s->metadata_pinned¾ÍÊÇ¿ªÊ¼ÊÂÎñÊ±ºò×öcheckpointµÄÏß³ÌÊÂÎñid
        if ((id = s->metadata_pinned) != WT_TXN_NONE && WT_TXNID_LT(id, metadata_pinned))
            metadata_pinned = id;

        /*
         * !!!
         * Note: Don't ignore pinned ID values older than the previous
         * oldest ID.  Read-uncommitted operations publish pinned ID
         * values without acquiring the scan lock to protect the global
         * table.  See the comment in __wt_txn_cursor_op for more
         * details.
         */
        //Ò²¾ÍÊÇËùÓÐsession¶ÔÓ¦ÊÂÎñÖÐ´óÓÚtxn_global->currentµÄ×îÐ¡ÊÂÎñpinned_id¸³Öµ¸øoldest_id£¬²¢¼ÇÂ¼ÏÂÕâ¸ösession

        //¿ìÕÕ¸ôÀë¼¶±ðWT_ISO_READ_UNCOMMITTEDµÄÊ±ºò, ¾Í²»»áÍ¨¹ý__wt_txn_get_snapshotµ÷ÓÃ__wt_txn_cursor_op»ñÈ¡¿ìÕÕÁÐ±í£¬ÕâÊ±ºò¾ÍÐèÒªÍ¨¹ýs->pinned_id»ñÈ¡oldest id
        //Èç¹û²»ÊÇWT_ISO_READ_UNCOMMITTED£¬ÔòÕâÀïµÄs->pinned_idÒ»°ã¾ÍºÍÇ°ÃæµÄlast_runningÊÇÏàµÈµÄ

        //ÕâÀïÖ÷ÒªÊÇÎªWT_ISO_READ_UNCOMMITTED¸ôÀë¼¶±ð×¼±¸µÄ
        if ((id = s->pinned_id) != WT_TXN_NONE && WT_TXNID_LT(id, oldest_id)) {
            oldest_id = id;
            //Ò²¾ÍÊÇµ±Ç°ËùÓÐÊÂÎñ(²»°üÀ¨checkpointÊÂÎñ)µÄ×îÐ¡ÊÂÎñid¶ÔÓ¦µÄÊÂÎñ
            oldest_session = &conn->sessions[i];
        }
    }

/*
oldest ID: 22
durable timestamp: (0, 20)
oldest timestamp: (0, 0)
pinned timestamp: (0, 0)
stable timestamp: (0, 0)
has_durable_timestamp: yes
has_oldest_timestamp: no
has_pinned_timestamp: no
has_stable_timestamp: no
oldest_is_pinned: no
stable_is_pinned: no
checkpoint running: yes
checkpoint generation: 2
checkpoint pinned ID: 22
checkpoint txn ID: 29
session count: 22
Transaction state of active sessions:
session ID: 16, txn ID: 31, pinned ID: 22, metadata pinned ID: 29, name: connection-open-session
transaction id: 31, mod count: 3, snap min: 22, snap max: 31, snapshot count: 5, snapshot: [22, 24, 26, 28, 29], commit_timestamp: (0, 22), durable_timestamp: (0, 22), first_commit_timestamp: (0, 21), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 21), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
session ID: 18, txn ID: 33, pinned ID: 24, metadata pinned ID: 29, name: connection-open-session
transaction id: 33, mod count: 3, snap min: 24, snap max: 33, snapshot count: 5, snapshot: [24, 26, 28, 29, 31], commit_timestamp: (0, 24), durable_timestamp: (0, 24), first_commit_timestamp: (0, 23), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 23), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
session ID: 19, txn ID: 35, pinned ID: 26, metadata pinned ID: 29, name: connection-open-session
transaction id: 35, mod count: 3, snap min: 26, snap max: 35, snapshot count: 5, snapshot: [26, 28, 29, 31, 33], commit_timestamp: (0, 26), durable_timestamp: (0, 26), first_commit_timestamp: (0, 25), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 25), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
session ID: 20, txn ID: 37, pinned ID: 28, metadata pinned ID: 29, name: connection-open-session
transaction id: 37, mod count: 3, snap min: 28, snap max: 37, snapshot count: 5, snapshot: [28, 29, 31, 33, 35], commit_timestamp: (0, 28), durable_timestamp: (0, 28), first_commit_timestamp: (0, 27), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 27), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
//ÕâÀïÎªÊ²Ã´pinned ID: 31£¬ÒòÎªpinned ID²»°üÀ¨ snapshot:[]ÖÐµÄcheckpoint¶ÔÓ¦ÊÂÎñid£¬Òò´ËÐèÒªÌÞ³ý29£¬29ÊÇcheckpointÊÂÎñid
session ID: 21, txn ID: 39, pinned ID: 31, metadata pinned ID: 29, name: connection-open-session
transaction id: 39, mod count: 3, snap min: 29, snap max: 39, snapshot count: 5, snapshot: [29, 31, 33, 35, 37], commit_timestamp: (0, 30), durable_timestamp: (0, 30), first_commit_timestamp: (0, 29), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 29), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
checkpoint session ID: 22, txn ID: 29, pinned ID: 22, metadata pinned ID: 0, name: eviction-server
 */
//ÒÔÉÏÃæµÄÕâ¸öÀý×Ó1ÎªÀý:
//  µ±Ç°ËùÓÐsessionÊÂÎñtxn ID·Ö±ðÊÇ: 31  33  35  37  39,  ÕâÀï¿ÉÒÔ¿´³ölast_running = 31£¬Ò²¾ÍÊÇµ±Ç°ÕýÔÚÔËÐÐµÄËùÓÐÊÂÎñµÄ×îÐ¡ÊÂÎñid
//  metadata pinned ID·Ö±ðÊÇ: 29  29  29  29  29, Ò²¾ÍÊÇ¶ÔÓ¦ÊÂÎñsnapshot[]ÖÐµÄcheckpointÊÂÎñ,  ÕâÀï¿ÉÒÔ¿´³ömetadata_pinned = 29
//  pinned ID·Ö±ðÊÇ: 22 24 26 28 31, Ò²¾ÍÊÇ¶ÔÓ¦ÊÂÎñsnapshot[]ÖÐ³ýÁËcheckpointÊÂÎñÒÔÍâµÄÊÂÎñidµÄ×îÐ¡Öµ, ÕâÀï¿ÉÒÔ¿´³öoldest_id = 22
//  ×îÖÕ: 
//     oldest_id=22,     Ò²¾ÍÊÇµ±Ç°ËùÓÐÊÂÎñµÄsnapshot[]ÖÐµÄ¿ìÕÕÖÐ³ýÁËcheckpointÊÂÎñidÒÔÍâµÄ¿ìÕÕ×îÐ¡Öµ, ×¢Òâ__txn_oldest_scanµÄoldest idÃ»¿¼ÂÇcheckpoint¿ìÕÕid, __wt_txn_oldest_id»á¿¼ÂÇcheckpointÏß³Ì¶ÔÓ¦ÊÂÎñid
//     last_running=31,  Ò²¾ÍÊÇµ±Ç°ÕýÔÚÔËÐÐµÄÊÂÎñµÄid×îÐ¡Öµ(²»°üÀ¨checkpointÊÂÎñid)
//     metadata_pinned=22 Ò²¾ÍÊÇµ±Ç°ËùÓÐÊÂÎñµÄsnapshot[]ÖÐµÄËùÓÐÊÂÎñ(°üÀ¨checkpointÊÂÎñid)µÄid×îÐ¡Öµ


/*
Transaction state of active sessions:
session ID: 20, txn ID: 15, pinned ID: 15, metadata pinned ID: 14, name: connection-open-session
transaction id: 15, mod count: 1, snap min: 14, snap max: 15, snapshot count: 1, snapshot: [14], commit_timestamp: (0, 1000000014), durable_timestamp: (0, 1000000014), first_commit_timestamp: (0, 1000000013), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 1000000013), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000601c, isolation: WT_ISO_SNAPSHOT
session ID: 22, txn ID: 16, pinned ID: 15, metadata pinned ID: 14, name: connection-open-session
transaction id: 16, mod count: 1, snap min: 14, snap max: 15, snapshot count: 1, snapshot: [14], commit_timestamp: (0, 1000000005), durable_timestamp: (0, 1000000005), first_commit_timestamp: (0, 1000000004), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 1000000004), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000601c, isolation: WT_ISO_SNAPSHOT
session ID: 23, txn ID: 17, pinned ID: 15, metadata pinned ID: 14, name: connection-open-session
transaction id: 17, mod count: 1, snap min: 14, snap max: 17, snapshot count: 3, snapshot: [14, 15, 16], commit_timestamp: (0, 1000000008), durable_timestamp: (0, 1000000008), first_commit_timestamp: (0, 1000000007), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 1000000007), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000601c, isolation: WT_ISO_SNAPSHOT
session ID: 24, txn ID: 18, pinned ID: 15, metadata pinned ID: 14, name: connection-open-session
transaction id: 18, mod count: 1, snap min: 14, snap max: 18, snapshot count: 4, snapshot: [14, 15, 16, 17], commit_timestamp: (0, 1000000011), durable_timestamp: (0, 1000000011), first_commit_timestamp: (0, 1000000010), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 1000000010), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000601c, isolation: WT_ISO_SNAPSHOT
session ID: 25, txn ID: 19, pinned ID: 15, metadata pinned ID: 14, name: connection-open-session
transaction id: 19, mod count: 1, snap min: 14, snap max: 19, snapshot count: 5, snapshot: [14, 15, 16, 17, 18], commit_timestamp: (0, 1000000017), durable_timestamp: (0, 1000000017), first_commit_timestamp: (0, 1000000016), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 1000000016), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000601c, isolation: WT_ISO_SNAPSHOT
session ID: 26, txn ID: 21, pinned ID: 15, metadata pinned ID: 14, name: connection-open-session
transaction id: 21, mod count: 1, snap min: 14, snap max: 20, snapshot count: 6, snapshot: [14, 15, 16, 17, 18, 19], commit_timestamp: (0, 1000000020), durable_timestamp: (0, 1000000020), first_commit_timestamp: (0, 1000000019), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 1000000019), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000601c, isolation: WT_ISO_SNAPSHOT
session ID: 27, txn ID: 20, pinned ID: 15, metadata pinned ID: 14, name: connection-open-session
transaction id: 20, mod count: 1, snap min: 14, snap max: 20, snapshot count: 6, snapshot: [14, 15, 16, 17, 18, 19], commit_timestamp: (0, 1000000023), durable_timestamp: (0, 1000000023), first_commit_timestamp: (0, 1000000022), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 1000000022), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000601c, isolation: WT_ISO_SNAPSHOT

//ÒÔÉÏÃæµÄÕâ¸öÀý×Ó2ÎªÀý:
//  µ±Ç°ËùÓÐsessionÊÂÎñtxn ID·Ö±ðÊÇ: 15 16 17 18 19 21 20,  ÕâÀï¿ÉÒÔ¿´³ölast_running = 15£¬Ò²¾ÍÊÇµ±Ç°ÕýÔÚÔËÐÐµÄËùÓÐÊÂÎñµÄ×îÐ¡ÊÂÎñid
//  metadata pinned ID·Ö±ðÊÇ: 14  14  14  14  14, Ò²¾ÍÊÇ¶ÔÓ¦ÊÂÎñsnapshot[]ÖÐµÄcheckpointÊÂÎñ,  ÕâÀï¿ÉÒÔ¿´³ömetadata_pinned = 14
//  pinned ID·Ö±ðÊÇ: 15 15 15 15 15, Ò²¾ÍÊÇ¶ÔÓ¦ÊÂÎñsnapshot[]ÖÐ³ýÁËcheckpointÊÂÎñÒÔÍâµÄÊÂÎñidµÄ×îÐ¡Öµ, ÕâÀï¿ÉÒÔ¿´³öoldest_id = 15
//  ×îÖÕ: 
//     oldest_id=15,     Ò²¾ÍÊÇµ±Ç°ËùÓÐÊÂÎñµÄsnapshot[]ÖÐµÄ¿ìÕÕÖÐ³ýÁËcheckpointÊÂÎñidÒÔÍâµÄ¿ìÕÕ×îÐ¡Öµ, ×¢Òâ__txn_oldest_scanµÄoldest idÃ»¿¼ÂÇcheckpoint¿ìÕÕid, __wt_txn_oldest_id»á¿¼ÂÇcheckpointÏß³Ì¶ÔÓ¦ÊÂÎñid
//     last_running=15,  Ò²¾ÍÊÇµ±Ç°ÕýÔÚÔËÐÐµÄ³ý¿ªcheckpointÊÂÎñÒÔÍâµÄid×îÐ¡Öµ(²»°üÀ¨checkpointÊÂÎñid)
//     metadata_pinned=14 Ò²¾ÍÊÇµ±Ç°ËùÓÐÊÂÎñµÄsnapshot[]ÖÐµÄÊÂÎñ(°üÀ¨checkpointÊÂÎñid)µÄid×îÐ¡Öµ
*/
    if (WT_TXNID_LT(last_running, oldest_id))
        oldest_id = last_running;

    /* The metadata pinned ID can't move past the oldest ID. */
    //metadata_pinned³õÊ¼ÖµÎªtxn_global->checkpoint_txn_shared.id
    if (WT_TXNID_LT(oldest_id, metadata_pinned))
        metadata_pinned = oldest_id;

    //yang add todo xxxxxxxxxxxxxxxxxxxxxx   Êµ¼ÊÉÏÕâÀïÓÃckpt_sessionÒ²²»¶Ô£¬ÒòÎªÓÃ»§¿ÉÄÜ×Ô¼º×öcheckpoint£¬¶ø²»Ò»¶¨ÊÇcheckpoint server×öcheckpoint
   // if (WT_TXNID_LT(metadata_pinned, oldest_id))
    //    oldest_session = &conn->ckpt_session;


    //µ½ÕâÀïËµÃ÷oldest_idÊÇËùÓÐsessionÖÐÊÂÎñid¡¢pinned_idÖÐ×îÐ¡µÄ
    //metadata_pinnedÊÇËùÓÐsessionÖÐmetadata_pinned×îÐ¡µÄ£¬²¢ÇÒmetadata_pinned²»ÄÜ´óÓÚoldest_id

    //ËùÓÐsessionÖÐÊÂÎñidÖÐ×îÐ¡µÄ
    *last_runningp = last_running;
    //metadata_pinnedÊÇËùÓÐsessionÖÐmetadata_pinned×îÐ¡µÄ£¬²¢ÇÒmetadata_pinned²»ÄÜ´óÓÚoldest_id
    *metadata_pinnedp = metadata_pinned;
    //µ½ÕâÀïËµÃ÷oldest_idÊÇËùÓÐsessionÖÐÊÂÎñid¡¢pinned_idÖÐ×îÐ¡µÄ
    *oldest_idp = oldest_id;
    //ËùÓÐsessionÖÐpinned_id×îÐ¡µÄsession
    *oldest_sessionp = oldest_session;
}

/*

 __wt_txn_update_oldest end:
 transaction state dump
 now print session ID: 6
 current ID: 40
 //µ±Ç°ËùÓÐtxn ID:ÖÐµÄ×îÐ¡Öµ
 last running ID: 31
 //µ±Ç°ËùÓÐpinned ID:ÖÐµÄ×îÐ¡Öµ
 metadata_pinned ID: 22
 //Ò²¾ÍÊÇµ±Ç°ËùÓÐtxn ID:ºÍpinned ID:ÖÐµÄ×îÐ¡Öµ
 oldest ID: 22
 durable timestamp: (0, 20)
 oldest timestamp: (0, 0)
 pinned timestamp: (0, 0)
 stable timestamp: (0, 0)
 has_durable_timestamp: yes
 has_oldest_timestamp: no
 has_pinned_timestamp: no
 has_stable_timestamp: no
 oldest_is_pinned: no
 stable_is_pinned: no
 checkpoint running: yes
 checkpoint generation: 2
 checkpoint pinned ID: 22
 checkpoint txn ID: 29
 session count: 22
 Transaction state of active sessions:
 session ID: 16, txn ID: 31, pinned ID: 22, metadata pinned ID: 29, name: connection-open-session
 transaction id: 31, mod count: 3, snap min: 22, snap max: 31, snapshot count: 5, snapshot: [22, 24, 26, 28, 29], commit_timestamp: (0, 22), durable_timestamp: (0, 22), first_commit_timestamp: (0, 21), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 21), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
 session ID: 18, txn ID: 33, pinned ID: 24, metadata pinned ID: 29, name: connection-open-session
 transaction id: 33, mod count: 3, snap min: 24, snap max: 33, snapshot count: 5, snapshot: [24, 26, 28, 29, 31], commit_timestamp: (0, 24), durable_timestamp: (0, 24), first_commit_timestamp: (0, 23), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 23), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
 session ID: 19, txn ID: 35, pinned ID: 26, metadata pinned ID: 29, name: connection-open-session
 transaction id: 35, mod count: 3, snap min: 26, snap max: 35, snapshot count: 5, snapshot: [26, 28, 29, 31, 33], commit_timestamp: (0, 26), durable_timestamp: (0, 26), first_commit_timestamp: (0, 25), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 25), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
 session ID: 20, txn ID: 37, pinned ID: 28, metadata pinned ID: 29, name: connection-open-session
 transaction id: 37, mod count: 3, snap min: 28, snap max: 37, snapshot count: 5, snapshot: [28, 29, 31, 33, 35], commit_timestamp: (0, 28), durable_timestamp: (0, 28), first_commit_timestamp: (0, 27), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 27), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
 session ID: 21, txn ID: 39, pinned ID: 31, metadata pinned ID: 29, name: connection-open-session
 transaction id: 39, mod count: 3, snap min: 29, snap max: 39, snapshot count: 5, snapshot: [29, 31, 33, 35, 37], commit_timestamp: (0, 30), durable_timestamp: (0, 30), first_commit_timestamp: (0, 29), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 29), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
 checkpoint session ID: 22, txn ID: 29, pinned ID: 22, metadata pinned ID: 0, name: eviction-server
 transaction id: 0, mod count: 0, snap min: 0, snap max: 0, snapshot count: 0, snapshot: [], commit_timestamp: (0, 0), durable_timestamp: (0, 0), first_commit_timestamp: (0, 0), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 0), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x00000000, isolation: WT_ISO_SNAPSHOT

 * __wt_txn_update_oldest --
 *     Sweep the running transactions to update the oldest ID required.
 evict¡¢reconcile¡¢checkpointµÈ²Ù×÷»áµ÷ÓÃ__wt_txn_update_oldest
 ÀýÈçevict workerÏß³ÌÃ¿Ò»ÂÖ±éÀú»ñÈ¡ÐèÒªÌÔÌ­µÄpageÇ°»áÍ¨¹ý__evict_server->__wt_txn_update_oldestÀ´¸üÐÂÊÂÎñÏà¹Ø³ÉÔ±
 ¸Ã¸üÐÂÖ÷ÒªÓ°Ïì__wt_txn_upd_visible_all£¬Ò²¾ÍÊÇ¶ÔÊÂÎñÈ«¾Ö¿É¼ûÓ°Ïì
 */
int
__wt_txn_update_oldest(WT_SESSION_IMPL *session, uint32_t flags)
{
    WT_CONNECTION_IMPL *conn;
    WT_DECL_RET;
    WT_SESSION_IMPL *oldest_session;
    WT_TXN_GLOBAL *txn_global;
    uint64_t current_id, last_running, metadata_pinned, oldest_id;
    uint64_t prev_last_running, prev_metadata_pinned, prev_oldest_id;
    bool strict, wait;
    
    conn = S2C(session);
    txn_global = &conn->txn_global;
    strict = LF_ISSET(WT_TXN_OLDEST_STRICT);
    wait = LF_ISSET(WT_TXN_OLDEST_WAIT);

    current_id = last_running = metadata_pinned = txn_global->current;
    prev_last_running = txn_global->last_running;
    prev_metadata_pinned = txn_global->metadata_pinned;
    prev_oldest_id = txn_global->oldest_id;

   // printf("yang test...__wt_txn_update_oldest...flags:ox%32x, current:%lu, prev_last_running:%lu, prev_metadata_pinned:%lu, prev_oldest_id:%lu\r\n", 
   //     flags, txn_global->current, prev_last_running, prev_metadata_pinned, prev_oldest_id);
   // WT_RET(__wt_verbose_dump_txn(session, "__wt_txn_update_oldest begin"));//yang add change

    /* Try to move the pinned timestamp forward. */
    if (strict)
        //»ñÈ¡ËùÓÐsessionÊÂÎñÖÐµÄ×îÐ¡read_timestamp¼°oldest_timestampµÄ×îÐ¡Öµ¸³Öµ¸øtxn_global->pinned_timestamp
        __wt_txn_update_pinned_timestamp(session, false);

    /*
     * For pure read-only workloads, or if the update isn't forced and the oldest ID isn't too far
     * behind, avoid scanning.
     */
    if ((prev_oldest_id == current_id && prev_metadata_pinned == current_id) ||
      (!strict && WT_TXNID_LT(current_id, prev_oldest_id + 100)))
        return (0);

    //ÏÈ¼Ó¶ÁËø¼ÆËãÒ»´Î
    /* First do a read-only scan. */
    if (wait)
        __wt_readlock(session, &txn_global->rwlock);
    else if ((ret = __wt_try_readlock(session, &txn_global->rwlock)) != 0)
        return (ret == EBUSY ? 0 : ret);
    //¸ù¾Ýtxn_shared_list´ÓÐÂ¼ÆËã³öoldest_idµÈ
    __txn_oldest_scan(session, &oldest_id, &last_running, &metadata_pinned, &oldest_session);
    __wt_readunlock(session, &txn_global->rwlock);

    /*
     * If the state hasn't changed (or hasn't moved far enough for non-forced updates), give up.
     */
    //±È½Ïtxn_shared_listÖØÐÂ¼ÆËãµÄoldest_id last_running metadata_pinnedºÍÖØÐÂ¼ÆËãÇ°µÄ²îÒì£¬Èç¹û²îÒì²»´ó£¬Ö±½Ó·µ»Ø£¬²»×ö¸úÐÂ
    if ((oldest_id == prev_oldest_id ||
          (!strict && WT_TXNID_LT(oldest_id, prev_oldest_id + 100))) &&
      ((last_running == prev_last_running) ||
        (!strict && WT_TXNID_LT(last_running, prev_last_running + 100))) &&
      metadata_pinned == prev_metadata_pinned)
        return (0);

    //Èç¹û¶ÁËøÆÚ¼äÅÐ¶Ï³öÓÐ±ØÒª¸üÐÂoldest_id last_running metadata_pinned£¬Ôò´ÓÐÂ¼ÓÐ´ËøÔÚ¼ÆËãÒ»´Î
    /* It looks like an update is necessary, wait for exclusive access. */
    if (wait)
        __wt_writelock(session, &txn_global->rwlock);
    else if ((ret = __wt_try_writelock(session, &txn_global->rwlock)) != 0)
        return (ret == EBUSY ? 0 : ret);

    /*
     * If the oldest ID has been updated while we waited, don't bother scanning.
     Ç°Ãæ¼ÓÐ´Ëø__wt_writelockÆÚ¼äÓÐ¿ÉÄÜÆäËûÏß³ÌÒÑ¾­×öÁË¸üÐÂ
     */
    if (WT_TXNID_LE(oldest_id, txn_global->oldest_id) &&
      WT_TXNID_LE(last_running, txn_global->last_running) &&
      WT_TXNID_LE(metadata_pinned, txn_global->metadata_pinned))
        goto done;

    /*
     * Re-scan now that we have exclusive access. This is necessary because threads get transaction
     * snapshots with read locks, and we have to be sure that there isn't a thread that has got a
     * snapshot locally but not yet published its snap_min.
     */
    //¼ÓÐ´ËøÔÙ´Î´ÓÐÂ¼ÆËãoldest_id last_running metadata_pinned
    __txn_oldest_scan(session, &oldest_id, &last_running, &metadata_pinned, &oldest_session);

    /* Update the public IDs. */
    if (WT_TXNID_LT(txn_global->metadata_pinned, metadata_pinned)) {
        txn_global->metadata_pinned = metadata_pinned;
        __wt_verbose_debug2(session, WT_VERB_TRANSACTION, "update metadata_pinned %" PRIu64, metadata_pinned);
    }
    if (WT_TXNID_LT(txn_global->oldest_id, oldest_id)) {
        txn_global->oldest_id = oldest_id;
        __wt_verbose_debug2(session, WT_VERB_TRANSACTION, "update oldest_id %" PRIu64, oldest_id);
        //printf("yang test ..........__wt_txn_update_oldest....oldest_id:%lu\r\n", oldest_id);
    }
    if (WT_TXNID_LT(txn_global->last_running, last_running)) {
        txn_global->last_running = last_running;
        __wt_verbose_debug2(session, WT_VERB_TRANSACTION, "update last_running %" PRIu64, last_running);
        /* Output a verbose message about long-running transactions,
         * but only when some progress is being made. */
        if (WT_VERBOSE_ISSET(session, WT_VERB_TRANSACTION) && current_id - oldest_id > 10000 &&
          oldest_session != NULL) {
            __wt_verbose(session, WT_VERB_TRANSACTION,
              "old snapshot %" PRIu64 " pinned in session %" PRIu32 " [%s] with snap_min %" PRIu64,
              oldest_id, oldest_session->id, oldest_session->lastop, oldest_session->txn->snap_min);
        }
    }
    WT_RET(__wt_verbose_dump_txn(session, "__wt_txn_update_oldest end"));//yang add change

done:
    __wt_writeunlock(session, &txn_global->rwlock);
    return (ret);
}

/*
 * __txn_config_operation_timeout --
 *     Configure a transactions operation timeout duration.
 //Èç¹ûÅäÖÃÁËtransaction.operation_timeout_ms£¬ÔòÆô¶¯¶¨Ê±Æ÷kill³¬Ê±µÄÊÂÎñ
 */
static int
__txn_config_operation_timeout(WT_SESSION_IMPL *session, const char *cfg[], bool start_timer)
{
    WT_CONFIG_ITEM cval;
    WT_TXN *txn;

    txn = session->txn;

    if (cfg == NULL)
        return (0);

    /* Retrieve the maximum operation time, defaulting to the database-wide configuration. */
    WT_RET(__wt_config_gets_def(session, cfg, "operation_timeout_ms", 0, &cval));

    /*
     * The default configuration value is 0, we can't tell if they're setting it back to 0 or, if
     * the default was automatically passed in.
     */
    if (cval.val != 0) {
        txn->operation_timeout_us = (uint64_t)(cval.val * WT_THOUSAND);
        /*
         * The op timer will generally be started on entry to the API call however when we configure
         * it internally we need to start it separately.
         */
        if (start_timer)
            __wt_op_timer_start(session);
    }
    return (0);
}

/*
 * __wt_txn_config --
 *     Configure a transaction.
 */
int
__wt_txn_config(WT_SESSION_IMPL *session, const char *cfg[])
{
    WT_CONFIG_ITEM cval;
    WT_DECL_RET;
    WT_TXN *txn;
    wt_timestamp_t read_ts;

    txn = session->txn;

    if (cfg == NULL)
        return (0);
   // printf("yang test ................ __wt_txn_config, cfg[0]:%s, cfg[1]:%s\r\n", cfg[0], cfg[1]);

    WT_ERR(__wt_config_gets_def(session, cfg, "isolation", 0, &cval));
    if (cval.len != 0)
        txn->isolation = WT_STRING_MATCH("snapshot", cval.str, cval.len) ?
          WT_ISO_SNAPSHOT :
          WT_STRING_MATCH("read-committed", cval.str, cval.len) ? WT_ISO_READ_COMMITTED :
                                                                  WT_ISO_READ_UNCOMMITTED;

    WT_ERR(__txn_config_operation_timeout(session, cfg, false));

    /*
     * The default sync setting is inherited from the connection, but can be overridden by an
     * explicit "sync" setting for this transaction.
     *
     * We want to distinguish between inheriting implicitly and explicitly.
     */
    F_CLR(txn, WT_TXN_SYNC_SET);
    WT_ERR(__wt_config_gets_def(session, cfg, "sync", (int)UINT_MAX, &cval));
    if (cval.val == 0 || cval.val == 1)
        /*
         * This is an explicit setting of sync. Set the flag so that we know not to overwrite it in
         * commit_transaction.
         */
        F_SET(txn, WT_TXN_SYNC_SET);

    /*
     * If sync is turned off explicitly, clear the transaction's sync field.
     */
    if (cval.val == 0)
        txn->txn_logsync = 0;

    /* Check if prepared updates should be ignored during reads. */
    WT_ERR(__wt_config_gets_def(session, cfg, "ignore_prepare", 0, &cval));
    if (cval.len > 0 && WT_STRING_MATCH("force", cval.str, cval.len))
        F_SET(txn, WT_TXN_IGNORE_PREPARE);
    else if (cval.val)
        F_SET(txn, WT_TXN_IGNORE_PREPARE | WT_TXN_READONLY);

    /* Check if commits without a timestamp are allowed. */
    WT_ERR(__wt_config_gets_def(session, cfg, "no_timestamp", 0, &cval));
    if (cval.val)
        F_SET(txn, WT_TXN_TS_NOT_SET);

    /*
     * Check if the prepare timestamp and the commit timestamp of a prepared transaction need to be
     * rounded up.
     */
    WT_ERR(__wt_config_gets_def(session, cfg, "roundup_timestamps.prepared", 0, &cval));
    if (cval.val)
        F_SET(txn, WT_TXN_TS_ROUND_PREPARED);

    /* Check if read timestamp needs to be rounded up. */
    WT_ERR(__wt_config_gets_def(session, cfg, "roundup_timestamps.read", 0, &cval));
    if (cval.val)
        F_SET(txn, WT_TXN_TS_ROUND_READ);

    WT_ERR(__wt_config_gets_def(session, cfg, "read_timestamp", 0, &cval));
    if (cval.len != 0) {
        WT_ERR(__wt_txn_parse_timestamp(session, "read", &read_ts, &cval));
        WT_ERR(__wt_txn_set_read_timestamp(session, read_ts));
    }

err:
    if (ret != 0)
        /*
         * In the event that we error during configuration we should clear the flags on the
         * transaction so they are not set in a subsequent call to transaction begin.
         */
        txn->flags = 0;
    return (ret);
}

/*
 * __wt_txn_reconfigure --
 *     WT_SESSION::reconfigure for transactions.
 */
int
__wt_txn_reconfigure(WT_SESSION_IMPL *session, const char *config)
{
    WT_CONFIG_ITEM cval;
    WT_DECL_RET;
    WT_TXN *txn;

    txn = session->txn;

    ret = __wt_config_getones(session, config, "isolation", &cval);
    if (ret == 0 && cval.len != 0) {
        session->isolation = txn->isolation = WT_STRING_MATCH("snapshot", cval.str, cval.len) ?
          WT_ISO_SNAPSHOT :
          WT_STRING_MATCH("read-uncommitted", cval.str, cval.len) ? WT_ISO_READ_UNCOMMITTED :
                                                                    WT_ISO_READ_COMMITTED;
    }
    WT_RET_NOTFOUND_OK(ret);

    return (0);
}

/*
 * __wt_txn_release --
 *     Release the resources associated with the current transaction.
 */
void
__wt_txn_release(WT_SESSION_IMPL *session)
{
    WT_TXN *txn;
    WT_TXN_GLOBAL *txn_global;

    txn = session->txn;
    txn_global = &S2C(session)->txn_global;

    WT_ASSERT(session, txn->mod_count == 0);

    /* Clear the transaction's ID from the global table. */
    if (WT_SESSION_IS_CHECKPOINT(session)) {
        WT_ASSERT(session, WT_SESSION_TXN_SHARED(session)->id == WT_TXN_NONE);
        txn->id = txn_global->checkpoint_txn_shared.id = WT_TXN_NONE;

        /*
         * Be extra careful to cleanup everything for checkpoints: once the global checkpoint ID is
         * cleared, we can no longer tell if this session is doing a checkpoint.
         */
        txn_global->checkpoint_id = 0;
    } else if (F_ISSET(txn, WT_TXN_HAS_ID)) {
        /*
         * If transaction is prepared, this would have been done in prepare.
         */
        if (!F_ISSET(txn, WT_TXN_PREPARE))
            //°Ñtxn_shared_list[]Êý×éÖÐsession¶ÔÓ¦idÖÃÎ»WT_TXN_NONE, Ò²¾ÍÊÇ±¾session¶ÔÓ¦µÄ¹²ÏíÊÂÎñidÇå0
            __txn_remove_from_global_table(session);
        else
            WT_ASSERT(session, WT_SESSION_TXN_SHARED(session)->id == WT_TXN_NONE);
        //°Ñ±¾session¹ÜÀíµÄÊÂÎñidÇå0
        txn->id = WT_TXN_NONE;
    }

    __wt_txn_clear_durable_timestamp(session);

    /* Free the scratch buffer allocated for logging. */
    __wt_logrec_free(session, &txn->logrec);

    /* Discard any memory from the session's stash that we can. */
    WT_ASSERT(session, __wt_session_gen(session, WT_GEN_SPLIT) == 0);
    __wt_stash_discard(session);

    /*
     * Reset the transaction state to not running and release the snapshot.
     */
    __wt_txn_release_snapshot(session);
    /* Clear the read timestamp. */
    __wt_txn_clear_read_timestamp(session);
    txn->isolation = session->isolation;

    txn->rollback_reason = NULL;

    /*
     * Ensure the transaction flags are cleared on exit
     *
     * Purposely do NOT clear the commit and durable timestamps on release. Other readers may still
     * find these transactions in the durable queue and will need to see those timestamps.
     */
    txn->flags = 0;
    txn->prepare_timestamp = WT_TS_NONE;

    /* Clear operation timer. */
    //ÕâÀïµÄ__wt_op_timer_stop(s); ÔÚAPI_ENDÊÍ·Å
    txn->operation_timeout_us = 0;
}

/*
 * __txn_prepare_rollback_restore_hs_update --
 *     Restore the history store update to the update chain before roll back prepared update evicted
 *     to disk
 */
static int
__txn_prepare_rollback_restore_hs_update(
  WT_SESSION_IMPL *session, WT_CURSOR *hs_cursor, WT_PAGE *page, WT_UPDATE *upd_chain)
{
    WT_DECL_ITEM(hs_value);
    WT_DECL_RET;
    WT_TIME_WINDOW *hs_tw;
    WT_UPDATE *tombstone, *upd;
    wt_timestamp_t durable_ts, hs_stop_durable_ts;
    size_t size, total_size;
    uint64_t type_full;
    char ts_string[2][WT_TS_INT_STRING_SIZE];

    WT_ASSERT(session, upd_chain != NULL);

    hs_tw = NULL;
    size = total_size = 0;
    tombstone = upd = NULL;

    WT_ERR(__wt_scr_alloc(session, 0, &hs_value));

    /* Get current value. */
    WT_ERR(hs_cursor->get_value(hs_cursor, &hs_stop_durable_ts, &durable_ts, &type_full, hs_value));

    /* The value older than the prepared update in the history store must be a full value. */
    WT_ASSERT(session, (uint8_t)type_full == WT_UPDATE_STANDARD);

    /* Use time window in cell to initialize the update. */
    __wt_hs_upd_time_window(hs_cursor, &hs_tw);
    WT_ERR(__wt_upd_alloc(session, hs_value, WT_UPDATE_STANDARD, &upd, &size));
    upd->txnid = hs_tw->start_txn;
    upd->durable_ts = hs_tw->durable_start_ts;
    upd->start_ts = hs_tw->start_ts;

    /*
     * Set the flag to indicate that this update has been restored from history store for the
     * rollback of a prepared transaction.
     */
    F_SET(upd, WT_UPDATE_RESTORED_FROM_HS | WT_UPDATE_TO_DELETE_FROM_HS);
    total_size += size;

    __wt_verbose_debug2(session, WT_VERB_TRANSACTION,
      "update restored from history store (txnid: %" PRIu64 ", start_ts: %s, durable_ts: %s",
      upd->txnid, __wt_timestamp_to_string(upd->start_ts, ts_string[0]),
      __wt_timestamp_to_string(upd->durable_ts, ts_string[1]));

    /* If the history store record has a valid stop time point, append it. */
    if (hs_stop_durable_ts != WT_TS_MAX) {
        WT_ASSERT(session, hs_tw->stop_ts != WT_TS_MAX);
        WT_ERR(__wt_upd_alloc(session, NULL, WT_UPDATE_TOMBSTONE, &tombstone, &size));
        tombstone->durable_ts = hs_tw->durable_stop_ts;
        tombstone->start_ts = hs_tw->stop_ts;
        tombstone->txnid = hs_tw->stop_txn;
        tombstone->next = upd;
        /*
         * Set the flag to indicate that this update has been restored from history store for the
         * rollback of a prepared transaction.
         */
        F_SET(tombstone, WT_UPDATE_RESTORED_FROM_HS | WT_UPDATE_TO_DELETE_FROM_HS);
        total_size += size;

        __wt_verbose_debug2(session, WT_VERB_TRANSACTION,
          "tombstone restored from history store (txnid: %" PRIu64 ", start_ts: %s, durable_ts: %s",
          tombstone->txnid, __wt_timestamp_to_string(tombstone->start_ts, ts_string[0]),
          __wt_timestamp_to_string(tombstone->durable_ts, ts_string[1]));

        upd = tombstone;
    }

    /* Walk to the end of the chain and we can only have prepared updates on the update chain. */
    for (;; upd_chain = upd_chain->next) {
        WT_ASSERT(session,
          upd_chain->txnid != WT_TXN_ABORTED && upd_chain->prepare_state == WT_PREPARE_INPROGRESS);

        if (upd_chain->next == NULL)
            break;
    }

    /* Append the update to the end of the chain. */
    WT_PUBLISH(upd_chain->next, upd);

    __wt_cache_page_inmem_incr(session, page, total_size);

    if (0) {
err:
        WT_ASSERT(session, tombstone == NULL || upd == tombstone);
        __wt_free_update_list(session, &upd);
    }
    __wt_scr_free(session, &hs_value);
    return (ret);
}

/*
 * __txn_timestamp_usage_check --
 *     Check if a commit will violate timestamp rules.
 */
static inline int
__txn_timestamp_usage_check(WT_SESSION_IMPL *session, WT_TXN_OP *op, WT_UPDATE *upd)
{
    WT_BTREE *btree;
    WT_TXN *txn;
    wt_timestamp_t op_ts, prev_op_durable_ts;
    uint16_t flags;
    char ts_string[2][WT_TS_INT_STRING_SIZE];
    const char *name;
    bool no_ts_ok, txn_has_ts;

    btree = op->btree;
    txn = session->txn;
    flags = btree->dhandle->ts_flags;
    name = btree->dhandle->name;
    txn_has_ts = F_ISSET(txn, WT_TXN_HAS_TS_COMMIT | WT_TXN_HAS_TS_DURABLE);

    /* Timestamps are ignored on logged files. */
    ////mongodb ¸±±¾¼¯ÆÕÍ¨Êý¾Ý±íµÄlogÊÇ¹Ø±ÕÁËµÄ£¬oplog±íµÄlog¹¦ÄÜÊÇ´ò¿ªµÄ
    if (F_ISSET(btree, WT_BTREE_LOGGED))
        return (0);

    /*
     * Do not check for timestamp usage in recovery. We don't expect recovery to be using timestamps
     * when applying commits, and it is possible that timestamps may be out-of-order in log replay.
     */
    if (F_ISSET(S2C(session), WT_CONN_RECOVERING))
        return (0);

    op_ts = upd->start_ts != WT_TS_NONE ? upd->start_ts : txn->commit_timestamp;

    /* Check for disallowed timestamps. */
    if (LF_ISSET(WT_DHANDLE_TS_NEVER)) {
        if (!txn_has_ts)
            return (0);

        __wt_err(session, EINVAL,
          "%s: " WT_TS_VERBOSE_PREFIX "timestamp %s set when disallowed by table configuration",
          name, __wt_timestamp_to_string(op_ts, ts_string[0]));
#ifdef HAVE_DIAGNOSTIC
        __wt_abort(session);
#endif
#ifdef WT_STANDALONE_BUILD
        return (EINVAL);
#endif
    }

    prev_op_durable_ts = upd->prev_durable_ts;

    /*
     * Ordered consistency requires all updates use timestamps, once they are first used, but this
     * test can be turned off on a per-transaction basis.
     */
    no_ts_ok = F_ISSET(txn, WT_TXN_TS_NOT_SET);
    if (!txn_has_ts && prev_op_durable_ts != WT_TS_NONE && !no_ts_ok) {
        __wt_err(session, EINVAL,
          "%s: " WT_TS_VERBOSE_PREFIX
          "no timestamp provided for an update to a table configured to always use timestamps "
          "once they are first used",
          name);
#ifdef HAVE_DIAGNOSTIC
        __wt_abort(session);
#endif
#ifdef WT_STANDALONE_BUILD
        return (EINVAL);
#endif
    }

    /* Ordered consistency requires all updates be in timestamp order. */
    if (txn_has_ts && prev_op_durable_ts > op_ts) {
        __wt_err(session, EINVAL,
          "%s: " WT_TS_VERBOSE_PREFIX
          "updating a value with a timestamp %s before the previous update %s",
          name, __wt_timestamp_to_string(op_ts, ts_string[0]),
          __wt_timestamp_to_string(prev_op_durable_ts, ts_string[1]));
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
 * __txn_fixup_hs_update --
 *     Fix the history store update with the max stop time point if we commit the prepared update.
 */
static int
__txn_fixup_hs_update(WT_SESSION_IMPL *session, WT_CURSOR *hs_cursor)
{
    WT_DECL_ITEM(hs_value);
    WT_DECL_RET;
    WT_TIME_WINDOW *hs_tw, tw;
    WT_TXN *txn;
    wt_timestamp_t hs_durable_ts, hs_stop_durable_ts;
    uint64_t type_full;
    bool txn_error, txn_prepare_ignore_api_check;

    hs_tw = NULL;
    txn = session->txn;

    __wt_hs_upd_time_window(hs_cursor, &hs_tw);

    /*
     * If the history update already has a stop time point there is no work to do. This happens if a
     * deleted key is reinserted by a prepared update.
     */
    if (WT_TIME_WINDOW_HAS_STOP(hs_tw))
        return (0);

    WT_RET(__wt_scr_alloc(session, 0, &hs_value));

    /*
     * Transaction error is cleared temporarily as cursor functions are not allowed after an error
     * or a prepared transaction.
     */
    txn_error = F_ISSET(txn, WT_TXN_ERROR);
    F_CLR(txn, WT_TXN_ERROR);

    /*
     * The API layer will immediately return an error if the WT_TXN_PREPARE flag is set before
     * attempting cursor operations. However, we can't clear the WT_TXN_PREPARE flag because a
     * function in the eviction flow may attempt to forcibly rollback the transaction if it is not
     * marked as a prepared transaction. The flag WT_TXN_PREPARE_IGNORE_API_CHECK is set so that
     * cursor operations can proceed without having to clear the WT_TXN_PREPARE flag.
     */
    txn_prepare_ignore_api_check = F_ISSET(txn, WT_TXN_PREPARE_IGNORE_API_CHECK);
    F_SET(txn, WT_TXN_PREPARE_IGNORE_API_CHECK);

    /* Get current value. */
    WT_ERR(
      hs_cursor->get_value(hs_cursor, &hs_stop_durable_ts, &hs_durable_ts, &type_full, hs_value));

    /* The old stop timestamp must be max. */
    WT_ASSERT(session, hs_stop_durable_ts == WT_TS_MAX);
    /* The value older than the prepared update in the history store must be a full value. */
    WT_ASSERT(session, (uint8_t)type_full == WT_UPDATE_STANDARD);

    /*
     * Set the stop time point to be the committing transaction's time point and copy the start time
     * point from the current history store update.
     */
    tw.stop_ts = txn->commit_timestamp;
    tw.durable_stop_ts = txn->durable_timestamp;
    tw.stop_txn = txn->id;
    WT_TIME_WINDOW_COPY_START(&tw, hs_tw);

    /*
     * We need to update the stop durable timestamp stored in the history store value.
     *
     * Pack the value using cursor api.
     */
    hs_cursor->set_value(hs_cursor, &tw, tw.durable_stop_ts, tw.durable_start_ts,
      (uint64_t)WT_UPDATE_STANDARD, hs_value);
    WT_ERR(hs_cursor->update(hs_cursor));

err:
    if (!txn_prepare_ignore_api_check)
        F_CLR(txn, WT_TXN_PREPARE_IGNORE_API_CHECK);
    if (txn_error)
        F_SET(txn, WT_TXN_ERROR);
    __wt_scr_free(session, &hs_value);

    return (ret);
}

/*
 * __txn_search_prepared_op --
 *     Search for an operation's prepared update.
 »ñÈ¡op¶ÔÓ¦key(op->u.op_row.key)µÄ¿É¼ûudpÁ´±í
 */
static int
__txn_search_prepared_op(
  WT_SESSION_IMPL *session, WT_TXN_OP *op, WT_CURSOR **cursorp, WT_UPDATE **updp)
{
    WT_CURSOR *cursor;
    WT_DECL_RET;
    WT_TXN *txn;
    uint32_t txn_flags;
    const char *open_cursor_cfg[] = {WT_CONFIG_BASE(session, WT_SESSION_open_cursor), NULL};

    *updp = NULL;

    txn = session->txn;

    //´´½¨Ò»¸öbtreeµÄcursor
    cursor = *cursorp;
    if (cursor == NULL || CUR2BT(cursor)->id != op->btree->id) {
        *cursorp = NULL;
        if (cursor != NULL)
            WT_RET(cursor->close(cursor));
        WT_RET(__wt_open_cursor(session, op->btree->dhandle->name, NULL, open_cursor_cfg, &cursor));
        *cursorp = cursor;
    }

    /*
     * Transaction error is cleared temporarily as cursor functions are not allowed after an error.
     */
    txn_flags = FLD_MASK(txn->flags, WT_TXN_ERROR);

    /*
     * The API layer will immediately return an error if the WT_TXN_PREPARE flag is set before
     * attempting cursor operations. However, we can't clear the WT_TXN_PREPARE flag because a
     * function in the eviction flow may attempt to forcibly rollback the transaction if it is not
     * marked as a prepared transaction. The flag WT_TXN_PREPARE_IGNORE_API_CHECK is set so that
     * cursor operations can proceed without having to clear the WT_TXN_PREPARE flag.
     */
    F_SET(txn, WT_TXN_PREPARE_IGNORE_API_CHECK);

    switch (op->type) {
    case WT_TXN_OP_BASIC_COL:
    case WT_TXN_OP_INMEM_COL:
        ((WT_CURSOR_BTREE *)cursor)->iface.recno = op->u.op_col.recno;
        break;
    case WT_TXN_OP_BASIC_ROW:
    case WT_TXN_OP_INMEM_ROW:
        F_CLR(txn, txn_flags);
        //ÉèÖÃcursorµÄkeyÎªop->u.op_row.key
        __wt_cursor_set_raw_key(cursor, &op->u.op_row.key);
        F_SET(txn, txn_flags);
        break;
    case WT_TXN_OP_NONE:
    case WT_TXN_OP_REF_DELETE:
    case WT_TXN_OP_TRUNCATE_COL:
    case WT_TXN_OP_TRUNCATE_ROW:
        WT_RET_PANIC_ASSERT(session, false, WT_PANIC, "invalid prepared operation update type");
        break;
    }

    F_CLR(txn, txn_flags);
    //»ñÈ¡op->u.op_row.keyÕâ¸ökeyµÄv updp
    WT_WITH_BTREE(session, op->btree, ret = __wt_btcur_search_prepared(cursor, updp));
    F_SET(txn, txn_flags);
    F_CLR(txn, WT_TXN_PREPARE_IGNORE_API_CHECK);
    WT_RET(ret);
    WT_RET_ASSERT(session, *updp != NULL, WT_NOTFOUND,
      "unable to locate update associated with a prepared operation");

    return (0);
}

/*
 * __txn_append_tombstone --
 *     Append a tombstone to the end of a keys update chain.
 */
static int
__txn_append_tombstone(WT_SESSION_IMPL *session, WT_TXN_OP *op, WT_CURSOR_BTREE *cbt)
{
    WT_BTREE *btree;
    WT_DECL_RET;
    WT_UPDATE *tombstone;
    size_t not_used;
    tombstone = NULL;
    btree = S2BT(session);

    WT_ERR(__wt_upd_alloc_tombstone(session, &tombstone, &not_used));
#ifdef HAVE_DIAGNOSTIC
    WT_WITH_BTREE(session, op->btree,
      ret = btree->type == BTREE_ROW ?
        __wt_row_modify(cbt, &cbt->iface.key, NULL, tombstone, WT_UPDATE_INVALID, false, false) :
        __wt_col_modify(cbt, cbt->recno, NULL, tombstone, WT_UPDATE_INVALID, false, false));
#else
    WT_WITH_BTREE(session, op->btree,
      ret = btree->type == BTREE_ROW ?
        __wt_row_modify(cbt, &cbt->iface.key, NULL, tombstone, WT_UPDATE_INVALID, false) :
        __wt_col_modify(cbt, cbt->recno, NULL, tombstone, WT_UPDATE_INVALID, false));
#endif
    WT_ERR(ret);
    tombstone = NULL;

err:
    __wt_free(session, tombstone);
    return (ret);
}

/*
 * __txn_resolve_prepared_update_chain --
 *     Helper for resolving updates. Recursively visit the update chain and resolve the updates on
 *     the way back out, so older updates are resolved first; this avoids a race with reconciliation
 *     (see WT-6778).
 °ÑudpÁ´±íÉÏÃæÊôÓÚ±¾sessionÊÂÎñÕýÔÚ²Ù×÷µÄudp½ÚµãµÄ×´Ì¬ÖÃÎªWT_PREPARE_RESOLVED
 */
static void
__txn_resolve_prepared_update_chain(WT_SESSION_IMPL *session, WT_UPDATE *upd, bool commit)
{

    /* If we've reached the end of the chain, we're done looking. */
    if (upd == NULL)
        return;

    /*
     * Aborted updates can exist in the update chain of our transaction. Generally this will occur
     * due to a reserved update. As such we should skip over these updates entirely.
     */
    //Ìø¹ýabortµÄudp½Úµã
    if (upd->txnid == WT_TXN_ABORTED) {
        //µÝ¹é
        __txn_resolve_prepared_update_chain(session, upd->next, commit);
        return;
    }

    /*
     * If the transaction id is then different and not aborted we know we've reached the end of our
     * update chain and don't need to look deeper.
     */
    //Ö»´¦Àí±¾session¶ÔÓ¦ÊÂÎñµÄudp
    if (upd->txnid != session->txn->id)
        return;

    /* Go down the chain. Do the resolves on the way back up. */
    //µÝ¹é
    __txn_resolve_prepared_update_chain(session, upd->next, commit);

    if (!commit) {
        //ËµÃ÷ÊÇÍ¨¹ý__wt_txn_rollbackµ½ÁËÕâÀï
        upd->txnid = WT_TXN_ABORTED;
        WT_STAT_CONN_INCR(session, txn_prepared_updates_rolledback);
        return;
    }

    //ËµÃ÷ÊÇ__wt_txn_commitµ÷ÓÃµ½ÁËÕâÀï
    /*
     * Performing an update on the same key where the truncate operation is performed can lead to
     * updates that are already resolved in the updated list. Ignore the already resolved updates.
     */
    if (upd->prepare_state == WT_PREPARE_RESOLVED) {
        WT_ASSERT(session, upd->type == WT_UPDATE_TOMBSTONE);
        return;
    }

    /* Resolve the prepared update to be a committed update. */
    __txn_resolve_prepared_update(session, upd);
    WT_STAT_CONN_INCR(session, txn_prepared_updates_committed);
}

/*
 * __txn_resolve_prepared_op --
 *     Resolve a transaction's operations indirect references.
 // °ÑudpÁ´±íÉÏÃæÊôÓÚ±¾sessionÊÂÎñÕýÔÚ²Ù×÷µÄudp½ÚµãµÄ×´Ì¬ÖÃÎªWT_PREPARE_RESOLVED
 */
static int
__txn_resolve_prepared_op(WT_SESSION_IMPL *session, WT_TXN_OP *op, bool commit, WT_CURSOR **cursorp)
{
    WT_BTREE *btree;
    WT_CURSOR *hs_cursor;
    WT_CURSOR_BTREE *cbt;
    WT_DECL_RET;
    WT_ITEM hs_recno_key;
    WT_PAGE *page;
    WT_TIME_WINDOW tw;
    WT_TXN *txn;
    WT_UPDATE *first_committed_upd, *upd, *upd_followed_tombstone;
#ifdef HAVE_DIAGNOSTIC
    WT_UPDATE *head_upd;
#endif
    uint8_t *p, resolve_case, hs_recno_key_buf[WT_INTPACK64_MAXSIZE];
    char ts_string[3][WT_TS_INT_STRING_SIZE];
    bool tw_found, has_hs_record;

    hs_cursor = NULL;
    txn = session->txn;
    has_hs_record = false;
#define RESOLVE_UPDATE_CHAIN 0
#define RESOLVE_PREPARE_ON_DISK 1
#define RESOLVE_PREPARE_EVICTION_FAILURE 2
#define RESOLVE_IN_MEMORY 3
    resolve_case = RESOLVE_UPDATE_CHAIN;

    // »ñÈ¡op¶ÔÓ¦key(op->u.op_row.key)µÄ¿É¼ûudpÁ´±í
    WT_RET(__txn_search_prepared_op(session, op, cursorp, &upd));

    if (commit)
        //yang add todo xxxxxxxxxxx  ÕâÁ½¸ö´òÓ¡ÍêÉÆÈÕÖ¾Ôö¼Óprepare timestamp
        __wt_verbose_debug2(session, WT_VERB_TRANSACTION,
          "commit resolving prepared transaction with txnid: %" PRIu64
          " and prepare timestamp: %s to commit and durable timestamps: %s, %s",
          txn->id, __wt_timestamp_to_string(txn->prepare_timestamp, ts_string[0]),
          __wt_timestamp_to_string(txn->commit_timestamp, ts_string[1]),
          __wt_timestamp_to_string(txn->durable_timestamp, ts_string[2]));
    else
        __wt_verbose_debug2(session, WT_VERB_TRANSACTION,
          "rollback resolving prepared transaction with txnid: %" PRIu64 " and prepare timestamp: %s",
          txn->id, __wt_timestamp_to_string(txn->prepare_timestamp, ts_string[0]));

    /*
     * Aborted updates can exist in the update chain of our transaction. Generally this will occur
     * due to a reserved update. As such we should skip over these updates.
     */
    for (; upd != NULL && upd->txnid == WT_TXN_ABORTED; upd = upd->next)
        ;
#ifdef HAVE_DIAGNOSTIC
    head_upd = upd;
#endif

    /*
     * The head of the update chain is not a prepared update, which means all the prepared updates
     * of the key are resolved. The head of the update chain can also be null in the scenario that
     * we rolled back all associated updates in the previous iteration of this function.
     */
    if (upd == NULL || upd->prepare_state != WT_PREPARE_INPROGRESS)
        goto prepare_verify;

    /* A prepared operation that is rolled back will not have a timestamp worth asserting on. */
    if (commit)
        WT_RET(__txn_timestamp_usage_check(session, op, upd));

    //first_committed_updÖ¸ÏòµÚÒ»¸ö·ÇWT_TXN_ABORTED×´Ì¬²¢ÇÒWT_PREPARE_INPROGRESSµÄudp
    for (first_committed_upd = upd; first_committed_upd != NULL &&
         (first_committed_upd->txnid == WT_TXN_ABORTED ||
           first_committed_upd->prepare_state == WT_PREPARE_INPROGRESS);
         first_committed_upd = first_committed_upd->next)
        ;

    /*
     * Get the underlying btree and the in-memory page with the prepared updates that are to be
     * resolved. The hazard pointer on the page is already acquired during the cursor search
     * operation to prevent eviction evicting the page while resolving the prepared updates.
     */
    //Ò²¾ÍÊÇkey(op->u.op_row.key)¶ÔÓ¦µÄpage
    cbt = (WT_CURSOR_BTREE *)(*cursorp);
    page = cbt->ref->page;

    /*
     * If the prepared update is a single tombstone, we don't need to do anything special and we can
     * directly resolve it in memory.
     *
     * If the prepared update is not a tombstone or we have multiple prepared updates in the same
     * transaction. There are four base cases:
     *
     * 1) Prepared updates are on the update chain and hasn't been reconciled to write to data
     *    store.
     *     Simply resolve the prepared updates in memory.
     *
     * 2) Prepared updates are written to the data store.
     *     If there is no older updates written to the history store:
     *         commit: simply resolve the prepared updates in memory.
     *         rollback: delete the whole key.
     *
     *     If there are older updates written to the history store:
     *         commit: fix the stop timestamp of the newest update in the history store if it has a
     *                 max timestamp.
     *         rollback: restore the newest update in the history store to the data store and mark
     *                   it to be deleted from the history store in the future reconciliation.
     *
     * 3) Prepared updates are successfully reconciled to a new disk image in eviction but the
     *    eviction fails and the updates are restored back to the old disk image.
     *     If there is no older updates written to the history store:
     *         commit: simply resolve the prepared updates in memory.
     *         rollback: delete the whole key.
     *
     *     If there are older updates written to the history store:
     *          commit: fix the stop timestamp of the newest update in the history store if it has a
     *                  max timestamp.
     *          rollback: mark the data update (or tombstone and data update) that is older
     *                    than the prepared updates to be deleted from the history store in the
     *                    future reconciliation.
     *
     * 4) We are running an in-memory database:
     *     commit: resolve the prepared updates in memory.
     *     rollback: if the prepared update is written to the disk image, delete the whole key.
     */

    /*
     * We also need to handle the on disk prepared updates if we have a prepared delete and a
     * prepared update on the disk image.
     */
    if (F_ISSET(upd, WT_UPDATE_PREPARE_RESTORED_FROM_DS) &&
      (upd->type != WT_UPDATE_TOMBSTONE ||
        (upd->next != NULL && upd->durable_ts == upd->next->durable_ts &&
          upd->txnid == upd->next->txnid && upd->start_ts == upd->next->start_ts)))
        resolve_case = RESOLVE_PREPARE_ON_DISK;
    /*
     * If the first committed update older than the prepared update has already been marked to be
     * deleted from the history store, we are in the case that there was an older prepared update
     * that was rolled back.
     *
     * 1) We have a prepared update Up and an update U on the update chain initially.
     * 2) An eviction writes Up to the disk and U to the history store.
     * 3) The eviction fails and everything is restored.
     * 4) We rollback Up and mark U to be deleted from the history store.
     * 5) We add another prepared update to the update chain.
     *
     * Check the WT_UPDATE_TO_DELETE_FROM_HS to see if we have already handled the older prepared
     * update or not. Ignore if it is already handled.
     */
    else if (first_committed_upd != NULL && F_ISSET(first_committed_upd, WT_UPDATE_HS) &&
      !F_ISSET(first_committed_upd, WT_UPDATE_TO_DELETE_FROM_HS))
        resolve_case = RESOLVE_PREPARE_EVICTION_FAILURE;
    else if (F_ISSET(S2C(session), WT_CONN_IN_MEMORY))
        resolve_case = RESOLVE_IN_MEMORY;
    else
        //Ò»°ã×ßÕâÀï
        resolve_case = RESOLVE_UPDATE_CHAIN;

    switch (resolve_case) {
    case RESOLVE_PREPARE_EVICTION_FAILURE:
        /*
         * If we see the first committed update has been moved to the history store, we must have
         * done a successful reconciliation on the page but failed to evict it. Also reconciliation
         * could not possibly empty the page because the prepared update is not globally visible.
         * Therefore, reconciliation must have either split the page or done a page rewrite.
         *
         * In this case, we still need to resolve the prepared update as if we have successfully
         * evicted the page because the value older than the prepared update has been written to the
         * history store with the max timestamp.
         */
        WT_ASSERT(session,
          page->modify->rec_result == WT_PM_REC_MULTIBLOCK ||
            page->modify->rec_result == WT_PM_REC_REPLACE);
        /*
         * Marked the update older than the prepared update that is already in the history store to
         * be deleted from the history store.
         */
        if (!commit) {
            if (first_committed_upd->type == WT_UPDATE_TOMBSTONE) {
                for (upd_followed_tombstone = first_committed_upd->next;
                     upd_followed_tombstone != NULL;
                     upd_followed_tombstone = upd_followed_tombstone->next)
                    if (upd_followed_tombstone->txnid != WT_TXN_ABORTED)
                        break;
                /* We may not find a full update following the tombstone if it is obsolete. */
                if (upd_followed_tombstone != NULL) {
                    WT_ASSERT(session, F_ISSET(upd_followed_tombstone, WT_UPDATE_HS));
                    F_SET(first_committed_upd, WT_UPDATE_TO_DELETE_FROM_HS);
                    F_SET(upd_followed_tombstone, WT_UPDATE_TO_DELETE_FROM_HS);
                }
            } else
                F_SET(first_committed_upd, WT_UPDATE_TO_DELETE_FROM_HS);
        }
        /* Fall through. */
    case RESOLVE_PREPARE_ON_DISK:
        btree = S2BT(session);

        /*
         * Open a history store table cursor and scan the history store for the given btree and key
         * with maximum start timestamp to let the search point to the last version of the key.
         */
        WT_ERR(__wt_curhs_open(session, NULL, &hs_cursor));
        F_SET(hs_cursor, WT_CURSTD_HS_READ_COMMITTED);
        if (btree->type == BTREE_ROW)
            hs_cursor->set_key(hs_cursor, 4, btree->id, &cbt->iface.key, WT_TS_MAX, UINT64_MAX);
        else {
            p = hs_recno_key_buf;
            WT_ERR(__wt_vpack_uint(&p, 0, cbt->recno));
            hs_recno_key.data = hs_recno_key_buf;
            hs_recno_key.size = WT_PTRDIFF(p, hs_recno_key_buf);
            hs_cursor->set_key(hs_cursor, 4, btree->id, &hs_recno_key, WT_TS_MAX, UINT64_MAX);
        }
        /*
         * Locate the previous update from the history store. We know there may be content in the
         * history store if the prepared update is written to the disk image or first committed
         * update older than the prepared update is marked as WT_UPDATE_HS. The second case is rare
         * but can happen if the previous eviction that writes the prepared update to the disk image
         * fails after reconciliation.
         *
         * We need to locate the history store update before we resolve the prepared updates because
         * if we abort the prepared updates first, the history store search may race with other
         * sessions modifying the same key and checkpoint moving the new updates to the history
         * store.
         */
        WT_ERR_NOTFOUND_OK(__wt_curhs_search_near_before(session, hs_cursor), true);

        /* We should only get not found if the prepared update is on disk. */
        WT_ASSERT(session, ret != WT_NOTFOUND || resolve_case == RESOLVE_PREPARE_ON_DISK);
        if (ret == 0) {
            has_hs_record = true;
            /*
             * Restore the history store update to the update chain if we are rolling back the
             * prepared update written to the disk image.
             */
            if (!commit && resolve_case == RESOLVE_PREPARE_ON_DISK)
                WT_ERR(__txn_prepare_rollback_restore_hs_update(session, hs_cursor, page, upd));
        } else {
            ret = 0;
            /*
             * Allocate a tombstone and prepend it to the row so when we reconcile the update chain
             * we don't copy the prepared cell, which is now associated with a rolled back prepare,
             * and instead write nothing.
             */
            if (!commit)
                WT_ERR(__txn_append_tombstone(session, op, cbt));
        }
        break;
    case RESOLVE_IN_MEMORY:
        /*
         * For in-memory configurations of WiredTiger if a prepared update is reconciled and then
         * rolled back the on-page value will not be marked as aborted until the next eviction. In
         * the special case where this rollback results in the update chain being entirely comprised
         * of aborted updates other transactions attempting to write to the same key will look at
         * the on-page value, think the prepared transaction is still active, and falsely report a
         * write conflict. To prevent this scenario append a tombstone to the update chain when
         * rolling back a prepared reconciled update would result in only aborted updates on the
         * update chain.
         */
        if (!commit && first_committed_upd == NULL) {
            tw_found = __wt_read_cell_time_window(cbt, &tw);
            if (tw_found && tw.prepare == WT_PREPARE_INPROGRESS)
                WT_ERR(__txn_append_tombstone(session, op, cbt));
        }
        break;
    default:
        WT_ASSERT(session, resolve_case == RESOLVE_UPDATE_CHAIN);
        break;
    }

    /*
     * Newer updates are inserted at head of update chain, and transaction operations are added at
     * the tail of the transaction modify chain.
     *
     * For example, a transaction has modified [k,v] as
     *	[k, v]  -> [k, u1]   (txn_op : txn_op1)
     *	[k, u1] -> [k, u2]   (txn_op : txn_op2)
     *	update chain : u2->u1
     *	txn_mod      : txn_op1->txn_op2.
     *
     * Only the key is saved in the transaction operation structure, hence we cannot identify
     * whether "txn_op1" corresponds to "u2" or "u1" during commit/rollback.
     *
     * To make things simpler we will handle all the updates that match the key saved in a
     * transaction operation in a single go. As a result, multiple updates of a key, if any will be
     * resolved as part of the first transaction operation resolution of that key, and subsequent
     * transaction operation resolution of the same key will be effectively a no-op.
     *
     * In the above example, we will resolve "u2" and "u1" as part of resolving "txn_op1" and will
     * not do any significant thing as part of "txn_op2".
     */
    // °ÑudpÁ´±íÉÏÃæÊôÓÚ±¾sessionÊÂÎñÕýÔÚ²Ù×÷µÄudp½ÚµãµÄ×´Ì¬ÖÃÎªWT_PREPARE_RESOLVED
    __txn_resolve_prepared_update_chain(session, upd, commit);

    /* Mark the page dirty once the prepared updates are resolved. */
    __wt_page_modify_set(session, page);

    /*
     * Fix the history store record's stop time point if we are committing the prepared update and
     * the previous update is written to the history store.
     */
    if (commit && has_hs_record)
        WT_ERR(__txn_fixup_hs_update(session, hs_cursor));

prepare_verify:
#ifdef HAVE_DIAGNOSTIC
    for (; head_upd != NULL; head_upd = head_upd->next) {
        /*
         * Assert if we still have an update from the current transaction that hasn't been resolved
         * or aborted.
         */
        WT_ASSERT(session,
          head_upd->txnid == WT_TXN_ABORTED || head_upd->prepare_state == WT_PREPARE_RESOLVED ||
            head_upd->txnid != txn->id);

        if (head_upd->txnid == WT_TXN_ABORTED)
            continue;

        /*
         * If we restored an update from the history store, it should be the last update on the
         * chain.
         */
        if (!commit && resolve_case == RESOLVE_PREPARE_ON_DISK &&
          head_upd->type == WT_UPDATE_STANDARD && F_ISSET(head_upd, WT_UPDATE_RESTORED_FROM_HS))
            WT_ASSERT(session, head_upd->next == NULL);
    }
#endif

err:
    if (hs_cursor != NULL)
        WT_TRET(hs_cursor->close(hs_cursor));
    return (ret);
}

/*
 * __txn_mod_compare --
 *     Qsort comparison routine for transaction modify list.
 */
static int WT_CDECL
__txn_mod_compare(const void *a, const void *b)
{
    WT_TXN_OP *aopt, *bopt;

    aopt = (WT_TXN_OP *)a;
    bopt = (WT_TXN_OP *)b;

    /* If the files are different, order by ID. */
    if (aopt->btree->id != bopt->btree->id)
        return (aopt->btree->id < bopt->btree->id);

    /*
     * If the files are the same, order by the key. Row-store collators require WT_SESSION pointers,
     * and we don't have one. Compare the keys if there's no collator, otherwise return equality.
     * Column-store is always easy.
     */
    if (aopt->type == WT_TXN_OP_BASIC_ROW || aopt->type == WT_TXN_OP_INMEM_ROW)
        return (aopt->btree->collator == NULL ?
            __wt_lex_compare(&aopt->u.op_row.key, &bopt->u.op_row.key, false) :
            0);
    return (aopt->u.op_col.recno < bopt->u.op_col.recno);
}

/*
 * __wt_txn_commit --
 *     Commit the current transaction.
  //Êµ¼ÊÉÏÆÕÍ¨µÄÐ´²Ù×÷Èç¹û²»ÏÔÊ¾Ö¸¶¨transaction_begin  transaction_commitµÈ²Ù×÷£¬Ò²»áµ±ÆÕÍ¨ÊÂÎñÁ÷³Ì´¦Àí
  // 1. __wt_cursor_func_init->__wt_txn_cursor_op->__wt_txn_get_snapshot»ñÈ¡¿ìÕÕ
  // 2. TXN_API_ENDÖÐµ÷ÓÃ__wt_txn_commitÌá½»ÊÂÎñ
 */
int
__wt_txn_commit(WT_SESSION_IMPL *session, const char *cfg[])
{
    WT_CONFIG_ITEM cval;
    WT_CONNECTION_IMPL *conn;
    WT_CURSOR *cursor;
    WT_DECL_RET;
    WT_TXN *txn;
    WT_TXN_GLOBAL *txn_global;
    WT_TXN_OP *op;
    WT_UPDATE *upd;
    wt_timestamp_t candidate_durable_timestamp, prev_durable_timestamp;
#ifdef HAVE_DIAGNOSTIC
    uint32_t prepare_count;
#endif
    uint8_t previous_state;
    u_int i;
    bool cannot_fail, locked, prepare, readonly, update_durable_ts;

    //printf("yang test .....__wt_txn_commit..........session id:%u\r\n", session->id);
    conn = S2C(session);
    cursor = NULL;
    txn = session->txn;
    txn_global = &conn->txn_global;
#ifdef HAVE_DIAGNOSTIC
    prepare_count = 0;
#endif
    //ËµÃ÷ÓÐµ÷ÓÃ->prepare_transaction()½Ó¿Ú
    prepare = F_ISSET(txn, WT_TXN_PREPARE);
    readonly = txn->mod_count == 0;
    cannot_fail = locked = false;

    /* Permit the commit if the transaction failed, but was read-only. */
    WT_ASSERT(session, F_ISSET(txn, WT_TXN_RUNNING));
    WT_ASSERT(session, !F_ISSET(txn, WT_TXN_ERROR) || txn->mod_count == 0);

    /* Configure the timeout for this commit operation. */
    //Èç¹ûÅäÖÃÁËtransaction.operation_timeout_ms£¬ÔòÆô¶¯¶¨Ê±Æ÷kill³¬Ê±µÄÊÂÎñ
    WT_ERR(__txn_config_operation_timeout(session, cfg, true));

    /*
     * Clear the prepared round up flag if the transaction is not prepared. There is no rounding up
     * to do in that case.
     */
    if (!prepare)
        F_CLR(txn, WT_TXN_TS_ROUND_PREPARED);

    /* Set the commit and the durable timestamps. */
    WT_ERR(__wt_txn_set_timestamp(session, cfg, true));

    if (prepare) {
        if (!F_ISSET(txn, WT_TXN_HAS_TS_COMMIT))
            WT_ERR_MSG(session, EINVAL, "commit_timestamp is required for a prepared transaction");

        if (!F_ISSET(txn, WT_TXN_HAS_TS_DURABLE))
            WT_ERR_MSG(session, EINVAL, "durable_timestamp is required for a prepared transaction");

        WT_ASSERT(session, txn->prepare_timestamp <= txn->commit_timestamp);
    } else {
        if (F_ISSET(txn, WT_TXN_HAS_TS_PREPARE))
            WT_ERR_MSG(session, EINVAL, "prepare timestamp is set for non-prepared transaction");

        if (F_ISSET(txn, WT_TXN_HAS_TS_DURABLE))
            WT_ERR_MSG(session, EINVAL,
              "durable_timestamp should not be specified for non-prepared transaction");
    }

    /*
     * Release our snapshot in case it is keeping data pinned (this is particularly important for
     * checkpoints). Before releasing our snapshot, copy values into any positioned cursors so they
     * don't point to updates that could be freed once we don't have a snapshot. If this transaction
     * is prepared, then copying values would have been done during prepare.
     */
    if (session->ncursors > 0 && !prepare) {
        WT_DIAGNOSTIC_YIELD;
        WT_ERR(__wt_session_copy_values(session));
    }
    __wt_txn_release_snapshot(session);

    /*
     * Resolving prepared updates is expensive. Sort prepared modifications so all updates for each
     * page within each file are done at the same time.
     */
    if (prepare)
        __wt_qsort(txn->mod, txn->mod_count, sizeof(WT_TXN_OP), __txn_mod_compare);

    /* If we are logging, write a commit log record. */
    /* ÀýÈçÈýÌõ²»Í¬±íÊý¾ÝÒ»¸öÊÂÎñÐ´Èë¶ÔÓ¦ÊÂÎñÈÕÖ¾ÈçÏÂ:
{ "lsn" : [1,10112],
  "hdr_flags" : "",
  "rec_len" : 256,
  "mem_len" : 256,
  "type" : "commit",
  "txnid" : 14,
  "ops": [
    { "optype": "row_put",
      "fileid": 2147483650 0x80000002,
      "key": "yang test timestamp_abort 0000000000\u0000",
      "value": "COLL: th"
    },
    { "optype": "row_put",
      "fileid": 2147483651 0x80000003,
      "key": "yang test timestamp_abort 0000000000\u0000",
      "value": "COLL: th"
    },
    { "optype": "row_put",
      "fileid": 5 0x5,
      "key": "yang test timestamp_abort 0000000000\u0000",
      "value": "OP"
    }
  ]
},*/
    //log=(enabled),Ä¬ÈÏÒ»°ãenabled£¬Òò´ËÊÂÎñ»áÐ´ÈÕÖ¾£¬Ò²¾ÍÊÇÕâÀï¿ÉÒÔÍê³ÉÈÕÖ¾³Ö¾Ã»¯£¬±£Ö¤È«²¿³É¹¦»òÕßÈ«²¿Ê§°Ü
    if (txn->logrec != NULL) {//Ò»¸öÊÂÎñÉú³ÉÒ»ÌõWALÈÕÖ¾
        /* Assert environment and tree are logging compatible, the fast-check is short-hand. */
        WT_ASSERT(session,
          !F_ISSET(conn, WT_CONN_RECOVERING) && FLD_ISSET(conn->log_flags, WT_CONN_LOG_ENABLED));

        /*
         * The default sync setting is inherited from the connection, but can be overridden by an
         * explicit "sync" setting for this transaction.
         */
        WT_ERR(__wt_config_gets_def(session, cfg, "sync", 0, &cval));

        /*
         * If the user chose the default setting, check whether sync is enabled for this transaction
         * (either inherited or via begin_transaction). If sync is disabled, clear the field to
         * avoid the log write being flushed.
         *
         * Otherwise check for specific settings. We don't need to check for "on" because that is
         * the default inherited from the connection. If the user set anything in begin_transaction,
         * we only override with an explicit setting.
         */
        if (cval.len == 0) {
            if (!FLD_ISSET(txn->txn_logsync, WT_LOG_SYNC_ENABLED) && !F_ISSET(txn, WT_TXN_SYNC_SET))
                txn->txn_logsync = 0;
        } else {
            /*
             * If the caller already set sync on begin_transaction then they should not be using
             * sync on commit_transaction. Flag that as an error.
             */
            if (F_ISSET(txn, WT_TXN_SYNC_SET))
                WT_ERR_MSG(session, EINVAL, "sync already set during begin_transaction");
            if (WT_STRING_MATCH("off", cval.str, cval.len))
                txn->txn_logsync = 0;
            /*
             * We don't need to check for "on" here because that is the default to inherit from the
             * connection setting.
             */
        }

        /*
         * We hold the visibility lock for reading from the time we write our log record until the
         * time we release our transaction so that the LSN any checkpoint gets will always reflect
         * visible data.
         */
        __wt_readlock(session, &txn_global->visibility_rwlock);
        locked = true;
        WT_ERR(__wt_txn_log_commit(session, cfg));
    }
    
    /* Process updates. */
    for (i = 0, op = txn->mod; i < txn->mod_count; i++, op++) {
        //printf("yang test .....__wt_txn_commit......1....op->type:%u\r\n", op->type);
        switch (op->type) {
        case WT_TXN_OP_NONE:
            break;
        case WT_TXN_OP_BASIC_COL:
        case WT_TXN_OP_BASIC_ROW:
        case WT_TXN_OP_INMEM_COL:
        case WT_TXN_OP_INMEM_ROW:
            if (!prepare) {//·ÇprepareµÄÊÂÎñ×ßÕâÀï
                upd = op->u.op_upd;

                /*
                 * Switch reserved operations to abort to simplify obsolete update list truncation.
                 */
                //Ö»ÓÐWT_CURSOR->reserve()½Ó¿Ú²Å»áÂú×ã¸ÃÌõ¼þ£¬Ò»°ã²»»á½øÀ´£¬¿ÉÒÔÏÈºöÂÔ
                if (upd->type == WT_UPDATE_RESERVE) {
                    upd->txnid = WT_TXN_ABORTED;
                    break;
                }
                
             //printf("yang test .....__wt_txn_commit..2........op->type:%u\r\n", op->type);
                /*
                 * Don't reset the timestamp of the history store records with history store
                 * transaction timestamp. Those records should already have the original time window
                 * when they are inserted into the history store.
                 */
                if (conn->cache->hs_fileid != 0 && op->btree->id == conn->cache->hs_fileid)
                    break;

                __wt_txn_op_set_timestamp(session, op);
                WT_ERR(__txn_timestamp_usage_check(session, op, upd));
            } else {//prepareµÄÊÂÎñ×ßÕâÀï
                /*
                 * If an operation has the key repeated flag set, skip resolving prepared updates as
                 * the work will happen on a different modification in this txn.
                 */
                if (!F_ISSET(op, WT_TXN_OP_KEY_REPEATED))
                    // °ÑudpÁ´±íÉÏÃæÊôÓÚ±¾sessionÊÂÎñÕýÔÚ²Ù×÷µÄudp½ÚµãµÄ×´Ì¬ÖÃÎªWT_PREPARE_RESOLVED
                    WT_ERR(__txn_resolve_prepared_op(session, op, true, &cursor));
#ifdef HAVE_DIAGNOSTIC
                ++prepare_count;
#endif
            }
            break;
        case WT_TXN_OP_REF_DELETE:
            __wt_txn_op_set_timestamp(session, op);
            break;
        case WT_TXN_OP_TRUNCATE_COL:
        case WT_TXN_OP_TRUNCATE_ROW:
            /* Other operations don't need timestamps. */
            break;
        }

        /* If we used the cursor to resolve prepared updates, the key now has been freed. */
        if (cursor != NULL)
            WT_CLEAR(cursor->key);
    }

    if (cursor != NULL) {
        WT_ERR(cursor->close(cursor));
        cursor = NULL;
    }

#ifdef HAVE_DIAGNOSTIC
    WT_ASSERT(session, txn->prepare_count == prepare_count);
    txn->prepare_count = 0;
#endif

    /*
     * Note: we're going to commit: nothing can fail after this point. Set a check, it's too easy to
     * call an error handling macro between here and the end of the function.
     */
    cannot_fail = true;

    /*
     * Free updates.
     *
     * Resolve any fast-truncate transactions and allow eviction to proceed on instantiated pages.
     * This isn't done as part of the initial processing because until now the commit could still
     * switch to an abort. The action allowing eviction to proceed is clearing the WT_UPDATE list,
     * (if any), associated with the commit. We're the only consumer of that list and we no longer
     * need it, and eviction knows it means abort or commit has completed on instantiated pages.
     */
    for (i = 0, op = txn->mod; i < txn->mod_count; i++, op++) {
        if (op->type == WT_TXN_OP_REF_DELETE) {
            WT_REF_LOCK(session, op->u.ref, &previous_state);

            /*
             * Only two cases are possible. First: the state is WT_REF_DELETED. In this case
             * page_del cannot be NULL yet because an uncommitted operation cannot have reached
             * global visibility. Otherwise: there is an uncommitted delete operation we're
             * handling, so the page can't be in a non-deleted state, and the tree can't be
             * readonly. Therefore the page must have been instantiated, the state must be
             * WT_REF_MEM, and there should be an update list in modify->inst_updates. There may
             * also be a non-NULL page_del to update.
             */
            if (previous_state != WT_REF_DELETED) {
                WT_ASSERT(session, op->u.ref->page != NULL && op->u.ref->page->modify != NULL);
                __wt_free(session, op->u.ref->page->modify->inst_updates);
            }
            if (op->u.ref->page_del != NULL)
                op->u.ref->page_del->committed = true;
            WT_REF_UNLOCK(op->u.ref, previous_state);
        }
        __wt_txn_op_free(session, op);
    }
    txn->mod_count = 0;

    /*
     * If durable is set, we'll try to update the global durable timestamp with that value. If
     * durable isn't set, durable is implied to be the same as commit so we'll use that instead.
     */
    candidate_durable_timestamp = WT_TS_NONE;
    if (F_ISSET(txn, WT_TXN_HAS_TS_DURABLE))
        candidate_durable_timestamp = txn->durable_timestamp;
    else if (F_ISSET(txn, WT_TXN_HAS_TS_COMMIT))
        candidate_durable_timestamp = txn->commit_timestamp;

    __wt_txn_release(session);
    if (locked)
        __wt_readunlock(session, &txn_global->visibility_rwlock);

    /*
     * If we have made some updates visible, start a new commit generation: any cached snapshots
     * have to be refreshed.
     */
    if (!readonly)
        __wt_gen_next(session, WT_GEN_COMMIT, NULL);

    /* First check if we've made something durable in the future. */
    update_durable_ts = false;
    prev_durable_timestamp = WT_TS_NONE;
    if (candidate_durable_timestamp != WT_TS_NONE) {
        prev_durable_timestamp = txn_global->durable_timestamp;
        update_durable_ts = candidate_durable_timestamp > prev_durable_timestamp;
    }

    /*
     * If it looks like we'll need to move the global durable timestamp, attempt atomic cas and
     * re-check.
     */
    if (update_durable_ts)
        while (candidate_durable_timestamp > prev_durable_timestamp) {
            //txn_global->durable_timestamp¸³ÖµÎª×îÐÂµÄdurable_timestamp
            if (__wt_atomic_cas64(&txn_global->durable_timestamp, prev_durable_timestamp,
                  candidate_durable_timestamp)) {
                //yang add todo xxxxxxxxxxxxxxxxxxxxxxxx Ôö¼Ódurable_timestampÀí½âÈÕÖ¾
                __wt_verbose_debug2(session, WT_VERB_TRANSACTION, "update durable_timestamp %" PRIu64, txn_global->durable_timestamp);
                txn_global->has_durable_timestamp = true;
                break;
            }
            prev_durable_timestamp = txn_global->durable_timestamp;
        }

    /*
     * Stable timestamp cannot be concurrently increased greater than or equal to the prepared
     * transaction's durable timestamp. Otherwise, checkpoint may only write partial updates of the
     * transaction.
     */
    if (prepare && txn->durable_timestamp <= txn_global->stable_timestamp) {
        WT_ERR(__wt_verbose_dump_sessions(session, true));
        WT_ERR_PANIC(session, WT_PANIC,
          "stable timestamp is larger than or equal to the committing prepared transaction's "
          "durable timestamp");
    }

    /*
     * We're between transactions, if we need to block for eviction, it's a good time to do so.
     * Ignore error returns, the return must reflect the fate of the transaction.
     */
    if (!readonly) {
        //printf("yang test ..................__wt_txn_commit...................................\r\n");
         //¼ì²é½ÚµãÒÑÊ¹ÓÃÄÚ´æ¡¢ÔàÊý¾Ý¡¢updateÊý¾Ý°Ù·Ö±È£¬ÅÐ¶ÏÊÇ·ñÐèÒªÓÃ»§Ïß³Ì¡¢evictÏß³Ì½øÐÐevict´¦Àí
        WT_IGNORE_RET(__wt_cache_eviction_check(session, false, false, NULL));
    }
    return (0);

err:
    if (cursor != NULL)
        WT_TRET(cursor->close(cursor));

    if (locked)
        __wt_readunlock(session, &txn_global->visibility_rwlock);

    /* Check for a failure after we can no longer fail. */
    if (cannot_fail)
        WT_RET_PANIC(session, ret,
          "failed to commit a transaction after data corruption point, failing the system");

    /*
     * Check for a prepared transaction, and quit: we can't ignore the error and we can't roll back
     * a prepared transaction.
     */
    if (prepare)
        WT_RET_PANIC(session, ret, "failed to commit prepared transaction, failing the system");

    WT_TRET(__wt_session_reset_cursors(session, false));
    WT_TRET(__wt_txn_rollback(session, cfg));
    return (ret);
}

/*
 * __wt_txn_prepare --
 *     Prepare the current transaction.
 */
int
__wt_txn_prepare(WT_SESSION_IMPL *session, const char *cfg[])
{
    WT_TXN *txn;
    WT_TXN_OP *op;
    WT_UPDATE *upd, *tmp;
    u_int i, prepared_updates, prepared_updates_key_repeated;

    txn = session->txn;
    prepared_updates = prepared_updates_key_repeated = 0;

    WT_ASSERT(session, F_ISSET(txn, WT_TXN_RUNNING));
    WT_ASSERT(session, !F_ISSET(txn, WT_TXN_ERROR));

    /*
     * A transaction should not have updated any of the logged tables, if debug mode logging is not
     * turned on.
     */
    if (txn->logrec != NULL && !FLD_ISSET(S2C(session)->log_flags, WT_CONN_LOG_DEBUG_MODE))
        WT_RET_MSG(session, EINVAL, "a prepared transaction cannot include a logged table");

    /* Set the prepare timestamp. */
    WT_RET(__wt_txn_set_timestamp(session, cfg, false));

    if (!F_ISSET(txn, WT_TXN_HAS_TS_PREPARE))
        WT_RET_MSG(session, EINVAL, "prepare timestamp is not set");

    if (F_ISSET(txn, WT_TXN_HAS_TS_COMMIT))
        WT_RET_MSG(
          session, EINVAL, "commit timestamp must not be set before transaction is prepared");

    /*
     * We are about to release the snapshot: copy values into any positioned cursors so they don't
     * point to updates that could be freed once we don't have a snapshot.
     */
    if (session->ncursors > 0) {
        WT_DIAGNOSTIC_YIELD;
        WT_RET(__wt_session_copy_values(session));
    }

    for (i = 0, op = txn->mod; i < txn->mod_count; i++, op++) {
        /* Assert it's not an update to the history store file. */
        WT_ASSERT(session, S2C(session)->cache->hs_fileid == 0 || !WT_IS_HS(op->btree->dhandle));

        /* Metadata updates should never be prepared. */
        WT_ASSERT(session, !WT_IS_METADATA(op->btree->dhandle));
        if (WT_IS_METADATA(op->btree->dhandle))
            continue;

        /*
         * Logged table updates should never be prepared. As these updates are immediately durable,
         * it is not possible to roll them back if the prepared transaction is rolled back.
         */
        if (F_ISSET(op->btree, WT_BTREE_LOGGED))
            WT_RET_MSG(session, ENOTSUP,
              "%s: transaction prepare is not supported on logged tables or tables without "
              "timestamps",
              op->btree->dhandle->name);
        switch (op->type) {
        case WT_TXN_OP_NONE:
            break;
        case WT_TXN_OP_BASIC_COL:
        case WT_TXN_OP_BASIC_ROW:
        case WT_TXN_OP_INMEM_COL:
        case WT_TXN_OP_INMEM_ROW:
            upd = op->u.op_upd;

            /*
             * Switch reserved operation to abort to simplify obsolete update list truncation. The
             * object free function clears the operation type so we don't try to visit this update
             * again: it can be discarded.
             */
            if (upd->type == WT_UPDATE_RESERVE) {
                upd->txnid = WT_TXN_ABORTED;
                __wt_txn_op_free(session, op);
                break;
            }

            ++prepared_updates;

            /* Set prepare timestamp. */
            upd->start_ts = txn->prepare_timestamp;

            /*
             * By default durable timestamp is assigned with 0 which is same as WT_TS_NONE. Assign
             * it with WT_TS_NONE to make sure in case if we change the macro value it shouldn't be
             * a problem.
             */
            upd->durable_ts = WT_TS_NONE;

            WT_PUBLISH(upd->prepare_state, WT_PREPARE_INPROGRESS);
            op->u.op_upd = NULL;

            /*
             * If there are older updates to this key by the same transaction, set the repeated key
             * flag on this operation. This is later used in txn commit/rollback so we only resolve
             * each set of prepared updates once. Skip reserved updates, they're ignored as they're
             * simply discarded when we find them. Also ignore updates created by instantiating fast
             * truncation pages, they aren't linked into the transaction's modify list and so can't
             * be considered.
             */
            for (tmp = upd->next; tmp != NULL && tmp->txnid == upd->txnid; tmp = tmp->next)
                if (tmp->type != WT_UPDATE_RESERVE &&
                  !F_ISSET(tmp, WT_UPDATE_RESTORED_FAST_TRUNCATE)) {
                    //Èç¹ûudpÁ´±íÉÏÃæÓÐÁ¬ÐøµÄÁ½¸öudpµÄÊÂÎñidÒ»Ñù(Ò²¾ÍÊÇÍ¬Ò»¸öÊÂÎñÁ¬ÐøÐÞ¸ÄÁËÍ¬Ò»¸öK)£¬Ôò»áÉèÖÃWT_TXN_OP_KEY_REPEATED±êÊ¶ __wt_txn_prepare
                    F_SET(op, WT_TXN_OP_KEY_REPEATED);
                    ++prepared_updates_key_repeated;
                    break;
                }
            break;
        case WT_TXN_OP_REF_DELETE:
            __wt_txn_op_delete_apply_prepare_state(session, op->u.ref, false);
            break;
        case WT_TXN_OP_TRUNCATE_COL:
        case WT_TXN_OP_TRUNCATE_ROW:
            /* Other operations don't need timestamps. */
            break;
        }
    }
    WT_STAT_CONN_INCRV(session, txn_prepared_updates, prepared_updates);
    WT_STAT_CONN_INCRV(session, txn_prepared_updates_key_repeated, prepared_updates_key_repeated);
#ifdef HAVE_DIAGNOSTIC
    txn->prepare_count = prepared_updates;
#endif

    /* Set transaction state to prepare. */
    F_SET(session->txn, WT_TXN_PREPARE);

    /* Release our snapshot in case it is keeping data pinned. */
    __wt_txn_release_snapshot(session);

    /*
     * Clear the transaction's ID from the global table, to facilitate prepared data visibility, but
     * not from local transaction structure.
     */
    if (F_ISSET(txn, WT_TXN_HAS_ID))
        __txn_remove_from_global_table(session);

    return (0);
}

/*
 * __wt_txn_rollback --
 *     Roll back the current transaction.
 */
int
__wt_txn_rollback(WT_SESSION_IMPL *session, const char *cfg[])
{
    WT_CURSOR *cursor;
    WT_DECL_RET;
    WT_TXN *txn;
    WT_TXN_OP *op;
    WT_UPDATE *upd;
    u_int i;
#ifdef HAVE_DIAGNOSTIC
    u_int prepare_count;
#endif
    bool prepare, readonly;

    cursor = NULL;
    txn = session->txn;
#ifdef HAVE_DIAGNOSTIC
    prepare_count = 0;
#endif
    prepare = F_ISSET(txn, WT_TXN_PREPARE);
    readonly = txn->mod_count == 0;

    WT_ASSERT(session, F_ISSET(txn, WT_TXN_RUNNING));

    /* Configure the timeout for this rollback operation. */
    WT_TRET(__txn_config_operation_timeout(session, cfg, true));

    /*
     * Resolving prepared updates is expensive. Sort prepared modifications so all updates for each
     * page within each file are done at the same time.
     */
    if (prepare)
        __wt_qsort(txn->mod, txn->mod_count, sizeof(WT_TXN_OP), __txn_mod_compare);

    /* Rollback and free updates. */
    for (i = 0, op = txn->mod; i < txn->mod_count; i++, op++) {
        /* Assert it's not an update to the history store file. */
        WT_ASSERT(session, S2C(session)->cache->hs_fileid == 0 || !WT_IS_HS(op->btree->dhandle));

        /* Metadata updates should never be rolled back. */
        WT_ASSERT(session, !WT_IS_METADATA(op->btree->dhandle));
        if (WT_IS_METADATA(op->btree->dhandle))
            continue;

        switch (op->type) {
        case WT_TXN_OP_NONE:
            break;
        case WT_TXN_OP_BASIC_COL:
        case WT_TXN_OP_BASIC_ROW:
        case WT_TXN_OP_INMEM_COL:
        case WT_TXN_OP_INMEM_ROW:
            upd = op->u.op_upd;

            if (!prepare) {
                if (S2C(session)->cache->hs_fileid != 0 &&
                  op->btree->id == S2C(session)->cache->hs_fileid)
                    break;
                WT_ASSERT(session, upd->txnid == txn->id || upd->txnid == WT_TXN_ABORTED);
                //»Ø¹öµÄÊ±ºòºÜ¼òµ¥£¬Ö÷Òª¾ÍÊÇ±êÊ¶¸ÃudpÊÂÎñÎªabort, ÕâÑù¸Ãudp¾Í²»¿É¼û£¬ÔÚÆäËûÂß¼­¶ÁÈ¡¸ÃudpµÄÊ±ºò»áÖ±½ÓÌø¹ý

                //µ«ÊÇÍâ²ãÍ¨¹ý__wt_txn_log_commitÒÑ¾­°ÑWAL³Ö¾Ã»¯ÁË£¬ÄÚ´æ»Ø¹öÁË£¬ÄÚ´æÃ»ÓÐ»Ø¹ö ???????  yang add todo xxxxxxxxxx
                upd->txnid = WT_TXN_ABORTED;
            } else {
                /*
                 * If an operation has the key repeated flag set, skip resolving prepared updates as
                 * the work will happen on a different modification in this txn.
                 */
                if (!F_ISSET(op, WT_TXN_OP_KEY_REPEATED))
                    WT_TRET(__txn_resolve_prepared_op(session, op, false, &cursor));
#ifdef HAVE_DIAGNOSTIC
                ++prepare_count;
#endif
            }
            break;
        case WT_TXN_OP_REF_DELETE:
            WT_TRET(__wt_delete_page_rollback(session, op->u.ref));
            break;
        case WT_TXN_OP_TRUNCATE_COL:
        case WT_TXN_OP_TRUNCATE_ROW:
            /*
             * Nothing to do: these operations are only logged for recovery. The in-memory changes
             * will be rolled back with a combination of WT_TXN_OP_REF_DELETE and WT_TXN_OP_INMEM
             * operations.
             */
            break;
        }

        __wt_txn_op_free(session, op);
        /* If we used the cursor to resolve prepared updates, the key now has been freed. */
        if (cursor != NULL)
            WT_CLEAR(cursor->key);
    }
    txn->mod_count = 0;
#ifdef HAVE_DIAGNOSTIC
    WT_ASSERT(session, txn->prepare_count == prepare_count);
    txn->prepare_count = 0;
#endif

    if (cursor != NULL) {
        WT_TRET(cursor->close(cursor));
        cursor = NULL;
    }

    __wt_txn_release(session);

    /*
     * We're between transactions, if we need to block for eviction, it's a good time to do so.
     * Ignore error returns, the return must reflect the fate of the transaction.
     */
    if (!readonly) {
       // printf("yang test ..................__wt_txn_rollback...................................\r\n");
        WT_IGNORE_RET(__wt_cache_eviction_check(session, false, false, NULL));
    }

    return (ret);
}

/*
 * __wt_txn_rollback_required --
 *     Prepare to log a reason if the user attempts to use the transaction to do anything other than
 *     rollback.
 //±êÊ¶¸ÃsessionÐèÒª»Ø¹ö£¬ÉèÖÃ»Ø¹öÔ­Òò
 */
int
__wt_txn_rollback_required(WT_SESSION_IMPL *session, const char *reason)
{
    session->txn->rollback_reason = reason;
    return (WT_ROLLBACK);
}

/*
 * __wt_txn_init --
 *     Initialize a session's transaction data.
 */
int
__wt_txn_init(WT_SESSION_IMPL *session, WT_SESSION_IMPL *session_ret)
{
    WT_TXN *txn;

    /* Allocate the WT_TXN structure, including a variable length array of snapshot information. */
    WT_RET(__wt_calloc(session, 1,
      sizeof(WT_TXN) + sizeof(txn->snapshot[0]) * S2C(session)->session_size, &session_ret->txn));
    txn = session_ret->txn;
    txn->snapshot = txn->__snapshot;
    txn->id = WT_TXN_NONE;

    WT_ASSERT(session,
      S2C(session_ret)->txn_global.txn_shared_list == NULL ||
        WT_SESSION_TXN_SHARED(session_ret)->pinned_id == WT_TXN_NONE);

    /*
     * Take care to clean these out in case we are reusing the transaction for eviction.
     */
    txn->mod = NULL;

    txn->isolation = session_ret->isolation;
    return (0);
}

/*
 * __wt_txn_init_checkpoint_cursor --
 *     Create a transaction object for a checkpoint cursor. On success, takes charge of the snapshot
 *     array passed down, which should have been allocated separately, and nulls the pointer. (On
 *     failure, the caller must destroy it.)
 */
int
__wt_txn_init_checkpoint_cursor(
  WT_SESSION_IMPL *session, WT_CKPT_SNAPSHOT *snapinfo, WT_TXN **txn_ret)
{
    WT_TXN *txn;

    /*
     * Allocate the WT_TXN structure. Don't use the variable-length array at the end, because the
     * code for reading the snapshot allocates the snapshot list itself; copying it serves no
     * purpose, and twisting up the read code to allow controlling the allocation from here is not
     * worthwhile.
     *
     * Allocate a byte at the end so that __snapshot (at the end of the struct) doesn't point at an
     * adjacent malloc block; we'd like to be able to assert that in checkpoint cursor transactions
     * snapshot doesn't point at __snapshot, to make sure an ordinary transaction doesn't flow to
     * the checkpoint cursor close function. If an adjacent malloc block, that might not be true.
     */
    WT_RET(__wt_calloc(session, 1, sizeof(WT_TXN) + 1, &txn));

    /* We have no transaction ID and won't gain one, being read-only. */
    txn->id = WT_TXN_NONE;

    /* Use snapshot isolation. */
    txn->isolation = WT_ISO_SNAPSHOT;

    /* Save the snapshot data. */
    txn->snap_min = snapinfo->snapshot_min;
    txn->snap_max = snapinfo->snapshot_max;
    txn->snapshot = snapinfo->snapshot_txns;
    txn->snapshot_count = snapinfo->snapshot_count;

    /*
     * At this point we have taken charge of the snapshot's transaction list; it has been moved to
     * the dummy transaction. Null the caller's copy so it doesn't get freed twice if something
     * above us fails after we return.
     */
    snapinfo->snapshot_txns = NULL;

    /* Set the read and oldest timestamps.  */
    txn->checkpoint_read_timestamp = snapinfo->stable_ts;
    txn->checkpoint_oldest_timestamp = snapinfo->oldest_ts;

    /* Set the flag that indicates if we have a timestamp. */
    if (txn->checkpoint_read_timestamp != WT_TS_NONE)
        F_SET(txn, WT_TXN_SHARED_TS_READ);

    /*
     * Set other relevant flags. Always ignore prepared values; they can get into checkpoints.
     *
     * Prepared values don't get written out by checkpoints by default, but can appear if pages get
     * evicted. So whether any given prepared value from any given prepared but yet-uncommitted
     * transaction shows up or not is arbitrary and unpredictable. Therefore, failing on it serves
     * no data integrity purpose and will only make the system flaky.
     *
     * There is a problem, however. Prepared transactions are allowed to commit before stable if
     * stable moves forward, as long as the durable timestamp is after stable. Such transactions can
     * therefore be committed after (in execution time) the checkpoint is taken but with a commit
     * timestamp less than the checkpoint's stable timestamp. They will then exist in the live
     * database and be visible if read as of the checkpoint timestamp, but not exist in the
     * checkpoint, which is inconsistent. There is probably nothing that can be done about this
     * without making prepared transactions durable in prepared state, which is a Big Deal, so
     * applications using prepared transactions and using this commit leeway need to be cognizant of
     * the issue.
     */
    F_SET(txn,
      WT_TXN_HAS_SNAPSHOT | WT_TXN_IS_CHECKPOINT | WT_TXN_READONLY | WT_TXN_RUNNING |
        WT_TXN_IGNORE_PREPARE);

    *txn_ret = txn;
    return (0);
}

/*
 * __wt_txn_close_checkpoint_cursor --
 *     Dispose of the private transaction object in a checkpoint cursor.
 */
void
__wt_txn_close_checkpoint_cursor(WT_SESSION_IMPL *session, WT_TXN **txn_arg)
{
    WT_TXN *txn;

    txn = *txn_arg;
    *txn_arg = NULL;

    /* The snapshot list isn't at the end of the transaction structure here; free it explicitly. */
    WT_ASSERT(session, txn->snapshot != txn->__snapshot);
    __wt_free(session, txn->snapshot);

    __wt_free(session, txn);
}

/*
 * __wt_txn_stats_update --
 *     Update the transaction statistics for return to the application.
 */
void
__wt_txn_stats_update(WT_SESSION_IMPL *session)
{
    WT_CONNECTION_IMPL *conn;
    WT_CONNECTION_STATS **stats;
    WT_TXN_GLOBAL *txn_global;
    wt_timestamp_t checkpoint_timestamp;
    wt_timestamp_t durable_timestamp;
    wt_timestamp_t oldest_active_read_timestamp;
    wt_timestamp_t pinned_timestamp;
    uint64_t checkpoint_pinned;

    conn = S2C(session);
    txn_global = &conn->txn_global;
    stats = conn->stats;
    checkpoint_pinned = txn_global->checkpoint_txn_shared.pinned_id;

    WT_STAT_SET(session, stats, txn_pinned_range, txn_global->current - txn_global->oldest_id);

    checkpoint_timestamp = txn_global->checkpoint_timestamp;
    durable_timestamp = txn_global->durable_timestamp;
    pinned_timestamp = txn_global->pinned_timestamp;

    if (checkpoint_timestamp != WT_TS_NONE && checkpoint_timestamp < pinned_timestamp)
        pinned_timestamp = checkpoint_timestamp;
    WT_STAT_SET(session, stats, txn_pinned_timestamp, durable_timestamp - pinned_timestamp);
    //yang add todo xxxxxxxxxx checkpointÍê³Éºó»á±»Çå0£¬ÕâÀïµÄÍ³¼ÆÓÐÎÊÌâ
    if (checkpoint_timestamp == WT_TS_NONE)
        WT_STAT_SET(session, stats, txn_pinned_timestamp_checkpoint, 0);
    else 
        WT_STAT_SET(
          session, stats, txn_pinned_timestamp_checkpoint, durable_timestamp - checkpoint_timestamp); 
          
    WT_STAT_SET(session, stats, txn_pinned_timestamp_oldest,
      durable_timestamp - txn_global->oldest_timestamp);

    __wt_txn_get_pinned_timestamp(session, &oldest_active_read_timestamp, 0);
    if (oldest_active_read_timestamp == 0) {
        WT_STAT_SET(session, stats, txn_timestamp_oldest_active_read, 0);
        WT_STAT_SET(session, stats, txn_pinned_timestamp_reader, 0);
    } else {
        WT_STAT_SET(session, stats, txn_timestamp_oldest_active_read, oldest_active_read_timestamp);
        WT_STAT_SET(session, stats, txn_pinned_timestamp_reader,
          durable_timestamp - oldest_active_read_timestamp);
    }

    WT_STAT_SET(session, stats, txn_pinned_checkpoint_range,
      checkpoint_pinned == WT_TXN_NONE ? 0 : txn_global->current - checkpoint_pinned);

    WT_STAT_SET(session, stats, txn_checkpoint_prep_max, conn->ckpt_prep_max);
    if (conn->ckpt_prep_min != UINT64_MAX)
        WT_STAT_SET(session, stats, txn_checkpoint_prep_min, conn->ckpt_prep_min);
    WT_STAT_SET(session, stats, txn_checkpoint_prep_recent, conn->ckpt_prep_recent);
    WT_STAT_SET(session, stats, txn_checkpoint_prep_total, conn->ckpt_prep_total);
    WT_STAT_SET(session, stats, txn_checkpoint_time_max, conn->ckpt_time_max);
    if (conn->ckpt_time_min != UINT64_MAX)
        WT_STAT_SET(session, stats, txn_checkpoint_time_min, conn->ckpt_time_min);
    WT_STAT_SET(session, stats, txn_checkpoint_time_recent, conn->ckpt_time_recent);
    WT_STAT_SET(session, stats, txn_checkpoint_time_total, conn->ckpt_time_total);
}

/*
 * __wt_txn_release_resources --
 *     Release resources for a session's transaction data.
 */
void
__wt_txn_release_resources(WT_SESSION_IMPL *session)
{
    WT_TXN *txn;

    if ((txn = session->txn) == NULL)
        return;

    WT_ASSERT(session, txn->mod_count == 0);
    __wt_free(session, txn->mod);
    txn->mod_alloc = 0;
    txn->mod_count = 0;
}

/*
 * __wt_txn_destroy --
 *     Destroy a session's transaction data.
 */
void
__wt_txn_destroy(WT_SESSION_IMPL *session)
{
    __wt_txn_release_resources(session);
    __wt_free(session, session->txn);
}

/*
 * __wt_txn_global_init --
 *     Initialize the global transaction state.
 */
int
__wt_txn_global_init(WT_SESSION_IMPL *session, const char *cfg[])
{
    WT_CONNECTION_IMPL *conn;
    WT_TXN_GLOBAL *txn_global;
    WT_TXN_SHARED *s;
    u_int i;

    WT_UNUSED(cfg);
    conn = S2C(session);

    txn_global = &conn->txn_global;
    txn_global->current = txn_global->last_running = txn_global->metadata_pinned =
      txn_global->oldest_id = WT_TXN_FIRST;

    WT_RWLOCK_INIT_TRACKED(session, &txn_global->rwlock, txn_global);
    WT_RET(__wt_rwlock_init(session, &txn_global->visibility_rwlock));

    WT_RET(__wt_calloc_def(session, conn->session_size, &txn_global->txn_shared_list));

    //printf("yang test ..............__wt_txn_global_init...............session_size:%u\r\n",
     //   conn->session_size);
    for (i = 0, s = txn_global->txn_shared_list; i < conn->session_size; i++, s++)
        s->id = s->metadata_pinned = s->pinned_id = WT_TXN_NONE;

    return (0);
}

/*
 * __wt_txn_global_destroy --
 *     Destroy the global transaction state.
 */
void
__wt_txn_global_destroy(WT_SESSION_IMPL *session)
{
    WT_CONNECTION_IMPL *conn;
    WT_TXN_GLOBAL *txn_global;

    conn = S2C(session);
    txn_global = &conn->txn_global;

    if (txn_global == NULL)
        return;

    __wt_rwlock_destroy(session, &txn_global->rwlock);
    __wt_rwlock_destroy(session, &txn_global->visibility_rwlock);
    __wt_free(session, txn_global->txn_shared_list);
}

/*
 * __wt_txn_activity_drain --
 *     Wait for transactions to quiesce.
 */
int
__wt_txn_activity_drain(WT_SESSION_IMPL *session)
{
    bool txn_active;

    /*
     * It's possible that the eviction server is in the middle of a long operation, with a
     * transaction ID pinned. In that case, we will loop here until the transaction ID is released,
     * when the oldest transaction ID will catch up with the current ID.
     */
    for (;;) {
        WT_RET(__wt_txn_activity_check(session, &txn_active));
        if (!txn_active)
            break;

        WT_STAT_CONN_INCR(session, txn_release_blocked);
        __wt_yield();
    }

    return (0);
}

/*
 * __wt_txn_global_shutdown --
 *     Shut down the global transaction state.
 */
//__conn_closeÊ±ºò£¬Ò²¾ÍÊÇ½Úµãshutdown£¬»ñÈ¡Ò»¸öclose_ckptÄÚ²¿session, È»ºó×öcheckpoint,
int
__wt_txn_global_shutdown(WT_SESSION_IMPL *session, const char **cfg)
{
    WT_CONFIG_ITEM cval;
    WT_CONNECTION_IMPL *conn;
    WT_DECL_RET;
    WT_SESSION_IMPL *s;
    char ts_string[WT_TS_INT_STRING_SIZE];
    const char *ckpt_cfg;
    bool use_timestamp;

    conn = S2C(session);
    use_timestamp = false;

    /*
     * Perform a system-wide checkpoint so that all tables are consistent with each other. All
     * transactions are resolved but ignore timestamps to make sure all data gets to disk. Do this
     * before shutting down all the subsystems. We have shut down all user sessions, but send in
     * true for waiting for internal races.
     */
    F_SET(conn, WT_CONN_CLOSING_CHECKPOINT);
    WT_TRET(__wt_config_gets(session, cfg, "use_timestamp", &cval));
    ckpt_cfg = "use_timestamp=false";
    if (cval.val != 0) {
        ckpt_cfg = "use_timestamp=true";
        if (conn->txn_global.has_stable_timestamp)
            use_timestamp = true;
    }
    if (!F_ISSET(conn, WT_CONN_IN_MEMORY | WT_CONN_READONLY | WT_CONN_PANIC)) {
        /*
         * Perform rollback to stable to ensure that the stable version is written to disk on a
         * clean shutdown.
         */
        if (use_timestamp) {
            __wt_verbose(session, WT_VERB_RTS,
              "performing shutdown rollback to stable with stable timestamp: %s",
              __wt_timestamp_to_string(conn->txn_global.stable_timestamp, ts_string));
            WT_TRET(__wt_rollback_to_stable(session, cfg, true));
        }

        s = NULL;
        //»ñÈ¡Ò»¸öclose_ckptÄÚ²¿session, È»ºó__wt_txn_checkpoint×öcheckpoint,
        WT_TRET(__wt_open_internal_session(conn, "close_ckpt", true, 0, 0, &s));
        if (s != NULL) {
            const char *checkpoint_cfg[] = {
              WT_CONFIG_BASE(session, WT_SESSION_checkpoint), ckpt_cfg, NULL};
            WT_TRET(__wt_txn_checkpoint(s, checkpoint_cfg, true));

            /*
             * Mark the metadata dirty so we flush it on close, allowing recovery to be skipped.
             */
            WT_WITH_DHANDLE(s, WT_SESSION_META_DHANDLE(s), __wt_tree_modify_set(s));

            WT_TRET(__wt_session_close_internal(s));
        }
    }

    return (ret);
}

/*
 * __wt_txn_is_blocking --
 *     Return an error if this transaction is likely blocking eviction because of a pinned
 *     transaction ID, called by eviction to determine if a worker thread should be released from
 *     eviction.
 */
int
__wt_txn_is_blocking(WT_SESSION_IMPL *session)
{
    WT_TXN *txn;
    WT_TXN_SHARED *txn_shared;
    uint64_t global_oldest;

    txn = session->txn;
    txn_shared = WT_SESSION_TXN_SHARED(session);
    global_oldest = S2C(session)->txn_global.oldest_id;

    /* We can't roll back prepared transactions. */
    if (F_ISSET(txn, WT_TXN_PREPARE))
        return (0);

#ifndef WT_STANDALONE_BUILD
    /*
     * FIXME: SERVER-44870
     *
     * MongoDB can't (yet) handle rolling back read only transactions. For this reason, don't check
     * unless there's at least one update or we're configured to time out thread operations (a way
     * to confirm our caller is prepared for rollback).
     */
    if (txn->mod_count == 0 && !__wt_op_timer_fired(session))
        return (0);
#endif

    /*
     * Check if either the transaction's ID or its pinned ID is equal to the oldest transaction ID.
     */
    return (txn_shared->id == global_oldest || txn_shared->pinned_id == global_oldest ?
        __wt_txn_rollback_required(session, WT_TXN_ROLLBACK_REASON_OLDEST_FOR_EVICTION) :
        0);
}

/*
 * __wt_verbose_dump_txn_one --
 *     Output diagnostic information about a transaction structure.
 */
int
__wt_verbose_dump_txn_one(
  WT_SESSION_IMPL *session, WT_SESSION_IMPL *txn_session, int error_code, const char *error_string, 
  WT_ITEM *txn_one_buf)
{
    WT_TXN *txn;
    WT_TXN_SHARED *txn_shared;
    uint32_t i, buf_len;
    //char buf[512 + 2560], snapshot_buf[2560], snapshot_buf_tmp[32];
    char snapshot_buf_tmp[32];
    char ts_string[6][WT_TS_INT_STRING_SIZE];
    const char *iso_tag;
    WT_DECL_ITEM(snapshot_buf);
    WT_DECL_ITEM(buf);
    WT_DECL_RET;

    txn = txn_session->txn;
    txn_shared = WT_SESSION_TXN_SHARED(txn_session);

    WT_NOT_READ(iso_tag, "INVALID");
    switch (txn->isolation) {
    case WT_ISO_READ_COMMITTED:
        iso_tag = "WT_ISO_READ_COMMITTED";
        break;
    case WT_ISO_READ_UNCOMMITTED:
        iso_tag = "WT_ISO_READ_UNCOMMITTED";
        break;
    case WT_ISO_SNAPSHOT:
        iso_tag = "WT_ISO_SNAPSHOT";
        break;
    }

    WT_ERR(__wt_scr_alloc(session, 2048, &snapshot_buf));
    WT_ERR(__wt_buf_fmt(session, snapshot_buf, "%s", "["));
    for (i = 0; i < txn->snapshot_count; i++) {
        if (i == 0)
            WT_ERR(__wt_snprintf(
              snapshot_buf_tmp, sizeof(snapshot_buf_tmp), "%lu", txn->snapshot[i]));
        else
            WT_ERR(__wt_snprintf(
              snapshot_buf_tmp, sizeof(snapshot_buf_tmp), ", %" PRIu64, txn->snapshot[i]));
        WT_ERR(__wt_buf_catfmt(session, snapshot_buf, "%s", snapshot_buf_tmp));
    }
    WT_ERR(__wt_buf_catfmt(session, snapshot_buf, "%s", "]\0"));

    buf_len = snapshot_buf->size + 512;
    WT_ERR(__wt_scr_alloc(session, buf_len, &buf));
    
    /*
     * Dump the information of the passed transaction into a buffer, to be logged with an optional
     * error message.
     */
    WT_ERR(
      __wt_snprintf((char*)buf->data, buf_len,
        "transaction id: %" PRIu64 ", mod count: %u"
        ", snap min: %" PRIu64 ", snap max: %" PRIu64 ", snapshot count: %u"
        ", snapshot: %s"
        ", commit_timestamp: %s"
        ", durable_timestamp: %s"
        ", first_commit_timestamp: %s"
        ", prepare_timestamp: %s"
        ", pinned_durable_timestamp: %s"
        ", read_timestamp: %s"
        ", checkpoint LSN: [%" PRIu32 "][%" PRIu32 "]"
        ", full checkpoint: %s"
        ", rollback reason: %s"
        ", flags: 0x%08" PRIx32 ", isolation: %s",
        txn->id, txn->mod_count, txn->snap_min, txn->snap_max,
        txn->snapshot_count, (char*)snapshot_buf->data,
        __wt_timestamp_to_string(txn->commit_timestamp, ts_string[0]),
        __wt_timestamp_to_string(txn->durable_timestamp, ts_string[1]),
        __wt_timestamp_to_string(txn->first_commit_timestamp, ts_string[2]),
        __wt_timestamp_to_string(txn->prepare_timestamp, ts_string[3]),
        __wt_timestamp_to_string(txn_shared->pinned_durable_timestamp, ts_string[4]),
        __wt_timestamp_to_string(txn_shared->read_timestamp, ts_string[5]), txn->ckpt_lsn.l.file,
        txn->ckpt_lsn.l.offset, txn->full_ckpt ? "true" : "false",
        txn->rollback_reason == NULL ? "" : txn->rollback_reason, txn->flags, iso_tag));

    /*
     * Log a message and return an error if error code and an optional error string has been passed.
     */
    if (0 != error_code) {
        WT_ERR_MSG(session, error_code, "%s, %s", (char*)buf->data, error_string != NULL ? error_string : "");
    } else {
        WT_ERR(__wt_msg(session, "%s", (char*)buf->data));
    }

    WT_ERR(__wt_buf_catfmt(session, txn_one_buf, "%s\r\n", (char*)buf->data));

err:
    __wt_scr_free(session, &snapshot_buf);
    __wt_scr_free(session, &buf);

    return (ret);
}


/*
 * __wt_verbose_dump_txn --
µ±Ç°ÓÐ¶à¸öÏß³Ì£¬¶à¸öÊÂÎñ£¬²¢ÇÒÕýÔÚ×öcheckpoint£¬²¢ÇÒÆäËûÊÂÎñµÄ¿ìÕÕÖÐÓÐ¸ÃÊÂÎñid,ÆäÈÕÖ¾ÀàËÆÈçÏÂ:
 checkpoint running: yes
 checkpoint generation: 2
 checkpoint pinned ID: 335
 checkpoint txn ID: 352 //ÊÂÎñID
 session count: 36
 Transaction state of active sessions:
 //pinned ID±íÊ¾¿ìÕÕÊý×ésnapshot[]ÖÐ³ýcheckpointÊÂÎñidÍâ×îÐ¡µÄÊÂÎñ;  metadata pinned ID±íÊ¾»ñÈ¡¿ìÕÕµÄÊ±ºòÕýÔÚ×öcheckpointµÄÊÂÎñid£¬checkpoiontÊÂÎñidµÄsnapshot¿ìÕÕÖÐ
 session ID: 22, txn ID: 383, pinned ID: 374, metadata pinned ID: 352, name: connection-open-session
 transaction id: 383, mod count: 3, snap min: 352, snap max: 383, snapshot count: 5, snapshot: [352, 374, 377, 379, 381], commit_timestamp: (0, 374), durable_timestamp: (0, 374), first_commit_timestamp: (0, 373), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 373), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
 session ID: 23, txn ID: 388, pinned ID: 381, metadata pinned ID: 352, name: connection-open-session
 transaction id: 388, mod count: 3, snap min: 352, snap max: 388, snapshot count: 4, snapshot: [352, 381, 383, 385], commit_timestamp: (0, 378), durable_timestamp: (0, 378), first_commit_timestamp: (0, 377), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 377), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
 session ID: 31, txn ID: 385, pinned ID: 377, metadata pinned ID: 352, name: connection-open-session
 transaction id: 385, mod count: 3, snap min: 352, snap max: 385, snapshot count: 5, snapshot: [352, 377, 379, 381, 383], commit_timestamp: (0, 376), durable_timestamp: (0, 376), first_commit_timestamp: (0, 375), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 375), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
 session ID: 34, txn ID: 381, pinned ID: 373, metadata pinned ID: 352, name: connection-open-session
 transaction id: 381, mod count: 3, snap min: 352, snap max: 381, snapshot count: 5, snapshot: [352, 373, 374, 377, 379], commit_timestamp: (0, 372), durable_timestamp: (0, 372), first_commit_timestamp: (0, 371), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 371), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT

 //µ±Ç°ÓÐ¶à¸öÏß³Ì£¬¶à¸öÊÂÎñ£¬²¢ÇÒÕýÔÚ×öcheckpoint£¬²¢ÇÒÆäËûÊÂÎñµÄ¿ìÕÕÖÐÓÐ¸ÃÊÂÎñid,ÆäÈÕÖ¾ÀàËÆÈçÏÂ:
 checkpoint running: yes
 checkpoint generation: 2
 checkpoint pinned ID: 23
 checkpoint txn ID: 32
 session count: 23
 Transaction state of active sessions:
 //metadata pinned ID±íÊ¾µ±Ç°ÕýÔÚ×öcheckpointµÄÊÂÎñid 32ÔÚ±¾ÊÂÎñµÄsnapshot¿ìÕÕÖÐ,Ôò±¾ÊÂÎñµÄmetadata pinned IDÎªcheckpoint¶ÔÓ¦ÊÂÎñid
 session ID: 17, txn ID: 36, pinned ID: 25, metadata pinned ID: 32, name: connection-open-session
 transaction id: 36, mod count: 3, snap min: 25, snap max: 36, snapshot count: 6, snapshot: [25, 27, 29, 31, 32, 34], commit_timestamp: (0, 28), durable_timestamp: (0, 28), first_commit_timestamp: (0, 27), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 27), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
 //metadata pinned ID±íÊ¾µ±Ç°ÕýÔÚ×öcheckpointµÄÊÂÎñid 32²»ÔÚ±¾ÊÂÎñµÄsnapshot¿ìÕÕÖÐ,Ôò±¾ÊÂÎñµÄmetadata pinned IDÎª0
 session ID: 18, txn ID: 25, pinned ID: 17, metadata pinned ID: 0, name: connection-open-session
 transaction id: 25, mod count: 3, snap min: 17, snap max: 25, snapshot count: 5, snapshot: [17, 18, 19, 21, 23], commit_timestamp: (0, 18), durable_timestamp: (0, 18), first_commit_timestamp: (0, 17), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 17), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
 session ID: 19, txn ID: 27, pinned ID: 18, metadata pinned ID: 0, name: connection-open-session
 transaction id: 27, mod count: 3, snap min: 18, snap max: 27, snapshot count: 5, snapshot: [18, 19, 21, 23, 25], commit_timestamp: (0, 20), durable_timestamp: (0, 20), first_commit_timestamp: (0, 19), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 19), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
 session ID: 20, txn ID: 34, pinned ID: 23, metadata pinned ID: 32, name: connection-open-session
 transaction id: 34, mod count: 3, snap min: 23, snap max: 34, snapshot count: 6, snapshot: [23, 25, 27, 29, 31, 32], commit_timestamp: (0, 26), durable_timestamp: (0, 26), first_commit_timestamp: (0, 25), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 25), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT


//checkpointÖ´ÐÐÍê³Éºó£¬Èç¹ûÆäËûÊÂÎñµÄsnapshot[]ÖÐÃ»ÓÐ¸ÃcheckpointµÄÊÂÎñid£¬Ôò¶ÔÓ¦ÊÂÎñµÄmetadata pinned ID:»Ö¸´Îª0
checkpoint running: no
checkpoint generation: 2
checkpoint pinned ID: 0
checkpoint txn ID: 0
session count: 23
Transaction state of active sessions:
session ID: 17, txn ID: 84, pinned ID: 74, metadata pinned ID: 0, name: connection-open-session
transaction id: 84, mod count: 3, snap min: 74, snap max: 84, snapshot count: 5, snapshot: [74, 76, 78, 80, 82], commit_timestamp: (0, 76), durable_timestamp: (0, 76), first_commit_timestamp: (0, 75), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 75), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
session ID: 18, txn ID: 86, pinned ID: 76, metadata pinned ID: 0, name: connection-open-session
transaction id: 86, mod count: 3, snap min: 76, snap max: 86, snapshot count: 5, snapshot: [76, 78, 80, 82, 84], commit_timestamp: (0, 78), durable_timestamp: (0, 78), first_commit_timestamp: (0, 77), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 77), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
session ID: 19, txn ID: 76, pinned ID: 66, metadata pinned ID: 32, name: connection-open-session
transaction id: 76, mod count: 3, snap min: 32, snap max: 76, snapshot count: 6, snapshot: [32, 66, 68, 70, 72, 74], commit_timestamp: (0, 68), durable_timestamp: (0, 68), first_commit_timestamp: (0, 67), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 67), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
session ID: 20, txn ID: 82, pinned ID: 72, metadata pinned ID: 32, name: connection-open-session
transaction id: 82, mod count: 3, snap min: 32, snap max: 82, snapshot count: 6, snapshot: [32, 72, 74, 76, 78, 80], commit_timestamp: (0, 74), durable_timestamp: (0, 74), first_commit_timestamp: (0, 73), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 73), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
session ID: 21, txn ID: 78, pinned ID: 68, metadata pinned ID: 32, name: connection-open-session
transaction id: 78, mod count: 3, snap min: 32, snap max: 78, snapshot count: 6, snapshot: [32, 68, 70, 72, 74, 76], commit_timestamp: (0, 70), durable_timestamp: (0, 70), first_commit_timestamp: (0, 69), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 69), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
session ID: 22, txn ID: 80, pinned ID: 70, metadata pinned ID: 32, name: connection-open-session
transaction id: 80, mod count: 3, snap min: 32, snap max: 80, snapshot count: 6, snapshot: [32, 70, 72, 74, 76, 78], commit_timestamp: (0, 72), durable_timestamp: (0, 72), first_commit_timestamp: (0, 71), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 71), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT


checkpoint running: yes
checkpoint generation: 2
//checkpoint pinned IDÒ²¾ÍÊÇcheckpoint session IDËùÄÜ¿´µ½µÄsnapshotÊÂÎñ¿ìÕÕµÄ×îÐ¡ÊÂÎñ
checkpoint pinned ID: 22
checkpoint txn ID: 29
session count: 22
Transaction state of active sessions:
session ID: 16, txn ID: 31, pinned ID: 22, metadata pinned ID: 29, name: connection-open-session
transaction id: 31, mod count: 3, snap min: 22, snap max: 31, snapshot count: 5, snapshot: [22, 24, 26, 28, 29], commit_timestamp: (0, 22), durable_timestamp: (0, 22), first_commit_timestamp: (0, 21), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 21), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
session ID: 18, txn ID: 22, pinned ID: 16, metadata pinned ID: 0, name: connection-open-session
transaction id: 22, mod count: 3, snap min: 16, snap max: 22, snapshot count: 4, snapshot: [16, 17, 18, 20], commit_timestamp: (0, 14), durable_timestamp: (0, 14), first_commit_timestamp: (0, 13), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 13), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
session ID: 19, txn ID: 24, pinned ID: 17, metadata pinned ID: 0, name: connection-open-session
transaction id: 24, mod count: 3, snap min: 17, snap max: 24, snapshot count: 4, snapshot: [17, 18, 20, 22], commit_timestamp: (0, 16), durable_timestamp: (0, 16), first_commit_timestamp: (0, 15), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 15), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
session ID: 20, txn ID: 26, pinned ID: 18, metadata pinned ID: 0, name: connection-open-session
transaction id: 26, mod count: 3, snap min: 18, snap max: 26, snapshot count: 4, snapshot: [18, 20, 22, 24], commit_timestamp: (0, 18), durable_timestamp: (0, 18), first_commit_timestamp: (0, 17), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 17), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
session ID: 21, txn ID: 28, pinned ID: 20, metadata pinned ID: 0, name: connection-open-session
transaction id: 28, mod count: 3, snap min: 20, snap max: 28, snapshot count: 4, snapshot: [20, 22, 24, 26], commit_timestamp: (0, 20), durable_timestamp: (0, 20), first_commit_timestamp: (0, 19), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 19), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
checkpoint session ID: 22, txn ID: 29, pinned ID: 22, metadata pinned ID: 0, name: WT_SESSION.__session_checkpoint
transaction id: 29, mod count: 0, snap min: 22, snap max: 31, snapshot count: 5, snapshot: [22, 24, 26, 28, 30], commit_timestamp: (0, 0), durable_timestamp: (0, 0), first_commit_timestamp: (0, 0), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 0), read_timestamp: (0, 0), checkpoint LSN: [1][20992], full checkpoint: true, rollback reason: , flags: 0x0000100c, isolation: WT_ISO_SNAPSHOT


__wt_txn_update_oldest end:
transaction state dump
now print session ID: 6
current ID: 40
//µ±Ç°ËùÓÐtxn ID:ÖÐµÄ×îÐ¡Öµ
last running ID: 31
//µ±Ç°ËùÓÐpinned ID:ÖÐµÄ×îÐ¡Öµ
metadata_pinned ID: 22
//Ò²¾ÍÊÇµ±Ç°ËùÓÐtxn ID:ºÍpinned ID:ÖÐµÄ×îÐ¡Öµ
oldest ID: 22
durable timestamp: (0, 20)
oldest timestamp: (0, 0)
pinned timestamp: (0, 0)
stable timestamp: (0, 0)
has_durable_timestamp: yes
has_oldest_timestamp: no
has_pinned_timestamp: no
has_stable_timestamp: no
oldest_is_pinned: no
stable_is_pinned: no
checkpoint running: yes
checkpoint generation: 2
checkpoint pinned ID: 22
checkpoint txn ID: 29
session count: 22
Transaction state of active sessions:
session ID: 16, txn ID: 31, pinned ID: 22, metadata pinned ID: 29, name: connection-open-session
transaction id: 31, mod count: 3, snap min: 22, snap max: 31, snapshot count: 5, snapshot: [22, 24, 26, 28, 29], commit_timestamp: (0, 22), durable_timestamp: (0, 22), first_commit_timestamp: (0, 21), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 21), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
session ID: 18, txn ID: 33, pinned ID: 24, metadata pinned ID: 29, name: connection-open-session
transaction id: 33, mod count: 3, snap min: 24, snap max: 33, snapshot count: 5, snapshot: [24, 26, 28, 29, 31], commit_timestamp: (0, 24), durable_timestamp: (0, 24), first_commit_timestamp: (0, 23), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 23), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
session ID: 19, txn ID: 35, pinned ID: 26, metadata pinned ID: 29, name: connection-open-session
transaction id: 35, mod count: 3, snap min: 26, snap max: 35, snapshot count: 5, snapshot: [26, 28, 29, 31, 33], commit_timestamp: (0, 26), durable_timestamp: (0, 26), first_commit_timestamp: (0, 25), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 25), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
session ID: 20, txn ID: 37, pinned ID: 28, metadata pinned ID: 29, name: connection-open-session
transaction id: 37, mod count: 3, snap min: 28, snap max: 37, snapshot count: 5, snapshot: [28, 29, 31, 33, 35], commit_timestamp: (0, 28), durable_timestamp: (0, 28), first_commit_timestamp: (0, 27), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 27), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
session ID: 21, txn ID: 39, pinned ID: 31, metadata pinned ID: 29, name: connection-open-session
transaction id: 39, mod count: 3, snap min: 29, snap max: 39, snapshot count: 5, snapshot: [29, 31, 33, 35, 37], commit_timestamp: (0, 30), durable_timestamp: (0, 30), first_commit_timestamp: (0, 29), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 29), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x0000301c, isolation: WT_ISO_SNAPSHOT
checkpoint session ID: 22, txn ID: 29, pinned ID: 22, metadata pinned ID: 0, name: eviction-server
transaction id: 0, mod count: 0, snap min: 0, snap max: 0, snapshot count: 0, snapshot: [], commit_timestamp: (0, 0), durable_timestamp: (0, 0), first_commit_timestamp: (0, 0), prepare_timestamp: (0, 0), pinned_durable_timestamp: (0, 0), read_timestamp: (0, 0), checkpoint LSN: [0][0], full checkpoint: false, rollback reason: , flags: 0x00000000, isolation: WT_ISO_SNAPSHOT

*     Output diagnostic information about the global transaction state.
 */
int
__wt_verbose_dump_txn(WT_SESSION_IMPL *session, const char *func_name)
{
    WT_CONNECTION_IMPL *conn;
    WT_SESSION_IMPL *sess;
    WT_TXN_GLOBAL *txn_global;
    WT_TXN_SHARED *s;
    uint64_t id;
    uint32_t i, session_cnt;
    char ts_string[WT_TS_INT_STRING_SIZE];
    WT_DECL_ITEM(snapshot_buf);
   // WT_DECL_ITEM(txn_one_buf);
    WT_DECL_RET;

    conn = S2C(session);
    txn_global = &conn->txn_global;

    return 0;//yang add change

    WT_ERR(__wt_scr_alloc(session, 20480, &snapshot_buf));
    
    if (func_name)
        WT_ERR(__wt_buf_fmt(session, snapshot_buf, "\r\n\r\n%s:", func_name));
    else 
        WT_ERR(__wt_buf_fmt(session, snapshot_buf, "\r\n\r\n%s:", WT_DIVIDER));

    WT_ERR(__wt_buf_catfmt(session, snapshot_buf, "%s", "transaction state dump\r\n"));
    WT_ERR(__wt_buf_catfmt(session, snapshot_buf, "now print session ID: %" PRIu32 "\r\n", session->id));
    WT_ERR(__wt_buf_catfmt(session, snapshot_buf, "current ID: %" PRIu64 "\r\n", txn_global->current));
    //ÕâÈý¸öÓÃÓÚÈ«¾ÖÊÂÎñ¿É¼ûÐÔÅÐ¶Ï£¬ÀúÊ·°æ±¾ÄÚ´æÊÍ·ÅµÈ
    WT_ERR(__wt_buf_catfmt(session, snapshot_buf, "last running ID: %" PRIu64 "\r\n", txn_global->last_running));
    WT_ERR(__wt_buf_catfmt(session, snapshot_buf, "metadata_pinned ID: %" PRIu64 "\r\n", txn_global->metadata_pinned));
    WT_ERR(__wt_buf_catfmt(session, snapshot_buf, "oldest ID: %" PRIu64 "\r\n", txn_global->oldest_id));


/*
{"t":{"$date":"2024-08-09T18:17:59.960+08:00"},"s":"D1", "c":"WTTS",     "id":22430,   "ctx":"conn8342","msg":"WiredTiger message","attr":{"message":{"ts_sec":1723198679,"ts_usec":960240,"thread":"12597:0x7fc11a267700","session_name":"WT_CONNECTION.set_timestamp","category":"WT_VERB_TIMESTAMP","category_id":38,"verbose_level":"DEBUG_1","verbose_level_id":1,"msg":"Timestamp (1723198679, 839): Updated global stable timestamp"}}}
{"t":{"$date":"2024-08-09T18:17:59.960+08:00"},"s":"D1", "c":"WTTS",     "id":22430,   "ctx":"conn8342","msg":"WiredTiger message","attr":{"message":{"ts_sec":1723198679,"ts_usec":960278,"thread":"12597:0x7fc11a267700","session_name":"WT_CONNECTION.set_timestamp","category":"WT_VERB_TIMESTAMP","category_id":38,"verbose_level":"DEBUG_1","verbose_level_id":1,"msg":"Timestamp (1723198379, 839): Updated global oldest timestamp"}}}
{"t":{"$date":"2024-08-09T18:17:59.960+08:00"},"s":"D1", "c":"WTTS",     "id":22430,   "ctx":"conn8342","msg":"WiredTiger message","attr":{"message":{"ts_sec":1723198679,"ts_usec":960298,"thread":"12597:0x7fc11a267700","session_name":"WT_CONNECTION.set_timestamp","category":"WT_VERB_TIMESTAMP","category_id":38,"verbose_level":"DEBUG_1","verbose_level_id":1,"msg":"Timestamp (1723198379, 839): Updated pinned timestamp"}}}

mongo server
Timestamp calculatedOldestTimestamp(stableTimestamp.getSecs() -
                                        minSnapshotHistoryWindowInSeconds.load(),
                                    stableTimestamp.getInc());
*/
//Ïò¸±±¾¼¯ÊµÀý²»Í£µÄÐ´Êý¾Ý£¬»á²»Í£´òÓ¡¸üÐÂstable timestampºÍoldest timestamp£¬Á½¸ö²îÖµ¾ÍÊÇÄ¬ÈÏµÄminSnapshotHistoryWindowInSeconds=300Ãë

    //https://source.wiredtiger.com/develop/timestamp_global_api.html




    //Èç¹ûÃ»ÓÐÏÖÊµÉèÖÃdurable_timestamp£¬Ã¿´ÎÊÂÎñÌá½»µÄÊ±ºò¶¼»áÄ¬ÈÏºÍcommit_timestamp±£³ÖÒ»ÖÂ£¬¼û__wt_txn_commit(¸³ÖµÎªµ±Ç°ÊÂÎñÌá½»Ê±ºò×î´óµÄcommit_timestamp)
    WT_ERR(__wt_buf_catfmt(session, snapshot_buf, "durable timestamp: %s\r\n",
      __wt_timestamp_to_string(txn_global->durable_timestamp, ts_string)));
    //mongodb»áÉèÖÃoldest_timestamp=stable_timestamp-minSnapshotHistoryWindowInSeconds, ÕæÕýÉúÐ§Êµ¼ÊÉÏÊÇÔÚ
    //  __wt_txn_update_pinned_timestampÓ°Ïìpinned_timestamp, È»ºó¼ä½ÓµÄÓ°ÏìÊÂÎñÈ«¾Ö¿É¼ûÐÔ__wt_txn_visible_all->__wt_txn_pinned_timestamp
    //  £¬½ø¶øÓ°ÏìÀúÊ·°æ±¾Êý¾Ý´ÓÄÚ´æÊÍ·ÅÒÔ¼°·ÃÎÊ¿É¼ûÐÔ
    WT_ERR(__wt_buf_catfmt(session, snapshot_buf, "oldest timestamp: %s\r\n",
      __wt_timestamp_to_string(txn_global->oldest_timestamp, ts_string)));
    //pinned_timestampÊÇÔÚ__wt_txn_update_pinned_timestampÖÐ¸ù¾Ýoldest_timestamp¡¢checkpoint_timestampºÍËùÓÐÊÂÎñµÄread_timestampÈ¡×îÐ¡ÖµËã³öÀ´µÄ(×¢Òâ: __wt_txn_set_read_timestampÉèÖÃµÄÊ±ºò£¬ read_timestamp±ØÐë±Èoldest timestamp´ó£¬·ñÔòÉèÖÃ²»³É¹¦)£¬(mongodb txnOpen.setReadSnapshot(_readAtTimestamp);)
    //Êµ¼ÊÉÏÕæÊµÓÃ´¦ÈçÏÂ:
    //  1. Ö»»áÔÚ__wt_update_obsolete_checkÖÐµ±Ò»¸öKÉÏÃæµÄÀúÊ·°æ±¾³¬¹ý20¸öµÄÊ±ºò²Å»áÓ°Ïì¸ÃupdateµÄ¼ì²é
    //  2. __wt_txn_visible_all->__wt_txn_pinned_timestampÓÃpinned_timestampÀ´ÅÐ¶ÏÊÂÎñ¿É¼ûÐÔ£¬½ø¶øÓ°ÏìÀúÊ·°æ±¾Êý¾Ý´ÓÄÚ´æÊÍ·ÅÒÔ¼°·ÃÎÊ¿É¼ûÐÔ
    WT_ERR(__wt_buf_catfmt(session, snapshot_buf, "pinned timestamp: %s\r\n",
      __wt_timestamp_to_string(txn_global->pinned_timestamp, ts_string)));
    //checkpoint³Ö¾Ã»¯µÄÊ±ºòÓÃ¸ÃÊ±¼ä×öÎªcheckpointÊ±¼äÐ´Èëwiredtiger.wt£¬×îÖÕÄ¿µÄÊÇrollback_to_stable¿ìËÙ»Ø¹öµ½Õâ¸öÊ±¼äµã
    WT_ERR(__wt_buf_catfmt(session, snapshot_buf, "stable timestamp: %s\r\n",
      __wt_timestamp_to_string(txn_global->stable_timestamp, ts_string)));

      
    WT_ERR(__wt_buf_catfmt(session, snapshot_buf, "has_durable_timestamp: %s\r\n", txn_global->has_durable_timestamp ? "yes" : "no"));
    WT_ERR(__wt_buf_catfmt(session, snapshot_buf, "has_oldest_timestamp: %s\r\n", txn_global->has_oldest_timestamp ? "yes" : "no"));
    WT_ERR(__wt_buf_catfmt(session, snapshot_buf, "has_pinned_timestamp: %s\r\n", txn_global->has_pinned_timestamp ? "yes" : "no"));
    WT_ERR(__wt_buf_catfmt(session, snapshot_buf, "has_stable_timestamp: %s\r\n", txn_global->has_stable_timestamp ? "yes" : "no"));
    WT_ERR(__wt_buf_catfmt(session, snapshot_buf, "oldest_is_pinned: %s\r\n", txn_global->oldest_is_pinned ? "yes" : "no"));
    WT_ERR(__wt_buf_catfmt(session, snapshot_buf, "stable_is_pinned: %s\r\n", txn_global->stable_is_pinned ? "yes" : "no"));


    WT_ERR(__wt_buf_catfmt(session, snapshot_buf, "checkpoint running: %s\r\n", txn_global->checkpoint_running ? "yes" : "no"));
    WT_ERR(__wt_buf_catfmt(session, snapshot_buf, "checkpoint generation: %" PRIu64 "\r\n", __wt_gen(session, WT_GEN_CHECKPOINT)));
    WT_ERR(__wt_buf_catfmt(session, snapshot_buf, "checkpoint pinned ID: %" PRIu64 "\r\n", txn_global->checkpoint_txn_shared.pinned_id));
    WT_ERR(__wt_buf_catfmt(session, snapshot_buf,  "checkpoint timestamp: %s\r\n",
      __wt_timestamp_to_string(txn_global->checkpoint_timestamp, ts_string)));
    WT_ERR(__wt_buf_catfmt(session, snapshot_buf, "checkpoint txn ID: %" PRIu64 "\r\n", txn_global->checkpoint_txn_shared.id));
    WT_ORDERED_READ(session_cnt, conn->session_cnt);
    WT_ERR(__wt_buf_catfmt(session, snapshot_buf, "session count: %" PRIu32 "\r\n", session_cnt));
    WT_ERR(__wt_buf_catfmt(session, snapshot_buf, "Transaction state of active sessions:\r\n"));
    WT_ERR(__wt_buf_catfmt(session, snapshot_buf, "has_oldest_timestamp: %s\r\n", txn_global->has_oldest_timestamp ? "yes" : "no"));
    WT_ERR(__wt_buf_catfmt(session, snapshot_buf, "has_pinned_timestamp: %s\r\n", txn_global->has_pinned_timestamp ? "yes" : "no"));
    WT_ERR(__wt_buf_catfmt(session, snapshot_buf, "has_stable_timestamp: %s\r\n", txn_global->has_stable_timestamp ? "yes" : "no"));
    WT_ERR(__wt_buf_catfmt(session, snapshot_buf, "oldest_is_pinned: %s\r\n", txn_global->oldest_is_pinned ? "yes" : "no"));
    WT_ERR(__wt_buf_catfmt(session, snapshot_buf, "stable_is_pinned: %s\r\n", txn_global->stable_is_pinned ? "yes" : "no"));
    WT_RET(
      __wt_buf_catfmt(session, snapshot_buf, "checkpoint running: %s\r\n", txn_global->checkpoint_running ? "yes" : "no"));
    WT_RET(
      __wt_buf_catfmt(session, snapshot_buf, "checkpoint generation: %" PRIu64 "\r\n", __wt_gen(session, WT_GEN_CHECKPOINT)));
    WT_RET(__wt_buf_catfmt(
      session, snapshot_buf, "checkpoint pinned ID: %" PRIu64 "\r\n", txn_global->checkpoint_txn_shared.pinned_id));
    WT_RET(__wt_buf_catfmt(session, snapshot_buf, "checkpoint timestamp: %s\r\n",
      __wt_timestamp_to_string(txn_global->checkpoint_timestamp, ts_string)));
    WT_RET(__wt_buf_catfmt(session, snapshot_buf, "checkpoint txn ID: %" PRIu64 "\r\n", txn_global->checkpoint_txn_shared.id));
    WT_ORDERED_READ(session_cnt, conn->session_cnt);
    WT_RET(__wt_buf_catfmt(session, snapshot_buf, "session count: %" PRIu32 "\r\n", session_cnt));
    WT_RET(__wt_buf_catfmt(session, snapshot_buf, "Transaction state of active sessions:"));

    /*
     * Walk each session transaction state and dump information. Accessing the content of session
     * handles is not thread safe, so some information may change while traversing if other threads
     * are active at the same time, which is OK since this is diagnostic code.
     */
    WT_STAT_CONN_INCR(session, txn_walk_sessions);
    for (i = 0, s = txn_global->txn_shared_list; i < session_cnt; i++, s++) {
        WT_STAT_CONN_INCR(session, txn_sessions_walked);
        /* Skip sessions with no active transaction */
        //ÏÂÃæµÄÄÚÈÝÖ»ÓÐÔÚ¶ÔÓ¦sessionÊÂÎñµ÷ÓÃ__wt_txn_id_alloc»ñÈ¡µ½ÊÂÎñid»òÕßs->pinned_id²¿Î»0ºó²Å»áÊä³ö

        //Ö»´òÓ¡ÓÐÊÂÎñidµÄsessionÐÅÏ¢£¬Ò²¾ÍÊÇÕâ¸ösessionµ±Ç°»¹ÔÚÊÂÎñ½øÐÐÖÐ£¬µ±Ò»¸ösession commitÌá½»ÊÂÎñºó£¬
        //  Õâ¸ösessionµÄÊÂÎñid»áÖÃÎªWT_TXN_NONE£¬ËùÒÔ¾Í²»»áÓÐºóÃæµÄ´òÓ¡
        if ((id = s->id) == WT_TXN_NONE && s->pinned_id == WT_TXN_NONE)
            continue;
            
        //if (!F_ISSET(session->txn, WT_TXN_HAS_SNAPSHOT)) //yang add change   yang add todo xxxxxxxxxxxxxxxx
        //    continue;

        sess = &conn->sessions[i];
        WT_RET(__wt_buf_catfmt(session, snapshot_buf,
          "session ID: %" PRIu32 ", txn ID: %" PRIu64 ", pinned ID: %" PRIu64 ", metadata pinned ID: %" PRIu64 ", name: %s\r\n", 
          i, id, s->pinned_id, s->metadata_pinned, sess->name == NULL ? "EMPTY" : sess->name));
        WT_RET(__wt_verbose_dump_txn_one(session, sess, 0, NULL, snapshot_buf));
    }

    s = &txn_global->checkpoint_txn_shared;
    if ((id = s->id) == WT_TXN_NONE && s->pinned_id == WT_TXN_NONE) {
        goto end;
    }
    //yang add todo xxxxxxxxxxxxxxxxxxxxxxÏÂÒ»ÆÚPR¼ÓÉÏ
    sess = session;
    WT_RET(__wt_buf_catfmt(session, snapshot_buf, 
      "checkpoint session ID: %" PRIu32 ", txn ID: %" PRIu64 ", pinned ID: %" PRIu64 ", metadata pinned ID: %" PRIu64 ", name: %s\r\n", i, id,
      s->pinned_id, s->metadata_pinned, sess->name == NULL ? "EMPTY" : sess->name));
    WT_RET(__wt_verbose_dump_txn_one(session, sess, 0, NULL, snapshot_buf));

end:
    WT_ERR(__wt_msg(session, "%s", (char*)snapshot_buf->data));
    
err:
    __wt_scr_free(session, &snapshot_buf);

    return (0);
}
