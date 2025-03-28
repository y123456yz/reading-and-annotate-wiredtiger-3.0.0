/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "util.h"

/*
 * usage --
 *     Display a usage message for the compact command.
 */
static int
usage(void)
{
    util_usage("compact uri", NULL, NULL);
    return (1);
}

/*
 * util_compact --
 *     The compact command.
 /data/mongodb-5.0-danjiedian-data/wt -R  -C "log=(enabled=true,path=journal,compressor=snappy)" compact file:test/collection/28--3692507698239099283.wt
 /data/mongodb-5.0-danjiedian-data/wt -R  -C "log=(enabled=true,path=journal,compressor=snappy)" compact table:test/collection/28--3692507698239099283
*/
int
util_compact(WT_SESSION *session, int argc, char *argv[])
{
    WT_DECL_RET;
    int ch;
    char *uri;

    uri = NULL;
    while ((ch = __wt_getopt(progname, argc, argv, "")) != EOF)
        switch (ch) {
        case '?':
        default:
            return (usage());
        }
    argc -= __wt_optind;
    argv += __wt_optind;

    /* The remaining argument is the table name. */
    if (argc != 1)
        return (usage());
    if ((uri = util_uri(session, *argv, "table")) == NULL)
        return (1);
    
    if ((ret = session->compact(session, uri, NULL)) != 0)
        (void)util_err(session, ret, "session.compact: %s", uri);

    free(uri);
    return (ret);
}
