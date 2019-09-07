#include <cstdio>
#include <cstdlib>
#include <sys/ioctl.h>

#include "dict.hpp"
#include "manager.hpp"
#include "sequence.hpp"
#include "ttty/buffer.hpp"
#include "ttty/screen.hpp"
#include "enc.c2w.hpp"

namespace contra::ttty {
  bool run(contra::app::context& actx) {
    contra_unused(actx);
    contra::ttty::ttty_screen screen(STDIN_FILENO, STDOUT_FILENO);
    contra::term::terminal_session_parameters params;
    {
      struct winsize ws;
      ::ioctl(STDIN_FILENO, TIOCGWINSZ, (char *) &ws);
      params.col = ws.ws_col;
      params.row = ws.ws_row;
      params.xpixel = ws.ws_xpixel;
      params.ypixel = ws.ws_ypixel;
      params.termios = &screen.old_termios;

      // params.dbg_fd_tee = STDOUT_FILENO;
      // params.dbg_sequence_logfile = "ttty-allseq.txt";
    }
    if (!screen.initialize(params)) {
      std::fprintf(stderr, "contra: failed to create the session");
      return false;
    }

    // auto& app = screen.manager().app();
    // app.state().m_default_fg_space = contra::ansi::attribute_t::color_space_indexed;
    // app.state().m_default_fg_color = 0;
    // app.state().m_default_bg_space = contra::ansi::attribute_t::color_space_indexed;
    // app.state().m_default_bg_color = 255;

    screen.do_loop();
    screen.finalize();
    return true;
  }
}

#ifdef ttty_main
int main() {
  contra::app::context actx;
  std::string config_dir = contra::term::get_config_directory();
  actx.load((config_dir + "/contra/twin.conf").c_str());
  if (!contra::ttty::run()) return 1;
  return 0;
}
#endif
