/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

#ifdef HAVE_DIAGNOSTIC
/*
 * __txn_op_log_row_key_check --
 *     Confirm the cursor references the correct key.
 */
static void
__txn_op_log_row_key_check(WT_SESSION_IMPL *session, WT_CURSOR_BTREE *cbt)
{
    WT_CURSOR *cursor;
    WT_ITEM key;
    WT_PAGE *page;
    WT_ROW *rip;
    int cmp;

    cursor = &cbt->iface;
    WT_ASSERT(session, F_ISSET(cursor, WT_CURSTD_KEY_SET));

    memset(&key, 0, sizeof(key));

    /*
     * We used to take the row-store logging key from the page referenced by the cursor, then
     * switched to taking it from the cursor itself. Check they are the same.
     *
     * If the cursor references a WT_INSERT item, take the key from there, else take the key from
     * the original page.
     */
    if (cbt->ins == NULL) {
        session = CUR2S(cbt);
        page = cbt->ref->page;
        WT_ASSERT(session, cbt->slot < page->entries);
        rip = &page->pg_row[cbt->slot];
        WT_ASSERT(session, __wt_row_leaf_key(session, page, rip, &key, false) == 0);
    } else {
        key.data = WT_INSERT_KEY(cbt->ins);
        key.size = WT_INSERT_KEY_SIZE(cbt->ins);
    }

    WT_ASSERT(session, __wt_compare(session, CUR2BT(cbt)->collator, &key, &cursor->key, &cmp) == 0);
    WT_ASSERT(session, cmp == 0);

    __wt_buf_free(session, &key);
}
#endif

/*
 * __txn_op_log --
 *     Log an operation for the current transaction.
 */
static int
__txn_op_log(
  WT_SESSION_IMPL *session, WT_ITEM *logrec, WT_TXN_OP *op, WT_CURSOR_BTREE *cbt, uint32_t fileid)
{
    WT_CURSOR *cursor;
    WT_ITEM value;
    WT_UPDATE *upd;
    uint64_t recno;

    cursor = &cbt->iface;
    upd = op->u.op_upd;
    value.data = upd->data;
    value.size = upd->size;

    //__wt_buf_extend logrec空间，向logrec追加op，这样可以保证同一个事务日志在一个logrec中
    /*
     * Log the row- or column-store insert, modify, remove or update. Our caller doesn't log reserve
     * operations, we shouldn't see them here.
     */
    if (CUR2BT(cbt)->type == BTREE_ROW) {
#ifdef HAVE_DIAGNOSTIC
        __txn_op_log_row_key_check(session, cbt);
#endif
        switch (upd->type) {
        case WT_UPDATE_MODIFY:
            /*
             * Write full updates to the log for size-changing modify operations: they aren't
             * idempotent and recovery cannot guarantee that they will be applied exactly once. We
             * rely on the cursor value already having the modify applied.
             */
            if (__wt_modify_idempotent(upd->data))
                WT_RET(__wt_logop_row_modify_pack(session, logrec, fileid, &cursor->key, &value));
            else
                WT_RET(
                  __wt_logop_row_put_pack(session, logrec, fileid, &cursor->key, &cursor->value));
            break;
        case WT_UPDATE_STANDARD:
            WT_RET(__wt_logop_row_put_pack(session, logrec, fileid, &cursor->key, &value));
            break;
        case WT_UPDATE_TOMBSTONE:
            WT_RET(__wt_logop_row_remove_pack(session, logrec, fileid, &cursor->key));
            break;
        default:
            return (__wt_illegal_value(session, upd->type));
        }
    } else {
        recno = WT_INSERT_RECNO(cbt->ins);
        WT_ASSERT(session, recno != WT_RECNO_OOB);

        switch (upd->type) {
        case WT_UPDATE_MODIFY:
            if (__wt_modify_idempotent(upd->data))
                WT_RET(__wt_logop_col_modify_pack(session, logrec, fileid, recno, &value));
            else
                WT_RET(__wt_logop_col_put_pack(session, logrec, fileid, recno, &cursor->value));
            break;
        case WT_UPDATE_STANDARD:
            WT_RET(__wt_logop_col_put_pack(session, logrec, fileid, recno, &value));
            break;
        case WT_UPDATE_TOMBSTONE:
            WT_RET(__wt_logop_col_remove_pack(session, logrec, fileid, recno));
            break;
        default:
            return (__wt_illegal_value(session, upd->type));
        }
    }

    return (0);
}

/*
 * __txn_oplist_printlog --
 *     Print a list of operations from a log record.
 */
static int
__txn_oplist_printlog(
  WT_SESSION_IMPL *session, const uint8_t **pp, const uint8_t *end, WT_TXN_PRINTLOG_ARGS *args)
{
    bool firstrecord;

    firstrecord = true;
    WT_RET(__wt_fprintf(session, args->fs, "    \"ops\": [\n"));

    /* The logging subsystem zero-pads records. */
    while (*pp < end && **pp) {
        if (!firstrecord)
            WT_RET(__wt_fprintf(session, args->fs, ",\n"));
        WT_RET(__wt_fprintf(session, args->fs, "      {"));

        firstrecord = false;

        WT_RET(__wt_txn_op_printlog(session, pp, end, args));
        WT_RET(__wt_fprintf(session, args->fs, "\n      }"));
    }

    WT_RET(__wt_fprintf(session, args->fs, "\n    ]\n"));

    return (0);
}

/*
 * __wt_txn_op_free --
 *     Free memory associated with a transactional operation.
 */
