#include <cstdio>
#include "line.h"

int main() {
  using namespace contra::ansi_term;
  line_t line;
  line.debug_init("hello world\n");
  line.debug_print(stdout);

  return 0;
}
