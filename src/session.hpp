// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef contra_session_hpp
#define contra_session_hpp
#include <cstdint>
#include <cstdio>
#include <sstream>
#include <iterator>
#include <memory>
#include <chrono>
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
    virtual ~terminal_application() {}

    bool initialize() {
      if (m_term) return true;
      m_term = std::make_unique<contra::ansi::term_t>(init_ws.ws_col, init_ws.ws_row);
      return true;
    }

  public:
    virtual bool process() { return false; }
    virtual bool is_active() const { return true; }
    virtual bool is_alive() { return true; }
    virtual void terminate() {}

  public:
    virtual void input_key(key_t key) {
      m_term->input_key(key);
    }
    virtual void input_mouse(key_t key, coord_t px, coord_t py, curpos_t x, curpos_t y) {
      m_term->input_mouse(key, px, py, x, y);
    }
    virtual void reset_size(std::size_t width, std::size_t height) {
      mwg_check(width > 0 && height > 0, "non-positive size is passed (width = %d, height = %d).", (int) width, (int) height);
      this->init_size(width, height);
      this->board().reset_size(width, height);
    }

  };

  // contra/ttty
  class terminal_session: public terminal_application {
    typedef terminal_application base;
  public:
    struct termios* init_termios = NULL;
    std::size_t init_read_buffer_size = 4096;

    pty_connection::exec_error_handler_t init_exec_error_handler = nullptr;
    std::uintptr_t init_exec_error_param = 0;
  public:
    void init_environment_variable(const char* name, const char* value) {
      m_pty.set_environment_variable(name, value);
    }

  private:
    contra::term::pty_connection m_pty; // ユーザ入力書込先
    contra::multicast_device m_dev;  // 受信データ書込先

  public:
    contra::idevice& input_device() { return m_pty; }
    contra::multicast_device& output_device() { return m_dev; }

  public:
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
    virtual bool process() override { return m_pty.read(&m_dev); }
    virtual bool is_active() const override { return m_pty.is_active(); }
    virtual bool is_alive() override { return m_pty.is_alive(); }
    virtual void terminate() override { return m_pty.terminate(); }

  public:
    virtual void reset_size(std::size_t width, std::size_t height) override {
      base::reset_size(width, height);
      m_pty.set_winsize(&base::winsize());
    }
  };

  class terminal_manager {
    std::vector<std::shared_ptr<terminal_application> > m_apps;

  public:
    terminal_manager() {}

  public:
    terminal_application& app() const {
      mwg_check(m_apps.size());
      return *m_apps[0].get();
    }
    template<typename T>
    void add_app(T&& app) { m_apps.emplace_back(std::forward<T>(app)); }

  private:
    // ToDo: foreground/background で優先順位をつけたい。
    // 後、現在 process1 を呼び出す度に全ての app に対して処理を行っているが、
    // 一度 processed = false になった物は何度も試さない様にする。
    bool process1() {
      bool processed = false;
      for (auto const& app : m_apps)
        if (app->process())
          processed = true;
      return processed;
    }
  public:
    bool do_events() {
      bool processed = false;
      auto const time0 = std::chrono::high_resolution_clock::now();
      while (this->process1()) {
        processed = true;
        auto const time1 = std::chrono::high_resolution_clock::now();
        auto const msec = std::chrono::duration_cast<std::chrono::milliseconds>(time1 - time0);
        if (msec.count() > 20) break;
      }
      return processed;
    }

    bool is_active() {
      for (auto const& app : m_apps)
        if (app->is_active()) return true;
      return false;
    }
    bool is_alive() {
      // ToDo: 死んだ物に関しては閉じる?
      for (auto const& app : m_apps)
        if (app->is_alive()) return true;
      return false;
    }
    void terminate() {
      for (auto const& app : m_apps)
        app->terminate();
    }

  public:
    void input_key(key_t key) {
      app().input_key(key);
    }
    void input_mouse(key_t key, coord_t px, coord_t py, curpos_t x, curpos_t y) {
      app().input_mouse(key, px, py, x, y);
    }
    void reset_size(std::size_t width, std::size_t height) {
      app().reset_size(width, height);
    }

  };
}
}
#endif
