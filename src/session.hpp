// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef contra_session_hpp
#define contra_session_hpp
#include <cstdint>
#include <cstdio>
#include <sstream>
#include <iterator>
#include <memory>
#include "ansi/line.hpp"
#include "ansi/term.hpp"
#include "enc.utf8.hpp"
#include "pty.hpp"

namespace contra {
namespace term {

  using term_t = contra::ansi::term_t;
  using tstate_t = contra::ansi::tstate_t;
  using board_t = contra::ansi::board_t;
  using curpos_t = contra::ansi::curpos_t;
  using coord_t = contra::ansi::coord_t;

  class terminal_application {
    struct winsize init_ws;
  public:
    void init_size(curpos_t width, curpos_t height) {
      init_ws.ws_col = width;
      init_ws.ws_row = height;
    }
    void init_size(curpos_t width, curpos_t height, coord_t xpixel, coord_t ypixel) {
      init_size(width, height);
      init_ws.ws_xpixel = xpixel;
      init_ws.ws_ypixel = ypixel;
    }
    struct winsize const& winsize() const { return init_ws; }

  private:
    std::unique_ptr<contra::ansi::term_t> m_term;
  public:
    term_t& term() { return *m_term; }
    term_t const& term() const { return *m_term; }
    board_t& board() { return m_term->board(); }
    board_t const& board() const { return m_term->board(); }
    tstate_t& state() { return m_term->state(); }
    tstate_t const& state() const { return m_term->state(); }

  public:
    terminal_application() {
      init_size(80, 24, 13, 7);
    }

    bool initialize() {
      if (m_term) return true;
      m_term = std::make_unique<contra::ansi::term_t>(init_ws.ws_col, init_ws.ws_row);
      return true;
    }

  };

  // contra/ttty
  class terminal_session: public terminal_application {
    typedef terminal_application base;
  public:
    struct termios* init_termios = NULL;
    std::size_t init_read_buffer_size = 4096;

    pty_session::exec_error_handler_t init_exec_error_handler = nullptr;
    std::uintptr_t init_exec_error_param = 0;
  public:
    void init_environment_variable(const char* name, const char* value) {
      m_pty.set_environment_variable(name, value);
    }

  private:
    contra::term::pty_session m_pty; // ユーザ入力書込先
    contra::multicast_device m_dev;  // 受信データ書込先

  public:
    contra::idevice& input_device() { return m_pty; }
    contra::multicast_device& output_device() { return m_dev; }

  public:
    bool is_active() const { return m_pty.is_active(); }
    bool initialize() {
      if (m_pty.is_active()) return true;

      if (!base::initialize()) return false;

      m_pty.set_read_buffer_size(init_read_buffer_size);
      m_pty.set_exec_error_handler(init_exec_error_handler, init_exec_error_param);
      if (!m_pty.start("/bin/bash", &base::winsize(), init_termios)) return false;

      base::term().set_input_target(m_pty);
      m_dev.push(&base::term());
      return true;
    }
    bool process() { return m_pty.read(&m_dev); }
    bool is_alive() { return m_pty.is_alive(); }
    void terminate() { return m_pty.terminate(); }

  public:
    void reset_size(std::size_t width, std::size_t height) {
      mwg_check(width > 0 && height > 0, "non-positive size is passed (width = %d, height = %d).", (int) width, (int) height);
      board().reset_size(width, height);
      base::init_size(width, height);
      m_pty.set_winsize(&base::winsize());
    }

  };
}
}
#endif
