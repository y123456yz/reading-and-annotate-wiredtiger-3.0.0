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

static void
access_example(void)
{
    /*! [access example connection] */
    WT_CONNECTION *conn;
    WT_CURSOR *cursor, *cursor2;
    WT_SESSION *session, *session2;
    const char *key, *value;
    int ret;

    /* Open a connection to the database, creating it if necessary. */
    error_check(wiredtiger_open(home, NULL, "create,statistics=(all),log=(enabled:true,recover=on,remove=true),"
    "verbose=[timestamp:5,transaction:5, metadata:0, recovery:5, recovery_progress:5, log:5]", &conn));

    error_check(conn->open_session(conn, NULL, NULL, &session));
    error_check(session->create(session, "table:access", "key_format=S,value_format=S"));
    error_check(session->open_cursor(session, "table:access", NULL, NULL, &cursor));

    error_check(conn->open_session(conn, NULL, NULL, &session2));
    error_check(session2->create(session2, "table:access2", "key_format=S,value_format=S"));
    error_check(session2->open_cursor(session2, "table:access2", NULL, NULL, &cursor2));


  //  /*
    cursor->set_key(cursor, "key1");  
    cursor->set_value(cursor, "value1");
    error_check(cursor->insert(cursor));

    cursor2->set_key(cursor2, "key1");  
    cursor2->set_value(cursor2, "value1");
    error_check(cursor2->insert(cursor2));
    
    error_check(session->checkpoint(session, NULL));
    
    cursor->set_key(cursor, "key2");  
    cursor->set_value(cursor, "value2");
    error_check(cursor->insert(cursor));
    cursor->set_key(cursor, "key3");  
    cursor->set_value(cursor, "value3");
    error_check(cursor->insert(cursor));
    error_check(cursor->reset(cursor)); 

    cursor2->set_key(cursor2, "key2");  
    cursor2->set_value(cursor2, "value2");
    error_check(cursor2->insert(cursor2));
    cursor2->set_key(cursor2, "key3");  
    cursor2->set_value(cursor2, "value3");
    error_check(cursor2->insert(cursor2));
    error_check(cursor2->reset(cursor2)); 
   // */
    while ((ret = cursor->next(cursor)) == 0) {
        error_check(cursor->get_key(cursor, &key));
        error_check(cursor->get_value(cursor, &value));

        printf("Got cursor record: %s : %s\n", key, value);
    }

    while ((ret = cursor2->next(cursor2)) == 0) {
        error_check(cursor2->get_key(cursor2, &key));
        error_check(cursor2->get_value(cursor2, &value));

        printf("Got cursor2 record: %s : %s\n", key, value);
    }

    //__wt_sleep(3, 10000);
    exit(1);
    scan_end_check(ret == WT_NOTFOUND); /* Check for end-of-table. */
    /*! [access example cursor list] */

    /*! [access example close] */
   // error_check(conn->close(conn, NULL)); /* Close all handles. */  //yang add change  模拟非正常close，也就是不做checkpoint
                                          /*! [access example close] */
}

int
main(int argc, char *argv[])
{
    home = example_setup(argc, argv);

    access_example();

    return (EXIT_SUCCESS);
}

