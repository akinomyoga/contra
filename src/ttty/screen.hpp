// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef contra_ttty_screen_hpp
#define contra_ttty_screen_hpp
#include <memory>
#include <cstdio>
#include <time.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <fcntl.h>

#include "../pty.hpp"
#include "../session.hpp"
#include "../dict.hpp"
#include "buffer.hpp"

namespace contra {
namespace ttty {

  struct ttty_screen {
    int fd_in, fd_out;

    struct termios old_termios;
    int old_nonblock;

    contra::dict::termcap_sgr_type sgrcap;

    contra::term::terminal_session sess;

    std::unique_ptr<contra::ttty::tty_observer> renderer;

  public:
    contra::term::terminal_session& session() { return sess; }
    contra::term::terminal_session const& session() const { return sess; }

  public:
    ttty_screen(int fd_in, int fd_out): fd_in(fd_in), fd_out(fd_out) {}

  private:
    void setup_terminal() {
      tcgetattr(fd_in, &this->old_termios);
      struct termios termios = this->old_termios;
      termios.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
      termios.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
      termios.c_cflag &= ~(CSIZE | PARENB);
      termios.c_cflag |= CS8;
      termios.c_oflag &= ~(OPOST);
      termios.c_cc[VMIN]  = 1;
      termios.c_cc[VTIME] = 0;
      tcsetattr(fd_in, TCSAFLUSH, &termios);
      this->old_nonblock = contra::term::set_fd_nonblock(fd_in, true);
    }
    void reset_terminal() {
      contra::term::set_fd_nonblock(fd_in, this->old_nonblock);
      tcsetattr(fd_in, TCSAFLUSH, &this->old_termios);
    }

  public:
    bool initialize() {
      setup_terminal();

      if (!sess.initialize()) return false;

      sgrcap.initialize();
      renderer = std::make_unique<contra::ttty::tty_observer>(sess.term(), stdout, &sgrcap);
      return true;
    }

    void do_loop(bool render_to_stdout = true) {
      char buff[4096];
      for (;;) {
        bool processed = false;
        clock_t time0 = clock();
        while (sess.process()) {
          processed = true;
          clock_t const time1 = clock();
          clock_t const msec = (time1 - time0) * 1000 / CLOCKS_PER_SEC;
          if (msec > 20) break;
        }
        if (processed && render_to_stdout)
          renderer->update();

        if (contra::term::read_from_fd(fd_in, &sess.input_device(), buff, sizeof(buff))) continue;
        if (!sess.is_alive()) break;
        if (!processed)
          contra::term::msleep(10);
      }

      sess.terminate();
    }

    void finalize() {
      this->reset_terminal();
    }
  };

}
}

#endif
