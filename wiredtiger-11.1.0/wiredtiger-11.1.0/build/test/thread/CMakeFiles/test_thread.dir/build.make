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
include test/thread/CMakeFiles/test_thread.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include test/thread/CMakeFiles/test_thread.dir/compiler_depend.make

# Include the progress variables for this target.
include test/thread/CMakeFiles/test_thread.dir/progress.make

# Include the compile flags for this target's objects.
include test/thread/CMakeFiles/test_thread.dir/flags.make

test/thread/CMakeFiles/test_thread.dir/file.c.o: test/thread/CMakeFiles/test_thread.dir/flags.make
test/thread/CMakeFiles/test_thread.dir/file.c.o: ../test/thread/file.c
test/thread/CMakeFiles/test_thread.dir/file.c.o: test/thread/CMakeFiles/test_thread.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object test/thread/CMakeFiles/test_thread.dir/file.c.o"
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/thread && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT test/thread/CMakeFiles/test_thread.dir/file.c.o -MF CMakeFiles/test_thread.dir/file.c.o.d -o CMakeFiles/test_thread.dir/file.c.o -c /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/test/thread/file.c

test/thread/CMakeFiles/test_thread.dir/file.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/test_thread.dir/file.c.i"
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/thread && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/test/thread/file.c > CMakeFiles/test_thread.dir/file.c.i

test/thread/CMakeFiles/test_thread.dir/file.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/test_thread.dir/file.c.s"
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/thread && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/test/thread/file.c -o CMakeFiles/test_thread.dir/file.c.s

test/thread/CMakeFiles/test_thread.dir/rw.c.o: test/thread/CMakeFiles/test_thread.dir/flags.make
test/thread/CMakeFiles/test_thread.dir/rw.c.o: ../test/thread/rw.c
test/thread/CMakeFiles/test_thread.dir/rw.c.o: test/thread/CMakeFiles/test_thread.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building C object test/thread/CMakeFiles/test_thread.dir/rw.c.o"
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/thread && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT test/thread/CMakeFiles/test_thread.dir/rw.c.o -MF CMakeFiles/test_thread.dir/rw.c.o.d -o CMakeFiles/test_thread.dir/rw.c.o -c /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/test/thread/rw.c

test/thread/CMakeFiles/test_thread.dir/rw.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/test_thread.dir/rw.c.i"
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/thread && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/test/thread/rw.c > CMakeFiles/test_thread.dir/rw.c.i

test/thread/CMakeFiles/test_thread.dir/rw.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/test_thread.dir/rw.c.s"
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/thread && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/test/thread/rw.c -o CMakeFiles/test_thread.dir/rw.c.s

test/thread/CMakeFiles/test_thread.dir/stats.c.o: test/thread/CMakeFiles/test_thread.dir/flags.make
test/thread/CMakeFiles/test_thread.dir/stats.c.o: ../test/thread/stats.c
test/thread/CMakeFiles/test_thread.dir/stats.c.o: test/thread/CMakeFiles/test_thread.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building C object test/thread/CMakeFiles/test_thread.dir/stats.c.o"
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/thread && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT test/thread/CMakeFiles/test_thread.dir/stats.c.o -MF CMakeFiles/test_thread.dir/stats.c.o.d -o CMakeFiles/test_thread.dir/stats.c.o -c /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/test/thread/stats.c

test/thread/CMakeFiles/test_thread.dir/stats.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/test_thread.dir/stats.c.i"
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/thread && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/test/thread/stats.c > CMakeFiles/test_thread.dir/stats.c.i

test/thread/CMakeFiles/test_thread.dir/stats.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/test_thread.dir/stats.c.s"
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/thread && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/test/thread/stats.c -o CMakeFiles/test_thread.dir/stats.c.s

test/thread/CMakeFiles/test_thread.dir/t.c.o: test/thread/CMakeFiles/test_thread.dir/flags.make
test/thread/CMakeFiles/test_thread.dir/t.c.o: ../test/thread/t.c
test/thread/CMakeFiles/test_thread.dir/t.c.o: test/thread/CMakeFiles/test_thread.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Building C object test/thread/CMakeFiles/test_thread.dir/t.c.o"
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/thread && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT test/thread/CMakeFiles/test_thread.dir/t.c.o -MF CMakeFiles/test_thread.dir/t.c.o.d -o CMakeFiles/test_thread.dir/t.c.o -c /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/test/thread/t.c

test/thread/CMakeFiles/test_thread.dir/t.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/test_thread.dir/t.c.i"
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/thread && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/test/thread/t.c > CMakeFiles/test_thread.dir/t.c.i

test/thread/CMakeFiles/test_thread.dir/t.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/test_thread.dir/t.c.s"
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/thread && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/test/thread/t.c -o CMakeFiles/test_thread.dir/t.c.s

# Object files for target test_thread
test_thread_OBJECTS = \
"CMakeFiles/test_thread.dir/file.c.o" \
"CMakeFiles/test_thread.dir/rw.c.o" \
"CMakeFiles/test_thread.dir/stats.c.o" \
"CMakeFiles/test_thread.dir/t.c.o"

# External object files for target test_thread
test_thread_EXTERNAL_OBJECTS =

test/thread/t: test/thread/CMakeFiles/test_thread.dir/file.c.o
test/thread/t: test/thread/CMakeFiles/test_thread.dir/rw.c.o
test/thread/t: test/thread/CMakeFiles/test_thread.dir/stats.c.o
test/thread/t: test/thread/CMakeFiles/test_thread.dir/t.c.o
test/thread/t: test/thread/CMakeFiles/test_thread.dir/build.make
test/thread/t: test/utility/libtest_util.a
test/thread/t: libwiredtiger.so.11.1.0
test/thread/t: /usr/local/lib/libtcmalloc.so
test/thread/t: /usr/lib64/libpthread.so
test/thread/t: /usr/lib64/librt.so
test/thread/t: /usr/lib64/libdl.so
test/thread/t: test/thread/CMakeFiles/test_thread.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "Linking C executable t"
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/thread && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/test_thread.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
test/thread/CMakeFiles/test_thread.dir/build: test/thread/t
.PHONY : test/thread/CMakeFiles/test_thread.dir/build

test/thread/CMakeFiles/test_thread.dir/clean:
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/thread && $(CMAKE_COMMAND) -P CMakeFiles/test_thread.dir/cmake_clean.cmake
.PHONY : test/thread/CMakeFiles/test_thread.dir/clean

test/thread/CMakeFiles/test_thread.dir/depend:
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0 /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/test/thread /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/thread /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/thread/CMakeFiles/test_thread.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : test/thread/CMakeFiles/test_thread.dir/depend
