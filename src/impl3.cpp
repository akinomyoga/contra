#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

#include <stdio.h>
#include <time.h>
#include <cstdlib>
#include <vector>

#include "impl.hpp"
#include "sequence.hpp"
#include "dict.hpp"
#include "ansi/line.hpp"
#include "ansi/term.hpp"
#include "ttty/buffer.hpp"
#include "enc.c2w.hpp"

int main() {
  struct termios oldTermios;
  tcgetattr(STDIN_FILENO, &oldTermios);
  struct termios termios = oldTermios;
  termios.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  termios.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  termios.c_cflag &= ~(CSIZE | PARENB);
  termios.c_cflag |= CS8;
  termios.c_oflag &= ~(OPOST);
  termios.c_cc[VMIN]  = 1;
  termios.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &termios);
  bool const oldNonblock = contra::term::set_fd_nonblock(STDIN_FILENO, true);

  contra::ansi::curpos_t cfg_col = 80, cfg_row = 24;
  struct winsize winsize;
  ioctl(STDIN_FILENO, TIOCGWINSZ, (char *) &winsize);
  if (winsize.ws_col > cfg_col) winsize.ws_col = cfg_col;
  if (winsize.ws_row > cfg_row) winsize.ws_row = cfg_row;

  contra::term::session sess;
  if (!contra::term::create_session(&sess, "/bin/bash", &winsize, &oldTermios)) return -1;

  contra::multicast_device dev;

  contra::ansi::board_t b(winsize.ws_col, winsize.ws_row);
  contra::ansi::term_t term(b);
  dev.push(&term);

  term.state().m_default_fg_space = contra::ansi::attribute_t::color_space_indexed;
  term.state().m_default_fg = 0;
  term.state().m_default_bg_space = contra::ansi::attribute_t::color_space_indexed;
  term.state().m_default_bg = 255;

  contra::dict::termcap_sgr_type sgrcap;
  sgrcap.initialize();
  contra::ttty::tty_observer renderer(term, stdout, &sgrcap);

  contra::sequence_printer printer("impl3-allseq.txt");
  dev.push(&printer);

  contra::term::fd_device devIn(sess.masterfd);
  term.set_response_target(devIn);

  char buff[4096];
  for (;;) {
    bool processed = false;
    clock_t time0 = clock();
    while (contra::term::read_from_fd(sess.masterfd, &dev, buff, sizeof(buff))) {
      processed = true;
      clock_t const time1 = clock();
      clock_t const msec = (time1 - time0) * 1000 / CLOCKS_PER_SEC;
      if (msec > 20) break;
    }
    if (processed)
      renderer.update();

    if (contra::term::read_from_fd(STDIN_FILENO, &devIn, buff, sizeof(buff))) continue;
    if (contra::term::is_child_terminated(sess.pid)) break;
    if (!processed)
      contra::term::msleep(10);
  }

  kill(sess.pid, SIGTERM);
  contra::term::set_fd_nonblock(STDIN_FILENO, oldNonblock);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &oldTermios);

  std::printf("\n");
  renderer.writer().print_screen(b);

  std::FILE* file = std::fopen("impl2-dump.txt", "w");
  b.debug_print(file);
  std::fclose(file);

  return 0;
}
