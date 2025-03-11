/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

/*  session->compact() 日志如下:
[1739870255:744240][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_1]: ============ testing for compaction
[1739870255:744258][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_1]: file size 572MB (599867392) with 99% space available 572MB (599834624)
[1739870255:760811][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_2]:  0%:           57MB, (free: 59962368B, 9%), (used: 0MB, 24371B, 74%)
[1739870255:760827][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_2]: 10%:           57MB, (free: 59986944B, 10%), (used: 0MB, 0B, 0%)
[1739870255:760830][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_2]: 20%:           57MB, (free: 59986432B, 10%), (used: 0MB, 307B, 0%)
[1739870255:760833][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_2]: 30%:           57MB, (free: 59986944B, 10%), (used: 0MB, 0B, 0%)
[1739870255:760836][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_2]: 40%:           57MB, (free: 59986432B, 10%), (used: 0MB, 307B, 0%)
[1739870255:760839][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_2]: 50%:           57MB, (free: 59986944B, 10%), (used: 0MB, 0B, 0%)
[1739870255:760841][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_2]: 60%:           57MB, (free: 59986944B, 10%), (used: 0MB, 0B, 0%)
[1739870255:760844][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_2]: 70%:           57MB, (free: 59986432B, 10%), (used: 0MB, 307B, 0%)
[1739870255:760847][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_2]: 80%:           57MB, (free: 59986944B, 10%), (used: 0MB, 0B, 0%)
[1739870255:760849][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_2]: 90%:           57MB, (free: 59978240B, 9%), (used: 0MB, 8499B, 25%)
[1739870255:760852][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_1]: access.wt: total reviewed 0 pages, total rewritten 0 pages
[1739870255:760855][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_1]: access.wt: 570MB (598634496) available space in the first 80% of the file
[1739870255:760858][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_1]: access.wt: 570MB (598634496) available space in the first 90% of the file
[1739870255:760861][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_1]: access.wt: require 10% or 57MB (59986739) in the first 90% of the file to perform compaction, compaction proceeding
[1739870259:861695][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][INFO]: access.wt: skipping because the file size must be greater than 1MB: 794624B.
[1739870259:861714][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_1]: ============ ending compaction pass
[1739870259:861717][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_1]: pages reviewed: 3
[1739870259:861720][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_1]: pages skipped: 1
[1739870259:861722][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_1]: pages rewritten : 2
[1739870259:861724][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_1]: file size 0MB (794624) with 95% space available 0MB (761856)
[1739870259:861748][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_2]:  0%:            0MB, (free: 55296B, 7%), (used: 0MB, 24166B, 73%)
[1739870259:861751][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_2]: 10%:            0MB, (free: 79360B, 10%), (used: 0MB, 102B, 0%)
[1739870259:861754][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_2]: 20%:            0MB, (free: 79360B, 10%), (used: 0MB, 102B, 0%)
[1739870259:861757][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_2]: 30%:            0MB, (free: 79360B, 10%), (used: 0MB, 102B, 0%)
[1739870259:861759][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_2]: 40%:            0MB, (free: 79360B, 10%), (used: 0MB, 102B, 0%)
[1739870259:861762][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_2]: 50%:            0MB, (free: 79872B, 10%), (used: 0MB, 0B, 0%)
[1739870259:861765][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_2]: 60%:            0MB, (free: 79360B, 10%), (used: 0MB, 102B, 0%)
[1739870259:861767][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_2]: 70%:            0MB, (free: 79360B, 10%), (used: 0MB, 102B, 0%)
[1739870259:861770][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_2]: 80%:            0MB, (free: 79360B, 10%), (used: 0MB, 102B, 0%)
[1739870259:861773][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_2]: 90%:            0MB, (free: 71168B, 9%), (used: 0MB, 8294B, 25%)
*/
#include "wt_internal.h"

static void __block_dump_file_stat(WT_SESSION_IMPL *, WT_BLOCK *, bool);

/*
 * __wt_block_compact_start --
 *     Start compaction of a file.
   __wt_session_compact->__compact_handle_append->__compact_start->__bm_compact_start
 */
int
__wt_block_compact_start(WT_SESSION_IMPL *session, WT_BLOCK *block)
{
    WT_UNUSED(session);

    /* Switch to first-fit allocation. */
    __wt_block_configure_first_fit(block, true);

    /* Reset the compaction state information. */
    block->compact_pct_tenths = 0;
    block->compact_pages_rewritten = 0;
    block->compact_pages_reviewed = 0;
    block->compact_pages_skipped = 0;

    return (0);
}

