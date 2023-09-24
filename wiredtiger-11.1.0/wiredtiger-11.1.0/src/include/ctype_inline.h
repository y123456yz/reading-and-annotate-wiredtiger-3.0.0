/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include <ctype.h>

/*
isalnum(测试字符是否为英文字母或数字)
isalpha(测试字符是否为英文字母)
isascii(测试字符是否为ASCII码字符)
isblank(测试字符是否为空格字符)
iscntrl(测试字符是否为ASCII码的控制字符)
isdigit(测试字符是否为阿拉伯数字)
isgraph(测试字符是否为可打印字符)
islower(测试字符是否为小写英文字母)
isprint(测试字符是否为可打印字符)
isspace(测试字符是否为空格字符)
ispunct(测试字符是否为标点符号或特殊符号)
isupper(测试字符是否为大写英文字母)
isxdigit(测试字符是否为16进制数字)
*/
/*
 * __wt_isalnum --
 *     Wrap the ctype function without sign extension.
 */
static inline bool
__wt_isalnum(u_char c)
{
    return (isalnum(c) != 0);
}

/*
 * __wt_isalpha --
 *     Wrap the ctype function without sign extension.
 */
static inline bool
__wt_isalpha(u_char c)
{
    return (isalpha(c) != 0);
}

/*
 * __wt_isascii --
 *     Wrap the ctype function without sign extension.
 */
static inline bool
__wt_isascii(u_char c)
{
    return (isascii(c) != 0);
}

/*
 * __wt_isdigit --
 *     Wrap the ctype function without sign extension.
 */
static inline bool
__wt_isdigit(u_char c)
{
    return (isdigit(c) != 0);
}

/*
 * __wt_isprint --
 *     Wrap the ctype function without sign extension.
 */
static inline bool
__wt_isprint(u_char c)
{
    //isprint(测试字符是否为可打印字符)
    return (isprint(c) != 0);
}

/*
 * __wt_isspace --
 *     Wrap the ctype function without sign extension.
 */
static inline bool
__wt_isspace(u_char c)
{
    return (isspace(c) != 0);
}

/*
 * __wt_tolower --
 *     Wrap the ctype function without sign extension.
 */
static inline u_char
__wt_tolower(u_char c)
{
    return ((u_char)tolower(c));
}
