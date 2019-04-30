#include <cstdio>
#include <cstdlib>
#include <sys/ioctl.h>

#include "pty.hpp"
#include "session.hpp"
#include "sequence.hpp"
#include "dict.hpp"
#include "ttty/buffer.hpp"
#include "ttty/screen.hpp"

int main() {
  contra::ttty::ttty_screen screen(STDIN_FILENO, STDOUT_FILENO);
  auto& sess = screen.session();
  {
    ioctl(STDIN_FILENO, TIOCGWINSZ, (char *) &sess.init_ws);
    sess.init_termios = &screen.old_termios;
  }
  if (!screen.initialize()) return 10;

  contra::term::fd_device d0(STDOUT_FILENO);
  sess.output_device().push(&d0);

  contra::sequence_printer printer("impl2-allseq.txt");
  sess.output_device().push(&printer);

  screen.do_loop(false);
  screen.finalize();

  contra::dict::termcap_sgr_type sgrcap;
  sgrcap.initialize();
  contra::ttty::tty_observer target(sess.term(), stdout, &sgrcap);
  target.writer().print_screen(sess.board());

  std::FILE* file = std::fopen("impl2-dump.txt", "w");
  sess.board().debug_print(file);
  std::fclose(file);

  return 0;
}
