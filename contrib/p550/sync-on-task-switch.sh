#!/bin/bash

set -eum

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <System.map or kallsyms>" >&2
  exit 1
fi

DIR="$(readlink -f "$(dirname "$0")")"
KSYMS="$1"

sym_addr() {
  local addr
  addr="$(sed -nE "s/([0-9a-f]+) [Tt] $1\$/\1/p" "$KSYMS")"
  if [[ "$addr" == "" ]]; then
    echo "WARN: Unable to find symbol $1" >&2
    return
  fi
  echo "0x$addr"
}

openocd -f "$DIR/openocd.cfg" &

sleep 1

{
"$DIR/gencmd-sync-on-addr.sh" \
  $(sym_addr __switch_to) \
  $(sym_addr __kvm_switch_resume) \
  $(sym_addr __kvm_switch_return) ;
sleep 1
} | telnet localhost 4444 || :

fg
