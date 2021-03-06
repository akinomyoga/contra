// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef contra_sys_signal_hpp
#define contra_sys_signal_hpp

namespace contra::sys {
  typedef void (*signal_handler_t)(int);
  void setup_signal();
  void process_signals();
  void add_sigwinch_handler(signal_handler_t h);
}

#endif
