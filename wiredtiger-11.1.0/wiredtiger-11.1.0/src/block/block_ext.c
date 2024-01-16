/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

/*
 * WT_BLOCK_RET --
 *	Handle extension list errors that would normally panic the system but
 * which should fail gracefully when verifying.
 */
#define WT_BLOCK_RET(session, block, v, ...)                                        \
    do {                                                                            \
        int __ret = (v);                                                            \
        __wt_err(session, __ret, __VA_ARGS__);                                      \
        return ((block)->verify ?                                                   \
            __ret :                                                                 \
            __wt_panic(session, WT_PANIC, "block manager extension list failure")); \
    } while (0)

static int __block_append(WT_SESSION_IMPL *, WT_BLOCK *, WT_EXTLIST *, wt_off_t, wt_off_t);
static int __block_ext_overlap(
  WT_SESSION_IMPL *, WT_BLOCK *, WT_EXTLIST *, WT_EXT **, WT_EXTLIST *, WT_EXT **);
static int __block_extlist_dump(WT_SESSION_IMPL *, WT_BLOCK *, WT_EXTLIST *, const char *);
static int __block_merge(WT_SESSION_IMPL *, WT_BLOCK *, WT_EXTLIST *, wt_off_t, wt_off_t);

/*
 * __block_off_srch_last --
 *     Return the last element in the list, along with a stack for appending.
 */
//获取head对应跳表的最后一个WT_EXT成员
static inline WT_EXT *
__block_off_srch_last(WT_EXTLIST *el, WT_EXT ***stack, bool need_traverse)
{
    WT_EXT **extp, *last;
    WT_EXT **head;
    int i;

    if (need_traverse == false)
        return el->last;

    last = NULL; /* The list may be empty */
    head = el->off;

    /*
     * Start at the highest skip level, then go as far as possible at each level before stepping
     * down to the next.
     */
    for (i = WT_SKIP_MAXDEPTH - 1, extp = &head[i]; i >= 0;)
        if (*extp != NULL) {
            last = *extp;
            extp = &(*extp)->next[i];
        } else
            stack[i--] = extp--;

    return (last);
}

static inline void
print_extent_list(const char *name, const char* filename, WT_EXTLIST *el)
{
    WT_EXT *extp, **astack[WT_SKIP_MAXDEPTH];

    return;
    if (el->off[0] == NULL || strcmp(el->name, "live.avail") != 0)
        return;

    extp = el->off[0];
    printf("yang test ....%s:%s..%s.ext list:", name, el->name, filename);
    while (extp != NULL) {
        printf("[%d, %d] -> ", (int)extp->off, (int)extp->size);
        extp = extp->next[0];
    }
    if (el->last)
        printf(" last:[%d:%d]\r\n", (int)el->last->off, (int)el->last->size);
    else
        printf(" last: NULL\r\n");

    extp = __block_off_srch_last(el, astack, true);
    if (extp != NULL && el->last!= NULL && extp->off != el->last->off)
        printf("yang test xxxxxxxxxxxxxxxxxxxxxxx error: %d, %d\r\n", (int)extp->off, (int)el->last->off);
}

static inline void
print_extent_list_not_printlast(const char *name, const char* filename, WT_EXTLIST *el)
{
    WT_EXT *extp;//, **astack[WT_SKIP_MAXDEPTH];

    return;;
    if (el->off[0] == NULL || strcmp(el->name, "live.avail") != 0)
        return;

    extp = el->off[0];
    printf("yang test ....%s:%s....%s.ext list:", name, el->name, filename);
    while (extp != NULL) {
        printf("[%d, %d] -> ", (int)extp->off, (int)extp->size);
        extp = extp->next[0];
    }
    if (el->last)
        printf(" last:[%d:%d]\r\n", (int)el->last->off, (int)el->last->size);
    else
        printf(" last: NULL\r\n");
}

/*
 * __block_off_srch --
 *     Search a by-offset skiplist (either the primary by-offset list, or the by-offset list
 *     referenced by a size entry), for the specified offset.

  __block_off_srch根据off搜索
 __block_off_remove根据off删除
 __block_off_insert根据off插入跳表
 */
//skip_off为true代表从__wt_extlist->sz下面的off跳跃表查找，head代表__wt_extlist->sz.off
//skip_off为false代表从__wt_extlist->off跳跃表查找，head代表__wt_extlist->off
static inline void
__block_off_srch(WT_EXT **head, wt_off_t off, WT_EXT ***stack, bool skip_off, WT_EXT **penultimate_ext)
{
    WT_EXT **extp, *ext_tmp;
    int i;

    /*
     * Start at the highest skip level, then go as far as possible at each level before stepping
     * down to the next.
     *
     * Return a stack for an exact match or the next-largest item.
     *
     * The WT_EXT structure contains two skiplists, the primary one and the per-size bucket one: if
     * the skip_off flag is set, offset the skiplist array by the depth specified in this particular
     * structure.
     */
    ext_tmp = NULL;
    for (i = WT_SKIP_MAXDEPTH - 1, extp = &head[i]; i >= 0;) {
        if (*extp != NULL && (*extp)->off < off) {
            ext_tmp = *extp;
           // printf("yang test ....__block_off_srch.......... xxxxxxxxxxxxxxxxxx [%d, %d] \r\n",
            //    (int)ext_tmp->off, (int)ext_tmp->size);
            extp = &(*extp)->next[i + (skip_off ? (*extp)->depth : 0)];
        } else {
            stack[i--] = extp--;
        }
    }

    if (penultimate_ext != NULL)
        *penultimate_ext = ext_tmp;
}

/*
 * __block_first_srch --
 *     Search the skiplist for the first available slot.
 */
static inline bool
__block_first_srch(WT_EXT **head, wt_off_t size, WT_EXT ***stack)
{
    WT_EXT *ext;

    /*
     * Linear walk of the available chunks in offset order; take the first one that's large enough.
     */
    WT_EXT_FOREACH (ext, head)
        if (ext->size >= size)
            break;
    if (ext == NULL)
        return (false);

    /* Build a stack for the offset we want. */
    __block_off_srch(head, ext->off, stack, false, NULL);
    return (true);
}

/*
 * __block_size_srch --
 *     Search the by-size skiplist for the specified size.
 参考https://www.jb51.net/article/199510.htm图形化理解
 */ //head跳表中查找第一个>=size长度的成员WT_SIZE，stack实际上就是记录查找的每一层的路径节点
static inline void
__block_size_srch(WT_SIZE **head, wt_off_t size, WT_SIZE ***stack)
{
    WT_SIZE **szp;
    int i;

    /*
     * Start at the highest skip level, then go as far as possible at each level before stepping
     * down to the next.
     *
     * Return a stack for an exact match or the next-largest item.
     */
    for (i = WT_SKIP_MAXDEPTH - 1, szp = &head[i]; i >= 0;)
        if (*szp != NULL && (*szp)->size < size)
            szp = &(*szp)->next[i];
        else
            stack[i--] = szp--;
}

/*
 * __block_off_srch_pair --
 *     Search a by-offset skiplist for before/after records of the specified offset.
 */

//例如场景1，a:[4096, 1404928], b:[1404928, 278528]， 删除的newext:[1401928,178528 ]就是横跨[A,B], beforep=a, afterp=b
//例如场景2，a:[4096, 1404928]， 删除的newext:[1104928,1204928 ]，也就是newext在a范围内，这时候beforep=a, after=NULL

//判断off位置是否在el对应ext跳跃表中，beforep对应最近的>=off的ext, afterp对应最近>=off的ext
static inline void
__block_off_srch_pair(WT_EXTLIST *el, wt_off_t off, WT_EXT **beforep, WT_EXT **afterp)
{
    WT_EXT **head, **extp;
    int i;

    *beforep = *afterp = NULL;

    head = el->off;

    /*
     * Start at the highest skip level, then go as far as possible at each level before stepping
     * down to the next.
     */
    for (i = WT_SKIP_MAXDEPTH - 1, extp = &head[i]; i >= 0;) {
        if (*extp == NULL) {
            --i;
            --extp;
            continue;
        }

        if ((*extp)->off < off) { /* Keep going at this level */
            *beforep = *extp;
            extp = &(*extp)->next[i];
        } else { /* Drop down a level */
            *afterp = *extp;
            --i;
            --extp;
        }
    }
}

/*
 * __block_ext_insert --
 *     Insert an extent into an extent list.
  注意__block_ext_insert和__block_append的区别:
  1. __block_ext_insert:ext会加入到el->size跳表中
  2. __block_append: ext不会加入el->size跳表中
 */