void
__wt_txn_op_free(WT_SESSION_IMPL *session, WT_TXN_OP *op)
{
    switch (op->type) {
    case WT_TXN_OP_NONE:
        /*
         * The free function can be called more than once: when there's no operation, a free is
         * unnecessary or has already been done.
         */
        return;
    case WT_TXN_OP_BASIC_COL:
    case WT_TXN_OP_INMEM_COL:
    case WT_TXN_OP_REF_DELETE:
    case WT_TXN_OP_TRUNCATE_COL:
        break;

    case WT_TXN_OP_BASIC_ROW:
    case WT_TXN_OP_INMEM_ROW:
        __wt_buf_free(session, &op->u.op_row.key);
        break;

    case WT_TXN_OP_TRUNCATE_ROW:
        __wt_buf_free(session, &op->u.truncate_row.start);
        __wt_buf_free(session, &op->u.truncate_row.stop);
        break;
    }

    (void)__wt_atomic_subi32(&op->btree->dhandle->session_inuse, 1);

    op->type = WT_TXN_OP_NONE;
    op->flags = 0;
}

/* 以timestamp_abort为例，例如三条不同表数据一个事务写入对应事务日志如下:
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
      "value": "COLL: thread"
    },
    { "optype": "row_put",
      "fileid": 2147483651 0x80000003,
      "key": "yang test timestamp_abort 0000000000\u0000",
      "value": "COLL: thread"
    },
    { "optype": "row_put",
      "fileid": 5 0x5,
      "key": "yang test timestamp_abort 0000000000\u0000",
      "value": "OPLOG: thread"
    }
  ]
},*/

/*
 * __txn_logrec_init --
 *     Allocate and initialize a buffer for a transaction's log records.
  //__wt_txn_log_op  __wt_txn_ts_log调用
 //分配一个WT_LOG_RECORD空间并存入rectype txn->id封包存入data中
 */
static int
__txn_logrec_init(WT_SESSION_IMPL *session)
{
    //logrec为WT_LOG_RECORD类型
    WT_DECL_ITEM(logrec);
    WT_DECL_RET;
    WT_TXN *txn;
    size_t header_size;
    uint32_t rectype;
    const char *fmt;

    txn = session->txn;
    rectype = WT_LOGREC_COMMIT;
    fmt = WT_UNCHECKED_STRING(Iq);

    //例如多个写放到一个事务，则只需要第一个op需要创建logrec空间，并对日志头部赋值
    if (txn->logrec != NULL) {
        WT_ASSERT(session, F_ISSET(txn, WT_TXN_HAS_ID));
        return (0);
    }

    /*
     * The only way we should ever get in here without a txn id is if we are recording diagnostic
     * information. In that case, allocate an id.
     */
    if (FLD_ISSET(S2C(session)->log_flags, WT_CONN_LOG_DEBUG_MODE) && txn->id == WT_TXN_NONE)
        WT_RET(__wt_txn_id_check(session));
    else
        WT_ASSERT(session, txn->id != WT_TXN_NONE);

    //计算rectype txn->id封包后的长度存入header_size
    WT_RET(__wt_struct_size(session, &header_size, fmt, rectype, txn->id));
    WT_RET(__wt_logrec_alloc(session, header_size, &logrec));

    //header_size rectype txn->id封包存入data中
    WT_ERR(__wt_struct_pack(
      session, (uint8_t *)logrec->data + logrec->size, header_size, fmt, rectype, txn->id));
    logrec->size += (uint32_t)header_size;
    txn->logrec = logrec;

    if (0) {
err:
        __wt_logrec_free(session, &logrec);
    }
    return (ret);
}

/* 以timestamp_abort为例，例如三条不同表数据一个事务写入对应事务日志如下:
{ "lsn" : [1,10112],
  "hdr_flags" : "",
  "rec_len" : 256,
  "mem_len" : 256,
  "type" : "commit",
  "txnid" : 14,
  "ops": [
    { "optype": "row_put",
      "fileid": 2147483650 0x80000002,   //fileid代表具体的一个表    
      "key": "yang test timestamp_abort 0000000000\u0000",
      "value": "COLL: thread"
    },
    { "optype": "row_put",
      "fileid": 2147483651 0x80000003,  
      "key": "yang test timestamp_abort 0000000000\u0000",
      "value": "COLL: thread"
    },
    { "optype": "row_put",
      "fileid": 5 0x5,
      "key": "yang test timestamp_abort 0000000000\u0000",
      "value": "OPLOG: thread"
    }
  ]
},*/

/*
 * __wt_txn_log_op --
 *     Write the last logged operation into the in-memory buffer.
 //事务日志__wt_txn_log_op封装事务日志到txn->logrec，__wt_txn_log_commit对事务日志txn->logrec写盘操作
 */ //也就是头部字段填充
int
__wt_txn_log_op(WT_SESSION_IMPL *session, WT_CURSOR_BTREE *cbt)
{
    WT_CONNECTION_IMPL *conn;
    WT_DECL_RET;
    WT_ITEM *logrec;
    WT_TXN *txn;
    WT_TXN_OP *op;
    uint32_t fileid;

    conn = S2C(session);
    txn = session->txn;

    /* We'd better have a transaction. */
    WT_ASSERT(session, F_ISSET(txn, WT_TXN_RUNNING) && F_ISSET(txn, WT_TXN_HAS_ID));

    WT_ASSERT(session, txn->mod_count > 0);
    op = txn->mod + txn->mod_count - 1;
    //代表对应的表
    fileid = op->btree->id;

    /*
     * If this operation is diagnostic only, set the ignore bit on the fileid so that recovery can
     * skip it.
     */
    if (!F_ISSET(S2BT(session), WT_BTREE_LOGGED) &&
      FLD_ISSET(conn->log_flags, WT_CONN_LOG_DEBUG_MODE))
        FLD_SET(fileid, WT_LOGOP_IGNORE);

/*
{ "lsn" : [1,10112],
  "hdr_flags" : "",
  "rec_len" : 256,
  "mem_len" : 256,
  "type" : "commit",
  "txnid" : 14,
*/
    //也就是头部相关字段填充，并创建txn->logrec空间
    WT_RET(__txn_logrec_init(session));
    logrec = txn->logrec;

    switch (op->type) {
    case WT_TXN_OP_NONE:
    case WT_TXN_OP_INMEM_COL:
    case WT_TXN_OP_INMEM_ROW:
    case WT_TXN_OP_REF_DELETE:
        /* Nothing to log, we're done. */
        break;
    case WT_TXN_OP_BASIC_COL:
    case WT_TXN_OP_BASIC_ROW:
        ret = __txn_op_log(session, logrec, op, cbt, fileid);
        break;
    case WT_TXN_OP_TRUNCATE_COL:
        ret = __wt_logop_col_truncate_pack(
          session, logrec, fileid, op->u.truncate_col.start, op->u.truncate_col.stop);
        break;
    case WT_TXN_OP_TRUNCATE_ROW:
        ret = __wt_logop_row_truncate_pack(session, logrec, fileid, &op->u.truncate_row.start,
          &op->u.truncate_row.stop, (uint32_t)op->u.truncate_row.mode);
        break;
    }
    return (ret);
}

