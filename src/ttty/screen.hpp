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
#include "../sequence.hpp"
#include "../sys.signal.hpp"
#include "../sys.terminfo.hpp"
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

    void reset_size(curpos_t width, curpos_t height, coord_t xunit, coord_t yunit) {
      m_manager.reset_size(width, height, xunit, yunit);
      renderer->reset_size(width, height);
    }

  private:

    class input_decoder_t: public idevice {
    private:
      typedef input_decoder<input_decoder_t> decoder_type;
      friend decoder_type;

      ttty_screen* m_screen;
      decoder_type m_decoder;
      ansi::term_t* term;

    public:
      input_decoder_t(ttty_screen* main): m_screen(main), m_decoder(this) {}

    private:
      std::uint64_t enc_state = 0;
      std::vector<char32_t> enc_buffer;
    public:
      void write(const char* data, std::size_t const size) {
        enc_buffer.resize(size);
        char32_t* const q0 = &enc_buffer[0];
        char32_t* q1 = q0;
        contra::encoding::utf8_decode(data, data + size, q1, q0 + size, enc_state);

        term = &m_screen->manager().app().term();
        m_decoder.decode(q0, q1);
        term->input_flush();
      }
    private:
      virtual void dev_write(char const* data, std::size_t size) override {
        this->write(data, size);
      }

    private:
      friend decoder_type;
      void process_input_data(contra::input_data_type type, std::u32string const& data) {
        if (type == contra::input_data_paste)
          m_screen->manager().input_paste(data);
      }
      void process_key(key_t key) {
        if (key & modifier_super)
          key = (key & ~modifier_super) | modifier_application;
        m_screen->manager().input_key(key);
      }
      void process_control_sequence(sequence const& seq) { contra_unused(seq); }
      void process_invalid_sequence(sequence const& seq) { contra_unused(seq); }
    };
    input_decoder_t m_input_decoder { this };

    class ttty_events: public contra::term::terminal_events {
      ttty_screen* win;
    public:
      ttty_events(ttty_screen* win): win(win) {}
      virtual bool create_new_session() override {
        return win->add_terminal_session();
      }
    };

    ttty_events m_events {this};

  public:
    ttty_screen(int fd_in, int fd_out): fd_in(fd_in), fd_out(fd_out) {
      m_manager.set_events(m_events);
    }

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

    void initialize_session(curpos_t width, curpos_t height, coord_t xunit, coord_t yunit) {
      setup_tty();
      m_manager.reset_size(width, height, xunit, yunit);

      sgrcap.initialize();
      renderer = std::make_unique<contra::ttty::tty_observer>(stdout, &sgrcap);
      renderer->reset_size(width, height);
      renderer->set_xenl(contra::sys::xenl());
    }

    bool add_terminal_session(term::terminal_session_parameters& params) {
      std::unique_ptr<term::terminal_application> sess = contra::term::create_terminal_session(params);
      if (!sess) return false;
      m_manager.add_app(std::move(sess));
      return true;
    }
    bool add_terminal_session() {
      term::terminal_session_parameters params;
      params.col = m_manager.width();
      params.row = m_manager.height();
      params.xunit = m_manager.xunit();
      params.yunit = m_manager.yunit();
      return add_terminal_session(params);
    }

  public:
    bool initialize(curpos_t width, curpos_t height, coord_t xunit, coord_t yunit) {
      if (!add_terminal_session()) return false;
      initialize_session(width, height, xunit, yunit);
      return true;
    }
    bool initialize(contra::term::terminal_session_parameters& params) {
      if (!add_terminal_session(params)) return false;
      initialize_session(params.col, params.row, params.xunit, params.yunit);
      return true;
    }

    void do_loop(bool render_to_stdout = true) {
      char buff[4096];
      for (;;) {
        bool const processed = m_manager.do_events();
        if (m_manager.m_dirty && render_to_stdout)
          renderer->update(m_manager.app().view());

        contra::sys::process_signals();

        // ToDo: 本来はここはキー入力に変換してから m_manager に渡すべき。
        //if (contra::term::read_from_fd(fd_in, &m_manager.app().term().input_device(), buff, sizeof(buff))) continue;
        if (contra::term::read_from_fd(fd_in, &m_input_decoder, buff, sizeof(buff))) continue;
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
