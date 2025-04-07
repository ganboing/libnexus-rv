cmake_minimum_required(VERSION 3.5)
project(libnexus-rv C)

set(CMAKE_C_STANDARD 11)

include(GNUInstallDirs)

option(BUILD_SHARED_LIBS "Build the shared library" ON)
option(UTIL "Enable utilities" ON)

add_subdirectory(lib)

if (UTIL)
  add_subdirectory(util)
endif (UTIL)
