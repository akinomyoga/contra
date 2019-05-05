#!/bin/bash

printf '\e[?1049h\e[2J'

function test-sco {
  printf '\e[?69h'

  printf '\e[2;3H\e[1mTest of SCO\e[m'
  printf '\e[5;s\e[5;5H'
  printf '\e[%d eEnglish 日本語 +-*/\e[ e\n' {0..7}
  echo
  printf '\eK'
  printf '\e[%d eEnglish 日本語 +-*/\e[ e\n' {0..7}
  printf '\eL'
  printf '\e[;s'
  printf '\e[30;s\e[5;30H'

  ble/util/sprintf line '\e[%dm\e[%%d eEnglish 日本語 +-*/\e[ e\n' 6703 6704
  printf "$line" {0..7}{,}
  printf '\e[6705m'

  printf '\e[?69l'
}

test-sco
sleep 5

printf '\e[?1049l'
