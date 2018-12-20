/*-
 * Public Domain 2014-2017 MongoDB, Inc.
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
//所有常用函数全局默认配置见config_entries
#define	CONN_CONFIG \
    "create,cache_size=1M,log=(archive=false,enabled=true,file_max=100K)"

static void
access_example(void)
{
	/*! [access example connection] */
	WT_CONNECTION *conn;
	WT_CURSOR *cursor;
	WT_SESSION *session;
	const char *key, *value;
	int ret;

	/* Open a connection to the database, creating it if necessary. */
	//error_check(wiredtiger_open(home, NULL, "create", &conn));
	error_check(wiredtiger_open(home, NULL, CONN_CONFIG, &conn));
    printf("yang test 11111111111111111111111111 wiredtiger_open end\r\n");

	/* Open a session handle for the database. */
	//__conn_open_session
	error_check(conn->open_session(conn, NULL, NULL, &session));
	/*! [access example connection] */
    printf("yang test 11111111111111111111111111 __conn_open_session end\r\n");

	/*! [access example table create] */
	//__session_create  创建table表
	error_check(session->create(
	    session, "table:access", "key_format=S,value_format=S"));
	/*! [access example table create] */
    printf("yang test 3333333333333333333333333333 __session_create end\r\n");

	/*! [access example cursor open] */
	//__session_open_cursor  //获取一个cursor通过cursorp返回
	error_check(session->open_cursor(
	    session, "table:access", NULL, NULL, &cursor));
	/*! [access example cursor open] */

   
	/*! [access example cursor insert] */
	//__wt_cursor_set_key
	cursor->set_key(cursor, "key1");	/* Insert a record. */
	//__wt_cursor_set_value
	cursor->set_value(cursor, "value1");
	printf("yang test ....................__wt_cursor_set_valuev end\r\n");
	//__curfile_insert
    error_check(cursor->insert(cursor));
 
    cursor->set_key(cursor, "key2");	/* Insert a record. */
	//__wt_cursor_set_value
	cursor->set_value(cursor, "value2");
	//__curfile_insert
    error_check(cursor->insert(cursor));

    sleep(600);

    /*! [access example close] */
	error_check(conn->close(conn, NULL));	/* Close all handles. */
	/*! [access example close] */
    
    
    return 1;
    
	/*! [access example cursor list] */
	//__curfile_reset
	//error_check(cursor->reset(cursor));	/* Restart the scan. */
	printf("yang test ....................__curfile_reset end\r\n");
	
	while ((ret = cursor->next(cursor)) == 0) { //__curfile_next
	    //__wt_cursor_get_key
		error_check(cursor->get_key(cursor, &key));
		printf("yang test ....................__wt_cursor_get_key end\r\n");

		//__wt_cursor_get_value
		error_check(cursor->get_value(cursor, &value));
		printf("yang test ....................__wt_cursor_set_valuev end\r\n");

		printf("Got record: %s : %s\n", key, value);
	}

	sleep(100);
	scan_end_check(ret == WT_NOTFOUND);	/* Check for end-of-table. */
	/*! [access example cursor list] */

	/*! [access example close] */
	error_check(conn->close(conn, NULL));	/* Close all handles. */
	/*! [access example close] */
}

int
main(int argc, char *argv[])
{
	home = example_setup(argc, argv);

	access_example();

	return (EXIT_SUCCESS);
}
