ACLOCAL_AMFLAGS = -I build_posix/aclocal

# BEGIN SUBDIRS, maintained by makemake and Make.subdirs
SUBDIRS =
SUBDIRS += ext/collators/reverse
SUBDIRS += ext/collators/revint
if LZ4
    SUBDIRS += ext/compressors/lz4
endif
SUBDIRS += ext/compressors/nop
if SNAPPY
    SUBDIRS += ext/compressors/snappy
endif
if ZLIB
    SUBDIRS += ext/compressors/zlib
endif
if ZSTD
    SUBDIRS += ext/compressors/zstd
endif
if HAVE_HELIUM
    SUBDIRS += ext/datasources/helium
endif
SUBDIRS += ext/encryptors/nop
SUBDIRS += ext/encryptors/rotn
SUBDIRS += ext/extractors/csv
if HAVE_BERKELEY_DB
    SUBDIRS += ext/test/kvs_bdb
endif
SUBDIRS += ext/test/fail_fs
SUBDIRS += .
if LEVELDB
    SUBDIRS += api/leveldb
endif
if JAVA
    SUBDIRS += lang/java
endif
if PYTHON
    SUBDIRS += lang/python
endif
SUBDIRS += test/utility
SUBDIRS += examples/c
if JAVA
    SUBDIRS += examples/java
endif
SUBDIRS += test/bloom
SUBDIRS += test/checkpoint
SUBDIRS += test/csuite
SUBDIRS += test/cursor_order
SUBDIRS += test/fops
SUBDIRS += test/format
SUBDIRS += test/huge
SUBDIRS += test/manydbs
SUBDIRS += test/packing
SUBDIRS += test/readonly
SUBDIRS += test/salvage
SUBDIRS += test/syscall
SUBDIRS += test/thread
if PYTHON
if HAVE_CXX
    SUBDIRS += bench/workgen
endif
endif
SUBDIRS += bench/wtperf
# END SUBDIRS

lib_LTLIBRARIES = libwiredtiger.la
LDADD = $(lib_LTLIBRARIES)

# The LevelDB libraries prefer to include all of the objects in a single
# library.  Create a convenience library for them.
if LEVELDB
noinst_LTLIBRARIES = libwiredtiger_static.la
endif

# BEGIN SOURCES, maintained by makemake and dist/filelist