/* 以timestamp_abort为例，例如三条不同表数据一个事务写入对应事务日志如下:
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
      "value": "COLL: thread"
    },
    { "optype": "row_put",
      "fileid": 2147483651 0x80000003,
      "key": "yang test timestamp_abort 0000000000\u0000",
      "value": "COLL: thread"
    },
    { "optype": "row_put",
      "fileid": 5 0x5,
      "key": "yang test timestamp_abort 0000000000\u0000",
      "value": "OPLOG: thread"
    }
  ]
},*/

/*
 * __wt_txn_log_commit --
 *     Write the operations of a transaction to the log at commit time.
 //事务日志__wt_txn_log_op封装事务日志到txn->logrec，__wt_txn_log_commit对事务日志txn->logrec写盘操作
 */
int
__wt_txn_log_commit(WT_SESSION_IMPL *session, const char *cfg[])
{
    WT_TXN *txn;

    WT_UNUSED(cfg);
    txn = session->txn;
    /*
     * If there are no log records there is nothing to do.
     */
    if (txn->logrec == NULL)
        return (0);

   // printf("yang test .............................__wt_txn_log_commit..........\r\n");
    /* Write updates to the log. */
    ////txn_logsync赋值见__wt_txn_begin，也就是conn->txn_logsync(__logmgr_sync_cfg)
    //transaction_sync.method配置存入到了txn_logsync，也就是sync还是flush
    return (__wt_log_write(session, txn->logrec, NULL, txn->txn_logsync));
}

/*
 * __txn_log_file_sync --
 *     Write a log record for a file sync.
 */
static int
__txn_log_file_sync(WT_SESSION_IMPL *session, uint32_t flags, WT_LSN *lsnp)
{
    WT_BTREE *btree;
    WT_DECL_ITEM(logrec);
    WT_DECL_RET;
    size_t header_size;
    uint32_t rectype, start;
    const char *fmt;
    bool need_sync;

    btree = S2BT(session);
    rectype = WT_LOGREC_FILE_SYNC;
    start = LF_ISSET(WT_TXN_LOG_CKPT_START) ? 1 : 0;
    fmt = WT_UNCHECKED_STRING(III);
    need_sync = LF_ISSET(WT_TXN_LOG_CKPT_SYNC);

    WT_RET(__wt_struct_size(session, &header_size, fmt, rectype, btree->id, start));
    WT_RET(__wt_logrec_alloc(session, header_size, &logrec));

    WT_ERR(__wt_struct_pack(session, (uint8_t *)logrec->data + logrec->size, header_size, fmt,
      rectype, btree->id, start));
    logrec->size += (uint32_t)header_size;

   // printf("yang test .............................__txn_log_file_sync..........\r\n");
    WT_ERR(__wt_log_write(session, logrec, lsnp, need_sync ? WT_LOG_FSYNC : 0));
err:
    __wt_logrec_free(session, &logrec);
    return (ret);
}

/*
 * __wt_txn_checkpoint_logread --
 *     Read a log record for a checkpoint operation.
 */
int
__wt_txn_checkpoint_logread(
  WT_SESSION_IMPL *session, const uint8_t **pp, const uint8_t *end, WT_LSN *ckpt_lsn)
{
    WT_DECL_RET;
    WT_ITEM ckpt_snapshot_unused;
    uint32_t ckpt_file, ckpt_offset;
    u_int ckpt_nsnapshot_unused;
    const char *fmt;

    fmt = WT_UNCHECKED_STRING(IIIu);

    if ((ret = __wt_struct_unpack(session, *pp, WT_PTRDIFF(end, *pp), fmt, &ckpt_file, &ckpt_offset,
           &ckpt_nsnapshot_unused, &ckpt_snapshot_unused)) != 0)
        WT_RET_MSG(session, ret, "txn_checkpoint_logread: unpack failure");
    WT_SET_LSN(ckpt_lsn, ckpt_file, ckpt_offset);
    *pp = end;
    return (0);
}

/*
 * __wt_txn_ts_log --
 *     Write a log record recording timestamps in the transaction.
 */
