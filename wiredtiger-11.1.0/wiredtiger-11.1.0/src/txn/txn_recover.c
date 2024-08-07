/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

/* Enable all recovery-related verbose messaging events. */
#define WT_VERB_RECOVERY_ALL        \
    WT_DECL_VERBOSE_MULTI_CATEGORY( \
      ((WT_VERBOSE_CATEGORY[]){WT_VERB_RECOVERY, WT_VERB_RECOVERY_PROGRESS}))

/* State maintained during recovery. */
typedef struct {
    //赋值见__recovery_setup_file
    const char *uri; /* File URI. */
    //访问该文件的cursor，赋值见__recovery_cursor
    WT_CURSOR *c;    /* Cursor used for recovery. */
    //也就是checkpoint_lsn
    WT_LSN ckpt_lsn; /* File's checkpoint LSN. */
} WT_RECOVERY_FILE;

//成员内容解析来自于__recovery_setup_file
typedef struct {
    WT_SESSION_IMPL *session;

    /* Files from the metadata, indexed by file ID. */
    //每个文件对应files[]数组的一个成员，例如files[0]对应wiredtiger.wt文件， files[1]对应file:WiredTigerHS.wt文件， 还有其他.wt文件以此类推
    WT_RECOVERY_FILE *files;
    size_t file_alloc; /* Allocated size of files array. */
    u_int max_fileid;  /* Maximum file ID seen. */
    //所有wt文件总数
    u_int nfiles;      /* Number of files in the metadata. */

    //__txn_log_recover中解析checkpoint信息
    WT_LSN ckpt_lsn;     /* Start LSN for main recovery loop. */
    WT_LSN max_ckpt_lsn; /* Maximum checkpoint LSN seen. */
    //__txn_log_recover中赋值
    WT_LSN max_rec_lsn;  /* Maximum recovery LSN seen. */

    bool missing;       /* Were there missing files? */
    //也就是0号文件,wiredtiger.wt,代表当前正在apply应用wal到wiredtiger.wt
    //赋值见__wt_txn_recover
    bool metadata_only; /*
                         * Set during the first recovery pass,
                         * when only the metadata is recovered.
                         */
} WT_RECOVERY;

/*
 * __recovery_cursor --
 *     Get a cursor for a recovery operation.
 //如果lsnp >= r->files[id].ckpt_lsn， 获取id对应wt文件，也就是r->files[id].uri文件对应的cursor
 //如果lsnp < r->files[id].ckpt_lsn，则cp返回null，该lsnp对应log可以忽略
 */
static int
__recovery_cursor(
  WT_SESSION_IMPL *session, WT_RECOVERY *r, WT_LSN *lsnp, u_int id, bool duplicate, WT_CURSOR **cp)
{
    WT_CURSOR *c;
    const char *cfg[] = {WT_CONFIG_BASE(session, WT_SESSION_open_cursor), "overwrite", NULL};
    bool metadata_op;

    c = NULL;

    /*
     * File ids with the bit set to ignore this operation are skipped.
     */
    //说明没有启用log功能
    if (WT_LOGOP_IS_IGNORED(id))
        return (0);
    /*
     * Metadata operations have an id of 0. Match operations based on the id and the current pass of
     * recovery for metadata.
     *
     * Only apply operations in the correct metadata phase, and if the LSN is more recent than the
     * last checkpoint. If there is no entry for a file, assume it was dropped or missing after a
     * hot backup.
     */
    metadata_op = id == WT_METAFILE_ID;
    //r->metadata_only的作用是:
    //  1. 如果id为0号文件并且metadata_only标识为true(apply到0号wiredtiger.wt文件)，则apply日志到wiredtiger.wt文件
    //  2. 如果id不为0号文件并且metadata_only标识为false(apply到0号wiredtiger.wt以外的文件)，则apply日志到wiredtiger.wt文件
    if (r->metadata_only != metadata_op) //wiredtiger.wt文件，也就是0号文件
        ;
    else if (id >= r->nfiles || r->files[id].uri == NULL) {//文件没找到，则标识missing
        /* If a file is missing, output a verbose message once. */
        if (!r->missing)
            __wt_verbose(
              session, WT_VERB_RECOVERY, "No file found with ID %u (max %u)", id, r->nfiles);
        r->missing = true;
    } else if (__wt_log_cmp(lsnp, &r->files[id].ckpt_lsn) >= 0) {
        /*
         * We're going to apply the operation. Get the cursor, opening one if none is cached.
         */
        //id对应wt文件的访问cursor
        if ((c = r->files[id].c) == NULL) {
            WT_RET(__wt_open_cursor(session, r->files[id].uri, NULL, cfg, &c));
            r->files[id].c = c;
        }
#ifndef WT_STANDALONE_BUILD
        /*
         * In the event of a clean shutdown, there shouldn't be any other table log records other
         * than metadata.
         */
        if (!metadata_op)
            S2C(session)->unclean_shutdown = true;
#endif
    }

    //判断是否需要再次打开获取cursor
    if (duplicate && c != NULL)
        WT_RET(__wt_open_cursor(session, r->files[id].uri, NULL, cfg, &c));

    *cp = c;
    return (0);
}

/*
 * Helper to a cursor if this operation is to be applied during recovery.
//yang add todo  Skipping op 4 to file 2147483650 at LSN 1/19840  2147483650对应十六进制80000002，首位为1代表debug_mode=(table_logging=true
输出op可以转换为字符串，fileid可以通过外层的WT_RECOVERY r->WT_RECOVERY_FILE转换为uri，这样日志会更加直观，方便理解
cp其实就i是cursor，这里最好用cursor替代cp,或者cursor用cp替代，这里统一一下更好理解代码
break需要空两格
 
 //跳过不属于本wt文件的op: Skipping op 4 to file 5 at LSN 1/19840
 //回放属于本wt文件的op: Applying op 4 to file 0 at LSN 1/24064
 */
//打印表名该wal是需要跳过还是需要回放  //获取fileid对应wt文件的访问cursor     
#define GET_RECOVERY_CURSOR(session, r, lsnp, fileid, cp)                            \
    ret = __recovery_cursor(session, r, lsnp, fileid, false, cp);                    \
    __wt_verbose_debug2(session, WT_VERB_RECOVERY,                                   \
      "%s op %" PRIu32 " to file %" PRIu32 " at LSN %" PRIu32 "/%" PRIu32,           \
      ret != 0 ? "Error" : cursor == NULL ? "Skipping" : "Applying", optype, fileid, \
      (lsnp)->l.file, (lsnp)->l.offset);                                             \
    WT_ERR(ret);                                                                     \
    if (cursor == NULL)                                                              \
    break

