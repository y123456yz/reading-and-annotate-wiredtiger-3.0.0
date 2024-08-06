/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

/*
 * We format timestamps in a couple of ways, declare appropriate sized buffers. Hexadecimal is 2x
 * the size of the value. MongoDB format (high/low pairs of 4B unsigned integers, with surrounding
 * parenthesis and separating comma and space), is 2x the maximum digits from a 4B unsigned integer
 * plus 4. Both sizes include a trailing null byte as well.
 */
#define WT_TS_HEX_STRING_SIZE (2 * sizeof(wt_timestamp_t) + 1)
#define WT_TS_INT_STRING_SIZE (2 * 10 + 4 + 1)

/*
 * We need an appropriately sized buffer for formatted time points, aggregates and windows. This is
 * for time windows with 4 timestamps, 2 transaction IDs, prepare state and formatting. The
 * formatting is currently about 32 characters - enough space that we don't need to think about it.
 */
#define WT_TP_STRING_SIZE (WT_TS_INT_STRING_SIZE * 2 + 1 + 20 + 1)
#define WT_TIME_STRING_SIZE (WT_TS_INT_STRING_SIZE * 4 + 20 * 2 + 64)

/* The time points that define a value's time window and associated prepare information. */
//官方文档参考: https://source.wiredtiger.com/develop/arch-timestamp.html
//事务相关，例如可见性，可以参考__wt_txn_tw_start_visible_all  __wt_txn_tw_stop_visible __wt_txn_tw_start_visible_all
//tw赋值给V的地方参考__wt_rec_cell_build_val, 最终通过__wt_rec_image_copy拷贝给V的tw然后通过reconcile逻辑写入磁盘

//__wt_rec_cell_build_val:  生成KV中的V(含WT_TIME_WINDOW)，最终通过__wt_rec_image_copy及reconcile流程写入磁盘
//__wt_row_leaf_value_cell: 从磁盘读取KV中的V(含WT_TIME_WINDOW)

//__wt_update_value.wt为该类型，真正的赋值来源见__rec_fill_tw_from_upd_select，最终在__rec_row_leaf_insert  __wt_rec_row_leaf通
// 过WT_TIME_AGGREGATE_UPDATE赋值给r后reconcile持久化到磁盘
struct __wt_time_window {
    //注意: __wt_rec_time_window_clear_obsolete中可能重新置为WT_TXN_NONE
    //WT_TIME_WINDOW_SET_START
    wt_timestamp_t durable_start_ts; /* default value: WT_TS_NONE */
    wt_timestamp_t start_ts;         /* default value: WT_TS_NONE */
    uint64_t start_txn;              /* default value: WT_TXN_NONE */

    //stop窗口相关赋值见__wt_upd_value_assign  __wt_txn_read_upd_list_internal  
    //WT_TIME_WINDOW_SET_STOP
    wt_timestamp_t durable_stop_ts; /* default value: WT_TS_NONE */
    wt_timestamp_t stop_ts;         /* default value: WT_TS_MAX */
    uint64_t stop_txn;              /* default value: WT_TXN_MAX */

    /*
     * Prepare information isn't really part of a time window, but we need to aggregate it to the
     * internal page information in reconciliation, and this is the simplest place to put it.
     */
    uint8_t prepare;
};

/*
 * The time points that define an aggregated time window and associated prepare information.
 *
 * - newest_start_durable_ts - Newest valid start durable/commit timestamp
 * - newest_stop_durable_ts  - Newest valid stop durable/commit timestamp doesn't include WT_TS_MAX
 * - oldest_start_ts         - Oldest start commit timestamp
 * - newest_txn              - Newest valid start/stop commit transaction doesn't include
 *                             WT_TXN_MAX
 * - newest_stop_ts          - Newest stop commit timestamp include WT_TS_MAX
 * - newest_stop_txn         - Newest stop commit transaction include WT_TXN_MAX
 * - prepare                 - Prepared updates
 官方说明参考: https://source.wiredtiger.com/develop/arch-timestamp.html
 */
//赋值参考WT_TIME_AGGREGATE_UPDATE
struct __wt_time_aggregate {
    //真正赋值的地方参考WT_TIME_AGGREGATE_UPDATE
    wt_timestamp_t newest_start_durable_ts; /* default value: WT_TS_NONE */
    //真正赋值的地方参考WT_TIME_AGGREGATE_UPDATE
    wt_timestamp_t newest_stop_durable_ts;  /* default value: WT_TS_NONE */

    //真正赋值的地方参考WT_TIME_AGGREGATE_UPDATE
    wt_timestamp_t oldest_start_ts; /* default value: WT_TS_NONE */
    //真正赋值的地方参考WT_TIME_AGGREGATE_UPDATE
    uint64_t newest_txn;            /* default value: WT_TXN_NONE */
    wt_timestamp_t newest_stop_ts;  /* default value: WT_TS_MAX */
    uint64_t newest_stop_txn;       /* default value: WT_TXN_MAX */

    uint8_t prepare;

    uint8_t init_merge; /* Initialized for aggregation and merge */
};
