/*-
 * Public Domain 2014-present MongoDB, Inc.
 * Public Domain 2008-2014 WiredTiger, Inc.
 *
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * ex_access.c
 * 	demonstrates how to create and access a simple table.
 */
#include <test_util.h>

static const char *home;

static int
cursor_search(WT_CURSOR *cursor)
{
    WT_ITEM value_item;
    const char *p = "12345\0\0\0";

    return 0;

    cursor->set_key(cursor, 15);

    error_check(cursor->search(cursor));
    error_check(cursor->get_value(cursor, &value_item));

    printf("Got cursor_search: %d  %d : %s\n", (int)sizeof(p), 15, (char*)value_item.data);

    return (0);
}


static void
print_cursor_for_access(WT_CURSOR *cursor)
{
    const char *desc, *pvalue;
    int64_t value;
    int ret;

    //pvalue和value值是一样的，至少一个是字符串，一个是对应数字
    while ((ret = cursor->next(cursor)) == 0) {
        error_check(cursor->get_value(cursor, &desc, &pvalue, &value));
        if (value != 0)
            printf("%s=%s\n", desc, pvalue);
    }
    scan_end_check(ret == WT_NOTFOUND);
}

static void
print_session_stats_for_access(WT_SESSION *session)
{
    WT_CURSOR *stat_cursor;

    /*! [statistics session function] */
    //error_check(session->open_cursor(session, "statistics:session", NULL, NULL, &stat_cursor));
    error_check(session->open_cursor(session, "statistics:session", NULL, "statistics=(fast)", &stat_cursor));

    print_cursor_for_access(stat_cursor);
    stat_cursor->reset(stat_cursor);//yang add todo xxxxxxxx   加这个就不会自己去做减法了
    error_check(stat_cursor->close(stat_cursor));
    /*! [statistics session function] */
}

static void
cursor_count_items(WT_CURSOR *cursor, uint64_t *countp)
{
    WT_DECL_RET;

    *countp = 0;

    testutil_check(cursor->reset(cursor));
    while ((ret = cursor->next(cursor)) == 0)
        (*countp)++;
    testutil_assert(ret == WT_NOTFOUND);
}

static void
print_cursor(WT_CURSOR *cursor)
{
    const char *key, *value;

    int ret = 0;
    printf("yang befor ...print_cursor.....1.........\r\n");
    //while ((ret = cursor->next(cursor)) == 0) {
    while (1) {
        ret = (int)cursor->next(cursor);
        if (ret == 0) {
            error_check(cursor->get_key(cursor, &key));
            error_check(cursor->get_value(cursor, &value));
            printf("Got record: %s : %s\n", key, value);
        } else {
            printf("yang test ...print_cursor..............ret:%d\r\n", ret);
            break;
        }
    }

    scan_end_check(ret == WT_NOTFOUND);
}

static void
access_txn01_test(void)
{
    WT_CONNECTION *conn;
    WT_CURSOR *cursor;
    WT_SESSION *session;
    uint64_t count;

    error_check(wiredtiger_open(home, NULL, "log=(prealloc:true, enabled,file_max=100KB), create, verbose=[transaction=0, timestamp=0, api:0]", &conn));

    //test_txn01
    error_check(conn->open_session(conn, NULL, NULL, &session));
    error_check(session->create(session, "table:access_txn", "key_format=S,value_format=S"));
    error_check(session->open_cursor(session, "table:access_txn", NULL, NULL, &cursor));
    error_check(session->begin_transaction(session, NULL));
    cursor->set_key(cursor, "key: aaa"); /* Insert a record. */
    cursor->set_value(cursor, "value: aaa");
    error_check(cursor->insert(cursor));
    error_check(session->commit_transaction(session, NULL));
    
    error_check(session->begin_transaction(session, NULL));
    cursor->set_key(cursor, "key: bbb"); /* Insert a record. */
    cursor->set_value(cursor, "value: bbb");
    error_check(cursor->insert(cursor));

    error_check(conn->open_session(conn, NULL, NULL, &session));
    error_check(session->open_cursor(session, "table:access_txn", NULL, NULL, &cursor));
    error_check(session->begin_transaction(session, "isolation=read-committed"));
    {
        WT_CURSOR_BTREE *cbt;
        WT_SESSION_IMPL *session_impl;
        cbt = (WT_CURSOR_BTREE *)cursor; //yang add xxxxxxxxx todo exampleìí?óbtree dump
        session_impl = CUR2S(cbt);
        error_check(__wt_verbose_dump_txn(session_impl, "ex access .................test_txn01"));
    }
    cursor_count_items(cursor, &count);
    print_cursor(cursor);
    printf("yang test...test_txn01.......can noly see bbb, not see aaa, aaa not commit\r\n");


    error_check(cursor->reset(cursor)); /* Restart the scan. */
    error_check(conn->close(conn, NULL)); 
}