/*
 * __txn_op_apply --
 *     Apply a transactional operation during recovery.
 //回放一条wal日志
 */
static int
__txn_op_apply(WT_RECOVERY *r, WT_LSN *lsnp, const uint8_t **pp, const uint8_t *end)
{
    WT_CURSOR *cursor, *start, *stop;
    WT_DECL_RET;
    WT_ITEM key, start_key, stop_key, value;
    WT_SESSION_IMPL *session;
    wt_timestamp_t commit, durable, first_commit, prepare, read;
    uint64_t recno, start_recno, stop_recno, t_nsec, t_sec;
    uint32_t fileid, mode, optype, opsize;

    session = r->session;
    cursor = NULL;
    __wt_verbose_debug2(session, WT_VERB_RECOVERY,                                   
      "__txn_op_apply apply at LSN %" PRIu32 "/%" PRIu32, 
      (lsnp)->l.file, (lsnp)->l.offset);   

    /* Peek at the size and the type. */
    WT_ERR(__wt_logop_read(session, pp, end, &optype, &opsize));
    end = *pp + opsize;

    /*
     * If it is an operation type that should be ignored, we're done. Note that file ids within
     * known operations also use the same macros to indicate that operation should be ignored.
     */
    if (WT_LOGOP_IS_IGNORED(optype)) {
        *pp += opsize;
        goto done;
    }

    switch (optype) {
    case WT_LOGOP_COL_MODIFY:
        WT_ERR(__wt_logop_col_modify_unpack(session, pp, end, &fileid, &recno, &value));
        GET_RECOVERY_CURSOR(session, r, lsnp, fileid, &cursor);
        cursor->set_key(cursor, recno);
        if ((ret = cursor->search(cursor)) != 0)
            WT_ERR_NOTFOUND_OK(ret, false);
        else {
            /*
             * Build/insert a complete value during recovery rather than using cursor modify to
             * create a partial update (for no particular reason than simplicity).
             */
            WT_ERR(__wt_modify_apply_item(
              CUR2S(cursor), cursor->value_format, &cursor->value, value.data));
            WT_ERR(cursor->insert(cursor));
        }
        break;

    case WT_LOGOP_COL_PUT:
        WT_ERR(__wt_logop_col_put_unpack(session, pp, end, &fileid, &recno, &value));
        GET_RECOVERY_CURSOR(session, r, lsnp, fileid, &cursor);
        cursor->set_key(cursor, recno);
        __wt_cursor_set_raw_value(cursor, &value);
        WT_ERR(cursor->insert(cursor));
        break;

    case WT_LOGOP_COL_REMOVE:
        WT_ERR(__wt_logop_col_remove_unpack(session, pp, end, &fileid, &recno));
        GET_RECOVERY_CURSOR(session, r, lsnp, fileid, &cursor);
        cursor->set_key(cursor, recno);
        /*
         * WT_NOTFOUND is an expected error because the checkpoint snapshot we're rolling forward
         * may race with a remove, resulting in the key not being in the tree, but recovery still
         * processing the log record of the remove.
         */
        WT_ERR_NOTFOUND_OK(cursor->remove(cursor), false);
        break;

    case WT_LOGOP_COL_TRUNCATE:
        WT_ERR(
          __wt_logop_col_truncate_unpack(session, pp, end, &fileid, &start_recno, &stop_recno));
        GET_RECOVERY_CURSOR(session, r, lsnp, fileid, &cursor);

        /* Set up the cursors. */
        start = stop = NULL;
        if (start_recno != WT_RECNO_OOB)
            start = cursor;

        if (stop_recno != WT_RECNO_OOB) {
            if (start != NULL)
                WT_ERR(__recovery_cursor(session, r, lsnp, fileid, true, &stop));
            else
                stop = cursor;
        }

        /* Set the keys. */
        if (start != NULL)
            start->set_key(start, start_recno);
        if (stop != NULL)
            stop->set_key(stop, stop_recno);

        /*
         * If the truncate log doesn't have a recorded start and stop recno, truncate the whole file
         * using the URI. Otherwise use the positioned start or stop cursors to truncate a range of
         * the file.
         */
        if (start == NULL && stop == NULL)
            WT_TRET(
              session->iface.truncate(&session->iface, r->files[fileid].uri, NULL, NULL, NULL));
        else
            WT_TRET(session->iface.truncate(&session->iface, NULL, start, stop, NULL));

        /* If we opened a duplicate cursor, close it now. */
        if (stop != NULL && stop != cursor)
            WT_TRET(stop->close(stop));
        WT_ERR(ret);
        break;

    case WT_LOGOP_ROW_MODIFY:  //10
        WT_ERR(__wt_logop_row_modify_unpack(session, pp, end, &fileid, &key, &value));
        GET_RECOVERY_CURSOR(session, r, lsnp, fileid, &cursor);
        __wt_cursor_set_raw_key(cursor, &key);
        if ((ret = cursor->search(cursor)) != 0)
            WT_ERR_NOTFOUND_OK(ret, false);
        else {
            /*
             * Build/insert a complete value during recovery rather than using cursor modify to
             * create a partial update (for no particular reason than simplicity).
             */
            WT_ERR(__wt_modify_apply_item(
              CUR2S(cursor), cursor->value_format, &cursor->value, value.data));
            WT_ERR(cursor->insert(cursor));
        }
        break;

    case WT_LOGOP_ROW_PUT:  //4
        WT_ERR(__wt_logop_row_put_unpack(session, pp, end, &fileid, &key, &value));
        GET_RECOVERY_CURSOR(session, r, lsnp, fileid, &cursor);
        __wt_cursor_set_raw_key(cursor, &key);
        __wt_cursor_set_raw_value(cursor, &value);
        WT_ERR(cursor->insert(cursor));
        break;

    case WT_LOGOP_ROW_REMOVE:  //5
        WT_ERR(__wt_logop_row_remove_unpack(session, pp, end, &fileid, &key));
        GET_RECOVERY_CURSOR(session, r, lsnp, fileid, &cursor);
        __wt_cursor_set_raw_key(cursor, &key);
        /*
         * WT_NOTFOUND is an expected error because the checkpoint snapshot we're rolling forward
         * may race with a remove, resulting in the key not being in the tree, but recovery still
         * processing the log record of the remove.
         */
        WT_ERR_NOTFOUND_OK(cursor->remove(cursor), false);
        break;

    case WT_LOGOP_ROW_TRUNCATE: //6
        WT_ERR(
          __wt_logop_row_truncate_unpack(session, pp, end, &fileid, &start_key, &stop_key, &mode));
        GET_RECOVERY_CURSOR(session, r, lsnp, fileid, &cursor);
        /* Set up the cursors. */
        start = stop = NULL;
        switch (mode) {
        case WT_TXN_TRUNC_ALL:
            /* Both cursors stay NULL. */
            break;
        case WT_TXN_TRUNC_BOTH:
            start = cursor;
            WT_ERR(__recovery_cursor(session, r, lsnp, fileid, true, &stop));
            break;
        case WT_TXN_TRUNC_START:
            start = cursor;
            break;
        case WT_TXN_TRUNC_STOP:
            stop = cursor;
            break;
        default:
            WT_ERR(__wt_illegal_value(session, mode));
        }

        /* Set the keys. */
        if (start != NULL)
            __wt_cursor_set_raw_key(start, &start_key);
        if (stop != NULL)
            __wt_cursor_set_raw_key(stop, &stop_key);

        /*
         * If the truncate log doesn't have a recorded start and stop key, truncate the whole file
         * using the URI. Otherwise use the positioned start or stop cursors to truncate a range of
         * the file.
         */
        if (start == NULL && stop == NULL)
            WT_TRET(
              session->iface.truncate(&session->iface, r->files[fileid].uri, NULL, NULL, NULL));
        else
            WT_TRET(session->iface.truncate(&session->iface, NULL, start, stop, NULL));

        /* If we opened a duplicate cursor, close it now. */
        if (stop != NULL && stop != cursor)
            WT_TRET(stop->close(stop));
        WT_ERR(ret);
        break;
    case WT_LOGOP_TXN_TIMESTAMP:
        /*
         * Timestamp records are informational only. We have to unpack it to properly move forward
         * in the log record to the next operation, but otherwise ignore.
         */
        WT_ERR(__wt_logop_txn_timestamp_unpack(
          session, pp, end, &t_sec, &t_nsec, &commit, &durable, &first_commit, &prepare, &read));
        break;
    default:
        WT_ERR(__wt_illegal_value(session, optype));
    }

done:
    /* Reset the cursor so it doesn't block eviction. */
    if (cursor != NULL)
        WT_ERR(cursor->reset(cursor));
    return (0);

err:
    __wt_err(session, ret,
      "operation apply failed during recovery: operation type %" PRIu32 " at LSN %" PRIu32
      "/%" PRIu32,
      optype, lsnp->l.file, lsnp->l.offset);
    return (ret);
}

