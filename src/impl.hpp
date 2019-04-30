// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef CONTRA_IMPL_HPP
#define CONTRA_IMPL_HPP
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

  std::size_t read_from_fd(int fdsrc, contra::idevice* dst, char* buff, std::size_t size);
  void write_to_fd(int fd, byte const* p, std::size_t size, int wait_interval);
  bool set_fd_nonblock(int fd, bool value);
  bool is_child_terminated(pid_t pid);

  struct session {
    int masterfd;
    int pid;
  };
  bool create_session(session* sess, const char* shell, winsize const* ws = NULL, struct termios* termios = NULL);

  class fd_device: public idevice {
    int m_fd;
    int m_milliseconds;

  public:
    fd_device(int fd): m_fd(fd), m_milliseconds(1) {}

    int interval() { return m_milliseconds; }
    void set_interval(int milliseconds) { this->m_milliseconds = milliseconds; }

    virtual void dev_write(char const* data, std::size_t size) override {
      contra::term::write_to_fd(m_fd, reinterpret_cast<byte const*>(data), size, m_milliseconds);
    }
  };

}
}
#endif