//ext按照ext->size大小添加到el->sz跳表，按照ext->off添加到el->off跳表，
//ext除了添加到el->sz跳表的size相同的el->sz.off中，还好添加到el->off跳表中
static int
__block_ext_insert(WT_SESSION_IMPL *session, WT_EXTLIST *el, WT_EXT *ext)
{
    WT_EXT **astack[WT_SKIP_MAXDEPTH];
    WT_SIZE *szp, **sstack[WT_SKIP_MAXDEPTH];
    u_int i;

    /*
     * If we are inserting a new size onto the size skiplist, we'll need a new WT_SIZE structure for
     * that skiplist.
     */
    //如果track_size为true, 则创建WT_SIZE添加添加到el->sz跳表中
    if (el->track_size) {
        //size跳表中查找第一个>=size长度的成员WT_SIZE，stack实际上就是记录查找的每一层的路径节点
        __block_size_srch(el->sz, ext->size, sstack);
        szp = *sstack[0];
        if (szp == NULL || szp->size != ext->size) {
            //这里是ext添加到size跳表的核心逻辑
            WT_RET(__wt_block_size_alloc(session, &szp));
            szp->size = ext->size;
            szp->depth = ext->depth;
            for (i = 0; i < ext->depth; ++i) {
                szp->next[i] = *sstack[i];
                *sstack[i] = szp;
            }
        }

        /*
         * Insert the new WT_EXT structure into the size element's offset skiplist.
         */
        //注意: 这里继续在上面找到的szp对应off跳表中查找和插入，这样做的目的应该是每个ext由唯一的off，但是可能多个ext的size相同
        //所以先确定size，然后在确定off

        //off跳表中查找第一个>=size长度的成员WT_SIZE，stack实际上就是记录查找的每一层的路径节点
        __block_off_srch(szp->off, ext->off, astack, true, NULL);
        for (i = 0; i < ext->depth; ++i) {
            //next数组大小实际上是ext->depth*2，next[0-ext->depth]这部分skip depth对应size跳跃表，把所有__wt_size串起来
            //  next[ext->depth, ext->depth*2]这部分skip depth对应Off跳跃表，代表拥有相同size但是off不相同的所有ext通过这里串起来
            ext->next[i + ext->depth] = *astack[i];
            *astack[i] = ext;
        }
    }
#ifdef HAVE_DIAGNOSTIC
    if (!el->track_size)
        for (i = 0; i < ext->depth; ++i)
            ext->next[i + ext->depth] = NULL;
#endif

    //ext添加到el->off跳表中
    /* Insert the new WT_EXT structure into the offset skiplist. */
    __block_off_srch(el->off, ext->off, astack, false, NULL);
    for (i = 0; i < ext->depth; ++i) {
        ext->next[i] = *astack[i];
        *astack[i] = ext;
    }

    ++el->entries;
    el->bytes += (uint64_t)ext->size;

    /* Update the cached end-of-list. */
    //说明这是el跳表中第一个elem
    if (ext->next[0] == NULL)
        el->last = ext;

    //从上面的分析可以看出ext除了添加到el->sz跳表的size相同的el->sz.off中，还好添加到el->off跳表中
    return (0);
}

/*
 * __block_off_insert --
 *     Insert a file range into an extent list.
 __block_off_srch根据off搜索
 __block_off_remove根据off删除
 __block_off_insert根据off插入跳表

 注意__block_ext_insert和__block_append的区别
 */
//分配一个ext，并按照ext->size大小添加到el->sz跳表，按照ext->off添加到el->off跳表
static int
__block_off_insert(WT_SESSION_IMPL *session, WT_EXTLIST *el, wt_off_t off, wt_off_t size)
{
    WT_EXT *ext;

    WT_RET(__wt_block_ext_alloc(session, &ext));
    ext->off = off;
    ext->size = size;

    //ext按照ext->size大小添加到el->sz跳表，按照ext->off添加到el->off跳表
    return (__block_ext_insert(session, el, ext));
}

#ifdef HAVE_DIAGNOSTIC
/*
 * __block_off_match --
 *     Return if any part of a specified range appears on a specified extent list.
 */
//判断[off,off+size]是否在el这个跳表对应磁盘空间中
static bool
__block_off_match(WT_EXTLIST *el, wt_off_t off, wt_off_t size)
{
    WT_EXT *before, *after;

    /* Search for before and after entries for the offset. */
    __block_off_srch_pair(el, off, &before, &after);

    /* If "before" or "after" overlaps, we have a winner. */
    if (before != NULL && before->off + before->size > off)
        return (true);
    if (after != NULL && off + size > after->off)
        return (true);
    return (false);
}

/*
 * __wt_block_misplaced --
 *     Complain if a block appears on the available or discard lists.
 */
//判断[off,off+size]是否在discard或者这个跳表对应磁盘空间中
int
__wt_block_misplaced(WT_SESSION_IMPL *session, WT_BLOCK *block, const char *list, wt_off_t offset,
  uint32_t size, bool live, const char *func, int line)
{
    const char *name;

    name = NULL;

    /*
     * Don't check during the salvage read phase, we might be reading an already freed overflow
     * page.
     */
    if (F_ISSET(session, WT_SESSION_QUIET_CORRUPT_FILE))
        return (0);

    /*
     * Verify a block the btree engine thinks it "owns" doesn't appear on the available or discard
     * lists (it might reasonably be on the alloc list, if it was allocated since the last
     * checkpoint). The engine "owns" a block if it's trying to read or free the block, and those
     * functions make this check.
     *
     * Any block being read or freed should not be "available".
     *
     * Any block being read or freed in the live system should not be on the discard list. (A
     * checkpoint handle might be reading a block which is on the live system's discard list; any
     * attempt to free a block from a checkpoint handle has already failed.)
     */
    __wt_spin_lock(session, &block->live_lock);
    //判断[off,off+size]是否在avail这个跳表对应磁盘空间中
    if (__block_off_match(&block->live.avail, offset, size))
        name = "available";
    //判断[off,off+size]是否在discard这个跳表对应磁盘空间中
    else if (live && __block_off_match(&block->live.discard, offset, size))
        name = "discard";
    __wt_spin_unlock(session, &block->live_lock);
    if (name != NULL)
        return (__wt_panic(session, WT_PANIC,
          "%s failed: %" PRIuMAX "/%" PRIu32 " is on the %s list (%s, %d)", list, (uintmax_t)offset,
          size, name, func, line));
    return (0);
}
#endif

/*
 * __block_off_remove --
 *     Remove a record from an extent list.
 __block_off_srch根据off搜索
 __block_off_remove根据off删除
 __block_off_insert根据off插入跳表
 */
//先从el->off中清除off对应的ext, 在从el->sz中清除对应的ext(包括sz跳跃表的，也包括sz.szp跳跃表的)
static int
__block_off_remove(
  WT_SESSION_IMPL *session, WT_BLOCK *block, WT_EXTLIST *el, wt_off_t off, WT_EXT **extp)
{
    WT_EXT *ext, *penultimate_ext, **astack[WT_SKIP_MAXDEPTH];
    WT_SIZE *szp, **sstack[WT_SKIP_MAXDEPTH];
    u_int i;

    /* Find and remove the record from the by-offset skiplist. */
    __block_off_srch(el->off, off, astack, false, &penultimate_ext);
    ext = *astack[0];
    if (ext == NULL || ext->off != off)
        goto corrupt;
    for (i = 0; i < ext->depth; ++i)
        *astack[i] = ext->next[i];

    /*
     * Find and remove the record from the size's offset skiplist; if that empties the by-size
     * skiplist entry, remove it as well.
     */
    if (el->track_size) {
        __block_size_srch(el->sz, ext->size, sstack);
        szp = *sstack[0];
        if (szp == NULL || szp->size != ext->size)
            WT_RET_PANIC(session, EINVAL, "extent not found in by-size list during remove");
        __block_off_srch(szp->off, off, astack, true, NULL);
        ext = *astack[0];
        if (ext == NULL || ext->off != off)
            goto corrupt;
        for (i = 0; i < ext->depth; ++i)
            *astack[i] = ext->next[i + ext->depth];
        if (szp->off[0] == NULL) {
            for (i = 0; i < szp->depth; ++i)
                *sstack[i] = szp->next[i];
            __wt_block_size_free(session, szp);
        }
    }
#ifdef HAVE_DIAGNOSTIC
    if (!el->track_size) {
        bool not_null;
        for (i = 0, not_null = false; i < ext->depth; ++i)
            if (ext->next[i + ext->depth] != NULL)
                not_null = true;
        WT_ASSERT(session, not_null == false);
    }
#endif

    --el->entries;
    el->bytes -= (uint64_t)ext->size;

    /* Return the record if our caller wants it, otherwise free it. */
    if (extp == NULL)
        __wt_block_ext_free(session, ext);
    else
        *extp = ext;

    /* Update the cached end-of-list. */
    if (el->last == ext) {
        //el->last = NULL;

        if (penultimate_ext == NULL) {
            el->last = NULL;
        } else {
            el->last = penultimate_ext;
        }

    }
    return (0);

corrupt:
    WT_BLOCK_RET(
      session, block, EINVAL, "attempt to remove non-existent offset from an extent list");
}

