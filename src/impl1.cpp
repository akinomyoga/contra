#include "board.h"
#include "tty_observer.h"
#include "tty_player.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <vector>

void check_data_presentation_conversion(bool verbose = false) {
  using namespace contra;

  board_line line;

  auto tester = [&](const char* s, const int* expected){
    std::size_t N = std::strlen(s);
    for (std::size_t i = 0; i < N; i++) {
      curpos_t const px = line.to_presentation_position(i, false);
      curpos_t const dx = line.to_data_position(px, false);
      mwg_check(px == expected[i], "i=%zd px=%d (should be %d)", i, (int) px, expected[i]);
      mwg_check(dx == (int) i, "i=%zd dx=%d", i, (int) dx);
      // if (verbose) std::printf("%d ", (int) px);
    }

    if (verbose) {
      for (std::size_t i = 0; i < N; i++) {
        curpos_t const dx = line.to_data_position(i, false);
        std::fputc(s[dx], stderr);
      }
      std::fputc('\n', stderr);
    }
  };

  // abcd_[efgh]_ij
  int const px_result1[] = {0, 1, 2, 3, 4, 8, 7, 6, 5, 9, 10, 11};
  line.strings.clear();
  line.strings.push_back(directed_string {5, 9, string_direction_rtol});
  tester("abcd_efgh_ij", px_result1);
  line.strings.clear();
  line.strings.push_back(directed_string {5, 9, string_direction_reversed});
  tester("abcd_efgh_ij", px_result1);

  // kl_[mnop_[qrst]_uvw]_xyz
  int const px_result2[] = {0, 1, 2, 15, 14, 13, 12, 11, 7, 8, 9, 10, 6, 5, 4, 3, 16, 17, 18, 19};
  line.strings.clear();
  line.strings.push_back({3, 16, string_direction_rtol});
  line.strings.push_back({8, 12, string_direction_ltor});
  tester("kl_mnop_qrst_uvw_xyz", px_result2);
  line.strings.clear();
  line.strings.push_back({3, 16, string_direction_reversed});
  line.strings.push_back({8, 12, string_direction_reversed});
  tester("kl_mnop_qrst_uvw_xyz", px_result2);
  line.strings.clear();
  line.strings.push_back({1, 17, string_direction_default});
  line.strings.push_back({3, 16, string_direction_reversed});
  line.strings.push_back({5, 14, string_direction_rtol});
  line.strings.push_back({8, 12, string_direction_reversed});
  line.strings.push_back({9, 11, string_direction_ltor});
  tester("kl_mnop_qrst_uvw_xyz", px_result2);
}

using namespace contra;
int main() {
  check_data_presentation_conversion();

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

  contra::termcap_sgr_type sgrcap;
  sgrcap.initialize();

  contra::tty_observer target(stdout, &sgrcap);
  target.print_screen(b);

  return 0;
}
