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
include bench/wt2853_perf/CMakeFiles/test_wt2853_perf.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include bench/wt2853_perf/CMakeFiles/test_wt2853_perf.dir/compiler_depend.make

# Include the progress variables for this target.
include bench/wt2853_perf/CMakeFiles/test_wt2853_perf.dir/progress.make

# Include the compile flags for this target's objects.
include bench/wt2853_perf/CMakeFiles/test_wt2853_perf.dir/flags.make

bench/wt2853_perf/CMakeFiles/test_wt2853_perf.dir/main.c.o: bench/wt2853_perf/CMakeFiles/test_wt2853_perf.dir/flags.make
bench/wt2853_perf/CMakeFiles/test_wt2853_perf.dir/main.c.o: ../bench/wt2853_perf/main.c
bench/wt2853_perf/CMakeFiles/test_wt2853_perf.dir/main.c.o: bench/wt2853_perf/CMakeFiles/test_wt2853_perf.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object bench/wt2853_perf/CMakeFiles/test_wt2853_perf.dir/main.c.o"
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/bench/wt2853_perf && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT bench/wt2853_perf/CMakeFiles/test_wt2853_perf.dir/main.c.o -MF CMakeFiles/test_wt2853_perf.dir/main.c.o.d -o CMakeFiles/test_wt2853_perf.dir/main.c.o -c /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/bench/wt2853_perf/main.c

bench/wt2853_perf/CMakeFiles/test_wt2853_perf.dir/main.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/test_wt2853_perf.dir/main.c.i"
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/bench/wt2853_perf && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/bench/wt2853_perf/main.c > CMakeFiles/test_wt2853_perf.dir/main.c.i

bench/wt2853_perf/CMakeFiles/test_wt2853_perf.dir/main.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/test_wt2853_perf.dir/main.c.s"
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/bench/wt2853_perf && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/bench/wt2853_perf/main.c -o CMakeFiles/test_wt2853_perf.dir/main.c.s

# Object files for target test_wt2853_perf
test_wt2853_perf_OBJECTS = \
"CMakeFiles/test_wt2853_perf.dir/main.c.o"

# External object files for target test_wt2853_perf
test_wt2853_perf_EXTERNAL_OBJECTS =

bench/wt2853_perf/test_wt2853_perf: bench/wt2853_perf/CMakeFiles/test_wt2853_perf.dir/main.c.o
bench/wt2853_perf/test_wt2853_perf: bench/wt2853_perf/CMakeFiles/test_wt2853_perf.dir/build.make
bench/wt2853_perf/test_wt2853_perf: test/utility/libtest_util.a
bench/wt2853_perf/test_wt2853_perf: libwiredtiger.so.11.1.0
bench/wt2853_perf/test_wt2853_perf: /usr/local/lib/libtcmalloc.so
bench/wt2853_perf/test_wt2853_perf: /usr/lib64/libpthread.so
bench/wt2853_perf/test_wt2853_perf: /usr/lib64/librt.so
bench/wt2853_perf/test_wt2853_perf: /usr/lib64/libdl.so
bench/wt2853_perf/test_wt2853_perf: bench/wt2853_perf/CMakeFiles/test_wt2853_perf.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C executable test_wt2853_perf"
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/bench/wt2853_perf && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/test_wt2853_perf.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
bench/wt2853_perf/CMakeFiles/test_wt2853_perf.dir/build: bench/wt2853_perf/test_wt2853_perf
.PHONY : bench/wt2853_perf/CMakeFiles/test_wt2853_perf.dir/build

bench/wt2853_perf/CMakeFiles/test_wt2853_perf.dir/clean:
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/bench/wt2853_perf && $(CMAKE_COMMAND) -P CMakeFiles/test_wt2853_perf.dir/cmake_clean.cmake
.PHONY : bench/wt2853_perf/CMakeFiles/test_wt2853_perf.dir/clean

bench/wt2853_perf/CMakeFiles/test_wt2853_perf.dir/depend:
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0 /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/bench/wt2853_perf /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/bench/wt2853_perf /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/bench/wt2853_perf/CMakeFiles/test_wt2853_perf.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : bench/wt2853_perf/CMakeFiles/test_wt2853_perf.dir/depend
