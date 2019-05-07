// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef contra_enc_utf8_hpp
#define contra_enc_utf8_hpp
#include <cstdio>
#include <cstdint>
#include <vector>
#include "contradef.hpp"

namespace contra {
namespace encoding {

  // for debugging
  bool put_u8(char32_t c, std::FILE* file);
  bool put_u8(char32_t c, std::vector<byte>& buffer);

  void utf8_decode(char const*& ibeg, char const* iend, char32_t*& obeg, char32_t* oend, std::uint64_t& state, std::int32_t error_char = 0xFFFD);

}
}
#endif
