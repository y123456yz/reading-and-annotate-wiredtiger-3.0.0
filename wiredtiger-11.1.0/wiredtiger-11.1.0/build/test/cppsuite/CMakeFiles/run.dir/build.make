# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.21

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/local/bin/cmake

# The command to remove a file.
RM = /usr/local/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build

# Include any dependencies generated for this target.
include test/cppsuite/CMakeFiles/run.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include test/cppsuite/CMakeFiles/run.dir/compiler_depend.make

# Include the progress variables for this target.
include test/cppsuite/CMakeFiles/run.dir/progress.make

# Include the compile flags for this target's objects.
include test/cppsuite/CMakeFiles/run.dir/flags.make

test/cppsuite/CMakeFiles/run.dir/tests/run.cpp.o: test/cppsuite/CMakeFiles/run.dir/flags.make
test/cppsuite/CMakeFiles/run.dir/tests/run.cpp.o: ../test/cppsuite/tests/run.cpp
test/cppsuite/CMakeFiles/run.dir/tests/run.cpp.o: test/cppsuite/CMakeFiles/run.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object test/cppsuite/CMakeFiles/run.dir/tests/run.cpp.o"
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/cppsuite && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT test/cppsuite/CMakeFiles/run.dir/tests/run.cpp.o -MF CMakeFiles/run.dir/tests/run.cpp.o.d -o CMakeFiles/run.dir/tests/run.cpp.o -c /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/test/cppsuite/tests/run.cpp

test/cppsuite/CMakeFiles/run.dir/tests/run.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/run.dir/tests/run.cpp.i"
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/cppsuite && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/test/cppsuite/tests/run.cpp > CMakeFiles/run.dir/tests/run.cpp.i

test/cppsuite/CMakeFiles/run.dir/tests/run.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/run.dir/tests/run.cpp.s"
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/cppsuite && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/test/cppsuite/tests/run.cpp -o CMakeFiles/run.dir/tests/run.cpp.s

# Object files for target run
run_OBJECTS = \
"CMakeFiles/run.dir/tests/run.cpp.o"

# External object files for target run
run_EXTERNAL_OBJECTS =

test/cppsuite/run: test/cppsuite/CMakeFiles/run.dir/tests/run.cpp.o
test/cppsuite/run: test/cppsuite/CMakeFiles/run.dir/build.make
test/cppsuite/run: test/utility/libtest_util.a
test/cppsuite/run: test/cppsuite/libcppsuite_test_harness.a
test/cppsuite/run: /usr/local/lib/libtcmalloc.so
test/cppsuite/run: test/utility/libtest_util.a
test/cppsuite/run: libwiredtiger.so.11.1.0
test/cppsuite/run: /usr/lib64/libpthread.so
test/cppsuite/run: /usr/lib64/librt.so
test/cppsuite/run: /usr/lib64/libdl.so
test/cppsuite/run: test/cppsuite/CMakeFiles/run.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable run"
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/cppsuite && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/run.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
test/cppsuite/CMakeFiles/run.dir/build: test/cppsuite/run
.PHONY : test/cppsuite/CMakeFiles/run.dir/build

test/cppsuite/CMakeFiles/run.dir/clean:
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/cppsuite && $(CMAKE_COMMAND) -P CMakeFiles/run.dir/cmake_clean.cmake
.PHONY : test/cppsuite/CMakeFiles/run.dir/clean

test/cppsuite/CMakeFiles/run.dir/depend:
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0 /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/test/cppsuite /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/cppsuite /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/cppsuite/CMakeFiles/run.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : test/cppsuite/CMakeFiles/run.dir/depend