/*
 * __txn_commit_apply --
 *     Apply a commit record during recovery.
 把写入WiredTigerLog.xxxx文件中的一条wal日志回放到B+tree中
 只会回放WT_LOGREC_COMMIT类型的wal日志
 */
static int
__txn_commit_apply(WT_RECOVERY *r, WT_LSN *lsnp, const uint8_t **pp, const uint8_t *end)
{
    /* The logging subsystem zero-pads records. */
    //有时候一个commit中包含多个表的数据写操作，就通过这里的while一条一条回放
    while (*pp < end && **pp)
        WT_RET(__txn_op_apply(r, lsnp, pp, end));

    return (0);
}

/*
 * __txn_log_recover --
 *     Roll the log forward to recover committed changes.
 //回放wal日志来恢复数据
 */
static int
__txn_log_recover(WT_SESSION_IMPL *session, WT_ITEM *logrec, WT_LSN *lsnp, WT_LSN *next_lsnp,
  void *cookie, int firstrecord)
{
    WT_DECL_RET;
    WT_RECOVERY *r;
    uint64_t txnid_unused;
    uint32_t rectype;
    const uint8_t *end, *p;

    r = cookie;
    p = WT_LOG_SKIP_HEADER(logrec->data);
    end = (const uint8_t *)logrec->data + logrec->size;
    WT_UNUSED(firstrecord);

    /* First, peek at the log record type. */
    WT_RET(__wt_logrec_read(session, &p, end, &rectype));

    /*
     * Record the highest LSN we process during the metadata phase. If not the metadata phase, then
     * stop at that LSN.
     */
    if (r->metadata_only)
        WT_ASSIGN_LSN(&r->max_rec_lsn, next_lsnp);
    else if (__wt_log_cmp(lsnp, &r->max_rec_lsn) >= 0)
        return (0);

    switch (rectype) {
    case WT_LOGREC_CHECKPOINT:
        if (r->metadata_only)
            //获取日志中的checkpoint信息
            WT_RET(__wt_txn_checkpoint_logread(session, &p, end, &r->ckpt_lsn));
        break;

    case WT_LOGREC_COMMIT:
        if ((ret = __wt_vunpack_uint(&p, WT_PTRDIFF(end, p), &txnid_unused)) != 0)
            WT_RET_MSG(session, ret, "txn_log_recover: unpack failure");
        // 把写入WiredTigerLog.xxxx文件中的一条wal日志回放到B+tree中
        WT_RET(__txn_commit_apply(r, lsnp, &p, end));
        break;
    }

    return (0);
}

/*
 * __recovery_set_checkpoint_timestamp --
 *     Set the checkpoint timestamp as retrieved from the metadata file.
 */
static int
__recovery_set_checkpoint_timestamp(WT_RECOVERY *r)
{
    WT_CONNECTION_IMPL *conn;
    WT_SESSION_IMPL *session;
    wt_timestamp_t ckpt_timestamp;
    char ts_string[WT_TS_INT_STRING_SIZE];

    session = r->session;
    conn = S2C(session);

    /*
     * Read the system checkpoint information from the metadata file and save the stable timestamp
     * of the last checkpoint for later query. This gets saved in the connection.
     */
/*
system:checkpoint\00
checkpoint_timestamp="3b9aca04",checkpoint_time=1721294244,write_gen=13\00
获取checkpoint_timestamp存入ckpt_timestamp
*/
    WT_RET(__wt_meta_read_checkpoint_timestamp(r->session, NULL, &ckpt_timestamp, NULL));

    /*
     * Set the recovery checkpoint timestamp and the metadata checkpoint timestamp so that the
     * checkpoint after recovery writes the correct value into the metadata.
     */
    conn->txn_global.meta_ckpt_timestamp = conn->txn_global.recovery_timestamp = ckpt_timestamp;

    //WT_VERB_RECOVERY_ALL.categories[__v_idx]
    //printf("yang test .......__recovery_set_checkpoint_timestamp............\r\n");
    __wt_verbose_multi(session, WT_VERB_RECOVERY_ALL, "Set global recovery timestamp: %s",
      __wt_timestamp_to_string(conn->txn_global.recovery_timestamp, ts_string));

    return (0);
}

