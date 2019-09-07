#include <cstring>
#include <string>
#include "context.hpp"
#include "pty.hpp"
#include "ttty/buffer.hpp"
#include "ttty/screen.hpp"
#include "sys.signal.hpp"
#include "sys.path.hpp"

namespace contra::tx11 {
  bool run(contra::app::context& actx);
}
namespace contra::twin {
  bool run(contra::app::context& actx);
}
namespace contra::ttty {
  bool run(contra::app::context& actx);
}

int main(int argc, char** argv) {
  contra::sys::initialize_path(argc, const_cast<const char**>(argv));
  contra::sys::setup_signal();

  contra::app::context actx;
  std::string config_dir = contra::sys::get_config_directory();
  actx.load((config_dir + "/contra.conf").c_str());

  if (argc < 2) {
#ifdef use_twin
    if (!contra::twin::run(actx)) return 1;
#else
    if (!contra::tx11::run(actx)) return 1;
#endif
  } else if (std::strcmp(argv[1], "win") == 0) {
#ifdef use_twin
    if (!contra::twin::run(actx)) return 1;
#else
    std::fprintf(stderr, "contra: twin not supported\n");
    return 1;
#endif
  } else if (std::strcmp(argv[1], "x11") == 0) {
    if (!contra::tx11::run(actx)) return 1;
  } else if (std::strcmp(argv[1], "tty") == 0) {
    if (!contra::ttty::run(actx)) return 1;
  } else if (std::strcmp(argv[1], "--help") == 0) {
    std::cout
#ifdef use_twin
      << "usage: contra [win|x11|tty]\n"
#else
      << "usage: contra [x11|tty]\n"
#endif
      << "usage: contra --help\n"
      << std::endl;
    return 0;
  } else {
    std::fprintf(stderr, "contra: unknown subcommand \"%s\"\n", argv[1]);
    return 1;
  }

  return 0;
}