libwiredtiger_la_LDFLAGS = -release @VERSION@
libwiredtiger_la_SOURCES =
libwiredtiger_la_SOURCES += src/async/async_api.c
libwiredtiger_la_SOURCES += src/async/async_op.c
libwiredtiger_la_SOURCES += src/async/async_worker.c
libwiredtiger_la_SOURCES += src/block/block_addr.c
libwiredtiger_la_SOURCES += src/block/block_ckpt.c
libwiredtiger_la_SOURCES += src/block/block_compact.c
libwiredtiger_la_SOURCES += src/block/block_ext.c
libwiredtiger_la_SOURCES += src/block/block_map.c
libwiredtiger_la_SOURCES += src/block/block_mgr.c
libwiredtiger_la_SOURCES += src/block/block_open.c
libwiredtiger_la_SOURCES += src/block/block_read.c
libwiredtiger_la_SOURCES += src/block/block_session.c
libwiredtiger_la_SOURCES += src/block/block_slvg.c
libwiredtiger_la_SOURCES += src/block/block_vrfy.c
libwiredtiger_la_SOURCES += src/block/block_write.c
libwiredtiger_la_SOURCES += src/bloom/bloom.c
libwiredtiger_la_SOURCES += src/btree/bt_compact.c
libwiredtiger_la_SOURCES += src/btree/bt_curnext.c
libwiredtiger_la_SOURCES += src/btree/bt_curprev.c
libwiredtiger_la_SOURCES += src/btree/bt_cursor.c
libwiredtiger_la_SOURCES += src/btree/bt_debug.c
libwiredtiger_la_SOURCES += src/btree/bt_delete.c
libwiredtiger_la_SOURCES += src/btree/bt_discard.c
libwiredtiger_la_SOURCES += src/btree/bt_handle.c
libwiredtiger_la_SOURCES += src/btree/bt_huffman.c
libwiredtiger_la_SOURCES += src/btree/bt_io.c
libwiredtiger_la_SOURCES += src/btree/bt_misc.c
libwiredtiger_la_SOURCES += src/btree/bt_ovfl.c
libwiredtiger_la_SOURCES += src/btree/bt_page.c
libwiredtiger_la_SOURCES += src/btree/bt_random.c
libwiredtiger_la_SOURCES += src/btree/bt_read.c
libwiredtiger_la_SOURCES += src/btree/bt_rebalance.c
libwiredtiger_la_SOURCES += src/btree/bt_ret.c
libwiredtiger_la_SOURCES += src/btree/bt_slvg.c
libwiredtiger_la_SOURCES += src/btree/bt_split.c
libwiredtiger_la_SOURCES += src/btree/bt_stat.c
libwiredtiger_la_SOURCES += src/btree/bt_sync.c
libwiredtiger_la_SOURCES += src/btree/bt_upgrade.c
libwiredtiger_la_SOURCES += src/btree/bt_vrfy.c
libwiredtiger_la_SOURCES += src/btree/bt_vrfy_dsk.c
libwiredtiger_la_SOURCES += src/btree/bt_walk.c
libwiredtiger_la_SOURCES += src/btree/col_modify.c
libwiredtiger_la_SOURCES += src/btree/col_srch.c
libwiredtiger_la_SOURCES += src/btree/row_key.c
libwiredtiger_la_SOURCES += src/btree/row_modify.c
libwiredtiger_la_SOURCES += src/btree/row_srch.c
libwiredtiger_la_SOURCES += src/cache/cache_las.c
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if ARM64_HOST
libwiredtiger_la_SOURCES += src/checksum/arm64/crc32-arm64.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if POWERPC_HOST
libwiredtiger_la_SOURCES += src/checksum/power8/crc32.sx
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if POWERPC_HOST
libwiredtiger_la_SOURCES += src/checksum/power8/crc32_wrapper.c
endif
libwiredtiger_la_SOURCES += src/checksum/software/checksum.c
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if X86_HOST
libwiredtiger_la_SOURCES += src/checksum/x86/crc32-x86.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if ZSERIES_HOST
libwiredtiger_la_SOURCES += src/checksum/zseries/crc32-s390x.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if ZSERIES_HOST
libwiredtiger_la_SOURCES += src/checksum/zseries/crc32le-vx.sx
endif
libwiredtiger_la_SOURCES += src/config/config.c
libwiredtiger_la_SOURCES += src/config/config_api.c
libwiredtiger_la_SOURCES += src/config/config_check.c
libwiredtiger_la_SOURCES += src/config/config_collapse.c
libwiredtiger_la_SOURCES += src/config/config_def.c
libwiredtiger_la_SOURCES += src/config/config_ext.c
libwiredtiger_la_SOURCES += src/config/config_upgrade.c
libwiredtiger_la_SOURCES += src/conn/api_strerror.c
libwiredtiger_la_SOURCES += src/conn/api_version.c
libwiredtiger_la_SOURCES += src/conn/conn_api.c
libwiredtiger_la_SOURCES += src/conn/conn_cache.c
libwiredtiger_la_SOURCES += src/conn/conn_cache_pool.c
libwiredtiger_la_SOURCES += src/conn/conn_ckpt.c
libwiredtiger_la_SOURCES += src/conn/conn_dhandle.c
libwiredtiger_la_SOURCES += src/conn/conn_handle.c
libwiredtiger_la_SOURCES += src/conn/conn_log.c
libwiredtiger_la_SOURCES += src/conn/conn_open.c
libwiredtiger_la_SOURCES += src/conn/conn_reconfig.c
libwiredtiger_la_SOURCES += src/conn/conn_stat.c
libwiredtiger_la_SOURCES += src/conn/conn_sweep.c
libwiredtiger_la_SOURCES += src/cursor/cur_backup.c
libwiredtiger_la_SOURCES += src/cursor/cur_bulk.c
libwiredtiger_la_SOURCES += src/cursor/cur_config.c
libwiredtiger_la_SOURCES += src/cursor/cur_ds.c
libwiredtiger_la_SOURCES += src/cursor/cur_dump.c
libwiredtiger_la_SOURCES += src/cursor/cur_file.c
libwiredtiger_la_SOURCES += src/cursor/cur_index.c
libwiredtiger_la_SOURCES += src/cursor/cur_join.c
libwiredtiger_la_SOURCES += src/cursor/cur_json.c
libwiredtiger_la_SOURCES += src/cursor/cur_log.c
libwiredtiger_la_SOURCES += src/cursor/cur_metadata.c
libwiredtiger_la_SOURCES += src/cursor/cur_stat.c
libwiredtiger_la_SOURCES += src/cursor/cur_std.c
libwiredtiger_la_SOURCES += src/cursor/cur_table.c
libwiredtiger_la_SOURCES += src/evict/evict_file.c
libwiredtiger_la_SOURCES += src/evict/evict_lru.c
libwiredtiger_la_SOURCES += src/evict/evict_page.c
libwiredtiger_la_SOURCES += src/evict/evict_stat.c
libwiredtiger_la_SOURCES += src/log/log.c
libwiredtiger_la_SOURCES += src/log/log_auto.c
libwiredtiger_la_SOURCES += src/log/log_slot.c
libwiredtiger_la_SOURCES += src/log/log_sys.c
libwiredtiger_la_SOURCES += src/lsm/lsm_cursor.c
libwiredtiger_la_SOURCES += src/lsm/lsm_cursor_bulk.c
libwiredtiger_la_SOURCES += src/lsm/lsm_manager.c
libwiredtiger_la_SOURCES += src/lsm/lsm_merge.c
libwiredtiger_la_SOURCES += src/lsm/lsm_meta.c
libwiredtiger_la_SOURCES += src/lsm/lsm_stat.c
libwiredtiger_la_SOURCES += src/lsm/lsm_tree.c
libwiredtiger_la_SOURCES += src/lsm/lsm_work_unit.c
libwiredtiger_la_SOURCES += src/lsm/lsm_worker.c
libwiredtiger_la_SOURCES += src/meta/meta_apply.c
libwiredtiger_la_SOURCES += src/meta/meta_ckpt.c
libwiredtiger_la_SOURCES += src/meta/meta_ext.c
libwiredtiger_la_SOURCES += src/meta/meta_table.c
libwiredtiger_la_SOURCES += src/meta/meta_track.c
libwiredtiger_la_SOURCES += src/meta/meta_turtle.c
libwiredtiger_la_SOURCES += src/os_common/filename.c
libwiredtiger_la_SOURCES += src/os_common/os_abort.c
libwiredtiger_la_SOURCES += src/os_common/os_alloc.c
libwiredtiger_la_SOURCES += src/os_common/os_errno.c
libwiredtiger_la_SOURCES += src/os_common/os_fhandle.c
libwiredtiger_la_SOURCES += src/os_common/os_fs_inmemory.c
libwiredtiger_la_SOURCES += src/os_common/os_fstream.c
libwiredtiger_la_SOURCES += src/os_common/os_fstream_stdio.c
libwiredtiger_la_SOURCES += src/os_common/os_getopt.c
libwiredtiger_la_SOURCES += src/os_common/os_strtouq.c
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if POSIX_HOST
libwiredtiger_la_SOURCES += src/os_posix/os_dir.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if POSIX_HOST
libwiredtiger_la_SOURCES += src/os_posix/os_dlopen.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if POSIX_HOST
libwiredtiger_la_SOURCES += src/os_posix/os_fallocate.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if POSIX_HOST
libwiredtiger_la_SOURCES += src/os_posix/os_fs.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if POSIX_HOST
libwiredtiger_la_SOURCES += src/os_posix/os_getenv.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if POSIX_HOST
libwiredtiger_la_SOURCES += src/os_posix/os_map.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if POSIX_HOST
libwiredtiger_la_SOURCES += src/os_posix/os_mtx_cond.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if POSIX_HOST
libwiredtiger_la_SOURCES += src/os_posix/os_once.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if POSIX_HOST
libwiredtiger_la_SOURCES += src/os_posix/os_pagesize.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if POSIX_HOST
libwiredtiger_la_SOURCES += src/os_posix/os_path.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if POSIX_HOST
libwiredtiger_la_SOURCES += src/os_posix/os_priv.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if POSIX_HOST
libwiredtiger_la_SOURCES += src/os_posix/os_setvbuf.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if POSIX_HOST
libwiredtiger_la_SOURCES += src/os_posix/os_sleep.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if POSIX_HOST
libwiredtiger_la_SOURCES += src/os_posix/os_snprintf.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if POSIX_HOST
libwiredtiger_la_SOURCES += src/os_posix/os_thread.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if POSIX_HOST
libwiredtiger_la_SOURCES += src/os_posix/os_time.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if POSIX_HOST
libwiredtiger_la_SOURCES += src/os_posix/os_yield.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if WINDOWS_HOST
libwiredtiger_la_SOURCES += src/os_win/os_dir.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if WINDOWS_HOST
libwiredtiger_la_SOURCES += src/os_win/os_dlopen.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if WINDOWS_HOST
libwiredtiger_la_SOURCES += src/os_win/os_fs.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if WINDOWS_HOST
libwiredtiger_la_SOURCES += src/os_win/os_getenv.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if WINDOWS_HOST
libwiredtiger_la_SOURCES += src/os_win/os_map.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if WINDOWS_HOST
libwiredtiger_la_SOURCES += src/os_win/os_mtx_cond.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if WINDOWS_HOST
libwiredtiger_la_SOURCES += src/os_win/os_once.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if WINDOWS_HOST
libwiredtiger_la_SOURCES += src/os_win/os_pagesize.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if WINDOWS_HOST
libwiredtiger_la_SOURCES += src/os_win/os_path.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if WINDOWS_HOST
libwiredtiger_la_SOURCES += src/os_win/os_priv.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if WINDOWS_HOST
libwiredtiger_la_SOURCES += src/os_win/os_setvbuf.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if WINDOWS_HOST
libwiredtiger_la_SOURCES += src/os_win/os_sleep.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if WINDOWS_HOST
libwiredtiger_la_SOURCES += src/os_win/os_snprintf.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if WINDOWS_HOST
libwiredtiger_la_SOURCES += src/os_win/os_thread.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if WINDOWS_HOST
libwiredtiger_la_SOURCES += src/os_win/os_time.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if WINDOWS_HOST
libwiredtiger_la_SOURCES += src/os_win/os_utf8.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if WINDOWS_HOST
libwiredtiger_la_SOURCES += src/os_win/os_winerr.c
endif
# DO NOT indent the "libwiredtiger_la_SOURCES" lines, it breaks the build.
if WINDOWS_HOST
libwiredtiger_la_SOURCES += src/os_win/os_yield.c
endif
libwiredtiger_la_SOURCES += src/packing/pack_api.c
libwiredtiger_la_SOURCES += src/packing/pack_impl.c
libwiredtiger_la_SOURCES += src/packing/pack_stream.c
libwiredtiger_la_SOURCES += src/reconcile/rec_track.c
libwiredtiger_la_SOURCES += src/reconcile/rec_write.c
libwiredtiger_la_SOURCES += src/schema/schema_alter.c
libwiredtiger_la_SOURCES += src/schema/schema_create.c
libwiredtiger_la_SOURCES += src/schema/schema_drop.c
libwiredtiger_la_SOURCES += src/schema/schema_list.c
libwiredtiger_la_SOURCES += src/schema/schema_open.c
libwiredtiger_la_SOURCES += src/schema/schema_plan.c
libwiredtiger_la_SOURCES += src/schema/schema_project.c
libwiredtiger_la_SOURCES += src/schema/schema_rename.c
libwiredtiger_la_SOURCES += src/schema/schema_stat.c
libwiredtiger_la_SOURCES += src/schema/schema_truncate.c
libwiredtiger_la_SOURCES += src/schema/schema_util.c
libwiredtiger_la_SOURCES += src/schema/schema_worker.c
libwiredtiger_la_SOURCES += src/session/session_api.c
libwiredtiger_la_SOURCES += src/session/session_compact.c
libwiredtiger_la_SOURCES += src/session/session_dhandle.c
libwiredtiger_la_SOURCES += src/session/session_salvage.c
libwiredtiger_la_SOURCES += src/support/cond_auto.c
libwiredtiger_la_SOURCES += src/support/crypto.c
libwiredtiger_la_SOURCES += src/support/err.c
libwiredtiger_la_SOURCES += src/support/generation.c
libwiredtiger_la_SOURCES += src/support/global.c
libwiredtiger_la_SOURCES += src/support/hash_city.c
libwiredtiger_la_SOURCES += src/support/hash_fnv.c
libwiredtiger_la_SOURCES += src/support/hazard.c
libwiredtiger_la_SOURCES += src/support/hex.c
libwiredtiger_la_SOURCES += src/support/huffman.c
libwiredtiger_la_SOURCES += src/support/modify.c
libwiredtiger_la_SOURCES += src/support/mtx_rw.c
libwiredtiger_la_SOURCES += src/support/pow.c
libwiredtiger_la_SOURCES += src/support/rand.c
libwiredtiger_la_SOURCES += src/support/scratch.c
libwiredtiger_la_SOURCES += src/support/stat.c
libwiredtiger_la_SOURCES += src/support/thread_group.c
libwiredtiger_la_SOURCES += src/support/time.c
libwiredtiger_la_SOURCES += src/txn/txn.c
libwiredtiger_la_SOURCES += src/txn/txn_ckpt.c
libwiredtiger_la_SOURCES += src/txn/txn_ext.c
libwiredtiger_la_SOURCES += src/txn/txn_log.c
libwiredtiger_la_SOURCES += src/txn/txn_nsnap.c
libwiredtiger_la_SOURCES += src/txn/txn_recover.c
libwiredtiger_la_SOURCES += src/txn/txn_rollback_to_stable.c
libwiredtiger_la_SOURCES += src/txn/txn_timestamp.c
# END SOURCES

