/*-
 * Copyright (c) 2014-2017 MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

/*
 * __wt_stream_set_line_buffer --
 *	Set line buffering on a stream.
 */
void
__wt_stream_set_line_buffer(FILE *fp)
{
	/*
	 * This function exists because MSVC doesn't support buffer sizes of 0
	 * to the setvbuf call. To avoid re-introducing the bug, we have helper
	 * functions and disallow calling setvbuf directly in WiredTiger code.
	 *
	 * Additionally, MSVC doesn't support line buffering, the result is the
	 * same as full-buffering. We assume our caller wants immediate output,
	 * set no-buffering instead.
	 */
	__wt_stream_set_no_buffer(fp);
}

/*
 * __wt_stream_set_no_buffer --
 *	Turn off buffering on a stream.
 */
void
__wt_stream_set_no_buffer(FILE *fp)
{
    //_IONBF	无缓冲：不使用缓冲。每个 I/O 操作都被即时写入。buffer 和 size 参数被忽略。
	(void)setvbuf(fp, NULL, _IONBF, 0);
}
