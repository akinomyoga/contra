#!/bin/bash

export OUTDIR=../../out/gen

function process-0213 {
  local input=$1
  local -x outfile_format=$2
  awk '
    BEGIN {
      outfile_format = ENVIRON["outfile_format"]
    }

    function enc96(value) {
      value += 32;
      if (value == 32) return "<SP>";
      if (value == 127) return "<DEL>";
      return sprintf("%c", value);
    }
  
    function rawdef_start_plane() {
      if (fname_plane) close(fname_plane);
      fname_plane = sprintf(outfile_format, plane_index);
    }
    function rawdef_define_character(data) {
      print "  define " enc96(ku_index) enc96(npoint) data > fname_plane;
    }
  
    function start_plane(iplane) {
      plane_index = iplane;
      rawdef_start_plane();
    }
    function start_ku(name, iplane, iku) {
      end_ku();
      if (iplane != plane_index)
        start_plane(iplane);
  
      ku_name = name;
      ku_index = iku;
      npoint = 0;
    }
    function end_ku() {
      if (!ku_name) return;
      if (npoint != 96) {
        print ku_name ": number of point in ku (" npoint  ") is not 96" > "/dev/stderr";
      }
    }
  
    {
      if (match($0, /^[[:space:]]*<th class="v" rowspan="6">(([0-9]+)面([0-9]+)区)<\/th>/, m)) {
        start_ku(m[1], m[2], m[3]);
      } else if ($0 ~ /^[[:space:]]*<td class="blnkn"><\/td>/) {
        npoint++;
      } else if (match($0, /^[[:space:]]*<td([[:space:]]+[-[:alnum:]]+="[^"]*"|[[:space:]])*>(.+)<\/td>/, m)) {
        cell_text = m[2];
        if (match(cell_text, /^&#x([0-9a-fA-F]+);$/, m)) {
          sub(/^0+/, m[1]);
          rawdef_define_character("<U+" m[1] ">");
        } else if (cell_text == "╂") {
          # 何故か一文字だけ実体参照ではなくて直接記入されている
          rawdef_define_character("<U+2542>");
        } else if (match(cell_text, /^(&#x[0-9a-fA-F]+;)+$/, m)) {
          gsub(/&#x/, "<U+", cell_text);
          gsub(/;/, ">", cell_text);
          rawdef_define_character(cell_text);
        }
        npoint++;
      }
    }
    END { end_ku(); }
  ' "$input"
}

function process {
  local input=$1
  local -x outfile=$2
  sed 's#\(</td>\)\([[:space:]]*<td\b\)#\1\n\2#g' "$input" | awk '
    function enc96(value) {
      value += 32;
      if (value == 32) return "<SP>";
      if (value == 127) return "<DEL>";
      return sprintf("%c", value);
    }

    function start_ku(ku) {
      sub(/^0+/, "", ku);
      ku_name = ku;
      ku_nten = 0;
    }
    function terminate_ku() {
      if (ku_name == "") return;
      if (ku_nten != 96) {
        print FILENAME ":" NR ":number of characters in ku" ku_name " table (" ku_nten ") is not 96" >"/dev/stderr"
      }
    }

    function add_ten(ch) {
      if (ch != "") {
        gsub(/&#x0*/, "<U+", ch);
        gsub(/;/, ">", ch);
        print "  define " enc96(ku_name) enc96(ku_nten) ch > fname_rawdef;
      }
      ku_nten++;
    }

    BEGIN {
      fname_rawdef = ENVIRON["outfile"];
    }
    {
      if (match($0, /<th class="v" rowspan="6">([0-9]+)区?<\/th>/, m)) {
        terminate_ku();
        start_ku(m[1]);
      } else if (match($0, /<td class="(blnk|cha)n?">(.*)<\/td>/, m)) {
        cell_content = m[2];
        if (match(cell_content, /(&#x[[:xdigit:]]+;)+|^$/, m)) {
          add_ten(m[1]);
        } else if (cell_content ~ /^[　ｐｑｒｓｔｕｖｗｘｙｚ｛｜｝￣]$/) {
          add_ten(cell_content);
        } else {
          add_ten("&#xFFFD;");
          print "ignore \"" cell_content "\"";
        }
      }
    }
    END {
      terminate_ku();
    }
  '
}

function process-cns {
  local input=$1
  local -x outfile=$2
  awk '
    BEGIN {
      outfile_format = ENVIRON["outfile"];
    }

    function update_plane(plane) {
      if (plane_name == plane) return;

      if (plane_name != "") {
        close(fname_plane);
      }

      plane_name = plane;
      plane_index = strtonum("0x" plane);
      if (plane_name != "") {
        fname_plane = sprintf(outfile_format, plane_index);
      }
    }
    function enc96(value) {
      value += 32;
      if (value == 32) return "<SP>";
      if (value == 127) return "<DEL>";
      return sprintf("%c", value);
    }
    function hex2asc(value) {
      return enc96(strtonum("0x" value) - 32);
    }

    match($1, /^T(.)-(..)(..)$/, m) {
      update_plane(m[1]);
      code = hex2asc(m[2]) hex2asc(m[3]);
      sub(/\r$/, "", $4);
      char = "<" $4 ">";
      print "  define " code char > fname_plane;
    }

    END {
      update_plane("");
    }
  ' "$input"
}

echo JISX0213...
url0213='https://www.asahi-net.or.jp/~ax2s-kmtn/ref/jisx0213/index.html'
file0213=$OUTDIR/iso2022.jisx0213.html
[[ -s "$file0213" ]] || curl -o "$file0213" "$url0213"
process-0213 "$file0213" "$OUTDIR/iso2022.jisx0213-plane%d.def"

echo JISX0212...
url0212='https://www.asahi-net.or.jp/~ax2s-kmtn/ref/jisx0212/index.html'
file0212=$OUTDIR/iso2022.jisx0212.html
[[ -s "$file0212" ]] || curl -o "$file0212" "$url0212"
process "$file0212" "$OUTDIR/iso2022.jisx0212-plane.def"

echo GB2312...
url2312='https://www.asahi-net.or.jp/~ax2s-kmtn/ref/gb2312-80.html'
file2312=$OUTDIR/iso2022.gb2312.html
[[ -s "$file2312" ]] || curl -o "$file2312" "$url2312"
process "$file2312" "$OUTDIR/iso2022.gb2312-plane.def"

echo KSC5601...
url5601='https://www.asahi-net.or.jp/~ax2s-kmtn/ref/ksx1001.html'
file5601=$OUTDIR/iso2022.ksc5601.html
[[ -s "$file5601" ]] || curl -o "$file5601" "$url5601"
process "$file5601" "$OUTDIR/iso2022.ksc5601-plane.def"

echo KPS9566...
url9566='https://www.asahi-net.or.jp/~ax2s-kmtn/ref/kps9566-97.html'
file9566=$OUTDIR/iso2022.ksc9566.html
[[ -s "$file9566" ]] || curl -o "$file9566" "$url9566"
process "$file9566" "$OUTDIR/iso2022.kps9566-plane.def"

echo CNS11643...
url11643='https://raw.githubusercontent.com/cjkvi/cjkvi-data/master/cns2ucs.txt'
file11643=$OUTDIR/iso2022.cns11643.txt
[[ -s "$file11643" ]] || curl -o "$file11643" "$url11643"
process-cns "$file11643" "$OUTDIR/iso2022.cns11643-plane%d.def"