/*
 * __wt_block_off_remove_overlap --
 *     Remove a range from an extent list, where the range may be part of an overlapping entry.

场景1:
                 ext1(add)                                   ext2(add)
                  /\                                           /\
    |-------------  ---------|                      |----------  ---------|
    |        a_size         off                 off+size      b_size      |
    |<---------------------->|<-------------------->|<------------------->|
    |______________________________  _____________________________________|
befor.off                          \/                            befor.off+befor.size
                             befor ext(remove)

场景2:
                                             ext1(add)
                                                 /\
                           |---------------------  -----------------------|
   off                 off+size      b_size
    |<-------------------->|<-------------------------------------------->|
    |______________________________  _____________________________________|
after.off                          \/                            after.off+after.size
                             after ext(remove)

//例如场景1: ext=[4096, 1712128], 要删除的off:1409024, size:28672，即删除ext[1409024, 1409024+28672]
//  最终这个ext会被拆分为2个ext=a:[4096, 1404928], b:[1437696, 278528]

//例如场景2: ext=[4096, 1712128], 要删除的off:4096, size:1612128，即删除ext[4096, 1712128]，然后insert一个ext[1612128, 1712128]

 */
//从跳跃表中删除[off, size]这块区间, 分割产生的新ext添加到跳跃表中
int
__wt_block_off_remove_overlap(
  WT_SESSION_IMPL *session, WT_BLOCK *block, WT_EXTLIST *el, wt_off_t off, wt_off_t size)
{
    WT_EXT *before, *after, *ext;
    wt_off_t a_off, a_size, b_off, b_size;
    WT_DECL_RET;

    WT_ASSERT(session, off != WT_BLOCK_INVALID_OFFSET);

    //例如场景1，a:[4096, 1404928], b:[1404928, 278528]， 删除的newext:[1401928,178528 ]就是横跨[A,B], beforep=a, afterp=b
    //例如场景2，a:[4096, 1404928]， 删除的newext:[1104928,1204928 ]，也就是newext在a范围内，这时候beforep=a, after=NULL
    /* Search for before and after entries for the offset. */
    __block_off_srch_pair(el, off, &before, &after);

//例如场景1: ext=[4096, 1712128], 要删除的off:1409024, size:28672，即删除ext[1409024, 1409024+28672]
//  最终这个ext会被拆分为2个ext=a:[4096, 1404928], b:[1437696, 278528]

//例如场景2: ext=[4096, 1712128], 要删除的off:4096, size:1612128，即删除ext[4096, 1712128]，然后insert一个ext[1612128, 1712128]

//例如场景3(不可能存在跨两个ext的情况，这种情况不可能): ext1:[4096, 1404928], ext2:[1404928, 2178528]， 删除的dekext:[1101928,1581528 ]就是横跨[ext1, ext2]
//  最终[ext1, ext2]两个ext会被删除，并且重新创建两个ext，范围分别是[4096, 1101928], [1581528, 2178528]

    /* If "before" or "after" overlaps, retrieve the overlapping entry. */
    //[off, off+size]在befor这个ext范围中
    if (before != NULL && before->off + before->size > off) {
        //例如ext=[4096, 1712128], 要删除的off:1409024, size:28672，即删除ext[4096, 1712128]磁盘范围中的[1409024,28672]
        //最终这个ext会被拆分为2个ext=a:[4096, 1404928], b:[1437696, 278528]
        /*
                         ext1(add)                                   ext2(add)
                          /\                                           /\
            |-------------  ---------|                      |----------  ---------|
            |        a_size         off                 off+size      b_size      |
            |<---------------------->|<-------------------->|<------------------->|
            |______________________________  _____________________________________|
        befor.off                          \/                            befor.off+befor.size
                                     befor ext(remove)
        */
       // printf("yang test ..1....__wt_block_off_remove_overlap.....befor:[%d, %d], off:%d, size:%d\r\n",
       //   (int)before->off, (int)before->size, (int)off, (int)size);
      if (before->off + before->size < off + size) {
          __wt_verbose_error(session, WT_VERB_BLOCK,
              "block off remove out of bounds befor=[%" PRIu64 ", %" PRIu64 "], off:size=[%" PRIu64 ", %" PRIu64 "]", (uint64_t)before->off, (uint64_t)before->size, (uint64_t)off, (uint64_t)size);
             WT_ERR_PANIC(session, EINVAL, "block off remove out of bounds");
      }
      WT_RET(__block_off_remove(session, block, el, before->off, &ext));
        /* Calculate overlapping extents. */
        a_off = ext->off;
        a_size = off - ext->off;
        b_off = off + size;
        b_size = ext->size - (a_size + size);

      if (a_size > 0 && b_size > 0) {
         __wt_verbose(session, WT_VERB_BLOCK,
          "%s: %" PRIdMAX "-%" PRIdMAX " range shrinks to %" PRIdMAX "-%" PRIdMAX " and %" PRIdMAX "-%" PRIdMAX,
          el->name, (intmax_t)before->off, (intmax_t)before->off + (intmax_t)before->size,
          (intmax_t)(a_off), (intmax_t)(a_off + a_size),
          (intmax_t)(b_off), (intmax_t)(b_off + b_size));
      } else if (a_size > 0) {
         __wt_verbose(session, WT_VERB_BLOCK,
          "%s: %" PRIdMAX "-%" PRIdMAX " range shrinks to %" PRIdMAX "-%" PRIdMAX,
          el->name, (intmax_t)before->off, (intmax_t)before->off + (intmax_t)before->size,
          (intmax_t)(a_off), (intmax_t)(a_off + a_size));
      } else if (b_size > 0) {
         __wt_verbose(session, WT_VERB_BLOCK,
          "%s: %" PRIdMAX "-%" PRIdMAX " range shrinks to %" PRIdMAX "-%" PRIdMAX,
          el->name, (intmax_t)before->off, (intmax_t)before->off + (intmax_t)before->size,
          (intmax_t)(b_off), (intmax_t)(b_off + b_size));
      }
    //[off, off+size]在after对应ext空间中，例如a:[4096, 1404928], b:[1404928, 278528]， newext:[1401928,178528 ]就是横跨[A,B]
    } else if (after != NULL && off + size > after->off) {
      /*
                                                  ext1(add)
                                                      /\
                                |---------------------  -----------------------|
        off                 off+size      b_size
         |<-------------------->|<-------------------------------------------->|
         |______________________________  _____________________________________|
     after.off                          \/                            after.off+after.size
                                  after ext(remove)

        */

      if (off != after->off || off + size > after->off + after->size) {
          __wt_verbose_error(session, WT_VERB_BLOCK,
              "block off remove out of bounds after=[%" PRIu64 ", %" PRIu64 "], off:size=[%" PRIu64 ", %" PRIu64 "]", (uint64_t)after->off, (uint64_t)after->size, (uint64_t)off, (uint64_t)size);
             WT_ERR_PANIC(session, EINVAL, "block off remove out of bounds");
      }

        //删除after->off这个ext
        WT_RET(__block_off_remove(session, block, el, after->off, &ext));

        /*
         * Calculate overlapping extents. There's no initial overlap since the after extent
         * presumably cannot begin before "off".
         */
        a_off = WT_BLOCK_INVALID_OFFSET;
        a_size = 0;
        b_off = off + size;
        b_size = ext->size - (b_off - ext->off);

        if (b_size > 0)
            __wt_verbose(session, WT_VERB_BLOCK,
                "%s: %" PRIdMAX "-%" PRIdMAX " range shrinks to %" PRIdMAX "-%" PRIdMAX,
                el->name, (intmax_t)after->off, (intmax_t)after->off + (intmax_t)after->size,
                (intmax_t)(b_off), (intmax_t)(b_off + b_size));

    } else
        return (WT_NOTFOUND);

    /*
     * If there are overlaps, insert the item; re-use the extent structure and save the allocation
     * (we know there's no need to merge).
     */
    if (a_size != 0) {
        ext->off = a_off;
        ext->size = a_size;
        WT_RET(__block_ext_insert(session, el, ext));
        ext = NULL;
    }
    if (b_size != 0) {
        if (ext == NULL) //after不存在，则直接分配一个新的off,并添加到跳跃表
            //分配一个ext，并按照ext->size大小添加到el->sz跳表，按照ext->off添加到el->off跳表
            WT_RET(__block_off_insert(session, el, b_off, b_size));
        else {//跳表中已经有这个after，但是after:[after->off, after->size]空间不够，则直接扩容空间即可，扩容后从新插入跳跃表
            ext->off = b_off;
            ext->size = b_size;
            WT_RET(__block_ext_insert(session, el, ext));
            ext = NULL;
        }
    }

    if (ext != NULL)
        __wt_block_ext_free(session, ext);

    return (0);

err:
    return (ret);
}

/*
 * __block_extend --
 *     Extend the file to allocate space.
 //block->size增加size长度，同时offp记录修改前block->size的长度
 */
