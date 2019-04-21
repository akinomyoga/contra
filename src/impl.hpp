// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef CONTRA_IMPL_HPP
#define CONTRA_IMPL_HPP
#include <cstdint>
#include <cstdio>
#include <vector>
#include <termios.h>
#include <stdio.h>
#include "sequence.h"
#include "ansi/line.hpp"
#include "ansi/term.hpp"
#include "ansi/observer.tty.hpp"

namespace contra {

  void msleep(int milliseconds);

  struct idevice {
    virtual void write(char const* data, ssize_t size) = 0;
  };

  bool read_from_fd(int fdsrc, contra::idevice* dst, char* buff, std::size_t size);
  void set_fd_nonblock(int fd);
  bool is_child_terminated(pid_t pid);

  struct session {
    int masterfd;
    int pid;
  };

  bool create_session(session* sess, struct termios& termios, const char* shell, int cols = -1, int rows = -1);

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

    int interval() { return m_milliseconds; }
    void set_interval(int milliseconds) { this->m_milliseconds = milliseconds; }

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
    ansi::term_t* term;
  public:
    tty_player_device(ansi::term_t* term): term(term) {}
    virtual void write(char const* data, ssize_t size) override {
      term->write(data, size);
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
#endif
