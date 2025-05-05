cmake_minimum_required(VERSION 3.5)
project(libnexus-rv C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-strict-aliasing -frtti -Wall -Wextra")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-strict-aliasing -Wall -Wextra")

include(GNUInstallDirs)

option(BUILD_SHARED_LIBS "Build the shared library" ON)
option(UTIL "Enable utilities" ON)
option(DOCS "Enable Documentation" OFF)

if (DOCS)
  find_package(Doxygen REQUIRED dot)
  set(DOXYGEN_USE_MDFILE_AS_MAINPAGE README.md)
  doxygen_add_docs(docs_html include README.md)
endif(DOCS)

add_subdirectory(lib)

if (UTIL)
  add_subdirectory(util)
endif (UTIL)