static inline int
__block_extend(WT_SESSION_IMPL *session, WT_BLOCK *block, wt_off_t *offp, wt_off_t size)
{
    /*
     * Callers of this function are expected to have already acquired any locks required to extend
     * the file.
     *
     * We should never be allocating from an empty file.
     */
    if (block->size < block->allocsize)
        WT_RET_MSG(session, EINVAL, "file has no description information");

    /*
     * Make sure we don't allocate past the maximum file size.  There's no
     * easy way to know the maximum wt_off_t on a system, limit growth to
     * 8B bits (we currently check an wt_off_t is 8B in verify_build.h). I
     * don't think we're likely to see anything bigger for awhile.
     */
    if (block->size > (wt_off_t)INT64_MAX - size)
        WT_RET_MSG(session, WT_ERROR, "block allocation failed, file cannot grow further");

    *offp = block->size;
    block->size += size;

    WT_STAT_DATA_INCR(session, block_extension);

//    [1694078097:748116][50315:0x7f6417db4800], file:access.wt, WT_CURSOR.__curfile_insert: [WT_VERB_BLOCK][DEBUG_1]: file extend 4096-32768
//    [1694078097:748208][50315:0x7f6417db4800], file:access.wt, WT_CURSOR.__curfile_insert: [WT_VERB_BLOCK][DEBUG_1]: file extend 32768-61440
//    [1694078097:748309][50315:0x7f6417db4800], file:access.wt, WT_CURSOR.__curfile_insert: [WT_VERB_BLOCK][DEBUG_1]: file extend 61440-90112
//    [1694078097:748369][50315:0x7f6417db4800], file:access.wt, WT_CURSOR.__curfile_insert: [WT_VERB_BLOCK][DEBUG_1]: file extend 90112-118784
//    [1694078097:748451][50315:0x7f6417db4800], file:access.wt, WT_CURSOR.__curfile_insert: [WT_VERB_BLOCK][DEBUG_1]: file extend 118784-147456
    __wt_verbose(session, WT_VERB_BLOCK, "file extend %" PRIdMAX "-%" PRIdMAX, (intmax_t)*offp,
      (intmax_t)(*offp + size));

    return (0);
}

/*
 * __wt_block_alloc --
 *     Alloc a chunk of space from the underlying file.
 offp实际上说就是该长度size数据在磁盘中的位置，也就是WT_BLOCK.size成员，代表追加的文件末尾位置
 size为reconcile后的chunk需要写入磁盘的数据长度
 */
int
__wt_block_alloc(WT_SESSION_IMPL *session, WT_BLOCK *block, wt_off_t *offp, wt_off_t size)
{
    WT_EXT *ext, **estack[WT_SKIP_MAXDEPTH];
    WT_SIZE *szp, **sstack[WT_SKIP_MAXDEPTH];

    /* If a sync is running, no other sessions can allocate blocks. */
    WT_ASSERT(session, WT_SESSION_BTREE_SYNC_SAFE(session, S2BT(session)));

    /* Assert we're maintaining the by-size skiplist. */
    WT_ASSERT(session, block->live.avail.track_size != 0);

    WT_STAT_DATA_INCR(session, block_alloc);
    if (size % block->allocsize != 0)
        WT_RET_MSG(session, EINVAL,
          "cannot allocate a block size %" PRIdMAX
          " that is not a multiple of the allocation size %" PRIu32,
          (intmax_t)size, block->allocsize);

    /*
     * Allocation is either first-fit (lowest offset), or best-fit (best size). If it's first-fit,
     * walk the offset list linearly until we find an entry that will work.
     *
     * If it's best-fit by size, search the by-size skiplist for the size and take the first entry
     * on the by-size offset list. This means we prefer best-fit over lower offset, but within a
     * size we'll prefer an offset appearing earlier in the file.
     *
     * If we don't have anything big enough, extend the file.
     */
   // printf("yang test .................__block_append....block->live.avail.bytes:%d............ext entries:%d\r\n",
   //     (int)block->live.avail.bytes, (int)block->live.alloc.entries);
    //block->live.avail.bytes也就是block_reuse_bytes file bytes available for reuse
    //也就是avail中可重复利用的空间不够，则在append中重新alloc新的ext来保存[off, size]
    if (block->live.avail.bytes < (uint64_t)size)
        goto append;
    if (block->allocfirst > 0) {
        if (!__block_first_srch(block->live.avail.off, size, estack))
            goto append;
        ext = *estack[0];
    } else {
        //head跳表中查找第一个>=size长度的成员WT_SIZE，stack实际上就是记录查找的每一层的路径节点
        __block_size_srch(block->live.avail.sz, size, sstack);
        if ((szp = *sstack[0]) == NULL) {//block->live.avail.sz跳表中没有找到一个>=size长度的成员WT_SIZE
        //如果block->live.avail上面一直找不到size这个ext，则后面的__block_append只会用一个ext来管理所有的reconcile的多个chunk数据
        //参考debug_wt_block_alloc1.c
append:
            //block->size增加size长度，同时offp记录修改前block->size的长度,
            //也就是向block对应磁盘空间往后移动size字节，在外层通过offp返回这部分空间的其实地址，offp开始的size字节空间就可以被新的WT_EXT使用
            WT_RET(__block_extend(session, block, offp, size));
            //获取一个WT_EXT元数据结构添加到block->live.alloc跳跃表中，WT_EXT用于存储一个page对应的磁盘元数据信息
            WT_RET(__block_append(session, block, &block->live.alloc, *offp, (wt_off_t)size));
            return (0);
        }

        /* Take the first record. */
        ext = szp->off[0];
    }

    //走这里说明直接利用avail中可重复利用的ext来存储[off, size]

   // printf("yang test .................__wt_block_alloc......2222222.............\r\n");
    //参考debug_wt_block_alloc2.c会走这里
    /* Remove the record, and set the returned offset. */
    WT_RET(__block_off_remove(session, block, &block->live.avail, ext->off, &ext));
    *offp = ext->off;

    /* If doing a partial allocation, adjust the record and put it back. */
    if (ext->size > size) {
        __wt_verbose(session, WT_VERB_BLOCK,
          "allocate from live.avail %" PRIdMAX " from range %" PRIdMAX "-%" PRIdMAX ", range shrinks to %" PRIdMAX
          "-%" PRIdMAX,
          (intmax_t)size, (intmax_t)ext->off, (intmax_t)(ext->off + ext->size),
          (intmax_t)(ext->off + size), (intmax_t)(ext->off + size + ext->size - size));

        ext->off += size;
        ext->size -= size;
        //ext按照ext->size大小添加到el->sz跳表，按照ext->off添加到el->off跳表
        WT_RET(__block_ext_insert(session, &block->live.avail, ext));
    } else {
        __wt_verbose(session, WT_VERB_BLOCK, "remove from live.avail range %" PRIdMAX "-%" PRIdMAX,
          (intmax_t)ext->off, (intmax_t)(ext->off + ext->size));

        __wt_block_ext_free(session, ext);
    }

    /* Add the newly allocated extent to the list of allocations. */
    WT_RET(__block_merge(session, block, &block->live.alloc, *offp, (wt_off_t)size));
    return (0);
}

/*
 * __wt_block_free --
 *     Free a cookie-referenced chunk of space to the underlying file.
 */
//__rec_write_wrapup->__wt_btree_block_free->__bm_free->__wt_block_free
int
__wt_block_free(WT_SESSION_IMPL *session, WT_BLOCK *block, const uint8_t *addr, size_t addr_size)
{
    WT_DECL_RET;
    wt_off_t offset;
    uint32_t checksum, objectid, size;

    WT_STAT_DATA_INCR(session, block_free);

    /* Crack the cookie. */
    //获取[offset, offset+size]这块磁盘空间对应的元数据
    WT_RET(__wt_block_addr_unpack(
      session, block, addr, addr_size, &objectid, &offset, &size, &checksum));

    /*
     * Freeing blocks in a previous object isn't possible in the current architecture. We'd like to
     * know when a previous object is either completely rewritten (or more likely, empty enough that
     * rewriting remaining blocks is worth doing). Just knowing which blocks are no longer in use
     * isn't enough to remove them (because the internal pages have to be rewritten and we don't
     * know where they are); the simplest solution is probably to keep a count of freed bytes from
     * each object in the metadata, and when enough of the object is no longer in use, perform a
     * compaction like process to do any remaining cleanup.
     */
    if (objectid != block->objectid)
        return (0);

    //释放[offset, offset+size]这部分磁盘空间，实际上不是真正的释放，而是
    __wt_verbose(session, WT_VERB_BLOCK, "block free %" PRIu32 ": %" PRIdMAX "/%" PRIdMAX, objectid,
      (intmax_t)offset, (intmax_t)size);

#ifdef HAVE_DIAGNOSTIC
    //判断[off,off+size]是否在discard或者这个跳表对应磁盘空间中
    WT_RET(__wt_block_misplaced(
      session, block, "free", offset, size, true, __PRETTY_FUNCTION__, __LINE__));
#endif

    WT_RET(__wt_block_ext_prealloc(session, 5));
    __wt_spin_lock(session, &block->live_lock);
    ret = __wt_block_off_free(session, block, objectid, offset, (wt_off_t)size);
    __wt_spin_unlock(session, &block->live_lock);

    return (ret);
}

/*
 * __wt_block_off_free --
 *     Free a file range to the underlying file.
 */
