#!/bin/bash

quote_args() {
  printf "%q " "$@"
}

CXX_DEBUG_OPTS="-D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC"
ASAN_OPTS="-fsanitize=address"

declare -A CMAKE_BUILD_OPTS
CMAKE_BUILD_OPTS["Debug"]="$(quote_args "-DCMAKE_BUILD_TYPE=Debug" \
  "-DCMAKE_CXX_FLAGS=$CXX_DEBUG_OPTS" \
)"
CMAKE_BUILD_OPTS["DebugAsan"]="$(quote_args "-DCMAKE_BUILD_TYPE=Debug" \
  "-DCMAKE_C_FLAGS=$ASAN_OPTS" \
  "-DCMAKE_CXX_FLAGS=$ASAN_OPTS $CXX_DEBUG_OPTS" \
)"
CMAKE_BUILD_OPTS["Release"]="$(quote_args "-DCMAKE_BUILD_TYPE=Release" \
)"
CMAKE_BUILD_OPTS["RelAsan"]="$(quote_args "-DCMAKE_BUILD_TYPE=Release" \
  "-DCMAKE_C_FLAGS=$ASAN_OPTS" \
  "-DCMAKE_CXX_FLAGS=$ASAN_OPTS" \
)"

BUILDTYPE="${BUILDTYPE:-Debug}"

while getopts hp:t: opt
do
  case "$opt" in
    p)
      INSTALLROOT="$OPTARG"
      ;;
    t)
      BUILDTYPE="$OPTARG"
      ;;
    *)
      printf "Usage: %s [-p PREFIX] [-t BUILDTYPE]\n" "$0"
      exit 1
      ;;
  esac
done

INSTALLROOT="${INSTALLROOT:-"/opt/libnexus-rv"}"
