#include <cstdio>
#include "line.hpp"
#include "term.hpp"

void do_test() {
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

  // to_presentation_position/to_data_position
  {
    board_t board(40, 1);
    tty_player play(board);
    auto check_conversion = [&] (curpos_t xdata, curpos_t xpres) {
      mwg_check(board.to_data_position(0, xpres) == xdata);
      mwg_check(board.to_presentation_position(0, xdata) == xpres);
    };

    play.printt("SDS(1)----\x1b[1]---\x1b[0]----\r");
    check_conversion(6 , 6 );
    check_conversion(7 , 7 );
    check_conversion(10, 10);
    check_conversion(11, 11);
    check_conversion(13, 13);
    check_conversion(14, 14);
    play.printt("SDS(2)----\x1b[2]---\x1b[0]----\r");
    check_conversion(6 , 6 );
    check_conversion(7 , 7 );
    check_conversion(10, 12);
    check_conversion(11, 11);
    check_conversion(13, 13);
    check_conversion(14, 14);
    play.printt("SRS(2)----\x1b[1[---\x1b[0[----\r");
    check_conversion(6 , 6 );
    check_conversion(7 , 7 );
    check_conversion(10, 12);
    check_conversion(11, 11);
    check_conversion(13, 13);
    check_conversion(14, 14);
    play.printt("--\x1b[2]---\x1b[1[----\x1b[0]---\r"); // ..SDS(2)..SRS(1)..SDS(0)..
    check_conversion(1, 1);
    check_conversion(2, 8);
    check_conversion(4, 6);
    check_conversion(5, 2);
    check_conversion(8, 5);
    check_conversion(9, 9);
    check_conversion(10, 10);
    play.printt("--\x1b[2]--\x1b[1]--\x1b[2]--\x1b[0]--\x1b[2]--\x1b[0]--\x1b[0]--\x1b[0]--\r"); // ..[..[..[..]..[..]..]..]..
    check_conversion( 0,  0);
    check_conversion( 2, 15);
    check_conversion( 4,  4);
    check_conversion( 6,  7);
    check_conversion( 8,  8);
    check_conversion(10, 11);
    check_conversion(12, 12);
    check_conversion(14,  3);
    check_conversion(16, 16);
  }
}
int main() {
  try {
    do_test();
  } catch(mwg::assertion_error& e) {}
  return 0;
}
