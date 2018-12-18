/*-
 * Copyright (c) 2014-2017 MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

/*
 * __metadata_turtle --
 *	Return if a key's value should be taken from the turtle file.
 */
static bool
__metadata_turtle(const char *key) 
{//file:WiredTiger.wt和WiredTiger version对应的元数据存储在WiredTiger.turtle文件
	switch (key[0]) {
	case 'f':
		if (strcmp(key, WT_METAFILE_URI) == 0) //"file:WiredTiger.wt"
			return (true);
		break;
	case 'W': //获取版本信息
		if (strcmp(key, "WiredTiger version") == 0)
			return (true);
		if (strcmp(key, "WiredTiger version string") == 0)
			return (true);
		break;
	}
	return (false);
}

/*
 * __wt_metadata_cursor_open --
 *	Opens a cursor on the metadata.
 */
 //获取一个cursor,通过cursorp返回  
 //获取一个file:WiredTiger.wt元数据文件对应的cursor，这里面存储有所有table的元数据
int
__wt_metadata_cursor_open(
    WT_SESSION_IMPL *session, const char *config, WT_CURSOR **cursorp)
{
	WT_BTREE *btree;
	WT_DECL_RET;

	/*
   	{ "WT_SESSION.open_cursor",
	  "append=false,bulk=false,checkpoint=,checkpoint_wait=true,dump=,"
	  "next_random=false,next_random_sample_size=0,overwrite=true,"
	  "raw=false,readonly=false,skip_sort_check=false,statistics=,"
	  "target=",
	  confchk_WT_SESSION_open_cursor, 13
	},
	*/
	const char *open_cursor_cfg[] = { //WT_CONFIG_ENTRY_WT_SESSION_open_cursor配置
	    WT_CONFIG_BASE(session, WT_SESSION_open_cursor), config, NULL };

	WT_WITHOUT_DHANDLE(session, ret = __wt_open_cursor(
	    session, WT_METAFILE_URI, NULL, open_cursor_cfg, cursorp));
	WT_RET(ret);

	/*
	 * Retrieve the btree from the cursor, rather than the session because
	 * we don't always switch the metadata handle in to the session before
	 * entering this function.
	 */
	btree = ((WT_CURSOR_BTREE *)(*cursorp))->btree;

	/*
	 * Special settings for metadata: skew eviction so metadata almost
	 * always stays in cache and make sure metadata is logged if possible.
	 *
	 * Test before setting so updates can't race in subsequent opens (the
	 * first update is safe because it's single-threaded from
	 * wiredtiger_open).
	 */
#define	WT_EVICT_META_SKEW	10000
	if (btree->evict_priority == 0)
		WT_WITH_BTREE(session, btree,
		    __wt_evict_priority_set(session, WT_EVICT_META_SKEW));
	if (F_ISSET(btree, WT_BTREE_NO_LOGGING))
		F_CLR(btree, WT_BTREE_NO_LOGGING);

	return (0);
}

/*
 * __wt_metadata_cursor --
 *	Returns the session's cached metadata cursor, unless it's in use, in
 * which case it opens and returns another metadata cursor.
 */

//__wt_turtle_read从WiredTiger.turtle文件中获取WT_METAFILE_URI或者WiredTiger version的元数据，
//__wt_metadata_cursor->cursor->search(cursor))从WiredTiger.wt文件获取key对应的元数据

/*打开一个meta cursor， 通过cursorp返回并记录到session->meta_cursor */
//获取一个file:WiredTiger.wt元数据文件对应的cursor，这里面存储有所有table的元数据
int
__wt_metadata_cursor(WT_SESSION_IMPL *session, WT_CURSOR **cursorp)
{
	WT_CURSOR *cursor;

	/*
	 * If we don't have a cached metadata cursor, or it's already in use,
	 * we'll need to open a new one.
	 */
	cursor = NULL;
	//没有meta_cursor或者在使用中，则重写打开一个新的meta_cursor
	if (session->meta_cursor == NULL ||
	    F_ISSET(session->meta_cursor, WT_CURSTD_META_INUSE)) {
	    //获取一个file:WiredTiger.wt元数据文件对应的cursor，这里面存储有所有table的元数据
		WT_RET(__wt_metadata_cursor_open(session, NULL, &cursor));
		if (session->meta_cursor == NULL) {
			session->meta_cursor = cursor;
			cursor = NULL;
		}
	}

	/*
	 * If there's no cursor return, we're done, our caller should have just
	 * been triggering the creation of the session's cached cursor. There
	 * should not be an open local cursor in that case, but caution doesn't
	 * cost anything.
	 */
	if (cursorp == NULL)
		return (cursor == NULL ? 0 : cursor->close(cursor));

	/*
	 * If the cached cursor is in use, return the newly opened cursor, else
	 * mark the cached cursor in use and return it.
	 */
	if (F_ISSET(session->meta_cursor, WT_CURSTD_META_INUSE))
		*cursorp = cursor;
	else {
		*cursorp = session->meta_cursor;
		F_SET(session->meta_cursor, WT_CURSTD_META_INUSE);
	}
	return (0);
}