//从alloc跳跃表中先删除[offset, offset+size]对应的ext，然后添加到avail或者discard跳跃表中
//如果avail中包含[offset, offset+size]对应ext，则添加到avail中，否则条件到discard中
int
__wt_block_off_free(
  WT_SESSION_IMPL *session, WT_BLOCK *block, uint32_t objectid, wt_off_t offset, wt_off_t size)
{
    WT_DECL_RET;

    /* If a sync is running, no other sessions can free blocks. */
    WT_ASSERT(session, WT_SESSION_BTREE_SYNC_SAFE(session, S2BT(session)));

    /* We can't reuse free space in an object. */
    if (objectid != block->objectid)
        return (0);

    /*
     * Callers of this function are expected to have already acquired any locks required to
     * manipulate the extent lists.
     *
     * We can reuse this extent immediately if it was allocated during this checkpoint, merge it
     * into the avail list (which slows file growth in workloads including repeated overflow record
     * modification). If this extent is referenced in a previous checkpoint, merge into the discard
     * list.
     */
   // printf("yang test ...1....__wt_block_off_free......alloc:%u, avail:%u, discard:%u\r\n", block->live.alloc.entries
    //    , block->live.avail.entries, block->live.discard.entries);
    //从alloc跳跃表中删除ext空间[offset, offset+size]，然后__block_merge中把该空间对应ext添加到avail跳跃表中
    if ((ret = __wt_block_off_remove_overlap(session, block, &block->live.alloc, offset, size)) ==
      0) {
       // printf("yang test ...2....__wt_block_off_free......alloc:%u, avail:%u, discard:%u\r\n", block->live.alloc.entries
         //   , block->live.avail.entries, block->live.discard.entries);
        //被删除的offset对应的ext重新添加到avail中，代表的实际上就是磁盘碎片
        ret = __block_merge(session, block, &block->live.avail, offset, size);
    } else if (ret == WT_NOTFOUND)
        //从alloc跳跃表中删除某个范围的ext，如果alloc跳跃表中没找到，则这个要删除范围对应的ext添加到discard跳跃表中，参考__wt_block_off_free
        ret = __block_merge(session, block, &block->live.discard, offset, size);
   /// printf("yang test ...3....__wt_block_off_free......alloc:%u, avail:%u, discard:%u\r\n", block->live.alloc.entries
   //     , block->live.avail.entries, block->live.discard.entries);
    return (ret);
}

#ifdef HAVE_DIAGNOSTIC
/*
 * __wt_block_extlist_check --
 *     Return if the extent lists overlap.
 */
int
__wt_block_extlist_check(WT_SESSION_IMPL *session, WT_EXTLIST *al, WT_EXTLIST *bl)
{
    WT_EXT *a, *b;

    a = al->off[0];
    b = bl->off[0];

    /* Walk the lists in parallel, looking for overlaps. */
    while (a != NULL && b != NULL) {
        /*
         * If there's no overlap, move the lower-offset entry to the next entry in its list.
         */
        if (a->off + a->size <= b->off) {
            a = a->next[0];
            continue;
        }
        if (b->off + b->size <= a->off) {
            b = b->next[0];
            continue;
        }
        WT_RET_PANIC(session, EINVAL, "checkpoint merge check: %s list overlaps the %s list",
          al->name, bl->name);
    }
    return (0);
}
#endif

/*
 * __wt_block_extlist_overlap --
 *     Review a checkpoint's alloc/discard extent lists, move overlaps into the live system's
 *     checkpoint-avail list.
 */
int
__wt_block_extlist_overlap(WT_SESSION_IMPL *session, WT_BLOCK *block, WT_BLOCK_CKPT *ci)
{
    WT_EXT *alloc, *discard;

    alloc = ci->alloc.off[0];
    discard = ci->discard.off[0];

    /* Walk the lists in parallel, looking for overlaps. */
    while (alloc != NULL && discard != NULL) {
        /*
         * If there's no overlap, move the lower-offset entry to the next entry in its list.
         */
        if (alloc->off + alloc->size <= discard->off) {
            alloc = alloc->next[0];
            continue;
        }
        if (discard->off + discard->size <= alloc->off) {
            discard = discard->next[0];
            continue;
        }

        /* Reconcile the overlap. */
        WT_RET(__block_ext_overlap(session, block, &ci->alloc, &alloc, &ci->discard, &discard));
    }
    return (0);
}

/*
 * __block_ext_overlap --
 *     Reconcile two overlapping ranges.
 */
static int
__block_ext_overlap(WT_SESSION_IMPL *session, WT_BLOCK *block, WT_EXTLIST *ael, WT_EXT **ap,
  WT_EXTLIST *bel, WT_EXT **bp)
{
    WT_EXT *a, *b, **ext;
    WT_EXTLIST *avail, *el;
    wt_off_t off, size;

    avail = &block->live.ckpt_avail;

    /*
     * The ranges overlap, choose the range we're going to take from each.
     *
     * We can think of the overlap possibilities as 11 different cases:
     *
     *		AAAAAAAAAAAAAAAAAA
     * #1		BBBBBBBBBBBBBBBBBB		ranges are the same
     * #2	BBBBBBBBBBBBB				overlaps the beginning
     * #3			BBBBBBBBBBBBBBBB	overlaps the end
     * #4		BBBBB				B is a prefix of A
     * #5			BBBBBB			B is middle of A
     * #6			BBBBBBBBBB		B is a suffix of A
     *
     * and:
     *
     *		BBBBBBBBBBBBBBBBBB
     * #7	AAAAAAAAAAAAA				same as #3
     * #8			AAAAAAAAAAAAAAAA	same as #2
     * #9		AAAAA				A is a prefix of B
     * #10			AAAAAA			A is middle of B
     * #11			AAAAAAAAAA		A is a suffix of B
     *
     *
     * By swapping the arguments so "A" is always the lower range, we can
     * eliminate cases #2, #8, #10 and #11, and only handle 7 cases:
     *
     *		AAAAAAAAAAAAAAAAAA
     * #1		BBBBBBBBBBBBBBBBBB		ranges are the same
     * #3			BBBBBBBBBBBBBBBB	overlaps the end
     * #4		BBBBB				B is a prefix of A
     * #5			BBBBBB			B is middle of A
     * #6			BBBBBBBBBB		B is a suffix of A
     *
     * and:
     *
     *		BBBBBBBBBBBBBBBBBB
     * #7	AAAAAAAAAAAAA				same as #3
     * #9		AAAAA				A is a prefix of B
     */
    a = *ap;
    b = *bp;
    if (a->off > b->off) { /* Swap */
        b = *ap;
        a = *bp;
        ext = ap;
        ap = bp;
        bp = ext;
        el = ael;
        ael = bel;
        bel = el;
    }

    if (a->off == b->off) {       /* Case #1, #4, #9 */
        if (a->size == b->size) { /* Case #1 */
                                  /*
                                   * Move caller's A and B to the next element Add that A and B
                                   * range to the avail list Delete A and B
                                   */
            *ap = (*ap)->next[0];
            *bp = (*bp)->next[0];
            WT_RET(__block_merge(session, block, avail, b->off, b->size));
            WT_RET(__block_off_remove(session, block, ael, a->off, NULL));
            WT_RET(__block_off_remove(session, block, bel, b->off, NULL));
        } else if (a->size > b->size) { /* Case #4 */
                                        /*
                                         * Remove A from its list Increment/Decrement A's
                                         * offset/size by the size of B Insert A on its list
                                         */
            WT_RET(__block_off_remove(session, block, ael, a->off, &a));
            a->off += b->size;
            a->size -= b->size;
            WT_RET(__block_ext_insert(session, ael, a));

            /*
             * Move caller's B to the next element Add B's range to the avail list Delete B
             */
            *bp = (*bp)->next[0];
            WT_RET(__block_merge(session, block, avail, b->off, b->size));
            WT_RET(__block_off_remove(session, block, bel, b->off, NULL));
        } else { /* Case #9 */
                 /*
                  * Remove B from its list Increment/Decrement B's offset/size by the size of A
                  * Insert B on its list
                  */
            WT_RET(__block_off_remove(session, block, bel, b->off, &b));
            b->off += a->size;
            b->size -= a->size;
            WT_RET(__block_ext_insert(session, bel, b));

            /*
             * Move caller's A to the next element Add A's range to the avail list Delete A
             */
            *ap = (*ap)->next[0];
            WT_RET(__block_merge(session, block, avail, a->off, a->size));
            WT_RET(__block_off_remove(session, block, ael, a->off, NULL));
        } /* Case #6 */
    } else if (a->off + a->size == b->off + b->size) {
        /*
         * Remove A from its list Decrement A's size by the size of B Insert A on its list
         */
        WT_RET(__block_off_remove(session, block, ael, a->off, &a));
        a->size -= b->size;
        WT_RET(__block_ext_insert(session, ael, a));

        /*
         * Move caller's B to the next element Add B's range to the avail list Delete B
         */
        *bp = (*bp)->next[0];
        WT_RET(__block_merge(session, block, avail, b->off, b->size));
        WT_RET(__block_off_remove(session, block, bel, b->off, NULL));
    } else if /* Case #3, #7 */
      (a->off + a->size < b->off + b->size) {
        /*
         * Add overlap to the avail list
         */
        off = b->off;
        size = (a->off + a->size) - b->off;
        WT_RET(__block_merge(session, block, avail, off, size));

        /*
         * Remove A from its list Decrement A's size by the overlap Insert A on its list
         */
        WT_RET(__block_off_remove(session, block, ael, a->off, &a));
        a->size -= size;
        WT_RET(__block_ext_insert(session, ael, a));

        /*
         * Remove B from its list Increment/Decrement B's offset/size by the overlap Insert B on its
         * list
         */
        WT_RET(__block_off_remove(session, block, bel, b->off, &b));
        b->off += size;
        b->size -= size;
        WT_RET(__block_ext_insert(session, bel, b));
    } else { /* Case #5 */
        /* Calculate the offset/size of the trailing part of A. */
        off = b->off + b->size;
        size = (a->off + a->size) - off;

        /*
         * Remove A from its list Decrement A's size by trailing part of A plus B's size Insert A on
         * its list
         */
        WT_RET(__block_off_remove(session, block, ael, a->off, &a));
        a->size = b->off - a->off;
        WT_RET(__block_ext_insert(session, ael, a));

        /* Add trailing part of A to A's list as a new element. */
        WT_RET(__block_merge(session, block, ael, off, size));

        /*
         * Move caller's B to the next element Add B's range to the avail list Delete B
         */
        *bp = (*bp)->next[0];
        WT_RET(__block_merge(session, block, avail, b->off, b->size));
        WT_RET(__block_off_remove(session, block, bel, b->off, NULL));
    }

    return (0);
}

