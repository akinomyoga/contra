#include <cstdio>
#include <cstdlib>
#include <sys/ioctl.h>

#include "dict.hpp"
#include "manager.hpp"
#include "sequence.hpp"
#include "ttty/buffer.hpp"
#include "ttty/screen.hpp"
#include "enc.c2w.hpp"

int main() {
  contra::ansi::curpos_t cfg_col = 80, cfg_row = 24;
  contra::ttty::ttty_screen screen(STDIN_FILENO, STDOUT_FILENO);
  contra::term::terminal_session_parameters params;
  {
    struct winsize ws;
    ::ioctl(STDIN_FILENO, TIOCGWINSZ, (char *) &ws);
    if (ws.ws_col > cfg_col) ws.ws_col = cfg_col;
    if (ws.ws_row > cfg_row) ws.ws_row = cfg_row;
    params.col = std::min<contra::ansi::curpos_t>(ws.ws_col, cfg_col);
    params.row = std::min<contra::ansi::curpos_t>(ws.ws_row, cfg_row);
    params.termios = &screen.old_termios;

    params.dbg_sequence_logfile = "ttty-allseq.txt";
  }
  if (!screen.initialize(params)) return 10;

  auto& app = screen.manager().app();
  app.state().m_default_fg_space = contra::ansi::attribute_t::color_space_indexed;
  app.state().m_default_fg_color = 0;
  app.state().m_default_bg_space = contra::ansi::attribute_t::color_space_indexed;
  app.state().m_default_bg_color = 255;

  screen.do_loop();
  screen.finalize();

  std::printf("\n");
  screen.renderer->writer().print_screen(app.board());

  std::FILE* file = std::fopen("impl3-dump.txt", "w");
  app.board().debug_print(file);
  std::fclose(file);

  return 0;
}
