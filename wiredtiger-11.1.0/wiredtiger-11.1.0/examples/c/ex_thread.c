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
 * ex_thread.c
 *	This is an example demonstrating how to create and access a simple
 *	table from multiple threads.
 */

#include "test_util.h"

static const char *home;

#define NUM_THREADS 2

struct thread_conn {
    WT_CONNECTION *conn;
    int thread_num;
} g_thread_conn;

int g_thread_num = 0;


/*! [thread scan] */
static WT_THREAD_RET
scan_thread(void *conn_arg)
{
    WT_CONNECTION *conn;
    struct thread_conn* t_conn;
    WT_CURSOR *cursor;
    WT_SESSION *session;
    int ret;
    //const char *key, *value;
    int i;
    char buf[1024];
    WT_RAND_STATE rnd;
    const char* filename;
    char copy_buf[1024];
    char checkpoint[1024];
    
    g_thread_num++;
    __wt_random_init_seed(NULL, &rnd);

    t_conn = conn_arg;
    conn = t_conn->conn;
    error_check(conn->open_session(conn, NULL, NULL, &session));

    if (g_thread_num == 2) {
        __wt_verbose((WT_SESSION_IMPL *)session, WT_VERB_VERSION, "yang test backup...befor...:%s", "");
        error_check(session->open_cursor(session, "backup:", NULL, NULL, &cursor));
       // __wt_sleep(1, 0); 
        //__curbackup_next
        while ((ret = cursor->next(cursor)) == 0) {
            error_check(cursor->get_key(cursor, &filename));
            __wt_sleep(1, 0); 
            snprintf(copy_buf, 1024, "cp ./WT_HOME/%s ./wt_backup/WT_HOME/%s", filename, filename);
            system(copy_buf);
            __wt_verbose((WT_SESSION_IMPL *)session, WT_VERB_VERSION, "yang test backup......:%s", copy_buf);
        }
        __wt_verbose((WT_SESSION_IMPL *)session, WT_VERB_VERSION, "yang test backup...end...:%s", "");
    } else if (g_thread_num == 1)  {

        __wt_verbose((WT_SESSION_IMPL *)session, WT_VERB_VERSION, "yang test checkpoint befor.:%s", "");
        for (i=1111;i > 0 ; i--) {
            error_check(conn->open_session(conn, NULL, NULL, &session));
            error_check(session->open_cursor(session, "table:access", NULL, NULL, &cursor));
            snprintf(buf, sizeof(buf), "value @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ %d", i);
            cursor->set_key(cursor, i);
            cursor->set_value(cursor, buf);
            
            error_check(cursor->insert(cursor));
            if (i%300 == 0) {
                snprintf(checkpoint, 1024, "name=midnight%d", i);
                error_check(session->checkpoint(session, checkpoint)); 
            }
            error_check(cursor->close(cursor));
            error_check(session->close(session, NULL));
        }
        __wt_sleep(21, 0);
        exit(0);
        
        //__wt_sync_file������sleep
        error_check(session->checkpoint(session, NULL));
        __wt_verbose((WT_SESSION_IMPL *)session, WT_VERB_VERSION, "yang test checkpoint end.:%s", "");

        for (i=2111;i > 1111 ; i--) {
            snprintf(buf, sizeof(buf), "value @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ %d", i);
            cursor->set_key(cursor, i);
            cursor->set_value(cursor, buf);

            error_check(cursor->insert(cursor));
        }
       // error_check(session->checkpoint(session, NULL));
        printf("yang test 1111111111111111111111111111  checkpoint end insert\r\n");
    }

    return (WT_THREAD_RET_VALUE);
}

static void
access_example2(void)
{
    /*! [access example connection] */
    WT_CONNECTION *conn;
    WT_CURSOR *cursor;
    WT_SESSION *session;
    const char *value;
    int ret;
   // int64_t num;
    int key;

    /* Open a connection to the database, creating it if necessary. */
    error_check(wiredtiger_open(home, NULL, "create,statistics=(all),log=(enabled:true,recover=on,remove=true),"
    "verbose=[checkpoint_progress:5, metadata:5, checkpoint_cleanup:5, config_all_verbos:5,checkpoint:5,timestamp:5,version:5, metadata:0, recovery:5, recovery_progress:5, log:5]", &conn));

    error_check(conn->open_session(conn, NULL, NULL, &session));
    //error_check(session->create(session, "table:access", "memory_page_max=32K,key_format=q,value_format=S"));
    error_check(session->open_cursor(session, "table:access", NULL, NULL, &cursor));

    error_check(conn->open_session(conn, NULL, NULL, &session));
    error_check(session->open_cursor(session, "table:access", NULL, NULL, &cursor));
    while ((ret = cursor->next(cursor)) == 0) {
        error_check(cursor->get_key(cursor, &key));
        error_check(cursor->get_value(cursor, &value));

        printf("recover, Got cursor record: %d : %s\n", key, value);
    }
    exit(0);
}

