# Install script for directory: /root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/test

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "0")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/utility/cmake_install.cmake")
  include("/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/bloom/cmake_install.cmake")
  include("/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/checkpoint/cmake_install.cmake")
  include("/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/cursor_order/cmake_install.cmake")
  include("/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/fops/cmake_install.cmake")
  include("/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/huge/cmake_install.cmake")
  include("/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/manydbs/cmake_install.cmake")
  include("/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/csuite/cmake_install.cmake")
  include("/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/packing/cmake_install.cmake")
  include("/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/format/cmake_install.cmake")
  include("/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/readonly/cmake_install.cmake")
  include("/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/salvage/cmake_install.cmake")
  include("/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/syscall/cmake_install.cmake")
  include("/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/thread/cmake_install.cmake")
  include("/root/reading-and-annotate-wiredtiger-3.0.0/wiredtiger-11.1.0/wiredtiger-11.1.0/build/test/cppsuite/cmake_install.cmake")

endif()

