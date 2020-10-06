#include "contradef.hpp"
#include <cstdio>
#include <cstring>
#include <sstream>
#include <cstdarg>
#include <memory>
#include "enc.utf8.hpp"

#include <unistd.h>
#include <fcntl.h>

namespace contra {

  class empty_device: public idevice {
  public:
    virtual void dev_write(char const* data, std::size_t size) override {
      contra_unused(data);
      contra_unused(size);
    }
  };
  class file_device: public idevice {
    FILE* file;
  public:
    file_device(FILE* file): file(file) {}
    virtual void dev_write(char const* data, std::size_t size) override {
      std::fwrite(data, 1, size, file);
    }
  };
  static std::unique_ptr<idevice> errdev_instance;
  idevice* errdev() { return errdev_instance.get(); }

  void initialize_errdev() {
    int const fd = fileno(stderr);
    if (fd != -1 && fcntl(fd, F_GETFD) != -1) {
      errdev_instance.reset(new file_device(stderr));
      return;
    }
    errdev_instance.reset(new empty_device);
  }

  int xprint(idevice* dev, char c) {
    dev->dev_write(&c, 1);
    return 1;
  }
  int xprint(idevice* dev, const char* str) {
    int const result = std::strlen(str);
    if (result > 0) dev->dev_write(str, result);
    return result;
  }
  int xprintf(idevice* dev, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buff[1024];
    int const result = std::vsprintf(buff, fmt, args);
    va_end(args);
    if (result > 0) dev->dev_write(buff, result);
    return result;
  }


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

  void print_key(key_t key, idevice* dev) {
    key_t const code = key & _character_mask;

    std::ostringstream str;
    xprint(dev, "key: ");
    if (key & modifier_alter) xprint(dev, "A-");
    if (key & modifier_hyper) xprint(dev, "H-");
    if (key & modifier_super) xprint(dev, "s-");
    if (key & modifier_control) xprint(dev, "C-");
    if (key & modifier_meta) xprint(dev, "M-");
    if (key & modifier_shift) xprint(dev, "S-");

    if (code < _key_base) {
      switch (code) {
      case ascii_bs:  xprint(dev, "BS" ); break;
      case ascii_ht:  xprint(dev, "TAB"); break;
      case ascii_cr:  xprint(dev, "RET"); break;
      case ascii_esc: xprint(dev, "ESC"); break;
      case ascii_sp:  xprint(dev, "SP" ); break;
      default:
        contra::encoding::put_u8((char32_t) code, dev);
        break;
      }
      xprint(dev, '\n');
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
        xprint(dev, name);
        xprint(dev, '\n');
      } else {
        xprintf(dev, "<unknown %08x>\n", code);
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

  int fileprintf(const char* filename, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    std::FILE* file = std::fopen(filename, "a");
    int const ret = std::vfprintf(file, fmt, args);
    va_end(args);
    std::fclose(file);
    return ret;
  }
}