/*
 * __recovery_set_oldest_timestamp --
 *     Set the oldest timestamp as retrieved from the metadata file. Setting the oldest timestamp
 *     doesn't automatically set the pinned timestamp.
 */
static int
__recovery_set_oldest_timestamp(WT_RECOVERY *r)
{
    WT_CONNECTION_IMPL *conn;
    WT_SESSION_IMPL *session;
    wt_timestamp_t oldest_timestamp;
    char ts_string[WT_TS_INT_STRING_SIZE];

    session = r->session;
    conn = S2C(session);
    /*
     * Read the system checkpoint information from the metadata file and save the oldest timestamp
     * of the last checkpoint for later query. This gets saved in the connection.
     */
    WT_RET(__wt_meta_read_checkpoint_oldest(r->session, NULL, &oldest_timestamp, NULL));
    conn->txn_global.oldest_timestamp = oldest_timestamp;
    conn->txn_global.has_oldest_timestamp = oldest_timestamp != WT_TS_NONE;

    __wt_verbose_multi(session, WT_VERB_RECOVERY_ALL, "Set global oldest timestamp: %s",
      __wt_timestamp_to_string(conn->txn_global.oldest_timestamp, ts_string));

    return (0);
}

/*
 * __recovery_set_checkpoint_snapshot --
 *     Set the checkpoint snapshot details as retrieved from the metadata file.

system:checkpoint_snapshot\00
snapshot_min=1,snapshot_max=1,snapshot_count=0,checkpoint_time=1721461898,write_gen=16\00
 */
static int
__recovery_set_checkpoint_snapshot(WT_SESSION_IMPL *session)
{
    WT_CONNECTION_IMPL *conn;

    conn = S2C(session);

    /*
     * WiredTiger versions 10.0.1 onward have a valid checkpoint snapshot on-disk. There was a bug
     * in some versions of WiredTiger that are tagged with the 10.0.0 release, which saved the wrong
     * checkpoint snapshot (see WT-8395), so we ignore the snapshot when it was created with one of
     * those versions. Versions of WiredTiger prior to 10.0.0 never saved a checkpoint snapshot.
     * Additionally the turtle file doesn't always exist (for example, backup doesn't include the
     * turtle file), so there isn't always a WiredTiger version available. If there is no version
     * available, assume that the snapshot is valid, otherwise restoring from a backup won't work.
     */
    if (__wt_version_defined(conn->recovery_version) &&
      __wt_version_lte(conn->recovery_version, (WT_VERSION){10, 0, 0})) {
        /* Return an empty snapshot. */
        conn->recovery_ckpt_snap_min = WT_TXN_NONE;
        conn->recovery_ckpt_snap_max = WT_TXN_NONE;
        conn->recovery_ckpt_snapshot = NULL;
        conn->recovery_ckpt_snapshot_count = 0;
        return (0);
    }

    /*
     * Read the system checkpoint information from the metadata file and save the snapshot related
     * details of the last checkpoint in the connection for later query.
     */
    return (__wt_meta_read_checkpoint_snapshot(session, NULL, NULL, &conn->recovery_ckpt_snap_min,
      &conn->recovery_ckpt_snap_max, &conn->recovery_ckpt_snapshot,
      &conn->recovery_ckpt_snapshot_count, NULL));
}

/*
 * __recovery_set_ckpt_base_write_gen --
 *     Set the base write gen as retrieved from the metadata file.
 //也就是获取WiredTiger.wt文件中的system:checkpoint_base_write_gen
 */
static int
__recovery_set_ckpt_base_write_gen(WT_RECOVERY *r)
{
    WT_CONFIG_ITEM cval;
    WT_DECL_RET;
    WT_SESSION_IMPL *session;
    char *sys_config;

    sys_config = NULL;
    session = r->session;

    //也就是获取WiredTiger.wt文件中的system:checkpoint_base_write_gen
    /* Search the metadata for checkpoint base write gen information. */
    WT_ERR_NOTFOUND_OK(
      __wt_metadata_search(session, WT_SYSTEM_BASE_WRITE_GEN_URI, &sys_config), false);
    if (sys_config != NULL) {
        WT_CLEAR(cval);
        WT_ERR(__wt_config_getones(session, sys_config, WT_SYSTEM_BASE_WRITE_GEN, &cval));
        if (cval.len != 0)
            S2C(session)->last_ckpt_base_write_gen = (uint64_t)cval.val;
    }

err:
    __wt_free(session, sys_config);
    return (ret);
}

/*
 * __recovery_txn_setup_initial_state --
 *     Setup the transaction initial state required by rollback to stable.
 //从wiredtiger.wt文件中获取checkpoint_timestamp   oldest_timestamp信息
 */
static int
__recovery_txn_setup_initial_state(WT_SESSION_IMPL *session, WT_RECOVERY *r)
{
    WT_CONNECTION_IMPL *conn;

    conn = S2C(session);
    //system:checkpoint_snapshot\00
    //snapshot_min=1,snapshot_max=1,snapshot_count=0,checkpoint_time=1721461898,write_gen=16\00
    WT_RET(__recovery_set_checkpoint_snapshot(session));

    /*
     * Set the checkpoint timestamp and oldest timestamp retrieved from the checkpoint metadata.
     * These are the stable timestamp and oldest timestamps of the last successful checkpoint.
     */

    //__txn_checkpoint->__wt_meta_sysinfo_set中把checkpoint_timestamp和oldest_timestamp写入wiredtiger.wt
    //__recovery_txn_setup_initial_state进行recover的时候从wiredtiger.wt中恢复
     
    //从wiredtiger.wt文件中获取checkpoint_timestamp信息
    WT_RET(__recovery_set_checkpoint_timestamp(r));
    //从wiredtiger.wt文件中获取oldest_timestamp信息
    WT_RET(__recovery_set_oldest_timestamp(r));

    /*
     * Now that timestamps extracted from the checkpoint metadata have been configured, configure
     * the pinned timestamp.
     */
    //设置checkpoint_timestamp给txn_global->pinned_timestamp
    __wt_txn_update_pinned_timestamp(session, true);

    WT_ASSERT(session,
      conn->txn_global.has_stable_timestamp == false &&
        conn->txn_global.stable_timestamp == WT_TS_NONE);

    /* Set the stable timestamp from recovery timestamp. */
    //注意stable_timestamp这里赋值，不是在__wt_txn_update_pinned_timestamp，因此
    //txn_global->stable_is_pinned = txn_global->pinned_timestamp == txn_global->stable_timestamp不满足条件
    //???????????? yang add todo xxxxxxxxxxxxx 这里要不要设置txn_global->stable_is_pinned = True，因为这里两个已经相等了
    conn->txn_global.stable_timestamp = conn->txn_global.recovery_timestamp;
    if (conn->txn_global.stable_timestamp != WT_TS_NONE)
        conn->txn_global.has_stable_timestamp = true;

    return (0);
}

