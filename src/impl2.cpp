#include <cstdio>
#include <cstdlib>
#include <sys/ioctl.h>

#include "pty.hpp"
#include "manager.hpp"
#include "sequence.hpp"
#include "dict.hpp"
#include "ttty/buffer.hpp"
#include "ttty/screen.hpp"

int main() {
  contra::ttty::ttty_screen screen(STDIN_FILENO, STDOUT_FILENO);
  contra::term::terminal_session_parameters params;
  {
    struct winsize ws;
    ioctl(STDIN_FILENO, TIOCGWINSZ, (char *) &ws);
    params.col = ws.ws_col;
    params.row = ws.ws_row;
    params.xpixel = ws.ws_xpixel;
    params.ypixel = ws.ws_ypixel;
    params.termios = &screen.old_termios;

    params.dbg_fd_tee = STDOUT_FILENO;
    params.dbg_sequence_logfile = "impl2-allseq.txt";
  }
  if (!screen.initialize(params)) return 10;

  screen.do_loop(false);
  screen.finalize();

  return 0;
}
