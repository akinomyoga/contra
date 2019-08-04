// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef contra_pty_hpp
#define contra_pty_hpp
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <string>
#include "contradef.hpp"
#include "manager.hpp"

struct termios;

namespace contra {
namespace term {

  void msleep(int milliseconds);
  void usleep(int usec);
  bool fd_set_nonblock(int fd, bool value);
  std::size_t read_from_fd(int fdsrc, contra::idevice* dst, char* buff, std::size_t size);
  void fd_set_winsize(int fd, struct winsize const* ws);

  class fd_device: public contra::idevice {
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

  typedef void (*exec_error_handler_t)(int errno_, std::uintptr_t param);

  struct terminal_session_parameters {
    curpos_t col = 80, row = 24, xpixel = 7, ypixel = 13;
    curpos_t scroll_buffer_size = 1000;
    exec_error_handler_t exec_error_handler = nullptr;
    std::uintptr_t exec_error_param = 0u;
    struct termios* termios = nullptr;
    std::unordered_map<std::string, std::string> env;
    const char* shell = nullptr;
    std::size_t fd_read_buffer_size = 4096;

    int dbg_fd_tee = -1;
    const char* dbg_sequence_logfile = nullptr;
  };

  std::unique_ptr<terminal_application> create_terminal_session(terminal_session_parameters& params);

}
}
#endif
