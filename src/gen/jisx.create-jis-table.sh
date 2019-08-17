#!/bin/bash

export OUTDIR=../../out/gen

file0212=$OUTDIR/jisx0212.html
file0213=$OUTDIR/jisx0213.html

function process-0213 {
  awk '
    BEGIN {
      outdir = ENVIRON["OUTDIR"];
      if (outdir == "") outdir = ".";
    }

    function enc96(value) {
      value += 32;
      if (value == 32) return "<SP>";
      if (value == 127) return "<DEL>";
      return sprintf("%c", value);
    }
  
    function rawdef_start_plane() {
      if (fname_plane) close(fname_plane);
      fname_plane = outdir "/jisx.plane0213-" plane_index ".def";
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
  ' "$file0213"
}

# https://www.asahi-net.or.jp/~ax2s-kmtn/ref/jisx0212/index.html
function process-0212 {
  sed 's#\(</td>\)\([[:space:]]*<td\b\)#\1\n\2#' "$file0212" | awk '
    function enc96(value) {
      value += 32;
      if (value == 32) return "<SP>";
      if (value == 127) return "<DEL>";
      return sprintf("%c", value);
    }

    function start_ku(ku) {
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
      outdir = ENVIRON["OUTDIR"];
      fname_rawdef = outdir "/jisx.plane0212.def";
    }
    {
      if (match($0, /<th class="v" rowspan="6">([0-9]+)<\/th>/, m)) {
        terminate_ku();
        start_ku(m[1]);
      } else if (match($0, /<td class="(blnkn|chan)">(.*)<\/td>/, m)) {
        if (m[2] ~ /(&#x[[:xdigit:]]+;)*/)
          add_ten(m[2]);
        else
          print "?" m[2] "?";
      }
    }
    END {
      terminate_ku();
    }
  '
}

[[ -s "$file0212" ]] ||
  curl -o "$file0212" https://www.asahi-net.or.jp/~ax2s-kmtn/ref/jisx0212/index.html
[[ -s "$file0213" ]] ||
  curl -o "$file0213" https://www.asahi-net.or.jp/~ax2s-kmtn/ref/jisx0213/index.html
process-0212
process-0213
