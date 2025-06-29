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
 * 	demonstrates how to create and access a simple table, include insert data and load exist table's data.
 */
#include <test_util.h>


static const char *home = "WT_TEST";
//test vs code 中文测试，阿达萨法法师打发发的发啊范德萨短发啊范德萨发达地方
/*
利用vscode或者cursor通过ssh + clangd方式走读代码，如果函数无法跳转，则可以查看vscode或者cursor的日志，在



-DCMAKE_EXPORT_COMPILE_COMMANDS=1 clangd需要代码编译加上这个参数

terminal->output
cursor对应开发机的clangd是否异常可以查看这个目录的clangd是否可以运行，例如下面这个以来的glibc版本太低，则存在下面的output打印
/root/.cursor-server/data/User/globalStorage/llvm-vs-code-extensions.vscode-clangd/install/19.1.2/clangd_19.1.2/bin/clangd
[Error - 4:26:22 PM] Clang Language Server client: couldn't create connection to server.
  Message: Pending response rejected since connection got disposed
  Code: -32097 
/root/.cursor-server/data/User/globalStorage/llvm-vs-code-extensions.vscode-clangd/install/19.1.2/clangd_19.1.2/bin/clangd: /lib64/libc.so.6: version `GLIBC_2.18' not found (required by /root/.cursor-server/data/User/globalStorage/llvm-vs-code-extensions.vscode-clangd/install/19.1.2/clangd_19.1.2/bin/clangd)
[Info  - 4:26:22 PM] Connection to server got closed. Server will restart.
[Error - 4:26:22 PM] Server initialization failed.

这时候可以自己通过llvm-project源码安装cland，然后拷贝替换上面的cland二进制
*/
/*
 * usage --
 *     wtperf usage print, no error.
 */
static void
usage(void)
{
    printf("ex_access [-i] [-l]\n");
    printf("\t-i insert data and scan data\n");
    printf("\t-l load exist data and scan data\n");
    printf("\n");
}

