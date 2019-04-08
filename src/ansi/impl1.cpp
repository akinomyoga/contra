#include <cstdio>
#include "line.hpp"
#include "tty.hpp"

int main() {
  using namespace contra::ansi;
  {
    board_t board(5, 5);
    tty_player player(board);
    player.printt("hello world!\n");
    player.printt("hello world!\n");
    board.debug_print(stdout);
  }

  // 全角文字の挿入と上書き
  {
    board_t board(5, 3);
    tty_player player(board);
    player.printt("hello\r日\n");
    player.printt("a日本\r全\n");
    player.printt("a日本\ba");
    board.debug_print(stdout);
  }

  // タブ挿入
  {
    board_t board(40, 1);
    tty_player player(board);
    player.printt("日本\thello\tworld");
    board.debug_print(stdout);
  }

  return 0;
}
