/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */
//WT_SESSION_IMPL.compact为该结构，参考__wt_session_compact
struct __wt_compact_state {
    uint32_t lsm_count;  /* Number of LSM trees seen */
    uint32_t file_count; /* Number of files seen */
    uint64_t max_time;   /* Configured timeout */

    struct timespec begin; /* Starting time */
};
