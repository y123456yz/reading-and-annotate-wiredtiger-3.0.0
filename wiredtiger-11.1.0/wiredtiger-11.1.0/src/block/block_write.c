/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

/*
 * __wt_block_truncate --
 *     Truncate the file.
 */
int
__wt_block_truncate(WT_SESSION_IMPL *session, WT_BLOCK *block, wt_off_t len)
{
    WT_CONNECTION_IMPL *conn;
    WT_DECL_RET;

    conn = S2C(session);

    //yang add todo xxxxx 完善日志
    __wt_verbose(session, WT_VERB_BLOCK, "truncate file:%s to %" PRIuMAX, block->name, (uintmax_t)len);

    /*
     * Truncate requires serialization, we depend on our caller for that.
     *
     * Truncation isn't a requirement of the block manager, it's only used to conserve disk space.
     * Regardless of the underlying file system call's result, the in-memory understanding of the
     * file size changes.
     */
    block->size = block->extend_size = len;

    /*
     * Backups are done by copying files outside of WiredTiger, potentially by system utilities. We
     * cannot truncate the file during the backup window, we might surprise an application.
     *
     * This affects files that aren't involved in the backup (for example, doing incremental
     * backups, which only copies log files, or targeted backups, stops all block truncation
     * unnecessarily). We may want a more targeted solution at some point.
     */
    if (conn->hot_backup_start == 0)
        WT_WITH_HOTBACKUP_READ_LOCK(session, ret = __wt_ftruncate(session, block->fh, len), NULL);

    /*
     * The truncate may fail temporarily or permanently (for example, there may be a file mapping if
     * there's an open checkpoint on the file on a POSIX system, in which case the underlying
     * function returns EBUSY). It's OK, we don't have to be able to truncate files.
     */
    return (ret == EBUSY || ret == ENOTSUP ? 0 : ret);
}

/*
 * __wt_block_discard --
 *     Discard blocks from the system buffer cache.
 posix_fadvise清理系统中的文件缓存
 */
int
__wt_block_discard(WT_SESSION_IMPL *session, WT_BLOCK *block, size_t added_size)
{
    WT_DECL_RET;
    WT_FILE_HANDLE *handle;

    /* The file may not support this call. */
    handle = block->fh->handle;
    if (handle->fh_advise == NULL)
        return (0);

    /* The call may not be configured. */
    if (block->os_cache_max == 0)
        return (0);

    /*
     * We're racing on the addition, but I'm not willing to serialize on it in the standard read
     * path without evidence it's needed.
     */
    if ((block->os_cache += added_size) <= block->os_cache_max)
        return (0);

    block->os_cache = 0;
    //posix_fadvise清理系统中的文件缓存
    ret = handle->fh_advise(
      handle, (WT_SESSION *)session, (wt_off_t)0, (wt_off_t)0, WT_FILE_HANDLE_DONTNEED);
    return (ret == EBUSY || ret == ENOTSUP ? 0 : ret);
}

/*
 * __wt_block_extend --
 *     Extend the file.
 */
static inline int
__wt_block_extend(WT_SESSION_IMPL *session, WT_BLOCK *block, WT_FH *fh, wt_off_t offset,
  size_t align_size, bool *release_lockp)
{
    WT_DECL_RET;
    WT_FILE_HANDLE *handle;

    /*
     * The locking in this function is messy: by definition, the live system is locked when we're
     * called, but that lock may have been acquired by our caller or our caller's caller. If our
     * caller's lock, release_lock comes in set and this function can unlock it before returning (so
     * it isn't held while extending the file). If it is our caller's caller, then release_lock
     * comes in not set, indicating it cannot be released here.
     *
     * If we unlock here, we clear release_lock.
     */

    //默认不配置，直接从这里返回
    /* If not configured to extend the file, we're done. */
    if (block->extend_len == 0)
        return (0);

    /*
     * Extend the file in chunks. We want to limit the number of threads extending the file at the
     * same time, so choose the one thread that's crossing the extended boundary. We don't extend
     * newly created files, and it's theoretically possible we might wait so long our extension of
     * the file is passed by another thread writing single blocks, that's why there's a check in
     * case the extended file size becomes too small: if the file size catches up, every thread
     * tries to extend it.
     */
    if (block->extend_size > block->size &&
      (offset > block->extend_size ||
        offset + block->extend_len + (wt_off_t)align_size < block->extend_size))
        return (0);

    /*
     * File extension may require locking: some variants of the system call used to extend the file
     * initialize the extended space. If a writing thread races with the extending thread, the
     * extending thread might overwrite already written data, and that would be very, very bad.
     */
    handle = fh->handle;
    if (handle->fh_extend == NULL && handle->fh_extend_nolock == NULL)
        return (0);

    /*
     * Set the extend_size before releasing the lock, I don't want to read and manipulate multiple
     * values without holding a lock.
     *
     * There's a race between the calculation and doing the extension, but it should err on the side
     * of extend_size being smaller than the actual file size, and that's OK, we simply may do
     * another extension sooner than otherwise.
     */
    //file_extend配置，默认为0，所以extend_size也就是是当前block size，见__wt_block_extend
    block->extend_size = block->size + block->extend_len * 2;

    /*
     * Release any locally acquired lock if not needed to extend the file, extending the file may
     * require updating on-disk file's metadata, which can be slow. (It may be a bad idea to
     * configure for file extension on systems that require locking over the extend call.)
     */
    if (handle->fh_extend_nolock != NULL && *release_lockp) {
        *release_lockp = false;
        __wt_spin_unlock(session, &block->live_lock);
    }

    /*
     * The extend might fail (for example, the file is mapped into memory or a backup is in
     * progress), or discover file extension isn't supported; both are OK.
     */
    if (S2C(session)->hot_backup_start == 0)
        WT_WITH_HOTBACKUP_READ_LOCK(
          session, ret = __wt_fextend(session, fh, block->extend_size), NULL);
    return (ret == EBUSY || ret == ENOTSUP ? 0 : ret);
}

