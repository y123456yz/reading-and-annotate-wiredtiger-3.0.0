
/*
Adding initial data
Taking initial backup
Iteration 1: adding data
Iteration 1: taking full backup
Iteration 1: taking incremental backup
Existing incremental ID string: ID0
Iteration 1: dumping and comparing data
yang test ..compare_backups: cmp ./backup_block_full.1 ./backup_block_incr.1
Iteration 1: Tables ./backup_block_full.1 and ./backup_block_incr.1 identical
Iteration 2: adding data
Iteration 2: taking full backup
Iteration 2: taking incremental backup
Existing incremental ID string: ID0
Existing incremental ID string: ID1
Iteration 2: dumping and comparing data
yang test ..compare_backups: cmp ./backup_block_full.2 ./backup_block_incr.2
Iteration 2: Tables ./backup_block_full.2 and ./backup_block_incr.2 identical
Iteration 3: adding data
Iteration 3: taking full backup
Iteration 3: taking incremental backup
Existing incremental ID string: ID2
Existing incremental ID string: ID1
Iteration 3: dumping and comparing data
yang test ..compare_backups: cmp ./backup_block_full.3 ./backup_block_incr.3
Iteration 3: Tables ./backup_block_full.3 and ./backup_block_incr.3 identical
Iteration 4: adding data
Iteration 4: taking full backup
Iteration 4: taking incremental backup
Existing incremental ID string: ID2
Existing incremental ID string: ID3
Iteration 4: dumping and comparing data
yang test ..compare_backups: cmp ./backup_block_full.4 ./backup_block_incr.4
Iteration 4: Tables ./backup_block_full.4 and ./backup_block_incr.4 identical
Close and reopen the connection
Verify query after reopen
Existing incremental ID string: ID4
Existing incremental ID string: ID3
Final comparison: dumping and comparing data
yang test ..compare_backups: cmp ./backup_block_full.0 ./backup_block_incr.0
Iteration MAIN: Tables ./backup_block_full.0 and ./backup_block_incr.0 identical

最终都是backup_block_full.i和backup_block_incr.i内容相比较
*/


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
 * ex_backup_block.c
 * 	demonstrates how to use block-based incremental backup.
 */
#include <test_util.h>

static const char *const home = "WT_BLOCK";
static const char *const home_full = "WT_BLOCK_LOG_FULL";
static const char *const home_incr = "WT_BLOCK_LOG_INCR";
static const char *const logpath = "logpath";

#define WTLOG "WiredTigerLog"
#define WTLOGLEN strlen(WTLOG)

static const char *const full_out = "./backup_block_full";
static const char *const incr_out = "./backup_block_incr";

static const char *const uri = "table:main";
static const char *const uri2 = "table:extra";

static int g_insert_num = 0;

typedef struct __filelist {
    const char *name;
    bool exist;
} FILELIST;

static FILELIST *last_flist = NULL;
static size_t filelist_count = 0;

#define FLIST_INIT 16

#define CONN_CONFIG "create,cache_size=100MB,verbose=[backup:5, split:0, reconcile:0],log=(enabled=true,path=logpath,file_max=100K)"
#define MAX_ITERATIONS 5