bin_PROGRAMS = wt
wt_SOURCES =\
	src/utilities/util_alter.c \
	src/utilities/util_backup.c \
	src/utilities/util_cpyright.c \
	src/utilities/util_compact.c \
	src/utilities/util_create.c \
	src/utilities/util_downgrade.c \
	src/utilities/util_drop.c \
	src/utilities/util_dump.c \
	src/utilities/util_list.c \
	src/utilities/util_load.c \
	src/utilities/util_load_json.c \
	src/utilities/util_loadtext.c \
	src/utilities/util_main.c \
	src/utilities/util_misc.c \
	src/utilities/util_printlog.c \
	src/utilities/util_read.c \
	src/utilities/util_rebalance.c \
	src/utilities/util_rename.c \
	src/utilities/util_salvage.c \
	src/utilities/util_stat.c \
	src/utilities/util_truncate.c \
	src/utilities/util_upgrade.c \
	src/utilities/util_verbose.c \
	src/utilities/util_verify.c \
	src/utilities/util_write.c

man1_MANS = $(MAN1_PAGES)
man3_MANS = $(MAN3_PAGES)

include_HEADERS= wiredtiger.h src/include/wiredtiger_ext.h
AM_CPPFLAGS = -I$(srcdir)/src/include

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = wiredtiger.pc