/*
 * __wt_block_extlist_merge --
 *     Merge one extent list into another.
 */
int
__wt_block_extlist_merge(WT_SESSION_IMPL *session, WT_BLOCK *block, WT_EXTLIST *a, WT_EXTLIST *b)
{
    WT_EXT *ext;
    WT_EXTLIST tmp;
    u_int i;

    __wt_verbose_debug2(session, WT_VERB_BLOCK, "block_extlist merging %s into %s", a->name, b->name);

    /*
     * Sometimes the list we are merging is much bigger than the other: if so, swap the lists around
     * to reduce the amount of work we need to do during the merge. The size lists have to match as
     * well, so this is only possible if both lists are tracking sizes, or neither are.
     * Attention:
     * We can no longer use the "a" variable inside and outside the function because it's value has changed
     */
    if (a->track_size == b->track_size && a->entries > b->entries) {
        tmp = *a;
        a->bytes = b->bytes;
        b->bytes = tmp.bytes;
        a->entries = b->entries;
        b->entries = tmp.entries;
        for (i = 0; i < WT_SKIP_MAXDEPTH; i++) {
            a->off[i] = b->off[i];
            b->off[i] = tmp.off[i];
            a->sz[i] = b->sz[i];
            b->sz[i] = tmp.sz[i];
        }
    }

    WT_EXT_FOREACH (ext, a->off)
        WT_RET(__block_merge(session, block, b, ext->off, ext->size));

    return (0);
}

/*
 * __block_append --
 *     Append a new entry to the allocation list.
注意__block_ext_insert和__block_append的区别:
1. __block_ext_insert:ext会加入到el->size跳表中
2. __block_append: ext不会加入el->size跳表中
 */
//获取已有的一个ext或者新建一个ext来管理reconcile等拆分后需要写入磁盘的多个chunk数据
//注意: 如果这里创建新的ext, 为什么只添加到了el->off跳表中，没有条件到el->size跳表中，原因是只有WT_BLOCK_CKPT.avail或者WT_BLOCK_CKPT.ckpt_avail才会在__wt_block_extlist_init中设置el->track_size为true
static int
__block_append(
  WT_SESSION_IMPL *session, WT_BLOCK *block, WT_EXTLIST *el, wt_off_t off, wt_off_t size)
{
    WT_EXT *ext, **astack[WT_SKIP_MAXDEPTH];
    u_int i;

    WT_UNUSED(block);
    //注意这里，说明不需要加入el->size跳表中
    WT_ASSERT(session, el->track_size == 0);

    /*
     * Identical to __block_merge, when we know the file is being extended, that is, the information
     * is either going to be used to extend the last object on the list, or become a new object
     * ending the list.
     *
     * The terminating element of the list is cached, check it; otherwise, get a stack for the last
     * object in the skiplist, check for a simple extension, and otherwise append a new structure.
     */
    //el->last有指向跳跃表的最后一个ext, 并且最后的这个ext末尾和off刚好相等，也就是可以直接追加数据
    //也就是可以让一次reconcile生成的多个chunk数据通过一个ext管理
    if ((ext = el->last) != NULL && ext->off + ext->size == off)
        ext->size += size;
    else {
        //从跳跃表中查找获取el->off对应跳表的最后一个WT_EXT成员
        ext = __block_off_srch_last(el, astack, true);
        if (ext != NULL && ext->off + ext->size == off)
            ext->size += size;
        else {
            //获取一个WT_EXT结构，先从预分配的cache中获取，如果cache中的用完了，则重新分配一个WT_EXT
            WT_RET(__wt_block_ext_alloc(session, &ext));
            ext->off = off;
            ext->size = size;

            for (i = 0; i < ext->depth; ++i)
                *astack[i] = ext;
            ++el->entries;
        }

        /* Update the cached end-of-list */
        //注意: 如果这里创建新的ext, 为什么只添加到了el->off跳表中，没有条件到el->size跳表中，
        //原因是只有WT_BLOCK_CKPT.avail或者WT_BLOCK_CKPT.ckpt_avail才会在__wt_block_extlist_init中设置el->track_size为true
        el->last = ext;
    }
    el->bytes += (uint64_t)size;

   // printf("yang test .................__block_append................ext entries:%d\r\n", (int)el->entries);
    return (0);
}

/*
 * __wt_block_insert_ext --
 *     Insert an extent into an extent list, merging if possible.
 */
int
__wt_block_insert_ext(
  WT_SESSION_IMPL *session, WT_BLOCK *block, WT_EXTLIST *el, wt_off_t off, wt_off_t size)
{
    /*
     * There are currently two copies of this function (this code is a one- liner that calls the
     * internal version of the function, which means the compiler should compress out the function
     * call). It's that way because the interface is still fluid, I'm not convinced there won't be a
     * need for a functional split between the internal and external versions in the future.
     *
     * Callers of this function are expected to have already acquired any locks required to
     * manipulate the extent list.
     */
    return (__block_merge(session, block, el, off, size));
}

/*
 * __block_merge --
 *     Insert an extent into an extent list, merging if possible (internal version).
 */
//把需要merge的ext[off, off+size]合并到el跳跃表，这个ext不能和el中对应的跳跃表不能有交集
static int
__block_merge(
  WT_SESSION_IMPL *session, WT_BLOCK *block, WT_EXTLIST *el, wt_off_t off, wt_off_t size)
{
    WT_EXT *ext, *after, *before;

    /*
     * Retrieve the records preceding/following the offset. If the records are contiguous with the
     * free'd offset, combine records.
     */
    //判断off位置是否在el对应ext跳跃表中
    __block_off_srch_pair(el, off, &before, &after);
    if (before != NULL) {
        //off和befor[before->off, before->off + before->size]不能有交集
        if (before->off + before->size > off)
            WT_BLOCK_RET(session, block, EINVAL,
              "%s: existing range %" PRIdMAX "-%" PRIdMAX " overlaps with merge range %" PRIdMAX
              "-%" PRIdMAX,
              el->name, (intmax_t)before->off, (intmax_t)(before->off + before->size),
              (intmax_t)off, (intmax_t)(off + size));

        //before != NULL代表befor与[off, off+size]可以直接拼接在一起
        if (before->off + before->size != off)
            //before = NULL代表befor与[off, off+size]不能直接拼接，没有交集，例如before=[100, 200], off:size=[300,400]
            before = NULL;
    }
    if (after != NULL) {
        if (off + size > after->off) {
            WT_BLOCK_RET(session, block, EINVAL,
              "%s: merge range %" PRIdMAX "-%" PRIdMAX " overlaps with existing range %" PRIdMAX
              "-%" PRIdMAX,
              el->name, (intmax_t)off, (intmax_t)(off + size), (intmax_t)after->off,
              (intmax_t)(after->off + after->size));
        }

        //after = NULL说明after与[off,off+size]不能直接拼接，可能after包含[off,off+size]，例如after=[100, 200], off:size=[100,150]
        if (off + size != after->off)
            after = NULL;
    }
    //也就是off位置不在el跳跃表中，直接从新alloc一个ext添加到el跳跃表中
    if (before == NULL && after == NULL) {
        __wt_verbose_debug2(session, WT_VERB_BLOCK, "%s: insert range %" PRIdMAX "-%" PRIdMAX,
          el->name, (intmax_t)off, (intmax_t)(off + size));

        return (__block_off_insert(session, el, off, size));
    }

    /*
     * If the "before" offset range abuts, we'll use it as our new record; if the "after" offset
     * range also abuts, include its size and remove it from the system. Else, only the "after"
     * offset range abuts, use the "after" offset range as our new record. In either case, remove
     * the record we're going to use, adjust it and re-insert it.
     */
    //下面流程说明可以和befor直接拼接在一起或者和after直接拼接到一起

    //例如off:size=[200,400],after=[400, 500], 合并到ext中[400,500]
    if (before == NULL) {//也就是after!=NULL, ext可以和after直接拼接到一起
        WT_RET(__block_off_remove(session, block, el, after->off, &ext));

        __wt_verbose_debug2(session, WT_VERB_BLOCK,
          "%s: range grows from %" PRIdMAX "-%" PRIdMAX ", to %" PRIdMAX "-%" PRIdMAX, el->name,
          (intmax_t)ext->off, (intmax_t)(ext->off + ext->size), (intmax_t)off,
          (intmax_t)(off + ext->size + size));
        ext->off = off;
        ext->size += size;
    } else {
        //ext[off, off+size]和跳跃表中的ext可以直接拼接
        //例如after=[300,400], off:size=[200,300],合并到ext中[200,400]
        if (after != NULL) {//也就是before != NULL && after != NULL, 也就是befor, ext, after三个可以直接合并到一起
            size += after->size;
            WT_RET(__block_off_remove(session, block, el, after->off, NULL));
        } //如果before != NULL && after = NULL，也就是befor, ext两个可以直接合并到一起

        WT_RET(__block_off_remove(session, block, el, before->off, &ext));

        __wt_verbose_debug2(session, WT_VERB_BLOCK,
          "%s: range grows from %" PRIdMAX "-%" PRIdMAX ", to %" PRIdMAX "-%" PRIdMAX, el->name,
          (intmax_t)ext->off, (intmax_t)(ext->off + ext->size), (intmax_t)ext->off,
          (intmax_t)(ext->off + ext->size + size));

        ext->size += size;
    }
    //ext按照ext->size大小添加到el->sz跳表，按照ext->off添加到el->off跳表
    return (__block_ext_insert(session, el, ext));
}