//这里可以判断除snapshot和read-committed事务级别的区别，对应test_txn20
static void
access_txn20_test(void)
{
    WT_CONNECTION *conn;
    WT_CURSOR *cursor;
    WT_CURSOR *cursor2;
    WT_CURSOR *cursor3;
    WT_SESSION *session;
    WT_SESSION *session2;
    WT_SESSION *session3;
   // uint64_t count;
    char *value;

    error_check(wiredtiger_open(home, NULL, "log=(prealloc:true, enabled=false,file_max=2G), create, verbose=[checkpoint=5, transaction=3, timestamp=0, api:2]", &conn));

    //test_txn20
    error_check(conn->open_session(conn, NULL, NULL, &session));
    error_check(session->create(session, "table:access_txn", "key_format=S,value_format=S"));
    error_check(session->open_cursor(session, "table:access_txn", NULL, NULL, &cursor));
    error_check(session->begin_transaction(session, NULL));
    cursor->set_key(cursor, "key"); /* Insert a record. */
    cursor->set_value(cursor, "value_old");
    error_check(cursor->insert(cursor));
    error_check(session->commit_transaction(session, NULL));

    //老session显示写一条数据，但是不提交
    error_check(session->begin_transaction(session, NULL));
    cursor->set_key(cursor, "key"); /* Insert a record. */
    cursor->set_value(cursor, "value_new");
    error_check(cursor->update(cursor));
    {//这里
        WT_CURSOR_BTREE *cbt;
        WT_SESSION_IMPL *session_impl;
        cbt = (WT_CURSOR_BTREE *)cursor; //yang add xxxxxxxxx todo  
        session_impl = CUR2S(cbt);
        error_check(__wt_verbose_dump_txn(session_impl, "ex access ............0.....test_txn20"));
    }

    //新session2 "isolation=snapshot"读这条数据
    error_check(conn->open_session(conn, NULL, NULL, &session2));
    error_check(session2->open_cursor(session2, "table:access_txn", NULL, NULL, &cursor2));
    error_check(session2->begin_transaction(session2, "isolation=snapshot"));
    {//这里snapshot方式begin_transaction的时候，会通过__wt_txn_begin->__wt_txn_get_snapshot获取快照
        WT_CURSOR_BTREE *cbt;
        WT_SESSION_IMPL *session_impl;
        cbt = (WT_CURSOR_BTREE *)cursor2; //yang add xxxxxxxxx todo  
        session_impl = CUR2S(cbt);
        error_check(__wt_verbose_dump_txn(session_impl, "ex access ............1.....test_txn20"));
    }

    //WT_ISO_READ_COMMITTED和WT_ISO_SNAPSHOT的区别是，在都显示begin_transaction的方式开始事务的时候，WT_ISO_READ_COMMITTED的__wt_txn_get_snapshot快照
    // 生成在接口操作(例如__curfile_search->__wt_txn_cursor_op->__wt_txn_get_snapshot)的时候完成，而WT_ISO_SNAPSHOT在begin_transaction(__wt_txn_begin->__wt_txn_get_snapshot)
    // 的时候生成，所以test_txn20测试时候，WT_ISO_READ_COMMITTED方式由于之前的update已提交，提交完成后update对应事务会清理掉，所以在update事务提交后WT_ISO_READ_COMMITTED
    // 方式无法获取update的快照。

    //从这个例子中可以看出:
    // snapshot隔离级别的事务: 读取操作只会读取begin_transaction这个时刻已经提交的数据
    // read-committed隔离级别的事务: 实际上是读取__curfile_search执行时候已经提交的数据

    //新session3 "isolation=read-committed"读这条数据
    error_check(conn->open_session(conn, NULL, NULL, &session3));
    error_check(session3->open_cursor(session3, "table:access_txn", NULL, NULL, &cursor3));
    error_check(session3->begin_transaction(session3, "isolation=read-committed"));
    {//这里read-committed方式begin_transaction的时候，不会通过__wt_txn_get_snapshot获取快照，也就是当前没有update的快照
        WT_CURSOR_BTREE *cbt;
        WT_SESSION_IMPL *session_impl;
        cbt = (WT_CURSOR_BTREE *)cursor3; //yang add xxxxxxxxx todo  
        session_impl = CUR2S(cbt);
        error_check(__wt_verbose_dump_txn(session_impl, "ex access ..............2...test_txn20"));
    }
    
    //session2  session3读数据前commit事务，update事务id会被清理掉，后续通过__wt_txn_get_snapshot获取快照无法获取update的事务id
    error_check(session->commit_transaction(session, NULL));

    {
        WT_CURSOR_BTREE *cbt;
        WT_SESSION_IMPL *session_impl;
        cbt = (WT_CURSOR_BTREE *)cursor2; //yang add xxxxxxxxx todo exampleìí?óbtree dump
        session_impl = CUR2S(cbt);
        error_check(__wt_verbose_dump_txn(session_impl, "ex access ..........3.......test_txn20"));
    }
    cursor2->set_key(cursor2, "key");
    error_check(cursor2->search(cursor2));
    error_check(cursor2->get_value(cursor2, &value));
    printf("Load snapshot search value: %s\n", value);

    printf("yang test...test_txn20......only see value_old, not see value_new, 因为begain txn的时候snpshot[]中已经保持了老session的事务\r\n");




    {
        WT_CURSOR_BTREE *cbt;
        WT_SESSION_IMPL *session_impl;
        cbt = (WT_CURSOR_BTREE *)cursor3; //yang add xxxxxxxxx todo exampleìí?óbtree dump
        session_impl = CUR2S(cbt);
        error_check(__wt_verbose_dump_txn(session_impl, "ex access .........4........test_txn20"));
    }
    cursor3->set_key(cursor3, "key");
    error_check(cursor3->search(cursor3));
    error_check(cursor3->get_value(cursor3, &value));
    printf("Load read-committed search value: %s\n", value);



    error_check(cursor->reset(cursor)); /* Restart the scan. */
    error_check(conn->close(conn, NULL)); 
}

