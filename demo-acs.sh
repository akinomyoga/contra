#!/bin/bash

acsc=$'\e(0+,-.0`abcdefghijklmnopqrstuvwxyz{|}~\e(B'
printf '\e[%dm'"$acsc"'\e[m\n' 0 6706 6703 6704
echo

function acsc_decdhl {
  echo $'\e[6705m'"$acsc"
  echo $'\e[6706m'"$acsc"
  echo $'\e[6703m'"$acsc"
  echo $'\e[6704m'"$acsc"
}

echo Bold
printf '\e[1;94;47m'
acsc_decdhl
printf '\e[m'
echo

echo Italic
printf '\e[3m'
acsc_decdhl
printf '\e[m'
