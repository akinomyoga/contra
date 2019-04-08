// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef CONTRA_ANSI_UTF8_HPP
#define CONTRA_ANSI_UTF8_HPP
#include <cstdio>

namespace contra {
namespace encoding {

  // for debugging
  bool put_u8(char32_t c, std::FILE* file) {
    if (c<0x80) {
      std::putc(c & 0x7F, file);
    } else if (c<0x0800) {
      std::putc(0xC0 | (c >> 6  & 0x1F), file);
      std::putc(0x80 | (c       & 0x3F), file);
    } else if (c<0x00010000) {
      std::putc(0xE0 | (c >> 12 & 0x0F), file);
      std::putc(0x80 | (c >> 6  & 0x3F), file);
      std::putc(0x80 | (c       & 0x3F), file);
    } else if (c<0x00200000) {
      std::putc(0xF0 | (c >> 18 & 0x07), file);
      std::putc(0x80 | (c >> 12 & 0x3F), file);
      std::putc(0x80 | (c >> 6  & 0x3F), file);
      std::putc(0x80 | (c       & 0x3F), file);
    } else if (c<0x04000000) {
      std::putc(0xF8 | (c >> 24 & 0x03), file);
      std::putc(0x80 | (c >> 18 & 0x3F), file);
      std::putc(0x80 | (c >> 12 & 0x3F), file);
      std::putc(0x80 | (c >> 6  & 0x3F), file);
      std::putc(0x80 | (c       & 0x3F), file);
    } else if (c<0x80000000) {
      std::putc(0xF8 | (c >> 30 & 0x01), file);
      std::putc(0x80 | (c >> 24 & 0x3F), file);
      std::putc(0x80 | (c >> 18 & 0x3F), file);
      std::putc(0x80 | (c >> 12 & 0x3F), file);
      std::putc(0x80 | (c >> 6  & 0x3F), file);
      std::putc(0x80 | (c       & 0x3F), file);
    } else {
      return false;
    }
    return true;
  }

}
}
#endif
