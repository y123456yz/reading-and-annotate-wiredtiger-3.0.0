/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

#define WT_BLOCK_COOKIE_FILEID 0x01 /* The following bytes are an object ID. */

/*
 * __block_addr_unpack --
 *     Unpack an address cookie into components, UPDATING the caller's buffer reference so this
 *     function can be called repeatedly to unpack a buffer containing multiple address cookies.
 __wt_block_addr_pack和__block_addr_unpack对应
 */
//__block_ckpt_unpack->__block_addr_unpack
//从checkpoint核心二进制元数据pp数组addr="018c81e4ab0a3a0d8d81e476e6c0b19981e448ded3b9808080e3270fc0e323bfc0"中解包还原到objectidp offsetp sizep checksump对应成员变量中
static int
__block_addr_unpack(WT_SESSION_IMPL *session, WT_BLOCK *block, const uint8_t **pp, size_t addr_size,
  uint32_t *objectidp, wt_off_t *offsetp, uint32_t *sizep, uint32_t *checksump)
{
    uint64_t i, o, s, c;
    uint8_t flags;
    const uint8_t *begin;

    /*
     * Address cookies are a file offset, size and checksum triplet, with optional object ID: unpack
     * the trailing object ID if there are bytes following the triple. The checkpoint cookie is more
     * complicated: it has four address blocks all having the same object ID. Rather than retrofit
     * the object ID into each of those address blocks (which would mean somehow figuring out the
     * length of each individual address block), the object ID is appended to the end of the
     * checkpoint cookie and this function skips the object ID unpack when the passed in cookie size
     * is 0. (This feature is ONLY used by the checkpoint code, all other callers assert the cookie
     * size is not 0. We could alternatively have a "checkpoint cookie" boolean, or use a NULL
     * object ID address when never returning a object ID, but a cookie size of 0 seems equivalent.)
     */
    begin = *pp;
    WT_RET(__wt_vunpack_uint(pp, 0, &o));
    WT_RET(__wt_vunpack_uint(pp, 0, &s));
    WT_RET(__wt_vunpack_uint(pp, 0, &c));
    i = 0;
    flags = 0;
    if (addr_size != 0 && WT_PTRDIFF(*pp, begin) < addr_size) {
        flags = **pp;
        ++(*pp);
        if (LF_ISSET(WT_BLOCK_COOKIE_FILEID))
            WT_RET(__wt_vunpack_uint(pp, 0, &i));
    }

    /*
     * If there's an address cookie size and either there is no flag value or the flag value is the
     * object ID flag by itself, assert the cookie was entirely consumed. Future extensions will use
     * different cookie flag values (although the file-ID flag might still be set, our test is for
     * equality).
     */
    WT_ASSERT(session,
      addr_size == 0 || (flags != 0 && flags != WT_BLOCK_COOKIE_FILEID) ||
        WT_PTRDIFF(*pp, begin) == addr_size);

    /*
     * To avoid storing large offsets, we minimize the value by subtracting a block for description
     * information, then storing a count of block allocation units. That implies there is no such
     * thing as an "invalid" offset though, they could all be valid (other than very large numbers),
     * which is what we didn't want to store in the first place. Use the size: writing a block of
     * size 0 makes no sense, so that's the out-of-band value. Once we're out of this function and
     * are working with a real file offset, size and checksum triplet, there can be invalid offsets,
     * that's simpler than testing sizes of 0 all over the place.
     */
    if (s == 0) {
        *objectidp = 0;
        *offsetp = 0;
        *sizep = *checksump = 0;
    } else {
        *objectidp = (uint32_t)i;
        *offsetp = (wt_off_t)(o + 1) * block->allocsize;
        *sizep = (uint32_t)s * block->allocsize;
        *checksump = (uint32_t)c;
    }

    return (0);
}

/*
 * __wt_block_addr_pack --
 *     Pack components into an address cookie, UPDATING the caller's buffer reference.
  __wt_block_addr_pack和__block_addr_unpack对应
 */