int
__wt_txn_ts_log(WT_SESSION_IMPL *session)
{
    struct timespec t;
    WT_ITEM *logrec;
    WT_TXN *txn;
    WT_TXN_SHARED *txn_shared;
    wt_timestamp_t commit, durable, first_commit, prepare, read;

    txn = session->txn;
    txn_shared = WT_SESSION_TXN_SHARED(session);

    /* We'd better have a transaction, but we may not have allocated an ID. */
    WT_ASSERT(session, F_ISSET(txn, WT_TXN_RUNNING));

    /*
     * There is a rare usage case of a prepared transaction that has no modifications, but then
     * commits and sets timestamps. If an empty transaction has been prepared, don't bother writing
     * a timestamp operation record.
     */
    if (F_ISSET(txn, WT_TXN_PREPARE) && txn->mod_count == 0)
        return (0);

    WT_RET(__txn_logrec_init(session));
    logrec = txn->logrec;
    commit = durable = first_commit = prepare = read = WT_TS_NONE;
    if (F_ISSET(txn, WT_TXN_HAS_TS_COMMIT)) {
        commit = txn->commit_timestamp;
        first_commit = txn->first_commit_timestamp;
    }
    if (F_ISSET(txn, WT_TXN_HAS_TS_DURABLE))
        durable = txn->durable_timestamp;
    if (F_ISSET(txn, WT_TXN_HAS_TS_PREPARE))
        prepare = txn->prepare_timestamp;
    if (F_ISSET(txn, WT_TXN_SHARED_TS_READ))
        read = txn_shared->read_timestamp;

    __wt_epoch(session, &t);
    return (__wt_logop_txn_timestamp_pack(session, logrec, (uint64_t)t.tv_sec, (uint64_t)t.tv_nsec,
      commit, durable, first_commit, prepare, read));
}

