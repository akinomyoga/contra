#include <csignal>
#include <vector>
#include <mwg/except.h>
#include "sys.signal.hpp"

namespace contra::sys {

  static void process_default_handler(int sig, signal_handler_t handler) {
    if (handler == SIG_DFL) {
      signal_handler_t const save = std::signal(sig, SIG_DFL);
      std::raise(sig);
      std::signal(sig, save);
    } else if (handler != nullptr && handler != SIG_IGN) {
      handler(sig);
    }
  }

  static bool sigwinch_raised = false;
  static signal_handler_t sigwinch_default_handler = nullptr;
  static std::vector<signal_handler_t> sigwinch_handlers;
  static void trap_winch(int) {
    // シグナル処理中に別のシグナルを受けると処理系定義なので
    // ここでは記録だけしてできるだけ早く抜ける様にする。
    sigwinch_raised = true;
  }

  void setup_signal() {
    sigwinch_default_handler = std::signal(SIGWINCH, &trap_winch);
  }
  void process_signals() {
    if (sigwinch_raised) {
      process_default_handler(SIGWINCH, sigwinch_default_handler);
      for (auto handler : sigwinch_handlers) handler(SIGWINCH);
      sigwinch_raised = false;
    }
  }
  void add_sigwinch_handler(signal_handler_t h) {
    sigwinch_handlers.push_back(h);
  }

}