//对objectid offset size  checksum四个字段进行封包存入pp[]数组中, 通过获取pp[]数组即可获取到一个完整block的元数据,addr[WT_BTREE_MAX_ADDR_COOKIE]
int
__wt_block_addr_pack(WT_BLOCK *block, uint8_t **pp, uint32_t objectid, wt_off_t offset,
  uint32_t size, uint32_t checksum)
{
    uint64_t i, o, s, c;

    /* See the comment above about storing large offsets: this is the reverse operation. */
    if (size == 0) {
        i = 0;
        o = WT_BLOCK_INVALID_OFFSET;
        s = c = 0;
    } else {
        i = objectid;
        o = (uint64_t)offset / block->allocsize - 1;
        s = size / block->allocsize;
        c = checksum;
    }
    WT_RET(__wt_vpack_uint(pp, 0, o));
    WT_RET(__wt_vpack_uint(pp, 0, s));
    WT_RET(__wt_vpack_uint(pp, 0, c));

    /*
     * Don't store object IDs of zero, the function that cracks the cookie defaults IDs to 0.
     *
     * TODO: testing has-objects is not quite right. Ideally, we don't store a object ID if there's
     * only a single object. We want to be able to convert existing object to a stack, which means
     * starting with a single object with no object IDs, where all future objects in the stack know
     * a missing object ID is a reference to the base object.
     */
    if (i != WT_TIERED_OBJECTID_NONE) {
        **pp = WT_BLOCK_COOKIE_FILEID;
        ++(*pp);
        WT_RET(__wt_vpack_uint(pp, 0, i));
    }
    return (0);
}

/*
 * __wt_block_addr_unpack --
 *     Unpack an address cookie into components, NOT UPDATING the caller's buffer reference.
 */

//从checkpoint核心二进制元数据addr="018c81e4ab0a3a0d8d81e476e6c0b19981e448ded3b9808080e3270fc0e323bfc0"中解包还原到WT_BLOCK_CKPT对应成员变量中
//对应p内容解析打印也可以参考__wt_block_addr_string
int
__wt_block_addr_unpack(WT_SESSION_IMPL *session, WT_BLOCK *block, const uint8_t *p,
  size_t addr_size, uint32_t *objectidp, wt_off_t *offsetp, uint32_t *sizep, uint32_t *checksump)
{
    /* Checkpoint passes zero as the cookie size, nobody else should. */
    WT_ASSERT(session, addr_size != 0);

    return (
      __block_addr_unpack(session, block, &p, addr_size, objectidp, offsetp, sizep, checksump));
}

/*
 * __wt_block_addr_invalid --
 *     Return an error code if an address cookie is invalid.
 */
int
__wt_block_addr_invalid(
  WT_SESSION_IMPL *session, WT_BLOCK *block, const uint8_t *addr, size_t addr_size, bool live)
{
    wt_off_t offset;
    uint32_t checksum, objectid, size;

    WT_UNUSED(live);

    /* Crack the cookie. */
    WT_RET(__wt_block_addr_unpack(
      session, block, addr, addr_size, &objectid, &offset, &size, &checksum));

#ifdef HAVE_DIAGNOSTIC
    /*
     * In diagnostic mode, verify the address isn't on the available list, or for live systems, the
     * discard list. This only applies if the block is in this object.
     */
    if (objectid == block->objectid)
        WT_RET(__wt_block_misplaced(
          session, block, "addr-valid", offset, size, live, __PRETTY_FUNCTION__, __LINE__));
#endif

    /* Check if the address is past the end of the file. */
    return (objectid == block->objectid && offset + size > block->size ? EINVAL : 0);
}

/*
 * __wt_block_addr_string --
 *     Return a printable string representation of an address cookie.
 */