/*
 * __wt_block_compact_end --
 *     End compaction of a file.
 */
int
__wt_block_compact_end(WT_SESSION_IMPL *session, WT_BLOCK *block)
{
    /* Restore the original allocation plan. */
    __wt_block_configure_first_fit(block, false);

    /* Dump the results of the compaction pass. */
    if (WT_VERBOSE_LEVEL_ISSET(session, WT_VERB_COMPACT, WT_VERBOSE_DEBUG_1)) {
        __wt_spin_lock(session, &block->live_lock);
        __block_dump_file_stat(session, block, false);
        __wt_spin_unlock(session, &block->live_lock);
    }
    return (0);
}

/*
 * __wt_block_compact_get_progress_stats --
 *     Collect compact progress stats.
 */
void
__wt_block_compact_get_progress_stats(WT_SESSION_IMPL *session, WT_BM *bm,
  uint64_t *pages_reviewedp, uint64_t *pages_skippedp, uint64_t *pages_rewrittenp)
{
    WT_BLOCK *block;

    WT_UNUSED(session);
    block = bm->block;
    *pages_reviewedp = block->compact_pages_reviewed;
    *pages_skippedp = block->compact_pages_skipped;
    *pages_rewrittenp = block->compact_pages_rewritten;
}

/*
 * __wt_block_compact_progress --
 *     Output compact progress message.
 20秒打印一次进度
 */
void
__wt_block_compact_progress(WT_SESSION_IMPL *session, WT_BLOCK *block, u_int *msg_countp)
{
    struct timespec cur_time;
    uint64_t time_diff;

    if (!WT_VERBOSE_LEVEL_ISSET(session, WT_VERB_COMPACT_PROGRESS, WT_VERBOSE_DEBUG_2))
        return;

    __wt_epoch(session, &cur_time);

    /* Log one progress message every twenty seconds. */
    time_diff = WT_TIMEDIFF_SEC(cur_time, session->compact->begin);
    if (time_diff / WT_PROGRESS_MSG_PERIOD > *msg_countp) {
        ++*msg_countp;
        __wt_verbose_debug1(session, WT_VERB_COMPACT_PROGRESS,
          " compacting %s for %" PRIu64 " seconds; reviewed %" PRIu64 " pages, rewritten %" PRIu64
          " pages",
          block->name, time_diff, block->compact_pages_reviewed, block->compact_pages_rewritten);
    }
}
/*
 * __wt_block_compact_skip --
 *     Return if compaction will shrink the file.
 //确认当前表的空洞总量占比，如果小于1M则直接返回
 //磁盘碎片空间占比小于10%或者小于1M直接跳过
 */
