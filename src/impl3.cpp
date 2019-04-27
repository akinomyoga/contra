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
#include "sequence.h"
#include "ansi/line.hpp"
#include "ansi/term.hpp"
#include "ansi/observer.tty.hpp"
#include "ansi/enc.c2w.hpp"

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

  bool const oldNonblock = contra::set_fd_nonblock(STDIN_FILENO, true);

  contra::multicast_device dev;

  contra::ansi::curpos_t cfg_col = 80, cfg_row = 24;

  struct winsize winsize;
  ioctl(STDIN_FILENO, TIOCGWINSZ, (char *) &winsize);
  if (winsize.ws_col > cfg_col) winsize.ws_col = cfg_col;
  if (winsize.ws_row > cfg_row) winsize.ws_row = cfg_row;

  contra::session sess;
  if (!create_session(&sess, oldTermios, "/bin/bash", winsize.ws_col, winsize.ws_row)) return -1;

  contra::ansi::board_t b(winsize.ws_col, winsize.ws_row);
  contra::ansi::term_t term(b);
  contra::tty_player_device d1(&term);
  dev.push(&d1);

  term.state().m_default_fg_space = contra::ansi::attribute_t::color_space_indexed;
  term.state().m_default_fg = 0;
  term.state().m_default_bg_space = contra::ansi::attribute_t::color_space_indexed;
  term.state().m_default_bg = 15;

  contra::ansi::termcap_sgr_type sgrcap;
  sgrcap.initialize();
  contra::ansi::tty_observer renderer(term, stdout, &sgrcap);

  // contra::sequence_printer printer("impl3-allseq.txt");
  // contra::sequence_printer_device d2(&printer);
  // dev.push(&d2);

  contra::fd_device devIn(sess.masterfd);
  term.set_response_target(devIn);

  char buff[4096];
  for (;;) {
    bool processed = false;
    clock_t time0 = clock();
    while (contra::read_from_fd(sess.masterfd, &dev, buff, sizeof(buff))) {
      processed = true;
      clock_t const time1 = clock();
      clock_t const msec = (time1 - time0) * 1000 / CLOCKS_PER_SEC;
      if (msec > 20) break;
    }
    if (processed)
      renderer.update();

    if (contra::read_from_fd(STDIN_FILENO, &devIn, buff, sizeof(buff))) continue;
    if (contra::is_child_terminated(sess.pid)) break;
    if (!processed)
      contra::msleep(10);
  }

  kill(sess.pid, SIGTERM);
  contra::set_fd_nonblock(STDIN_FILENO, oldNonblock);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &oldTermios);

  renderer.print_screen(b);

  std::FILE* file = std::fopen("impl2-dump.txt", "w");
  b.debug_print(file);
  std::fclose(file);

  return 0;
}
