/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#ifndef __WT_INTERNAL_H
#define __WT_INTERNAL_H

#if defined(__cplusplus)
extern "C" {
#endif

/*******************************************
 * WiredTiger public include file, and configuration control.
 *******************************************/
#include "wiredtiger_config.h"
#include "wiredtiger_ext.h"

/*******************************************
 * WiredTiger system include files.
 *******************************************/
#ifndef _WIN32
#include <sys/mman.h>
#endif
#include <sys/stat.h>
#ifndef _WIN32
#include <sys/time.h>
#include <sys/uio.h>
#endif
#ifndef _WIN32
#include <dlfcn.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <inttypes.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <limits.h>
#ifdef _WIN32
#include <process.h>
#else
#include <pthread.h>
#endif
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <time.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

/*
 * DO NOT EDIT: automatically built by dist/s_typedef.
 * Forward type declarations for internal types: BEGIN
 */
struct __wt_addr;
typedef struct __wt_addr WT_ADDR;
struct __wt_addr_copy;
typedef struct __wt_addr_copy WT_ADDR_COPY;
struct __wt_backup_target;
typedef struct __wt_backup_target WT_BACKUP_TARGET;
struct __wt_blkcache;
typedef struct __wt_blkcache WT_BLKCACHE;
struct __wt_blkcache_item;
typedef struct __wt_blkcache_item WT_BLKCACHE_ITEM;
struct __wt_blkincr;
typedef struct __wt_blkincr WT_BLKINCR;

struct __wt_block;  
//����ռ�͸�ֵ��__wt_block_open��__wt_bm.blockΪ������
typedef struct __wt_block WT_BLOCK;
struct __wt_block_ckpt;
//__wt_block.liveΪ������
typedef struct __wt_block_ckpt WT_BLOCK_CKPT;
struct __wt_block_desc;
typedef struct __wt_block_desc WT_BLOCK_DESC;
struct __wt_block_header;
typedef struct __wt_block_header WT_BLOCK_HEADER;
struct __wt_block_mods;
typedef struct __wt_block_mods WT_BLOCK_MODS;
struct __wt_bloom;
typedef struct __wt_bloom WT_BLOOM;
struct __wt_bloom_hash;
typedef struct __wt_bloom_hash WT_BLOOM_HASH;
struct __wt_bm;
//__wt_btree_open->__wt_blkcache_open��Ƭ�ռ䣬__bm_method_set�н��нӿڶ���
//__wt_btree.bmΪ������
typedef struct __wt_bm WT_BM;
struct __wt_btree;
//btree�ڴ����__wt_conn_dhandle_alloc  //�洢��WT_DATA_HANDLE.handle��Ա
typedef struct __wt_btree WT_BTREE;
struct __wt_bucket_storage;
typedef struct __wt_bucket_storage WT_BUCKET_STORAGE;
struct __wt_cache;
//Internally, WiredTiger's cache state is represented by the WT_CACHE structure, which contains counters and parameter settings for tracking cache usage and controlling eviction policy. The WT_CACHE also includes state WiredTiger uses to track the progress of eviction. There is a single WT_CACHE for each connection, accessed via the WT_CONNECTION_IMPL structure.
typedef struct __wt_cache WT_CACHE; //ÿ��connection��Ӧһ��WT_CACHE(__wt_cache)��WT_CONNECTION_IMPL(__wt_connection_impl)
struct __wt_cache_pool;
//__wt_process.cache_pool�������ȫ�ֱ�����Ա��
typedef struct __wt_cache_pool WT_CACHE_POOL;
struct __wt_capacity;
//__wt_connection_impl.capacityΪ������  __wt_capacity_throttle��ʹ��
typedef struct __wt_capacity WT_CAPACITY;
struct __wt_cell;
typedef struct __wt_cell WT_CELL;
struct __wt_cell_unpack_addr;
typedef struct __wt_cell_unpack_addr WT_CELL_UNPACK_ADDR;
struct __wt_cell_unpack_common;
typedef struct __wt_cell_unpack_common WT_CELL_UNPACK_COMMON;
struct __wt_cell_unpack_kv;
typedef struct __wt_cell_unpack_kv WT_CELL_UNPACK_KV;
struct __wt_ckpt;
typedef struct __wt_ckpt WT_CKPT;
struct __wt_ckpt_snapshot;
typedef struct __wt_ckpt_snapshot WT_CKPT_SNAPSHOT;
struct __wt_col;
typedef struct __wt_col WT_COL;
struct __wt_col_fix_auxiliary_header;
typedef struct __wt_col_fix_auxiliary_header WT_COL_FIX_AUXILIARY_HEADER;
struct __wt_col_fix_tw;
typedef struct __wt_col_fix_tw WT_COL_FIX_TW;
struct __wt_col_fix_tw_entry;
typedef struct __wt_col_fix_tw_entry WT_COL_FIX_TW_ENTRY;
struct __wt_col_rle;
typedef struct __wt_col_rle WT_COL_RLE;
struct __wt_col_var_repeat;
typedef struct __wt_col_var_repeat WT_COL_VAR_REPEAT;
struct __wt_colgroup;
typedef struct __wt_colgroup WT_COLGROUP;
struct __wt_compact_state;
typedef struct __wt_compact_state WT_COMPACT_STATE;
struct __wt_condvar;
typedef struct __wt_condvar WT_CONDVAR;
struct __wt_config;
typedef struct __wt_config WT_CONFIG;
struct __wt_config_check;
//ÿ���������һЩ���Ƽ��
typedef struct __wt_config_check WT_CONFIG_CHECK;
struct __wt_config_entry;
//config_entries[]����Ϊ������
typedef struct __wt_config_entry WT_CONFIG_ENTRY;
struct __wt_config_parser_impl;
typedef struct __wt_config_parser_impl WT_CONFIG_PARSER_IMPL;
struct __wt_connection_impl;
//ÿ��connection��Ӧһ��WT_CACHE(__wt_cache)��WT_CONNECTION_IMPL(__wt_connection_impl)
typedef struct __wt_connection_impl WT_CONNECTION_IMPL;
struct __wt_connection_stats;
typedef struct __wt_connection_stats WT_CONNECTION_STATS;
struct __wt_cursor_backup;
typedef struct __wt_cursor_backup WT_CURSOR_BACKUP;
struct __wt_cursor_bounds_state;
typedef struct __wt_cursor_bounds_state WT_CURSOR_BOUNDS_STATE;
struct __wt_cursor_btree;
typedef struct __wt_cursor_btree WT_CURSOR_BTREE;
struct __wt_cursor_bulk;
typedef struct __wt_cursor_bulk WT_CURSOR_BULK;
struct __wt_cursor_config;
typedef struct __wt_cursor_config WT_CURSOR_CONFIG;
struct __wt_cursor_data_source;
typedef struct __wt_cursor_data_source WT_CURSOR_DATA_SOURCE;
struct __wt_cursor_dump;
typedef struct __wt_cursor_dump WT_CURSOR_DUMP;
struct __wt_cursor_hs;
typedef struct __wt_cursor_hs WT_CURSOR_HS;
struct __wt_cursor_index;
typedef struct __wt_cursor_index WT_CURSOR_INDEX;
struct __wt_cursor_join;
typedef struct __wt_cursor_join WT_CURSOR_JOIN;
struct __wt_cursor_join_endpoint;
typedef struct __wt_cursor_join_endpoint WT_CURSOR_JOIN_ENDPOINT;
struct __wt_cursor_join_entry;
typedef struct __wt_cursor_join_entry WT_CURSOR_JOIN_ENTRY;
struct __wt_cursor_join_iter;
typedef struct __wt_cursor_join_iter WT_CURSOR_JOIN_ITER;
struct __wt_cursor_json;
typedef struct __wt_cursor_json WT_CURSOR_JSON;
struct __wt_cursor_log;
typedef struct __wt_cursor_log WT_CURSOR_LOG;
struct __wt_cursor_lsm;
typedef struct __wt_cursor_lsm WT_CURSOR_LSM;
struct __wt_cursor_metadata;
typedef struct __wt_cursor_metadata WT_CURSOR_METADATA;
struct __wt_cursor_stat;
typedef struct __wt_cursor_stat WT_CURSOR_STAT;
struct __wt_cursor_table;
typedef struct __wt_cursor_table WT_CURSOR_TABLE;
struct __wt_cursor_version;
typedef struct __wt_cursor_version WT_CURSOR_VERSION;
struct __wt_data_handle;
//һ��__wt_data_handleʵ���϶�Ӧһ��BTREE��ͨ��BTREE btree = (WT_BTREE *)dhandle->handle;��ȡ
//__wt_conn_dhandle_alloc�з���ڵ��ڴ�    һ��__wt_data_handleʵ���϶�Ӧһ��BTREE��ͨ��BTREE btree = (WT_BTREE *)dhandle->handle;��ȡ
typedef struct __wt_data_handle WT_DATA_HANDLE;
struct __wt_data_handle_cache;
typedef struct __wt_data_handle_cache WT_DATA_HANDLE_CACHE;
struct __wt_delete_hs_upd;
typedef struct __wt_delete_hs_upd WT_DELETE_HS_UPD;
struct __wt_dlh;
typedef struct __wt_dlh WT_DLH;
struct __wt_dsrc_stats;
typedef struct __wt_dsrc_stats WT_DSRC_STATS;
struct __wt_evict_entry;
typedef struct __wt_evict_entry WT_EVICT_ENTRY;
struct __wt_evict_queue;
typedef struct __wt_evict_queue WT_EVICT_QUEUE;
struct __wt_ext;
typedef struct __wt_ext WT_EXT;
struct __wt_extlist;
//__wt_block_ckpt��alloc avail discardΪ������
typedef struct __wt_extlist WT_EXTLIST;
struct __wt_fh;
typedef struct __wt_fh WT_FH;
struct __wt_file_handle_inmem;
typedef struct __wt_file_handle_inmem WT_FILE_HANDLE_INMEM;
struct __wt_file_handle_posix;
typedef struct __wt_file_handle_posix WT_FILE_HANDLE_POSIX;
struct __wt_file_handle_win;
typedef struct __wt_file_handle_win WT_FILE_HANDLE_WIN;
struct __wt_fstream;
typedef struct __wt_fstream WT_FSTREAM;
struct __wt_hazard;
//__wt_hazard_set_func
typedef struct __wt_hazard WT_HAZARD;
struct __wt_ikey;
typedef struct __wt_ikey WT_IKEY;
struct __wt_import_entry;
typedef struct __wt_import_entry WT_IMPORT_ENTRY;
struct __wt_import_list;
typedef struct __wt_import_list WT_IMPORT_LIST;
struct __wt_index;
typedef struct __wt_index WT_INDEX;
struct __wt_insert;
//__wt_row_insert_alloc  //WT_INSERTͷ��+level�ռ�+��ʵ����key
//KV�е�key��ӦWT_INSERT��value��ӦWT_UPDATE
typedef struct __wt_insert WT_INSERT;
struct __wt_insert_head;
//WT_PAGE_ALLOC_AND_SWAP __wt_leaf_page_can_split����ռ�
typedef struct __wt_insert_head WT_INSERT_HEAD;
struct __wt_join_stats;
typedef struct __wt_join_stats WT_JOIN_STATS;
struct __wt_join_stats_group;
typedef struct __wt_join_stats_group WT_JOIN_STATS_GROUP;
struct __wt_keyed_encryptor;
typedef struct __wt_keyed_encryptor WT_KEYED_ENCRYPTOR;
struct __wt_log;
typedef struct __wt_log WT_LOG;
struct __wt_log_desc;
typedef struct __wt_log_desc WT_LOG_DESC;
struct __wt_log_op_desc;
typedef struct __wt_log_op_desc WT_LOG_OP_DESC;
struct __wt_log_rec_desc;
typedef struct __wt_log_rec_desc WT_LOG_REC_DESC;
struct __wt_log_record;
typedef struct __wt_log_record WT_LOG_RECORD;
struct __wt_logslot;
typedef struct __wt_logslot WT_LOGSLOT;
struct __wt_lsm_chunk;
typedef struct __wt_lsm_chunk WT_LSM_CHUNK;
struct __wt_lsm_cursor_chunk;
typedef struct __wt_lsm_cursor_chunk WT_LSM_CURSOR_CHUNK;
struct __wt_lsm_data_source;
typedef struct __wt_lsm_data_source WT_LSM_DATA_SOURCE;
struct __wt_lsm_manager;
typedef struct __wt_lsm_manager WT_LSM_MANAGER;
struct __wt_lsm_tree;
typedef struct __wt_lsm_tree WT_LSM_TREE;
struct __wt_lsm_work_unit;
typedef struct __wt_lsm_work_unit WT_LSM_WORK_UNIT;
struct __wt_lsm_worker_args;
typedef struct __wt_lsm_worker_args WT_LSM_WORKER_ARGS;
struct __wt_lsm_worker_cookie;
typedef struct __wt_lsm_worker_cookie WT_LSM_WORKER_COOKIE;
struct __wt_multi;
//һ��page���Ϊ���pageʱ���õ������Բο�__rec_split_dump_keys�ı���,__rec_split_write���ﴴ���ռ�͸�ֵ
typedef struct __wt_multi WT_MULTI;
struct __wt_myslot;
typedef struct __wt_myslot WT_MYSLOT;
struct __wt_name_flag;
//�ο�__wt_verbose_config
typedef struct __wt_name_flag WT_NAME_FLAG;
struct __wt_named_collator;
typedef struct __wt_named_collator WT_NAMED_COLLATOR;
struct __wt_named_compressor;
typedef struct __wt_named_compressor WT_NAMED_COMPRESSOR;
struct __wt_named_data_source;
typedef struct __wt_named_data_source WT_NAMED_DATA_SOURCE;
struct __wt_named_encryptor;
typedef struct __wt_named_encryptor WT_NAMED_ENCRYPTOR;
struct __wt_named_extractor;
typedef struct __wt_named_extractor WT_NAMED_EXTRACTOR;
struct __wt_named_storage_source;
typedef struct __wt_named_storage_source WT_NAMED_STORAGE_SOURCE;
struct __wt_optrack_header;
typedef struct __wt_optrack_header WT_OPTRACK_HEADER;
struct __wt_optrack_record;
typedef struct __wt_optrack_record WT_OPTRACK_RECORD;
struct __wt_ovfl_reuse;
typedef struct __wt_ovfl_reuse WT_OVFL_REUSE;
struct __wt_ovfl_track;
typedef struct __wt_ovfl_track WT_OVFL_TRACK;
struct __wt_page;
//__wt_page_alloc
typedef struct __wt_page WT_PAGE;
struct __wt_page_deleted;
typedef struct __wt_page_deleted WT_PAGE_DELETED;
struct __wt_page_header;
typedef struct __wt_page_header WT_PAGE_HEADER;
struct __wt_page_index;
//https://github.com/wiredtiger/wiredtiger/wiki/In-Memory-Tree-Layout
//__wt_page_alloc����ռ䣬ͨ��WT_INTL_INDEX_GET(session, page, pindex);��ȡpage��Ӧ��__wt_page_index
typedef struct __wt_page_index WT_PAGE_INDEX;
struct __wt_page_modify;
//__wt_page.modify ��ֵ��__wt_page_modify_init  //����leaf page����Ծ���нڵ����ݶ��ڸ�modify��
typedef struct __wt_page_modify WT_PAGE_MODIFY;
struct __wt_process;
typedef struct __wt_process WT_PROCESS;
struct __wt_rec_chunk;
//__wt_reconcile.chunk_A chunk_B cur_ptrΪ������
//__rec_split_chunk_init�г�ʼ��
typedef struct __wt_rec_chunk WT_REC_CHUNK;
struct __wt_rec_dictionary;
typedef struct __wt_rec_dictionary WT_REC_DICTIONARY;
struct __wt_rec_kv;
typedef struct __wt_rec_kv WT_REC_KV;
struct __wt_reconcile;
//WT_SESSION_IMPL.reconcileΪ������  __rec_init��Ƭ�ռ�
typedef struct __wt_reconcile WT_RECONCILE;
struct __wt_ref;
//btree��Ӧroot page,��ֵ�ο�__btree_tree_open_empty
typedef struct __wt_ref WT_REF;
struct __wt_ref_hist;
typedef struct __wt_ref_hist WT_REF_HIST;
struct __wt_row;
typedef struct __wt_row WT_ROW;
struct __wt_rwlock;
typedef struct __wt_rwlock WT_RWLOCK;
struct __wt_salvage_cookie;
typedef struct __wt_salvage_cookie WT_SALVAGE_COOKIE;
struct __wt_save_upd;
typedef struct __wt_save_upd WT_SAVE_UPD;
struct __wt_scratch_track;
typedef struct __wt_scratch_track WT_SCRATCH_TRACK;
struct __wt_session_impl;
//__open_session�л�ȡsession //session�ӿڣ���ֵ�ο�__open_session
typedef struct __wt_session_impl WT_SESSION_IMPL;
struct __wt_session_stash;
typedef struct __wt_session_stash WT_SESSION_STASH;
struct __wt_session_stats;
typedef struct __wt_session_stats WT_SESSION_STATS;
struct __wt_size;
typedef struct __wt_size WT_SIZE;
struct __wt_spinlock;
typedef struct __wt_spinlock WT_SPINLOCK;
struct __wt_stash;
typedef struct __wt_stash WT_STASH;
struct __wt_table;
typedef struct __wt_table WT_TABLE;
struct __wt_thread;
typedef struct __wt_thread WT_THREAD;
struct __wt_thread_group;
typedef struct __wt_thread_group WT_THREAD_GROUP;
struct __wt_tiered;
typedef struct __wt_tiered WT_TIERED;
struct __wt_tiered_object;
typedef struct __wt_tiered_object WT_TIERED_OBJECT;
struct __wt_tiered_tiers;
typedef struct __wt_tiered_tiers WT_TIERED_TIERS;
struct __wt_tiered_tree;
typedef struct __wt_tiered_tree WT_TIERED_TREE;
struct __wt_tiered_work_unit;
typedef struct __wt_tiered_work_unit WT_TIERED_WORK_UNIT;
struct __wt_time_aggregate;
typedef struct __wt_time_aggregate WT_TIME_AGGREGATE;
struct __wt_time_window;
typedef struct __wt_time_window WT_TIME_WINDOW;
struct __wt_txn;
typedef struct __wt_txn WT_TXN;
struct __wt_txn_global;
typedef struct __wt_txn_global WT_TXN_GLOBAL;
struct __wt_txn_op;
typedef struct __wt_txn_op WT_TXN_OP;
struct __wt_txn_printlog_args;
typedef struct __wt_txn_printlog_args WT_TXN_PRINTLOG_ARGS;
struct __wt_txn_shared;
typedef struct __wt_txn_shared WT_TXN_SHARED;
struct __wt_update;
//����__wt_upd_alloc�ռ� //KV�е�key��ӦWT_INSERT��value��ӦWT_UPDATE
//__wt_insert.updΪ������
typedef struct __wt_update WT_UPDATE;
struct __wt_update_value;
//__wt_cursor_btree.modify_updateΪ������
typedef struct __wt_update_value WT_UPDATE_VALUE;
struct __wt_update_vector;
typedef struct __wt_update_vector WT_UPDATE_VECTOR;
struct __wt_verbose_multi_category;
typedef struct __wt_verbose_multi_category WT_VERBOSE_MULTI_CATEGORY;
struct __wt_verify_info;
typedef struct __wt_verify_info WT_VERIFY_INFO;
struct __wt_version;
typedef struct __wt_version WT_VERSION;
union __wt_lsn;
typedef union __wt_lsn WT_LSN;
union __wt_rand_state;
typedef union __wt_rand_state WT_RAND_STATE;

typedef uint64_t wt_timestamp_t;

/*
 * Forward type declarations for internal types: END
 * DO NOT EDIT: automatically built by dist/s_typedef.
 */

/*******************************************
 * WiredTiger internal include files.
 *******************************************/
#if defined(__GNUC__)
#include "gcc.h"
#elif defined(_MSC_VER)
#include "msvc.h"
#endif
/*
 * GLIBC 2.26 and later use the openat syscall to implement open. Set this flag so that our strace
 * tests know to expect this.
 */
#ifdef __GLIBC_PREREQ
#if __GLIBC_PREREQ(2, 26)
#define WT_USE_OPENAT 1
#endif
#endif

#include "hardware.h"
#include "swap.h"

#include "queue.h"

#ifdef _WIN32
#include "os_windows.h"
#else
#include "posix.h"
#endif

#include "misc.h"
#include "mutex.h"

#include "stat.h"      /* required by dhandle.h */
#include "dhandle.h"   /* required by btree.h */
#include "timestamp.h" /* required by reconcile.h */

#include "api.h"
#include "block.h"
#include "block_cache.h"
#include "bloom.h"
#include "btmem.h"
#include "btree.h"
#include "cache.h"
#include "capacity.h"
#include "cell.h"
#include "compact.h"
#include "config.h"
#include "cursor.h"
#include "dlh.h"
#include "error.h"
#include "log.h"
#include "lsm.h"
#include "meta.h" /* required by block.h */
#include "optrack.h"
#include "os.h"
#include "reconcile.h"
#include "schema.h"
#include "thread_group.h"
#include "tiered.h"
#include "txn.h"
#include "verbose.h"

#include "session.h" /* required by connection.h */
#include "version.h" /* required by connection.h */
#include "connection.h"

#include "extern.h"
#ifdef _WIN32
#include "extern_win.h"
#else
#include "extern_posix.h"
#endif
#include "verify_build.h"

#include "cache_inline.h"   /* required by misc_inline.h */
#include "ctype_inline.h"   /* required by packing_inline.h */
#include "intpack_inline.h" /* required by cell_inline.h, packing_inline.h */
#include "misc_inline.h"    /* required by mutex_inline.h */

#include "buf_inline.h"       /* required by cell_inline.h */
#include "timestamp_inline.h" /* required by btree_inline.h */
#include "cell_inline.h"      /* required by btree_inline.h */
#include "mutex_inline.h"     /* required by btree_inline.h */
#include "txn_inline.h"       /* required by btree_inline.h */

#include "bitstring_inline.h"
#include "block_inline.h"
#include "btree_inline.h" /* required by cursor_inline.h */
#include "btree_cmp_inline.h"
#include "column_inline.h"
#include "cursor_inline.h"
#include "log_inline.h"
#include "os_fhandle_inline.h"
#include "os_fs_inline.h"
#include "os_fstream_inline.h"
#include "packing_inline.h"
#include "reconcile_inline.h"
#include "serial_inline.h"
#include "time_inline.h"

#ifdef HAVE_DIAGNOSTIC  

typedef struct __wt_dbg WT_DBG;
struct __wt_dbg {
    WT_CURSOR *hs_cursor;
    WT_SESSION_IMPL *session; /* Enclosing session */

    WT_ITEM *key;

    WT_ITEM *hs_key; /* History store lookups */
    WT_ITEM *hs_value;

    /*
     * When using the standard event handlers, the debugging output has to do its own message
     * handling because its output isn't line-oriented.
     */
    FILE *fp;     /* Optional file handle */
    WT_ITEM *msg; /* Buffered message */

    int (*f)(WT_DBG *, const char *, ...) /* Function to write */
      WT_GCC_FUNC_DECL_ATTRIBUTE((format(printf, 2, 3)));

    const char *key_format;
    const char *value_format;

    WT_ITEM *t1, *t2; /* Temporary space */
};

 int __debug_item_key(WT_DBG *ds, const char *tag, const void *data_arg, size_t size);
 int __debug_col_skip(WT_DBG *, WT_INSERT_HEAD *, const char *, bool, WT_CURSOR *);
 int __debug_config(WT_SESSION_IMPL *, WT_DBG *, const char *);
 int __debug_modify(WT_DBG *, const uint8_t *);
 int __debug_page(WT_DBG *, WT_REF *, uint32_t);
 int __debug_page_col_fix(WT_DBG *, WT_REF *);
 int __debug_page_col_int(WT_DBG *, WT_PAGE *, uint32_t);
 int __debug_page_col_var(WT_DBG *, WT_REF *);
 int __debug_page_metadata(WT_DBG *, WT_REF *);
 int __debug_page_row_int(WT_DBG *, WT_PAGE *, uint32_t);
 int __debug_page_row_leaf(WT_DBG *, WT_PAGE *);
 int __debug_ref(WT_DBG *, WT_REF *);
 int __debug_row_skip(WT_DBG *, WT_INSERT_HEAD *);
 int __debug_tree(WT_SESSION_IMPL *, WT_REF *, const char *, uint32_t);
 int __debug_update(WT_DBG *, WT_UPDATE *, bool);
 int __debug_wrapup(WT_DBG *);

#endif


#if defined(__cplusplus)
}
#endif
#endif /* !__WT_INTERNAL_H */