int
__wt_block_compact_skip(WT_SESSION_IMPL *session, WT_BLOCK *block, bool *skipp)
{
    WT_EXT *ext;
    WT_EXTLIST *el;
    wt_off_t avail_eighty, avail_ninety, eighty, ninety;

    //磁盘碎片空间占比小于10%或者小于1M直接跳过
    *skipp = true; /* Return a default skip. */

    /*
     * We do compaction by copying blocks from the end of the file to the beginning of the file, and
     * we need some metrics to decide if it's worth doing. Ignore small files, and files where we
     * are unlikely to recover 10% of the file.
     */
    if (block->size <= WT_MEGABYTE) {
        __wt_verbose_info(session, WT_VERB_COMPACT,
          "%s: skipping because the file size must be greater than 1MB: %" PRIuMAX "B.",
          block->name, (uintmax_t)block->size);

        return (0);
    }

    __wt_spin_lock(session, &block->live_lock);

    /* Dump the current state of the file. */
    if (WT_VERBOSE_LEVEL_ISSET(session, WT_VERB_COMPACT, WT_VERBOSE_DEBUG_2))
        __block_dump_file_stat(session, block, true);

    /* Sum the available bytes in the initial 80% and 90% of the file. */
    //代表文件的前面80%或者90%长度内有多少碎片空间
    avail_eighty = avail_ninety = 0;
    //90%文件大小
    ninety = block->size - block->size / 10;
    //80%文件大小
    eighty = block->size - ((block->size / 10) * 2);

    el = &block->live.avail;
    WT_EXT_FOREACH (ext, el->off)
        //也就是查找表文件的前面90%长度
        if (ext->off < ninety) {
            //代表文件的前面90%长度内有多少碎片空间
            avail_ninety += ext->size;
            //也就是查找表文件的前面80%长度
            if (ext->off < eighty)
                //代表文件的前面80%长度内有多少碎片空间
                avail_eighty += ext->size;
        }

    /*
     * Skip files where we can't recover at least 1MB.
     *
     * If at least 20% of the total file is available and in the first 80% of the file, we'll try
     * compaction on the last 20% of the file; else, if at least 10% of the total file is available
     * and in the first 90% of the file, we'll try compaction on the last 10% of the file.
     *
     * We could push this further, but there's diminishing returns, a mostly empty file can be
     * processed quickly, so more aggressive compaction is less useful.
     */
    //__wt_block_compact_skip这里限制了这里for遍历一轮所有internal page后最多只会搬迁处于wt文件尾部10%或者20%(compact_pct_tenths)的page，
    // 配合__compact_page_skip阅读
    if (avail_eighty > WT_MEGABYTE && avail_eighty >= ((block->size / 10) * 2)) {
        // 文件前面80%长度内有大于20%的空洞,我们选择表文件最后的20%中的leaf page来做搬迁
        *skipp = false;
        block->compact_pct_tenths = 2;
    } else if (avail_ninety > WT_MEGABYTE && avail_ninety >= block->size / 10) {
        // 文件前面90%长度内有10%-20的空洞，我们选择表文件最后10%中的leaf page来做搬迁填充
        *skipp = false;
        block->compact_pct_tenths = 1;
    } //yang add todo xxxxxxxxxxxxxxxx  这里最好compact_pct_tenths支持可配置，当前是默认空洞超过10%才回收，一些大表几T 10%空间也是挺大的

    __wt_verbose_debug1(session, WT_VERB_COMPACT,
      "%s: total reviewed %" PRIu64 " pages, total rewritten %" PRIu64 " pages", block->name,
      block->compact_pages_reviewed, block->compact_pages_rewritten);
    __wt_verbose_debug1(session, WT_VERB_COMPACT,
      "%s: %" PRIuMAX "MB (%" PRIuMAX ") available space in the first 80%% of the file",
      block->name, (uintmax_t)avail_eighty / WT_MEGABYTE, (uintmax_t)avail_eighty);
    __wt_verbose_debug1(session, WT_VERB_COMPACT,
      "%s: %" PRIuMAX "MB (%" PRIuMAX ") available space in the first 90%% of the file",
      block->name, (uintmax_t)avail_ninety / WT_MEGABYTE, (uintmax_t)avail_ninety);
    __wt_verbose_debug1(session, WT_VERB_COMPACT,
      "%s: require 10%% or %" PRIuMAX "MB (%" PRIuMAX
      ") in the first 90%% of the file to perform compaction, compaction %s",
      block->name, (uintmax_t)(block->size / 10) / WT_MEGABYTE, (uintmax_t)block->size / 10,
      *skipp ? "skipped" : "proceeding");

    __wt_spin_unlock(session, &block->live_lock);

    return (0);
}

/*
 * __compact_page_skip --
 *     Return if writing a particular page will shrink the file.
 判断[offset, size]这个page是否处于分割点的后半段，并且前半段avil空洞ext是否可以容纳这个page
 确定该page是否可以搬到前半段的空洞碎片中，只会搬迁文件的后半段compact_pct_tenths(10%或者20%的page迁移到)

 //__wt_block_compact_skip和__compact_page_skip限制了这里for遍历一轮所有internal page后最多只会搬迁处于wt文件尾部10%或者20%(compact_pct_tenths)的page，
 */
static void
__compact_page_skip(
  WT_SESSION_IMPL *session, WT_BLOCK *block, wt_off_t offset, uint32_t size, bool *skipp)
{
    WT_EXT *ext;
    WT_EXTLIST *el;
    wt_off_t limit;

    *skipp = true; /* Return a default skip. */

    /*
     * If this block is in the chosen percentage of the file and there's a block on the available
     * list that appears before that percentage of the file, rewrite the block. Checking the
     * available list is necessary (otherwise writing the block would extend the file), but there's
     * an obvious race if the file is sufficiently busy.
     */
    __wt_spin_lock(session, &block->live_lock);
    //也就是分割点，分割点前是需要填充的碎片空洞
    limit = block->size - ((block->size / 10) * block->compact_pct_tenths);
    //说明该page处于wt文件分割点后部，可以搬迁填充空洞
    if (offset > limit) {
        el = &block->live.avail;
        WT_EXT_FOREACH (ext, el->off) {
            //说明空洞在文件compact_pct_tenths分割的后半段中，则停止查找遍历，因为我们需要把后半段的page填充到前半段的空洞中
            if (ext->off >= limit)
                break;
            //说明这个空洞处于前半段，并且可以容纳后半段这个需要搬迁的长度为size的page
            if (ext->size >= size) {
                *skipp = false;
                break;
            }
        }
    }
    __wt_spin_unlock(session, &block->live_lock);

    //page中可以填充文件前半段的空洞page总数
    ++block->compact_pages_reviewed;
    if (*skipp)
        //page中不能填充文件前半段的空洞page总数
        ++block->compact_pages_skipped;
    else
        //page中可以填充文件前半段的空洞page总数
        ++block->compact_pages_rewritten;
}