/*
 * __wt_txn_checkpoint_log --
 *     Write a log record for a checkpoint operation.
 一个完整的checkpoint日志信息如下:
 
{ "lsn" : [1,11776],  ----------------------------- wiredTiger.wt元数据的checkpoint_lsn也就是这个
    "hdr_flags" : "",
    "rec_len" : 128,
    "mem_len" : 128,
    "type" : "system",
    "ops": [
      { "optype": "checkpoint_start"   ----------------标识checkpoint开始时间，做recover恢复的时候从这个时间点开始恢复后面的LSN日志

      }
    ]
  },
..............  这期间有其他写操作事务，具体可以看printlog.log
..............
  { "lsn" : [1,16512],
    "hdr_flags" : "",
    "rec_len" : 7168,
    "mem_len" : 7168,
    "type" : "commit",
    "txnid" : 19,
    "ops": [
      { "optype": "row_put",
        "fileid": 0,
        "fileid-hex": "0x0",
        "key": "file:oplog.wt\u0000",
        "value": "access_pattern_hint=none,allocation_size=4KB,app_metadata=,assert=(commit_timestamp=none,durable_timestamp=none,read_timestamp=none,write_timestamp=off),block_allocation=best,block_compressor=,cache_resident=false,checksum=on,collator=,columns=,dictionary=0,encryption=(keyid=,name=),format=btree,huffman_key=,huffman_value=,id=5,ignore_in_memory_cache_size=false,internal_item_max=0,internal_key_max=0,internal_key_truncate=true,internal_page_max=4KB,key_format=S,key_gap=10,leaf_item_max=0,leaf_key_max=0,leaf_page_max=32KB,leaf_value_max=0,log=(enabled=true),memory_page_image_max=0,memory_page_max=5MB,os_cache_dirty_max=0,os_cache_max=0,prefix_compression=false,prefix_compression_min=4,readonly=false,split_deepen_min_child=0,split_deepen_per_child=0,split_pct=90,tiered_object=false,tiered_storage=(auth_token=,bucket=,bucket_prefix=,cache_directory=,local_retention=300,name=,object_target_size=0),value_format=u,verbose=[],version=(major=2,minor=1),write_timestamp_usage=none,checkpoint=(WiredTigerCheckpoint.1=(addr=\"018181e447dd9a358281e41546bd168381e4fe888a74808080e22fc0cfc0\",order=1,time=1720168135,size=8192,newest_start_durable_ts=0,oldest_start_ts=0,newest_txn=0,newest_stop_durable_ts=0,newest_stop_ts=-1,newest_stop_txn=-11,prepare=0,write_gen=3,run_write_gen=1)),checkpoint_backup_info=,checkpoint_lsn=(1,11776)\u0000"
      },
      { "optype": "row_put",
        "fileid": 0,
        "fileid-hex": "0x0",
        "key": "file:local.wt\u0000",
        "value": "access_pattern_hint=none,allocation_size=4KB,app_metadata=,assert=(commit_timestamp=none,durable_timestamp=none,read_timestamp=none,write_timestamp=off),block_allocation=best,block_compressor=,cache_resident=false,checksum=on,collator=,columns=,dictionary=0,encryption=(keyid=,name=),format=btree,huffman_key=,huffman_value=,id=4,ignore_in_memory_cache_size=false,internal_item_max=0,internal_key_max=0,internal_key_truncate=true,internal_page_max=4KB,key_format=S,key_gap=10,leaf_item_max=0,leaf_key_max=0,leaf_page_max=32KB,leaf_value_max=0,log=(enabled=true),memory_page_image_max=0,memory_page_max=5MB,os_cache_dirty_max=0,os_cache_max=0,prefix_compression=false,prefix_compression_min=4,readonly=false,split_deepen_min_child=0,split_deepen_per_child=0,split_pct=90,tiered_object=false,tiered_storage=(auth_token=,bucket=,bucket_prefix=,cache_directory=,local_retention=300,name=,object_target_size=0),value_format=u,verbose=[],version=(major=2,minor=1),write_timestamp_usage=none,checkpoint=(WiredTigerCheckpoint.1=(addr=\"018181e44bc913448281e41546bd168381e4df967412808080e22fc0cfc0\",order=1,time=1720168135,size=8192,newest_start_durable_ts=0,oldest_start_ts=0,newest_txn=0,newest_stop_durable_ts=0,newest_stop_ts=-1,newest_stop_txn=-11,prepare=0,write_gen=3,run_write_gen=1)),checkpoint_backup_info=,checkpoint_lsn=(1,11776)\u0000"
      },
      { "optype": "row_put",
        "fileid": 0,
        "fileid-hex": "0x0",
        "key": "file:shadow.wt\u0000",
        "value": "access_pattern_hint=none,allocation_size=4KB,app_metadata=,assert=(commit_timestamp=none,durable_timestamp=none,read_timestamp=none,write_timestamp=off),block_allocation=best,block_compressor=,cache_resident=false,checksum=on,collator=,columns=,dictionary=0,encryption=(keyid=,name=),format=btree,huffman_key=,huffman_value=,id=3,ignore_in_memory_cache_size=false,internal_item_max=0,internal_key_max=0,internal_key_truncate=true,internal_page_max=4KB,key_format=S,key_gap=10,leaf_item_max=0,leaf_key_max=0,leaf_page_max=32KB,leaf_value_max=0,log=(enabled=false),memory_page_image_max=0,memory_page_max=5MB,os_cache_dirty_max=0,os_cache_max=0,prefix_compression=false,prefix_compression_min=4,readonly=false,split_deepen_min_child=0,split_deepen_per_child=0,split_pct=90,tiered_object=false,tiered_storage=(auth_token=,bucket=,bucket_prefix=,cache_directory=,local_retention=300,name=,object_target_size=0),value_format=u,verbose=[],version=(major=2,minor=1),write_timestamp_usage=none,checkpoint=(WiredTigerCheckpoint.1=(addr=\"018181e42859f1f78281e41546bd168381e41ac58c8d808080e22fc0cfc0\",order=1,time=1720168135,size=8192,newest_start_durable_ts=1000000005,oldest_start_ts=0,newest_txn=16,newest_stop_durable_ts=0,newest_stop_ts=-1,newest_stop_txn=-11,prepare=0,write_gen=3,run_write_gen=1)),checkpoint_backup_info=,checkpoint_lsn=(1,11776)\u0000"
      },
      { "optype": "row_put",
        "fileid": 0,
        "fileid-hex": "0x0",
        "key": "file:collection.wt\u0000",
        "value": "access_pattern_hint=none,allocation_size=4KB,app_metadata=,assert=(commit_timestamp=none,durable_timestamp=none,read_timestamp=none,write_timestamp=off),block_allocation=best,block_compressor=,cache_resident=false,checksum=on,collator=,columns=,dictionary=0,encryption=(keyid=,name=),format=btree,huffman_key=,huffman_value=,id=2,ignore_in_memory_cache_size=false,internal_item_max=0,internal_key_max=0,internal_key_truncate=true,internal_page_max=4KB,key_format=S,key_gap=10,leaf_item_max=0,leaf_key_max=0,leaf_page_max=32KB,leaf_value_max=0,log=(enabled=false),memory_page_image_max=0,memory_page_max=5MB,os_cache_dirty_max=0,os_cache_max=0,prefix_compression=false,prefix_compression_min=4,readonly=false,split_deepen_min_child=0,split_deepen_per_child=0,split_pct=90,tiered_object=false,tiered_storage=(auth_token=,bucket=,bucket_prefix=,cache_directory=,local_retention=300,name=,object_target_size=0),value_format=u,verbose=[],version=(major=2,minor=1),write_timestamp_usage=none,checkpoint=(WiredTigerCheckpoint.1=(addr=\"018181e477ed4e168281e41546bd168381e4f367f73e808080e22fc0cfc0\",order=1,time=1720168135,size=8192,newest_start_durable_ts=0,oldest_start_ts=0,newest_txn=0,newest_stop_durable_ts=0,newest_stop_ts=-1,newest_stop_txn=-11,prepare=0,write_gen=3,run_write_gen=1)),checkpoint_backup_info=,checkpoint_lsn=(1,11776)\u0000"
      },
      { "optype": "row_put",
        "fileid": 0,
        "fileid-hex": "0x0",
        "key": "file:WiredTigerHS.wt\u0000",
        "value": "access_pattern_hint=none,allocation_size=4KB,app_metadata=,assert=(commit_timestamp=none,durable_timestamp=none,read_timestamp=none,write_timestamp=off),block_allocation=best,block_compressor=none,cache_resident=false,checksum=on,collator=,columns=,dictionary=0,encryption=(keyid=,name=),format=btree,huffman_key=,huffman_value=,id=1,ignore_in_memory_cache_size=false,internal_item_max=0,internal_key_max=0,internal_key_truncate=true,internal_page_max=16KB,key_format=IuQQ,key_gap=10,leaf_item_max=0,leaf_key_max=0,leaf_page_max=32KB,leaf_value_max=64MB,log=(enabled=true),memory_page_image_max=0,memory_page_max=5MB,os_cache_dirty_max=0,os_cache_max=0,prefix_compression=false,prefix_compression_min=4,readonly=false,split_deepen_min_child=0,split_deepen_per_child=0,split_pct=90,tiered_object=false,tiered_storage=(auth_token=,bucket=,bucket_prefix=,cache_directory=,local_retention=300,name=,object_target_size=0),value_format=QQQu,verbose=[],version=(major=2,minor=1),write_timestamp_usage=none,checkpoint=(WiredTigerCheckpoint.1=(addr=\"\",order=1,time=1720168135,size=0,newest_start_durable_ts=0,oldest_start_ts=0,newest_txn=0,newest_stop_durable_ts=0,newest_stop_ts=-1,newest_stop_txn=-11,prepare=0,write_gen=1,run_write_gen=1)),checkpoint_backup_info=,checkpoint_lsn=(1,0)\u0000"
      },
      { "optype": "row_put",
        "fileid": 0,
        "fileid-hex": "0x0",
        "key": "system:checkpoint\u0000",
        "value": "checkpoint_timestamp=\"3b9aca04\",checkpoint_time=1720168135,write_gen=1\u0000"
      },
      { "optype": "row_put",
        "fileid": 0,
        "fileid-hex": "0x0",
        "key": "system:oldest\u0000",
        "value": "oldest_timestamp=\"3b9aca04\",checkpoint_time=1720168135,write_gen=1\u0000"
      },
      { "optype": "row_put",
        "fileid": 0,
        "fileid-hex": "0x0",
        "key": "system:checkpoint_snapshot\u0000",
        "value": "snapshot_min=18,snapshot_max=19,snapshot_count=1,snapshots=[18],checkpoint_time=1720168135,write_gen=1\u0000"
      },
      { "optype": "row_put",
        "fileid": 0,
        "fileid-hex": "0x0",
        "key": "system:checkpoint_base_write_gen\u0000",
        "value": "base_write_gen=1\u0000"
      }
    ]
  },
  ......................  这期间有其他写操作事务，具体可以看printlog.log
  ......................
  
  { "lsn" : [1,24576],
    "hdr_flags" : "",
    "rec_len" : 128,
    "mem_len" : 128,
    "type" : "message",
    "message" : "RENAME: posix_directory_sync WT_HOME/WiredTiger.turtle.set"
  },
  { "lsn" : [1,24704],
    "hdr_flags" : "",
    "rec_len" : 128,
    "mem_len" : 128,
    "type" : "message",
    "message" : "RENAME: DONE posix_directory_sync WT_HOME/WiredTiger.turtle.set"
  },
  { "lsn" : [1,24832],
    "hdr_flags" : "",
    "rec_len" : 128,
    "mem_len" : 128,
    "type" : "checkpoint",  ------------------ 代表事务提交的LSN
    "ckpt_lsn" : [1,11776]  ------------------ wiredTiger.wt元数据的checkpoint_lsn也就是这个
  },
 */
