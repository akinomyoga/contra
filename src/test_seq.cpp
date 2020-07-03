#include <cstdio>
#include <cstring>
#include "sequence.hpp"

using namespace contra;

int main() {
  sequence_print_processor processor(stdout);
  sequence_decoder_config config; // ToDo: should be simpler instance
  sequence_decoder decoder {&processor, &config};
  auto _write = [&](const char* str) {
    while (*str)
      decoder.decode_char((byte) *str++);
  };
  _write("hello world\n");
  _write("\x1bM");
  _write("\x1b[1;2:3m");
  _write("\x1b]12;hello\a");
  _write("\033Pqhello\x9c");

  _write("\033[1\n2m");
  _write("\033[1\033[2m");

  return 0;
}