//  clear && rm -rf WT_HOME && ./ex_hello -i 1
/*
[1698325054:332413][71578:0x7f9d57e49700], eviction-server: [WT_VERB_EVICTSERVER][DEBUG_2]: Eviction pass with: Max: 104857600 In use: 6172764 Dirty: 4978115
[1698325054:332484][71578:0x7f9d57e49700], eviction-server: [WT_VERB_MUTEX][DEBUG_2]: wait cache eviction server
[1698325054:334638][71578:0x7f9d57e49700], eviction-server: [WT_VERB_EVICTSERVER][DEBUG_2]: Eviction pass with: Max: 104857600 In use: 6172764 Dirty: 4978115
[1698325054:334728][71578:0x7f9d57e49700], file:access.wt, evict pass: [WT_VERB_EVICTSERVER][DEBUG_2]: file:access.wt walk: seen 55, queued 0
[1698325054:334735][71578:0x7f9d57e49700], eviction-server: [WT_VERB_MUTEX][DEBUG_2]: wait cache eviction server
[1698325054:336340][71578:0x7f9d57e49700], eviction-server: [WT_VERB_EVICTSERVER][DEBUG_2]: Eviction pass with: Max: 104857600 In use: 6172764 Dirty: 4978115
[1698325054:336370][71578:0x7f9d57e49700], eviction-server: [WT_VERB_MUTEX][DEBUG_2]: wait cache eviction server
[1698325054:338519][71578:0x7f9d57e49700], eviction-server: [WT_VERB_EVICTSERVER][DEBUG_2]: Eviction pass with: Max: 104857600 In use: 6172764 Dirty: 4978115
[1698325054:338543][71578:0x7f9d57e49700], eviction-server: [WT_VERB_MUTEX][DEBUG_2]: wait cache eviction server
[1698325054:340420][71578:0x7f9d57e49700], eviction-server: [WT_VERB_EVICTSERVER][DEBUG_2]: Eviction pass with: Max: 104857600 In use: 6172764 Dirty: 4978115
[1698325054:340463][71578:0x7f9d57e49700], eviction-server: [WT_VERB_MUTEX][DEBUG_2]: wait cache eviction server
[1698325054:342356][71578:0x7f9d57e49700], eviction-server: [WT_VERB_EVICTSERVER][DEBUG_2]: Eviction pass with: Max: 104857600 In use: 6172764 Dirty: 4978115
[1698325054:342411][71578:0x7f9d57e49700], eviction-server: [WT_VERB_MUTEX][DEBUG_2]: wait cache eviction server
[1698325054:344355][71578:0x7f9d57e49700], eviction-server: [WT_VERB_EVICTSERVER][DEBUG_2]: Eviction pass with: Max: 104857600 In use: 6172764 Dirty: 4978115
[1698325054:344413][71578:0x7f9d57e49700], eviction-server: [WT_VERB_MUTEX][DEBUG_2]: wait cache eviction server
[1698325054:346369][71578:0x7f9d57e49700], eviction-server: [WT_VERB_EVICTSERVER][DEBUG_2]: Eviction pass with: Max: 104857600 In use: 6172764 Dirty: 4978115
[1698325054:346394][71578:0x7f9d57e49700], eviction-server: [WT_VERB_MUTEX][DEBUG_2]: wait cache eviction server
[1698325054:348437][71578:0x7f9d57e49700], eviction-server: [WT_VERB_EVICTSERVER][DEBUG_2]: Eviction pass with: Max: 104857600 In use: 6172764 Dirty: 4978115
[1698325054:348477][71578:0x7f9d57e49700], eviction-server: [WT_VERB_MUTEX][DEBUG_2]: wait cache eviction server
[1698325054:350411][71578:0x7f9d57e49700], eviction-server: [WT_VERB_EVICTSERVER][DEBUG_2]: Eviction pass with: Max: 104857600 In use: 6172764 Dirty: 4978115
[1698325054:350451][71578:0x7f9d57e49700], eviction-server: [WT_VERB_MUTEX][DEBUG_2]: wait cache eviction server
[1698325054:352267][71578:0x7f9d57e49700], eviction-server: [WT_VERB_EVICTSERVER][DEBUG_2]: Eviction pass with: Max: 104857600 In use: 6172764 Dirty: 4978115
[1698325054:352318][71578:0x7f9d57e49700], file:access.wt, evict pass: [WT_VERB_EVICTSERVER][DEBUG_2]: file:access.wt walk: seen 55, queued 0
[1698325054:352324][71578:0x7f9d57e49700], eviction-server: [WT_VERB_MUTEX][DEBUG_2]: wait cache eviction server
[1698325054:354308][71578:0x7f9d57e49700], eviction-server: [WT_VERB_EVICTSERVER][DEBUG_2]: Eviction pass with: Max: 104857600 In use: 6172764 Dirty: 4978115
[1698325054:354340][71578:0x7f9d57e49700], eviction-server: [WT_VERB_MUTEX][DEBUG_2]: wait cache eviction server
*/

