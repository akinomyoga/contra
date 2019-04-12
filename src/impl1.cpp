#include <cstdio>
#include <algorithm>
#include <vector>
#include <utility>
#include "ansi/line.hpp"
#include "ansi/term.hpp"
#include "ansi/observer.tty.hpp"

void test_strings() {
  using namespace contra::ansi;

  board_t board(40, 1);
  term_t term(board);

  // to_presentation_position/to_data_position
  auto check_conversion = [&] (curpos_t xdata, curpos_t xpres) {
    mwg_check(board.to_data_position(0, xpres) == xdata);
    mwg_check(board.to_presentation_position(0, xdata) == xpres);
  };

  // calculate_data_ranges_from_presentation_range
  auto check_slice = [&] (curpos_t x1, curpos_t x2) {
    line_t::slice_ranges_t ranges1;
    std::vector<curpos_t> positions;
    for (curpos_t i = x1; i < x2; i++)
      positions.push_back(board.to_data_position(0, i));
    std::sort(positions.begin(), positions.end());
    for (curpos_t num : positions) {
      if (ranges1.size() && ranges1.back().second == num)
        ranges1.back().second = num + 1;
      else
        ranges1.push_back(std::make_pair(num, num + 1));
    }

    line_t::slice_ranges_t ranges2;
    board.line().calculate_data_ranges_from_presentation_range(ranges2, x1,  x2, board.m_width, false);

    try {
      for (std::size_t i = 0, j = 0; i < ranges1.size() && j < ranges2.size(); i++, j++) {
        while (j < ranges2.size() && ranges2[j].first == ranges2[j].second) j++;
        mwg_check(j < ranges2.size());
        mwg_check(ranges1[i].first == ranges2[j].first && ranges1[i].second == ranges2[j].second,
          "[%d %d] (result) vs [%d %d] (exp)",
          (int) ranges2[j].first, (int) ranges2[j].second, (int) ranges1[i].first, (int) ranges1[i].second);
      }
    } catch(mwg::assertion_error&) {
      std::fprintf(stderr, "slice: ");
      board.line().debug_dump();
      std::fprintf(stderr, "x1=%d x2=%d\nresult: ", (int) x1, (int) x2);
      for (auto const& range : ranges2)
        std::fprintf(stderr, "%d-%d ", range.first, range.second);
      std::putc('\n', stderr);
      throw;
    }
  };

  term.printt("SDS(1)----\x1b[1]---\x1b[0]----\r");
  check_conversion(6 , 6 );
  check_conversion(7 , 7 );
  check_conversion(10, 10);
  check_conversion(11, 11);
  check_conversion(13, 13);
  check_conversion(14, 14);
  mwg_check(board.line().find_innermost_string(10, true , board.m_width, false) == 0);
  mwg_check(board.line().find_innermost_string(10, false, board.m_width, false) == 1);
  mwg_check(board.line().find_innermost_string(13, true,  board.m_width, false) == 1);
  mwg_check(board.line().find_innermost_string(13, false, board.m_width, false) == 0);
  term.printt("SDS(2)----\x1b[2]---\x1b[0]----\r");
  check_conversion(6 , 6 );
  check_conversion(7 , 7 );
  check_conversion(10, 12);
  check_conversion(11, 11);
  check_conversion(13, 13);
  check_conversion(14, 14);
  term.printt("SRS(2)----\x1b[1[---\x1b[0[----\r");
  check_conversion(6 , 6 );
  check_conversion(7 , 7 );
  check_conversion(10, 12);
  check_conversion(11, 11);
  check_conversion(13, 13);
  check_conversion(14, 14);
  term.printt("--\x1b[2]---\x1b[1[----\x1b[0]---\r"); // ..SDS(2)..SRS(1)..SDS(0)..
  check_conversion(1, 1);
  check_conversion(2, 8);
  check_conversion(4, 6);
  check_conversion(5, 2);
  check_conversion(8, 5);
  check_conversion(9, 9);
  check_conversion(10, 10);
  check_slice(0, 3);
  mwg_check(board.line().find_innermost_string(2, true , board.m_width, false) == 0);
  mwg_check(board.line().find_innermost_string(2, false, board.m_width, false) == 1);
  mwg_check(board.line().find_innermost_string(5, true,  board.m_width, false) == 1);
  mwg_check(board.line().find_innermost_string(5, false, board.m_width, false) == 2);
  mwg_check(board.line().find_innermost_string(9, true,  board.m_width, false) == 2);
  mwg_check(board.line().find_innermost_string(9, false, board.m_width, false) == 0);

  // DATA ab[cd[ef[gh]ij[kl]mn]op]qr
  //      ..[<<[..[<<]..[<<]..]<<]..
  // PRES ab[po[ef[hg]ij[lk]mn]dc]qr
  term.printt("ab\x1b[2]cd\x1b[1]ef\x1b[2]gh\x1b[0]ij\x1b[2]kl\x1b[0]mn\x1b[0]op\x1b[0]qr\r");
  check_conversion( 0,  0);
  check_conversion( 2, 15);
  check_conversion( 4,  4);
  check_conversion( 6,  7);
  check_conversion( 8,  8);
  check_conversion(10, 11);
  check_conversion(12, 12);
  check_conversion(14,  3);
  check_conversion(16, 16);
  check_slice(0, 3);
  check_slice(0, 5);
  check_slice(0, 7);
  check_slice(0, 9);
  check_slice(0, 11);
  check_slice(7, 7);
  check_slice(7, 9);
  check_slice(7, 11);
  mwg_check(board.line().find_innermost_string( 2, true , board.m_width, false) == 0);
  mwg_check(board.line().find_innermost_string( 2, false, board.m_width, false) == 1);
  mwg_check(board.line().find_innermost_string( 4, true,  board.m_width, false) == 1);
  mwg_check(board.line().find_innermost_string( 4, false, board.m_width, false) == 2);
  mwg_check(board.line().find_innermost_string( 6, true,  board.m_width, false) == 2);
  mwg_check(board.line().find_innermost_string( 6, false, board.m_width, false) == 3);
  mwg_check(board.line().find_innermost_string( 8, true,  board.m_width, false) == 3);
  mwg_check(board.line().find_innermost_string( 8, false, board.m_width, false) == 2);
  mwg_check(board.line().find_innermost_string(10, true,  board.m_width, false) == 2);
  mwg_check(board.line().find_innermost_string(10, false, board.m_width, false) == 4);
  mwg_check(board.line().find_innermost_string(12, true , board.m_width, false) == 4);
  mwg_check(board.line().find_innermost_string(12, false, board.m_width, false) == 2);
  mwg_check(board.line().find_innermost_string(14, true , board.m_width, false) == 2);
  mwg_check(board.line().find_innermost_string(14, false, board.m_width, false) == 1);
  mwg_check(board.line().find_innermost_string(16, true , board.m_width, false) == 1);
  mwg_check(board.line().find_innermost_string(16, false, board.m_width, false) == 0);

  auto _presentation = [&] (std::vector<char32_t>& buff) {
    for (curpos_t p = 0; p < board.m_width; p++) {
      curpos_t const x = board.to_data_position(0, p);
      buff.push_back(board.line().char_at(x).value);
    }
    for (char c : buff) std::putc(c ? c : '@', stderr);
    std::putc('\n', stderr);
    // board.line().debug_dump();
  };
  auto check_ech = [&] (const char* esc, curpos_t p1, curpos_t p2) {
    term.printt(esc);

    std::vector<char32_t> before, after;
    std::fprintf(stderr, "ECH before: "); _presentation(before);
    char ech[20]; std::sprintf(ech, "\r\x1b[9l\x1b[%dD\x1b[%dC\x1b[%dX\x1b[9h\r", board.m_width, p1, p2 - p1); term.printt(ech);
    //board.line().ech(p1, p2, board.m_width, board.m_presentation_direction, board.cur.attribute, true);
    std::fprintf(stderr, "ECH after : "); _presentation(after);

    for (int i = 0; i < board.m_width; i++)
      mwg_check((p1 <= i && i < p2) || before[i] == after [i]);
    board.line().clear();
  };

  // PRES a      [[[g]ij[lk]mn]dc]qr
  // DATA a      [cd[[g]ij[kl]mn]]qr
  check_ech("ab\x1b[2]cd\x1b[1]ef\x1b[2]gh\x1b[0]ij\x1b[2]kl\x1b[0]mn\x1b[0]op\x1b[0]qr\r", 1, 7);
  // PRES ab[po[e]]        [[n]dc]qr
  // DATA ab[[e]op]        [cd[n]]qr
  check_ech("ab\x1b[2]cd\x1b[1]ef\x1b[2]gh\x1b[0]ij\x1b[2]kl\x1b[0]mn\x1b[0]op\x1b[0]qr\r", 5, 13);
  check_ech("ab\x1b[2]cd\x1b[1]ef\x1b[2]gh\x1b[0]ij\x1b[2]kl\x1b[0]mn\x1b[0]op\x1b[0]qr\r", 13, 20);
  check_ech("ab\x1b[1]cdefgh\x1b[0]ij\r", 4, 6);
  check_ech("ab\x1b[1]cd\x1b[1]efgh\x1b[0]ij\x1b[0]kl\r", 5, 7);

  board.m_presentation_direction = presentation_direction_rltb;
  check_ech("ab\x1b[2]cd\x1b[1]ef\x1b[2]gh\x1b[0]ij\x1b[2]kl\x1b[0]mn\x1b[0]op\x1b[0]qr\r", 33, 39);
  board.m_presentation_direction = presentation_direction_default;

  auto check_dch = [&] (const char* esc, curpos_t p1, curpos_t shift) {
    term.printt(esc);

    std::vector<char32_t> before, after;
    std::fprintf(stderr, "DCH before: "); _presentation(before);
    board.line().dch(p1, shift, board.m_width, board.m_presentation_direction, board.cur.attribute, true);
    std::fprintf(stderr, "DCH after : "); _presentation(after);

    // for (int i = 0; i < board.m_width; i++)
    //   mwg_check((p1 <= i && i < p2) || before[i] == after [i]);
    board.line().clear();
  };
  check_dch("ab\x1b[2]cd\x1b[1]ef\x1b[2]gh\x1b[0]ij\x1b[2]kl\x1b[0]mn\x1b[0]op\x1b[0]qr\r", 1, 6);
  check_dch("ab\x1b[2]cd\x1b[1]ef\x1b[2]gh\x1b[0]ij\x1b[2]kl\x1b[0]mn\x1b[0]op\x1b[0]qr\r", 5, 8);
  check_dch("ab\x1b[2]cd\x1b[1]ef\x1b[2]gh\x1b[0]ij\x1b[2]kl\x1b[0]mn\x1b[0]op\x1b[0]qr\r", 13, 7);
  board.m_presentation_direction = presentation_direction_rltb;
  check_dch("ab\x1b[2]cd\x1b[1]ef\x1b[2]gh\x1b[0]ij\x1b[2]kl\x1b[0]mn\x1b[0]op\x1b[0]qr\r", 33, 6);
  board.m_presentation_direction = presentation_direction_default;

  auto check_ich = [&] (const char* esc, curpos_t p1, curpos_t shift) {
    term.printt(esc);

    std::vector<char32_t> before, after;
    std::fprintf(stderr, "ICH before: "); _presentation(before);
    board.line().ich(p1, shift, board.m_width, board.m_presentation_direction, board.cur.attribute, true);
    std::fprintf(stderr, "ICH after : "); _presentation(after);

    // for (int i = 0; i < board.m_width; i++)
    //   mwg_check((p1 <= i && i < p2) || before[i] == after [i]);
    board.line().clear();
  };
  check_ich("ab\x1b[2]cd\x1b[1]ef\x1b[2]gh\x1b[0]ij\x1b[2]kl\x1b[0]mn\x1b[0]op\x1b[0]qr\r", 1, 6);
  check_ich("ab\x1b[2]cd\x1b[1]ef\x1b[2]gh\x1b[0]ij\x1b[2]kl\x1b[0]mn\x1b[0]op\x1b[0]qr\r", 5, 8);
  check_ich("ab\x1b[2]cd\x1b[1]ef\x1b[2]gh\x1b[0]ij\x1b[2]kl\x1b[0]mn\x1b[0]op\x1b[0]qr\r", 13, 7);
  board.m_presentation_direction = presentation_direction_rltb;
  check_ich("ab\x1b[2]cd\x1b[1]ef\x1b[2]gh\x1b[0]ij\x1b[2]kl\x1b[0]mn\x1b[0]op\x1b[0]qr\r", 33, 6);
  board.m_presentation_direction = presentation_direction_default;

  // test cases from src/impl1.cpp
  // // abcd_[efgh]_ij
  // term.printt("\x1b[Habcd_\x1b[2]efgh\x1b[0]_ij");
  // tester("abcd_efgh_ij", px_result1);
  // term.printt("\x1b[Habcd_\x1b[1[efgh\x1b[0[_ij");
  // tester("abcd_efgh_ij", px_result1);
  // // kl_[mnop_[qrst]_uvw]_xyz
  // term.printt("\x1b[Hkl_\x1b[2]mnop_\x1b[1]qrst\x1b[]_uvw\x1b[]_xyz");
  // tester("kl_mnop_qrst_uvw_xyz", px_result2);
  // term.printt("\x1b[Hkl_\x1b[1[mnop_\x1b[1[qrst\x1b[[_uvw\x1b[[_xyz");
  // tester("kl_mnop_qrst_uvw_xyz", px_result2);
  // term.printt("\x1b[Hk\x1b[1]l_\x1b[1[mn\x1b[2]op_\x1b[1[q\x1b[1]rs\x1b[]t\x1b[[_u\x1b[]vw\x1b[[_\x1b[]xyz");
  // tester("kl_mnop_qrst_uvw_xyz", px_result2);
}

