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
    printf("yang befor ...print_cursor..............\r\n");
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
access_txn_test(void)
{
    WT_CONNECTION *conn;
    WT_CURSOR *cursor;
    WT_SESSION *session;
    uint64_t count;

    error_check(wiredtiger_open(home, NULL, "create, verbose=[transaction=5, timestamp=5, api:5]", &conn));

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


    access_txn_test();
   // exit(0);

    /* Open a connection to the database, creating it if necessary. */
    //error_check(wiredtiger_open(home, NULL, "create,statistics=(all),create,verbose=[evictserver=5,evict=5,split=5,evict_stuck=5]", &conn));
    //error_check(wiredtiger_open(home, NULL, "create,cache_size=1M, statistics=(all),create,verbose=[split=5, overflow=5, generation=5, block=5, write=5, evictserver=5, evict_stuck=5, block_cache=5, checkpoint_progress=5,  checkpoint=5, checkpoint_cleanup=5, block=5,overflow=5,reconcile=5,evictserver=5,evict=5,split=5,evict_stuck=5]", &conn));

  /*  error_check(wiredtiger_open(home, NULL, "create,cache_size=10M, statistics=(all),create,verbose=[\
    backup=5, block=5, block_cache=5, checkpoint=5, checkpoint_cleanup=5,checkpoint_progress=5,compact=5,\
    compact_progress=5,error_returns=5,evict=5,evict_stuck=5,evictserver=5,fileops=5,generation=5,handleops=5,log=5,\
    hs=5, history_store_activity=5,lsm=5,lsm_manager=5,metadata=5,mutex=5,out_of_order=5,overflow=5,read=5,reconcile=5,recovery=5, \
    recovery_progress=5,rts=5, salvage=5, shared_cache=5,split=5,temporary=5,thread_group=5,timestamp=5,tiered=5,transaction=5,verify=5,\
    version=5,write=5, config_all_verbos=1, api=-3, metadata=-3]  ", &conn));*/
    
    error_check(wiredtiger_open(home, NULL, "log=(enabled,file_max=100KB),create,cache_size=25M, statistics=(all),create,verbose=[write:0,reconcile:0, metadata:0, api:0]", &conn));
     //config_all_verbos=]", &conn));verbose=[recovery_progress,checkpoint_progress,compact_progress]

    /* Open a session handle for the database. */
    error_check(conn->open_session(conn, NULL, NULL, &session));
    /*! [access example connection] */

    /*! [access example table create] */
    //error_check(session->create(session, "table:access", "memory_page_max=1K,key_format=q,value_format=u")); memory_page_image_max=128KB,
    error_check(session->create(session, "table:access", "memory_page_image_max=77KB, leaf_page_max=32KB,key_format=q,value_format=u"));
    /*! [access example table create] */

    /*! [access example cursor open] */
    error_check(session->open_cursor(session, "table:access", NULL, NULL, &cursor));
    /*! [access example cursor open] */

    value_item.data = "value old @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\0";
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
    error_check(session->reconfigure(session, "memory_page_image_max=177KB"));

    value_item2.data = "value new @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\0";
    value_item2.size = strlen(value_item2.data);
    for (i=500000;i > 0; i--) {
   // for (i=0;i < 9011 ; i++) {
        rval = __wt_random(&rnd);

        cursor->set_key(cursor, i); /* Insert a record. */
        cursor->set_value(cursor, &value_item2);
        if (max_i < rval % 12000)
            max_i =  (rval % 12000);

       // if (i % 5 == 0)
        //    continue;

        //printf("yang test insert ......... i:%lu, max_i:%lu\r\n", rval % 23519, max_i);
        error_check(cursor->insert(cursor));
    }
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

    access_example();

    return (EXIT_SUCCESS);
}
