#!/bin/sh
# Build Wired Tiger Level DB benchmark.
# Assumes that a pre-built Wired Tiger library exists in ../wiredtiger.

WTOPT_PATH="../wiredtiger/build_posix"
WTDBG_PATH="../wiredtiger.dbg/build_posix"
BDB_PATH="../db-5.3.21/build_unix"
BASHO_PATH="../basho.leveldb"
MDB_PATH="../mdb/libraries/liblmdb"
SNAPPY_PATH="ext/compressors/snappy/.libs"

make && make db_bench
echo "Making DB-specific benchmarks"
if test -f doc/bench/db_bench_wiredtiger.cc; then
    #
    # If the sources have a sleep in it, then we're profiling.  Use
    # the debug library so functions are not inlined.
    #
    grep -q sleep doc/bench/db_bench_wiredtiger.cc
    if test "$?" -eq 0; then
	    WT_PATH=$WTDBG_PATH
    else
	    WT_PATH=$WTOPT_PATH
	    echo "Making SYMAS configured WT benchmark into db_bench_wtsymas"
	    rm -f doc/bench/db_bench_wiredtiger.o
	    env LDFLAGS="-L$WT_PATH/.libs" CXXFLAGS="-I$WT_PATH -DSYMAS_CONFIG" make db_bench_wiredtiger
	    mv db_bench_wiredtiger db_bench_wtsymas
    fi
    rm -f doc/bench/db_bench_wiredtiger.o
    echo "Making standard WT benchmark"
    env LDFLAGS="-L$WT_PATH/.libs" CXXFLAGS="-I$WT_PATH" make db_bench_wiredtiger
fi

if test -e $BASHO_PATH; then
    echo "Making Leveldb benchmark with Basho LevelDB library"
    #
    # We need to actually make the benchmarks in the Basho leveldb tree
    # in order for it to properly pick up all the right Basho files.
    #
    (cd $BASHO_PATH; make && make db_bench && make db_bench_leveldb)
    if test -e $BASHO_PATH/db_bench; then
        mv $BASHO_PATH/db_bench ./db_bench_basho
    else
	echo "db_bench did not build in Basho tree."
    fi
    if test -e $BASHO_PATH/db_bench_leveldb; then
        mv $BASHO_PATH/db_bench_leveldb ./db_bench_bashosymas
    else
	echo "db_bench_leveldb did not build in Basho tree."
    fi
fi

if test -f doc/bench/db_bench_bdb.cc; then
    echo "Making SYMAS configured BerkeleyDB benchmark into db_bench_bdbsymas"
    rm -f doc/bench/db_bench_bdb.o
    env LDFLAGS="-L$BDB_PATH/.libs" CXXFLAGS="-I$BDB_PATH -DSYMAS_CONFIG" make db_bench_bdb
    mv db_bench_bdb db_bench_bdbsymas
    rm -f doc/bench/db_bench_bdb.o
    echo "Making standard BerkeleyDB benchmark"
    env LDFLAGS="-L$BDB_PATH/.libs" CXXFLAGS="-I$BDB_PATH" make db_bench_bdb
fi

if test -f doc/bench/db_bench_leveldb.cc; then
    echo "Making SYMAS configured LevelDB benchmark"
    make db_bench_leveldb
fi

if test -e $MDB_PATH; then
    (cd $MDB_PATH; make)
    echo "Making SYMAS configured MDB benchmark into db_bench_mdbsymas"
    rm -f doc/bench/db_bench_mdb.o
    env LDFLAGS="-L$MDB_PATH" CXXFLAGS="-I$MDB_PATH -DSYMAS_CONFIG" make db_bench_mdb
    mv db_bench_mdb db_bench_mdbsymas
    rm -f doc/bench/db_bench_mdb.o
    echo "Making standard MDB benchmark"
    env LDFLAGS="-L$MDB_PATH" CXXFLAGS="-I$MDB_PATH" make db_bench_mdb
fi
