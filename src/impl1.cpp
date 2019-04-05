#include "board.h"
#include "tty_observer.h"
#include "tty_player.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <vector>

void check_data_presentation_conversion(bool useseq, bool verbose = false) {
  using namespace contra;

  contra::board b;
  b.resize(40, 1);
  contra::board_line* line = b.line(0);

  contra::tty_player term(b);

  auto tester = [&](const char* s, const int* expected){
    std::size_t N = std::strlen(s);
    for (std::size_t i = 0; i < N; i++) {
      curpos_t const px = b.to_presentation_position(0, i);
      curpos_t const dx = b.to_data_position(0, px);
      mwg_check(px == expected[i], "i=%zd px=%d (should be %d)", i, (int) px, expected[i]);
      mwg_check(dx == (int) i, "i=%zd dx=%d", i, (int) dx);
      // if (verbose) std::printf("%d ", (int) px);
    }

    if (verbose) {
      for (std::size_t i = 0; i < N; i++) {
        curpos_t const dx = b.to_data_position(0, i);
        std::fputc(s[dx], stderr);
      }
      std::fputc('\n', stderr);
    }
  };

  //
  // Test Directed Strings
  //

  // abcd_[efgh]_ij
  int const px_result1[] = {0, 1, 2, 3, 4, 8, 7, 6, 5, 9, 10, 11};
  line->clear_markers();
  if (useseq)
    term.printt("\x1b[Habcd_\x1b[2]efgh\x1b[0]_ij");
  else
    line->_set_string(5, 9, string_directed_rtol);
  tester("abcd_efgh_ij", px_result1);

  line->clear_markers();
  if (useseq)
    term.printt("\x1b[Habcd_\x1b[1[efgh\x1b[0[_ij");
  else
    line->_set_string(5, 9, string_reversed);
  tester("abcd_efgh_ij", px_result1);

  // kl_[mnop_[qrst]_uvw]_xyz
  int const px_result2[] = {0, 1, 2, 15, 14, 13, 12, 11, 7, 8, 9, 10, 6, 5, 4, 3, 16, 17, 18, 19};
  line->clear_markers();
  if (useseq)
    term.printt("\x1b[Hkl_\x1b[2]mnop_\x1b[1]qrst\x1b[]_uvw\x1b[]_xyz");
  else {
    line->_set_string(3, 16, string_directed_rtol);
    line->_set_string(8, 12, string_directed_ltor);
  }
  tester("kl_mnop_qrst_uvw_xyz", px_result2);

  line->clear_markers();
  if (useseq)
    term.printt("\x1b[Hkl_\x1b[1[mnop_\x1b[1[qrst\x1b[[_uvw\x1b[[_xyz");
  else {
    line->_set_string(3, 16, string_reversed);
    line->_set_string(8, 12, string_reversed);
  }
  tester("kl_mnop_qrst_uvw_xyz", px_result2);

  line->clear_markers();
  if (useseq)
    term.printt("\x1b[Hk\x1b[1]l_\x1b[1[mn\x1b[2]op_\x1b[1[q\x1b[1]rs\x1b[]t\x1b[[_u\x1b[]vw\x1b[[_\x1b[]xyz");
  else {
    line->_set_string(1, 17, string_directed_charpath);
    line->_set_string(3, 16, string_reversed     );
    line->_set_string(5, 14, string_directed_rtol);
    line->_set_string(8, 12, string_reversed     );
    line->_set_string(9, 11, string_directed_ltor);
  }
  tester("kl_mnop_qrst_uvw_xyz", px_result2);
}

using namespace contra;
int main() {
  check_data_presentation_conversion(true);

  contra::board b;
  b.resize(20, 10);

  contra::tty_player term(b);
  term.printt("\x1b[38:5:196;4mabcdefghijklmnopqrstuvwxyz");
  term.printt("\x1b[38:5:202mabcdefghijklmnopqrstuvwxyz");
  term.printt("\x1b[38:5:220;24mabcdefghijklmnopqrstuvwxyz");
  term.printt("\x1b[38:5:154mabcdefghijklmnopqrstuvwxyz");
  term.printt("\x1b[38:5:63mabcdefghijklmnopqrstuvwxyz\n");
  term.reset_fg();
  term.printt("Hello, world!\n");
  term.printt("Thank you!\n");

  term.printt("\nA\x1b[AB\x1b[BC\x1b[2DD\x1b[0CE");
  term.printt("\x1b[HA\x1b[2;2HB");

  contra::termcap_sgr_type sgrcap;
  sgrcap.initialize();

  contra::tty_observer target(stdout, &sgrcap);
  target.print_screen(b);

  return 0;
}