/*
 * __wt_block_write_size --
 *     Return the buffer size required to write a block.
 */ //block size = WT_PAGE_HEADER_SIZE + WT_BLOCK_HEADER_SIZE + 实际数据sizep
int
__wt_block_write_size(WT_SESSION_IMPL *session, WT_BLOCK *block, size_t *sizep)
{
    WT_UNUSED(session);

    /*
     * We write the page size, in bytes, into the block's header as a 4B unsigned value, and it's
     * possible for the engine to accept an item we can't write. For example, a huge key/value where
     * the allocation size has been set to something large will overflow 4B when it tries to align
     * the write. We could make this work (for example, writing the page size in units of allocation
     * size or something else), but it's not worth the effort, writing 4GB objects into a btree
     * makes no sense. Limit the writes to (4GB - 1KB), it gives us potential mode bits, and I'm not
     * interested in debugging corner cases anyway.
     */
    *sizep = (size_t)WT_ALIGN(*sizep + WT_BLOCK_HEADER_BYTE_SIZE, block->allocsize);
    return (*sizep > UINT32_MAX - 1024 ? EINVAL : 0);
}

/*
 //internal page持久化到ext流程: __reconcile->__wt_rec_row_int->__wt_rec_split_finish->__rec_split_write->__rec_write
 //    ->__wt_blkcache_write->__bm_checkpoint->__wt_block_checkpoint

 //leaf page持久化到ext流程: __reconcile->__wt_rec_row_leaf->__wt_rec_split_finish->__rec_split_write->__rec_write
 //    ->__wt_blkcache_write->__bm_write->__wt_block_write

 * __wt_block_write --
 *     Write a buffer into a block, returning the block's address cookie.
 //buf数据内容 = 包括page header + block header + 实际数据
 //bug实际上指向该page对应的真实磁盘空间，WT_REC_CHUNK.image=WT_PAGE_HEADER_SIZE + WT_BLOCK_HEADER_SIZE + 实际数据
 */

//数据写入磁盘，并对写入磁盘的以下元数据进行封装处理，objectid offset size  checksum四个字段进行封包存入addr数组中，addr_sizep为数组存入数据总长度
int
__wt_block_write(WT_SESSION_IMPL *session, WT_BLOCK *block, WT_ITEM *buf, uint8_t *addr,
  size_t *addr_sizep, bool data_checksum, bool checkpoint_io)
{
    wt_off_t offset;
    uint32_t checksum, objectid, size;
    uint8_t *endp;

    //数据写入磁盘，并返回objectidp, offsetp, sizep和checksump
    WT_RET(__wt_block_write_off(session, block, buf, &objectid, &offset, &size, &checksum,
      data_checksum, checkpoint_io, false));

    endp = addr;
    //对objectid offset size  checksum四个字段进行封包存入addr数组中
    WT_RET(__wt_block_addr_pack(block, &endp, objectid, offset, size, checksum));
    //封装后的数据存入到addr数组后，数组长度大小
    *addr_sizep = WT_PTRDIFF(endp, addr);

    return (0);
}

/*
 * __block_write_off --
 *     Write a buffer into a block, returning the block's offset, size and checksum.
 //数据写入磁盘
 //bug实际上指向该page对应的真实磁盘空间，WT_REC_CHUNK.image=WT_PAGE_HEADER_SIZE + WT_BLOCK_HEADER_SIZE + 实际数据

//从avail中查找一个可用ext或者alloc一个新的ext来存储buf数据在磁盘上面的off元数据信息，
//数据写入磁盘，并返回objectidp, offsetp, sizep和checksump
 */
