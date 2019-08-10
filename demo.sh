#!/bin/bash

if ((!_ble_bash)); then
  echo 'usage: source demo.sh'
  echo '  To run this script, ble.sh (git@github.com:akinomyoga/ble.sh) is needed.'
  exit 1
fi

contra_demo_keys=()

function contra/demo/title {
  [[ :$3: != *:noclear:* ]] && printf '\e[2J'
  printf '\e[2;3H\e[1m%s\e[m' "$1"
  [[ $2 ]] && printf ' -- %s' "$2"
  printf '\e[4H'
}
function contra/demo/subtitle {
  printf '\e[3G%s\n' "$1"
}

contra_demo_keys+=(plu)
function contra/demo:plu {
  contra/demo/title 'PLU/PLD' 'PARTIAL LINE UP/DOWN'
  local esc=$'[Cu(H\eK2\eLO)\eK4\eL]\eL2+\eK'
  printf '    \e[6705m%s\e[m\n' "$esc"
  printf '    \e[6706m%s\e[m\n' "$esc"
  printf '    \e[6703m%s\e[m\n' "$esc"
  printf '    \e[6704m%s\e[m\n' "$esc"
}

contra_demo_keys+=(decslrm)
function contra/demo:decslrm {
  contra/demo/title DECSLRM/DECSTBM 'Set Left and Right Margins'
  printf '\e[?69h\e[4;'$((LINES-1))'r'

  printf '\e[5;15s\e[4;5H'
  printf 'Hello, world%s! ' {1..15}

  printf '\e[20;40s\e[4;20H'
  printf 'Hello, world%s! ' {1..15}

  printf '\e[45;75s\e[4;45H'
  printf 'Hello, world%s! ' {1..15}

  ble/util/msleep 1000
  printf '\e[5;15s\e[4;5H'
  seq -s ' ' -f 'Hello, world %.f!' 100000

  printf '\e[20;40s\e[4;20H'
  seq -s ' ' -f 'Hello, world %.f!' 100000

  printf '\e[45;75s\e[4;45H'
  seq -s ' ' -f 'Hello, world %.f!' 100000

  printf '\e[;s\e[;r\e[?69l'
}