/*
 * __wt_block_extlist_read_avail --
 *     Read an avail extent list, includes minor special handling.

 //__wt_block_checkpoint->__ckpt_process进行checkpoint相关元数据持久化
 //__wt_meta_checkpoint获取checkpoint信息，然后__wt_block_checkpoint_load加载checkpoint相关元数据
 //__btree_preload->__wt_blkcache_read循环进行真正的数据加载

 //__wt_btree_open->__wt_block_checkpoint_load->__wt_block_extlist_read_avail

 __wt_block_extlist_read与__wt_block_extlist_write对应
 //加载ext跳表元数据到内存中
 */
int
__wt_block_extlist_read_avail(
  WT_SESSION_IMPL *session, WT_BLOCK *block, WT_EXTLIST *el, wt_off_t ckpt_size)
{
    WT_DECL_RET;

    /* If there isn't a list, we're done. */
    //重启的时候通过checkpoint元数据在外层从__wt_block_ckpt_unpack加载起来对应跳表元数据获取到offset
    if (el->offset == WT_BLOCK_INVALID_OFFSET)
        return (0);

#ifdef HAVE_DIAGNOSTIC
    /*
     * In diagnostic mode, reads are checked against the available and discard lists (a block being
     * read should never appear on either). Checkpoint threads may be running in the file, don't
     * race with them.
     */
    __wt_spin_lock(session, &block->live_lock);
#endif

    WT_ERR(__wt_block_extlist_read(session, block, el, ckpt_size));

    /*
     * Extent blocks are allocated from the available list: if reading the avail list, the extent
     * blocks might be included, remove them.
     */
    WT_ERR_NOTFOUND_OK(
      __wt_block_off_remove_overlap(session, block, el, el->offset, el->size), false);

err:
#ifdef HAVE_DIAGNOSTIC
    __wt_spin_unlock(session, &block->live_lock);
#endif

    return (ret);
}

/*
 * __wt_block_extlist_read --
 *     Read an extent list.
 yang add todo xxxxxxxxxxxx, 如果文件有很多ext空洞，也就是有很多avail，这里会不会很慢??????????????????????//
 //从磁盘加载ext到内存中
 */
int
__wt_block_extlist_read(
  WT_SESSION_IMPL *session, WT_BLOCK *block, WT_EXTLIST *el, wt_off_t ckpt_size)
{
    WT_DECL_ITEM(tmp);
    WT_DECL_RET;
    wt_off_t off, size;
    const uint8_t *p;
    int (*func)(WT_SESSION_IMPL *, WT_BLOCK *, WT_EXTLIST *, wt_off_t, wt_off_t);

    off = size = 0;

    /* If there isn't a list, we're done. */
    if (el->offset == WT_BLOCK_INVALID_OFFSET)
        return (0);

    WT_RET(__wt_scr_alloc(session, el->size, &tmp));
    // 读取磁盘上面的avail或者alloc跳跃表中的ext元数据到内存中
    WT_ERR(
      __wt_block_read_off(session, block, tmp, el->objectid, el->offset, el->size, el->checksum));

    p = WT_BLOCK_HEADER_BYTE(tmp->mem);
    WT_ERR(__wt_extlist_read_pair(&p, &off, &size));
    //if (off != WT_BLOCK_EXTLIST_MAGIC || size != 0)  这里可以加上tmp变量，最终对el->byte做检查
    if (off != WT_BLOCK_EXTLIST_MAGIC)
        goto corrupted;

    /*
     * If we're not creating both offset and size skiplists, use the simpler append API, otherwise
     * do a full merge. There are two reasons for the test: first, checkpoint "available" lists are
     * NOT sorted (checkpoints write two separate lists, both of which are sorted but they're not
     * merged). Second, the "available" list is sorted by size as well as by offset, and the
     * fast-path append code doesn't support that, it's limited to offset. The test of "track size"
     * is short-hand for "are we reading the available-blocks list".
     */
    func = el->track_size == 0 ? __block_append : __block_merge;
    //把从磁盘上面加载的ext元数据信息，加载所有ext到内存中
    for (;;) {
        WT_ERR(__wt_extlist_read_pair(&p, &off, &size));
        if (off == WT_BLOCK_INVALID_OFFSET)
            break;

        /*
         * We check the offset/size pairs represent valid file ranges, then insert them into the
         * list. We don't necessarily have to check for offsets past the end of the checkpoint, but
         * it's a cheap test to do here and we'd have to do the check as part of file verification,
         * regardless.
         */
        if (off < block->allocsize || off % block->allocsize != 0 || size % block->allocsize != 0 ||
          off + size > ckpt_size) {
corrupted:
            __wt_scr_free(session, &tmp);
            WT_BLOCK_RET(session, block, WT_ERROR,
              "file contains a corrupted %s extent list, range %" PRIdMAX "-%" PRIdMAX
              " past end-of-file",
              el->name, (intmax_t)off, (intmax_t)(off + size));
        }

        //执行 __block_append或者__block_merge;
        WT_ERR(func(session, block, el, off, size));
    }

    WT_ERR(__block_extlist_dump(session, block, el, "extlist read"));

err:
    __wt_scr_free(session, &tmp);
    return (ret);
}

/*
 * __wt_block_extlist_write --
 *     Write an extent list at the tail of the file.
  __wt_block_extlist_read与__wt_block_extlist_write对应
 */
