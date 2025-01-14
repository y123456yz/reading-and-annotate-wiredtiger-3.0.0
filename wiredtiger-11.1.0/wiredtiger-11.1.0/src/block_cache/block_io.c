/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

/*
 * Define a function that increments histogram statistics compression ratios.
 */
WT_STAT_COMPR_RATIO_HIST_INCR_FUNC(ratio)

/*
 * __blkcache_read_corrupt --
 *     Handle a failed read.
 */
static int
__blkcache_read_corrupt(WT_SESSION_IMPL *session, int error, const uint8_t *addr, size_t addr_size,
  const char *fail_msg) //WT_GCC_FUNC_ATTRIBUTE((cold))
{
    WT_BM *bm;
    WT_BTREE *btree;
    WT_DECL_RET;

    btree = S2BT(session);
    bm = btree->bm;

    ret = error;
    WT_ASSERT(session, ret != 0);

    F_SET(S2C(session), WT_CONN_DATA_CORRUPTION);
    if (!F_ISSET(btree, WT_BTREE_VERIFY) && !F_ISSET(session, WT_SESSION_QUIET_CORRUPT_FILE)) {
        WT_TRET(bm->corrupt(bm, session, addr, addr_size));
        WT_RET_PANIC(session, ret, "%s: fatal read error: %s", btree->dhandle->name, fail_msg);
    }
    return (ret);
}

/*
 * __wt_blkcache_read --
 *     Read an address-cookie referenced block into a buffer.
 //__ckpt_process进行checkpoint相关元数据持久化
 //__wt_meta_checkpoint获取checkpoint信息，然后__wt_block_checkpoint_load加载checkpoint相关元数据
 //__btree_preload->__wt_blkcache_read循环进行真正的数据加载

 //__wt_btree_open->__wt_btree_tree_open->__wt_blkcache_read: 根据root addr读取磁盘上面的echeckpoint avail或者alloc跳跃表中的ext元数据到内存中
 //__wt_btree_open->__btree_preload->__wt_blkcache_read: 根据ext的元数据地址addr信息从磁盘读取真实ext到buf内存中
 */

//可以参考在__wt_sync_file中会遍历所有的btree过程，看__wt_sync_file是如何遍历btree的，然后走到这里
/*
                        root page                           (root page ext持久化__wt_rec_row_int)
                        /         \
                      /             \
                    /                 \
       internal-1 page             internal-2 page          (internal page ext持久化__wt_rec_row_int)
         /      \                      /    \
        /        \                    /       \
       /          \                  /          \
leaf-1 page    leaf-2 page    leaf3 page      leaf4 page    (leaf page ext持久化__wt_rec_row_leaf)

上面这一棵树的遍历顺序: leaf1->leaf2->internal1->leaf3->leaf4->internal2->root

//从上面的图可以看出，internal page(root+internal1+internal2)总共三次走到这里, internal1记录leaf1和leaf2的page元数据[ref key, leaf page ext addr元数据]
//  internal2记录leaf3和leaf4的page元数据[ref key, leaf page ext addr元数据],
//  root记录internal1和internal2的元数据[ref key, leaf page ext addr元数据],
*/

