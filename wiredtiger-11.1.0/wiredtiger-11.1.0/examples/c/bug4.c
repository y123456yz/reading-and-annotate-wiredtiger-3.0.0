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

#define MAX_TEST_KV_NUM 2000
/*

yang test __split_insert: {1057}
yang test ..................... end, api_call_counter: 1  0
[1698137566:294048][86102:0x7f318ac2a800], file:debug_tree.wt, WT_CURSOR.__curfile_insert: [WT_VERB_DEFAULT][ERROR]: __curfile_insert, 406: (session)->api_call_counter > 1 || (session)->hs_cursor_counter <= 3
[1698137566:294078][86102:0x7f318ac2a800], file:debug_tree.wt, WT_CURSOR.__curfile_insert: [WT_VERB_DEFAULT][ERROR]: __wt_abort, 28: aborting WiredTiger library
Aborted

https://jira.mongodb.org/browse/WT-7247
*/
static void
debug_tree_example(void)
{
    /*! [access example connection] */
    WT_CONNECTION *conn;
    WT_CURSOR *cursor;
    WT_SESSION *session;
    int i;
    WT_ITEM value_item;
    WT_BTREE *btree;
    WT_CURSOR_BTREE *cbt;
    WT_SESSION_IMPL *session_impl;
    //int ret;

    /* Open a connection to the database, creating it if necessary. */
    error_check(wiredtiger_open(home, NULL, "create,cache_size=1M, statistics=(all), io_capacity=(total=1M)", &conn));

    /* Open a session handle for the database. */
    error_check(conn->open_session(conn, NULL, NULL, &session));
    /*! [access example connection] */

    /*! [access example table create] */
    error_check(session->create(session, "table:debug_tree", "memory_page_max=20K, key_format=q,value_format=u"));
    /*! [access example table create] */

    /*! [access example cursor open] */
    error_check(session->open_cursor(session, "table:debug_tree", NULL, NULL, &cursor));
    /*! [access example cursor open] */

    //insert
    for (i = 0; i < MAX_TEST_KV_NUM; i++) {
        cursor->set_key(cursor, i);

        value_item.data = "old value  ###############################################################################################################################################################################################################\0";
        value_item.size = strlen(value_item.data) + 1;
        
        cursor->set_value(cursor, &value_item);
        error_check(cursor->insert(cursor));
    }

    //update
    for (i = 0; i < MAX_TEST_KV_NUM; i++) {
        cursor->set_key(cursor, i);

        value_item.data = "new value ###############################################################################################################################################################################################################\0";
        value_item.size = strlen(value_item.data) + 1;
        cursor->set_value(cursor, &value_item);
        error_check(cursor->update(cursor));
    }

        //update
    for (i = 0; i < MAX_TEST_KV_NUM; i++) {
        cursor->set_key(cursor, i);

        value_item.data = "new value 2 #####################################################\0";
        value_item.size = strlen(value_item.data) + 1;
        cursor->set_value(cursor, &value_item);
        error_check(cursor->update(cursor));
    }

    error_check(cursor->reset(cursor));
   /* while ((ret = cursor->next(cursor)) == 0) {
         error_check(cursor->get_key(cursor, &i));
         error_check(cursor->get_value(cursor, &value_item.data));
    
         printf("Got record: %d : %s\r\n", i, (char*)value_item.data);
     }*/

    
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