//把el跳跃表管理的所有ext持久化到磁盘中，这样就可以直接从磁盘加载还原内存中的有序ext跳跃表
int
__wt_block_extlist_write(
  WT_SESSION_IMPL *session, WT_BLOCK *block, WT_EXTLIST *el, WT_EXTLIST *additional)
{
    WT_DECL_ITEM(tmp);
    WT_DECL_RET;
    WT_EXT *ext;
    WT_PAGE_HEADER *dsk;
    size_t size;
    uint32_t entries;
    uint8_t *p;

    //计算el跳表中所有ext的大小分布, 并打印
    WT_RET(__block_extlist_dump(session, block, el, "write"));

    /*
     * Figure out how many entries we're writing -- if there aren't any entries, there's nothing to
     * write, unless we still have to write the extent list to include the checkpoint recovery
     * information.
     */
    entries = el->entries + (additional == NULL ? 0 : additional->entries);
    if (entries == 0 && block->final_ckpt == NULL) {
        el->offset = WT_BLOCK_INVALID_OFFSET;
        el->checksum = el->size = 0;
        return (0);
    }

    /*
     * Get a scratch buffer, clear the page's header and data, initialize the header.
     *
     * Allocate memory for the extent list entries plus two additional entries: the initial
     * WT_BLOCK_EXTLIST_MAGIC/0 pair and the list- terminating WT_BLOCK_INVALID_OFFSET/0 pair.
     */
    size = ((size_t)entries + 2) * 2 * WT_INTPACK64_MAXSIZE;
    //size按照block->allocsize对齐
    WT_RET(__wt_block_write_size(session, block, &size));
    //分片内存空间
    WT_RET(__wt_scr_alloc(session, size, &tmp));
    dsk = tmp->mem;
    memset(dsk, 0, WT_BLOCK_HEADER_BYTE_SIZE);
    dsk->type = WT_PAGE_BLOCK_MANAGER;
    dsk->version = WT_PAGE_VERSION_TS;

    /* Fill the page's data. */
    //指向WT_BLOCK_HEADER_BYTE_SIZE字节处
    p = WT_BLOCK_HEADER_BYTE(dsk);
    /* Extent list starts */
    //写magic到头部后面
    //yang add todo xxxxxxxxx 这里带上byte，也就是整个ext list对应真实数据长度会不会更好? 这样可以计算大数据节点数据加载进度
    //WT_ERR(__wt_extlist_write_pair(&p, WT_BLOCK_EXTLIST_MAGIC, (wt_off_t)el->bytes));
    WT_ERR(__wt_extlist_write_pair(&p, WT_BLOCK_EXTLIST_MAGIC, 0));
    //所有ext，包括root、internal page、leaf page对应的ext:[off, off+size]写入tmp mem内存
    WT_EXT_FOREACH (ext, el->off) /* Free ranges */
        WT_ERR(__wt_extlist_write_pair(&p, ext->off, ext->size));
    if (additional != NULL)
        WT_EXT_FOREACH (ext, additional->off) /* Free ranges */
            WT_ERR(__wt_extlist_write_pair(&p, ext->off, ext->size));
    /* Extent list stops */
    //checkpoint结束标识追加到mem末尾
    WT_ERR(__wt_extlist_write_pair(
      &p, WT_BLOCK_INVALID_OFFSET, block->final_ckpt == NULL ? 0 : WT_BLOCK_EXTLIST_VERSION_CKPT));

    dsk->u.datalen = WT_PTRDIFF32(p, WT_BLOCK_HEADER_BYTE(dsk));
    tmp->size = dsk->mem_size = WT_PTRDIFF32(p, dsk);

#ifdef HAVE_DIAGNOSTIC
    /*
     * The extent list is written as a valid btree page because the salvage functionality might move
     * into the btree layer some day, besides, we don't need another format and this way the page
     * format can be easily verified.
     */
    WT_ERR(__wt_verify_dsk(session, "[extent list check]", tmp));
#endif

    /* Write the extent list to disk. */
    //上面组织的内存tmp mem内存数据写入磁盘，并返回对应磁盘的元数据信息objectidp, offsetp, sizep和checksump
    //从avail或者alloc中获取一个ext来存储checkpoint相关的元数据信息
    //不用保存到alloc中, 因此下面的__wt_block_off_remove_overlap需要删除该ext
    WT_ERR(__wt_block_write_off(
      session, block, tmp, &el->objectid, &el->offset, &el->size, &el->checksum, true, true, true));

    /*
     * Remove the allocated blocks from the system's allocation list, extent blocks never appear on
     * any allocation list.
     */

    //checkpoint对应的磁盘ext元数据不用记录到内存中，这个持久化到磁盘就可以了
    WT_TRET(
      __wt_block_off_remove_overlap(session, block, &block->live.alloc, el->offset, el->size));

    //yang add todo xxxxxxxx  日志完善
    __wt_verbose(session, WT_VERB_BLOCK, "%s written extent list %" PRIdMAX "/%" PRIu32, el->name,
      (intmax_t)el->offset, el->size);

err:
    __wt_scr_free(session, &tmp);
    return (ret);
}

/*
 * __wt_block_extlist_truncate --
 *     Truncate the file based on the last available extent in the list.
 */
//也就是判断avail的最后一个ext不为NULL，并且最后一个ext就是文件的末尾，说明文件末尾的ext可以truncate
//做checkpoint的时候会走这里判断是否需要truncate截断文件大小，一般大量删除数据的情况下会存在需要截断文件的这种情况
int
__wt_block_extlist_truncate(WT_SESSION_IMPL *session, WT_BLOCK *block, WT_EXTLIST *el)
{
    WT_EXT *ext, **astack[WT_SKIP_MAXDEPTH];
    wt_off_t size;

    /*
     * Check if the last available extent is at the end of the file, and if so, truncate the file
     * and discard the extent.
     */
    if ((ext = __block_off_srch_last(el, astack, false)) == NULL)
        return (0);

    WT_ASSERT(session, ext->off + ext->size <= block->size);
    if (ext->off + ext->size < block->size)
        return (0);

    //走到这里说明是最后的一个ext, 因为ext->off + ext->size = block->size

    /*
     * Remove the extent list entry. (Save the value, we need it to reset the cached file size, and
     * that can't happen until after the extent list removal succeeds.)
     */
    size = ext->off;
    WT_RET(__block_off_remove(session, block, el, size, NULL));

    /* Truncate the file. */
    return (__wt_block_truncate(session, block, size));
}

/*
 * __wt_block_extlist_init --
 *     Initialize an extent list.
 */
int
__wt_block_extlist_init(
  WT_SESSION_IMPL *session, WT_EXTLIST *el, const char *name, const char *extname, bool track_size)
{
    size_t size;

    WT_CLEAR(*el);

    size =
      (name == NULL ? 0 : strlen(name)) + strlen(".") + (extname == NULL ? 0 : strlen(extname) + 1);
    WT_RET(__wt_calloc_def(session, size, &el->name));
    WT_RET(__wt_snprintf(
      el->name, size, "%s.%s", name == NULL ? "" : name, extname == NULL ? "" : extname));

    el->offset = WT_BLOCK_INVALID_OFFSET;
    el->track_size = track_size;
    return (0);
}

/*
 * __wt_block_extlist_free --
 *     Discard an extent list.
 */
void
__wt_block_extlist_free(WT_SESSION_IMPL *session, WT_EXTLIST *el)
{
    WT_EXT *ext, *next;
    WT_SIZE *szp, *nszp;

    __wt_free(session, el->name);

    for (ext = el->off[0]; ext != NULL; ext = next) {
        next = ext->next[0];
        __wt_free(session, ext);
    }
    for (szp = el->sz[0]; szp != NULL; szp = nszp) {
        nszp = szp->next[0];
        __wt_free(session, szp);
    }

    /* Extent lists are re-used, clear them. */
    WT_CLEAR(*el);
}

/*
 * __block_extlist_dump --
 *     Dump an extent list as verbose messages.
 计算el跳表中所有ext的大小分布
 */
static int
__block_extlist_dump(WT_SESSION_IMPL *session, WT_BLOCK *block, WT_EXTLIST *el, const char *tag)
{
    WT_DECL_ITEM(t1);
    WT_DECL_ITEM(t2);
    WT_DECL_RET;
    WT_EXT *ext;
    WT_VERBOSE_LEVEL level;
    uint64_t pow, sizes[64];
    u_int i;
    const char *sep;

    if (!block->verify_layout &&
      !WT_VERBOSE_LEVEL_ISSET(session, WT_VERB_BLOCK, WT_VERBOSE_DEBUG_2))
        return (0);

    WT_ERR(__wt_scr_alloc(session, 0, &t1));
    if (block->verify_layout)
        level = WT_VERBOSE_NOTICE;
    else
        level = WT_VERBOSE_DEBUG_2;
    //例如write extent list live.alloc, 2 entries, 2MB (2326528) bytes
    __wt_verbose_level(session, WT_VERB_BLOCK, level,
      "%s extent list %s, %" PRIu32 " entries, %s bytes", tag, el->name, el->entries,
      __wt_buf_set_size(session, el->bytes, true, t1));

    if (el->entries == 0)
        goto done;

    memset(sizes, 0, sizeof(sizes));
    //计算每个ext对应的大小范围断
    WT_EXT_FOREACH (ext, el->off)
        //2的9次方=512
        for (i = 9, pow = 512;; ++i, pow *= 2)
            if (ext->size <= (wt_off_t)pow) {
                ++sizes[i];
                break;
            }
    //extents by bucket: {1MB: 1}, {2MB: 1}
    sep = "extents by bucket:";
    t1->size = 0;
    WT_ERR(__wt_scr_alloc(session, 0, &t2));
    for (i = 9, pow = 512; i < WT_ELEMENTS(sizes); ++i, pow *= 2)
        if (sizes[i] != 0) {
            WT_ERR(__wt_buf_catfmt(session, t1, "%s {%s: %" PRIu64 "}", sep,
              __wt_buf_set_size(session, pow, false, t2), sizes[i]));
            sep = ",";
        }

    __wt_verbose_level(session, WT_VERB_BLOCK, level, "__block_extlist_dump %s", (char *)t1->data);

done:
err:
    __wt_scr_free(session, &t1);
    __wt_scr_free(session, &t2);
    return (ret);
}

#ifdef HAVE_UNITTEST
/*int
__ut_block_off_insert(WT_SESSION_IMPL *session, WT_EXTLIST *el, wt_off_t off, wt_off_t size)
{
    return (__block_off_insert(session, el, off, size));
}

int
__ut_block_off_remove(
  WT_SESSION_IMPL *session, WT_BLOCK *block, WT_EXTLIST *el, wt_off_t off, WT_EXT **extp)
{
    return (__block_off_remove(session, block, el, off, extp));
}*/

WT_EXT *
__ut_block_off_srch_last(WT_EXTLIST *el, WT_EXT ***stack, bool need_traverse)
{
    return (__block_off_srch_last(el, stack, need_traverse));
}

void
__ut_block_off_srch(WT_EXT **head, wt_off_t off, WT_EXT ***stack, bool skip_off)
{
    __block_off_srch(head, off, stack, skip_off, NULL);
}

bool
__ut_block_first_srch(WT_EXT **head, wt_off_t size, WT_EXT ***stack)
{
    return (__block_first_srch(head, size, stack));
}

void
__ut_block_size_srch(WT_SIZE **head, wt_off_t size, WT_SIZE ***stack)
{
    __block_size_srch(head, size, stack);
}

#endif