/*
 * __wt_block_compact_page_skip --
 *     Return if writing a particular page will shrink the file.
 */
int
__wt_block_compact_page_skip(
  WT_SESSION_IMPL *session, WT_BLOCK *block, const uint8_t *addr, size_t addr_size, bool *skipp)
{
    wt_off_t offset;
    uint32_t size, checksum, objectid;

    WT_UNUSED(addr_size);
    *skipp = true; /* Return a default skip. */
    offset = 0;

    /* Crack the cookie. */
    WT_RET(__wt_block_addr_unpack(
      session, block, addr, addr_size, &objectid, &offset, &size, &checksum));

    __compact_page_skip(session, block, offset, size, skipp);

    return (0);
}

/*
 * __wt_block_compact_page_rewrite --
 *     Rewrite a page if it will shrink the file.
 判断addr这个page是否处于文件的后半段，如果处于文件的后半段，则继续判断文件前半段是否有可用的avil ext空洞，如果有则把这个page迁移到前半段
 */
int
__wt_block_compact_page_rewrite(
  WT_SESSION_IMPL *session, WT_BLOCK *block, 
  //addr中存储的是一个page的磁盘元数据信息(objectid offset size  checksum)
  uint8_t *addr, 
  size_t *addr_sizep, bool *skipp)
{
    WT_DECL_ITEM(tmp);
    WT_DECL_RET;
    wt_off_t offset, new_offset;
    uint32_t size, checksum, objectid;
    uint8_t *endp;
    bool discard_block;

    *skipp = true;  /* Return a default skip. */
    new_offset = 0; /* -Werror=maybe-uninitialized */

    discard_block = false;

    //根据addr磁盘元数据解析出对应的page地址 offset objectid等
    WT_ERR(__wt_block_addr_unpack(
      session, block, addr, *addr_sizep, &objectid, &offset, &size, &checksum));

    /* Check if the block is worth rewriting. */
    //判断[offset, size]这个page是否处于分割点的后半段，并且前半段avil空洞ext是否可以容纳这个page
    __compact_page_skip(session, block, offset, size, skipp);

    //说明这个page处于前半段，或者这个page处于后半段但是前半段没有可容纳它的空洞 
    //确定该page是否可以从后半段搬迁到前半段的空洞中

    //说明这个page不可以搬迁，直接返回
    if (*skipp)
        return (0);

    /* Read the block. */
    //读出要搬迁的page的数据到tmp中
    WT_ERR(__wt_scr_alloc(session, size, &tmp));
    WT_ERR(__wt_read(session, block->fh, offset, size, tmp->mem));

    /* Allocate a replacement block. */
    //__wt_block_alloc从avail空洞跳表中获取一个size长度的ext
    WT_ERR(__wt_block_ext_prealloc(session, 5));
    __wt_spin_lock(session, &block->live_lock);
    ret = __wt_block_alloc(session, block, &new_offset, (wt_off_t)size);
    __wt_spin_unlock(session, &block->live_lock);
    WT_ERR(ret);
    discard_block = true;

    /* Write the block. */
    //把要迁移的page数据写到前半段的空洞ext
    WT_ERR(__wt_write(session, block->fh, new_offset, size, tmp->mem));

    /* Free the original block. */
    __wt_spin_lock(session, &block->live_lock);
    //释放后半段搬迁完成的这个page对应的ext元数据
    ret = __wt_block_off_free(session, block, objectid, offset, (wt_off_t)size);
    __wt_spin_unlock(session, &block->live_lock);
    WT_ERR(ret);

    /* Build the returned address cookie. */
    //后半段的page已经搬迁到了文件前半段，重新生成前半段新page的addr[WT_BTREE_MAX_ADDR_COOKIE]元数据
    endp = addr;
    WT_ERR(__wt_block_addr_pack(block, &endp, objectid, new_offset, size, checksum));
    *addr_sizep = WT_PTRDIFF(endp, addr);

    WT_STAT_CONN_INCR(session, block_write);
    WT_STAT_CONN_INCRV(session, block_byte_write, size);

    discard_block = false;

err:
    if (discard_block) {
        __wt_spin_lock(session, &block->live_lock);
        WT_TRET(__wt_block_off_free(session, block, objectid, new_offset, (wt_off_t)size));
        __wt_spin_unlock(session, &block->live_lock);
    }
    __wt_scr_free(session, &tmp);
    return (ret);
}