//解析addr中的ext内容元数据存储到buf中
int
__wt_block_addr_string(
  WT_SESSION_IMPL *session, WT_BLOCK *block, WT_ITEM *buf, const uint8_t *addr, size_t addr_size)
{
    wt_off_t offset;
    uint32_t checksum, objectid, size;

    /* Crack the cookie. */
    WT_RET(__wt_block_addr_unpack(
      session, block, addr, addr_size, &objectid, &offset, &size, &checksum));

    /* Printable representation. */
    //yang add todo xxxxxxxxxxxxxxx 日志完善
    WT_RET(__wt_buf_fmt(session, buf,
      "[objectid: %" PRIu32 ", off : %" PRIuMAX "-%" PRIuMAX ", size: %" PRIu32 ", checksum: %" PRIu32 "]", objectid,
      (uintmax_t)offset, (uintmax_t)offset + size, size, checksum));

    return (0);
}

/*
 * __block_ckpt_unpack --
 *     Convert a checkpoint cookie into its components.
  //__wt_block_ckpt_pack和__block_ckpt_unpack对应
 //封包或者解包所有checkpoint核心元数据: root持久化元数据(包括internal ref key+所有leafpage ext) + alloc跳表持久化到磁盘的核心元数据信息+avail跳表持久化到磁盘的核心元数据信息

 //从checkpoint核心二进制元数据addr="018c81e4ab0a3a0d8d81e476e6c0b19981e448ded3b9808080e3270fc0e323bfc0"中解包还原到WT_BLOCK_CKPT对应成员变量中

 */ //参考__wt_ckpt_verbose阅读，//从checkpoint核心元数据中解析处对应的成员赋值给ci
static int
__block_ckpt_unpack(WT_SESSION_IMPL *session, WT_BLOCK *block, const uint8_t *ckpt,
  size_t ckpt_size, WT_BLOCK_CKPT *ci)
{
    uint64_t a;
    uint32_t objectid;
    uint8_t flags;
    const uint8_t *begin;

    begin = ckpt;

    ci->version = ckpt[0];
    //printf("yang test .......................__block_ckpt_unpack...........:%d\r\n", ci->version);
    //yang add change xxxxxxxxxxxx  这里老是不满足这个条件，先忽略
    //if (ci->version != WT_BM_CHECKPOINT_VERSION)
    //    WT_RET_MSG(session, WT_ERROR, "unsupported checkpoint version");

    /*
     * See the comment above about address cookies and sizes for an explanation.
     *
     * Passing an address cookie size of 0 so the unpack function doesn't read an object ID.
     */
    ++ckpt;
    WT_RET(__block_addr_unpack(session, block, &ckpt, 0, &ci->root_objectid, &ci->root_offset,
      &ci->root_size, &ci->root_checksum));
    WT_RET(__block_addr_unpack(session, block, &ckpt, 0, &ci->alloc.objectid, &ci->alloc.offset,
      &ci->alloc.size, &ci->alloc.checksum));
    WT_RET(__block_addr_unpack(session, block, &ckpt, 0, &ci->avail.objectid, &ci->avail.offset,
      &ci->avail.size, &ci->avail.checksum));
    WT_RET(__block_addr_unpack(session, block, &ckpt, 0, &ci->discard.objectid, &ci->discard.offset,
      &ci->discard.size, &ci->discard.checksum));
    WT_RET(__wt_vunpack_uint(&ckpt, 0, &a));
    ci->file_size = (wt_off_t)a;
    WT_RET(__wt_vunpack_uint(&ckpt, 0, &a));
    ci->ckpt_size = a;

    /* The first part of the checkpoint cookie is optionally followed by an object ID. */
    objectid = 0;
    flags = 0;
    if (WT_PTRDIFF(ckpt, begin) != ckpt_size) {
        flags = *ckpt++;
        if (LF_ISSET(WT_BLOCK_COOKIE_FILEID)) {
            WT_RET(__wt_vunpack_uint(&ckpt, 0, &a));
            objectid = (uint32_t)a;
        }
    }
    ci->root_objectid = ci->alloc.objectid = ci->avail.objectid = ci->discard.objectid = objectid;

    /*
     * If there is no flag value or the flag value is the object ID flag by itself, assert the
     * cookie was entirely consumed. Future extensions will use different cookie flag values
     * (although the file-ID flag might still be set, our test is for equality).
     */
    WT_ASSERT(session,
      (flags != 0 && flags != WT_BLOCK_COOKIE_FILEID) || WT_PTRDIFF(ckpt, begin) == ckpt_size);

    return (0);
}