//yang add todo xxxxxxxxxxx  wt工具只读方式下可以分析wt文件，当前毕现要停止mongod才可以分析
//yang add todo xxxxxxxxxxx  wt backup只读模式下物理备份支持，类似percona mysql物理备份工具XtraBackup
//mongod开启db.setLogLevel(5, 'query');日志中的执行计划信息无法序列化，例如下面一行日志，日志打印无法序列化:
//  {"bestSolution":"FETCH\n---nodeId = 2\n---fetched = 1\n---sortedByDiskLoc = 1\n---providedSorts = {baseSortPattern: {}, ignoredFields: [filed1, filed2]}\n---Child:\n------IXSCAN\n---------indexName = filed1_1_filed2_1\n---------keyPattern = { filed1: 1.0, filed2: 1.0 }\n---------direction = 1\n---------bounds = field #0['filed1']: [\"a10\", \"a10\"], field #1['filed2']: [\"b10\", \"b10\"]\n---------iets = (iets { filed1: 1.0, filed2: 1.0 } (filed1: 1.0 (eval $eq #0)) (filed2: 1.0 (eval $eq #1)))\n---------nodeId = 1\n---------fetched = 0\n---------sortedByDiskLoc = 1\n---------providedSorts = {baseSortPattern: {}, ignoredFields: [filed1, filed2]}\n"}
// 改为这样就可以{\"bestSolution\":\"FETCH\n---nodeId = 2\n---fetched = 1\n---sortedByDiskLoc = 1\n---providedSorts = {baseSortPattern: {}, ignoredFields: [filed1, filed2]}\n---Child:\n------IXSCAN\n---------indexName = filed1_1_filed2_1\n---------keyPattern = { filed1: 1.0, filed2: 1.0 }\n---------direction = 1\n---------bounds = field #0['filed1']: [\"a10\", \"a10\"], field #1['filed2']: [\"b10\", \"b10\"]\n---------iets = (iets { filed1: 1.0, filed2: 1.0 } (filed1: 1.0 (eval $eq #0)) (filed2: 1.0 (eval $eq #1)))\n---------nodeId = 1\n---------fetched = 0\n---------sortedByDiskLoc = 1\n---------providedSorts = {baseSortPattern: {}, ignoredFields: [filed1, filed2]}\n\"}
// 测试方法，写一个c的demo，prinf验证打印这个字符串


//yang add todo xxxxxxxxxxxxxxxxxx  如果这里MAX_KEYS为10000的话，WT_BACKUP_RANGE会更多，如果改为100000，则WT_BACKUP_RANGE会
//  更少,这是因为用10000的时候，第一次checkpoint后即使只写一条数据，第二次checkpoint还是会大量split，具体原因还没分析
#define MAX_KEYS 100000
//inc backup原理实际上主要就是把两次inc期间修改的xx.wt数据文件块和wal日志文件拷贝出来，实际用的时候一般只需要备份两个增量备份期间变化的ext文件块，例如两次inc期间
//  wt已经做了5次checkpoint， 则第二次相比第一次需要拷贝这5次checkpoint期间变化的ext文件块(包含offset和size)

