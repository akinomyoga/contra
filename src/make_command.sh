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

    $1 == "flags" {
      flags[flags_count, "Name"] = $2;
      flags[flags_count, "Mode"] = $3;
      flags[flags_count, "Value"] = $4;
      flags_count++;
      next;
    }
    $1 == "accessor" {
      accessors[accessor_count, "Name"] = $2;
      accessors[accessor_count, "Mode"] = $3;
      accessors[accessor_count, "Value"] = $4;
      accessor_count++;
      next;
    }

    function print_def(file) {
      print "/* automatically generated from term.mode.def */" > file;
      for (i = 0; i < flags_count; i++)
        printf("%-25s = %5d,\n", flags[i, "Name"], i) > file;
      for (i = 0; i < accessor_count; i++)
        printf("%-25s = %5d | accessor_flag,\n", accessors[i, "Name"], i) > file;
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
    function print_reg(file, _, i, mode, param) {
      print "/* automatically generated from term.mode.def */" > file;
      for (i = 0; i < flags_count; i++)
        print_reg_entry(file, flags[i, "Name"], flags[i, "Mode"]);
      for (i = 0; i < accessor_count; i++)
        print_reg_entry(file, accessors[i, "Name"], accessors[i, "Mode"]);
    }
    function print_init(file, _, i, name, value) {
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

    END {
      print_def("../out/gen/term.mode_def.hpp");
      print_reg("../out/gen/term.mode_register.hpp");
      print_init("../out/gen/term.mode_init.hpp");
    }
  ' ansi/term.mode.def
}
sub:generate-mode-defs

