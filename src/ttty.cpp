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
    struct winsize ws = sess.winsize();
    ioctl(STDIN_FILENO, TIOCGWINSZ, (char *) &ws);
    if (ws.ws_col > cfg_col) ws.ws_col = cfg_col;
    if (ws.ws_row > cfg_row) ws.ws_row = cfg_row;
    sess.init_size(ws.ws_col, ws.ws_row);
    sess.init_termios = &screen.old_termios;
  }
  if (!screen.initialize()) return 10;

  sess.state().m_default_fg_space = contra::ansi::attribute_t::color_space_indexed;
  sess.state().m_default_fg_color = 0;
  sess.state().m_default_bg_space = contra::ansi::attribute_t::color_space_indexed;
  sess.state().m_default_bg_color = 255;

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
