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

  term.set_fg(196);
  for (int i = 0; i < 26; i++)
    term.insert_char('a' + i);
  term.set_fg(202);
  for (int i = 0; i < 26; i++)
    term.insert_char('a' + i);
  term.set_fg(220);
  for (int i = 0; i < 26; i++)
    term.insert_char('a' + i);
  term.set_fg(154);
  for (int i = 0; i < 26; i++)
    term.insert_char('a' + i);
  term.set_fg(63);
  for (int i = 0; i < 26; i++)
    term.insert_char('a' + i);
  term.printt("\n");
  term.reset_fg();
  term.printt("Hello, world!\n");
  term.printt("Thank you!\n");

  contra::termcap_sgr_type sgrcap;
  sgrcap.initialize();

  contra::tty_observer target(stdout, &sgrcap);
  target.print_screen(b);

  return 0;
}
