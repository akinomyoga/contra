#!/bin/bash

function check1 {
  local -i code=$1
  ble/util/c2w+east "$code"; local a=$ret
  ble/util/c2w+west "$code"; local b=$ret
  if ((a!=b)); then
    echo "$1 ($2) is ambiguous"
  fi
}

check1 0x2666 ACS_DIAMOND
check1 0x2592 ACS_CKBOARD
check1 0x00B0 ACS_DEGREE
check1 0x00B1 ACS_PLMINUS
check1 0x240B ACS_LANTERN
check1 0x03C0 ACS_PI
check1 0x2260 ACS_NEQUAL
check1 0x00A3 ACS_STERLING
check1 0x2022 ACS_BULLET
check1 0x2192 ACS_RARROW
check1 0x2190 ACS_LARROW
check1 0x2191 ACS_UARROW
check1 0x2193 ACS_DARROW
check1 0x2264 ACS_LEQUAL
check1 0x2265 ACS_GEQUAL
