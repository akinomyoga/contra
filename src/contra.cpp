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
  contra::initialize_errdev();
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
    contra::xprint(contra::errdev(), "contra: twin not supported\n");
    return 1;
#endif
  } else if (std::strcmp(argv[1], "x11") == 0) {
    if (!contra::tx11::run(actx)) return 1;
  } else if (std::strcmp(argv[1], "tty") == 0) {
    if (!contra::ttty::run(actx)) return 1;
  } else if (std::strcmp(argv[1], "test") == 0) {
    using curpos_t = contra::ansi::curpos_t;

    const char* sw = 2 < argc ? argv[2] : std::getenv("COLUMNS");
    const char* sh = 3 < argc ? argv[3] : std::getenv("LINES");
    int const w = std::clamp(sw ? std::atoi(sw) : 80, 1, 9999);
    int const h = std::clamp(sh ? std::atoi(sh) : 24, 1, 9999);

    contra::ansi::term_t term(w, h);

    // 標準入力を全て term に食わせる
    for (int c; (c = std::getchar()) != EOF; ) {
      char b = (char) c;
      term.write(&b, sizeof b);
    }

    // 中身を標準出力に書き出し
    auto const& board = term.board();
    for (curpos_t y = 0; y < board.height(); y++) {
      auto const& line = board.line(y);
      for (curpos_t x = 0; x < board.width(); x++) {
        char32_t c = line.char_at(x).value;
        if (!(c & contra::charflag_wide_extension))
          contra::encoding::put_u8(c == U'\0' ? U' ' : c, stdout);
      }
      std::fputc('\n', stdout);
    }

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
    contra::xprint(contra::errdev(), "contra: unknown subcommand \"");
    contra::xprint(contra::errdev(), argv[1]);
    contra::xprint(contra::errdev(), "\"\n");
    return 1;
  }

  return 0;
}
