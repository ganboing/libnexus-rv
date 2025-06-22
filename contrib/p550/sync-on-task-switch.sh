#!/bin/bash

set -eu

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <System.map or kallsyms>" >&2
  exit 1
fi

DIR="$(readlink -f "$(dirname "$0")")"
KSYMS="$1"

sym_addr() {
  local addr
  addr="$(sed -nE "s/([0-9a-f]+) T $1\$/\1/p" "$KSYMS")"
  if [[ "$addr" == "" ]]; then
    echo "Unable to find symbol $1" >&2
    return 1
  fi
  echo "0x$addr"
}

exec "$DIR/trace-sync-on-addr.sh" "$(sym_addr __switch_to)"