static void
access_example(int argc, char *argv[])
{
    /*! [access example connection] */
    WT_CONNECTION *conn;
    WT_CURSOR *cursor;
    WT_SESSION *session;
    const char  *value;
    int key;
    int ch;
    bool insertConfig = false;
    bool loadDataConfig = false;
    char cmd_buf[512];
    char buf[512];
    int i;
    WT_DECL_RET;
    int count = 0;
    const char *cmdflags = "i:l:";

/*    
    WT_RAND_STATE rnd;
    uint64_t rval;
    __wt_random_init_seed(NULL, &rnd);
    for (i = 0; i < 1000000; i++) {
        rval = __wt_random(&rnd);
        printf("yang test  random value:%lu\r\n", rval);
    }
*/
    
    /* Do a basic validation of options */
    while ((ch = __wt_getopt("ex_access", argc, argv, cmdflags)) != EOF) {
        switch (ch) {
        /* insert and scan data */
        case 'i':
            insertConfig = true;
            break;
        /* load and scan data */
        case 'l':
            loadDataConfig = true;
            break;
        case '?':
        default:
            usage();
            return;
        }
    }

    if (!insertConfig && !loadDataConfig) {
        usage();
        return;
    }

    /* prepare data */
    if (insertConfig) {
        (void)snprintf(cmd_buf, sizeof(cmd_buf), "rm -rf %s && mkdir %s", home, home);
        error_check(system(cmd_buf));

        /* Open a connection to the database, creating it if necessary. */
        //error_check(wiredtiger_open(home, NULL, "create,statistics=(all),verbose=[config_all_verbos:0, metadata:0, api:0]", &conn));
        error_check(wiredtiger_open(home, NULL, "checkpoint=[wait=60],eviction=(threads_min=1, threads_max=1),create, cache_size=1M, verbose=[block:0,reconcile:0,compact=0, api:0, config_all_verbos:0, metadata:0, api:0, evict:0]", &conn));
        //
       // error_check(wiredtiger_open(home, NULL, "create,statistics=(fast),statistics_log=(json,wait=1),in_memory=true", &conn));
                

        /* Open a session handle for the database. */
        error_check(conn->open_session(conn, NULL, NULL, &session));
        /*! [access example connection] */

        /*! [access example table create] */
        //针对server层的命令为: db.createCollection("sbtest4",{storageEngine: { wiredTiger: {configString: "leaf_page_max:4KB, leaf_key_max=4K"}}})
        error_check(session->create(session, "table:access", "memory_page_max=32K,key_format=q,value_format=S"));
        /*! [access example table create] */

        /*! [access example cursor open] */
        error_check(session->open_cursor(session, "table:access", NULL, NULL, &cursor));
        /*! [access example cursor open] */

        #define MAX_TEST_KV_NUM 8000000//1000000
         //insert
        for (i = 0; i < MAX_TEST_KV_NUM; i++) {
            snprintf(buf, sizeof(buf), "key%d", i);
            cursor->set_key(cursor, i);

            //value_item.data = "old value  ###############################################################################################################################################################################################################\0";
            //value_item.size = strlen(value_item.data) + 1;

            //printf("yang test 111111111111111111111111111111111111111111\r\n");
            cursor->set_value(cursor, "old value  ###############################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################\0");
            error_check(cursor->insert(cursor));
            if (i % 100 == 0) {
              //  printf("yang test xxxx.......sss..................11111.........................\r\n");
             //   __wt_sleep(0, 50000);
              //  printf("yang test xxxx.........................11111.........................\r\n");
            }
        }
        printf("yang test checkpoint.........................11111.........................\r\n");
        testutil_check(session->checkpoint(session, NULL));
        for (i = 150000; i < MAX_TEST_KV_NUM - 300000; i++) {
           // continue;
            snprintf(buf, sizeof(buf), "key%d", i);
            cursor->set_key(cursor, i);

            //value_item.data = "old value  ###############################################################################################################################################################################################################\0";
            //value_item.size = strlen(value_item.data) + 1;

            error_check(cursor->remove(cursor));
           // break;
        }
        printf("yang test checkpoint.........................222222.........................\r\n");
       // testutil_check(session->checkpoint(session, NULL));
       //error_check(session->compact(session, "table:access", NULL));
        //__wt_sleep(3, 0);

      //  testutil_check(conn->reconfigure(conn, "eviction_target=11,eviction_trigger=22, cache_size=1G,"
      //  "file_manager=(close_idle_time=10000), hash=(dhandle_buckets=1000), verbose=[config_all_verbos:5, metadata:0, api:0]"));

        // error_check(cursor->close(cursor));

        /*! [access example cursor insert] */

        /*! [access example cursor list] */
        /*error_check(cursor->reset(cursor));
        while ((ret = cursor->next(cursor)) == 0) {
            error_check(cursor->get_key(cursor, &key));
            error_check(cursor->get_value(cursor, &value));

            printf("Got record: %s : %s\n", key, value);
        }
        scan_end_check(ret == WT_NOTFOUND); */ /* Check for end-of-table. */
        /*! [access example cursor list] */

        /*! [access example close] */
        printf("yang test .........................close connection.................begin\r\n");
        error_check(conn->close(conn, NULL)); /* Close all handles. */
                                              /*! [access example close] */
    }

    /* load exist data, for example: when process restart, wo should warmup and load exist data*/
    if (loadDataConfig) {
        /* Open a connection to the database, creating it if necessary. block*/
        error_check(wiredtiger_open(home, NULL, "statistics=(all),verbose=[compact=5,config_all_verbos:0, block=0,metadata:0, api:0]", &conn));

        /* Open a session handle for the database. */
        error_check(conn->open_session(conn, NULL, NULL, &session));
        /*! [access example connection] */

        /*! [access example cursor open] */
        error_check(session->open_cursor(session, "table:access", NULL, "next_random=false", &cursor)); 
        //error_check(session->open_cursor(session, "table:access", NULL, "next_random=true,next_random_sample_size=10", &cursor));
        /*! [access example cursor open] */

      //  cursor->set_key(cursor, "key1");
      //  error_check(cursor->search(cursor));
      //  error_check(cursor->get_value(cursor, &value));
     //   printf("Load search record: %s : %s\n", "key5", value);

        error_check(cursor->reset(cursor)); /* Restart the scan. */
        //普通访问 __curfile_next__， 随机访问 __wt_curfile_next_random
        while ((ret = cursor->next(cursor)) == 0) {
            error_check(cursor->get_key(cursor, &key));
            error_check(cursor->get_value(cursor, &value));
            count++;

          //  printf("Load record : %d , count:%d\n", key, count);
           // if(count == 11)
            //    break;
        }

        //__wt_session_compact
        error_check(session->compact(session, "table:access", NULL));
       // scan_end_check(ret == WT_NOTFOUND); 
        /*! [access example cursor list] */

        /*! [access example close] */
        error_check(conn->close(conn, NULL)); /* Close all handles. */
                                              /*! [access example close] */
    }
}