$(srcdir)/Makefile.am: $(srcdir)/build_posix/Make.base $(srcdir)/build_posix/makemake $(srcdir)/dist/filelist
	@cd $(srcdir)/build_posix && sh makemake

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status libtool

$(srcdir)/src/include/extern.h: auto-includes.chk
$(srcdir)/src/include/wt_internal.h: auto-includes.chk

auto-includes.chk: $(libwiredtiger_la_SOURCES)
	@(cd $(srcdir)/dist && sh s_prototypes && sh s_typedef -b) && touch $@

$(srcdir)/docs/index.html:
	@cd $(srcdir)/dist && sh s_docs

$(srcdir)/docs/man/man1/wt.1: $(srcdir)/docs/index.html
$(srcdir)/docs/man/man3/wiredtiger.3: $(srcdir)/docs/index.html

libwiredtiger_la_LIBADD =
if HAVE_BUILTIN_EXTENSION_LZ4
libwiredtiger_la_LIBADD += ext/compressors/lz4/libwiredtiger_lz4.la
endif
if HAVE_BUILTIN_EXTENSION_SNAPPY
libwiredtiger_la_LIBADD += ext/compressors/snappy/libwiredtiger_snappy.la
endif
if HAVE_BUILTIN_EXTENSION_ZLIB
libwiredtiger_la_LIBADD += ext/compressors/zlib/libwiredtiger_zlib.la
endif
if HAVE_BUILTIN_EXTENSION_ZSTD
libwiredtiger_la_LIBADD += ext/compressors/zstd/libwiredtiger_zstd.la
endif

libwiredtiger_static_la_LIBADD=$(libwiredtiger_la_LIBADD)
libwiredtiger_static_la_SOURCES=$(libwiredtiger_la_SOURCES)

clean-local:
	rm -rf WT_TEST