/*
 * __recovery_setup_file --
 *     Set up the recovery slot for a file, track the largest file ID, and update the base write gen
 *     based on the file's configuration.

access_pattern_hint=none,allocation_size=4KB,app_metadata=,assert=(commit_timestamp=none,durable_timestamp=none,
read_timestamp=none,write_timestamp=off),block_allocation=best,block_compressor=,cache_resident=false,checksum=on,
collator=,columns=,dictionary=0,encryption=(keyid=,name=),format=btree,huffman_key=,huffman_value=,id=0,
ignore_in_memory_cache_size=false,internal_item_max=0,internal_key_max=0,internal_key_truncate=true,internal_page_max=4KB,
key_format=S,key_gap=10,leaf_item_max=0,leaf_key_max=0,leaf_page_max=32KB,leaf_value_max=0,log=(enabled=true),
memory_page_image_max=0,memory_page_max=5MB,os_cache_dirty_max=0,os_cache_max=0,prefix_compression=false,prefix_compression_min=4,
readonly=false,split_deepen_min_child=0,split_deepen_per_child=0,split_pct=90,tiered_object=false,tiered_storage=(auth_token=,
bucket=,bucket_prefix=,cache_directory=,local_retention=300,name=,object_target_size=0),value_format=S,verbose=[],
version=(major=2,minor=1),write_timestamp_usage=none,checkpoint=(WiredTigerCheckpoint.8=(addr="018681e4ccf56d788781e49e36b6498881e47052b90c808080e27fc0e20fc0",
order=8,time=1720158473,size=16384,newest_start_durable_ts=0,oldest_start_ts=0,newest_txn=11,newest_stop_durable_ts=0,
newest_stop_ts=-1,newest_stop_txn=-11,prepare=0,write_gen=23,run_write_gen=19)),checkpoint_backup_info=,checkpoint_lsn=(2,2048)


__recovery_file_scan_prefix:  wiredtiger.wt元数据文件的恢复
 __wt_txn_recover: 其他.wt文件数据的恢复
 
 //根据config配置信息解析对应自动内容赋值给WT_RECOVERY结构
 */
static int
__recovery_setup_file(WT_RECOVERY *r, const char *uri, const char *config)
{
    WT_CONFIG_ITEM cval;
    WT_DECL_RET;
    WT_LSN lsn;
    uint32_t fileid, lsnfile, lsnoffset;

    //获取id配置，也就是文件号，每个文件都有一个文件号,wiredtiger.wt文件号为0
    WT_RET(__wt_config_getones(r->session, config, "id", &cval));
    fileid = (uint32_t)cval.val;
    
    /* Track the largest file ID we have seen. */
    if (fileid > r->max_fileid)
        r->max_fileid = fileid;

    if (r->nfiles <= fileid) {
        WT_RET(__wt_realloc_def(r->session, &r->file_alloc, fileid + 1, &r->files));
        r->nfiles = fileid + 1;
    }

    //wt文件不应该有重名
    if (r->files[fileid].uri != NULL)
        WT_RET_PANIC(r->session, WT_PANIC,
          "metadata corruption: files %s and %s have the same file ID %u", uri,
          r->files[fileid].uri, fileid);
    WT_RET(__wt_strdup(r->session, uri, &r->files[fileid].uri));
    if ((ret = __wt_config_getones(r->session, config, "checkpoint_lsn", &cval)) != 0)
        WT_RET_MSG(
          r->session, ret, "Failed recovery setup for %s: cannot parse config '%s'", uri, config);
    /* If there is no checkpoint logged for the file, apply everything. */
    if (cval.type != WT_CONFIG_ITEM_STRUCT)
        WT_INIT_LSN(&lsn);
        
    /* NOLINTNEXTLINE(cert-err34-c) */
    //获取checkpoint_lsn的配置信息
    else if (sscanf(cval.str, "(%" SCNu32 ",%" SCNu32 ")", &lsnfile, &lsnoffset) == 2)
        WT_SET_LSN(&lsn, lsnfile, lsnoffset);
    else
        WT_RET_MSG(r->session, EINVAL,
          "Failed recovery setup for %s: cannot parse checkpoint LSN '%.*s'", uri, (int)cval.len,
          cval.str);
    WT_ASSIGN_LSN(&r->files[fileid].ckpt_lsn, &lsn);

    //从这个checkpoint_lsn日志处开始恢复日志
    __wt_verbose(r->session, WT_VERB_RECOVERY,
      "Recovering %s with id %" PRIu32 " @ (%" PRIu32 ", %" PRIu32 ")", uri, fileid, lsn.l.file,
      lsn.l.offset);

    //记录所有.wt文件的最大checkpoint_lsn
    if ((!WT_IS_MAX_LSN(&lsn) && !WT_IS_INIT_LSN(&lsn)) &&
      (WT_IS_MAX_LSN(&r->max_ckpt_lsn) || __wt_log_cmp(&lsn, &r->max_ckpt_lsn) > 0))
        WT_ASSIGN_LSN(&r->max_ckpt_lsn, &lsn);

    /* Update the base write gen and most recent checkpoint based on this file's configuration. */
    if ((ret = __wt_metadata_update_connection(r->session, config)) != 0)
        WT_RET_MSG(r->session, ret, "Failed recovery setup for %s: cannot update write gen", uri);
    return (0);
}

/*
 * __recovery_close_cursors --
 *     Close the logging recovery cursors.
 */
static int
__recovery_close_cursors(WT_RECOVERY *r)
{
    WT_CURSOR *c;
    WT_DECL_RET;
    WT_SESSION_IMPL *session;
    u_int i;

    session = r->session;
    for (i = 0; i < r->nfiles; i++) {
        __wt_free(session, r->files[i].uri);
        if ((c = r->files[i].c) != NULL)
            WT_TRET(c->close(c));
    }

    r->nfiles = 0;
    __wt_free(session, r->files);
    return (ret);
}

/*
 * __recovery_file_scan_prefix --
 *     Scan the files matching the prefix referenced from the metadata and gather information about
 *     them for recovery.
 */