int
__wt_txn_checkpoint_log(WT_SESSION_IMPL *session, bool full, uint32_t flags, WT_LSN *lsnp)
{
    WT_CONNECTION_IMPL *conn;
    WT_DECL_ITEM(logrec);
    WT_DECL_RET;
    WT_ITEM *ckpt_snapshot, empty;
    WT_LSN *ckpt_lsn;
    WT_TXN *txn;
    WT_TXN_GLOBAL *txn_global;
    size_t recsize;
    uint32_t i, rectype;
    uint8_t *end, *p;
    const char *fmt;

    conn = S2C(session);
    txn_global = &conn->txn_global;
    txn = session->txn;
    ckpt_lsn = &txn->ckpt_lsn;

   // printf("yang test .............................__wt_txn_checkpoint_log........flags:0x%x\r\n", flags);

    /*
     * If this is a file sync, log it unless there is a full checkpoint in progress.
     */
    if (!full) {
        if (txn->full_ckpt) {
            if (lsnp != NULL)
                *lsnp = *ckpt_lsn;
            return (0);
        }
        return (__txn_log_file_sync(session, flags, lsnp));
    }

    switch (flags) {
    case WT_TXN_LOG_CKPT_PREPARE:
        txn->full_ckpt = true;

        if (__wt_version_gte(conn->compat_version, WT_LOG_V2_VERSION)) {
            /*
             * Write the system log record containing a checkpoint start operation.
             */
            rectype = WT_LOGREC_SYSTEM;
            fmt = WT_UNCHECKED_STRING(I);
            WT_ERR(__wt_struct_size(session, &recsize, fmt, rectype));
            WT_ERR(__wt_logrec_alloc(session, recsize, &logrec));

            WT_ERR(__wt_struct_pack(
              session, (uint8_t *)logrec->data + logrec->size, recsize, fmt, rectype));
            logrec->size += (uint32_t)recsize;
            WT_ERR(__wt_logop_checkpoint_start_pack(session, logrec));
            WT_ERR(__wt_log_write(session, logrec, ckpt_lsn, 0));
        } else {
            WT_ERR(__wt_log_printf(session, "CHECKPOINT: Starting record"));
            WT_ERR(__wt_log_flush_lsn(session, ckpt_lsn, true));
        }

        /*
         * We take and immediately release the visibility lock. Acquiring the write lock guarantees
         * that any transaction that has written to the log has also made its transaction visible at
         * this time.
         */
        __wt_writelock(session, &txn_global->visibility_rwlock);
        __wt_writeunlock(session, &txn_global->visibility_rwlock);

        /*
         * We need to make sure that the log records in the checkpoint LSN are on disk. In
         * particular to make sure that the current log file exists.
         */
        WT_ERR(__wt_log_force_sync(session, ckpt_lsn));
        break;
    case WT_TXN_LOG_CKPT_START:
        /* Take a copy of the transaction snapshot. */
        txn->ckpt_nsnapshot = txn->snapshot_count;
        recsize = (size_t)txn->ckpt_nsnapshot * WT_INTPACK64_MAXSIZE;
        WT_ERR(__wt_scr_alloc(session, recsize, &txn->ckpt_snapshot));
        end = p = txn->ckpt_snapshot->mem;
        /* There many not be any snapshot entries. */
        if (end != NULL) {
            end += recsize;
            for (i = 0; i < txn->snapshot_count; i++)
                WT_ERR(__wt_vpack_uint(&p, WT_PTRDIFF(end, p), txn->snapshot[i]));
        }
        break;
    case WT_TXN_LOG_CKPT_STOP:
        /*
         * During a clean connection close, we get here without the prepare or start steps. In that
         * case, log the current LSN as the checkpoint LSN.
         */
        if (!txn->full_ckpt) {
            txn->ckpt_nsnapshot = 0;
            WT_CLEAR(empty);
            ckpt_snapshot = &empty;
            WT_ERR(__wt_log_flush_lsn(session, ckpt_lsn, true));
        } else
            ckpt_snapshot = txn->ckpt_snapshot;

        /* Write the checkpoint log record. */
        //对应的printlog信息为"type" : "checkpoint",
        rectype = WT_LOGREC_CHECKPOINT;
        fmt = WT_UNCHECKED_STRING(IIIIu);
        WT_ERR(__wt_struct_size(session, &recsize, fmt, rectype, ckpt_lsn->l.file,
          ckpt_lsn->l.offset, txn->ckpt_nsnapshot, ckpt_snapshot));
        WT_ERR(__wt_logrec_alloc(session, recsize, &logrec));

        WT_ERR(__wt_struct_pack(session, (uint8_t *)logrec->data + logrec->size, recsize, fmt,
          rectype, ckpt_lsn->l.file, ckpt_lsn->l.offset, txn->ckpt_nsnapshot, ckpt_snapshot));
        logrec->size += (uint32_t)recsize;
        WT_ERR(__wt_log_write(
          session, logrec, lsnp, F_ISSET(conn, WT_CONN_CKPT_SYNC) ? WT_LOG_FSYNC : 0));

        /*
         * If this full checkpoint completed successfully and there is no hot backup in progress and
         * this is not an unclean recovery, tell the logging subsystem the checkpoint LSN so that it
         * can remove log files. Do not update the logging checkpoint LSN if this is during a clean
         * connection close, only during a full checkpoint. A clean close may not update any
         * metadata LSN and we do not want to remove log files in that case.
         */
        if (conn->hot_backup_start == 0 &&
          (!FLD_ISSET(conn->log_flags, WT_CONN_LOG_RECOVER_DIRTY) ||
            FLD_ISSET(conn->log_flags, WT_CONN_LOG_FORCE_DOWNGRADE)) &&
          txn->full_ckpt)
            __wt_log_ckpt(session, ckpt_lsn);
        /* FALLTHROUGH */
    case WT_TXN_LOG_CKPT_CLEANUP:
        /* Cleanup any allocated resources */
        WT_INIT_LSN(ckpt_lsn);
        txn->ckpt_nsnapshot = 0;
        __wt_scr_free(session, &txn->ckpt_snapshot);
        txn->full_ckpt = false;
        break;
    default:
        WT_ERR(__wt_illegal_value(session, flags));
    }

err:
    __wt_logrec_free(session, &logrec);
    return (ret);
}

