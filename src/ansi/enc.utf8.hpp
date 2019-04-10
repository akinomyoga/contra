// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef CONTRA_ANSI_ENC_UTF8_HPP
#define CONTRA_ANSI_ENC_UTF8_HPP
#include <cstdio>
#include <cstdint>

namespace contra {
namespace encoding {

  // for debugging
  bool put_u8(char32_t c, std::FILE* file);

  void utf8_decode(char const*& ibeg, char const* iend, char32_t*& obeg, char32_t* oend, std::uint64_t& state, std::int32_t error_char = -1);

}
}
#endif