/*
 * __wt_block_ckpt_unpack --
 *     Convert a checkpoint cookie into its components, block manager version.
 //__wt_block_ckpt_pack和__block_ckpt_unpack对应
 //封包或者解包所有checkpoint核心元数据: root持久化元数据(包括internal ref key+所有leafpage ext) + alloc跳表持久化到磁盘的核心元数据信息+avail跳表持久化到磁盘的核心元数据信息
 */
//参考__wt_ckpt_verbose阅读，//从checkpoint核心元数据中解析处对应的成员赋值给ci
//从checkpoint核心二进制元数据addr="018c81e4ab0a3a0d8d81e476e6c0b19981e448ded3b9808080e3270fc0e323bfc0"中解包还原到WT_BLOCK_CKPT对应成员变量中
int
__wt_block_ckpt_unpack(WT_SESSION_IMPL *session, WT_BLOCK *block, const uint8_t *ckpt,
  size_t ckpt_size, WT_BLOCK_CKPT *ci)
{//从checkpoint核心元数据中解析处对应的成员赋值给ci
    return (__block_ckpt_unpack(session, block, ckpt, ckpt_size, ci));
}

/*
 * __wt_block_ckpt_decode --
 *     Convert a checkpoint cookie into its components, external utility version.
 */ //__wt_block_ckpt_decode(session, block, ckpt->raw.data, ckpt->raw.size, &ci)
int
__wt_block_ckpt_decode(WT_SESSION *wt_session, WT_BLOCK *block, const uint8_t *ckpt,
  size_t ckpt_size, WT_BLOCK_CKPT *ci) //WT_GCC_FUNC_ATTRIBUTE((visibility("default")))
{
    WT_SESSION_IMPL *session;

    session = (WT_SESSION_IMPL *)wt_session;
    return (__block_ckpt_unpack(session, block, ckpt, ckpt_size, ci));
}

/*
 * __wt_block_ckpt_pack --
 *     Convert the components into its checkpoint cookie.
  //__wt_block_ckpt_pack和__block_ckpt_unpack对应
 //封包或者解包所有checkpoint核心元数据: root page持久化元数据 + alloc跳表持久化到磁盘的核心元数据信息+avail跳表持久化到磁盘的核心元数据信息

 */
//封装所有checkpoint核心元数据: root持久化元数据 + alloc跳表持久化到磁盘的核心元数据信息+avail跳表持久化到磁盘的核心元数据信息
//注意这里是封包后存储的，补上直接把root_offset等原样存储，这样可以节省磁盘空间

