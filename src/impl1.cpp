#include "board.h"
#include "tty_observer.h"
#include "tty_player.h"
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <vector>

using namespace contra;

int main() {
  contra::board b;
  b.resize(20, 10);

  contra::tty_player term(b);

  term.printt("\x1b[38:5:196;4mabcdefghijklmnopqrstuvwxyz");
  term.printt("\x1b[38:5:202;24mabcdefghijklmnopqrstuvwxyz");
  term.printt("\x1b[38:5:220mabcdefghijklmnopqrstuvwxyz");
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