/*
 * __block_dump_bucket_stat --
 *     Dump out the information about available and used blocks in the given bucket (part of the
 *     file).
 */
static void
__block_dump_bucket_stat(WT_SESSION_IMPL *session, 
    //file_size: .wt文件总大小     //file_free: 空洞总字节数
    uintmax_t file_size, uintmax_t file_free,
    //整个wt文件分成10段，每一段的长度
    uintmax_t bucket_size, 
    //在i这个十分位范围内有多少字节碎片空间
    uintmax_t bucket_free, 
    //bucket_pct代表十分位段，i *10就代表是0   10  20  30 ..... 90
    u_int bucket_pct)
{
    uintmax_t bucket_used, free_pct, used_pct;

    free_pct = used_pct = 0;

    /* Handle rounding error in which case bucket used size can be negative. */
    //表示该十分位段内非碎片空间大小
    bucket_used = (bucket_size > bucket_free) ? (bucket_size - bucket_free) : 0;

    if (file_free != 0)
        //该十分位段内的碎片空间相对总的碎片空间的比例
        free_pct = (bucket_free * 100) / file_free;

    if (file_size > file_free)
        used_pct = (bucket_used * 100) / (file_size - file_free);

//(free: 59986944B, 10%)表示该十分位段内碎片空间大小，以及该段内碎片空间相比总碎片空间的占比      
//(used: 0MB, 0B, 0%)表示该十分位段内非碎片空间大小，以及该段内非碎片空间相比总非碎片空间的占比   

//    [WT_VERB_COMPACT][DEBUG_2]: 80%:           57MB, (free: 59986944B, 10%), (used: 0MB, 0B, 0%)
//    [WT_VERB_COMPACT][DEBUG_2]: 90%:           57MB, (free: 59978240B, 9%), (used: 0MB, 8499B, 25%)
    __wt_verbose_debug2(session, WT_VERB_COMPACT,
      "%2u%%: %12" PRIuMAX "MB, (free: %" PRIuMAX "B, %" PRIuMAX "%%), (used: %" PRIuMAX
      "MB, %" PRIuMAX "B, %" PRIuMAX "%%)",
      bucket_pct, bucket_free / WT_MEGABYTE, bucket_free, free_pct, bucket_used / WT_MEGABYTE,
      bucket_used, used_pct);
}

/*
 * __block_dump_file_stat --
 *     Dump out the avail/used list so we can see what compaction will look like.
 */