////只有设置了commit_timestamp并且没有启用WAL功能，timestamp功能才会有效, 所以该用例不能启用log enable功能，参考__wt_txn_op_set_timestamp
static void
access_txn23_test(void)
{
    WT_CONNECTION *conn;
    WT_CURSOR *cursor;
    WT_SESSION *session;
   // uint64_t count;
   // char tscfg[512];
    int i;
    char *value;
    const int kv_num = 1;
    ////只有设置了commit_timestamp并且没有启用WAL功能，timestamp功能才会有效, 所以该用例不能启用log enable功能，参考__wt_txn_op_set_timestamp
    error_check(wiredtiger_open(home, NULL, "log=(prealloc:true,file_max=100KB), create, "
        "verbose=[config_all_verbos:5,transaction=5, timestamp=5, api:5]", &conn));

    //test_txn23
    error_check(conn->open_session(conn, NULL, NULL, &session));
    error_check(session->create(session, "table:access_txn23", "key_format=i,value_format=S"));
    error_check(session->open_cursor(session, "table:access_txn23", NULL, NULL, &cursor));
    error_check(__wt_verbose_dump_txn((WT_SESSION_IMPL *)session, "ex access ...access_txn23....1"));
    //__conn_set_timestamp
    testutil_check(conn->set_timestamp(conn, "oldest_timestamp=10, stable_timestamp=10"));
    error_check(__wt_verbose_dump_txn((WT_SESSION_IMPL *)session, "ex access ...access_txn23....2"));
    for (i = 0; i < kv_num; i++) {
        error_check(session->begin_transaction(session, NULL));
        cursor->set_key(cursor, i); /* Insert a record. */
        cursor->set_value(cursor, "value: aaa");
        error_check(cursor->insert(cursor));
        //testutil_check(session->timestamp_transaction(session, "commit_timestamp=20"));
        error_check(session->commit_transaction(session, "commit_timestamp=20"));
        error_check(cursor->close(cursor));
    }
    error_check(__wt_verbose_dump_txn((WT_SESSION_IMPL *)session, "ex access ...access_txn23...3"));

    for (i = 0; i < kv_num; i++) {
        error_check(session->open_cursor(session, "table:access_txn23", NULL, NULL, &cursor));
        error_check(session->begin_transaction(session, NULL));
        cursor->set_key(cursor, i); /* Insert a record. */
        cursor->set_value(cursor, "value: bbb");
        error_check(cursor->insert(cursor));
        //testutil_check(session->timestamp_transaction(session, "commit_timestamp=30"));
        //__session_commit_transaction
        error_check(session->commit_transaction(session, "commit_timestamp=30"));
        error_check(cursor->close(cursor));
    }
    error_check(__wt_verbose_dump_txn((WT_SESSION_IMPL *)session, "ex access ...access_txn23...4"));

    for (i = 0; i < kv_num; i++) {
        error_check(session->open_cursor(session, "table:access_txn23", NULL, NULL, &cursor));
        //__session_begin_transaction
        //重要，这里如果是15则返回的是WT_NOTFOUND，如果是20-30之间读到的是"value: aaa"，如果是大于等于30读到的是"value: bbb"
        error_check(session->begin_transaction(session, "read_timestamp=31"));
        //error_check(__wt_verbose_dump_txn((WT_SESSION_IMPL *)session, "ex access ...access_txn23...5"));
        error_check(session->open_cursor(session, "table:access_txn23", NULL, NULL, &cursor));
        cursor->set_key(cursor, i);
        //error_check(__wt_verbose_dump_txn((WT_SESSION_IMPL *)session, "ex access ...access_txn23"));
        error_check(cursor->search(cursor));
        error_check(cursor->get_value(cursor, &value));
        printf("yang test ............. test_txn23, value:%s\r\n", value); //读取的结构是aaa
        error_check(session->commit_transaction(session, NULL));
        error_check(cursor->close(cursor));
    }

    error_check(cursor->reset(cursor)); /* Restart the scan. */
    error_check(conn->close(conn, NULL)); 
}