//获取wiredtiger.wt中prefix开头的前缀wt文件的 checkpoint_lsn等信息保存到WT_RECOVERY
static int
__recovery_file_scan_prefix(WT_RECOVERY *r, const char *prefix, const char *ignore_suffix)
{
    WT_CURSOR *c;
    WT_DECL_RET;
    int cmp;
    const char *uri, *config;

    /* Scan through all entries in the metadata matching the prefix. */
    //也就是访问wiredtiger.wt文件的cursor
    c = r->files[0].c;
    c->set_key(c, prefix);
    if ((ret = c->search_near(c, &cmp)) != 0) {
        /* Is the metadata empty? */
        WT_RET_NOTFOUND_OK(ret);
        return (0);
    }
    if (cmp < 0 && (ret = c->next(c)) != 0) {
        /* No matching entries? */
        WT_RET_NOTFOUND_OK(ret);
        return (0);
    }
    for (; ret == 0; ret = c->next(c)) {
        WT_RET(c->get_key(c, &uri));
        if (!WT_PREFIX_MATCH(uri, prefix))
            break;
        //跳过ignore_suffix的文件
        if (ignore_suffix != NULL && WT_SUFFIX_MATCH(uri, ignore_suffix))
            continue;

        //获取该.wt文件的元数据配置信息
        WT_RET(c->get_value(c, &config));
        WT_RET(__recovery_setup_file(r, uri, config));
    }
    WT_RET_NOTFOUND_OK(ret);
    return (0);
}

/*
 * __recovery_file_scan --
 *     Scan the files referenced from the metadata and gather information about them for recovery.
 //获取wiredtiger.wt中prefix开头的前缀wt文件的 checkpoint_lsn等信息保存到WT_RECOVERY
 */
static int
__recovery_file_scan(WT_RECOVERY *r)
{
    /* Scan through all files and tiered entries in the metadata. */
    WT_RET(__recovery_file_scan_prefix(r, "file:", ".wtobj"));
    WT_RET(__recovery_file_scan_prefix(r, "tiered:", NULL));

    /*
     * Set the connection level file id tracker, as such upon creation of a new file we'll begin
     * from the latest file id.
     */
    S2C(r->session)->next_file_id = r->max_fileid;

    return (0);
}

/*
 * __hs_exists --
 *     Check whether the history store exists. This function looks for both the history store URI in
 *     the metadata file and for the history store data file itself. If we're running salvage, we'll
 *     attempt to salvage the history store here.
 //从wiredtiger.wt文件中WiredTigerHS.wt文件元数据是否存在
 */
static int
__hs_exists(WT_SESSION_IMPL *session, WT_CURSOR *metac, const char *cfg[], bool *hs_exists)
{
    WT_CONNECTION_IMPL *conn;
    WT_DECL_RET;
    WT_SESSION *wt_session;

    conn = S2C(session);

    /*
     * We should check whether the history store file exists in the metadata or not. If it does not,
     * then we should skip rollback to stable for each table. This might happen if we're upgrading
     * from an older version. If it does exist in the metadata we should check that it exists on
     * disk to confirm that it wasn't deleted between runs.
     *
     * This needs to happen after we apply the logs as they may contain the metadata changes which
     * include the history store creation. As such the on disk metadata file won't contain the
     * history store but will after log application.
     */
    metac->set_key(metac, WT_HS_URI);
    WT_ERR_NOTFOUND_OK(metac->search(metac), true);
    if (ret == WT_NOTFOUND) {
        *hs_exists = false;
        ret = 0;
    } else {
        /* Given the history store exists in the metadata validate whether it exists on disk. */
        WT_ERR(__wt_fs_exist(session, WT_HS_FILE, hs_exists));
        if (*hs_exists) {
            /*
             * Attempt to configure the history store, this will detect corruption if it fails.
             */
            ret = __wt_hs_config(session, cfg);
            if (ret != 0) {
                if (F_ISSET(conn, WT_CONN_SALVAGE)) {
                    wt_session = &session->iface;
                    WT_ERR(wt_session->salvage(wt_session, WT_HS_URI, NULL));
                } else
                    WT_ERR(ret);
            }
        } else {
            /*
             * We're attempting to salvage the database with a missing history store, remove it from
             * the metadata and pretend it never existed. As such we won't run rollback to stable
             * later.
             */
            if (F_ISSET(conn, WT_CONN_SALVAGE)) {
                *hs_exists = false;
                metac->remove(metac);
            } else
                /* The history store file has likely been deleted, we cannot recover from this. */
                WT_ERR_MSG(session, WT_TRY_SALVAGE, "%s file is corrupted or missing", WT_HS_FILE);
        }
    }
err:
    /* Unpin the page from cache. */
    WT_TRET(metac->reset(metac));
    return (ret);
}

/*
 * __wt_txn_recover --
 *     Run recovery.
 */