contra_demo_keys+=(sgr48)
function contra/demo:sgr48 {
  printf '\e[?69h'

  local content=$(
    printf $'\e[5;s'
    contra/demo/title 'SGR(48)' 'ISO 8613-6 Colors (Background)' noclear
    contra/demo/subtitle 256color
    for ((i=0;i<256;i++)); do
      printf %s $'\e[48:5:'$i'm '
      (((i+1)%64==0)) && printf %s $'\e[m\n'
    done

    contra/demo/subtitle '24bit color'
    for ((i=0;i<72;i++)); do ((v=255-i*255/71)); printf %s $'\e[48:2:'$v':0:0m '; done; printf %s $'\n'
    for ((i=0;i<72;i++)); do ((v=255-i*255/71)); printf %s $'\e[48:2:0:'$v':0m '; done; printf %s $'\n'
    for ((i=0;i<72;i++)); do ((v=255-i*255/71)); printf %s $'\e[48:2:0:0:'$v'm '; done; printf %s $'\n'
    for ((i=0;i<12;i++)); do ((v=i*255/11)); printf %s $'\e[48:2:255:'$v':0m '; done
    for ((i=0;i<12;i++)); do ((v=i*255/11)); printf %s $'\e[48:2:'$((255-v))':255:0m '; done
    for ((i=0;i<12;i++)); do ((v=i*255/11)); printf %s $'\e[48:2:0:255:'$v'm '; done
    for ((i=0;i<12;i++)); do ((v=i*255/11)); printf %s $'\e[48:2:0:'$((255-v))':255m '; done
    for ((i=0;i<12;i++)); do ((v=i*255/11)); printf %s $'\e[48:2:'$v':0:255m '; done
    for ((i=0;i<12;i++)); do ((v=i*255/11)); printf %s $'\e[48:2:255:0:'$((255-v))'m '; done
    printf %s "$buff"$'\e[m\n'

    contra/demo/subtitle 'ISO 8613-6 RGB'
    for ((i=0;i<72;i++)); do ((v=255-i*255/71)); printf %s $'\e[48:2::'$v':0:0m '; done; printf %s $'\n'
    for ((i=0;i<72;i++)); do ((v=255-i*255/71)); printf %s $'\e[48:2::0:'$v':0m '; done; printf %s $'\n'
    for ((i=0;i<72;i++)); do ((v=255-i*255/71)); printf %s $'\e[48:2::0:0:'$v'm '; done; printf %s $'\n'
    for ((i=0;i<12;i++)); do ((v=i*255/11)); printf %s $'\e[48:2::255:'$v':0m '; done
    for ((i=0;i<12;i++)); do ((v=i*255/11)); printf %s $'\e[48:2::'$((255-v))':255:0m '; done
    for ((i=0;i<12;i++)); do ((v=i*255/11)); printf %s $'\e[48:2::0:255:'$v'm '; done
    for ((i=0;i<12;i++)); do ((v=i*255/11)); printf %s $'\e[48:2::0:'$((255-v))':255m '; done
    for ((i=0;i<12;i++)); do ((v=i*255/11)); printf %s $'\e[48:2::'$v':0:255m '; done
    for ((i=0;i<12;i++)); do ((v=i*255/11)); printf %s $'\e[48:2::255:0:'$((255-v))'m '; done
    printf %s "$buff"$'\e[m\n'

    contra/demo/subtitle 'ISO 8613-6 CMY'
    for ((i=0;i<72;i++)); do ((v=255-i*255/71)); printf %s $'\e[48:3::'$v':0:0m '; done; printf %s $'\n'
    for ((i=0;i<72;i++)); do ((v=255-i*255/71)); printf %s $'\e[48:3::0:'$v':0m '; done; printf %s $'\n'
    for ((i=0;i<72;i++)); do ((v=255-i*255/71)); printf %s $'\e[48:3::0:0:'$v'm '; done; printf %s $'\n'
    printf %s "$buff"$'\e[m'

    contra/demo/subtitle 'ISO 8613-6 CMYK'
    for ((i=0;i<72;i++)); do ((v=255-i*255/71)); printf %s $'\e[48:4::'$v':0:0:0m '; done; printf %s $'\n'
    for ((i=0;i<72;i++)); do ((v=255-i*255/71)); printf %s $'\e[48:4::0:'$v':0:0m '; done; printf %s $'\n'
    for ((i=0;i<72;i++)); do ((v=255-i*255/71)); printf %s $'\e[48:4::0:0:'$v':0m '; done; printf %s $'\n'
    for ((i=0;i<72;i++)); do ((v=255-i*255/71)); printf %s $'\e[48:4::0:0:0:'$v'm '; done; printf %s $'\n'
    printf %s "$buff"$'\e[m' )


  function contra/demo:sgr48/bg_rgb {
    local k=$1
    local i v line=
    for ((i=0;i<COLUMNS;i++)); do
      ((v=i*255/(COLUMNS-1)))
      line+=$'\e[48:2:R:'$v':'$k'm '
    done
    local j R buff=$'\e[H\e[;s'
    for ((j=0;j<LINES;j++)); do
      ((R=j*255/(LINES-1)))
      buff+=${line//R/$R}
    done; buff+=$'\e[m'
    ret=$buff
  }
  function contra/demo:sgr48/bg_cmyk {
    local line= a b c OVERLAP=83
    local unit=$((512-OVERLAP))
    for ((i=0;i<COLUMNS;i++)); do
      local hue=$((i*unit*3/COLUMNS))
      local a=$((hue%unit))
      ((b=255-a,b<0&&(b=0)))
      ((c=a-(256-OVERLAP),c<0&&(c=0)))
      case $((hue/unit)) in
      (0) line+=$'\e[48:4::'$c':255:'$b':Km ' ;;
      (1) line+=$'\e[48:4::255:'$b':'$c':Km ' ;;
      (2) line+=$'\e[48:4::'$b':'$c':255:Km ' ;;
      esac
    done

    local buff=$'\e[H\e[;s'
    local i v j u
    for ((j=0;j<LINES;j++)); do
      local K=$((255-j*255/(LINES-1)))
      buff+=${line//K/$K}
    done; buff+=$'\e[m'
    ret=$buff
  }

  local k ret
  for ((k=0;k<=255;k+=51)); do
    contra/demo:sgr48/bg_rgb "$k"
    printf '%s' "$ret$content"
    ble/util/msleep 1000
  done

  contra/demo:sgr48/bg_cmyk
  printf '%s' "$ret$content"

  printf '\e[;s\e[?69l'
}

contra_demo_keys+=(sco)
function contra/demo:sco {
  contra/demo/title SCO 'SELECT CHARACTER ORIENTATION'
  printf '\e[?69h'

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

contra_demo_keys+=(acs)
function contra/demo:acs {
  contra/demo/title 'GZD4(0)' 'DEC Special Graphic (VT100 Line Drawing set)'
  local acsc=$'\e(0+,-.0`abcdefghijklmnopqrstuvwxyz{|}~\e(B'

  function acsc_decdhl {
    printf '\e[%dm'"$acsc"'\e[6705m\n' 6705 6706 6703 6704
    echo
  }

  printf '\e[?69h'
  printf '\e[5;s\e[4;5H'
  acsc_decdhl

  printf $'\e[3GBold\n'
  printf '\e[1;94;47m'
  acsc_decdhl
  printf '\e[m'

  printf $'\e[3GItalic\n'
  printf '\e[3m'
  acsc_decdhl

  printf '\e[m\e[;s\e[?69l'
}

function contra/demo/reset-terminal {
  printf '\e[?1049l'
  printf '\e[m\e[;s\e[?69l'
  exit
}
(
  builtin trap contra/demo/reset-terminal EXIT INT QUIT
  printf '\e[?1049h\e[2J'
  if declare -f "contra/demo:$1" &>/dev/null; then
    "contra/demo:$1"
    ble/util/msleep 5000
  else
    for key in "${contra_demo_keys[@]}"; do
      contra/demo:"$key"
      ble/util/msleep 3000
    done
  fi
  printf '\e[?1049l'
)
