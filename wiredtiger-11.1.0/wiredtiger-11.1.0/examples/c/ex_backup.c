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
 * ex_backup.c
 * 	demonstrates how to use incremental backup and log files.
 */
#include <test_util.h>

static const char *const home = "WT_HOME_LOG";
static const char *const home_full = "WT_HOME_LOG_FULL";
static const char *const home_incr = "WT_HOME_LOG_INCR";

static const char *const full_out = "./backup_full";
static const char *const incr_out = "./backup_incr";

static const char *const uri = "table:logtest";

//yang add todo xxxxxxxxxxxxxxxxxxxxx
//db.adminCommand({setParameter:1, wiredTigerEngineRuntimeConfig:'log=(remove=false) '})支持可配置，默认remove为ture，我们支持可配置的话，就可以实现增量数据备份

//#define CONN_CONFIG "create,cache_size=100MB,log=(enabled=true,file_max=100K,remove=false)"  要支持增量备份，remove必须为false，否则报错incremental log file backup not possible when automatic log removal configured: Invalid argument
#define CONN_CONFIG "verbose=[config_all_verbos:5, api:0, log:0], create,cache_size=100MB,log=(enabled=true,file_max=100K,remove=false)" //yang add change remove修改

/*
对应日志:
Adding initial data
Taking initial backup
Iteration 1: adding data
Iteration 1: taking full backup
Iteration 1: taking incremental backup
Iteration 1: dumping and comparing data
yang test ..compare_backups: cmp ./backup_full.1 ./backup_incr.1
Iteration 1: Tables ./backup_full.1 and ./backup_incr.1 identical
Iteration 2: adding data
Iteration 2: taking full backup
Iteration 2: taking incremental backup
Iteration 2: dumping and comparing data
yang test ..compare_backups: cmp ./backup_full.2 ./backup_incr.2
Iteration 2: Tables ./backup_full.2 and ./backup_incr.2 identical
Iteration 3: adding data
Iteration 3: taking full backup
Iteration 3: taking incremental backup
Iteration 3: dumping and comparing data
yang test ..compare_backups: cmp ./backup_full.3 ./backup_incr.3
Iteration 3: Tables ./backup_full.3 and ./backup_incr.3 identical
Iteration 4: adding data
Iteration 4: taking full backup
Iteration 4: taking incremental backup
Iteration 4: dumping and comparing data
yang test ..compare_backups: cmp ./backup_full.4 ./backup_incr.4
Iteration 4: Tables ./backup_full.4 and ./backup_incr.4 identical
Final comparison: dumping and comparing data
yang test ..compare_backups: cmp ./backup_full.0 ./backup_incr.0
Iteration MAIN: Tables ./backup_full.0 and ./backup_incr.0 identical

最终都是backup_full.i和backup_incr.i内容相比较
*/

//WT_HOME_LOG_INCR.5存储的是第1轮的inc + 第2轮的inc + .... 第5轮的inc
//WT_HOME_LOG_FULL.5也就是第5轮时候的full, 他的内容和WT_HOME_LOG_INCR.5中的内容最终相等

//WT_HOME_LOG_INCR.4存储的是第1轮的inc + 第2轮的inc + .... 第4轮的inc
//WT_HOME_LOG_FULL.4也就是第4轮时候的full, 他的内容实际上和WT_HOME_LOG_INCR.4中的内容最终相等

//拷贝几份
#define MAX_ITERATIONS 5
#define MAX_KEYS 10000

