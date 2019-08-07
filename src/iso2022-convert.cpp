#include <string>
#include <iostream>
#include "enc.utf8.hpp"

/*
 * COMBINING 文字が在ると Emacs での編集に難があるので、
 * UTF-8 の文字で COMBINING 文字だけを <U+XXXX> の形式に変換する。
 */

int main() {
  std::string line;
  std::vector<char32_t> buffer;
  while (std::getline(std::cin, line)) {
    char const* p = &line[0];
    char const* pN = p + line.size();
    buffer.resize(line.size());
    char32_t* q = &buffer[0];
    char32_t* qN = q + line.size();
    std::uint64_t state = 0;
    contra::encoding::utf8_decode(p, pN, q, qN, state);
    if (state) *q++ = 0xFFFD;

    for (char32_t* q1 = &buffer[0]; q1 != q; q1++) {
      char32_t const u = *q1;
      // if (0x20 <= u && u < 0x80) {
      //   std::putc((char) u, stdout);
      // } else if (0xA0 <= u && u < 0xFF) {
      //   contra::encoding::put_u8(u, stdout);
      // } else
      //   std::fprintf(stdout, "U+%04X", u);

      if (0x300 <= u && u < 0x370) {
        // combining characrters
        std::fprintf(stdout, "<U+%04X>", (unsigned) u);
      } else {
        contra::encoding::put_u8(u, stdout);
      }
    }
    std::fprintf(stdout, "\n");
  }
  std::fflush(stdout);
  return 0;
}
