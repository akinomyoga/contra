#include "contradef.hpp"
#include <cstdio>
#include <cstring>
#include <sstream>
#include "enc.utf8.hpp"

namespace contra {

  const char* get_ascii_name(char32_t value) {
    static const char* c0names[32] = {
      "NUL", "SOH", "STK", "ETX", "EOT", "ENQ", "ACK", "BEL",
      "BS" , "HT" , "LF" , "VT" , "FF" , "CR" , "SO" , "SI" ,
      "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
      "CAN", "EM ", "SUB", "ESC", "FS" , "GS" , "RS" , "US" ,
    };
    static const char* c1names[32] = {
      "PAD" , "HOP" , "BPH" , "NBH" , "IND" , "NEL" , "SSA" , "ESA" ,
      "HTS" , "HTJ" , "VTS" , "PLD" , "PLU" , "RI"  , "SS2" , "SS3" ,
      "DCS" , "PU1" , "PU2" , "STS" , "CCH" , "MW"  , "SPA" , "EPA" ,
      "SOS" , "SGCI", "SCI" , "CSI" , "ST"  , "OSC" , "PM"  , "APC" ,
    };

    if (value < 0x20)
      return c0names[value];
    else if (0x80 <= value && value < 0xA0)
      return c1names[value - 0x80];
    else if (value == 0x20)
      return "SP";
    else if (value == 0x7F)
      return "DEL";
    else
      return nullptr;
  }

  void print_key(key_t key, std::FILE* file) {
    key_t const code = key & _character_mask;

    std::ostringstream str;
    if (key & modifier_alter) str << "A-";
    if (key & modifier_hyper) str << "H-";
    if (key & modifier_super) str << "s-";
    if (key & modifier_control) str << "C-";
    if (key & modifier_meta) str << "M-";
    if (key & modifier_shift) str << "S-";
    std::fprintf(file, "key: %s", str.str().c_str());

    if (code < _key_base) {
      switch (code) {
      case ascii_bs: std::fprintf(file, "BS"); break;
      case ascii_ht: std::fprintf(file, "TAB"); break;
      case ascii_cr: std::fprintf(file, "RET"); break;
      case ascii_esc: std::fprintf(file, "ESC"); break;
      case ascii_sp: std::fprintf(file, "SP"); break;
      default:
        contra::encoding::put_u8((char32_t) code, file);
        break;
      }
      std::putc('\n', file);
    } else {
      static const char* table[] = {
        NULL,
        "f1", "f2", "f3", "f4", "f5", "f6",
        "f7", "f8", "f9", "f10", "f11", "f12",
        "f13", "f14", "f15", "f16", "f17", "f18",
        "f19", "f20", "f21", "f22", "f23", "f24",
        NULL, NULL, NULL, NULL, NULL, NULL,
        "insert", "delete", "home", "end", "prior", "next",
        "begin", "left", "right", "up", "down",
        "kp0", "kp1", "kp2", "kp3", "kp4",
        "kp5", "kp6", "kp7", "kp8", "kp9",
        "kpdec", "kpsep", "kpmul", "kpadd", "kpsub", "kpdiv",
        "focus", "blur",
        "kpf1", "kpf2", "kpf3", "kpf4", "kpent", "kpeq",
      };
      std::uint32_t const index = code - _key_base;
      const char* name = index <= std::size(table) ? table[index] : NULL;
      if (name) {
        std::fprintf(file, "%s\n", name);
      } else {
        std::fprintf(file, "unknown %08x\n", code);
      }
    }
  }

  bool parse_modifier(key_t& value, const char* text) {
    if (std::strcmp(text, "none") == 0)
      value = 0;
    else if (std::strcmp(text, "shift") == 0)
      value = modifier_shift;
    else if (std::strcmp(text, "meta") == 0)
      value = modifier_meta;
    else if (std::strcmp(text, "control") == 0)
      value = modifier_control;
    else if (std::strcmp(text, "alter") == 0)
      value = modifier_alter;
    else if (std::strcmp(text, "super") == 0)
      value = modifier_super;
    else if (std::strcmp(text, "hyper") == 0)
      value = modifier_hyper;
    else if (std::strcmp(text, "application") == 0)
      value = modifier_application;
    else
      return false;
    return true;
  }

}