/*
 * __wt_metadata_cursor_release --
 *	Release a metadata cursor.
 */
int
__wt_metadata_cursor_release(WT_SESSION_IMPL *session, WT_CURSOR **cursorp)
{
	WT_CURSOR *cursor;

	WT_UNUSED(session);

	if ((cursor = *cursorp) == NULL)
		return (0);
	*cursorp = NULL;

	/*
	 * If using the session's cached metadata cursor, clear the in-use flag
	 * and reset it, otherwise, discard the cursor.
	 */
	if (F_ISSET(cursor, WT_CURSTD_META_INUSE)) {
		WT_ASSERT(session, cursor == session->meta_cursor);

		F_CLR(cursor, WT_CURSTD_META_INUSE);
		return (cursor->reset(cursor));
	}
	return (cursor->close(cursor));
}

/*
 * __wt_metadata_insert --
 *	Insert a row into the metadata.
 */

/*插入一个meta key/value对到meta中*/
//把key value写入到WiredTiger.wt元数据文件
int
__wt_metadata_insert(
    WT_SESSION_IMPL *session, const char *key, const char *value)
{
	WT_CURSOR *cursor;
	WT_DECL_RET;

	__wt_verbose(session, WT_VERB_METADATA,
	    "Insert: key: %s, value: %s, tracking: %s, %s" "turtle",
	    key, value, WT_META_TRACKING(session) ? "true" : "false",
	    __metadata_turtle(key) ? "" : "not ");

	if (__metadata_turtle(key))
		WT_RET_MSG(session, EINVAL,
		    "%s: insert not supported on the turtle file", key);

	WT_RET(__wt_metadata_cursor(session, &cursor));

	//__wt_cursor_set_keyv  填充key到cursor->key
	cursor->set_key(cursor, key);
	//__wt_cursor_set_valuev 填充value到cursor->value
	cursor->set_value(cursor, value);
	//__curfile_insert
	WT_ERR(cursor->insert(cursor));
	if (WT_META_TRACKING(session))
		WT_ERR(__wt_meta_track_insert(session, key));
err:	WT_TRET(__wt_metadata_cursor_release(session, &cursor));
	return (ret);
}

/*
 * __wt_metadata_update --
 *	Update a row in the metadata.
 */
int
__wt_metadata_update(
    WT_SESSION_IMPL *session, const char *key, const char *value)
{
	WT_CURSOR *cursor;
	WT_DECL_RET;

	__wt_verbose(session, WT_VERB_METADATA,
	    "Update: key: %s, value: %s, tracking: %s, %s" "turtle",
	    key, value, WT_META_TRACKING(session) ? "true" : "false",
	    __metadata_turtle(key) ? "" : "not ");

	if (__metadata_turtle(key)) {
		WT_WITH_TURTLE_LOCK(session,
		    ret = __wt_turtle_update(session, key, value));
		return (ret);
	}

	if (WT_META_TRACKING(session))
		WT_RET(__wt_meta_track_update(session, key));

	WT_RET(__wt_metadata_cursor(session, &cursor));
	/* This cursor needs to have overwrite semantics. */
	WT_ASSERT(session, F_ISSET(cursor, WT_CURSTD_OVERWRITE));

	cursor->set_key(cursor, key);
	cursor->set_value(cursor, value);
	WT_ERR(cursor->insert(cursor));
err:	WT_TRET(__wt_metadata_cursor_release(session, &cursor));
	return (ret);
}

/*
 * __wt_metadata_remove --
 *	Remove a row from the metadata.
 */
int
__wt_metadata_remove(WT_SESSION_IMPL *session, const char *key)
{
	WT_CURSOR *cursor;
	WT_DECL_RET;

	__wt_verbose(session, WT_VERB_METADATA,
	    "Remove: key: %s, tracking: %s, %s" "turtle",
	    key, WT_META_TRACKING(session) ? "true" : "false",
	    __metadata_turtle(key) ? "" : "not ");

	if (__metadata_turtle(key))
		WT_RET_MSG(session, EINVAL,
		    "%s: remove not supported on the turtle file", key);

	/*
	 * Take, release, and reacquire the metadata cursor. It's complicated,
	 * but that way the underlying meta-tracking function doesn't have to
	 * open a second metadata cursor, it can use the session's cached one.
	 */
	WT_RET(__wt_metadata_cursor(session, &cursor));
	cursor->set_key(cursor, key);
	WT_ERR(cursor->search(cursor));
	WT_ERR(__wt_metadata_cursor_release(session, &cursor));

	if (WT_META_TRACKING(session))
		WT_ERR(__wt_meta_track_update(session, key));

	WT_ERR(__wt_metadata_cursor(session, &cursor));
	cursor->set_key(cursor, key);
	ret = cursor->remove(cursor);

err:	WT_TRET(__wt_metadata_cursor_release(session, &cursor));
	return (ret);
}

