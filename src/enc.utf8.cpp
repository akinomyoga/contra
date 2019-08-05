#include <cstdint>
#include <cstdio>
#include <vector>
#include <string>
#include <mwg/except.h>
#include "enc.utf8.hpp"
#include "contradef.hpp"

namespace contra {
namespace encoding {

  template<typename Function>
  static bool put_u8_impl(char32_t c, Function put) {
    std::uint32_t code = c;
    if (code < 0x80) {
      put(code & 0x7F);
    } else if (code < 0x0800) {
      put(0xC0 | (code >> 6  & 0x1F));
      put(0x80 | (code       & 0x3F));
    } else if (code < 0x00010000) {
      put(0xE0 | (code >> 12 & 0x0F));
      put(0x80 | (code >> 6  & 0x3F));
      put(0x80 | (code       & 0x3F));
    } else if (code < 0x00200000) {
      put(0xF0 | (code >> 18 & 0x07));
      put(0x80 | (code >> 12 & 0x3F));
      put(0x80 | (code >> 6  & 0x3F));
      put(0x80 | (code       & 0x3F));
    } else if (code < 0x04000000) {
      put(0xF8 | (code >> 24 & 0x03));
      put(0x80 | (code >> 18 & 0x3F));
      put(0x80 | (code >> 12 & 0x3F));
      put(0x80 | (code >> 6  & 0x3F));
      put(0x80 | (code       & 0x3F));
    } else if (code < 0x80000000) {
      put(0xF8 | (code >> 30 & 0x01));
      put(0x80 | (code >> 24 & 0x3F));
      put(0x80 | (code >> 18 & 0x3F));
      put(0x80 | (code >> 12 & 0x3F));
      put(0x80 | (code >> 6  & 0x3F));
      put(0x80 | (code       & 0x3F));
    } else {
      return false;
    }
    return true;
  }

  bool put_u8(char32_t c, std::vector<byte>& buffer) {
    return put_u8_impl(c, [&] (byte b) { buffer.emplace_back(b); });
  }
  bool put_u8(char32_t c, std::FILE* file) {
    return put_u8_impl(c, [=] (byte b) { std::putc(b, file); });
  }

  void utf8_decode(char const*& ibeg, char const* iend, char32_t*& obeg, char32_t* oend, std::uint64_t& state, std::int32_t error_char) {
    // flush
    if (ibeg == iend) {
      if (state) {
        if (error_char >= 0)
          *obeg++ = error_char;
        state = 0;
      }
      return;
    }

    std::uint_fast32_t code = state & 0xFFFFFFFF;
    std::uint_fast32_t norm = state >> 32 & 0xFFFF;
    std::uint_fast32_t mode = state >> 48;

    // mode は残りの後続バイトの数を保持する。
    // code は mode が有限の時に意味を持ち、
    // 現在組み立て中の文字のコードを表す。
    // norm は mode が有限の時に意味を持ち、
    // そのマルチバイト表現で表される文字が、
    // 最低でも norm bit では表現しきれない事を要求する。

    while (ibeg != iend && obeg != oend) {
      byte const c = *ibeg++;

      if (mode) {
        if ((c & 0xC0) == 0x80) {
          code = code << 6 | (c & 0x3F);
          if (--mode == 0) {
            if (code >> norm && code < 0x00110000)
              *obeg++ = code;
            else if (error_char >= 0)
              *obeg++ = error_char;
            norm = 0;
            code = 0;
          }
          continue;
        } else {
          mode = 0;
          norm = 0;
          code = 0;
          if (error_char >= 0) {
            *obeg++ = error_char;
            if (obeg == oend) {
              ibeg--;
              break;
            }
          }
        }
      }

      // 最初のバイト
      if (c < 0xF0) {
        if (c < 0xC0) {
          if (c < 0x80) {
            // 0xxxxxxx [00-80]
            *obeg++ = c;
          } else {
            // 10xxxxxx [80-C0]
            if (error_char >= 0) *obeg++ = error_char;
          }
        } else {
          if (c < 0xE0) {
            // 110xxxxx [C0-E0]
            mode = 1;
            code = c & 0x1F;
            norm = 7;
          } else {
            // 1110xxxx [E0-F0]
            mode = 2;
            code = c & 0x0F;
            norm = 11;
          }
        }
      } else {
        if (c < 0xFC) {
          if (c < 0xF8) {
            // 11110xxx [F0-F8]
            mode = 3;
            code = c & 0x07;
            norm = 16;
          } else {
            // 111110xx [F8-FC]
            mode = 4;
            code = c & 0x03;
            norm = 21;
          }
        } else {
          if(c < 0xFE) {
            // 1111110x [FC-FE]
            mode = 5;
            code = c & 0x01;
            norm = 26;
          } else {
            // 0xFE 0xFF
            if (error_char >= 0) *obeg++ = error_char;
          }
        }
      }
    }

    state = std::uint64_t(mode << 16 | norm) << 32 | code;
    return;
  }

  template<typename ProcessChar32>
  void utf16_decode_impl(char16_t const* ibeg, char16_t const* iend, ProcessChar32 proc, std::uint64_t& state, std::int32_t error_char) {
    char32_t high = (char32_t) state;
    mwg_assert(!high || is_high_surrogate(high));

    while (ibeg < iend) {
      char16_t const u = *ibeg++;
      if (high) {
        if (is_low_surrogate(u)) {
          std::uint32_t const offset = (high & 0x3FF) << 10 | (u & 0x3FF);
          proc(char32_t(0x1000 + offset));
          high = 0;
          continue;
        } else {
          if (error_char >= 0) proc((char32_t) error_char);
        }
        high = 0;
      }

      if (!is_surrogate(u)) {
        proc(u);
      } else if (is_high_surrogate(u)) {
        high = u;
      } else {
        if (error_char >= 0)
          proc((char32_t) error_char);
      }
    }

    state = (std::uint64_t) high;
  }
  void utf16_decode(char16_t const* ibeg, char16_t const* iend, std::u32string& buffer, std::uint64_t& state, std::int32_t error_char) {
    return utf16_decode_impl(ibeg, iend, [&] (char32_t u) { buffer.append(1, u); }, state, error_char);
  }
  void utf16_decode(char16_t const* ibeg, char16_t const* iend, std::vector<char32_t>& buffer, std::uint64_t& state, std::int32_t error_char) {
    return utf16_decode_impl(ibeg, iend, [&] (char32_t u) { buffer.push_back(u); }, state, error_char);
  }

  void utf16_encode(char32_t const* ibeg, char32_t const* iend, std::u16string& buffer, std::uint64_t& state, std::int32_t error_char) {
    contra_unused(state);
    while (ibeg < iend) {
      char32_t const u = *ibeg++;
      if (u < (char32_t) 0x10000) {
        buffer.append(1, char16_t(u));
      } else if (u <= (char32_t) 0x10FFFF) {
        std::uint32_t offset = u - 0x10000;
        buffer.append(1, char16_t(0xD800 | (offset >> 10 & 0x3FF)));
        buffer.append(1, char16_t(0xDC00 | (offset & 0x3FF)));
      } else {
        if (0 <= error_char && error_char < 0x10000)
          buffer.append(1, char16_t(error_char));
      }
    }
  }

}
}
