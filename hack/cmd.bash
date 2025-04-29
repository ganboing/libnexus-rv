#!/bin/bash

run_cmd_notrack() {
  echo -e "\e[32mRUN " "$(printf "%q " "$@")" "\e[39m" >&2
  "$@"
}

run_cmd(){
  local retval=0
  run_cmd_notrack "$@" || retval=$?
  if [ $retval -ne 0 ] ; then
    echo -e "\e[31mFAILED " "$(printf "%q " "$@")" "\e[39m" >&2
  fi
  return $retval
}
