#!/bin/bash

export OUTDIR=../../out/gen

file0212=$OUTDIR/jisx0212.html
file0213=$OUTDIR/jisx0213.html

function process-0213 {
  awk '
    BEGIN {
      outdir = ENVIRON["OUTDIR"];
      if (outdir == "") outdir = ".";
      fname_binsh = outdir "/jisx0213.bin.sh";
      fname_cpp = outdir "/jisx0213.cpp";
      fname_def = outdir "/jisx0213.def";
      fname_raw = outdir "/jisx0213raw.def";
  
      plane_desig[1] = "Q";
      plane_desig[2] = "P";
      plane_name[1] = "ISO-IR-229 Japanese Graphic Character Set for Information Interchange, Plane 1 (Update of ISO-IR 228)";
      plane_name[2] = "ISO-IR-233 Japanese Graphic Character Set for Information Interchange --- Plane 2";
    }
  
    function rawdef_start_plane() {
      if (fname_plane) close(fname_plane);

      print "MB94(2," plane_desig[plane_index] ") " plane_name[plane_index] > fname_raw;
      fname_plane = outdir "/jisx.plane0213-" plane_index ".def";
    }
    function rawdef_define_character(data) {
      print "  define " ascii(ku_index) ascii(npoint) data > fname_raw;
      print "  define " ascii(ku_index) ascii(npoint) data > fname_plane;
    }
  
    function ascii(value) {
      value += 32;
      if (value == 32) return "<SP>";
      if (value == 127) return "<DEL>";
      return sprintf("%c", value);
    }
  
    function start_plane(iplane) {
      plane_index = iplane;
      print "MB94(2,X) ISO-IR-XXX Plane " iplane > fname_def
      rawdef_start_plane();
    }
  
    function start_ku(name, iplane, iku) {
      end_ku();
      if (iplane != plane_index)
        start_plane(iplane);
  
      ku_name = name;
      ku_index = iku;
      npoint = 0;
  
      print "  // " ku_name > fname_cpp;
    }
    function end_ku() {
      if (!ku_name) return;
  
      print "" > fname_cpp;
      if (npoint != 96) {
        print ku_name ": number of point in ku (" npoint  ") is not 96" > "/dev/stderr";
      }
    }
    function push_point(code) {
      if (npoint % 8 == 0) printf("  ") > fname_cpp;
      if ((npoint + 1) % 8 != 0) {
        printf("0x%05X, ", code) > fname_cpp;
      } else {
        printf("0x%05X,\n", code) > fname_cpp;
      }
  
      count++;
  
      code16 = code;
      if (code >= 0x10000) {
        printf("  define %s%s<U+%5x>\n", ascii(ku_index), ascii(npoint), code) > fname_def;
        code16 = 0;
      }
  
      script = sprintf("printf '\''\\x%02x\\x%02x'\''", code16 % 256, int(code16 / 256));
      print script > fname_binsh;
    }
  
  
    {
      if (match($0, /^[[:space:]]*<th class="v" rowspan="6">(([0-9]+)面([0-9]+)区)<\/th>/, m)) {
        start_ku(m[1], m[2], m[3]);
      } else if ($0 ~ /^[[:space:]]*<td class="blnkn"><\/td>/) {
        push_point(0xFFFD);
        npoint++;
      } else if (match($0, /^[[:space:]]*<td([[:space:]]+[-[:alnum:]]+="[^"]*"|[[:space:]])*>(.+)<\/td>/, m)) {
        cell_text = m[2];
        if (match(cell_text, /^&#x([0-9a-fA-F]+);$/, m)) {
          push_point(strtonum("0x" m[1]));
          sub(/^0+/, m[1]);
          rawdef_define_character("<U+" m[1] ">");
        } else if (cell_text == "╂") {
          # 何故か一文字だけ実体参照ではなくて直接記入されている
          push_point(0x2542);
          rawdef_define_character("<U+2542>");
        } else if (match(cell_text, /^(&#x[0-9a-fA-F]+;)+$/, m)) {
          gsub(/&#x/, "<U+", cell_text);
          gsub(/;/, ">", cell_text);
          print "  define " ascii(ku_index) ascii(npoint) cell_text > fname_def;
          rawdef_define_character(cell_text);
          push_point(0x0000);
        }
        npoint++;
      }
    }
    END {
      end_ku();
      print 2*count >"/dev/stderr";
      system("cd " outdir "; bash jisx0213.bin.sh > jisx0213.bin");
    }
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