/*
 * __wt_txn_truncate_log --
 *     Begin truncating a range of a file.
 */
int
__wt_txn_truncate_log(
  WT_SESSION_IMPL *session, WT_ITEM *orig_start_key, WT_ITEM *orig_stop_key, bool local_start)
{
    WT_BTREE *btree;
    WT_ITEM *item;
    WT_TXN_OP *op;
    uint64_t start_recno, stop_recno;

    btree = S2BT(session);
    start_recno = WT_RECNO_OOB;
    stop_recno = WT_RECNO_OOB;

    WT_RET(__txn_next_op(session, &op));

    if (btree->type == BTREE_ROW) {
        op->type = WT_TXN_OP_TRUNCATE_ROW;
        op->u.truncate_row.mode = WT_TXN_TRUNC_ALL;
        WT_CLEAR(op->u.truncate_row.start);
        WT_CLEAR(op->u.truncate_row.stop);
        /*
         * If the user provided a start cursor key (i.e. local_start is false) then use the original
         * key provided.
         */
        if (!local_start && orig_start_key != NULL) {
            op->u.truncate_row.mode = WT_TXN_TRUNC_START;
            item = &op->u.truncate_row.start;
            WT_RET(__wt_buf_set(session, item, orig_start_key->data, orig_start_key->size));
        }
        if (orig_stop_key != NULL) {
            op->u.truncate_row.mode =
              (op->u.truncate_row.mode == WT_TXN_TRUNC_ALL) ? WT_TXN_TRUNC_STOP : WT_TXN_TRUNC_BOTH;
            item = &op->u.truncate_row.stop;
            WT_RET(__wt_buf_set(session, item, orig_stop_key->data, orig_stop_key->size));
        }
    } else {
        /*
         * If the user provided cursors, unpack the original keys that were saved in the cursor's
         * lower_bound field.
         */
        if (!local_start && orig_start_key != NULL)
            WT_RET(__wt_struct_unpack(
              session, orig_start_key->data, orig_start_key->size, "q", &start_recno));
        if (orig_stop_key != NULL)
            WT_RET(__wt_struct_unpack(
              session, orig_stop_key->data, orig_stop_key->size, "q", &stop_recno));

        op->type = WT_TXN_OP_TRUNCATE_COL;
        op->u.truncate_col.start = start_recno;
        op->u.truncate_col.stop = stop_recno;
    }

    /* Write that operation into the in-memory log. */
    WT_RET(__wt_txn_log_op(session, NULL));

    WT_ASSERT(session, !F_ISSET(session, WT_SESSION_LOGGING_INMEM));
    F_SET(session, WT_SESSION_LOGGING_INMEM);
    return (0);
}

/*
 * __wt_txn_truncate_end --
 *     Finish truncating a range of a file.
 */
void
__wt_txn_truncate_end(WT_SESSION_IMPL *session)
{
    F_CLR(session, WT_SESSION_LOGGING_INMEM);
}

/*
 * __txn_printlog --
 *     Print a log record in a human-readable format.
 打印一条log
 ./wt printlog -u
 ../../../wt printlog -ux
 */

