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

int main() {
  struct termios oldTermios;
  tcgetattr(STDIN_FILENO, &oldTermios);
  contra::session sess;
  if (!create_session(&sess, oldTermios, "/bin/bash")) return -1;

  struct termios termios = oldTermios;
  termios.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  termios.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  termios.c_cflag &= ~(CSIZE | PARENB);
  termios.c_cflag |= CS8;
  termios.c_oflag &= ~(OPOST);
  termios.c_cc[VMIN]  = 1;
  termios.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &termios);

  contra::multicast_device dev;
  contra::fd_device d0(STDOUT_FILENO);
  dev.push(&d0);

  struct winsize winsize;
  ioctl(STDIN_FILENO, TIOCGWINSZ, (char *) &winsize);
  contra::ansi::board_t b(winsize.ws_col, winsize.ws_row);

  contra::ansi::term_t term(b);
  contra::tty_player_device d1(&term);
  dev.push(&d1);

  contra::sequence_printer printer("impl2-allseq.txt");
  contra::sequence_printer_device d2(&printer);
  dev.push(&d2);

  bool const oldNonblock = contra::set_fd_nonblock(STDIN_FILENO, true);
  contra::fd_device devIn(sess.masterfd);

  char buff[4096];
  for (;;) {
    if (contra::read_from_fd(sess.masterfd, &dev, buff, sizeof(buff))) continue;
    if (contra::read_from_fd(STDIN_FILENO, &devIn, buff, sizeof(buff))) continue;
    if (contra::is_child_terminated(sess.pid)) break;
    contra::msleep(10);
  }

  kill(sess.pid, SIGTERM);
  contra::set_fd_nonblock(STDIN_FILENO, oldNonblock);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &oldTermios);

  contra::ansi::termcap_sgr_type sgrcap;
  sgrcap.initialize();
  contra::ansi::tty_observer target(term, stdout, &sgrcap);
  target.print_screen(b);

  std::FILE* file = std::fopen("impl2-dump.txt", "w");
  b.debug_print(file);
  std::fclose(file);

  return 0;
}
