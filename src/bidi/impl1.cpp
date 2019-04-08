#include <cstdio>
#include "line.h"
#include "tty.h"

int main() {
  using namespace contra::ansi;
  board_t board(5, 11);
  tty_player player(board);
  player.printt("hello world!\n");
  player.printt("hello world!\n");
  player.printt("hello world!\n");
  player.printt("hello world!\n");
  player.printt("hello world!\n");
  board.debug_print(stdout);

  return 0;
}
