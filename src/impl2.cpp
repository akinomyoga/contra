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

  auto& app = screen.manager().app();

  screen.do_loop(false);
  screen.finalize();

  contra::dict::termcap_sgr_type sgrcap;
  sgrcap.initialize();
  contra::ttty::tty_observer target(app.term(), stdout, &sgrcap);
  target.writer().print_screen(app.board());

  std::FILE* file = std::fopen("impl2-dump.txt", "w");
  app.board().debug_print(file);
  std::fclose(file);

  return 0;
}