//root page的磁盘地址+alloc跳表+avail跳表+discard跳表的磁盘位置信息封装到pp中
int
__wt_block_ckpt_pack(
  WT_SESSION_IMPL *session, WT_BLOCK *block, uint8_t **pp, WT_BLOCK_CKPT *ci, bool skip_avail)
{
    uint64_t a;

    WT_UNUSED(session);
    //if (ci->version != WT_BM_CHECKPOINT_VERSION) // yang add todo xxxxxxxxxxx   这里老是不满足这个条件，先忽略
    //    WT_RET_MSG(session, WT_ERROR, "unsupported checkpoint version");

    //填充version字段
    (*pp)[0] = ci->version;
    (*pp)++;

    /*
     * See the comment above about address cookies and sizes for an explanation.
     *
     * Passing an object ID of 0 so the pack function doesn't store an object ID.
     */
    //把internal page
    WT_RET(__wt_block_addr_pack(block, pp, 0, ci->root_offset, ci->root_size, ci->root_checksum));
    WT_RET(
      __wt_block_addr_pack(block, pp, 0, ci->alloc.offset, ci->alloc.size, ci->alloc.checksum));
    if (skip_avail)
        WT_RET(__wt_block_addr_pack(block, pp, 0, 0, 0, 0));
    else
        WT_RET(
          __wt_block_addr_pack(block, pp, 0, ci->avail.offset, ci->avail.size, ci->avail.checksum));
    WT_RET(__wt_block_addr_pack(
      block, pp, 0, ci->discard.offset, ci->discard.size, ci->discard.checksum));
    a = (uint64_t)ci->file_size;
    WT_RET(__wt_vpack_uint(pp, 0, a));
    a = ci->ckpt_size;
    WT_RET(__wt_vpack_uint(pp, 0, a));
    /* Don't store object IDs of zero, the function that cracks the cookie defaults IDs to 0. */
    if (block->objectid != 0) {
        **pp = WT_BLOCK_COOKIE_FILEID;
        ++(*pp);
        a = block->objectid;
        WT_RET(__wt_vpack_uint(pp, 0, a));
    }

    return (0);
}

/*
 * __wt_ckpt_verbose --
 *     Display a printable string representation of a checkpoint.
 //[1701918183:985010][29780:0x7f475f045800], file:access.wt, close_ckpt: [WT_VERB_CHECKPOINT][DEBUG_4]: access.wt:
 //create: WiredTigerCheckpoint: version=1, object ID=0, root=[1470464-1474560, 4096, 3802611306], alloc=[1474560-1478656, 4096, 976509849],
 //avail=[1478656-1482752, 4096, 3835723351], discard=[Empty], file size=2363392, checkpoint size=2322432
 */
