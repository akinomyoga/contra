#include <cstdio>
#include "line.hpp"
#include "term.hpp"

int main() {
  using namespace contra::ansi;
  {
    board_t board(5, 5);
    tty_player play(board);
    play.printt("hello world!\n");
    play.printt("hello world!\n");
    board.debug_print(stdout);
  }

  // 全角文字の挿入と上書き
  {
    board_t board(5, 3);
    tty_player play(board);
    play.printt("hello\r日\n");
    play.printt("a日本\r全\n");
    play.printt("a日本\ba");
    board.debug_print(stdout);
  }

  // タブ挿入
  {
    board_t board(40, 1);
    tty_player play(board);
    play.printt("日本\thello\tworld");
    board.debug_print(stdout);
  }

  return 0;
}