int
__wt_blkcache_read(WT_SESSION_IMPL *session, WT_ITEM *buf, const uint8_t *addr, size_t addr_size)
{
    WT_BLKCACHE *blkcache;
    WT_BLKCACHE_ITEM *blkcache_item;
    WT_BM *bm;
    WT_BTREE *btree;
    WT_COMPRESSOR *compressor;
    WT_DECL_ITEM(etmp);
    WT_DECL_ITEM(tmp);
    WT_DECL_RET;
    WT_ENCRYPTOR *encryptor;
    WT_ITEM *ip;
    const WT_PAGE_HEADER *dsk;
    size_t compression_ratio, result_len;
    uint64_t time_diff, time_start, time_stop;
    bool blkcache_found, expect_conversion, found, skip_cache_put, timer;

    blkcache = &S2C(session)->blkcache;
    blkcache_item = NULL;
    btree = S2BT(session);
    bm = btree->bm;
    compressor = btree->compressor;
    encryptor = btree->kencryptor == NULL ? NULL : btree->kencryptor->encryptor;
    blkcache_found = found = false;
    skip_cache_put = (blkcache->type == BLKCACHE_UNCONFIGURED);

    /*
     * If anticipating a compressed or encrypted block, start with a scratch buffer and convert into
     * the caller's buffer. Else, start with the caller's buffer.
     */
    ip = buf;
    expect_conversion = compressor != NULL || encryptor != NULL;
    if (expect_conversion) {
        WT_RET(__wt_scr_alloc(session, 4 * 1024, &tmp));
        ip = tmp;
    }

    /* Check for mapped blocks. */
    WT_RET(__wt_blkcache_map_read(session, ip, addr, addr_size, &found));
    if (found) {
        skip_cache_put = true;
        if (!expect_conversion)
            goto verify;
    }

    /* Check the block cache. */
    //默认配置不使用block cache
    if (!found && blkcache->type != BLKCACHE_UNCONFIGURED) {
        __wt_blkcache_get(session, addr, addr_size, &blkcache_item, &found, &skip_cache_put);
        if (found) {
            blkcache_found = true;
            ip->data = blkcache_item->data;
            ip->size = blkcache_item->data_size;
            if (!expect_conversion) {
                /* Copy to the caller's buffer before releasing our reference. */
                WT_ERR(__wt_buf_set(session, buf, ip->data, ip->size));
                goto verify;
            }
        }
    }

    /* Read the block. */
    if (!found) {
        timer = WT_STAT_ENABLED(session) && !F_ISSET(session, WT_SESSION_INTERNAL);
        time_start = timer ? __wt_clock(session) : 0;
        //__bm_read 从addr对应磁盘地址开始读取数据
        //根据  addr读取磁盘上面的avail或者alloc跳跃表中的ext元数据到内存中

        //这里读出来的一般是一个page的大小，一般32K左右，所以下面的统计是真的page大小的
        //__bm_read
        WT_ERR(bm->read(bm, session, ip, addr, addr_size));
        if (timer) {
            time_stop = __wt_clock(session);
            time_diff = WT_CLOCKDIFF_US(time_stop, time_start);
            WT_STAT_CONN_INCR(session, cache_read_app_count);
            WT_STAT_CONN_INCRV(session, cache_read_app_time, time_diff);
            //session添加次数统计"storage":{"data":{"bytesRead":6552449495,"timeReadingMicros":595459}},"remote":"10.5.210.112:34066","protocol":"op_msg","durationMillis":9624}}
            //WiredTigerOperationStats::_statNameMap
            //这样我们可以算出单次耗时，以及单词读的page大小，确定一个page在磁盘的大小
            WT_STAT_SESSION_INCRV(session, read_time, time_diff);
        }

        dsk = ip->data;
        WT_STAT_CONN_DATA_INCR(session, cache_read);
        if (F_ISSET(dsk, WT_PAGE_COMPRESSED))
            WT_STAT_DATA_INCR(session, compress_read);
        WT_STAT_CONN_DATA_INCRV(session, cache_bytes_read, dsk->mem_size);
        WT_STAT_SESSION_INCRV(session, bytes_read, dsk->mem_size);
        (void)__wt_atomic_add64(&S2C(session)->cache->bytes_read, dsk->mem_size);
    }

    /*
     * If the block is encrypted, copy the skipped bytes of the image into place, then decrypt. DRAM
     * block-cache blocks are never encrypted.
     */
    dsk = ip->data;
    if (!blkcache_found || blkcache->type != BLKCACHE_DRAM) {
        if (F_ISSET(dsk, WT_PAGE_ENCRYPTED)) {
            if (encryptor == NULL || encryptor->decrypt == NULL)
                WT_ERR(__blkcache_read_corrupt(session, WT_ERROR, addr, addr_size,
                  "encrypted block for which no decryptor configured"));

            /*
             * If checksums were turned off because we're depending on decryption to fail on any
             * corrupted data, we'll end up here on corrupted data.
             */
            WT_ERR(__wt_scr_alloc(session, 0, &etmp));
            if ((ret = __wt_decrypt(session, encryptor, WT_BLOCK_ENCRYPT_SKIP, ip, etmp)) != 0)
                WT_ERR(__blkcache_read_corrupt(
                  session, ret, addr, addr_size, "block decryption failed"));

            ip = etmp;
        } else if (btree->kencryptor != NULL)
            WT_ERR(__blkcache_read_corrupt(session, WT_ERROR, addr, addr_size,
              "unencrypted block for which encryption configured"));
    }

    /* Store the decrypted, possibly compressed, block in the block_cache. */
    if (!skip_cache_put)
        WT_ERR(__wt_blkcache_put(session, ip, addr, addr_size, false));

    dsk = ip->data;
    if (F_ISSET(dsk, WT_PAGE_COMPRESSED)) {
        if (compressor == NULL || compressor->decompress == NULL) {
            ret = __blkcache_read_corrupt(session, WT_ERROR, addr, addr_size,
              "compressed block for which no compression configured");
            /* Odd error handling structure to avoid static analyzer complaints. */
            WT_ERR(ret == 0 ? WT_ERROR : ret);
        }

        /* Size the buffer based on the in-memory bytes we're expecting from decompression. */
        WT_ERR(__wt_buf_initsize(session, buf, dsk->mem_size));

        /*
         * Note the source length is NOT the number of compressed bytes, it's the length of the
         * block we just read (minus the skipped bytes). We don't store the number of compressed
         * bytes: some compression engines need that length stored externally, they don't have
         * markers in the stream to signal the end of the compressed bytes. Those engines must store
         * the compressed byte length somehow, see the snappy compression extension for an example.
         * In other words, the "tmp" in the decompress call isn't a mistake.
         */
        memcpy(buf->mem, ip->data, WT_BLOCK_COMPRESS_SKIP);
        ret = compressor->decompress(btree->compressor, &session->iface,
          (uint8_t *)ip->data + WT_BLOCK_COMPRESS_SKIP, tmp->size - WT_BLOCK_COMPRESS_SKIP,
          (uint8_t *)buf->mem + WT_BLOCK_COMPRESS_SKIP, dsk->mem_size - WT_BLOCK_COMPRESS_SKIP,
          &result_len);
        if (result_len != dsk->mem_size - WT_BLOCK_COMPRESS_SKIP)
            WT_TRET(WT_ERROR);

        /*
         * If checksums were turned off because we're depending on decompression to fail on any
         * corrupted data, we'll end up here on corrupted data.
         */
        if (ret != 0)
            WT_ERR(
              __blkcache_read_corrupt(session, ret, addr, addr_size, "block decompression failed"));

        compression_ratio = result_len / (tmp->size - WT_BLOCK_COMPRESS_SKIP);
        __wt_stat_compr_ratio_hist_incr(session, compression_ratio);

    } else {
        /*
         * If we uncompressed above, the page is in the correct buffer. If we get here the data may
         * be in the wrong buffer and the buffer may be the wrong size. If needed, get the page into
         * the destination buffer.
         */
        if (ip != buf)
            WT_ERR(__wt_buf_set(session, buf, ip->data, dsk->mem_size));
    }

verify:
    /* If the handle is a verify handle, verify the physical page. */
    if (F_ISSET(btree, WT_BTREE_VERIFY)) {
        if (tmp == NULL)
            WT_ERR(__wt_scr_alloc(session, 4 * 1024, &tmp));
        WT_ERR(bm->addr_string(bm, session, tmp, addr, addr_size));
        WT_ERR(__wt_verify_dsk(session, tmp->data, buf));
    }

err:
    /* If we pulled the block from the block cache, decrement its reference count. */
    if (blkcache_found)
        (void)__wt_atomic_subv32(&blkcache_item->ref_count, 1);

    __wt_scr_free(session, &tmp);
    __wt_scr_free(session, &etmp);
    return (ret);
}