void do_test() {
  using namespace contra::ansi;
  {
    board_t board(5, 5);
    term_t term(board);
    term.printt("hello world!\n");
    term.printt("hello world!\n");
    board.debug_print(stdout);
  }

  // 全角文字の挿入と上書き
  {
    board_t board(5, 3);
    term_t term(board);
    term.printt("hello\r日\n");
    term.printt("a日本\r全\n");
    term.printt("a日本\ba");
    board.debug_print(stdout);
  }

  // タブ挿入
  {
    board_t board(40, 1);
    term_t term(board);
    term.printt("日本\thello\tworld");
    board.debug_print(stdout);
  }
}

void test_sgr() {
  using namespace contra::ansi;
  board_t board(20, 10);

  term_t term(board);
  term.printt("\x1b[38:5:196;4mabcdefghijklmnopqrstuvwxyz");
  term.printt("\x1b[38:5:202mabcdefghijklmnopqrstuvwxyz");
  term.printt("\x1b[38:5:220;24mabcdefghijklmnopqrstuvwxyz");
  term.printt("\x1b[38:5:154mabcdefghijklmnopqrstuvwxyz");
  term.printt("\x1b[38:5:63mabcdefghijklmnopqrstuvwxyz\n");
  term.printt("\x1b[39m");
  term.printt("Hello, world!\n");
  term.printt("Thank you!\n");

  term.printt("\nA\x1b[AB\x1b[BC\x1b[2DD\x1b[0CE");
  term.printt("\x1b[HA\x1b[2;2HB");

  termcap_sgr_type sgrcap;
  sgrcap.initialize();

  tty_observer target(stdout, &sgrcap);
  target.print_screen(board);
}

int main() {
  try {
    do_test();
    test_strings();
    test_sgr();
  } catch(mwg::assertion_error& e) {}
  return 0;
}
