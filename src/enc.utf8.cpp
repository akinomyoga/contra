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

  // ToDo: 注意: 非正規UTF-8のチェックをしていない。
  void utf8_decode(char const*& ibeg, char const* iend, char32_t*& obeg, char32_t* oend, std::uint64_t& state, std::int32_t error_char) {
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
          }
          continue;
        } else {
          mode = 0;
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

}
}
