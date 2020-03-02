#include <cstdio>
#include <algorithm>
#include <vector>
#include <utility>
#include "ansi/line.hpp"
#include "ansi/term.hpp"
#include "ttty/buffer.hpp"

namespace contra {
  static inline int strcmp(const char32_t* a, const char32_t* b) {
    for (; *a && *b; a++, b++) {
      std::uint32_t a1(*a);
      std::uint32_t b1(*b);
      if (a1 != b1) return a1 > b1 ? 1: -1;
    }
    return *a ? 1 : *b ? -1 : 0;
  }
}
using namespace contra::ansi;

std::u32string get_data_content(board_t& board) {
  std::vector<char32_t> buff;
  for (curpos_t x = 0; x < board.width(); x++) {
    //curpos_t const x = board.to_presentation_position(board.cur.y, p);
    char32_t c = board.line().char_at(x).value;
    if (!(c & contra::charflag_wide_extension))
      buff.push_back(c == U'\0' ? U'@' : c);
  }
  return std::u32string(buff.begin(), buff.end());
}
std::u32string get_presentation_content(board_t& board) {
  std::vector<char32_t> buff;
  for (curpos_t p = 0; p < board.width(); p++) {
    curpos_t const x = board.to_data_position(board.cur.y(), p);
    char32_t c = board.line().char_at(x).value;
    if (!(c & contra::charflag_wide_extension))
      buff.push_back(c == U'\0' ? U'@' : c);
  }
  if (board.line_r2l())
    std::reverse(buff.begin(), buff.end());
  return std::u32string(buff.begin(), buff.end());
}
bool check_strings(const char* title, const char32_t* result, const char32_t* expected) {
  if (contra::strcmp(result, expected) == 0) return true;
  std::fprintf(stderr, "%s\n", title);
  std::fprintf(stderr, "  result: ");
  while (*result) contra::encoding::put_u8(*result++, stderr);
  std::putc('\n', stderr);
  std::fprintf(stderr, "  expect: ");
  while (*expected) contra::encoding::put_u8(*expected++, stderr);
  std::putc('\n', stderr);
  std::fflush(stderr);
  return false;
}
bool check_data_content(board_t& board, const char32_t* expected) {
  std::u32string const result = get_data_content(board);
  return check_strings("check_data_content", result.c_str(), expected);
}
bool check_presentation_content(board_t& board, const char32_t* expected) {
  std::u32string const result = get_presentation_content(board);
  return check_strings("check_presentation_content", result.c_str(), expected);
}


void test_insert() {
  using namespace contra::ansi;
  {
    term_t term(5, 5);
    term.printt("hello world!\n");
    term.printt("hello world!\n");
    term.board().debug_print(stdout);
  }

  // 全角文字の挿入と上書き
  {
    term_t term(5, 3);
    term.printt("hello\r日\n");
    term.printt("a日本\r全\n");
    term.printt("a日本\ba");
    term.board().debug_print(stdout);
  }

  // タブ挿入
  {
    term_t term(40, 1);
    term.printt("日本\thello\tworld");
    term.board().debug_print(stdout);
  }
}