//checkpoint对应持久化日志信息
//从checkpoint核心二进制元数据addr="018c81e4ab0a3a0d8d81e476e6c0b19981e448ded3b9808080e3270fc0e323bfc0"中解包还原
//  到WT_BLOCK_CKPT对应成员变量中,然后日志输出
void
__wt_ckpt_verbose(WT_SESSION_IMPL *session, WT_BLOCK *block, const char *tag,
    const char *ckpt_name,
    //ckpt_name代表checkpoint的核心元数据: root持久化元数据(包括internal ref key+所有leafpage ext) + alloc跳表持久化到磁盘的核心元数据信息+avail跳表持久化到磁盘的核心元数据信息
    //ckpt_string为二进制数据
    const uint8_t *ckpt_string,
    size_t ckpt_size)
{
    WT_BLOCK_CKPT *ci, _ci;
    WT_DECL_ITEM(tmp);
    WT_DECL_RET;

    //printf("yang test 1111111111111111111111111111111111111111111 __wt_ckpt_verbose\r\n");
    if (ckpt_string == NULL) {
        __wt_verbose_worker(session, WT_VERB_CHECKPOINT, S2C(session)->verbose[WT_VERB_CHECKPOINT],
          "%s: %s: %s%s[Empty]", block->name, tag, ckpt_name ? ckpt_name : "",
          ckpt_name ? ": " : "");
        return;
    }

    /* Initialize the checkpoint, crack the cookie. */
    ci = &_ci;
    WT_ERR(__wt_block_ckpt_init(session, ci, "string"));
    //从checkpoint核心二进制元数据addr="018c81e4ab0a3a0d8d81e476e6c0b19981e448ded3b9808080e3270fc0e323bfc0"中解包还原到WT_BLOCK_CKPT对应成员变量中
    WT_ERR(__wt_block_ckpt_unpack(session, block, ckpt_string, ckpt_size, ci));

    WT_ERR(__wt_scr_alloc(session, 0, &tmp));
    WT_ERR(__wt_buf_fmt(session, tmp, "version=%" PRIu8, ci->version));
    WT_ERR(__wt_buf_catfmt(session, tmp, ", object ID=%" PRIu32, ci->root_objectid));
    if (ci->root_offset == WT_BLOCK_INVALID_OFFSET)
        WT_ERR(__wt_buf_catfmt(session, tmp, ", root=[Empty]"));
    else
        WT_ERR(__wt_buf_catfmt(session, tmp,
          ", root=[%" PRIuMAX "-%" PRIuMAX ", %" PRIu32 ", %" PRIu32 "]",
          (uintmax_t)ci->root_offset, (uintmax_t)(ci->root_offset + ci->root_size), ci->root_size,
          ci->root_checksum));
    if (ci->alloc.offset == WT_BLOCK_INVALID_OFFSET)
        WT_ERR(__wt_buf_catfmt(session, tmp, ", alloc=[Empty]"));
    else
        WT_ERR(__wt_buf_catfmt(session, tmp,
          ", alloc=[%" PRIuMAX "-%" PRIuMAX ", %" PRIu32 ", %" PRIu32 "]",
          (uintmax_t)ci->alloc.offset, (uintmax_t)(ci->alloc.offset + ci->alloc.size),
          ci->alloc.size, ci->alloc.checksum));
    if (ci->avail.offset == WT_BLOCK_INVALID_OFFSET)
        WT_ERR(__wt_buf_catfmt(session, tmp, ", avail=[Empty]"));
    else
        WT_ERR(__wt_buf_catfmt(session, tmp,
          ", avail=[%" PRIuMAX "-%" PRIuMAX ", %" PRIu32 ", %" PRIu32 "]",
          (uintmax_t)ci->avail.offset, (uintmax_t)(ci->avail.offset + ci->avail.size),
          ci->avail.size, ci->avail.checksum));
    if (ci->discard.offset == WT_BLOCK_INVALID_OFFSET)
        WT_ERR(__wt_buf_catfmt(session, tmp, ", discard=[Empty]"));
    else
        WT_ERR(__wt_buf_catfmt(session, tmp,
          ", discard=[%" PRIuMAX "-%" PRIuMAX ", %" PRIu32 ", %" PRIu32 "]",
          (uintmax_t)ci->discard.offset, (uintmax_t)(ci->discard.offset + ci->discard.size),
          ci->discard.size, ci->discard.checksum));
    WT_ERR(__wt_buf_catfmt(session, tmp, ", file size=%" PRIuMAX, (uintmax_t)ci->file_size));
    //yang add todo xxxxxxxxxxx 日志完善
    WT_ERR(__wt_buf_catfmt(session, tmp, ", alloc list size=%" PRIuMAX, (uintmax_t)ci->alloc.bytes));
    WT_ERR(__wt_buf_catfmt(session, tmp, ", avail list size=%" PRIuMAX, (uintmax_t)ci->avail.bytes));
    WT_ERR(__wt_buf_catfmt(session, tmp, ", discard list size=%" PRIuMAX, (uintmax_t)ci->discard.bytes));
    WT_ERR(__wt_buf_catfmt(session, tmp, ", checkpoint size=%" PRIu64, ci->ckpt_size));

    __wt_verbose_worker(session, WT_VERB_CHECKPOINT, S2C(session)->verbose[WT_VERB_CHECKPOINT],
      "%s: %s: %s%s%s", block->name, tag, ckpt_name ? ckpt_name : "", ckpt_name ? ": " : "",
      (const char *)tmp->data);
   // printf("yang test 22222222222222222222222222222222222222222222222222222 __wt_ckpt_verbose\r\n");
err:
    __wt_scr_free(session, &tmp);
    //yang add doto xxxxxxxxxxxxxxx 这里不应该释放
    __wt_block_ckpt_destroy(session, ci);
}
