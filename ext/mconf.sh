# bash source -*- mode: sh; mode: sh-bash -*-

#------------------------------------------------------------------------------
# Settings

## @var mconf_flags
##   以下の文字の組み合わせで各機能の on/off を制御します。
##
##   o ... 標準出力にテスト結果を出力します
##
##   d ... シェル変数を定義します
##
##   h ... ヘッダファイルに出力します (fd 5 に出力します)
##
##   l ... ログが有効です (fd 6 に出力します)
##
##   c ... キャッシュが有効です。
##     $mconf_cache_dir 以下にあるキャッシュファイルを利用・更新します。
##
mconf_flags=o

exec 5>/dev/null
exec 6>/dev/null
mconf_cache_dir=

## @var mconf_cxx
## @var mconf_cxx_options
##
##   コンパイルに使用するコンパイラ及びオプションを設定します。
##
mconf_cxx=g++
mconf_cxx_options=()

#------------------------------------------------------------------------------

if [[ -t 1 ]]; then
  if [[ -s ${MWGDIR:-$HOME/.mwg}/share/mshex/shrc/mwg_term.sh ]]; then
    source "${MWGDIR:-$HOME/.mwg}/share/mshex/shrc/mwg_term.sh"
    mwg_term.set _mconf_term_sgr34 fDB
    mwg_term.set _mconf_term_sgr35 fDM
    mwg_term.set _mconf_term_sgr91 fHR
    mwg_term.set _mconf_term_sgr94 fHB
    mwg_term.set _mconf_term_sgr95 fHM
    mwg_term.set _mconf_term_sgr0  sgr0
  elif type tput &>/dev/null; then
    _mconf_term_sgr34=$(tput setaf 4)
    _mconf_term_sgr35=$(tput setaf 5)
    _mconf_term_sgr91=$(tput setaf 9)
    _mconf_term_sgr94=$(tput setaf 12)
    _mconf_term_sgr95=$(tput setaf 13)
    _mconf_term_sgr0=$(tput sgr0)
  else
    _mconf_term_sgr34='[34m'
    _mconf_term_sgr35='[35m'
    _mconf_term_sgr91='[91m'
    _mconf_term_sgr94='[94m'
    _mconf_term_sgr95='[95m'
    _mconf_term_sgr0='[m'
  fi
else
  _mconf_term_sgr34=
  _mconf_term_sgr35=
  _mconf_term_sgr91=
  _mconf_term_sgr94=
  _mconf_term_sgr95=
  _mconf_term_sgr0=
fi

_mconf_msg_ok="${_mconf_term_sgr94}ok${_mconf_term_sgr0}"
_mconf_msg_missing="${_mconf_term_sgr95}missing${_mconf_term_sgr0}"
_mconf_msg_invalid="${_mconf_term_sgr95}invalid${_mconf_term_sgr0}"

#------------------------------------------------------------------------------
# utils

((mwg_bash=BASH_VERSINFO[0]*10000+BASH_VERSINFO[1]*100+BASH_VERSINFO[2]))
if ((mwg_bash>=40100)); then
  declare -A _mconf_s2c_table
  function mconf/util/s2c {
    local c=${1::1}
    ret=${_mconf_s2c_table[x$c]}
    [[ $ret ]] && return

    printf -v ret '%d' "'$c"
    _mconf_s2c_table[x$c]=$ret
  }
