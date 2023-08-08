# CMake generated Testfile for 
# Source directory: /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/bench/wtperf
# Build directory: /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/bench/wtperf
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(test_wtperf_small_lsm "/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/bench/wtperf/wtperf" "-O" "/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/bench/wtperf/runners/small-lsm.wtperf" "-o" "run_time=20")
set_tests_properties(test_wtperf_small_lsm PROPERTIES  LABELS "check;wtperf" WORKING_DIRECTORY "/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/bench/wtperf/test_wtperf_small_lsm_test_dir" _BACKTRACE_TRIPLES "/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/test/ctest_helpers.cmake;196;add_test;/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/bench/wtperf/CMakeLists.txt;38;define_test_variants;/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/bench/wtperf/CMakeLists.txt;0;")