//打印lsnp这条log
static int
__txn_printlog(WT_SESSION_IMPL *session, 
    //log内容保持在rawrec中
    WT_ITEM *rawrec, 
    //这条log的序号信息，代表在对应文件中的位置
    WT_LSN *lsnp, WT_LSN *next_lsnp,
  void *cookie, int firstrecord)
{
    WT_LOG_RECORD *logrec;
    WT_TXN_PRINTLOG_ARGS *args;
    uint64_t txnid;
    uint32_t fileid, lsnfile, lsnoffset, rectype;
    int32_t start;
    const uint8_t *end, *p;
    const char *msg;
    bool compressed;

    WT_UNUSED(next_lsnp);
    args = cookie;

    p = WT_LOG_SKIP_HEADER(rawrec->data);
    end = (const uint8_t *)rawrec->data + rawrec->size;
    logrec = (WT_LOG_RECORD *)rawrec->data;
    compressed = F_ISSET(logrec, WT_LOG_RECORD_COMPRESSED);

    /* First, peek at the log record type. */
    //读取log header以外的log内容，解包获取rectype
    WT_RET(__wt_logrec_read(session, &p, end, &rectype));

    /*
     * When printing just the message records, display the message by itself without the usual log
     * header information.
     */
    // ./wt printlog -m 只获取message信息
    if (F_ISSET(args, WT_TXN_PRINTLOG_MSG)) {
        if (rectype != WT_LOGREC_MESSAGE)
            return (0);
            
        WT_RET(__wt_struct_unpack(session, p, WT_PTRDIFF(end, p), WT_UNCHECKED_STRING(S), &msg));
        return (__wt_fprintf(session, args->fs, "%s\n", msg));
    }

    if (!firstrecord)
        WT_RET(__wt_fprintf(session, args->fs, ",\n"));

    WT_RET(__wt_fprintf(session, args->fs, "  { \"lsn\" : [%" PRIu32 ",%" PRIu32 "],\n",
      lsnp->l.file, lsnp->l.offset));
    WT_RET(__wt_fprintf(
      session, args->fs, "    \"hdr_flags\" : \"%s\",\n", compressed ? "compressed" : ""));
    //lsnp->l.file + lsnp->l.offset确定log在文件的起始位置，logrec->len确定整个log的内容
    WT_RET(__wt_fprintf(session, args->fs, "    \"rec_len\" : %" PRIu32 ",\n", logrec->len));
    WT_RET(__wt_fprintf(session, args->fs, "    \"mem_len\" : %" PRIu32 ",\n",
      compressed ? logrec->mem_len : logrec->len));

    switch (rectype) {
    case WT_LOGREC_CHECKPOINT:
        //配合__wt_txn_checkpoint_log阅读
        WT_RET(__wt_struct_unpack(
          session, p, WT_PTRDIFF(end, p), WT_UNCHECKED_STRING(II), &lsnfile, &lsnoffset));
        WT_RET(__wt_fprintf(session, args->fs, "    \"type\" : \"checkpoint\",\n"));
        WT_RET(__wt_fprintf(
          session, args->fs, "    \"ckpt_lsn\" : [%" PRIu32 ",%" PRIu32 "]\n", lsnfile, lsnoffset));
        break;

    case WT_LOGREC_COMMIT:
        WT_RET(__wt_vunpack_uint(&p, WT_PTRDIFF(end, p), &txnid));
        WT_RET(__wt_fprintf(session, args->fs, "    \"type\" : \"commit\",\n"));
        //赋值来源参考__txn_logrec_init，同一个session的txnid是自增的
        WT_RET(__wt_fprintf(session, args->fs, "    \"txnid\" : %" PRIu64 ",\n", txnid));
        WT_RET(__txn_oplist_printlog(session, &p, end, args));
        break;

    case WT_LOGREC_FILE_SYNC:
        WT_RET(__wt_struct_unpack(
          session, p, WT_PTRDIFF(end, p), WT_UNCHECKED_STRING(Ii), &fileid, &start));
        WT_RET(__wt_fprintf(session, args->fs, "    \"type\" : \"file_sync\",\n"));
        WT_RET(__wt_fprintf(session, args->fs, "    \"fileid\" : %" PRIu32 ",\n", fileid));
        WT_RET(__wt_fprintf(session, args->fs, "    \"start\" : %" PRId32 "\n", start));
        break;

    case WT_LOGREC_MESSAGE:
        WT_RET(__wt_struct_unpack(session, p, WT_PTRDIFF(end, p), WT_UNCHECKED_STRING(S), &msg));
        WT_RET(__wt_fprintf(session, args->fs, "    \"type\" : \"message\",\n"));
        WT_RET(__wt_fprintf(session, args->fs, "    \"message\" : \"%s\"\n", msg));
        break;

    case WT_LOGREC_SYSTEM:
        WT_RET(__wt_struct_unpack(
          session, p, WT_PTRDIFF(end, p), WT_UNCHECKED_STRING(II), &lsnfile, &lsnoffset));
        WT_RET(__wt_fprintf(session, args->fs, "    \"type\" : \"system\",\n"));
        WT_RET(__txn_oplist_printlog(session, &p, end, args));
        break;
    }

    WT_RET(__wt_fprintf(session, args->fs, "  }"));

    return (0);
}

/*
 * __wt_txn_printlog --
 *     Print the log in a human-readable format.
 ../../../wt printlog -u
 */
int
__wt_txn_printlog(WT_SESSION *wt_session, const char *ofile, uint32_t flags, WT_LSN *start_lsn,
  WT_LSN *end_lsn) //WT_GCC_FUNC_ATTRIBUTE((visibility("default")))
{
    WT_DECL_RET;
    WT_FSTREAM *fs;
    WT_SESSION_IMPL *session;
    WT_TXN_PRINTLOG_ARGS args;

    session = (WT_SESSION_IMPL *)wt_session;
    if (ofile == NULL)
        fs = WT_STDOUT(session);
    else
        WT_RET(
          __wt_fopen(session, ofile, WT_FS_OPEN_CREATE | WT_FS_OPEN_FIXED, WT_STREAM_WRITE, &fs));

    if (!LF_ISSET(WT_TXN_PRINTLOG_MSG))
        WT_ERR(__wt_fprintf(session, fs, "[\n"));
    //解析出的日志记录到fs这个文件中
    args.fs = fs;
    args.flags = flags;
    WT_ERR(__wt_log_scan(session, start_lsn, end_lsn, 0x0, __txn_printlog, &args));
    if (!LF_ISSET(WT_TXN_PRINTLOG_MSG))
        ret = __wt_fprintf(session, fs, "\n]\n");

err:
    if (ofile != NULL)
        WT_TRET(__wt_fclose(session, &fs));

    return (ret);
}