static int
__block_write_off(WT_SESSION_IMPL *session, WT_BLOCK *block, WT_ITEM *buf, uint32_t *objectidp,
  wt_off_t *offsetp, uint32_t *sizep, uint32_t *checksump, bool data_checksum, bool checkpoint_io,
  bool caller_locked)
{
    WT_BLOCK_HEADER *blk;
    WT_DECL_RET;
    WT_FH *fh;
    wt_off_t offset;
    size_t align_size;
    uint32_t checksum, objectid;
    uint8_t *file_sizep;
    bool local_locked;

    *offsetp = 0;   /* -Werror=maybe-uninitialized */
    *sizep = 0;     /* -Werror=maybe-uninitialized */
    *checksump = 0; /* -Werror=maybe-uninitialized */

    fh = block->fh;
    objectid = block->objectid;

    /* Buffers should be aligned for writing. */
    if (!F_ISSET(buf, WT_ITEM_ALIGNED)) {
        WT_ASSERT(session, F_ISSET(buf, WT_ITEM_ALIGNED));
        WT_RET_MSG(session, EINVAL, "direct I/O check: write buffer incorrectly allocated");
    }

    /*
     * File checkpoint/recovery magic: done before sizing the buffer as it may grow the buffer.
     */
    //如果需要对checkpoint元数据信息持久化，则持久化到wiredtiger.wt中
    if (block->final_ckpt != NULL)
        //Append metadata and checkpoint information to a buffer.
        WT_RET(__wt_block_checkpoint_final(session, block, buf, &file_sizep));

    /*
     * Align the size to an allocation unit.
     *
     * The buffer must be big enough for us to zero to the next allocsize boundary, this is one of
     * the reasons the btree layer must find out from the block-manager layer the maximum size of
     * the eventual write.
     */
    //buf数据线性化对齐
    align_size = WT_ALIGN(buf->size, block->allocsize);
    if (align_size > buf->memsize) {
        WT_ASSERT(session, align_size <= buf->memsize);
        WT_RET_MSG(session, EINVAL, "buffer size check: write buffer incorrectly allocated");
    }
    if (align_size > UINT32_MAX) {
        WT_ASSERT(session, align_size <= UINT32_MAX);
        WT_RET_MSG(session, EINVAL, "buffer size check: write buffer too large to write");
    }

    /* Pre-allocate some number of extension structures. */
    //提前分配辅助空间
    //为session->block_manager提前分配5个WT_EXT and WT_SIZE structures.
    WT_RET(__wt_block_ext_prealloc(session, 5));

    /*
     * Acquire a lock, if we don't already hold one. Allocate space for the write, and optionally
     * extend the file (note the block-extend function may release the lock). Release any locally
     * acquired lock.
     */
    local_locked = false;
    if (!caller_locked) {
        __wt_spin_lock(session, &block->live_lock);
        local_locked = true;
    }

    //align_size是一个page在磁盘上面的总大小，=WT_PAGE_HEADER_SIZE + WT_BLOCK_HEADER_SIZE + 实际数据
    //也就是一次性分配一个page该有的磁盘空间元数据
    //offset也就是buf数据需要从文件的offset这个位置开始向文件中写入
    ret = __wt_block_alloc(session, block, &offset, (wt_off_t)align_size);
    if (ret == 0)
        //默认file_extend不配置，忽略该函数
        ret = __wt_block_extend(session, block, fh, offset, align_size, &local_locked);
    if (local_locked)
        __wt_spin_unlock(session, &block->live_lock);
    WT_RET(ret);

    /*
     * The file has finished changing size. If this is the final write in a checkpoint, update the
     * checkpoint's information inline.
     */
    if (block->final_ckpt != NULL)
        WT_RET(__wt_vpack_uint(&file_sizep, 0, (uint64_t)block->size));

    /* Zero out any unused bytes at the end of the buffer. */
    //因为4098字节对齐填充的部分先全部置为0，例如buf mem大小1000字节，但是我们进行了4098字节对齐，多了3098字节，这里需要用0填充
    memset((uint8_t *)buf->mem + buf->size, 0, align_size - buf->size);

    /*
     * Clear the block header to ensure all of it is initialized, even the unused fields.
     */
    // WT_PAGE_HEADER_SIZE + WT_BLOCK_HEADER_SIZE + 实际数据 中的block header初始化清零
    blk = WT_BLOCK_HEADER_REF(buf->mem);
    memset(blk, 0, sizeof(*blk));

    /*
     * Set the disk size so we don't have to incrementally read blocks during salvage.
     */
    blk->disk_size = WT_STORE_SIZE(align_size);

    /*
     * Update the block's checksum: checksum the complete data if our caller specifies, otherwise
     * checksum the leading WT_BLOCK_COMPRESS_SKIP bytes. Applications with a compression or
     * encryption engine that includes checksums won't need a separate checksum. However, if the
     * block was too small for compression, or compression failed to shrink the block, the block
     * wasn't compressed, in which case our caller will tell us to checksum the data. If skipping
     * checksums because of compression or encryption, we still need to checksum the first
     * WT_BLOCK_COMPRESS_SKIP bytes because they're not compressed or encrypted, both to give
     * salvage a quick test of whether a block is useful and to give us a test so we don't lose the
     * first WT_BLOCK_COMPRESS_SKIP bytes without noticing.
     *
     * Checksum a little-endian version of the header, and write everything in little-endian format.
     * The checksum is (potentially) returned in a big-endian format, swap it into place in a
     * separate step.
     */
    blk->flags = 0;
    if (data_checksum)
        F_SET(blk, WT_BLOCK_DATA_CKSUM);
    blk->checksum = 0;
    __wt_block_header_byteswap(blk);
    blk->checksum = checksum =
      __wt_checksum(buf->mem, data_checksum ? align_size : WT_BLOCK_COMPRESS_SKIP);
#ifdef WORDS_BIGENDIAN
    blk->checksum = __wt_bswap32(blk->checksum);
#endif

    /* Write the block. */
    // WT_PAGE_HEADER_SIZE + WT_BLOCK_HEADER_SIZE + 实际数据，把这个page对应的数据写入磁盘
    if ((ret = __wt_write(session, fh, offset, align_size, buf->mem)) != 0) {
        if (!caller_locked)
            __wt_spin_lock(session, &block->live_lock);
        WT_TRET(__wt_block_off_free(session, block, objectid, offset, (wt_off_t)align_size));
        if (!caller_locked)
            __wt_spin_unlock(session, &block->live_lock);
        WT_RET(ret);
    }

    /*
     * Optionally schedule writes for dirty pages in the system buffer cache, but only if the
     * current session can wait.
     */
    //os_cache_dirty_max配置，默认为0, 也就是每写入多少数据，就进行强制__wt_fsync刷盘
    if (block->os_cache_dirty_max != 0 && fh->written > block->os_cache_dirty_max &&
      __wt_session_can_wait(session)) {
        fh->written = 0;
        if ((ret = __wt_fsync(session, fh, false)) != 0) {
            /*
             * Ignore ENOTSUP, but don't try again.
             */
            if (ret != ENOTSUP)
                return (ret);
            block->os_cache_dirty_max = 0;
        }
    }

    /* Optionally discard blocks from the buffer cache. */
    WT_RET(__wt_block_discard(session, block, align_size));

    WT_STAT_CONN_INCR(session, block_write);
    WT_STAT_CONN_INCRV(session, block_byte_write, align_size);
    if (checkpoint_io)
        //只统计因为checkpoint产生的写操作
        WT_STAT_CONN_INCRV(session, block_byte_write_checkpoint, align_size);

    __wt_verbose_debug2(session, WT_VERB_WRITE,
      "off %" PRIuMAX ", size %" PRIuMAX ", padding len %" PRIuMAX ", checksum %#" PRIx32, (uintmax_t)offset,
      (uintmax_t)align_size, (uintmax_t)align_size - buf->size, checksum);

    *objectidp = objectid;
    *offsetp = offset;
    *sizep = WT_STORE_SIZE(align_size);
    *checksump = checksum;

    return (0);
}

