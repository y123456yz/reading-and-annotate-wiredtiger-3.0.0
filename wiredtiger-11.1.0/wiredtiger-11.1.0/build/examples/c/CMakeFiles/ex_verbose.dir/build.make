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
include examples/c/CMakeFiles/ex_verbose.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include examples/c/CMakeFiles/ex_verbose.dir/compiler_depend.make

# Include the progress variables for this target.
include examples/c/CMakeFiles/ex_verbose.dir/progress.make

# Include the compile flags for this target's objects.
include examples/c/CMakeFiles/ex_verbose.dir/flags.make

examples/c/CMakeFiles/ex_verbose.dir/ex_verbose.c.o: examples/c/CMakeFiles/ex_verbose.dir/flags.make
examples/c/CMakeFiles/ex_verbose.dir/ex_verbose.c.o: ../examples/c/ex_verbose.c
examples/c/CMakeFiles/ex_verbose.dir/ex_verbose.c.o: examples/c/CMakeFiles/ex_verbose.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object examples/c/CMakeFiles/ex_verbose.dir/ex_verbose.c.o"
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/examples/c && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT examples/c/CMakeFiles/ex_verbose.dir/ex_verbose.c.o -MF CMakeFiles/ex_verbose.dir/ex_verbose.c.o.d -o CMakeFiles/ex_verbose.dir/ex_verbose.c.o -c /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/examples/c/ex_verbose.c

examples/c/CMakeFiles/ex_verbose.dir/ex_verbose.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/ex_verbose.dir/ex_verbose.c.i"
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/examples/c && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/examples/c/ex_verbose.c > CMakeFiles/ex_verbose.dir/ex_verbose.c.i

examples/c/CMakeFiles/ex_verbose.dir/ex_verbose.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/ex_verbose.dir/ex_verbose.c.s"
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/examples/c && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/examples/c/ex_verbose.c -o CMakeFiles/ex_verbose.dir/ex_verbose.c.s

# Object files for target ex_verbose
ex_verbose_OBJECTS = \
"CMakeFiles/ex_verbose.dir/ex_verbose.c.o"

# External object files for target ex_verbose
ex_verbose_EXTERNAL_OBJECTS =

examples/c/ex_verbose/ex_verbose: examples/c/CMakeFiles/ex_verbose.dir/ex_verbose.c.o
examples/c/ex_verbose/ex_verbose: examples/c/CMakeFiles/ex_verbose.dir/build.make
examples/c/ex_verbose/ex_verbose: test/utility/libtest_util.a
examples/c/ex_verbose/ex_verbose: libwiredtiger.so.11.1.0
examples/c/ex_verbose/ex_verbose: /usr/local/lib/libtcmalloc.so
examples/c/ex_verbose/ex_verbose: /usr/lib64/libpthread.so
examples/c/ex_verbose/ex_verbose: /usr/lib64/librt.so
examples/c/ex_verbose/ex_verbose: /usr/lib64/libdl.so
examples/c/ex_verbose/ex_verbose: examples/c/CMakeFiles/ex_verbose.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C executable ex_verbose/ex_verbose"
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/examples/c && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/ex_verbose.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
examples/c/CMakeFiles/ex_verbose.dir/build: examples/c/ex_verbose/ex_verbose
.PHONY : examples/c/CMakeFiles/ex_verbose.dir/build

examples/c/CMakeFiles/ex_verbose.dir/clean:
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/examples/c && $(CMAKE_COMMAND) -P CMakeFiles/ex_verbose.dir/cmake_clean.cmake
.PHONY : examples/c/CMakeFiles/ex_verbose.dir/clean

examples/c/CMakeFiles/ex_verbose.dir/depend:
	cd /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0 /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/examples/c /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/examples/c /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/examples/c/CMakeFiles/ex_verbose.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : examples/c/CMakeFiles/ex_verbose.dir/depend
