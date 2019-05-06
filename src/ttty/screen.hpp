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
#include "../manager.hpp"
#include "../dict.hpp"
#include "buffer.hpp"

namespace contra {
namespace ttty {

  struct ttty_screen {
    int fd_in, fd_out;

    struct termios old_termios;
    int old_nonblock;

    contra::dict::termcap_sgr_type sgrcap;
    std::unique_ptr<contra::ttty::tty_observer> renderer;

  private:
    contra::term::terminal_manager m_manager;
  public:
    contra::term::terminal_manager& manager() { return m_manager; }
    contra::term::terminal_manager const& manager() const { return m_manager; }

  public:
    ttty_screen(int fd_in, int fd_out): fd_in(fd_in), fd_out(fd_out) {}

  private:
    void setup_tty() {
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
      this->old_nonblock = contra::term::fd_set_nonblock(fd_in, true);
    }

    void reset_tty() {
      contra::term::fd_set_nonblock(fd_in, this->old_nonblock);
      tcsetattr(fd_in, TCSAFLUSH, &this->old_termios);
    }

  public:
    bool initialize(contra::term::terminal_session_parameters const& params) {
      setup_tty();

      std::unique_ptr<contra::term::terminal_application> sess = contra::term::create_terminal_session(params);
      if (!sess) return false;
      m_manager.add_app(std::move(sess));

      sgrcap.initialize();
      renderer = std::make_unique<contra::ttty::tty_observer>(sess->term(), stdout, &sgrcap);
      return true;
    }

    void do_loop(bool render_to_stdout = true) {
      char buff[4096];
      for (;;) {
        bool processed = m_manager.do_events();
        if (m_manager.m_dirty && render_to_stdout)
          renderer->update();

        // ToDo: 本来はここはキー入力に変換してから m_manager に渡すべき。
        if (contra::term::read_from_fd(fd_in, &m_manager.app().term().input_device(), buff, sizeof(buff))) continue;
        if (!m_manager.is_alive()) break;
        if (!processed)
          contra::term::msleep(10);
      }

      m_manager.terminate();
    }

    void finalize() {
      this->reset_tty();
    }
  };

}
}

#endif