static void
access_example(void)
{
    /*! [access example connection] */
    WT_CONNECTION *conn;
    WT_CURSOR *cursor;
    WT_SESSION *session;
   // const char *key, *value;
    //int ret;
    //int key_item;
    WT_ITEM value_item;
    WT_ITEM value_item2;
    int i =0;
    char buf[512];

#ifdef HAVE_DIAGNOSTIC
    WT_BTREE *btree;
    int ret;
    WT_CURSOR_BTREE *cbt;
    WT_SESSION_IMPL *session_impl;
#endif
    WT_RAND_STATE rnd;
    uint64_t rval;
    uint64_t max_i = 0;
    uint64_t start, stop, time_ms;


    /* Open a connection to the database, creating it if necessary. */
    //error_check(wiredtiger_open(home, NULL, "create,statistics=(all),create,verbose=[evictserver=5,evict=5,split=5,evict_stuck=5]", &conn));
    //error_check(wiredtiger_open(home, NULL, "create,cache_size=1M, statistics=(all),create,verbose=[split=5, overflow=5, generation=5, block=5, write=5, evictserver=5, evict_stuck=5, block_cache=5, checkpoint_progress=5,  checkpoint=5, checkpoint_cleanup=5, block=5,overflow=5,reconcile=5,evictserver=5,evict=5,split=5,evict_stuck=5]", &conn));

  /*  error_check(wiredtiger_open(home, NULL, "create,cache_size=10M, statistics=(all),create,verbose=[\
    backup=5, block=5, block_cache=5, checkpoint=5, checkpoint_cleanup=5,checkpoint_progress=5,compact=5,\
    compact_progress=5,error_returns=5,evict=5,evict_stuck=5,evictserver=5,fileops=5,generation=5,handleops=5,log=5,\
    hs=5, history_store_activity=5,lsm=5,lsm_manager=5,metadata=5,mutex=5,out_of_order=5,overflow=5,read=5,reconcile=5,recovery=5, \
    recovery_progress=5,rts=5, salvage=5, shared_cache=5,split=5,temporary=5,thread_group=5,timestamp=5,tiered=5,transaction=5,verify=5,\  ,timing_stress_for_test:[split_7, split_5,split_6]
    version=5,write=5, config_all_verbos=1, api=-3, metadata=-3]  ", &conn));*/  //eviction_dirty_target=90,eviction_dirty_trigger=95,eviction_target=95,eviction_trigger=96,
    
    error_check(wiredtiger_open(home, NULL, ""
    "log=(enabled,file_max=2000KB), checkpoint=[wait=60],create,cache_size=1G, statistics=(all), create,verbose=[config_all_verbos:0, "
    "checkpoint:2,write:0,reconcile:2, split:2, evict:2,metadata:0, api:0,log:5]", &conn));
     //config_all_verbos=]", &conn));verbose=[recovery_progress,checkpoint_progress,compact_progress]

    /* Open a session handle for the database. */
    error_check(conn->open_session(conn, NULL, NULL, &session));
    /*! [access example connection] */

    /*! [access example table create] */
    //error_check(session->create(session, "table:access", "memory_page_max=1K,key_format=q,value_format=u")); memory_page_image_max=128KB, memory_page_max=100K,memory_page_image_max=4M, leaf_page_max=32KB,
    error_check(session->create(session, "table:access", "key_format=q,value_format=S"));
    /*! [access example table create] */

    /*! [access example cursor open] */
    error_check(session->open_cursor(session, "table:access", NULL, NULL, &cursor));
    /*! [access example cursor open] */

    value_item.data = "value old @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ \0";
    /*
      "abcdefghijklmnopqrstabcdefghijklmnopqrstabcdefghijklmnopqrstabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzuvwxyzabcdefghijklmnopqrstabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzuvwxyzuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzuvwxyz"
      "abcdefghijklmnopqrstabcdefghijklmnopqrstabcdefghijklmnopqrstabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzuvwxyzabcdefghijklmnopqrstabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzuvwxyzuvwxyzabcdefghijklmnopqrstabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzuvwxyzabcdefghijklmnopqrstabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzuvwxyzuvwxyz"
      "abcdefghijklmnopqrstabcdefghijklmnopqrstabcdefghijklmnopqrstabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzuvwxyzabcdefghijklmnopqrstabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzuvwxyzuvwxyzabcdefghijklmnopqrstabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzuvwxyzabcdefghijklmnopqrstabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzuvwxyzabcdefg"
      "hijklmnopqrstabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzuvwxyzabcdefghijklmnopqrstabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzuvwxyz";
    */
    /*value_item.data ="yangyazhou abcdefghijklmnopqrstabcdefghijklmnopqrstabcdefghijklmnopqrstabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzuvwxyzabcdefghijklmnopqrstabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzuvwxyzuvwxyzabcdefghijklmnopqrstabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzuvwxyzabcdefghijklmnopqrstabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzuvwxyzabcdefghijklmnopqrstabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzuvwxyzabcdefghijklmnopqrstabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzuvwxyz"
"klmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzuvwxyzabcdefghijklmnopqrstabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzuvwxyzuvwxyzabcdefghijklmnopqrstabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzuvwxyzabcdefghijklmnopqrstabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzuvwxyzabcdefghijklmnopqrstabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzuvwxyzabcdefghijklmnopqrstabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzuvwxyz\0";*/
    value_item.size = sizeof(value_item.data);
    printf("yang test ..............size:%d\r\n", (int)sizeof("123456\0\0\0"));
    __wt_random_init_seed(NULL, &rnd);
    
    cbt = (WT_CURSOR_BTREE *)cursor; 
    session_impl = CUR2S(cbt);
    start = __wt_clock(session_impl);
    //error_check(session->reconfigure(session, "memory_page_image_max=177KB"));

    value_item2.data = "value new @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\0";
    value_item2.size = strlen(value_item2.data);
   // for (i=0;i < 1195100; i++) {
    for (i=3529011;i > 0 ; i--) {
        rval = __wt_random(&rnd)%3529011;

        snprintf(buf, sizeof(buf), "value @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ %d", i);
        cursor->set_key(cursor, i); /* Insert a record. */
        cursor->set_value(cursor, buf);
        if (max_i < rval % 12000)
            max_i =  (rval % 12000);

       // if (i % 5 == 0)
        //    continue;

        //printf("yang test insert ......... i:%d\r\n", i);
        error_check(cursor->insert(cursor));
        //usleep(1000);
    }
    printf("yang test   ............................insert.............. end:\r\n");
    session->checkpoint(session, "force");
    exit(0);
    session->checkpoint(session, "force");
    for (i=15000; i < 15001; i++) {
    //for (i=0;i < 0; i++) {
        rval = __wt_random(&rnd) % 10111;
        snprintf(buf, sizeof(buf), "value xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx %d", i);
        cursor->set_key(cursor, 0); /* Insert a record. */
        cursor->set_value(cursor, buf);
        printf("yang test   ............................update.............. i:%d\r\n", i);
        error_check(cursor->update(cursor));
        //usleep(1000);
    }

    printf("yang test   ............................update.............. end:\r\n");
    for (i=0; i < 1200; i++) {
    //for (i=0;i < 0; i++) {
        rval = __wt_random(&rnd) % 10;
        cursor->set_key(cursor, rval); /* Insert a record. */
        error_check(cursor->remove(cursor));
        //usleep(1000);
    }

    sleep(10);
    
    session->checkpoint(session, "force");
    exit(0);
    
    stop = __wt_clock(session_impl);
    time_ms = WT_CLOCKDIFF_MS(stop, start);
    printf("yang test insert ......... run time:%lu\r\n", time_ms);

    session->checkpoint(session, "force");
    print_session_stats_for_access(session);
     printf("yang test ............serch begin\r\n"); 
    cursor->set_key(cursor, 4);
    error_check(cursor->search(cursor));
    error_check(cursor->get_value(cursor, &value_item));

    cursor->set_key(cursor, 3);
    error_check(cursor->search(cursor));
    error_check(cursor->get_value(cursor, &value_item));

    printf("yang test ............get len:%d, value:%s\r\n", (int)value_item.size, "xxxxxxxxxxxxxxxxxx");
    print_session_stats_for_access(session);
  //  sleep(10);
  /*   for (i=120;i > 0; i--) {
        rval = __wt_random(&rnd);

        error_check(session->begin_transaction(session, NULL));
        cursor->set_key(cursor, i);
        cursor->set_value(cursor, &value_item2);
        error_check(cursor->update(cursor));
        error_check(session->commit_transaction(session, NULL));
    }*/

    cursor->set_key(cursor, -1); /* Insert a record. */
    cursor->set_value(cursor, &value_item);
    error_check(cursor->insert(cursor));

    cursor->set_key(cursor, 5); /* Insert a record. */
    cursor->set_value(cursor, &value_item);
    error_check(cursor->insert(cursor));

    cursor->set_key(cursor, 15); /* Insert a record. */
    cursor->set_value(cursor, &value_item);
    error_check(cursor->insert(cursor));

    sleep(100);
    exit(0);
    //error_check(session->open_cursor(session, "table:access", NULL, NULL, &cursor));
#ifdef HAVE_DIAGNOSTIC
    cbt = (WT_CURSOR_BTREE *)cursor; //yang add xxxxxxxxx todo exampleìí?óbtree dump
    session_impl = CUR2S(cbt);
    btree = CUR2BT(cbt);
    //usleep(10000000);

    WT_WITH_BTREE(session_impl, btree, ret = __wt_debug_tree_all(session_impl, NULL, NULL, NULL));
    if (!ret)
        printf("yang test 111111111111111111111__wt_debug_tree_all11111ss1111111111111111111111 error\r\n");
#endif
     /*! [access example cursor insert]
    cursor->set_key(cursor, 1111);
    cursor->set_value(cursor, &value_item);
    error_check(cursor->insert(cursor));
    printf("yang test 2222222222222222222222222222222222222222222222222\r\n");
    cursor->set_key(cursor, 11);
    cursor->set_value(cursor, &value_item);
    error_check(cursor->insert(cursor));
    printf("yang test 333333333333333333333333333333333333333333333333\r\n");

    cursor->set_key(cursor, 2222);
    cursor->set_value(cursor, &value_item);
    error_check(cursor->insert(cursor));
    printf("yang test 444444444444444444444444444444444444444444444444\r\n");

        cursor->set_key(cursor, 11);
    cursor->set_value(cursor, &value_item);
    error_check(cursor->insert(cursor));*/

    printf("yang test 555555555555555555555555555555555555555555555555555555555555\r\n");

    error_check(cursor->reset(cursor)); /* Restart the scan. */
    /*! [access example cursor insert] */
    error_check(cursor_search(cursor));
    printf("yang test 66666666666666666666666666666666666666666\r\n");

    /*! [access example cursor list] */
    error_check(cursor->reset(cursor)); /* Restart the scan. */

   // while ((ret = cursor->next(cursor)) == 0) {
       // error_check(cursor->get_key(cursor, &key_item));
       // error_check(cursor->get_value(cursor, &value_item));

       // printf("Got record: %d : %s\n", key_item, (char*)value_item.data);
   // }
    //scan_end_check(ret == WT_NOTFOUND); /* Check for end-of-table. */
    /*! [access example cursor list] */

    /*! [access example close] */
    error_check(conn->close(conn, NULL)); /* Close all handles. */
                                          /*! [access example close] */
}

int
main(int argc, char *argv[])
{
    home = example_setup(argc, argv);
    printf("yang test insert ......... ex access\r\n");

    access_txn20_test();

    

    exit(0);
    access_txn23_test();
    access_example();

    access_txn20_test();

    access_txn01_test();

    return (EXIT_SUCCESS);
}