/*

 //internal page持久化到ext流程: __reconcile->__wt_rec_row_int->__wt_rec_split_finish->__rec_split_write->__rec_write
 //    ->__wt_blkcache_write->__bm_checkpoint->__bm_checkpoint

 //leaf page持久化到ext流程: __reconcile->__wt_rec_row_leaf->__wt_rec_split_finish->__rec_split_write->__rec_write
 //    ->__wt_blkcache_write->__bm_write->__wt_block_write

 * __wt_blkcache_write --
 *     Write a buffer into a block, returning the block's address cookie.
 //internal page持久化到ext流程: __reconcile->__wt_rec_row_int->__wt_rec_split_finish->__rec_split_write->__rec_write
 //    ->__wt_blkcache_write
 */
//buf数据内容 = 包括page header + block header + 实际数据
//bug实际上指向该page对应的真实磁盘空间，WT_REC_CHUNK.image=WT_PAGE_HEADER_SIZE + WT_BLOCK_HEADER_SIZE + 实际数据

//数据写入磁盘，并对写入磁盘的以下元数据进行封装处理，objectid offset size  checksum四个字段进行封包存入addr数组中，addr_sizep为数组存入数据总长度
//__rec_write->__wt_blkcache_write
int
__wt_blkcache_write(WT_SESSION_IMPL *session, WT_ITEM *buf, uint8_t *addr, size_t *addr_sizep,
  //压缩后的数据长度，如果没有压缩，或者压缩后空间并没有减少，则直接compressed_sizep返回0
  size_t *compressed_sizep, 
  bool checkpoint, bool checkpoint_io, bool compressed)
{
    WT_BLKCACHE *blkcache;
    WT_BM *bm;
    WT_BTREE *btree;
    WT_DECL_ITEM(ctmp);
    WT_DECL_ITEM(etmp);
    WT_DECL_RET;
    WT_ITEM *ip;
    WT_KEYED_ENCRYPTOR *kencryptor;
    WT_PAGE_HEADER *dsk;
    size_t dst_len, len, result_len, size, src_len;
    uint64_t time_diff, time_start, time_stop;
    uint8_t *dst, *src;
    int compression_failed; /* Extension API, so not a bool. */
    bool data_checksum, encrypted, timer;

    //如果没有压缩，或者压缩后空间并没有减少，则直接compressed_sizep返回0
    if (compressed_sizep != NULL)
        *compressed_sizep = 0;

    blkcache = &S2C(session)->blkcache;
    btree = S2BT(session);
    bm = btree->bm;
    encrypted = false;

    /*
     * Optionally stream-compress the data, but don't compress blocks that are already as small as
     * they're going to get.
     */
    //不需要压缩
    if (btree->compressor == NULL || btree->compressor->compress == NULL || compressed)
        ip = buf;
    else if (buf->size <= btree->allocsize) {//内容不大，不需要压缩
        ip = buf;
        WT_STAT_DATA_INCR(session, compress_write_too_small);
    } else {//需要对buf进行压缩
        /* Skip the header bytes of the source data. */
        src = (uint8_t *)buf->mem + WT_BLOCK_COMPRESS_SKIP;
        src_len = buf->size - WT_BLOCK_COMPRESS_SKIP;

        /*
         * Compute the size needed for the destination buffer. We only allocate enough memory for a
         * copy of the original by default, if any compressed version is bigger than the original,
         * we won't use it. However, some compression engines (snappy is one example), may need more
         * memory because they don't stop just because there's no more memory into which to
         * compress.
         */
        if (btree->compressor->pre_size == NULL)
            len = src_len;
        else
            WT_ERR(
              btree->compressor->pre_size(btree->compressor, &session->iface, src, src_len, &len));

        size = len + WT_BLOCK_COMPRESS_SKIP;
        //__bm_write_size
        WT_ERR(bm->write_size(bm, session, &size));
        WT_ERR(__wt_scr_alloc(session, size, &ctmp));

        /* Skip the header bytes of the destination data. */
        //压缩后的数据存储到这里
        dst = (uint8_t *)ctmp->mem + WT_BLOCK_COMPRESS_SKIP;
        dst_len = len;

        compression_failed = 0;
        WT_ERR(btree->compressor->compress(btree->compressor, &session->iface, src, src_len, dst,
          dst_len, &result_len, &compression_failed));

        //长度要加上WT_BLOCK_COMPRESS_SKIP
        result_len += WT_BLOCK_COMPRESS_SKIP;

        /*
         * If compression fails, or doesn't gain us at least one unit of allocation, fallback to the
         * original version. This isn't unexpected: if compression doesn't work for some chunk of
         * data for some reason (noting likely additional format/header information which compressed
         * output requires), it just means the uncompressed version is as good as it gets, and
         * that's what we use.
         */
        //1. 压缩失败
        //2. 压缩后数据反而更大了，也就是代表压缩没有效果
        if (compression_failed || buf->size / btree->allocsize <= result_len / btree->allocsize) {
            ip = buf;
            WT_STAT_DATA_INCR(session, compress_write_fail);
        } else {
            compressed = true;
            WT_STAT_DATA_INCR(session, compress_write);

            /* Copy in the skipped header bytes and set the final data size. */
            //头部WT_BLOCK_COMPRESS_SKIP字节拷贝到压缩后的ctmp数据头部
            memcpy(ctmp->mem, buf->mem, WT_BLOCK_COMPRESS_SKIP);
            ctmp->size = result_len;
            ip = ctmp;

            /* Set the disk header flags. */
            dsk = ip->mem;
            F_SET(dsk, WT_PAGE_COMPRESSED);

            /* Optionally return the compressed size. */
            //压缩后的数据长度, 注意不包括WT_BLOCK_COMPRESS_SKIP头部没压缩的字段
            if (compressed_sizep != NULL)
                *compressed_sizep = result_len;
        }
        printf("yang test ...__wt_blkcache_write....befor compress len:%d, after len:%d\r\n",
            (int)src_len, (int)result_len - WT_BLOCK_COMPRESS_SKIP);
    }
    
    /*
     * Optionally encrypt the data. We need to add in the original length, in case both compression
     * and encryption are done.
     */
    if ((kencryptor = btree->kencryptor) != NULL) {//加密相关
        /*
         * Get size needed for encrypted buffer.
         */
        __wt_encrypt_size(session, kencryptor, ip->size, &size);

        WT_ERR(bm->write_size(bm, session, &size));
        WT_ERR(__wt_scr_alloc(session, size, &etmp));
        WT_ERR(__wt_encrypt(session, kencryptor, WT_BLOCK_ENCRYPT_SKIP, ip, etmp));

        encrypted = true;
        ip = etmp;

        /* Set the disk header flags. */
        dsk = ip->mem;
        if (compressed)
            F_SET(dsk, WT_PAGE_COMPRESSED);
        F_SET(dsk, WT_PAGE_ENCRYPTED);
    }

    /* Determine if the data requires a checksum. */
    data_checksum = true;
    switch (btree->checksum) {
    case CKSUM_ON://默认走这里
        /* Set outside the switch to avoid compiler and analyzer complaints. */
        break;
    case CKSUM_OFF:
        data_checksum = false;
        break;
    case CKSUM_UNCOMPRESSED:
        data_checksum = !compressed;
        break;
    case CKSUM_UNENCRYPTED:
        data_checksum = !encrypted;
        break;
    }

    //printf("yang test .............__wt_blkcache_write.........................checkpoint:%d\r\n", checkpoint);
    /* Call the block manager to write the block. */
    timer = WT_STAT_ENABLED(session) && !F_ISSET(session, WT_SESSION_INTERNAL);
    time_start = timer ? __wt_clock(session) : 0;
                        //__bm_checkpoint
    WT_ERR(checkpoint ? bm->checkpoint(bm, session, ip, btree->ckpt, data_checksum) :
    //数据写入磁盘，并对写入磁盘的以下元数据进行封装处理，objectid offset size  checksum四个字段进行封包存入addr数组中，addr_sizep为数组存入数据总长度
                        //__bm_write
                        bm->write(bm, session, ip, addr, addr_sizep, data_checksum, checkpoint_io));
    if (timer) {
        time_stop = __wt_clock(session);
        time_diff = WT_CLOCKDIFF_US(time_stop, time_start);
        WT_STAT_CONN_INCR(session, cache_write_app_count);
        WT_STAT_CONN_INCRV(session, cache_write_app_time, time_diff);
        WT_STAT_SESSION_INCRV(session, write_time, time_diff);
    }

    /*
     * The page image must have a proper write generation number before writing it to disk. The page
     * images that are created during recovery may have the write generation number less than the
     * btree base write generation number, so don't verify it.
     */
    dsk = ip->mem;
    WT_ASSERT(session, dsk->write_gen != 0);

    //对应查询对应mongo server的WiredTigerOperationStats::_statNameMap
    WT_STAT_CONN_DATA_INCR(session, cache_write);
    WT_STAT_CONN_DATA_INCRV(session, cache_bytes_write, dsk->mem_size);
    WT_STAT_SESSION_INCRV(session, bytes_write, dsk->mem_size);
    (void)__wt_atomic_add64(&S2C(session)->cache->bytes_written, dsk->mem_size);

    /*
     * Store a copy of the compressed buffer in the block cache.
     *
     * Optional if the write is part of a checkpoint. Hot blocks get written and over-written a lot
     * as part of checkpoint, so we don't want to cache them, because (a) they are in the in-memory
     * cache anyway, and (b) they are likely to be overwritten again in the next checkpoint. Writes
     * that are not part of checkpoint I/O are done in the service of eviction. Those are the blocks
     * that the in-memory cache would like to keep but can't, and we definitely want to keep them.
     *
     * Optional on normal writes (vs. reads) if the no-write-allocate setting is on.
     *
     * Ignore the final checkpoint writes.
     */
    //默认不使能，直接跳过
    if (blkcache->type == BLKCACHE_UNCONFIGURED)
        ;
    else if (!blkcache->cache_on_checkpoint && checkpoint_io)
        WT_STAT_CONN_INCR(session, block_cache_bypass_chkpt);
    else if (!blkcache->cache_on_writes)
        WT_STAT_CONN_INCR(session, block_cache_bypass_writealloc);
    else if (!checkpoint)
        WT_ERR(__wt_blkcache_put(session, compressed ? ctmp : buf, addr, *addr_sizep, true));

err:
    __wt_scr_free(session, &ctmp);
    __wt_scr_free(session, &etmp);
    return (ret);
}