#include <sys/resource.h>
#include <stdio.h>

static void print_memory_usage(void) {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    printf("@@@@@@@@@@@@@@@@@@@@@@@@@ Memory usage: %ld KB\n", usage.ru_maxrss);
}

/*! [statistics display function] */
static void
print_cursor(WT_CURSOR *cursor)
{
    const char *desc, *pvalue;
    int64_t value;
    int ret;

    while ((ret = cursor->next(cursor)) == 0) {
        error_check(cursor->get_value(cursor, &desc, &pvalue, &value));
       // printf("%s=%s\n", desc, pvalue);
        if (value != 0) {
            if (strcmp(desc, "connection: memory allocations") == 0 ||
                strcmp(desc, "connection: memory frees") == 0 ||
                strcmp(desc, "connection: memory re-allocations") == 0 ||
                strcmp(desc, "data-handle: connection data handles currently active") == 0 ||
                strcmp(desc, "data-handle: connection sweep dhandles closed") == 0 ||
                strcmp(desc, "data-handle: connection sweep dhandles removed from hash list") == 0 ||
                strcmp(desc, "data-handle: connection sweeps") == 0)
                
            printf("%s=%s\n", desc, pvalue);
        }
    }
}
/*! [statistics display function] */

static void
print_database_stats(WT_SESSION *session)
{
    WT_CURSOR *cursor;

    /*! [statistics database function] */
    error_check(session->open_cursor(session, "statistics:", NULL, NULL, &cursor));

    (void)print_cursor(cursor);
    error_check(cursor->close(cursor));
    /*! [statistics database function] */
}


