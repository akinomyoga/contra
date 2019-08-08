#!/bin/bash

# iconv -c -f ISO-IR-55 -t utf-32le <<'EOF' | od -t x4
#  !"#$%&'()*+,-./
# 0123456789:;<=>?
# @ABCDEFGHIJKLMNO
# PQRSTUVWXYZ[\]^_
# `abcdefghijklmno
# pqrstuvwxyz{|}~
# EOF

# iconv -c -f ISO-IR-55 -t utf-8 <<'EOF'
#  !"#$%&'()*+,-./
# 0123456789:;<=>?
# @ABCDEFGHIJKLMNO
# PQRSTUVWXYZ[\]^_
# `abcdefghijklmno
# pqrstuvwxyz{|}~
# EOF

charlist94='!"#$%&'\''()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~'
function process {
  local enc=$1
  echo -n "$enc..." >/dev/tty
  echo "$enc"
  echo -n '  map '
  for ((i=0;i<${#charlist94};i++)); do
    echo -n "$i.." >/dev/tty
    local c=${charlist94:i:1}
    local d=$(iconv -c -f "$enc" -t utf-8 <<< $c)
    [[ ! $d || $c == $d ]] && continue
    echo -n "$c$d"
  done
  echo
}

function output-iconv-def {
  process ISO-IR-4
  process ISO-IR-6
  process ISO-IR-8-1
  process ISO-IR-9-1
  process ISO-IR-10
  process ISO-IR-11
  process ISO-IR-14
  process ISO-IR-15
  process ISO-IR-16
  process ISO-IR-17
  process ISO-IR-18
  process ISO-IR-19
  process ISO-IR-21
  process ISO-IR-25
  process ISO-IR-27
  process ISO-IR-37
  process ISO-IR-49
  process ISO-IR-50
  process ISO-IR-51
  process ISO-IR-54
  process ISO-IR-55
  process ISO-IR-57
  process ISO-IR-60
  process ISO-IR-61
  process ISO-IR-69
  process ISO-IR-84
  process ISO-IR-85
  process ISO-IR-86
  process ISO-IR-88
  process ISO-IR-89
  process ISO-IR-90
  process ISO-IR-92
  process ISO-IR-98
  process ISO-IR-99
  process ISO-IR-100
  process ISO-IR-101
  process ISO-IR-103
  process ISO-IR-109
  process ISO-IR-110
  process ISO-IR-111
  process ISO-IR-121
  process ISO-IR-122
  process ISO-IR-126
  process ISO-IR-127
  process ISO-IR-138
  process ISO-IR-139
  process ISO-IR-141
  process ISO-IR-143
  process ISO-IR-144
  process ISO-IR-148
  process ISO-IR-150
  process ISO-IR-151
  process ISO-IR-153
  process ISO-IR-155
  process ISO-IR-156
  process ISO-IR-157
  process ISO-IR-166
  process ISO-IR-179
  process ISO-IR-193
  process ISO-IR-197
  process ISO-IR-199
  process ISO-IR-203
  process ISO-IR-209
  process ISO-IR-226
}

function output-iconv-def-for-cygwin {
  process ISO-IR-58
  process ISO-IR-87
  process ISO-IR-149
  process ISO-IR-159
  process ISO-IR-165
  process ISO-IR-230
}
# output-iconv-def-for-cygwin > iso2022.cygwin.def