static void
__block_dump_file_stat(WT_SESSION_IMPL *session, WT_BLOCK *block, bool start)
{
    WT_EXT *ext;
    WT_EXTLIST *el;
    wt_off_t decile[10], percentile[100], size;
    uintmax_t bucket_size;
    u_int i;

    el = &block->live.avail;
    size = block->size;

    __wt_verbose_debug1(session, WT_VERB_COMPACT, "============ %s",
      start ? "testing for compaction" : "ending compaction pass");

    if (!start) {
        __wt_verbose_debug1(
          session, WT_VERB_COMPACT, "pages reviewed: %" PRIu64, block->compact_pages_reviewed);
        __wt_verbose_debug1(
          session, WT_VERB_COMPACT, "pages skipped: %" PRIu64, block->compact_pages_skipped);
        __wt_verbose_debug1(
          session, WT_VERB_COMPACT, "pages rewritten : %" PRIu64, block->compact_pages_rewritten);
    }

    //file size 87MB (91987968) with 99% space available 87MB (91516928)
    __wt_verbose_debug1(session, WT_VERB_COMPACT,
      "file size %" PRIuMAX "MB (%" PRIuMAX ") with %" PRIuMAX "%% space available %" PRIuMAX
      "MB (%" PRIuMAX ")",
      (uintmax_t)size / WT_MEGABYTE, (uintmax_t)size,
      ((uintmax_t)el->bytes * 100) / (uintmax_t)size, (uintmax_t)el->bytes / WT_MEGABYTE,
      (uintmax_t)el->bytes);

    //也就是空洞数没有了
    if (el->entries == 0)
        return;

    /*
     * Bucket the available memory into file deciles/percentiles. Large pieces of memory will cross
     * over multiple buckets, assign to the decile/percentile in 512B chunks.
     */
    memset(decile, 0, sizeof(decile));
    memset(percentile, 0, sizeof(percentile));
    WT_EXT_FOREACH (ext, el->off)
        //((ext->off + (wt_off_t)i * 512) * 10) / size = ((ext->off + (wt_off_t)i * 512) / size) * 10 
        //例如也就是把1个文件size拆分为10段，ext->size按照512字节细分，计算这个ext以512字节为单位处于size中10段拆分的那一段
        //  wt file size: 1-----------------------------12800-----------------------------25600-----------------------------------128000
        //                     |     |      |     |
        //ext(512-2046) :     512---1024---1536---2046  这个ext后decile[0]=4      
        //
        for (i = 0; i < ext->size / 512; ++i) {
            ++decile[((ext->off + (wt_off_t)i * 512) * 10) / size];
            ++percentile[((ext->off + (wt_off_t)i * 512) * 100) / size];
        }

#ifdef __VERBOSE_OUTPUT_PERCENTILE
    /*
     * The verbose output always displays 10% buckets, running this code as well also displays 1%
     * buckets. There will be rounding error in the `used` stats because of the bucket size
     * calculation. Adding 50 to minimize the rounding error.
     */
    bucket_size = (uintmax_t)((size + 50) / 100);
    for (i = 0; i < WT_ELEMENTS(percentile); ++i)
        __block_dump_bucket_stat(session, (uintmax_t)size, (uintmax_t)el->bytes, bucket_size,
          (uintmax_t)percentile[i] * 512, i);
#endif

    /*
     * There will be rounding error in the `used` stats because of the bucket size calculation.
     * Adding 5 to minimize the rounding error.
     
     Block_compact.c (src\block):[1739870255:760811][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_2]:  0%:           57MB, (free: 59962368B, 9%), (used: 0MB, 24371B, 74%)
     Block_compact.c (src\block):[1739870255:760827][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_2]: 10%:           57MB, (free: 59986944B, 10%), (used: 0MB, 0B, 0%)
     Block_compact.c (src\block):[1739870255:760830][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_2]: 20%:           57MB, (free: 59986432B, 10%), (used: 0MB, 307B, 0%)
     Block_compact.c (src\block):[1739870255:760833][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_2]: 30%:           57MB, (free: 59986944B, 10%), (used: 0MB, 0B, 0%)
     Block_compact.c (src\block):[1739870255:760836][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_2]: 40%:           57MB, (free: 59986432B, 10%), (used: 0MB, 307B, 0%)
     Block_compact.c (src\block):[1739870255:760839][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_2]: 50%:           57MB, (free: 59986944B, 10%), (used: 0MB, 0B, 0%)
     Block_compact.c (src\block):[1739870255:760841][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_2]: 60%:           57MB, (free: 59986944B, 10%), (used: 0MB, 0B, 0%)
     Block_compact.c (src\block):[1739870255:760844][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_2]: 70%:           57MB, (free: 59986432B, 10%), (used: 0MB, 307B, 0%)
     Block_compact.c (src\block):[1739870255:760847][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_2]: 80%:           57MB, (free: 59986944B, 10%), (used: 0MB, 0B, 0%)
     Block_compact.c (src\block):[1739870255:760849][95569:0x7f1f7f0d1800], file:access.wt, WT_SESSION.__wt_session_compact: [WT_VERB_COMPACT][DEBUG_2]: 90%:           57MB, (free: 59978240B, 9%), (used: 0MB, 8499B, 25%)
     */
    //wt文件file拆分为10段，每一段的大小
    bucket_size = (uintmax_t)((size + 5) / 10);
    for (i = 0; i < WT_ELEMENTS(decile); ++i)
        __block_dump_bucket_stat(session, (uintmax_t)size, (uintmax_t)el->bytes, bucket_size,
          //每个十分位段上面有多少个512字节, 乘以512也就是在i这个十分位范围内有多少字节碎片空间
          (uintmax_t)decile[i] * 512, 
          //i代表十分位段，i *10就代表是0   10  20  30 ..... 90
          i * 10);
}
