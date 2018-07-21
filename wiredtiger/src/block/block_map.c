/*-
 * Copyright (c) 2014-2017 MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

/*
 * __wt_block_map --
 *	Map a segment of the file in, if possible.
 */
/*conn指定了用mmap方式操作文件，那么block的操作方式需要设置成mmap方式操作*/
//文件对应的内存地址通过mapped_regionp返回，长度lengthp
int
__wt_block_map(WT_SESSION_IMPL *session, WT_BLOCK *block,
    void *mapped_regionp, size_t *lengthp, void *mapped_cookiep)
{
	WT_DECL_RET;
	WT_FILE_HANDLE *handle;

	*(void **)mapped_regionp = NULL;
	*lengthp = 0;
	*(void **)mapped_cookiep = NULL;

	/* Map support is configurable. */
	if (!S2C(session)->mmap)
		return (0);

	/*
	 * Turn off mapping when verifying the file, because we can't perform
	 * checksum validation of mapped segments, and verify has to checksum
	 * pages.
	 */
	/*如果block设置了verify操作函数，不能用mmap*/
	if (block->verify)
		return (0);

	/*
	 * Turn off mapping if the application configured a cache size maximum,
	 * we can't control how much of the cache size we use in that case.
	 */
	/*block对应的文件设置了os page cache，无法使用mmap,因为文件需要根据os_cache_max去清空os page cache,这是mmap做不到的*/
	if (block->os_cache_max != 0)
		return (0);

	/*
	 * There may be no underlying functionality.
	 */
	handle = block->fh->handle;
	if (handle->fh_map == NULL)
		return (0);

	/*
	 * Map the file into memory.
	 * Ignore not-supported errors, we'll read the file through the cache
	 * if map fails.
	 */
	//__wt_posix_map /*block 文件的mmap隐射挂载*/
	ret = handle->fh_map(handle,
	    (WT_SESSION *)session, mapped_regionp, lengthp, mapped_cookiep);
	if (ret == EBUSY || ret == ENOTSUP) {
		*(void **)mapped_regionp = NULL;
		ret = 0;
	}

	return (ret);
}

/*
 * __wt_block_unmap --
 *	Unmap any mapped-in segment of the file.
 */
int
__wt_block_unmap(WT_SESSION_IMPL *session,
    WT_BLOCK *block, void *mapped_region, size_t length, void *mapped_cookie)
{
	WT_FILE_HANDLE *handle;

	/* Unmap the file from memory. */
	handle = block->fh->handle;
	return (handle->fh_unmap(handle,
	    (WT_SESSION *)session, mapped_region, length, mapped_cookie));
}
