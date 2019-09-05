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

function process-for-charlist {
  local enc=$1 list=$2
  [[ $verbose ]] && echo -n "$enc..." >/dev/tty
  echo "$enc"
  echo -n '  map '
  for ((i=0;i<${#list};i++)); do
    [[ $verbose ]] && echo -n "$i:" >/dev/tty
    local c=${list:i:1}
    local d=$(iconv -c -f "$enc" -t utf-8 <<< $c)
    [[ ! $d || $c == $d ]] && continue
    echo -n "$c$d"
  done
  echo
}

charlist94='!"#$%&'\''()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~'
function process {
  process-for-charlist "$1" "$charlist94"
}
# for a in {160..255}; do printf -v x '\\u%04X' "$a"; eval "printf '$x'"; done >> a.txt
charlist96=' ¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞßàáâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþÿ'


function initialize-charset96 {
  for ((i=160;i<256;i++)); do
    printf -v h '%02X' "$i"
    printf -v H '%02X' "$((i-128))"
    eval "charset96_bytes[i]=\$'\\x$h'"
    eval "charset96_chars[i]=\$'\\u$h'"
    eval "charset96_ascii[i]=\$'\\u$H'"
  done
  charset96_ascii[160]='<SP>'
  charset96_ascii[255]='<DEL>'
}
initialize-charset96
function process96 {
  local verbose=1

  local enc=$1 list=$2
  [[ $verbose ]] && echo -n "$enc..." >/dev/tty
  echo "$enc"
  echo -n '  map '
  for ((i=160;i<256;i++)); do
    [[ $verbose ]] && echo -n "$i:" >/dev/tty
    local c=${charset96_chars[i]}
    local d=$(iconv -c -f "$enc" -t utf-8 <<< "${charset96_bytes[i]}")
    [[ ! $d || $c == $d ]] && continue
    echo -n "${charset96_ascii[i]}$d"
  done
  echo
}

function output-iconv-def {
  local keys=(
    4 6 8-1 9-1 10 11 14 15 16 17 18 19 21 25 27 37 49 50 51 54 55
    57 60 61 69 84 85 86 88 89 90 92 98 99 100 101 103 109 110 111
    121 122 126 127 138 139 141 143 144 148 150 151 153 155 156 157
    166 179 193 197 199 203 209 226)
  local key
  for key in "${keys[@]}"; do
    process "ISO-IR-$key"
  done
}

function output-iconv-def-for-cygwin {
  process ISO-IR-58
  process ISO-IR-87
  process ISO-IR-149
  process ISO-IR-159
  process ISO-IR-165
  process ISO-IR-230
}

function output-iconv-def-for-96chars {
  local keys=(
    100 101 109 110 111 126 127 138 139 143 144 148 153 155 156 157 166
    179 197 199 203 226 209)
  for key in "${keys[@]}"; do
    process96 "ISO-IR-$key"
  done
}

function gendef/ISO-IR-165/output {
  local a=$1 b=$2 u=$3
  u=${u//' '/'<SP>'}
  u=${u//$'\x7F'/'<DEL>'}
  echo "  define $a$b$u"
}
function output-iconv-def-for-ISO-IR-165 {
  local outdir=../../out/gen
  local ia ib a b u
  for ((ia=0;ia<94;ia++)); do
    echo "[$(date +'%F %T %Z')] ia=$ia..." >&2
    a=${charlist94:ia:1}

    # 一括変換を試みる
    local line=
    for ((ib=0;ib<94;ib++)); do
      b=${charlist94:ib:1}
      line=$line$a$b
    done
    if u=$(echo -n "$line" | iconv -f ISO-IR-165 -t utf-8 2>/dev/null) && ((${#u}==94)); then
      for ((ib=0;ib<94;ib++)); do
        gendef/ISO-IR-165/output "$a" "${charlist94:ib:1}" "${u:ib:1}"
      done
      continue
    fi

    # 一文字ずつ変換する
    for ((ib=0;ib<94;ib++)); do
      b=${charlist94:ib:1}
      if u=$(echo -n "$a$b" | iconv -f ISO-IR-165 -t utf-8 2>/dev/null) && [[ $u ]]; then
        gendef/ISO-IR-165/output "$a" "$b" "$u"
      fi
    done
  done > "$outdir"/iso2022.ir165.def.part
  mv -f "$outdir"/iso2022.ir165.def.part "$outdir"/iso2022.ir165.def
}

case $1 in
(165)
  output-iconv-def-for-ISO-IR-165 ;;
(sb96)
  output-iconv-def-for-96chars ;;
(cygwin)
  output-iconv-def-for-cygwin > iso2022.cygwin.def ;;
(*)
  echo "usage: $0 sb96|cygwin|165" ;;
esac