int
main(int argc, char *argv[])
{
    //WT_CONNECTION *conn = g_thread_conn.conn;
    WT_SESSION *session;
    WT_CURSOR *cursor;
    wt_thread_t threads[NUM_THREADS];
    int i;
    char buf[1024];
    
    //access_example2();
    
    
    home = example_setup(argc, argv);

    error_check(wiredtiger_open(home, NULL, "create, cache_size:1M,checkpoint=[wait=60],"
    "verbose=[reconcile:0,metadata:5, version:5, transaction:0, block:0, checkpoint:2, write:0,split:0, evict:0, log:0,api:0,config_all_verbos:0,fileops:0], "
        "log=(enabled,file_max=10M),checkpoint=(log_size=0,wait=100)", &g_thread_conn.conn));

    error_check(g_thread_conn.conn->open_session(g_thread_conn.conn, NULL, NULL, &session));
    error_check(session->create(session, "table:access", "key_format=q,value_format=S"));
    error_check(session->open_cursor(session, "table:access", NULL, "overwrite", &cursor));
    cursor->set_key(cursor, 11111);
    cursor->set_value(cursor, "value1");
    error_check(cursor->insert(cursor));
   // error_check(session->close(session, NULL));


    error_check(session->open_cursor(session, "table:access", NULL, NULL, &cursor));
    printf("prepare data\r\n");
    for (i=201100;i > 200000 ; i--) {
        snprintf(buf, sizeof(buf), "value @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ %d", i);
        cursor->set_key(cursor, i);
        cursor->set_value(cursor, buf);
    
        error_check(cursor->insert(cursor));
    }
    error_check(session->checkpoint(session, "name=midnight")); 


    for (i=201200;i > 201100 ; i--) {
        snprintf(buf, sizeof(buf), "value @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ %d", i);
        cursor->set_key(cursor, i);
        cursor->set_value(cursor, buf);
    
        error_check(cursor->insert(cursor));
    }
    error_check(session->checkpoint(session, "name=midnight2")); 
    error_check(session->close(session, NULL));

    for (i = 0; i < NUM_THREADS; i++) {
        error_check(__wt_thread_create(NULL, &threads[i], scan_thread, &g_thread_conn));
        __wt_sleep(0, 500);
    }
    for (i = 0; i < NUM_THREADS; i++)
        error_check(__wt_thread_join(NULL, &threads[i]));

    error_check(g_thread_conn.conn->close(g_thread_conn.conn, NULL));

    return (EXIT_SUCCESS);
    access_example2();
}

/*
#include <sys/wait.h>
#include <pthread.h>
#include <stdio.h>
#include<string.h> 
#include<errno.h>
#include <unistd.h> 

static pthread_mutex_t bio_mutex ;
static pthread_cond_t bio_newjob_cond;
static unsigned long long bio_pending=0;
static unsigned long long count = 0;

static void *test_thread(void *arg)
{
    
    sigset_t sigset;
    char *p = (char*)arg;
    p = NULL;
    (void)(p);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    pthread_mutex_lock(&bio_mutex);
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGALRM);
    if (pthread_sigmask(SIG_BLOCK, &sigset, NULL))
        printf("Warning: can't mask SIGALRM in bio.c thread: %s", strerror(errno));
    usleep(1000);
    while(1) {
           //The loop always starts with the lock hold. 
        if (bio_pending == 0) {
            pthread_cond_wait(&bio_newjob_cond,&bio_mutex);
        }
        pthread_mutex_unlock(&bio_mutex);
        
        for(int j = 0; j < 100; j++) {
            count++;
            count--;
        }
        
        pthread_mutex_lock(&bio_mutex);
        bio_pending--;
        count++;
    }

    return 0;
}
*/

/*! [thread main]
int
main(int argc, char *argv[])
{
    pthread_t threads;
    int i;
    size_t stacksize;
    pthread_attr_t attr;
    time_t T1, T2;
    void *arg;
    double time_diff;
    (void)(argc);
    (void)(argv);

    pthread_mutex_init(&bio_mutex,NULL);
    pthread_cond_init(&bio_newjob_cond,NULL);


#define REDIS_THREAD_STACK_SIZE (1024*1024*4)
    
    pthread_attr_init(&attr);
    pthread_attr_getstacksize(&attr,&stacksize);
    if (!stacksize) stacksize = 1; 
    while (stacksize < REDIS_THREAD_STACK_SIZE) stacksize *= 2;
    pthread_attr_setstacksize(&attr, stacksize);

    arg = (void*)(unsigned long) 0;
    pthread_create(&threads,&attr,test_thread,arg);

    
    T1 = time(NULL);
    for (i=0;i<9000000;i++) {
        pthread_mutex_lock(&bio_mutex);
        bio_pending++;
        for(int j = 0; j < 10000; j++) {
            bio_pending++;
            bio_pending--;
        }    
        pthread_cond_signal(&bio_newjob_cond);
        pthread_mutex_unlock(&bio_mutex);
    }
    T2 = time(NULL);
    time_diff = difftime(T2, T1);
    printf("yang tst.........:%f, %llu\r\n", time_diff, count);
    usleep(10000000);

    return (1);
}
*/


