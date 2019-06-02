#!/bin/bash

if ((!_ble_bash)); then
  echo 'usage: source demo.sh'
  echo '  To run this script, ble.sh (git@github.com:akinomyoga/ble.sh) is needed.'
  exit 1
fi

printf '\e[?1049h\e[2J'

function test-sco {
  printf '\e[?69h'

  printf '\e[2;3H\e[1mTest of SCO\e[m'
  printf '\e[5;s\e[4;5H'
  printf '\e[%d eEnglish 日本語 +-*/\e[ e\n' {0..7}
  echo
  printf '\eK'
  printf '\e[%d eEnglish 日本語 +-*/\e[ e\n' {0..7}
  printf '\eL'
  echo
  printf '\e[%d e\e[3mHello, \e[20mworld!\e[23m\e[ e\n' {0..3}
  printf '\e[;s'

  printf '\e[30;s\e[4;30H'

  ble/util/sprintf line '\e[%dm\e[%%d eEnglish 日本語 +-*/\e[ e\n' 6703 6704
  printf "$line" {0..7}{,}
  printf '\e[6705m'

  printf '\e[?69l'
}

test-sco
ble/util/msleep 5000

printf '\e[?1049l'
