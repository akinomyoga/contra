// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef contra_enc_c2w_hpp
#define contra_enc_c2w_hpp

namespace contra {
namespace encoding {

  enum c2w_type {
    _c2w_width_mask = 0x0F,
    _c2w_emoji_mask = 0xF0,
    _c2w_emoji_shift = 4,

    c2w_width_west  = 1,
    c2w_width_east  = 2,
    c2w_width_emacs = 3,

    c2w_emoji_narrow = 0x10,
    c2w_emoji_wide   = 0x20,
  };

  int c2w(char32_t u, c2w_type type);

}
}

#endif
