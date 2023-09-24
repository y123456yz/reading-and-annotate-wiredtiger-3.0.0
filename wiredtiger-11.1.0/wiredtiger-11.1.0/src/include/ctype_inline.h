/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include <ctype.h>

/*
isalnum(�����ַ��Ƿ�ΪӢ����ĸ������)
isalpha(�����ַ��Ƿ�ΪӢ����ĸ)
isascii(�����ַ��Ƿ�ΪASCII���ַ�)
isblank(�����ַ��Ƿ�Ϊ�ո��ַ�)
iscntrl(�����ַ��Ƿ�ΪASCII��Ŀ����ַ�)
isdigit(�����ַ��Ƿ�Ϊ����������)
isgraph(�����ַ��Ƿ�Ϊ�ɴ�ӡ�ַ�)
islower(�����ַ��Ƿ�ΪСдӢ����ĸ)
isprint(�����ַ��Ƿ�Ϊ�ɴ�ӡ�ַ�)
isspace(�����ַ��Ƿ�Ϊ�ո��ַ�)
ispunct(�����ַ��Ƿ�Ϊ�����Ż��������)
isupper(�����ַ��Ƿ�Ϊ��дӢ����ĸ)
isxdigit(�����ַ��Ƿ�Ϊ16��������)
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
    //isprint(�����ַ��Ƿ�Ϊ�ɴ�ӡ�ַ�)
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