static int
compare_backups(int i)
{
    int ret;
    char buf[1024], msg[32];

    /*
     * We run 'wt dump' on both the full backup directory and the incremental backup directory for
     * this iteration. Since running 'wt' runs recovery and makes both directories "live", we need a
     * new directory for each iteration.
     *
     * If i == 0, we're comparing against the main, original directory with the final incremental
     * directory.
     */
    //把log表的数据dump出来存入backup_full.x中  ./../wt -R -h WT_HOME_LOG_FULL.1 dump logtest > ./backup_full.1
    if (i == 0)
        (void)snprintf(
          buf, sizeof(buf), "../../wt -R -h %s dump logtest > %s.%d", home, full_out, i);
    else
        (void)snprintf(
          buf, sizeof(buf), "../../wt -R -h %s.%d dump logtest > %s.%d", home_full, i, full_out, i);
    error_check(system(buf));
    printf("yang test compare_backups .......1..........%s\r\n", buf);
    /*
     * Now run dump on the incremental directory.
     */
    //把log表的数据dump出来存入backup_incr.x中 ./../wt -R -h WT_HOME_LOG_INCR.1 dump logtest > ./backup_incr.1
    (void)snprintf(
      buf, sizeof(buf), "../../wt -R -h %s.%d dump logtest > %s.%d", home_incr, i, incr_out, i);
    error_check(system(buf));
    printf("yang test compare_backups .......2..........%s\r\n", buf);

    /*
     * Compare the files.
     */
    (void)snprintf(buf, sizeof(buf), "cmp %s.%d %s.%d", full_out, i, incr_out, i);
    ret = system(buf);
    //cmp ./backup_full.1 ./backup_incr.1
    printf("yang test compare_backups .......1..........%s\r\n", buf);
    if (i == 0)
        (void)snprintf(msg, sizeof(msg), "%s", "MAIN");
    else
        (void)snprintf(msg, sizeof(msg), "%d", i);
    printf("Iteration %s: Tables %s.%d and %s.%d %s\n", msg, full_out, i, incr_out, i,
      ret == 0 ? "identical" : "differ");
    //if (ret != 0) //cmp比较内容不一致，则直接退出    yang add change xxxxx 屏蔽这个的目的是保证所有输出日志打印
    //    exit(1);

    /*
     * If they compare successfully, clean up.
     */
    if (i != 0) {
        (void)snprintf(buf, sizeof(buf), "rm -rf %s.%d %s.%d %s.%d %s.%d", home_full, i, home_incr,
          i, full_out, i, incr_out, i);
        //error_check(system(buf));
        printf("yang test compare_backups .......3..........%s\r\n", buf);
    }
    return (ret);
}

/*
 * Set up all the directories needed for the test. We have a full backup directory for each
 * iteration and an incremental backup for each iteration. That way we can compare the full and
 * incremental each time through.
 */
static void
setup_directories(void)
{
    int i;
    char buf[1024];

    for (i = 0; i < MAX_ITERATIONS; i++) {
        /*
         * For incremental backups we need 0-N. The 0 incremental directory will compare with the
         * original at the end.
         */
        (void)snprintf(buf, sizeof(buf), "rm -rf %s.%d && mkdir %s.%d", home_incr, i, home_incr, i);
        error_check(system(buf));
        printf("yang test setup_directories .......1..........%s\r\n", buf);
        if (i == 0)
            continue;
        /*
         * For full backups we need 1-N.
         */
        (void)snprintf(buf, sizeof(buf), "rm -rf %s.%d && mkdir %s.%d", home_full, i, home_full, i);
        error_check(system(buf));
        printf("yang test setup_directories .......2..........%s\r\n", buf);
    }
}

static void
add_work(WT_SESSION *session, int iter)
{
    WT_CURSOR *cursor;
    int i;
    char k[32], v[32];

    error_check(session->open_cursor(session, uri, NULL, NULL, &cursor));
    /*
     * Perform some operations with individual auto-commit transactions.
     */
    for (i = 0; i < MAX_KEYS; i++) {
        (void)snprintf(k, sizeof(k), "key.%d.%d", iter, i);
        (void)snprintf(v, sizeof(v), "value.%d.%d", iter, i);
        cursor->set_key(cursor, k);
        cursor->set_value(cursor, v);
        error_check(cursor->insert(cursor));
    }
    error_check(cursor->close(cursor));
}