int
__wt_txn_recover(WT_SESSION_IMPL *session, const char *cfg[])
{
    WT_CONNECTION_IMPL *conn;
    WT_CURSOR *metac;
    WT_DECL_RET;
    WT_RECOVERY r;
    WT_RECOVERY_FILE *metafile;
    wt_off_t hs_size;
    char *config;
    char ts_string[2][WT_TS_INT_STRING_SIZE];
    bool do_checkpoint, eviction_started, hs_exists, needs_rec, was_backup;
    bool rts_executed;

    conn = S2C(session);
    F_SET(conn, WT_CONN_RECOVERING);

    WT_CLEAR(r);
    WT_INIT_LSN(&r.ckpt_lsn);
    config = NULL;
    do_checkpoint = hs_exists = true;
    rts_executed = false;
    eviction_started = false;
    was_backup = F_ISSET(conn, WT_CONN_WAS_BACKUP);

    /* We need a real session for recovery. */
    WT_RET(__wt_open_internal_session(conn, "txn-recover", false, 0, 0, &session));
    r.session = session;
    WT_MAX_LSN(&r.max_ckpt_lsn);
    WT_MAX_LSN(&r.max_rec_lsn);
    conn->txn_global.recovery_timestamp = conn->txn_global.meta_ckpt_timestamp = WT_TS_NONE;

    //从wireditger.turtle文件获取wiredtiger.wt的元数据配置信息
    WT_ERR(__wt_metadata_search(session, WT_METAFILE_URI, &config));
    WT_ERR(__recovery_setup_file(&r, WT_METAFILE_URI, config));
    WT_ERR(__wt_metadata_cursor_open(session, NULL, &metac));
    metafile = &r.files[WT_METAFILE_ID];
    //也就是访问WiredTiger.wt的cursor
    metafile->c = metac;

    //也就是获取WiredTiger.wt文件中的system:checkpoint_base_write_gen
    WT_ERR(__recovery_set_ckpt_base_write_gen(&r));

    /*
     * If no log was found (including if logging is disabled), or if the last checkpoint was done
     * with logging disabled, recovery should not run. Scan the metadata to figure out the largest
     * file ID.
     */
    //没有WiredTigerLog.xxxx日志文件或者关闭了日志功能，见 __wt_log_open
    if (!FLD_ISSET(conn->log_flags, WT_CONN_LOG_EXISTED) || WT_IS_MAX_LSN(&metafile->ckpt_lsn)) {
        /*
         * Detect if we're going from logging disabled to enabled. We need to know this to verify
         * LSNs and start at the correct log file later. If someone ran with logging, then disabled
         * it and removed all the log files and then turned logging back on, we have to start
         * logging in the log file number that is larger than any checkpoint LSN we have from the
         * earlier time.
         */

        //获取wiredtiger.wt中prefix开头的前缀wt文件的 checkpoint_lsn等信息保存到WT_RECOVERY
        WT_ERR(__recovery_file_scan(&r));
        /*
         * The array can be re-allocated in recovery_file_scan. Reset our pointer after scanning all
         * the files.
         */
        metafile = &r.files[WT_METAFILE_ID];

        if (FLD_ISSET(conn->log_flags, WT_CONN_LOG_ENABLED) && WT_IS_MAX_LSN(&metafile->ckpt_lsn) &&
          !WT_IS_MAX_LSN(&r.max_ckpt_lsn))
            WT_ERR(__wt_log_reset(session, r.max_ckpt_lsn.l.file));
        else
            do_checkpoint = false;
        WT_ERR(__hs_exists(session, metac, cfg, &hs_exists));
        goto done;
    }

    /*
     * First, do a pass through the log to recover the metadata, and establish the last checkpoint
     * LSN. Skip this when opening a hot backup: we already have the correct metadata in that case.
     *
     * If we're running with salvage and we hit an error, we ignore it and continue. In salvage we
     * want to recover whatever part of the data we can from the last checkpoint up until whatever
     * problem we detect in the log file. In salvage, we ignore errors from scanning the log so
     * recovery can continue. Other errors remain errors.
     */
    if (!was_backup) {
        //标识该if分支apply日志到wiredtiger.wt
        r.metadata_only = true;
        /*
         * If this is a read-only connection, check if the checkpoint LSN in the metadata file is up
         * to date, indicating a clean shutdown.
         */
        if (F_ISSET(conn, WT_CONN_READONLY)) {
            WT_ERR(__wt_log_needs_recovery(session, &metafile->ckpt_lsn, &needs_rec));
            if (needs_rec)
                WT_ERR_MSG(session, WT_RUN_RECOVERY, "Read-only database needs recovery");
        }

        //例如节点刚启动，写数据还不到一分钟，还没有做checkpoint，这时候只有WAL日志，回放WAL日志
        if (WT_IS_INIT_LSN(&metafile->ckpt_lsn))
            //没有checkpoint信息，则start_lsnp 和end lsn都为NULL，也就是从
            ret = __wt_log_scan(session, NULL, NULL, WT_LOGSCAN_FIRST, __txn_log_recover, &r);
        else {
            /*
             * Start at the last checkpoint LSN referenced in the metadata. If we see the end of a
             * checkpoint while scanning, we will change the full scan to start from there.
             */
            WT_ASSIGN_LSN(&r.ckpt_lsn, &metafile->ckpt_lsn);
            //从ckpt_lsn开始恢复wiredtiger.wt的WAL日志
            ret = __wt_log_scan(session, &metafile->ckpt_lsn, NULL, WT_LOGSCAN_RECOVER_METADATA,
              __txn_log_recover, &r);
        }
        if (F_ISSET(conn, WT_CONN_SALVAGE))
            ret = 0;
        /*
         * If log scan couldn't find a file we expected to be around, this indicates a corruption of
         * some sort.
         */
        if (ret == ENOENT) {
            F_SET(conn, WT_CONN_DATA_CORRUPTION);
            ret = WT_ERROR;
        }

        WT_ERR(ret);
    }

    /* Scan the metadata to find the live files and their IDs. */
    //获取wiredtiger.wt中prefix开头的前缀wt文件的 checkpoint_lsn等信息保存到WT_RECOVERY
    WT_ERR(__recovery_file_scan(&r));

    /*
     * Check whether the history store exists.
     *
     * This will open a dhandle on the history store and initialize its write gen so we must ensure
     * that the connection-wide base write generation is stable at this point. Performing a recovery
     * file scan will involve updating the connection-wide base write generation so we MUST do this
     * before checking for the existence of a history store file.
     */
    //从wiredtiger.wt文件中WiredTigerHS.wt文件元数据是否存在
    WT_ERR(__hs_exists(session, metac, cfg, &hs_exists));

    /*
     * Clear this out. We no longer need it and it could have been re-allocated when scanning the
     * files.
     */
    WT_NOT_READ(metafile, NULL);

    /*
     * We no longer need the metadata cursor: close it to avoid pinning any resources that could
     * block eviction during recovery.
     */
    //
    r.files[0].c = NULL;
    WT_ERR(metac->close(metac));

    /*
     * Now, recover all the files apart from the metadata. Pass WT_LOGSCAN_RECOVER so that old logs
     * get truncated.
     */
    //说明后面开始apply wal日志到 wiredtiger.wt以外的wt文件   yang add todo xxxxxxxxxxxxxxxxxxxxx 同样的
    r.metadata_only = false;
    __wt_verbose_multi(session, WT_VERB_RECOVERY_ALL,
      "Main recovery loop: starting at %" PRIu32 "/%" PRIu32 " to %" PRIu32 "/%" PRIu32,
      r.ckpt_lsn.l.file, r.ckpt_lsn.l.offset, r.max_rec_lsn.l.file, r.max_rec_lsn.l.offset);

    // needs_rec判断是否有需要恢复得WiredTigerLog.xxxx中的wal记录，1标识有，0标识没有
    WT_ERR(__wt_log_needs_recovery(session, &r.ckpt_lsn, &needs_rec));
    /*
     * Check if the database was shut down cleanly. If not return an error if the user does not want
     * automatic recovery.
     */
    //recover失败，给出异常打印，如果是./wt需要加-R参数
    if (needs_rec &&
      (FLD_ISSET(conn->log_flags, WT_CONN_LOG_RECOVER_ERR) || F_ISSET(conn, WT_CONN_READONLY))) {
        if (F_ISSET(conn, WT_CONN_READONLY))
            WT_ERR_MSG(session, WT_RUN_RECOVERY, "Read-only database needs recovery");
        WT_ERR_MSG(session, WT_RUN_RECOVERY, "Database needs recovery");
    }

    if (F_ISSET(conn, WT_CONN_READONLY)) {
        do_checkpoint = false;
        goto done;
    }

    if (!hs_exists) {
        __wt_verbose_multi(session, WT_VERB_RECOVERY_ALL, "%s",
          "Creating the history store before applying log records. Likely recovering after an"
          "unclean shutdown on an earlier version");
        /*
         * Create the history store as we might need it while applying log records in recovery.
         */
        WT_ERR(__wt_hs_open(session, cfg));
    }

    /*
     * Recovery can touch more data than fits in cache, so it relies on regular eviction to manage
     * paging. Start eviction threads for recovery without history store cursors.
     */
    WT_ERR(__wt_evict_create(session));
    eviction_started = true;

    /*
     * Always run recovery even if it was a clean shutdown only if this is not a read-only
     * connection. We can consider skipping it in the future.
     */
    if (needs_rec)
        FLD_SET(conn->log_flags, WT_CONN_LOG_RECOVER_DIRTY);
    if (WT_IS_INIT_LSN(&r.ckpt_lsn))
        ret = __wt_log_scan(
          session, NULL, NULL, WT_LOGSCAN_FIRST | WT_LOGSCAN_RECOVER, __txn_log_recover, &r);
    else
        ret = __wt_log_scan(session, &r.ckpt_lsn, NULL, WT_LOGSCAN_RECOVER, __txn_log_recover, &r);
    if (F_ISSET(conn, WT_CONN_SALVAGE))
        ret = 0;
    WT_ERR(ret);

done:
    /* Close cached cursors, rollback-to-stable asserts exclusive access. */
    WT_ERR(__recovery_close_cursors(&r));
#ifndef WT_STANDALONE_BUILD
    /*
     * There is a known problem with upgrading from release 10.0.0 specifically. There are now fixes
     * that can properly upgrade from 10.0.0 without hitting the problem but only from a clean
     * shutdown of 10.0.0. Earlier releases are not affected by the upgrade issue.
     */
    if (conn->unclean_shutdown && __wt_version_eq(conn->recovery_version, (WT_VERSION){10, 0, 0}))
        WT_ERR_MSG(session, WT_ERROR,
          "Upgrading from a WiredTiger version 10.0.0 database that was not shutdown cleanly is "
          "not allowed. Perform a clean shutdown on version 10.0.0 and then upgrade.");
#endif
    //从wiredtiger.wt文件中获取checkpoint_timestamp   oldest_timestamp信息
    WT_ERR(__recovery_txn_setup_initial_state(session, &r));

    /*
     * Set the history store file size as it may already exist after a restart.
     */
    if (hs_exists) {
        WT_ERR(__wt_block_manager_named_size(session, WT_HS_FILE, &hs_size));
        WT_STAT_CONN_SET(session, cache_hs_ondisk, hs_size);
    }

    /*
     * Perform rollback to stable only when the following conditions met.
     * 1. The connection is not read-only. A read-only connection expects that there shouldn't be
     *    any changes that need to be done on the database other than reading.
     * 2. The history store file was found in the metadata.
     */
    if (hs_exists && !F_ISSET(conn, WT_CONN_READONLY)) {
        /* Start the eviction threads for rollback to stable if not already started. */
        if (!eviction_started) {
            WT_ERR(__wt_evict_create(session));
            eviction_started = true;
        }

        __wt_verbose_multi(session,
          WT_DECL_VERBOSE_MULTI_CATEGORY(((WT_VERBOSE_CATEGORY[]){WT_VERB_RECOVERY, WT_VERB_RTS})),
          "performing recovery rollback_to_stable with stable timestamp: %s and oldest timestamp: "
          "%s",
          __wt_timestamp_to_string(conn->txn_global.stable_timestamp, ts_string[0]),
          __wt_timestamp_to_string(conn->txn_global.oldest_timestamp, ts_string[1]));
        rts_executed = true;
        WT_ERR(__wt_rollback_to_stable(session, NULL, true));
    }

    /*
     * Sometimes eviction is triggered after doing a checkpoint. However, we don't want eviction to
     * make the tree dirty after checkpoint as this will interfere with WT_SESSION alter which
     * expects a clean tree.
     */
    if (eviction_started)
        WT_TRET(__wt_evict_destroy(session));

    if (do_checkpoint || rts_executed)
        /*
         * Forcibly log a checkpoint so the next open is fast and keep the metadata up to date with
         * the checkpoint LSN and removal.
         */
        WT_ERR(session->iface.checkpoint(&session->iface, "force=1"));

    /* Remove any backup file now that metadata has been synced. */
    WT_ERR(__wt_backup_file_remove(session));

    /*
     * Update the open dhandles write generations and base write generation with the connection's
     * base write generation because the recovery checkpoint writes the pages to disk with new write
     * generation number which contains transaction ids that are needed to reset later. The
     * connection level base write generation number is updated at the end of the recovery
     * checkpoint.
     */
    WT_ERR(__wt_dhandle_update_write_gens(session));

    /*
     * If we're downgrading and have newer log files, force log removal, no matter what the remove
     * setting is.
     */
    if (FLD_ISSET(conn->log_flags, WT_CONN_LOG_FORCE_DOWNGRADE))
        WT_ERR(__wt_log_truncate_files(session, NULL, true));
    FLD_SET(conn->log_flags, WT_CONN_LOG_RECOVER_DONE);

err:
    WT_TRET(__recovery_close_cursors(&r));
    __wt_free(session, config);
    FLD_CLR(conn->log_flags, WT_CONN_LOG_RECOVER_DIRTY);

    if (ret != 0) {
        FLD_SET(conn->log_flags, WT_CONN_LOG_RECOVER_FAILED);
        __wt_err(session, ret, "Recovery failed");
    }

    /*
     * Destroy the eviction threads that were started in support of recovery. They will be restarted
     * once the history store table is created.
     */
    if (eviction_started)
        WT_TRET(__wt_evict_destroy(session));

    WT_TRET(__wt_session_close_internal(session));
    F_SET(conn, WT_CONN_RECOVERY_COMPLETE);
    F_CLR(conn, WT_CONN_RECOVERING);

    return (ret);
}
