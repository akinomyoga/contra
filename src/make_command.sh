#!/bin/bash

gendir=../out/gen
[[ -d $gendir ]] || mkdir -p "$gendir"

function sub:generate-mode-defs {
  awk '
    BEGIN {
      flags_count = 0;
      accessor_count = 0;
    }

    {
      sub(/(^|[[:space:]])#.*/, "");
      sub(/^[[:space:]]+$/, "");
    }

    $1 ~ /^f/ {
      flags[flags_count, "Type"] = $1;
      flags[flags_count, "Name"] = $2;
      flags[flags_count, "Mode"] = $3;
      flags[flags_count, "Value"] = $4;
      flags_count++;
      next;
    }
    $1 ~ /^a/ {
      accessors[accessor_count, "Type"] = $1;
      accessors[accessor_count, "Name"] = $2;
      accessors[accessor_count, "Mode"] = $3;
      accessors[accessor_count, "Value"] = $4;
      accessor_count++;
      next;
    }

    function create_mode_flag(type, _, mode_flag) {
      mode_flag = "";
      if (type ~ /^a/)
        mode_flag = " | mode_flag_accessor";
      if (!(type ~ /r/))
        mode_flag = mode_flag " | mode_flag_guarded";
      if (!(type ~ /w/))
        mode_flag = mode_flag " | mode_flag_protect";
      if (type ~ /c/)
        mode_flag = mode_flag " | mode_flag_const";
      return mode_flag;
    }

    function save_def(file, _, i, mode_flag) {
      print "/* automatically generated from term.mode.def */" > file;
      for (i = 0; i < flags_count; i++) {
        mode_flag = create_mode_flag(flags[i, "Type"]);
        printf("%-25s = %5d%s,\n", flags[i, "Name"], i, mode_flag) > file;
      }
      for (i = 0; i < accessor_count; i++) {
        mode_flag = create_mode_flag(accessors[i, "Type"]);
        printf("%-25s = %5d%s,\n", accessors[i, "Name"], i, mode_flag) > file;
      }
      close(file);
    }
    function print_reg_entry(file, name, mode, _, param) {
      if (mode ~ /^ansi/) {
        param = substr(mode, 5);
        mode = substr(mode, 1, 4);
      } else if (mode ~ /^dec/) {
        param = substr(mode, 4);
        mode = substr(mode, 1, 3);
      } else return;

      printf("data_%s[%d] = %s;\n", mode, param, name) > file;
    }
    function save_reg(file, _, i, mode, param) {
      print "/* automatically generated from term.mode.def */" > file;
      for (i = 0; i < flags_count; i++)
        print_reg_entry(file, flags[i, "Name"], flags[i, "Mode"]);
      for (i = 0; i < accessor_count; i++)
        print_reg_entry(file, accessors[i, "Name"], accessors[i, "Mode"]);
    }
    function save_init(file, _, i, name, value) {
      print "/* automatically generated from term.mode.def */" > file;
      for (i = 0; i < flags_count; i++) {
        name = flags[i, "Name"];
        value = flags[i, "Value"];
        if (value == "true" || value == "false")
          printf("set_mode(%s, %s);\n", name, value) > file;
      }
      for (i = 0; i < accessor_count; i++) {
        name = accessors[i, "Name"];
        value = accessors[i, "Value"];
        if (value == "true" || value == "false")
          printf("set_mode(%s, %s);\n", name, value) > file;
      }
      close(file);
    }
    function save_dispatch_set(filename, _, i, name) {
      print "/* automatically generated from term.mode.def */" > filename;
      for (i = 0; i < accessor_count; i++) {
        name = accessors[i, "Name"];
        sub(/^mode_/, "", name);
        printf("case mode_index(mode_%s): return do_sm_%s(*m_term, value);\n", name, name) > filename;
      }
      close(filename);
    }
    function save_dispatch_rqm(filename) {
      print "/* automatically generated from term.mode.def */" > filename;
      for (i = 0; i < accessor_count; i++) {
        name = accessors[i, "Name"];
        sub(/^mode_/, "", name);
        printf("case mode_index(mode_%s): return do_rqm_%s(*m_term);\n", name, name) > filename;
      }
      close(filename);
    }

    END {
      save_def("../out/gen/term.mode_def.hpp");
      save_reg("../out/gen/term.mode_register.hpp");
      save_init("../out/gen/term.mode_init.hpp");
      save_dispatch_set("../out/gen/term.mode_set.hpp");
      save_dispatch_rqm("../out/gen/term.mode_rqm.hpp");
    }
  ' ansi/term.mode.def
}

function sub:check-memo-numbering {
  printf '%s\n' '----- BEGIN check-memo-numbering -----'
  local nline_numbered=$(awk 'done && /^\* .*\[#D[0-9]+\]/ {print;} /^  Done/ {done=1}' ../memo.txt | wc -l)
  echo "NumberOfLinesWithID: $nline_numbered"
  printf '%s\n' '----- list of items without ID -----'
  awk 'done && /^\* .*/ && !/\[#D[0-9]+\]/ {print;} /^  Done/ {done=1}' ../memo.txt
  printf '%s\n' '----- list of duplicate IDs -----'
  grep -o '\[#D[0-9]\{1,\}\]' ../memo.txt | sort | uniq -d
  printf '%s\n' '----- list of ID references -----'
  awk '/#D[0-9]+/ && !/\[#D[0-9]+\]$/ {print;}' ../memo.txt
  grc '#D[0-9]+'
  printf '%s\n' '----- END -----'
}

function is-function { declare -f "$1" &>/dev/null; }

if (($#==0)); then
  echo "usage: make_command.sh SUBCOMMAND ARGS..." >&2
  exit 2
elif is-function "sub:$1"; then
  "sub:$@"
else
  echo "unrecognized subcommand '$1'" >&2
  exit 2
fi