/*
 * __wt_metadata_search --
 *	Return a copied row from the metadata.
 *	The caller is responsible for freeing the allocated memory.
WiredTiger.basecfg存储基本配置信息
WiredTiger.lock用于防止多个进程连接同一个Wiredtiger数据库
table*.wt存储各个tale（数据库中的表）的数据
WiredTiger.wt是特殊的table，用于存储所有其他table的元数据信息
WiredTiger.turtle存储WiredTiger.wt的元数据信息
journal存储Write ahead log
 */
//__wt_turtle_read从WiredTiger.turtle文件中获取WT_METAFILE_URI或者WiredTiger version的元数据，
//__wt_metadata_cursor->cursor->search(cursor))从WiredTiger.wt文件获取key对应的元数据
//获取WT_METAFILE_URI WiredTiger version，或者table的元数据,通过valuep返回
int
__wt_metadata_search(WT_SESSION_IMPL *session, const char *key, char **valuep)
{
	WT_CURSOR *cursor;
	WT_DECL_RET;
	const char *value;

	*valuep = NULL;

    //WT_SESSION.create: Search: key: table:access, tracking: true, not turtle
	__wt_verbose(session, WT_VERB_METADATA,
	    "Search: key: %s, tracking: %s, %s" "turtle",
	    key, WT_META_TRACKING(session) ? "true" : "false",
	    __metadata_turtle(key) ? "" : "not ");

	if (__metadata_turtle(key)) { 
	//file:WiredTiger.wt和WiredTiger version对应的元数据存储在WiredTiger.turtle文件，从WiredTiger.turtle中获取
		/*
		 * The returned value should only be set if ret is non-zero, but
		 * Coverity is convinced otherwise. The code path is used enough
		 * that Coverity complains a lot, add an error check to get some
		 * peace and quiet.
		 */ 
		 /*读取turtle "WiredTiger.turtle"文件,并找到key对应的value值返回， 返回内容填充到valuep中
           WiredTiger.turtle存储WiredTiger.wt的元数据信息，也就是查找是否有WiredTiger.wt配置信息
		 */ 
		//key默认为WT_METAFILE_URI，或者WiredTiger version字符串,因为WT_METAFILE_URI和"WiredTiger version"的元数据在WiredTiger.turtle文件中
		WT_WITH_TURTLE_LOCK(session,
		//__wt_turtle_read从WiredTiger.turtle文件中获取WT_METAFILE_URI或者WiredTiger version的元数据，
		//__wt_metadata_cursor->cursor->search(cursor))从WiredTiger.wt文件获取key对应的元数据
		    ret = __wt_turtle_read(session, key, valuep));
		if (ret != 0)
			__wt_free(session, *valuep);
		return (ret);
	}

    //普通table的元数据存储在WiredTiger.wt，从WiredTiger.wt中获取
	/*
	 * All metadata reads are at read-uncommitted isolation.  That's
	 * because once a schema-level operation completes, subsequent
	 * operations must see the current version of checkpoint metadata, or
	 * they may try to read blocks that may have been freed from a file.
	 * Metadata updates use non-transactional techniques (such as the
	 * schema and metadata locks) to protect access to in-flight updates.
	 */
	//__wt_turtle_read从WiredTiger.turtle文件中获取WT_METAFILE_URI或者WiredTiger version的元数据，
	//__wt_metadata_cursor->cursor->search(cursor))从WiredTiger.wt文件获取key对应的元数据
	//获取一个file:WiredTiger.wt元数据文件对应的cursor，这里面存储有所有table的元数据
	WT_RET(__wt_metadata_cursor(session, &cursor));
	cursor->set_key(cursor, key);
	WT_WITH_TXN_ISOLATION(session, WT_ISO_READ_UNCOMMITTED,
	    ret = cursor->search(cursor));
	WT_ERR(ret);

	WT_ERR(cursor->get_value(cursor, &value));
	WT_ERR(__wt_strdup(session, value, valuep));

err:	WT_TRET(__wt_metadata_cursor_release(session, &cursor));

	if (ret != 0)
		__wt_free(session, *valuep);
	return (ret);
}