else
  declare _mconf_s2c_table
  function _mconf_s2c_table.init {
    local table00=$'\x3f\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f'
    local table01=$'\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f'
    local table02=$'\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f'
    local table03=$'\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\x3e\x3f'
    local table04=$'\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4a\x4b\x4c\x4d\x4e\x4f'
    local table05=$'\x50\x51\x52\x53\x54\x55\x56\x57\x58\x59\x5a\x5b\x5c\x5d\x5e\x5f'
    local table06=$'\x60\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f'
    local table07=$'\x70\x71\x72\x73\x74\x75\x76\x77\x78\x79\x7a\x7b\x7c\x7d\x7e\x7f'
    local table08=$'\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f'
    local table09=$'\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f'
    local table0a=$'\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf'
    local table0b=$'\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf'
    local table0c=$'\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf'
    local table0d=$'\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf'
    local table0e=$'\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef'
    local table0f=$'\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff'
    _mconf_s2c_table="$table00$table01$table02$table03$table04$table05$table06$table07$table08$table09$table0a$table0b$table0c$table0d$table0e$table0f"
  }
  _mconf_s2c_table.init

  function mconf/util/s2c {
    local c=${1::1}
    if [[ $c == '?' ]]; then
      ret=63
    elif [[ $c == '*' ]]; then
      ret=42
    else
      local tmp=${_mconf_s2c_table%%"$c"*}
      ret=${#tmp}
    fi
  }
fi

# function mconf/util/hex {
#   local -i n=$1
#   local hi=$((n/16))
#   local lo=$((n%16))
#   case "$hi" in
#   (10) hi=a ;;
#   (11) hi=b ;;
#   (12) hi=c ;;
#   (13) hi=d ;;
#   (14) hi=e ;;
#   (15) hi=f ;;
#   esac
#   case "$lo" in
#   (10) lo=a ;;
#   (11) lo=b ;;
#   (12) lo=c ;;
#   (13) lo=d ;;
#   (14) lo=e ;;
#   (15) lo=f ;;
#   esac
#   ret=$hi$lo
# }

# function mconf/string#hex {
#   local text="$*"
#   local out=
#   local i ret
#   for ((i=0;i<${#text};i++));do
#     mconf/util/s2c "${text:$i:1}"
#     mconf/util/hex "$ret"
#     out=$out$ret
#   done
#   ret=$out
# }

#_mconf_base64_table='ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'
_mconf_base64_table='ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+%'
function mconf/util/base64 {
  local text="$*" i j s
  local code=
  for ((i=0;i<${#text};i+=3)); do
    local cook="${text:i:3}"

    s=0
    for ((j=0;j<3;j++));do
      mconf/util/s2c "${cook:j:1}"
      ((s=s*256+ret))
    done

    local quartet=
    for ((j=3;j>=0;j--));do
      if ((j>${#cook})); then
        quartet="=$quartet"
      else
        quartet="${_mconf_base64_table:$((s%64)):1}$quartet"
      fi
      ((s/=64))
    done

    code="$code$quartet"
  done

  ret=$code
}

if ((mwg_bash>=40100)); then
  function mconf/string#toupper {
    ret=${1^^?}
  }
else
  function mconf/string#toupper {
    ret=$(tr a-z A-Z <<< "$1")
  }
fi

if [[ $COMSPEC && -x $COMSPEC ]]; then
  # for slow forks in Windows
  # 0.522s in linux

  # @var[out] _var
  # @var[out] _ret
  function mconf/hash.impl {
    # 1 read arguments
    local hlen=
    while [[ $1 == -* ]]; do
      local arg="$1"; shift
      case "$arg" in
      (-v)  _var="$1"; shift ;;
      (-v*) _var="${arg:2}"  ;;
      (-l)  hlen="$1"; shift ;;
      (-l*) hlen="${arg:2}"  ;;
      (*)   break ;;
      esac
    done

    # 2. text to char array
    local text="$*"
    local tlen="${#text}"
    (((!hlen||hlen>tlen)&&(hlen=tlen)))
    local i data ret
    data=()
    for ((i=0;i<tlen;i++)); do
      mconf/util/s2c "${text:i:1}"
      ((data[i%hlen]+=ret))
    done

    # 3. base64 encoding
    local i _j _s quartet buff
    buff=()
    for((i=0;i<hlen;i+=3)); do
      ((_s=data[i]<<16&0xFF0000|data[i+1]<<8&0xFF00|data[i+2]&0xFF))

      quartet=
      for((_j=3;_j>=0;_j--));do
        if ((i+_j>hlen)); then
          quartet="=$quartet"
        else
          quartet="${_mconf_base64_table:_s%64:1}$quartet"
        fi
        ((_s/=64))
      done

      buff[${#buff[@]}]="$quartet"
    done

    IFS= eval '_ret="${buff[*]}"'
  }

  function mconf/hash {
    local _var= _ret
    mconf/hash.impl "$@"
    if [[ $_var ]]; then
      eval "$_var=\"\$_ret\""
    else
      echo "$_ret"
    fi
  }
else
  function mconf/hash {
    local _var _len
    while [[ $1 == -* ]]; do
      local arg="$1"; shift
      case "$arg" in
      (-v)  _var="$1"; shift ;;
      (-v*) _var="${arg:2}"  ;;
      (-l)  _len="$1"; shift ;;
      (-l*) _len="${arg:2}"  ;;
      (*)   break ;;
      esac
    done

    local _ret="$(echo -n "$*"|md5sum)"
    _ret="${_ret//[   -]}"

    if [[ $_var ]]; then
      eval "$_var=\"\$_ret\""
    else
      echo "$_ret"
    fi
  }
fi

function mconf/util/mkd {
  [[ -d $1 ]] || mkdir -p "$1"
}

#------------------------------------------------------------------------------
# output

function mconf/log/print-title {
  echo '-------------------------------------------------------------------------------'
  echo "$*"
  echo
}

function mconf/header/print {
  [[ $mconf_flags == *h* ]] || return 0
  echo "$*" >&5
}
function mconf/header/define {
  [[ $mconf_flags == *h* ]] || return 0
  local name=$1
  if [[ $2 ]]; then
    echo "#undef $name"
    echo "#define $name $2"
    echo
  else
    echo "#undef $name"
    echo "/* #define $name */"
    echo
  fi >&5
}

function mconf/define {
  local d=$1 v=$2
  [[ $mconf_flags == *h* ]] && mconf/header/define "$d" "$v"
  [[ $mconf_flags == *s* ]] && eval "$d=$v"
}

function mconf/echo {
  local type= prefix=mconf flags=
  local -a echo_options=()
  while [[ $1 == -* ]]; do
    local arg=$1; shift
    case $arg in
    (--) break ;;
    (--*)
      mconf/echo -Ep mconf/echo "unrecognized long option $arg." >&2
      flags=E ;;
    (-*)
      local i c
      for ((i=1;i<${#arg};i++)); do
        c=${arg:i:1}
        case $c in
        ([EXM])
          type=$c ;;
        ([en])
          echo_options[${#echo_options[@]}]=-$c ;;
        (p)
          if [[ ${arg:i+1} ]]; then
            prefix=${arg:i+1}; break
          elif (($#)); then
            prefix=$1; shift
          else
            mconf/echo -Ep mconf/echo "missing option argument for -p" >&2
            flags=E
          fi ;;
        (*)
          mconf/echo -Ep mconf/echo "unrecognized option -$c" >&2
          flags=E ;;
        esac
      done
    esac
  done
  [[ $flags == *E* ]] && return 1

  case $type in
  (X) prefix=$_mconf_term_sgr94$prefix'$ '$_mconf_term_sgr0 ;;
  (E) prefix=$_mconf_term_sgr91$prefix'! '$_mconf_term_sgr0 ;;
  (M) prefix=$_mconf_term_sgr34$prefix': '$_mconf_term_sgr0 ;;
  esac

  echo "${echo_options[@]}" "$prefix$*"
}

#------------------------------------------------------------------------------
# basic check

## 関数 mconf/test/.check type
##   @var[in] mconf_flags
##   @var[in] mconf_cache_name
##   @var[in] mconf_log_title
##   @var[in] mconf_msg_title
##   @var[in] mconf_msg_ok
##   @var[in] mconf_msg_ng
function mconf/test/.check {
  local type=$1
  local opt_msg= opt_log=
  [[ $mconf_flags == *o* ]] && opt_msg=1
  [[ $mconf_flags == *l* ]] && opt_log=1

  local result=
  local message_head='  checking'

  local cache=
  if [[ $mconf_flags == *c* ]]; then
    mconf/util/mkd "$mconf_cache_dir"
    cache=$mconf_cache_dir/$mconf_cache_name.stamp
  fi
  if [[ $cache && -e $cache ]]; then
    if [[ -s $cache ]]; then
      result=1
      [[ $opt_msg ]] && echo "$message_head $mconf_msg_title ... $mconf_msg_ok (cached)"
    else
      [[ $opt_msg ]] && echo "$message_head $mconf_msg_title ... $mconf_msg_ng (cached)"
    fi
    [[ $result ]]; return
  fi

  # Note: 並列コンパイルすると表示が乱れてしまうので中途半端な行は出力しない。
  #[[ $opt_msg ]] && echo -n "$message_head $mconf_msg_title ... "
  if mconf/test/check:$type/check 1>&6 2>&6; then
    result=1
    [[ $opt_msg ]] && echo "$message_head $mconf_msg_title ... $mconf_msg_ok"
    [[ $cache ]] && echo -n 1 > "$cache"
  else
    [[ $opt_msg ]] && echo "$message_head $mconf_msg_title ... $mconf_msg_ng"
    [[ $cache ]] && : > "$cache"
    mconf/test/check:$type/onfail 1>&6 2>&6
  fi

  [[ $result ]]
}

## 関数 mconf/test/check:compile/check
##   @var[in] mconf_log_title
##   @var[in] mconf_cxx
##   @var[in] mconf_cxx_options
##   @var[in] mconf_cmd_code
function mconf/test/check:compile/check {
  mconf/log/print-title "$mconf_log_title"
  local code=$($mconf_cmd_code)
  _mconf_test_check_compiles_code=$code
  "$mconf_cxx" -c -xc++ - -o tmp.o "${mconf_cxx_options[@]}" <<< "$code"
  local ext=$?
  rm -f tmp.o tmp.obj
  return "$ext"
}

function mconf/test/check:compile/onfail {
  local code=$_mconf_test_check_compiles_code
  [[ $opt_log ]] && {
    echo "compiler options: ${mconf_cxx_options[@]}"
    sed 's/^/| /' <<< "$code"
  }
}

##   @var[in] mconf_log_title
##   @var[in] mconf_cxx
##   @var[in] mconf_cxx_options
##   @var[in] mconf_lib
function mconf/test/check:link/check {
  mconf/log/print-title "$mconf_log_title"
  "$mconf_cxx" -c -xc++ - -o tmp.exe "${mconf_cxx_options[@]}" -l"$mconf_lib" <<< "int main() {}"
  local ext=$?
  rm -f tmp.exe
  return "$ext"
}
function mconf/test/check:link/onfail {
  local code=$_mconf_test_check_compiles_code
  [[ $opt_log ]] && {
    echo "compiler options: ${mconf_cxx_options[@]} -l$mconf_lib"
    echo "| int main() {}"
  }
}

#------------------------------------------------------------------------------
# header

function mconf/test/header.code {
  echo "#include <$header>"
}

## 関数 mconf/test/header header_name
## @var[in] title
function mconf/test/header.impl {
  local header=$1

  local SL='/'
  local name="${header//$SL/%}"
  local mconf_cache_name="H+$name"
  local mconf_log_title="test_header $header"
  local mconf_msg_title="header $_mconf_term_sgr35${title:-#include <$header>}$_mconf_term_sgr0"
  local mconf_cmd_code=mconf/test/header.code
  local mconf_msg_ok=$_mconf_msg_ok
  local mconf_msg_ng=$_mconf_msg_missing
   mconf/test/.check compile
}

function mconf/test/header {
  # read arguments
  local header= def_name= title=
  while (($#)); do
    local arg=$1
    shift
    case "$arg" in
    (-o?*) def_name=${arg:2}   ;;
    (-o)   def_name=$1; shift  ;;
    (-t?*) title=${arg:2}  ;;
    (-t)   title=$1; shift ;;
    (*)
      if [[ ! $header ]]; then
        header=$arg
      elif [[ ! $def_name ]]; then
        def_name=$arg
      else
        mconf/echo -Ep mconf/test/header "ignored argument '$arg'."
      fi ;;
    esac
  done

  if [[ ! $header ]]; then
    mconf/echo -Ep mconf/test/header 'no header is specified.'
    return 1
  fi

  if [[ $def_name ]]; then
    def_name=${def_name//[!0-9a-zA-Z/_]}
  elif [[ $mconf_flags == *[sh]* ]]; then
    local ret; mconf/string#toupper "${header//[!0-9a-zA-Z]/_}"
    def_name=MWGCONF_HEADER_$ret
  fi

  local r=
  mconf/test/header.impl "$header" && r=1
  mconf/define "$def_name" "$r"
  [[ $r ]]
}

#------------------------------------------------------------------------------
# macro

function mconf/test/macro.code {
  local h
  for h in $headers; do
    echo "#include <$h>"
  done
  cat <<EOF
int main(){
#ifdef $macro
  return 0;
#else
  __choke__ /* macro is not defined! */
#endif
}
EOF
}

function mconf/test/macro.impl {
  local name=$1
  local headers=$2
  local macro=$3

  local ret
  mconf/util/base64 $headers; local enc_head=$ret
  local mconf_cache_name=M-$enc_head-$macro
  local mconf_log_title="test_macro $macro"
  local mconf_msg_title="(M) $_mconf_term_sgr35#define $macro$_mconf_term_sgr0"
  local mconf_cmd_code=mconf/test/macro.code
  local mconf_msg_ok=$_mconf_msg_ok
  local mconf_msg_ng=$_mconf_msg_missing
   mconf/test/.check compile
}

function mconf/test/macro {
  local ret; mconf/string#toupper "${1//[!0-9a-zA-Z]/_}"
  local def_name=MWGCONF_$ret
  local headers=$2
  local macro=$3

  local r=
  mconf/test/macro.impl "$@" && r=1
  mconf/define "$def_name" "$r"
  [[ $r ]]
}

#------------------------------------------------------------------------------
# expr

function mconf/test/expression.code {
  local h
  for h in "${headers[@]}"; do
    echo "#include <$h>"
  done
  echo "void f(){$expression;}"
}

## @var[in] title
## @var[in] headers=(...)
## @var[in] expression
function mconf/test/expression.impl {
  local enc_head enc_expr
  mconf/hash -venc_head -l64 "${headers[*]}"
  mconf/hash -venc_expr -l64 "$expression"

  local mconf_cache_name="X-$enc_head-$enc_expr"
  local mconf_log_title="test_expression $headers $expression"
  if [[ ! $title || $title == - ]]; then
    local mconf_msg_title=
  else
    local mconf_msg_title="expression $_mconf_term_sgr35$title$_mconf_term_sgr0"
  fi
  local mconf_cmd_code=mconf/test/expression.code
  local mconf_msg_ok=$_mconf_msg_ok
  local mconf_msg_ng=$_mconf_msg_invalid
   mconf/test/.check compile
}

function mconf/test/expression {
  # read arguments
  local def_name= expression= title=
  local fAbsoluteName= fSetName= fSetHeader=
  local -a headers
  headers=()
  while (($#)); do
    local arg="$1"
    shift
    case "$arg" in
    (-o?*) fAbsoluteName=1 fSetName=1 def_name="${arg:2}"   ;;
    (-o)   fAbsoluteName=1 fSetName=1 def_name="$1"; shift  ;;
    (-t?*) title="${arg:2}"  ;;
    (-t)   title="$1"; shift ;;
    (-h?*) fSetHeader=1 headers+=("${arg:2}")  ;;
    (-h*)  fSetHeader=1 headers+=($1); shift ;;
    (*)
      if [[ ! $fSetName ]]; then
        fSetName=1 def_name="$arg"
      elif [[ ! $fSetHeader ]]; then
        fSetHeader=1 headers+=($arg)
      elif [[ ! $expression ]]; then
        expression="$arg"
      else
        echo -Ep mconf/test/expression "ignored argument '$arg'."
      fi ;;
    esac
  done

  : "${title:="${def_name:-"$expression"}"}"

  if [[ ! $def_name || $def_name == - ]]; then
    def_name=
  elif [[ ! $fAbsoluteName ]]; then
    local ret; mconf/string#toupper "${def_name//[!0-9a-zA-Z]/_}"; def_name=$ret
    [[ $def_name =~ ^MWGCONF_ ]] || def_name="MWGCONF_HAS_$def_name"
  else
    def_name="${def_name//[^0-9a-zA-Z]/_}"
  fi

  if [[ ! $expression ]]; then
    echo -Ep mconf/test/expression 'expression is not specified!'
    return 1
  fi

  local r=
  mconf/test/expression.impl && r=1
  [[ $def_name ]] && mconf/define "$def_name" "$r"
  [[ $r ]]
}

#------------------------------------------------------------------------------
# source

function mconf/test/source.code {
  local h
  for h in "${headers[@]}"; do
    echo "#include <$h>"
  done
  echo "$source"
}

## @var[in] title
## @var[in] headers=(...)
## @var[in] source
function mconf/test/source.impl {
  local enc_head enc_expr
  mconf/hash -venc_head -l64 "${headers[*]}"
  mconf/hash -venc_expr -l64 "$source"

  local mconf_cache_name="S-$enc_head-$enc_expr"
  local mconf_log_title="test_source $headers $source"
  local mconf_msg_title="source $_mconf_term_sgr35${title}$_mconf_term_sgr0"
  local mconf_cmd_code=mconf/test/source.code
  local mconf_msg_ok=$_mconf_msg_ok
  local mconf_msg_ng=$_mconf_msg_invalid
   mconf/test/.check compile
}

function mconf/test/source {
  # read arguments
  local def_name= source= title=
  local fAbsoluteName= fSetName= fSetHeader=
  local -a headers
  headers=()
  while (($#)); do
    local arg="$1"
    shift
    case "$arg" in
    (-o?*) fAbsoluteName=1 fSetName=1 def_name="${arg:2}"   ;;
    (-o)   fAbsoluteName=1 fSetName=1 def_name="$1"; shift  ;;
    (-t?*) title="${arg:2}"  ;;
    (-t)   title="$1"; shift ;;
    (-h?*) fSetHeader=1 headers+=("${arg:2}")  ;;
    (-h*)  fSetHeader=1 headers+=("$1"); shift ;;
    (*)
      if [[ ! $fSetName ]]; then
        fSetName=1 def_name="$arg"
      elif [[ ! $fSetHeader ]]; then
        fSetHeader=1 headers+=($arg)
      elif [[ ! $source ]]; then
        source="$arg"
      else
        mconf/echo -Ep mconf/test/source "ignored argument '$arg'."
      fi ;;
    esac
  done

  : "${title:="${def_name:-"$source"}"}"

  if [[ ! $fAbsoluteName ]]; then
    local ret; mconf/string#toupper "${def_name//[!0-9a-zA-Z]/_}"; def_name=$ret
    [[ "$def_name" =~ ^MWGCONF_ ]] || def_name="MWGCONF_$def_name"
  else
    def_name="${def_name//[^0-9a-zA-Z]/_}"
  fi

  if [[ ! $source ]]; then
    mconf/echo -Ep mconf/test/source 'source is not specified!'
    return 1
  fi

  local r=
  mconf/test/source.impl && r=1
  [[ $def_name ]] && mconf/define "$def_name" "$r"
  [[ $r ]]
}

#------------------------------------------------------------------------------
# lib

function mconf/test/lib {
  local lib=$1

  local mconf_cache_name="L-$enc_head-$enc_expr"
  local mconf_log_title="test_lib $lib"
  local mconf_msg_title="lib $_mconf_term_sgr35${lib}$_mconf_term_sgr0"
  local mconf_msg_ok=$_mconf_msg_ok
  local mconf_msg_ng=$_mconf_msg_missing
  local mconf_lib=$lib
  mconf/test/.check link
}

#------------------------------------------------------------------------------
# test named

# function mconf/test/named.code {
#   for h in $headers; do
#     echo "#include <$h>"
#   done
#   echo "$source"
# }

# function mconf/test/named.impl {
#   local name="$1"
#   local headers="$2"
#   local source="$3"

#   local ret
#   mconf/util/base64 $headers ; local enc_head=$ret
#   mconf/util/base64 "$source"; local enc_expr=$ret

#   local mconf_cache_name="S-$enc_head-$enc_expr"
#   local mconf_log_title="test_named $headers $source"
#   local mconf_msg_title="named $_mconf_term_sgr35$name$_mconf_term_sgr0"
#   local mconf_cmd_code=mconf/test/named.code
#   local mconf_msg_ok=$_mconf_msg_ok
#   local mconf_msg_ng=$_mconf_msg_invalid
#    mconf/test/.check compile
# }

# function mconf/test/named {
#   local name=$1
#   local ret; mconf/string#toupper "MWGCONF_${1//[^0-9a-zA-Z]/_}"; local def_name=$ret
#   local headers="$2"
#   local source="$3"

#   local r=
#   mconf/test/named.impl "$@" && r=1
#   [[ $name ]] && mconf/define "$def_name" "$r"
#   [[ $r ]]
# }
