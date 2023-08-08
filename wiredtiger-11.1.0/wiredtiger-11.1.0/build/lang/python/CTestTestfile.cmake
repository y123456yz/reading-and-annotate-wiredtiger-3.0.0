# CMake generated Testfile for 
# Source directory: /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/lang/python
# Build directory: /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/lang/python
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(test_ex_access "/usr/local/bin/cmake" "-E" "env" "PYTHONPATH=/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/lang/python" "/usr/bin/python3.8" "-S" "/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/examples/python/ex_access.py")
set_tests_properties(test_ex_access PROPERTIES  LABELS "check" _BACKTRACE_TRIPLES "/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/lang/python/CMakeLists.txt;184;add_test;/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/lang/python/CMakeLists.txt;0;")
