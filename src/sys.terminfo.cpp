#include "sys.terminfo.hpp"

#ifdef use_ncurses
#include <term.h>

namespace contra::sys {
  bool xenl() {
    return ::tgetflag((char*) "xn");
  }
}

#else

namespace contra::sys {
  bool xenl() {
    // ToDo 自前で /usr/share/terminfo を読み取る?
    return true;
  }
}

#endif