void test_strings() {
  using namespace contra::ansi;

  term_t term(40, 1);
  board_t& b = term.board();

  // to_presentation_position/to_data_position
  auto check_conversion = [&] (curpos_t xdata, curpos_t xpres) {
    mwg_check(b.to_data_position(0, xpres) == xdata);
    mwg_check(b.to_presentation_position(0, xdata) == xpres);
  };

  // calculate_data_ranges_from_presentation_range
  auto check_slice = [&] (curpos_t x1, curpos_t x2) {
    line_t::slice_ranges_t ranges1;
    std::vector<curpos_t> positions;
    for (curpos_t i = x1; i < x2; i++)
      positions.push_back(b.to_data_position(0, i));
    std::sort(positions.begin(), positions.end());
    for (curpos_t num : positions) {
      if (ranges1.size() && ranges1.back().second == num)
        ranges1.back().second = num + 1;
      else
        ranges1.push_back(std::make_pair(num, num + 1));
    }

    line_t::slice_ranges_t ranges2;
    b.line().calculate_data_ranges_from_presentation_range(ranges2, x1,  x2, b.width(), false);

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
      b.line().debug_dump();
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
  mwg_check(b.line().find_innermost_string(10, true , b.width(), false) == 0);
  mwg_check(b.line().find_innermost_string(10, false, b.width(), false) == 1);
  mwg_check(b.line().find_innermost_string(13, true,  b.width(), false) == 1);
  mwg_check(b.line().find_innermost_string(13, false, b.width(), false) == 0);
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
  mwg_check(b.line().find_innermost_string(2, true , b.width(), false) == 0);
  mwg_check(b.line().find_innermost_string(2, false, b.width(), false) == 1);
  mwg_check(b.line().find_innermost_string(5, true,  b.width(), false) == 1);
  mwg_check(b.line().find_innermost_string(5, false, b.width(), false) == 2);
  mwg_check(b.line().find_innermost_string(9, true,  b.width(), false) == 2);
  mwg_check(b.line().find_innermost_string(9, false, b.width(), false) == 0);

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
  mwg_check(b.line().find_innermost_string( 2, true , b.width(), false) == 0);
  mwg_check(b.line().find_innermost_string( 2, false, b.width(), false) == 1);
  mwg_check(b.line().find_innermost_string( 4, true,  b.width(), false) == 1);
  mwg_check(b.line().find_innermost_string( 4, false, b.width(), false) == 2);
  mwg_check(b.line().find_innermost_string( 6, true,  b.width(), false) == 2);
  mwg_check(b.line().find_innermost_string( 6, false, b.width(), false) == 3);
  mwg_check(b.line().find_innermost_string( 8, true,  b.width(), false) == 3);
  mwg_check(b.line().find_innermost_string( 8, false, b.width(), false) == 2);
  mwg_check(b.line().find_innermost_string(10, true,  b.width(), false) == 2);
  mwg_check(b.line().find_innermost_string(10, false, b.width(), false) == 4);
  mwg_check(b.line().find_innermost_string(12, true , b.width(), false) == 4);
  mwg_check(b.line().find_innermost_string(12, false, b.width(), false) == 2);
  mwg_check(b.line().find_innermost_string(14, true , b.width(), false) == 2);
  mwg_check(b.line().find_innermost_string(14, false, b.width(), false) == 1);
  mwg_check(b.line().find_innermost_string(16, true , b.width(), false) == 1);
  mwg_check(b.line().find_innermost_string(16, false, b.width(), false) == 0);

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

void test_presentation() {
  using namespace contra::ansi;
  term_t term(18, 1);
  board_t& b = term.board();

  term.printt("\x1b[H");
  term.printt("ab\x1b[2]cd\x1b[1]ef\x1b[2]gh\x1b[0]ij\x1b[2]kl\x1b[0]mn\x1b[0]op\x1b[0]qr\r");
  std::vector<char32_t> buff;
  for (curpos_t p = 0; p < b.width(); p++) {
    curpos_t const x = b.to_data_position(0, p);
    curpos_t const p2 = b.to_presentation_position(0, x);
    char32_t c = b.line().char_at(x).value;
    if (!(c & contra::charflag_wide_extension))
      buff.push_back(c == U'\0' ? U'@' : c);
    mwg_check(p == p2, "p=%d -> x=%d -> p=%d", p, x, p2);
  }
  std::u32string result(buff.begin(), buff.end());
  mwg_check(result == U"abpoefhgijlkmndcqr");


  bool const line_r2l = b.line_r2l();
  term.printt("\x1b[H\x1b[2Kab\x1b[2]cd\x1b[1]ef\x1b[2]gh\x1b[0]ij\x1b[2]kl\x1b[0]mn\x1b[0]op\x1b[0]qr");
  mwg_check(check_presentation_content(b, U"abpoefhgijlkmndcqr"));
  curpos_t data_positions[18][2] = {
    {0, 1}, {1, 2}, {16, 15}, {15, 14}, {4, 5}, {5, 6},
    {8, 7}, {7, 6}, {8, 9}, {9, 10}, {12, 11}, {11, 10},
    {12, 13}, {13, 14}, {4, 3}, {3, 2}, {16, 17}, {17,18},
  };
  for (std::size_t i = 0; i < std::size(data_positions); i++) {
    auto const ledge  = b.line().convert_position(false, i    , -1, b.width(), line_r2l);
    auto const redge  = b.line().convert_position(false, i + 1, +1, b.width(), line_r2l);
    auto const center = b.line().convert_position(false, i    ,  0, b.width(), line_r2l);
    mwg_check(ledge == data_positions[i][0], "to_data: i=%d result=%d expected=%d", i, ledge, data_positions[i][0]);
    mwg_check(redge == data_positions[i][1], "to_data: i=%d result=%d expected=%d", i, redge, data_positions[i][1]);
    mwg_check(center == std::min(data_positions[i][0], data_positions[i][1]),
      "i=%d result=%d expected=%d", i, redge, std::min(data_positions[i][0], data_positions[i][1]));

    curpos_t const p0 = data_positions[i][0];
    curpos_t const p1 = data_positions[i][1];
    auto const pres_edge0 = b.line().convert_position(true, p0, p0 < p1 ? -1 : +1, b.width(), line_r2l);
    auto const pres_edge1 = b.line().convert_position(true, p1, p0 < p1 ? +1 : -1, b.width(), line_r2l);
    mwg_check(pres_edge0 == (curpos_t) i    , "to_pres: in=%d out=%d expected=%d", p0, pres_edge0, i);
    mwg_check(pres_edge1 == (curpos_t) i + 1, "to_pres: in=%d out=%d expected=%d", p1, pres_edge1, i);
  }

}

void test_ech() {
  using namespace contra::ansi;
  term_t term(20, 1);
  board_t& b = term.board();

  // ECH ICH DCH (mono)
  term.printt("\x1b[Habcdefghijklmnopqr");
  term.printt("\x1b[1;3HC");
  mwg_check(check_data_content(b, U"abCdefghijklmnopqr@@"));
  term.printt("\x1b[H                    ");
  term.printt("\x1b[1;3H\x1b[3X");
  mwg_check(check_data_content(b, U"  @@@               "));
  term.printt("\x1b[H0123456789          ");
  term.printt("\x1b[1;3H\x1b[3P");
  mwg_check(check_data_content(b, U"0156789          @@@"));
  term.printt("\x1b[H0123456789          ");
  term.printt("\x1b[1;3H\x1b[3@");
  mwg_check(check_data_content(b, U"01@@@23456789       "));
  b.line().clear();

  // ECH ICH DCH (prop)
  term.printt("\r\x1b[2]abcdefghijklmnopqr\x1b[0]");
  mwg_check(check_data_content(b, U"abcdefghijklmnopqr@@"));
  mwg_check(check_presentation_content(b, U"rqponmlkjihgfedcba@@"));
  term.printt("\x1b[1;3f\x1b[3X");
  mwg_check(check_data_content(b, U"ab@@@fghijklmnopqr@@"));
  term.printt("\r\x1b[2]abcdefghijklmnopqr\x1b[0]  ");
  term.printt("\x1b[1;3f\x1b[3P");
  mwg_check(check_data_content(b, U"abfghijklmnopqr  @@@"));
  term.printt("\r\x1b[2]abcdefghijklmnopqr\x1b[0]  ");
  term.printt("\x1b[1;3f\x1b[3@");
  mwg_check(check_data_content(b, U"ab@@@cdefghijklmnopq"));
  b.line().clear();

  // ECH ICH DCH (prop SDS を踏み潰す場合)
  term.printt("\rab\x1b[2]cdef\x1b[1]ghijkl\x1b[0]mnop\x1b[0]qr  ");
  mwg_check(check_presentation_content(b, U"abponmghijklfedcqr  "));
  term.printt("\x1b[1;5f\x1b[4X");
  mwg_check(check_data_content(b, U"abcd@@@@ijklmnopqr  "));
  mwg_check(check_presentation_content(b, U"abdc@@@@ijklmnopqr  "));
  term.printt("\rab\x1b[2]cdef\x1b[1]ghijkl\x1b[0]mnop\x1b[0]qr  ");
  term.printt("\x1b[1;5f\x1b[4P");
  mwg_check(check_data_content(b, U"abcdijklmnopqr  @@@@"));
  mwg_check(check_presentation_content(b, U"ablkjidcmnopqr  @@@@"));
  term.printt("\rab\x1b[2]cdef\x1b[1]ghijkl\x1b[0]mnop\x1b[0]qr  ");
  term.printt("\x1b[1;5f\x1b[4@");
  mwg_check(check_data_content(b, U"abcd@@@@efghijklmnop"));
  mwg_check(check_presentation_content(b, U"abdc@@@@efghijklmnop"));
  b.line().clear();

  // ECH ICH DCH (prop !dcsm)
  term.printt("\x1b[9l"); // DCSM(PRESENTATION)
  term.printt("\rab\x1b[2]cdef\x1b[1]ghijkl\x1b[]mnop\x1b[]qr  ");
  mwg_check(check_presentation_content(b, U"abponmghijklfedcqr  "));
  term.printt("\x1b[1;5H\x1b[4X");
  mwg_check(check_presentation_content(b, U"abpo@@@@ijklfedcqr  "));
  term.printt("\rab\x1b[2]cdef\x1b[1]ghijkl\x1b[0]mnop\x1b[0]qr  ");
  term.printt("\x1b[1;5H\x1b[4P");
  mwg_check(check_presentation_content(b, U"abpoijklfedcqr  @@@@"));
  term.printt("\rab\x1b[2]cdef\x1b[1]ghijkl\x1b[0]mnop\x1b[0]qr  ");
  term.printt("\x1b[1;5H\x1b[4@");
  mwg_check(check_presentation_content(b, U"abpo@@@@nmghijklfedc"));
  term.printt("\x1b[9h"); // DCSM(DATA);
  b.line().clear();

  // ECH ICH DCH (prop !dcsm)
  b.reset_size(40, 1);
  term.printt("\x1b[9l"); // DCSM(PRESENTATION)
  term.printt("\r\x1b[2Kab\x1b[2]cd\x1b[1]ef\x1b[2]gh\x1b[0]ij\x1b[2]kl\x1b[0]mn\x1b[0]op\x1b[0]qr");
  mwg_check(check_presentation_content(b, U"abpoefhgijlkmndcqr@@@@@@@@@@@@@@@@@@@@@@"));
  // PRES a      [[[g]ij[lk]mn]dc]qr
  // DATA a      [cd[[g]ij[kl]mn]]qr
  term.printt("\x1b[1;2H\x1b[6X");
  mwg_check(check_presentation_content(b, U"a@@@@@@gijlkmndcqr@@@@@@@@@@@@@@@@@@@@@@"));
  // PRES ab[po[e]]        [[n]dc]qr
  // DATA ab[[e]op]        [cd[n]]qr
  term.printt("\r\x1b[2Kab\x1b[2]cd\x1b[1]ef\x1b[2]gh\x1b[0]ij\x1b[2]kl\x1b[0]mn\x1b[0]op\x1b[0]qr");
  term.printt("\x1b[1;6H\x1b[8X");
  mwg_check(check_presentation_content(b, U"abpoe@@@@@@@@ndcqr@@@@@@@@@@@@@@@@@@@@@@"));
  term.printt("\r\x1b[2Kab\x1b[2]cd\x1b[1]ef\x1b[2]gh\x1b[0]ij\x1b[2]kl\x1b[0]mn\x1b[0]op\x1b[0]qr");
  term.printt("\x1b[1;14H\x1b[7X");
  mwg_check(check_presentation_content(b, U"abpoefhgijlkm@@@@@@@@@@@@@@@@@@@@@@@@@@@"));
  term.printt("\r\x1b[2Kab\x1b[2]cd\x1b[1]ef\x1b[2]gh\x1b[0]ij\x1b[2]kl\x1b[0]mn\x1b[0]op\x1b[0]qr");
  term.printt("\x1b[1;2H\x1b[6P");
  mwg_check(check_presentation_content(b, U"agijlkmndcqr@@@@@@@@@@@@@@@@@@@@@@@@@@@@"));
  term.printt("\r\x1b[2Kab\x1b[2]cd\x1b[1]ef\x1b[2]gh\x1b[0]ij\x1b[2]kl\x1b[0]mn\x1b[0]op\x1b[0]qr");
  term.printt("\x1b[1;6H\x1b[8P");
  mwg_check(check_presentation_content(b, U"abpoendcqr@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"));
  term.printt("\r\x1b[2Kab\x1b[2]cd\x1b[1]ef\x1b[2]gh\x1b[0]ij\x1b[2]kl\x1b[0]mn\x1b[0]op\x1b[0]qr");
  term.printt("\x1b[1;14H\x1b[7P");
  mwg_check(check_presentation_content(b, U"abpoefhgijlkm@@@@@@@@@@@@@@@@@@@@@@@@@@@"));
  term.printt("\r\x1b[2Kab\x1b[2]cd\x1b[1]ef\x1b[2]gh\x1b[0]ij\x1b[2]kl\x1b[0]mn\x1b[0]op\x1b[0]qr");
  term.printt("\x1b[1;2H\x1b[6@");
  mwg_check(check_presentation_content(b, U"a@@@@@@bpoefhgijlkmndcqr@@@@@@@@@@@@@@@@"));
  term.printt("\r\x1b[2Kab\x1b[2]cd\x1b[1]ef\x1b[2]gh\x1b[0]ij\x1b[2]kl\x1b[0]mn\x1b[0]op\x1b[0]qr");
  term.printt("\x1b[1;6H\x1b[8@");
  mwg_check(check_presentation_content(b, U"abpoe@@@@@@@@fhgijlkmndcqr@@@@@@@@@@@@@@"));
  term.printt("\r\x1b[2Kab\x1b[2]cd\x1b[1]ef\x1b[2]gh\x1b[0]ij\x1b[2]kl\x1b[0]mn\x1b[0]op\x1b[0]qr");
  term.printt("\x1b[1;14H\x1b[7@");
  mwg_check(check_presentation_content(b, U"abpoefhgijlkm@@@@@@@ndcqr@@@@@@@@@@@@@@@"));
  term.printt("\x1b[9h"); // DCSM(DATA);
  b.line().clear();

  // ECH (prop !dcsm)
  term.printt("\x1b[9l"); // DCSM(PRESENTATION)
  term.printt("\r\x1b[2Kab\x1b[1]cdefgh\x1b[0]ij\r");
  mwg_check(check_presentation_content(b, U"abcdefghij@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"));
  term.printt("\x1b[1;5H\x1b[2X");
  mwg_check(check_presentation_content(b, U"abcd@@ghij@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"));
  term.printt("\r\x1b[2Kab\x1b[1]cd\x1b[1]efgh\x1b[0]ij\x1b[0]kl\r");
  mwg_check(check_presentation_content(b, U"abcdefghijkl@@@@@@@@@@@@@@@@@@@@@@@@@@@@"));
  term.printt("\x1b[1;6H\x1b[2X");
  mwg_check(check_presentation_content(b, U"abcde@@hijkl@@@@@@@@@@@@@@@@@@@@@@@@@@@@"));
  term.printt("\x1b[9h"); // DCSM(DATA);
  b.line().clear();

  // ECH ICH DCH (prop !dcsm)
  term.printt("\x1b[9l\x1b[3 S"); // DCSM(PRESENTATION), SPD(3)
  term.printt("\r\x1b[2Kab\x1b[2]cd\x1b[1]ef\x1b[2]gh\x1b[0]ij\x1b[2]kl\x1b[0]mn\x1b[0]op\x1b[0]qr");
  mwg_check(check_presentation_content(b, U"@@@@@@@@@@@@@@@@@@@@@@rqpoefhgijlkmndcba"));
  term.printt("\r\x1b[2Kab\x1b[2]cd\x1b[1]ef\x1b[2]gh\x1b[0]ij\x1b[2]kl\x1b[0]mn\x1b[0]op\x1b[0]qr");
  term.printt("\x1b[1;2H\x1b[6X");
  mwg_check(check_presentation_content(b, U"@@@@@@@@@@@@@@@@@@@@@@rqpoefhgijl@@@@@@a"));
  term.printt("\r\x1b[2Kab\x1b[2]cd\x1b[1]ef\x1b[2]gh\x1b[0]ij\x1b[2]kl\x1b[0]mn\x1b[0]op\x1b[0]qr");
  term.printt("\x1b[1;2H\x1b[6P");
  mwg_check(check_presentation_content(b, U"@@@@@@@@@@@@@@@@@@@@@@@@@@@@rqpoefhgijla"));
  term.printt("\r\x1b[2Kab\x1b[2]cd\x1b[1]ef\x1b[2]gh\x1b[0]ij\x1b[2]kl\x1b[0]mn\x1b[0]op\x1b[0]qr");
  term.printt("\x1b[1;2H\x1b[6@");
  mwg_check(check_presentation_content(b, U"@@@@@@@@@@@@@@@@rqpoefhgijlkmndcb@@@@@@a"));
  term.printt("\x1b[9h\x1b[ S"); // DCSM(DATA), SPD(0)
  b.line().clear();
}

void test_erm() {
  using namespace contra::ansi;
  term_t term(20, 1);
  board_t& b = term.board();

  // 全角
  term.printt("\x1b[H日本語日本語日本語");
  term.printt("\x1b[1;4H\x1b[12X");
  mwg_check(check_presentation_content(b, U"日 @@@@@@@@@@@@ 語@@"));

  // SPA-EPA (mono)
  term.printt("\x1b[Hab\eVcdef\eWghij\eVklmn\eWopqr");
  mwg_check(check_presentation_content(b, U"abcdefghijklmnopqr@@"));
  term.printt("\x1b[1;2H\x1b[14X");
  mwg_check(check_presentation_content(b, U"a@cdef@@@@klmn@pqr@@"));
  term.printt("\x1b[H日本\eV語日\eW本\eV語日\eW本語");
  mwg_check(check_presentation_content(b, U"日本語日本語日本語@@"));
  term.printt("\x1b[1;4H\x1b[12X");
  mwg_check(check_presentation_content(b, U"日 @語日@@語日@ 語@@"));
  term.printt("\x1b[H日本\eV語日\eW本\eV語日\eW本語");
  term.printt("\x1b[1;6H\x1b[8X");
  mwg_check(check_presentation_content(b, U"日本語日@@語日本語@@"));
  b.line().clear();

  // SPA-EPA (prop)
  term.printt("\x1b[Hab\eVcd\x1b[2]ef\eWgh\eVij\eWklmn\x1b[0]opqr");
  mwg_check(check_presentation_content(b, U"abcdnmlkjihgfeopqr@@"));
  term.printt("\x1b[H\x1b[16X");
  mwg_check(check_presentation_content(b, U"@@cdfe@@ij@@@@@@qr@@"));
  b.line().clear();

  // SPA-EPA (prop&presentation)
  term.printt("\x1b[9l"); // DCSM(PRESENTATION)
  term.printt("\x1b[Hab\eVcd\x1b[2]ef\eWgh\eVij\eWklmn\x1b[0]opqr");
  mwg_check(check_presentation_content(b, U"abcdnmlkjihgfeopqr@@"));
  term.printt("\x1b[H\x1b[16X");
  mwg_check(check_presentation_content(b, U"@@cdfe@@ij@@@@@@qr@@"));
  term.printt("\x1b[Hab\eVcd\x1b[2]ef\eWgh\eVij\eWklmn\x1b[0]opqr");
  term.printt("\x1b[1;10H\x1b[7X");
  mwg_check(check_presentation_content(b, U"abcdnmlkjef@@i@@qr@@"));
  term.printt("\x1b[H日本語\x1b[2]日本語\x1b[]日本語");
  mwg_check(check_presentation_content(b, U"日本語語本日日本語@@"));
  term.printt("\x1b[1;6H\x1b[6X");
  mwg_check(check_presentation_content(b, U"日本 @@@@@@ 日本語@@"));
  term.printt("\x1b[H日本\eV語\x1b[2]日本\eW語\x1b[]日本語");
  mwg_check(check_presentation_content(b, U"日本語語本日日本語@@"));
  term.printt("\x1b[1;6H\x1b[6X");
  mwg_check(check_presentation_content(b, U"日本語本日@@日本語@@"));
  term.printt("\x1b[H日本\eV語\x1b[2]日本\eW語\x1b[]日本語");
  term.printt("\x1b[1;7H\x1b[6X");
  mwg_check(check_presentation_content(b, U"日本語日本@@日本語@@"));
  term.printt("\x1b[9h"); // DCSM(DATA);
  b.line().clear();
}

void test_sgr() {
  using namespace contra::ansi;
  term_t term(20, 10);

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

  contra::dict::termcap_sgr_type sgrcap;
  sgrcap.initialize();

  contra::ansi::attribute_table atable;
  contra::dict::tty_writer w(atable, stdout, &sgrcap);
  w.print_screen(term.board());
}

int main() {
  try {
    test_insert();
    test_strings();
    test_presentation();
    test_ech();
    test_erm();
    test_sgr();
  } catch(mwg::assertion_error& e) {}
  return 0;
}
