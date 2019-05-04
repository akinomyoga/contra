// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef contra_pty_hpp
#define contra_pty_hpp
#include <sys/ioctl.h>
#include <termios.h>
#include <stdio.h>
#include <limits.h>
#include <cstdint>
#include <cstdio>
#include <vector>
#include "sequence.hpp"
#include "ansi/line.hpp"
#include "ansi/term.hpp"
#include "ttty/buffer.hpp"

namespace contra {
namespace term {

  void msleep(int milliseconds);
  bool fd_set_nonblock(int fd, bool value);
  std::size_t read_from_fd(int fdsrc, contra::idevice* dst, char* buff, std::size_t size);
  void fd_set_winsize(int fd, struct winsize const* ws);

  class fd_device: public idevice {
  protected:
    int m_fd = -1;
    int m_write_interval = 1;

  public:
    fd_device() {}
    explicit fd_device(int fd): m_fd(fd) {}

    int fd() const { return m_fd; }

    int write_interval() const { return m_write_interval; }
    void set_write_interval(int milliseconds) { this->m_write_interval = milliseconds; }
    void write(char const* data, std::size_t size) const;

  private:
    virtual void dev_write(char const* data, std::size_t size) override {
      this->write(data, size);
    }
  };

  class pty_session: public fd_device {
  public:
    typedef void (*exec_error_handler_t)(int errno_, std::uintptr_t param);

  private:
    bool m_active = false;
    int slave_pid = -1;

    std::size_t m_read_buffer_size = 4096;
    std::vector<char> m_read_buffer;

    exec_error_handler_t m_exec_error_handler = NULL;
    std::uintptr_t m_exec_error_param = 0;

  public:
    pty_session() {}
    pty_session(const char* shell, winsize const* ws = NULL, struct termios* termios = NULL) {
      start(shell, ws, termios);
    }

    void set_read_buffer_size(std::size_t value) {
      m_read_buffer_size = contra::clamp(value, 0x100, 0x10000);
    }
    void set_exec_error_handler(exec_error_handler_t handler, std::uintptr_t param) {
      this->m_exec_error_handler = handler;
      this->m_exec_error_param = param;
    }
    bool start(const char* shell, winsize const* ws = NULL, struct termios* termios = NULL);

    std::size_t read(contra::idevice* dst) {
      if (!m_active) return 0;
      return read_from_fd(m_fd, dst, &m_read_buffer[0], m_read_buffer.size());
    }

    void terminate();

    bool is_active() const { return m_active; }
    bool is_alive();

    virtual ~pty_session() {}

    void set_winsize(struct winsize* ws) {
      fd_set_winsize(m_fd, ws);
    }
  };

}
}
#endif