/*
 * __wt_block_write_off --
 *     Write a buffer into a block, returning the block's offset, size and checksum.
 */
//bug实际上指向该page对应的真实磁盘空间，WT_REC_CHUNK.image=WT_PAGE_HEADER_SIZE + WT_BLOCK_HEADER_SIZE + 实际数据

//数据写入磁盘，并返回objectidp, offsetp, sizep和checksump
int
__wt_block_write_off(WT_SESSION_IMPL *session, WT_BLOCK *block, WT_ITEM *buf, uint32_t *objectidp,
  wt_off_t *offsetp, uint32_t *sizep, uint32_t *checksump, bool data_checksum, bool checkpoint_io,
  bool caller_locked)
{
    WT_DECL_RET;

    /*
     * Ensure the page header is in little endian order; this doesn't belong here, but it's the best
     * place to catch all callers. After the write, swap values back to native order so callers
     * never see anything other than their original content.
     */
    //大小端对齐，忽略
    __wt_page_header_byteswap(buf->mem);

    //数据写入磁盘，并返回objectidp, offsetp, sizep和checksump
    ret = __block_write_off(session, block, buf, objectidp, offsetp, sizep, checksump,
      data_checksum, checkpoint_io, caller_locked);

    //大小端对齐，忽略
    __wt_page_header_byteswap(buf->mem);
    return (ret);
}
