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
 * ex_verbose.c
 *	Demonstrate how to configure verbose messaging in WiredTiger.
 */
#include <test_util.h>

static const char *home;

int handle_wiredtiger_message(WT_EVENT_HANDLER *, WT_SESSION *, const char *);

/*
 * handle_wiredtiger_message --
 *     Function to handle message callbacks from WiredTiger.
 */
int
handle_wiredtiger_message(WT_EVENT_HANDLER *handler, WT_SESSION *session, const char *message)
{
    /* Unused parameters */
    (void)handler;
    printf("WiredTiger Message - Session: %p, Message: %s\n", (void *)session, message);

    return (0);
}

// ./wt dump file:access.wt 

//"ctx":"initandlisten","msg":"Opening WiredTiger","attr":{"config":"create,cache_size=512M,session_max=33000,eviction=(threads_min=4,
//threads_max=4),config_base=false,statistics=(fast),log=(enabled=true,archive=true,path=journal,compressor=snappy),builtin_extension_config=
//(zstd=(compression_level=6)),file_manager=(close_idle_time=600,close_scan_interval=10,close_handle_minimum=250),statistics_log=(wait=0),
//verbose=[recovery_progress,checkpoint_progress,compact_progress],"}}
static void
config_verbose(void)
{
    WT_CONNECTION *conn;
    WT_SESSION *session;
    WT_CURSOR *cursor;

    WT_EVENT_HANDLER event_handler;

    event_handler.handle_message = handle_wiredtiger_message;
    event_handler.handle_error = NULL;
    event_handler.handle_progress = NULL;
    event_handler.handle_close = NULL;
    event_handler.handle_general = NULL;

    fprintf(stderr, "WiredTiger Error: stderr\r\n");

    //../wt dump file:verbose.wt获取文件内容
     //__conn_reconfigure可以修改参数配置
    /*! [Configure verbose_messaging] */
    error_check(wiredtiger_open( //可以配合mongodb内核的WiredTigerKVEngine::WiredTigerKVEngine进行阅读
    home, (WT_EVENT_HANDLER *)&event_handler, "create,operation_tracking=(enabled=false,path=\"./\"),verbose=[api=5,block=5,checkpoint=5,checkpoint_progress=5,compact=5,evict=5,evict_stuck=5,evictserver=5,fileops=5,handleops=5,log=5,lsm=5,lsm_manager=5,metadata=5,mutex=5,overflow=5,read=5,reconcile=5,reconcile=5,recovery=5,recovery_progress=5,salvage=5,shared_cache=5,split=5,thread_group=5,split=5,thread_group=5,timestamp=5,transaction=5,verify=5,version=5,write=5]", &conn));
        
        //home, (WT_EVENT_HANDLER *)&event_handler, "create,verbose=[api=1,block=1,checkpoint=1,checkpoint_progress=1,compact=1,evict=1,evict_stuck=1,evictserver=1,fileops=1,handleops=1,log=1,lsm=1,lsm_manager=1,metadata=1,mutex=1,overflow=1,read=1,reconcile=1,reconcile=1,recovery=1,recovery_progress=1,salvage=1,shared_cache=1,split=1,thread_group=1,split=1,thread_group=1,timestamp=1,transaction=1,verify=1,version=1,write=1]", &conn));
     //home, (WT_EVENT_HANDLER *)&event_handler, "create,verbose=[api:1,version,write:0]", &conn));
     // home, (WT_EVENT_HANDLER *)&event_handler, "create,verbose=[]", &conn));
    /*! [Configure verbose_messaging] */
    // usleep(30000000);

    
    /* Make a series of API calls, to ensure verbose messages are produced. */
    printf("\r\n\r\n\r\nex_verbose: expect verbose messages to follow: step1:\n");
    //__conn_open_session  //从session hash桶中获取一个session
    error_check(conn->open_session(conn, NULL, NULL, &session));

    printf("\r\n\r\n\r\nex_verbose: expect verbose messages to step2:\n");
    //__session_create 
    //Format types参考http://source.wiredtiger.com/3.2.1/schema.html
    //https://source.wiredtiger.com/develop/data_sources.html
    //http://source.wiredtiger.com/3.2.1/devdoc-schema.html#schema_create
    error_check(session->create(session, "table:verbose", "key_format=S,value_format=S"));
    printf("\r\n\r\n\r\nex_verbose: expect verbose messageskey_format to step3:\n");
    //__session_open_cursor
    error_check(session->open_cursor(session, "table:verbose", NULL, NULL, &cursor));
    printf("\r\n\r\n\r\nex_verbose: expect verbose messages to step4:\n");
    //__wt_cursor_set_keyv
    cursor->set_key(cursor, "foo");
    printf("\r\n\r\n\r\nex_verbose: expect verbose messages to step5:\n");
    cursor->set_value(cursor, "bar");
    printf("\r\n\r\n\r\nex_verbose: expect verbose messages to step6:\n");
    error_check(cursor->insert(cursor));
    printf("\r\n\r\n\r\nex_verbose: expect verbose messages to step7:\n");
    error_check(cursor->close(cursor));
    printf("\r\n\r\n\r\nex_verbose: expect verbose messages to step8:\n");
    printf("\r\n\r\n\r\nex_verbose: end of verbose messages\n");

    error_check(conn->close(conn, NULL));
}

int
main(int argc, char *argv[])
{
    home = example_setup(argc, argv);

    config_verbose();

    return (EXIT_SUCCESS);
}