static void
access_example2(int argc, char *argv[])
{
    /*! [access example connection] */
    WT_CONNECTION *conn;
    WT_CURSOR *cursor;
    WT_SESSION *session;
    const char  *value;
    int key;
    int ch;
    bool insertConfig = false;
    bool loadDataConfig = false;
    char cmd_buf[512];
    char buf[512];
    int i;
    WT_DECL_RET;
    int count = 0;
    const char *cmdflags = "i:l:";
    int table_count;

    /* Do a basic validation of options */
    while ((ch = __wt_getopt("ex_access", argc, argv, cmdflags)) != EOF) {
        switch (ch) {
        /* insert and scan data */
        case 'i':
            insertConfig = true;
            break;
        /* load and scan data */
        case 'l':
            loadDataConfig = true;
            break;
        case '?':
        default:
            usage();
            return;
        }
    }

    if (!insertConfig && !loadDataConfig) {
        usage();
        return;
    }

    /* prepare data */
    if (insertConfig) {
        (void)snprintf(cmd_buf, sizeof(cmd_buf), "rm -rf %s && mkdir -p %s/journal", home, home);
        error_check(system(cmd_buf));
        //file_manager=(close_handle_minimum=0,close_idle_time=3,close_scan_interval=1)

        /* Open a connection to the database, creating it if necessary. */
        //error_check(wiredtiger_open(home, NULL, "create,statistics=(all),verbose=[config_all_verbos:0, metadata:0, api:0]", &conn));
        //error_check(wiredtiger_open(home, NULL, "prefetch=(available=true,default=false),create,cache_size=3G,session_max=33000,eviction=(threads_min=4,threads_max=4),config_base=false,statistics=(fast),log=(enabled=true,remove=true,path=journal),builtin_extension_config=(zstd=(compression_level=6)),file_manager=(close_idle_time=5,close_scan_interval=5,close_handle_minimum=0),statistics_log=(wait=0),json_output=(error,message),verbose=[recovery_progress:1,checkpoint_progress:1,compact_progress:1,backup:0,checkpoint:0,compact:0,,history_store:0,recovery:0,rts:0,salvage:0,tiered:0,timestamp:0,transaction:0,verify:0,log:0],", &conn));
        error_check(wiredtiger_open(home, NULL, ",checkpoint=(log_size=0,wait=300),create,cache_size=3G,session_max=33000,eviction=(threads_min=4,threads_max=4),config_base=false,statistics=(fast),log=(enabled=true,remove=true,path=journal),builtin_extension_config=(zstd=(compression_level=6)),"
        "file_manager=(close_idle_time=10,close_scan_interval=5,close_handle_minimum=1),statistics_log=(wait=0),json_output=(error,message),", &conn));

        //error_check(wiredtiger_open(home, NULL, "verbose=[fileops:5], create,file_manager=(close_handle_minimum=0,close_idle_time=6,close_scan_interval=5)", &conn));

        //
       // error_check(wiredtiger_open(home, NULL, "create,statistics=(fast),statistics_log=(json,wait=1),in_memory=true", &conn));
                

        /* Open a session handle for the database. */
       // error_check(conn->open_session(conn, NULL, NULL, &session));
        /*! [access example connection] */

        print_memory_usage();
        #define MAX_TABLE_COUNT 10
        for (table_count = 0; table_count < MAX_TABLE_COUNT; table_count++) {
            error_check(conn->open_session(conn, NULL, NULL, &session));
            (void)snprintf(cmd_buf, sizeof(cmd_buf), "table:ycsb_lynn_%d", table_count);
            //针对server层的命令为: db.createCollection("sbtest4",{storageEngine: { wiredTiger: {configString: "leaf_page_max:4KB, leaf_key_max=4K"}}})
            error_check(session->create(session, cmd_buf, "key_format=q,value_format=S"));

            error_check(session->open_cursor(session, cmd_buf, NULL, NULL, &cursor));
            /*! [access example cursor open] */

            #define MAX_TEST_KV_NUM2 0//1000000
             //insert
            for (i = 0; i < MAX_TEST_KV_NUM2; i++) {
                snprintf(buf, sizeof(buf), "keyxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx_%d", i);
                cursor->set_key(cursor, i);
                cursor->set_value(cursor, buf);
                error_check(cursor->insert(cursor));
            }

            if (table_count == MAX_TABLE_COUNT - 1) {
                error_check(session->checkpoint(session, NULL));
                printf("yang test .................. create end\r\n\r\n\r\n ");
            }
            error_check(cursor->close(cursor));
            error_check(session->close(session, NULL));
            printf("yang test ...... table:%d,  ", table_count);
            print_memory_usage();
        }
       
       
       //__wt_sleep(1100,200);
       error_check(conn->open_session(conn, NULL, NULL, &session));
       for (table_count = 10001; table_count < 13000; table_count++) {
           /*(void)snprintf(cmd_buf, sizeof(cmd_buf), "table:ycsb_lynn_%d", 88888);
           //针对server层的命令为: db.createCollection("sbtest4",{storageEngine: { wiredTiger: {configString: "leaf_page_max:4KB, leaf_key_max=4K"}}})
           error_check(session->create(session, cmd_buf, "key_format=q,value_format=S"));
       
           error_check(session->open_cursor(session, cmd_buf, NULL, NULL, &cursor));
       
    #define MAX_TEST_KV_NUM3 10//1000000
            //insert
           for (i = 0; i < MAX_TEST_KV_NUM3; i++) {
               snprintf(buf, sizeof(buf), "keyxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx_%d", i);
               cursor->set_key(cursor, i);
               cursor->set_value(cursor, buf);
               error_check(cursor->insert(cursor));
           }
           error_check(cursor->close(cursor));*/
           print_database_stats(session);
           //error_check(session->close(session, NULL));
           
           __wt_sleep(2,200);
       }




       
        return;


        //(void)conn->debug_info(conn, "cache");
         error_check(conn->open_session(conn, NULL, NULL, &session));
        //for (table_count = 0; table_count < 600; table_count++) {
            
            (void)snprintf(cmd_buf, sizeof(cmd_buf), "table:ycsb_lynn_%d", 9999999);
            //针对server层的命令为: db.createCollection("sbtest4",{storageEngine: { wiredTiger: {configString: "leaf_page_max:4KB, leaf_key_max=4K"}}})
            error_check(session->create(session, cmd_buf, "key_format=q,value_format=S"));

            error_check(session->open_cursor(session, cmd_buf, NULL, NULL, &cursor));
            /*! [access example cursor open] */

    #define MAX_TEST_KV_NUMx 100000//1000000
             //insert
            for (i = 0; i < MAX_TEST_KV_NUMx; i++) {
               // error_check(session->checkpoint(session, NULL));
            
                snprintf(buf, sizeof(buf), "keyxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx_%d", i);
                cursor->set_key(cursor, i);
                cursor->set_value(cursor, buf);
                error_check(cursor->insert(cursor));

                //(void)conn->debug_info(conn, "cache");
                __wt_sleep(10,500);
            }
        //}

        error_check(session->close(session, NULL));
        __wt_sleep(1110011,11111111);
        
        error_check(conn->close(conn, NULL)); /* Close all handles. */
                                      /*! [access example close] */
    }

    /* load exist data, for example: when process restart, wo should warmup and load exist data*/
    if (loadDataConfig) {
        /* Open a connection to the database, creating it if necessary. block*/
        error_check(wiredtiger_open(home, NULL, "statistics=(all),verbose=[compact=5,config_all_verbos:0, block=0,metadata:0, api:0]", &conn));

        /* Open a session handle for the database. */
        error_check(conn->open_session(conn, NULL, NULL, &session));
        /*! [access example connection] */

        /*! [access example cursor open] */
        error_check(session->open_cursor(session, "table:access", NULL, "next_random=false", &cursor)); 
        //error_check(session->open_cursor(session, "table:access", NULL, "next_random=true,next_random_sample_size=10", &cursor));
        /*! [access example cursor open] */

      //  cursor->set_key(cursor, "key1");
      //  error_check(cursor->search(cursor));
      //  error_check(cursor->get_value(cursor, &value));
     //   printf("Load search record: %s : %s\n", "key5", value);

        error_check(cursor->reset(cursor)); /* Restart the scan. */
        //普通访问 __curfile_next__， 随机访问 __wt_curfile_next_random
        while ((ret = cursor->next(cursor)) == 0) {
            error_check(cursor->get_key(cursor, &key));
            error_check(cursor->get_value(cursor, &value));
            count++;

          //  printf("Load record : %d , count:%d\n", key, count);
           // if(count == 11)
            //    break;
        }

        //__wt_session_compact
        error_check(session->compact(session, "table:access", NULL));
       // scan_end_check(ret == WT_NOTFOUND); 
        /*! [access example cursor list] */

        /*! [access example close] */
        error_check(conn->close(conn, NULL)); /* Close all handles. */
                                              /*! [access example close] */
    }
}

/*
run step:
  step 1(prepare data):                ./ex_access -i 1
  step 2(warmup and load exist data):  ./ex_access -l 1
*/
int
main(int argc, char *argv[])
{
    access_example2(argc, argv);
    access_example(argc, argv);

    return (EXIT_SUCCESS);
}

