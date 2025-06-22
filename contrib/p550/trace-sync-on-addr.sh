#!/bin/bash

set -eu

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <ADDR>" >&2
  exit 1
fi

DIR="$(readlink -f "$(dirname "$0")")"
ADDR="$1"

CPUS=( 0 1 2 3 )
MCONTROL6_LOAD=0x1
MCONTROL6_STORE=0x2
MCONTROL6_EXEC=0x4
MCONTROL6_U=0x8
MCONTROL6_S=0x10
MCONTROL6_M=0x40
MCONTROL6_VU=0x800000
MCONTROL6_VS=0x1000000
MCONTROL6_ACT_SHIFT=12
MCONTROL6_TRACE_ON=2
MCONTROL6_TRACE_OFF=3
MCONTROL6_TRACE_SYNC=4
MCONTROL6_DMODE=0x800000000000000

gen_openocd_scr() {
  echo -n "halt;"
  local tdata1=$(printf '0x%x' $(( \
    $MCONTROL6_EXEC | \
    $MCONTROL6_S | \
    $MCONTROL6_DMODE | \
    ($MCONTROL6_TRACE_SYNC << $MCONTROL6_ACT_SHIFT) )) )
  local tdata2="$ADDR"
  for cpu in "${CPUS[@]}"; do
    for wp in 0 1 2; do
      echo -n "riscv.cpu$cpu set_reg {tselect $wp tdata1 0 tdata2 0};"
    done
    echo -n "riscv.cpu$cpu set_reg {tselect 3 tdata1 $tdata1 tdata2 $tdata2};"
    echo -n "riscv.cpu$cpu riscv set_ebreaku off;"
    echo -n "riscv.cpu$cpu riscv set_ebreaks off;"
  done
  echo -n "resume; exit;"
}

cmd="$(gen_openocd_scr)"

set -x

exec openocd -f "$DIR/openocd.cfg" -c "$cmd"
