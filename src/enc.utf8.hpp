// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef contra_enc_utf8_hpp
#define contra_enc_utf8_hpp
#include <cstdio>
#include <cstdint>
#include <vector>
#include <string>
#include "contradef.hpp"

namespace contra {
namespace encoding {

  // for debugging
  bool put_u8(char32_t c, std::FILE* file);
  bool put_u8(char32_t c, std::vector<byte>& buffer);

  constexpr bool is_surrogate(char16_t code) { return 0xD800 <= code && code < 0xE000; }
  constexpr bool is_high_surrogate(char16_t code) { return 0xD800 <= code && code < 0xDC00; }
  constexpr bool is_low_surrogate(char16_t code) { return 0xDC00 <= code && code < 0xE000; }

  void utf8_decode(char const*& ibeg, char const* iend, char32_t*& obeg, char32_t* oend, std::uint64_t& state, std::int32_t error_char = 0xFFFD);

  void utf16_decode(char16_t const* ibeg, char16_t const* iend, std::vector<char32_t>& buffer, std::uint64_t& state, std::int32_t error_char = 0xFFFD);
  void utf16_decode(char16_t const* ibeg, char16_t const* iend, std::u32string& buffer, std::uint64_t& state, std::int32_t error_char = 0xFFFD);
  void utf16_encode(char32_t const* ibeg, char32_t const* iend, std::u16string& buffer, std::uint64_t& state, std::int32_t error_char = 0xFFFD);
}
}
#endif