static int
compare_backups(int i)
{
    int ret;
    char buf[1024], msg[32];
    //if(i == 4)
     //   return 0;//yang add change

    /*
     * We run 'wt dump' on both the full backup directory and the incremental backup directory for
     * this iteration. Since running 'wt' runs recovery and makes both directories "live", we need a
     * new directory for each iteration.
     *
     * If i == 0, we're comparing against the main, original directory with the final incremental
     * directory.
     */
    if (i == 0) {
        testutil_system("../../wt -R -h %s dump main > %s.%d", home, full_out, i);
        printf("yang test ..compare_backups: ../../wt -R -h %s dump main > %s.%d\r\n", home, full_out, i);
    } else {
        testutil_system("../../wt -R -h %s.%d dump main > %s.%d", home_full, i, full_out, i);
        printf("yang test ..compare_backups: ../../wt -R -h %s.%d dump main > %s.%d\r\n", home_full, i, full_out, i);
    }
    /*
     * Now run dump on the incremental directory.
     */
    testutil_system("../../wt -R -h %s.%d dump main > %s.%d", home_incr, i, incr_out, i);
    printf("yang test ..compare_backups: ../../wt -R -h %s.%d dump main > %s.%d\r\n", home_incr, i, incr_out, i);
    
    /*
     * Compare the files.
     */
    (void)snprintf(buf, sizeof(buf), "cmp %s.%d %s.%d", full_out, i, incr_out, i);
    printf("yang test ..compare_backups: %s\r\n", buf);
    ret = system(buf);
    if (i == 0)
        (void)snprintf(msg, sizeof(msg), "%s", "MAIN");
    else
        (void)snprintf(msg, sizeof(msg), "%d", i);
    printf("Iteration %s: Tables %s.%d and %s.%d %s\n", msg, full_out, i, incr_out, i,
      ret == 0 ? "identical" : "differ");
    if (ret != 0)
        exit(1);

    /*
     * If they compare successfully, clean up.
     */
    //if (i != 0) {//yang add change
    if (0) {
        testutil_system(
          "rm -rf %s.%d %s.%d %s.%d %s.%d", home_full, i, home_incr, i, full_out, i, incr_out, i);
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

    for (i = 0; i < MAX_ITERATIONS; i++) {
        /*
         * For incremental backups we need 0-N. The 0 incremental directory will compare with the
         * original at the end.
         */
        testutil_system("rm -rf %s.%d && mkdir -p %s.%d/%s", home_incr, i, home_incr, i, logpath);
        if (i == 0)
            continue;
        /*
         * For full backups we need 1-N.
         */
        testutil_system("rm -rf %s.%d && mkdir -p %s.%d/%s", home_full, i, home_full, i, logpath);
    }
}

static void
add_work(WT_SESSION *session, int iter, int iterj)
{
    WT_CURSOR *cursor, *cursor2;
    int i;
    char k[64], v[64];


    error_check(session->open_cursor(session, uri, NULL, NULL, &cursor));
    /*
     * Only on even iterations add content to the extra table. This illustrates and shows that
     * sometimes only some tables will be updated.
     */
    cursor2 = NULL;
    //if (iter % 2 == 0)
    //    error_check(session->open_cursor(session, uri2, NULL, NULL, &cursor2));
    /*
     * Perform some operations with individual auto-commit transactions.
     */
    for (i = 0; i < MAX_KEYS; i++) {
        g_insert_num++;
        (void)snprintf(k, sizeof(k), "key.%d.%d.%d", iter, iterj, i);
        (void)snprintf(v, sizeof(v), "value.%d.%d.%d", iter, iterj, i);
        //printf("yang test ..........add work:%s\r\n", k);
        cursor->set_key(cursor, k);
        cursor->set_value(cursor, v);
        error_check(cursor->insert(cursor));
        if (cursor2 != NULL) {
            cursor2->set_key(cursor2, k);
            cursor2->set_value(cursor2, v);
            error_check(cursor2->insert(cursor2));
        }
    }
  /*  
    error_check(session->checkpoint(session, "force=true"));
    (void)snprintf(k, sizeof(k), "key.%d.%d.%d", iter, iterj, 111111);
    (void)snprintf(v, sizeof(v), "value.%d.%d.%d", iter, iterj, 111111);
    //printf("yang test ..........add work:%s\r\n", k);
    cursor->set_key(cursor, k);
    cursor->set_value(cursor, v);
    error_check(cursor->insert(cursor));
    printf("\r\n\r\n\r\n\r\n\r\n");
    error_check(session->checkpoint(session, "force=true"));
*/
    error_check(cursor->close(cursor));
    if (cursor2 != NULL)
        error_check(cursor2->close(cursor2));
   // exit(0);
}


static void
add_inc_work(WT_SESSION *session, int iter, int iterj)
{
    WT_CURSOR *cursor, *cursor2;
    int i;
    char k[64], v[64];

    error_check(session->open_cursor(session, uri, NULL, NULL, &cursor));
    /*
     * Only on even iterations add content to the extra table. This illustrates and shows that
     * sometimes only some tables will be updated.
     */
    cursor2 = NULL;
    //if (iter % 2 == 0)
    //    error_check(session->open_cursor(session, uri2, NULL, NULL, &cursor2));
    /*
     * Perform some operations with individual auto-commit transactions.
     */
    for (i = 0; i < 1; i++) {
        (void)snprintf(k, sizeof(k), "key.%d.%d.%d", iter, iterj, i);
        (void)snprintf(v, sizeof(v), "value.%d.%d.%d", iter, iterj, i);
        //printf("yang test ..........add work:%s\r\n", k);
        cursor->set_key(cursor, k);
        cursor->set_value(cursor, v);
        error_check(cursor->insert(cursor));
        if (cursor2 != NULL) {
            cursor2->set_key(cursor2, k);
            cursor2->set_value(cursor2, v);
            error_check(cursor2->insert(cursor2));
        }
    }
    error_check(cursor->close(cursor));
    if (cursor2 != NULL)
        error_check(cursor2->close(cursor2));
}


static int
finalize_files(FILELIST *flistp, size_t count)
{
    size_t i;

    /*
     * Process files that were removed. Any file that is not marked in the previous list as existing
     * in this iteration should be removed. Free all previous filenames as we go along. Then free
     * the overall list.
     */
    for (i = 0; i < filelist_count; ++i) {
        if (last_flist[i].name == NULL)
            break;
        if (!last_flist[i].exist) {
            testutil_system("rm WT_BLOCK_LOG_*/%s%s",
              strncmp(last_flist[i].name, WTLOG, WTLOGLEN) == 0 ? "logpath/" : "",
              last_flist[i].name);
        }
        free((void *)last_flist[i].name);
    }
    free(last_flist);

    /* Set up the current list as the new previous list. */
    last_flist = flistp;
    filelist_count = count;
    return (0);
}

/*
 * Process a file name. Build up a list of current file names. But also process the file names from
 * the previous iteration. Mark any name we see as existing so that the finalize function can remove
 * any that don't exist. We walk the list each time. This is slow.
 */
static int
process_file(FILELIST **flistp, size_t *countp, size_t *allocp, const char *filename)
{
    FILELIST *flist;
    size_t alloc, i, new, orig;

    /* Build up the current list, growing as needed. */
    i = *countp;
    alloc = *allocp;
    flist = *flistp;
    if (i == alloc) {
        orig = alloc * sizeof(FILELIST);
        new = orig * 2;
        flist = realloc(flist, new);
        testutil_assert(flist != NULL);
        memset(flist + alloc, 0, new - orig);
        *allocp = alloc * 2;
        *flistp = flist;
    }

    flist[i].name = strdup(filename);
    flist[i].exist = false;
    ++(*countp);

    /* Check against the previous list. */
    for (i = 0; i < filelist_count; ++i) {
        /* If name is NULL, we've reached the end of the list. */
        if (last_flist[i].name == NULL)
            break;
        if (strcmp(filename, last_flist[i].name) == 0) {
            last_flist[i].exist = true;
            break;
        }
    }
    return (0);
}

static void
take_full_backup(WT_SESSION *session, int i)
{
    FILELIST *flist;
    WT_CURSOR *cursor;
    size_t alloc, count;
    int j, ret;
    char buf[1024], f[256], h[256];
    const char *filename, *hdir;

   // return; //yang add change
    /*
     * First time through we take a full backup into the incremental directories. Otherwise only
     * into the appropriate full directory.
     */
    if (i != 0) {
        (void)snprintf(h, sizeof(h), "%s.%d", home_full, i);
        hdir = h;
    } else
        hdir = home_incr;
    if (i == 0) {
        (void)snprintf(
          buf, sizeof(buf), "incremental=(granularity=32k,enabled=true,this_id=\"ID%d\")", i);
        error_check(session->open_cursor(session, "backup:", NULL, buf, &cursor));
    } else
        error_check(session->open_cursor(session, "backup:", NULL, NULL, &cursor));

    count = 0;
    alloc = FLIST_INIT;
    flist = calloc(alloc, sizeof(FILELIST));
    testutil_assert(flist != NULL);
    while ((ret = cursor->next(cursor)) == 0) {
        error_check(cursor->get_key(cursor, &filename));
        error_check(process_file(&flist, &count, &alloc, filename));

        /*
         * If it is a log file, prepend the path for cp.
         */
        if (strncmp(filename, WTLOG, WTLOGLEN) == 0)
            (void)snprintf(f, sizeof(f), "%s/%s", logpath, filename);
        else
            (void)snprintf(f, sizeof(f), "%s", filename);

        if (i == 0)
            /*
             * Take a full backup into each incremental directory.
             */
            for (j = 0; j < MAX_ITERATIONS; j++) {
                (void)snprintf(h, sizeof(h), "%s.%d", home_incr, j);
                testutil_system("cp %s/%s %s/%s", home, f, h, f);
#if 0
                printf("FULL: Copy: %s\n", buf);
#endif
            }
        else {
#if 0
            (void)snprintf(h, sizeof(h), "%s.%d", home_full, i);
#endif
            testutil_system("cp %s/%s %s/%s", home, f, hdir, f);
#if 0
            printf("FULL %d: Copy: %s\n", i, buf);
#endif
        }
    }
    scan_end_check(ret == WT_NOTFOUND);
    error_check(cursor->close(cursor));
    error_check(finalize_files(flist, count));
}

static void
take_incr_backup(WT_SESSION *session, int i)
{
    FILELIST *flist;
    WT_CURSOR *backup_cur, *incr_cur;
    uint64_t offset, size, type;
    size_t alloc, count, rdsize, tmp_sz;
    int j, ret, rfd, wfd;
    char buf[1024], h[256], *tmp;
    const char *filename, *idstr;
    bool first;
    
   // return; //yang add change

    tmp = NULL;
    tmp_sz = 0;
    /*! [Query existing IDs] */
    error_check(session->open_cursor(session, "backup:query_id", NULL, NULL, &backup_cur));
    while ((ret = backup_cur->next(backup_cur)) == 0) {
        error_check(backup_cur->get_key(backup_cur, &idstr));
        printf("Existing incremental ID string: %s\n", idstr);
    }
    error_check(backup_cur->close(backup_cur));
    /*! [Query existing IDs] */

    /* Open the backup data source for incremental backup. */
    (void)snprintf(buf, sizeof(buf), "incremental=(src_id=\"ID%d\",this_id=\"ID%d\"%s)", i - 1, i,
      i % 2 == 0 ? "" : ",consolidate=true");
    error_check(session->open_cursor(session, "backup:", NULL, buf, &backup_cur));
    rfd = wfd = -1;
    count = 0;
    alloc = FLIST_INIT;
    flist = calloc(alloc, sizeof(FILELIST));
    testutil_assert(flist != NULL);
    /* For each file listed, open a duplicate backup cursor and copy the blocks. */
    while ((ret = backup_cur->next(backup_cur)) == 0) {
        error_check(backup_cur->get_key(backup_cur, &filename));
        error_check(process_file(&flist, &count, &alloc, filename));
        (void)snprintf(h, sizeof(h), "%s.0", home_incr);
        if (strncmp(filename, WTLOG, WTLOGLEN) == 0)
            testutil_system("cp %s/%s/%s %s/%s/%s", home, logpath, filename, h, logpath, filename);
        else
            testutil_system("cp %s/%s %s/%s", home, filename, h, filename);
#if 0
        printf("Copying backup: %s\n", buf);
#endif
        first = true;

        (void)snprintf(buf, sizeof(buf), "incremental=(file=%s)", filename);
        error_check(session->open_cursor(session, NULL, backup_cur, buf, &incr_cur));
#if 0
        printf("Taking incremental %d: File %s\n", i, filename);
#endif
        while ((ret = incr_cur->next(incr_cur)) == 0) {
            error_check(incr_cur->get_key(incr_cur, &offset, &size, &type));
            scan_end_check(type == WT_BACKUP_FILE || type == WT_BACKUP_RANGE);
#if 1
            printf("Incremental %s: KEY: Off %" PRIu64 " Size: %" PRIu64 " %s\n", filename, offset,
              size, type == WT_BACKUP_FILE ? "WT_BACKUP_FILE" : "WT_BACKUP_RANGE");
#endif
            if (type == WT_BACKUP_RANGE) {
                /*
                 * We should never get a range key after a whole file so the read file descriptor
                 * should be valid. If the read descriptor is valid, so is the write one.
                 */
                if (tmp_sz < size) {
                    tmp = realloc(tmp, size);
                    testutil_assert(tmp != NULL);
                    tmp_sz = size;
                }
                if (first) {
                    (void)snprintf(buf, sizeof(buf), "%s/%s", home, filename);
                    error_sys_check(rfd = open(buf, O_RDONLY, 0));
                    (void)snprintf(h, sizeof(h), "%s.%d", home_incr, i);
                    (void)snprintf(buf, sizeof(buf), "%s/%s", h, filename);
                    error_sys_check(wfd = open(buf, O_WRONLY | O_CREAT, 0));
                    first = false;
                }

                /*
                 * Don't use the system checker for lseek. The system check macro uses an int which
                 * is often 4 bytes and checks for any negative value. The offset returned from
                 * lseek is 8 bytes and we can have a false positive error check.
                 */
                if (lseek(rfd, (wt_off_t)offset, SEEK_SET) == -1)
                    testutil_die(errno, "lseek: read");
                error_sys_check(rdsize = (size_t)read(rfd, tmp, (size_t)size));
                if (lseek(wfd, (wt_off_t)offset, SEEK_SET) == -1)
                    testutil_die(errno, "lseek: write");
                /* Use the read size since we may have read less than the granularity. */
                error_sys_check(write(wfd, tmp, rdsize));
            } else {
                /* Whole file, so close both files and just copy the whole thing. */
                testutil_assert(first == true);
                rfd = wfd = -1;
                if (strncmp(filename, WTLOG, WTLOGLEN) == 0)
                    testutil_system(
                      "cp %s/%s/%s %s/%s/%s", home, logpath, filename, h, logpath, filename);
                else
                    testutil_system("cp %s/%s %s/%s", home, filename, h, filename);
#if 1
                printf("Incremental: Whole file copy: %s\n", buf);
#endif
            }
        }
        scan_end_check(ret == WT_NOTFOUND);
        /* Done processing this file. Close incremental cursor. */
        error_check(incr_cur->close(incr_cur));

        /* Close file descriptors if they're open. */
        if (rfd != -1) {
            error_check(close(rfd));
            error_check(close(wfd));
        }
        /*
         * For each file, we want to copy the file into each of the later incremental directories so
         * that they start out at the same for the next incremental round. We then check each
         * incremental directory along the way.
         */
        for (j = i; j < MAX_ITERATIONS; j++) {
            (void)snprintf(h, sizeof(h), "%s.%d", home_incr, j);
            if (strncmp(filename, WTLOG, WTLOGLEN) == 0)
                testutil_system(
                  "cp %s/%s/%s %s/%s/%s", home, logpath, filename, h, logpath, filename);
            else
                testutil_system("cp %s/%s %s/%s", home, filename, h, filename);
        }
    }
    scan_end_check(ret == WT_NOTFOUND);

    /* Done processing all files. Close backup cursor. */
    error_check(backup_cur->close(backup_cur));
    error_check(finalize_files(flist, count));
    free(tmp);
}

int
main(int argc, char *argv[])
{
    struct stat sb;
    WT_CONNECTION *wt_conn;
    WT_CURSOR *backup_cur;
    WT_SESSION *session;
    int i, j, ret;
    char cmd_buf[256], *idstr;

    (void)argc; /* Unused variable */
    (void)testutil_set_progname(argv);

    testutil_system("rm -rf %s && mkdir -p %s/%s", home, home, logpath);
    error_check(wiredtiger_open(home, NULL, CONN_CONFIG, &wt_conn));

    setup_directories();
    error_check(wt_conn->open_session(wt_conn, NULL, NULL, &session));
    error_check(session->create(session, uri, "key_format=S,value_format=S"));
    error_check(session->create(session, uri2, "key_format=S,value_format=S"));
    printf("Adding initial data\n");
    add_work(session, 0, 0);
    
    error_check(session->checkpoint(session, NULL));
    
    printf("Taking initial backup\n");
    take_full_backup(session, 0);

    //error_check(session->checkpoint(session, NULL));

    for (i = 1; i < MAX_ITERATIONS; i++) {
        printf("\r\n\r\n\r\n\r\nIteration %d: adding data\n", i);
        /* For each iteration we may add work and checkpoint multiple times. */
        //for (j = 0; j < i; j++) {
            //add_work(session, i, j); //yang add change
            (void)(j);
            add_inc_work(session, i, i);
            error_check(session->checkpoint(session, NULL));
        //}

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
        error_check(compare_backups(i));
    }
    exit(0); //yang add change

    printf("Close and reopen the connection\n");
    /*
     * Close and reopen the connection to illustrate the durability of id information.
     */
    error_check(wt_conn->close(wt_conn, NULL));
    error_check(wiredtiger_open(home, NULL, CONN_CONFIG, &wt_conn));
    error_check(wt_conn->open_session(wt_conn, NULL, NULL, &session));

    printf("Verify query after reopen\n");
    error_check(session->open_cursor(session, "backup:query_id", NULL, NULL, &backup_cur));
    while ((ret = backup_cur->next(backup_cur)) == 0) {
        error_check(backup_cur->get_key(backup_cur, &idstr));
        printf("Existing incremental ID string: %s\n", idstr);
    }
    error_check(backup_cur->close(backup_cur));
    
    exit(0); //yang add change
    /*
     * We should have an entry for i-1 and i-2. Use the older one.
     */
    (void)snprintf(
      cmd_buf, sizeof(cmd_buf), "incremental=(src_id=\"ID%d\",this_id=\"ID%d\")", i - 2, i);
    error_check(session->open_cursor(session, "backup:", NULL, cmd_buf, &backup_cur));
    error_check(backup_cur->close(backup_cur));

    /*
     * After we're done, release resources. Test the force stop setting.
     */
    (void)snprintf(cmd_buf, sizeof(cmd_buf), "incremental=(force_stop=true)");
    error_check(session->open_cursor(session, "backup:", NULL, cmd_buf, &backup_cur));
    error_check(backup_cur->close(backup_cur));

    /*
     * Close the connection. We're done and want to run the final comparison between the incremental
     * and original.
     */
    error_check(wt_conn->close(wt_conn, NULL));

    printf("Final comparison: dumping and comparing data\n");
    error_check(compare_backups(0));
    for (i = 0; i < (int)filelist_count; ++i) {
        if (last_flist[i].name == NULL)
            break;
        free((void *)last_flist[i].name);
    }
    free(last_flist);

    /*
     * Reopen the connection to verify that the forced stop should remove incremental information.
     */
    error_check(wiredtiger_open(home, NULL, CONN_CONFIG, &wt_conn));
    error_check(wt_conn->open_session(wt_conn, NULL, NULL, &session));
    /*
     * We should not have any information.
     */
    (void)snprintf(
      cmd_buf, sizeof(cmd_buf), "incremental=(src_id=\"ID%d\",this_id=\"ID%d\")", i - 2, i);
    testutil_assert(session->open_cursor(session, "backup:", NULL, cmd_buf, &backup_cur) == ENOENT);
    error_check(wt_conn->close(wt_conn, NULL));

    (void)snprintf(cmd_buf, sizeof(cmd_buf), "%s/WiredTiger.backup.block", home);
    ret = stat(cmd_buf, &sb);
    testutil_assert(ret == -1 && errno == ENOENT);

    return (EXIT_SUCCESS);
}