//假设一共需要MAX_ITERATIONS为5轮，则
//take_full_backup: full backup也就是把某轮backup时候的全量文件拷贝到WT_BLOCK_LOG_FULL.i目录，最终
//  WT_BLOCK_LOG_FULL.0目录数据代表第一轮的全量文件备份，WT_BLOCK_LOG_FULL.1目录数据代表第2轮的全量文件备份，以此类推

//take_incr_backup: inc backup也就是把某轮获取到的wal文件拷贝到前面每轮对应目录，最终
//  WT_HOME_LOG_INCR.0存储第一轮的WAL文件，WT_HOME_LOG_INCR.0存储第0轮和第1轮的WAL文件，WT_HOME_LOG_INCR.1存储第0轮、第1轮、第2轮的WAL文件，
//  也就是WT_HOME_LOG_INCR.2中存储的文件包括前3个周期内拷贝的wal文件，WT_HOME_LOG_INCR.4中存储的文件包括前5个周期内拷贝的wal文件，
static void
take_full_backup(WT_SESSION *session, int i)
{
    WT_CURSOR *cursor;
    int j, ret;
    char buf[1024], h[256];
    const char *filename, *hdir;

    /*
     * First time through we take a full backup into the incremental directories. Otherwise only
     * into the appropriate full directory.
     */
    if (i != 0) {
        (void)snprintf(h, sizeof(h), "%s.%d", home_full, i);
        hdir = h;
    } else
        hdir = home_incr;
    //__wt_curbackup_open
    error_check(session->open_cursor(session, "backup:", NULL, NULL, &cursor));

    //__curbackup_next
    while ((ret = cursor->next(cursor)) == 0) {
        //__curbackup_next中会获取到key,这里通过__wt_cursor_get_key获取这个key，存入filename
        error_check(cursor->get_key(cursor, &filename));
        if (i == 0) //为0的时候拷贝多份
            /*
             * Take a full backup into each incremental directory.
             */
            for (j = 0; j < MAX_ITERATIONS; j++) {
                (void)snprintf(h, sizeof(h), "%s.%d", home_incr, j); //注意这里是home_incr 代表增量文件，因为0的时候没有做checkpoint
                (void)snprintf(buf, sizeof(buf), "cp %s/%s %s/%s", home, filename, h, filename);
                error_check(system(buf));
                printf("yang test ex backup ....take_full_backup.............%s\r\n", buf);
            }
        else {//不为0只拷贝一份
            (void)snprintf(h, sizeof(h), "%s.%d", home_full, i); //注意这里不为0的时候为home_full，因为不为0的时候都会提前做checkpoint
            (void)snprintf(buf, sizeof(buf), "cp %s/%s %s/%s", home, filename, hdir, filename);
            error_check(system(buf));
            printf("yang test ex backup ....take_full_backup.............%s\r\n", buf);
        }
        
    }
    scan_end_check(ret == WT_NOTFOUND);
    error_check(cursor->close(cursor));
}

//假设一共需要MAX_ITERATIONS为5轮，则
//take_full_backup: full backup也就是把某轮backup时候的全量文件拷贝到WT_BLOCK_LOG_FULL.i目录，最终
//  WT_BLOCK_LOG_FULL.0目录数据代表第一轮的全量文件备份，WT_BLOCK_LOG_FULL.1目录数据代表第2轮的全量文件备份，以此类推

