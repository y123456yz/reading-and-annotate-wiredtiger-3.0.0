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
 * ex_debug_tree.c
 * 	Shows how to debug the tree.
 */
#include <test_util.h>

static const char *home;

#define MAX_TEST_KV_NUM 20000
//btree打印不全，有问题

/*
原因:key value字符串必须提前分片空间， 例如下面这样，否则空间会覆盖

//insert
for (i = 0; i < MAX_TEST_KV_NUM; i++) {
    p = malloc(100);
    memset(p, 0, 100);
    snprintf(buf, 1024, "key%d", i);
    memcpy(p, buf, strlen(buf));
    cursor->set_key(cursor, p);
   // printf("yang test 111111111 buf%d:%s\r\n", i, buf);

    p = malloc(100);
    memset(p, 0, 100);
    memcpy(p, "old value #####################################################", 
        strlen("old value #####################################################"));
   // printf("yang test 22222222222 buf%d:%s\r\n", i, buf);
    cursor->set_value(cursor, p);
    error_check(cursor->insert(cursor));
}

*/
static void
debug_tree_example(void)
{
    /*! [access example connection] */
    WT_CONNECTION *conn;
    WT_CURSOR *cursor;
    WT_SESSION *session;
//    const char *key, *value;
//    int ret;
    char buf[1024];
    int i;
    WT_BTREE *btree;
    WT_CURSOR_BTREE *cbt;
    WT_SESSION_IMPL *session_impl;

    /* Open a connection to the database, creating it if necessary. */
    error_check(wiredtiger_open(home, NULL, "create,cache_size=1M, statistics=(all)", &conn));

    /* Open a session handle for the database. */
    error_check(conn->open_session(conn, NULL, NULL, &session));
    /*! [access example connection] */

    /*! [access example table create] */
    error_check(session->create(session, "table:debug_tree", "memory_page_max=21K, key_format=q,value_format=S"));
    /*! [access example table create] */

    /*! [access example cursor open] */
    error_check(session->open_cursor(session, "table:debug_tree", NULL, NULL, &cursor));
    /*! [access example cursor open] */

    //insert
    for (i = 0; i < MAX_TEST_KV_NUM; i++) {
        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf), "key%d", i);
        /*! [access example cursor insert] */
        cursor->set_key(cursor, i);
       // printf("yang test 111111111 buf%d:%s\r\n", i, buf);

        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf), "old value%d #####################################################", i);
       // printf("yang test 22222222222 buf%d:%s\r\n", i, buf);
        cursor->set_value(cursor, buf);
        error_check(cursor->insert(cursor));
    }

    //update
  /*  for (i = 0; i < MAX_TEST_KV_NUM; i++) {
        snprintf(buf, sizeof(buf), "key%d", i);
        
        cursor->set_key(cursor, buf);

        snprintf(buf, sizeof(buf), "new value%d #####################################################", i);
        cursor->set_value(cursor, buf);
        error_check(cursor->update(cursor));
    }
    */

    cbt = (WT_CURSOR_BTREE *)cursor;  
    session_impl = CUR2S(cbt);
    btree = CUR2BT(cbt);
    WT_WITH_BTREE(session_impl, btree, error_check(__wt_debug_tree_all(session_impl, NULL, NULL, NULL)));

    error_check(cursor->close(cursor));
    /*! [access example close] */
    error_check(conn->close(conn, NULL)); /* Close all handles. */
                                          /*! [access example close] */
}

int
main(int argc, char *argv[])
{
    home = example_setup(argc, argv);

    debug_tree_example();

    return (EXIT_SUCCESS);
}



