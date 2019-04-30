#include <cstdio>
#include <cstdlib>
#include <sys/ioctl.h>

#include "dict.hpp"
#include "session.hpp"
#include "sequence.hpp"
#include "ttty/buffer.hpp"
#include "ttty/screen.hpp"
#include "enc.c2w.hpp"

int main() {
  contra::ansi::curpos_t cfg_col = 80, cfg_row = 24;
  contra::ttty::ttty_screen screen(STDIN_FILENO, STDOUT_FILENO);
  auto& sess = screen.session();
  {
    ioctl(STDIN_FILENO, TIOCGWINSZ, (char *) &sess.init_ws);
    if (sess.init_ws.ws_col > cfg_col) sess.init_ws.ws_col = cfg_col;
    if (sess.init_ws.ws_row > cfg_row) sess.init_ws.ws_row = cfg_row;
    sess.init_termios = &screen.old_termios;
  }
  if (!screen.initialize()) return 10;

  sess.term().state().m_default_fg_space = contra::ansi::attribute_t::color_space_indexed;
  sess.term().state().m_default_fg = 0;
  sess.term().state().m_default_bg_space = contra::ansi::attribute_t::color_space_indexed;
  sess.term().state().m_default_bg = 255;

  contra::sequence_printer printer("impl3-allseq.txt");
  sess.output_device().push(&printer);

  screen.do_loop();
  screen.finalize();

  std::printf("\n");
  screen.renderer->writer().print_screen(sess.board());

  std::FILE* file = std::fopen("impl3-dump.txt", "w");
  sess.board().debug_print(file);
  std::fclose(file);

  return 0;
}