//take_incr_backup: inc backup也就是把某轮获取到的wal文件拷贝到前面每轮对应目录，最终
//  WT_HOME_LOG_INCR.0存储第一轮的WAL文件，WT_HOME_LOG_INCR.0存储第0轮和第1轮的WAL文件，WT_HOME_LOG_INCR.1存储第0轮、第1轮、第2轮的WAL文件，
//  也就是WT_HOME_LOG_INCR.2中存储的文件包括前3个周期内拷贝的wal文件，WT_HOME_LOG_INCR.4中存储的文件包括前5个周期内拷贝的wal文件，
static void
take_incr_backup(WT_SESSION *session, int i)
{
    WT_CURSOR *cursor;
    int j, ret;
    char buf[1024], h[256];
    const char *filename;

    error_check(session->open_cursor(session, "backup:", NULL, "target=(\"log:\")", &cursor));

    while ((ret = cursor->next(cursor)) == 0) {
        error_check(cursor->get_key(cursor, &filename));
        /*
         * Copy into the 0 incremental directory and then each of the incremental directories for
         * this iteration and later.
         */
        (void)snprintf(h, sizeof(h), "%s.0", home_incr);
        (void)snprintf(buf, sizeof(buf), "cp %s/%s %s/%s", home, filename, h, filename);
        error_check(system(buf));
        printf("yang test ex backup .....take_incr_backup...1.........%s\r\n", buf);
        for (j = i; j < MAX_ITERATIONS; j++) {
            (void)snprintf(h, sizeof(h), "%s.%d", home_incr, j);
            (void)snprintf(buf, sizeof(buf), "cp %s/%s %s/%s", home, filename, h, filename);
            error_check(system(buf));
            printf("yang test ex backup .....take_incr_backup....2........%s\r\n", buf);
        }
    }
    scan_end_check(ret == WT_NOTFOUND);

    /*
     * With an incremental cursor, we want to truncate on the backup cursor to remove the logs. Only
     * do this if the copy process was entirely successful.
     */
    /*! [Truncate a backup cursor] */
    error_check(session->truncate(session, "log:", cursor, NULL, NULL));
    /*! [Truncate a backup cursor] */
    error_check(cursor->close(cursor));
}

int
main(int argc, char *argv[])
{
    WT_CONNECTION *wt_conn;
    WT_SESSION *session;
    int i;
    char cmd_buf[256];

    (void)argc; /* Unused variable */
    (void)testutil_set_progname(argv);

    (void)snprintf(cmd_buf, sizeof(cmd_buf), "rm -rf %s && mkdir %s", home, home);
    error_check(system(cmd_buf));
    error_check(wiredtiger_open(home, NULL, CONN_CONFIG, &wt_conn));

    setup_directories();
    error_check(wt_conn->open_session(wt_conn, NULL, NULL, &session));
    error_check(session->create(session, uri, "key_format=S,value_format=S"));
    printf("Adding initial data\n");
    //准备MAX_KEYS条数据
    add_work(session, 0);
    //__wt_sleep(10, 10000);

    printf("Taking initial backup\n");
    error_check(session->checkpoint(session, NULL));
    add_work(session, 1); //yang add change
    printf("Taking begin backup\n");
    take_full_backup(session, 1);
    return (0); //yang add change

    
    for (i = 1; i < MAX_ITERATIONS; i++) {
        printf("Iteration %d: adding data\n", i);
        add_work(session, i);
        error_check(session->checkpoint(session, NULL));
        __wt_sleep(10, 10000);
        /*
         * The full backup here is only needed for testing and comparison purposes. A normal
         * incremental backup procedure would not include this.
         */
        printf("Iteration %d: taking full backup\n", i);
        take_full_backup(session, i);
        /*
         * Taking the incremental backup also calls truncate to remove the log files, if the copies
         * were successful. See that function for details on that call.
         */
        printf("Iteration %d: taking incremental backup\n", i);
        take_incr_backup(session, i);

        printf("Iteration %d: dumping and comparing data\n", i);
        //error_check(compare_backups(i)); yang add change todo xxxxx 忽略错误，保证print正常输出
        WT_IGNORE_RET(compare_backups(i));
    }

    /*
     * Close the connection. We're done and want to run the final comparison between the incremental
     * and original.
     */
    error_check(wt_conn->close(wt_conn, NULL));

    printf("Final comparison: dumping and comparing data\n");
    //error_check(compare_backups(0));
    WT_IGNORE_RET(compare_backups(0));

    return (EXIT_SUCCESS);
}
