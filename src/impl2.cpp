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

void msleep(int milliseconds) {
  struct timespec tv;
  tv.tv_sec = 0;
  tv.tv_nsec = milliseconds * 1000000;
  nanosleep(&tv, NULL);
}

#include "sequence.h"
#include "ansi/line.hpp"
#include "ansi/term.hpp"
#include "ansi/observer.tty.hpp"

namespace contra {
  struct idevice {
    virtual void write(char const* data, ssize_t size) = 0;
  };

  class multicast_device: public idevice {
    std::vector<idevice*> m_list;

  public:
    void push(idevice* dev) {this->m_list.push_back(dev);}

    virtual void write(char const* data, ssize_t size) override {
      for (idevice* const dev: m_list)
        dev->write(data, size);
    }
  };

  class fd_device: public idevice {
    int m_fd;
    int m_milliseconds;

  public:
    fd_device(int fd): m_fd(fd), m_milliseconds(1) {}

    int interval() {return m_milliseconds;}
    void set_interval(int milliseconds) {this->m_milliseconds = milliseconds;}

    virtual void write(char const* data, ssize_t size) override {
      const char* p = data;
      for (; size > 0; ) {
        int const n = ::write(m_fd, p, size);
        if (n > 0) {
          p += n;
          size -= n;
        } else
          msleep(m_milliseconds);
      }
    }
  };

  class tty_player_device: public idevice {
    ansi::tty_player* play;
  public:
    tty_player_device(ansi::tty_player* play): play(play) {}
    virtual void write(char const* data, ssize_t size) override {
      play->write(data, size);
    }
  };

  class sequence_printer_device: public idevice {
    sequence_printer* printer;
  public:
    sequence_printer_device(sequence_printer* printer): printer(printer) {}
    virtual void write(char const* data, ssize_t size) override {
      printer->write(data, size);
    }
  };
}

bool read_from_fd(int fdsrc, contra::idevice* dst, char* buff, std::size_t size) {
  ssize_t const nread = read(fdsrc, buff, size);
  if (nread <= 0) return false;
  dst->write(buff, nread);
  return true;
}

void set_fd_nonblock(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    perror("sample_openpt (fcntl(0, F_GETFL))");
    exit(1);
  }

  if (flags & O_NONBLOCK) return;

  int const result = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  if (result < 0) {
    perror("sample_openpt (fcntl(0, F_SETFL))");
    exit(1);
  }
}

bool is_child_terminated(pid_t pid) {
  int status;
  pid_t const result = waitpid(pid, &status, WNOHANG);
  if (result < 0) {
    perror("sample_openpt (waitpid)");
    exit(1);
  }
  return result > 0;
}

struct session {
  int masterfd;
  int pid;
};

bool create_session(session* sess, struct termios& termios, const char* shell) {
  struct winsize winsize;
  ioctl(STDIN_FILENO, TIOCGWINSZ, (char *) &winsize);

  int masterfd, slavefd;
  char const* slavedevice;
  if ((masterfd = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK)) < 0
    || grantpt(masterfd) < 0
    || unlockpt(masterfd) < 0
    || (slavedevice = ptsname(masterfd)) == NULL
    || (slavefd = open(slavedevice, O_RDWR | O_NOCTTY)) < 0)
    return false;

  pid_t const pid = fork();
  if (pid < 0) return false;

  if (pid == 0) {
    setsid();
    tcsetattr(slavefd, TCSANOW, &termios);
    ioctl(slavefd, TIOCSWINSZ, &winsize);

    dup2(slavefd, STDIN_FILENO);
    dup2(slavefd, STDOUT_FILENO);
    dup2(slavefd, STDERR_FILENO);
    close(slavefd);
    close(masterfd);
    //execl(shell, shell, "--norc", NULL);
    execl(shell, shell, NULL);
    perror("sample_openpt (exec(SHELL))");
    _exit(1);
  } else {
    close(slavefd);
    sess->pid = pid;
    sess->masterfd = masterfd;
    return true;
  }
}

int main() {
  struct termios oldTermios;
  tcgetattr(STDIN_FILENO, &oldTermios);
  session sess;
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

  contra::ansi::tty_player term(b);
  contra::tty_player_device d1(&term);
  dev.push(&d1);

  // contra::sequence_printer printer("impl2-allseq.txt");
  // contra::sequence_printer_device d2(&printer);
  // dev.push(&d2);

  set_fd_nonblock(STDIN_FILENO);
  contra::fd_device devIn(sess.masterfd);

  char buff[4096];
  for (;;) {
    if (read_from_fd(sess.masterfd, &dev, buff, sizeof(buff))) continue;
    if (read_from_fd(STDIN_FILENO, &devIn, buff, sizeof(buff))) continue;
    if (is_child_terminated(sess.pid)) break;
    msleep(10);
  }

  kill(sess.pid, SIGTERM);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &oldTermios);

  contra::ansi::termcap_sgr_type sgrcap;
  sgrcap.initialize();
  contra::ansi::tty_observer target(stdout, &sgrcap);
  target.print_screen(b);

  return 0;
}
