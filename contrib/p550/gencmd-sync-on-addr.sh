#!/bin/bash

set -eu

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <ADDR...>" >&2
  exit 1
fi

DIR="$(readlink -f "$(dirname "$0")")"

CPUS=( 0 1 2 3 )
NUM_TRIGGERS=4
NUM_TRIG_USED=$#
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
MCONTROL6_TYPE=0x6000000000000000

if [[ $NUM_TRIG_USED -gt $NUM_TRIGGERS ]]; then
  echo "Specified more than $NUM_TRIGGERS addresses" >&2
  exit 1
fi

NUM_TRIG_FREE=$(( NUM_TRIGGERS - NUM_TRIG_USED ))

gen_openocd_scr() {
  local addrs=("$@")
  echo "halt;"
  local tdata1=$(printf '0x%x' $(( \
    $MCONTROL6_EXEC | \
    $MCONTROL6_S | \
    $MCONTROL6_VS | \
    $MCONTROL6_DMODE | $MCONTROL6_TYPE | \
    ($MCONTROL6_TRACE_SYNC << $MCONTROL6_ACT_SHIFT) )) )
  local wp
  for cpu in "${CPUS[@]}"; do
    #echo "riscv.cpu$cpu riscv set_ebreaku off;"
    #echo "riscv.cpu$cpu riscv set_ebreaks off;"
    for (( wp=0; wp<$NUM_TRIG_FREE; wp++)); do
      echo "riscv.cpu$cpu set_reg {tselect $wp tdata1 0 tdata2 0};"
    done
    for (( wp=0; wp<$#; wp++)) do
      echo "riscv.cpu$cpu set_reg {tselect $((wp + NUM_TRIG_FREE)) tdata1 $tdata1 tdata2 ${addrs[$wp]}};"
    done
  done
  echo "resume;"
}

gen_openocd_scr "$@"
