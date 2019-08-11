#!/bin/bash

export OUTDIR=../../out/gen

awk '
  BEGIN {
    BINMODE = 3;
    outdir = ENVIRON["OUTDIR"];
    if (outdir == "") outdir = ".";
    fname_binsh = outdir "/jisx0213.bin.sh";
    fname_cpp = outdir "/jisx0213.cpp";
    fname_def = outdir "/jisx0213.def";
    fname_raw = outdir "/jisx0213raw.def";
  }

  function rawdef_start_plane() {
    print "MB94(2,X) ISO-IR-XXX Plane " plane_index > fname_raw;
  }
  function rawdef_define_character(data) {
    print "  define " ascii(ku_index) ascii(npoint) data > fname_raw;
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
' jisx0213.html
