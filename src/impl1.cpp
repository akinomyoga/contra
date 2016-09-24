#include "board.h"
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <vector>

using namespace contra;

//-----------------------------------------------------------------------------
//
// SGR capabilities
//
// Note:
//   さすがにどの端末も SGR 0 は対応していると仮定している。
//   (但し、端末が SGR 自体に対応していない場合はこの限りではない。)
//

struct termcap_sgrflag1 {
  aflags_t bit;
  unsigned on;
  unsigned off;
};

struct termcap_sgrflag2 {
  aflags_t bit1;
  unsigned on1;
  aflags_t bit2;
  unsigned on2;
  unsigned off;
};

struct termcap_sgrideogram {
  /*?lwiki
   * @var bool is_decoration_exclusive;
   *   現在のところ ideogram decorations はどれか一つだけが有効になる様に実装することにする。
   *   (但し、将来それぞれ独立に on/off する様に変更できる余地を残すために各項目 1bit 占有している。)
   *   但し、実際の端末では複数を有効にすることができる実装もあるだろう。
   *   その場合には、一貫した動作をさせる為には必ず既に設定されている装飾を解除する必要がある。
   *   この設定項目は出力先の端末で ideogram decorations が排他的かどうかを保持する。
   */
  // ToDo: RLogin の動作を確認する。
  bool is_decoration_exclusive {true};
  unsigned single_rb {60};
  unsigned double_rb {61};
  unsigned single_lt {62};
  unsigned double_lt {63};
  unsigned single_lb {66};
  unsigned double_lb {67};
  unsigned single_rt {68};
  unsigned double_rt {69};
  unsigned stress    {64};
  unsigned reset     {65};
};

struct termcap_sgrcolor {
  aflags_t bit;
  unsigned base;
  unsigned off;

  // 90-97, 100-107
  unsigned aixterm_color;

  // 38:5:0-255
  unsigned      iso8613_color       ; //!< SGR number of ISO 8613-6 color specification
  unsigned char iso8613_color_spaces; //!< supported color spaces specified by bits
  char          iso8613_separater   ; //!< the separator character of ISO 8613-6 SGR arguments
  unsigned      indexed_color_number; //!< the number of colors available through 38:5:*

  // 1,22 / 5,25 で明るさを切り替える方式
  unsigned high_intensity_on ;
  unsigned high_intensity_off;
};

struct termcap_sgr_type {
  // aflags
  termcap_sgrflag2 cap_bold         {is_bold_set     , 1, is_faint_set           ,  2, 22};
  termcap_sgrflag2 cap_italic       {is_italic_set   , 3, is_fraktur_set         , 20, 23};
  termcap_sgrflag2 cap_underline    {is_underline_set, 4, is_double_underline_set, 21, 24};
  termcap_sgrflag2 cap_blink        {is_blink_set    , 5, is_rapid_blink_set     ,  6, 25};
  termcap_sgrflag1 cap_inverse      {is_inverse_set  , 7, 27};
  termcap_sgrflag1 cap_invisible    {is_invisible_set, 8, 28};
  termcap_sgrflag1 cap_strike       {is_strike_set   , 9, 29};

  // xflags
  termcap_sgrflag2 cap_framed       {is_frame_set       , 51, is_circle_set, 52, 54};

  termcap_sgrflag1 cap_proportional {is_proportional_set, 26, 50};
  termcap_sgrflag1 cap_overline     {is_overline_set    , 53, 55};

  termcap_sgrideogram cap_ideogram;

  // colors
  termcap_sgrcolor cap_fg {
    is_fg_color_set, 30, 39,  90,
    38, color_space_indexed_bit | color_space_rgb_bit, ascii_semicolon, 256,
    0, 0
  };
  termcap_sgrcolor cap_bg {
    is_bg_color_set, 40, 49, 100,
    48, color_space_indexed_bit | color_space_rgb_bit, ascii_semicolon, 256,
    0, 0
  };

  aflags_t aflagsNotResettable {0};
  xflags_t xflagsNotResettable {0};
private:
  static void initialize_unresettable_flags(aflags_t& flags, termcap_sgrflag1 const& sgrflag) {
    if (!sgrflag.off && sgrflag.on) flags |= sgrflag.bit;
  }

  static void initialize_unresettable_flags(aflags_t& flags, termcap_sgrflag2 const& sgrflag) {
    if (!sgrflag.off) {
      if (sgrflag.on1) flags |= sgrflag.bit1;
      if (sgrflag.on2) flags |= sgrflag.bit2;
    }
  }

  static void initialize_unresettable_flags(aflags_t& flags, termcap_sgrcolor const& sgrcolor) {
    if (!sgrcolor.off && (sgrcolor.base || sgrcolor.iso8613_color))
      flags = sgrcolor.bit;
  }

  static void initialize_unresettable_flags(aflags_t& flags, termcap_sgrideogram const& capIdeogram) {
    if (!capIdeogram.reset)
      flags |= is_ideogram_decoration_mask;
  }

public:
  void initialize() {
    aflagsNotResettable = 0;
    initialize_unresettable_flags(aflagsNotResettable, cap_bold);
    initialize_unresettable_flags(aflagsNotResettable, cap_italic);
    initialize_unresettable_flags(aflagsNotResettable, cap_underline);
    initialize_unresettable_flags(aflagsNotResettable, cap_blink);
    initialize_unresettable_flags(aflagsNotResettable, cap_inverse);
    initialize_unresettable_flags(aflagsNotResettable, cap_invisible);
    initialize_unresettable_flags(aflagsNotResettable, cap_strike);
    initialize_unresettable_flags(aflagsNotResettable, cap_fg);
    initialize_unresettable_flags(aflagsNotResettable, cap_bg);

    xflagsNotResettable = 0;
    initialize_unresettable_flags(xflagsNotResettable, cap_framed);
    initialize_unresettable_flags(xflagsNotResettable, cap_proportional);
    initialize_unresettable_flags(xflagsNotResettable, cap_overline);
    initialize_unresettable_flags(xflagsNotResettable, cap_ideogram);
  }
};

//-----------------------------------------------------------------------------

struct tty_observer {
  std::FILE* file;
  termcap_sgr_type const* sgrcap;

  tty_observer(std::FILE* file, termcap_sgr_type* sgrcap):
    file(file), sgrcap(sgrcap)
  {
    this->m_attr = 0;
    m_xattr.clear();
  }

  void put(char c) const {std::fputc(c, file);}
  void put_str(const char* s) const {
    while (*s) put(*s++);
  }
  void put_unsigned(unsigned value) const {
    if (value >= 10)
      put_unsigned(value/10);
    put(ascii_0 + value % 10);
  }

private:
  void sgr_clear() const {
    put(ascii_esc);
    put(ascii_lbracket);
    put(ascii_m);
  }

  void sgr_put(unsigned value) {
    if (this->sgr_isOpen) {
      put(ascii_semicolon);
    } else {
      this->sgr_isOpen = true;
      put(ascii_esc);
      put(ascii_lbracket);
    }

    if (value) put_unsigned(value);
  }

  attribute_t        m_attr {0};
  extended_attribute m_xattr;
  bool sgr_isOpen;

  void update_sgrflag1(
    aflags_t aflagsNew, aflags_t aflagsOld,
    termcap_sgrflag1 const& sgrflag
  ) {
    // when the attribute is not changed
    if (((aflagsNew ^ aflagsOld) & sgrflag.bit) == 0) return;

    // when the attribute is not supported by TERM
    if (!sgrflag.on) return;

    if (aflagsNew&sgrflag.bit) {
      sgr_put(sgrflag.on);
    } else {
      sgr_put(sgrflag.off);
    }
  }

  void update_sgrflag2(
    aflags_t aflagsNew, aflags_t aflagsOld,
    termcap_sgrflag2 const& sgrflag
  ) {
    // when the attribute is not changed
    aflags_t bit1 = sgrflag.bit1;
    aflags_t bit2 = sgrflag.bit2;
    if (((aflagsNew ^ aflagsOld) & (bit1 | bit2)) == 0) return;

    // when the attribute is not supported by TERM
    unsigned const sgr1 = sgrflag.on1;
    unsigned const sgr2 = sgrflag.on2;
    if (!sgr1) bit1 = 0;
    if (!sgr2) bit2 = 0;
    if (!(bit1 | bit2)) return;

    aflags_t const added = aflagsNew & ~aflagsOld;
    aflags_t const removed = ~aflagsNew & aflagsOld;

    if (removed & (bit1 | bit2)) {
      sgr_put(sgrflag.off);
      if (aflagsNew & bit2)
        sgr_put(sgr2);
      if (aflagsNew & bit1)
        sgr_put(sgr1);
    } else {
      if (added & bit2)
        sgr_put(sgr2);
      if (added & bit1)
        sgr_put(sgr1);
    }
  }

  void update_ideogram_decoration(xflags_t xflagsNew, xflags_t xflagsOld, termcap_sgrideogram const& cap) {
    if (((xflagsNew ^ xflagsOld) & is_ideogram_decoration_mask) == 0) return;

    int sgr;
    if (xflagsNew & is_ideogram_decoration_mask) {
      if (xflagsNew & (is_ideogram_single_rb_set | is_ideogram_double_rb_set))
        sgr = xflagsNew & is_ideogram_single_rb_set? cap.single_rb: cap.double_rb;
      else if (xflagsNew & (is_ideogram_single_lt_set | is_ideogram_double_lt_set))
        sgr = xflagsNew & is_ideogram_single_lt_set? cap.single_lt: cap.double_lt;
      else if (xflagsNew & (is_ideogram_single_lb_set | is_ideogram_double_lb_set))
        sgr = xflagsNew & is_ideogram_single_lb_set? cap.single_lb: cap.double_lb;
      else if (xflagsNew & (is_ideogram_single_rt_set | is_ideogram_double_rt_set))
        sgr = xflagsNew & is_ideogram_single_rt_set? cap.single_rt: cap.double_rt;
      else if (xflagsNew & is_ideogram_stress_set)
        sgr = cap.stress;
      else
        mwg_assert(0, "is_ideogram_decoration_mask is wrong");

      if (sgr == 0
        || (xflagsOld & is_ideogram_decoration_mask && cap.is_decoration_exclusive)
      )
        sgr_put(cap.reset);
    } else
      sgr = cap.reset;

    if (sgr) sgr_put(sgr);
  }

  void update_sgrcolor(
    int colorSpaceNew, color_t colorNew,
    int colorSpaceOld, color_t colorOld,
    termcap_sgrcolor const& sgrcolor
  ) {
    if (colorSpaceNew == colorSpaceOld && colorNew == colorOld) return;

    // sgrAnsiColor, sgrAixColor
    if (colorSpaceNew == color_space_indexed) {
      if (colorNew < 8) {
        if (sgrcolor.base) {
          // e.g \e[31m
          if (sgrcolor.high_intensity_off)
            sgr_put(sgrcolor.high_intensity_off);
          sgr_put(sgrcolor.base + colorNew);
          return;
        }
      } else if (colorNew < 16) {
        if (sgrcolor.aixterm_color) {
          // e.g. \e[91m
          sgr_put(sgrcolor.aixterm_color + (colorNew & 7));
          return;
        } else if (sgrcolor.base && sgrcolor.high_intensity_on) {
          // e.g. \e[1;31m \e[2;42m
          sgr_put(sgrcolor.high_intensity_on);
          sgr_put(sgrcolor.base + (colorNew & 7));
          return;
        }
      }
    }

    if (colorSpaceNew == color_space_default && sgrcolor.off) {
      sgr_put(sgrcolor.off);
      return;
    }

    // sgrISO8613_6Color
    if (sgrcolor.iso8613_color
      && (sgrcolor.iso8613_color_spaces & 1 << colorSpaceNew)
    ) {
      sgr_put(sgrcolor.iso8613_color);
      put(sgrcolor.iso8613_separater);
      put_unsigned(colorSpaceNew);

      if (colorSpaceNew == color_space_indexed) {
        if (colorNew < sgrcolor.indexed_color_number) {
          put(sgrcolor.iso8613_separater);
          put_unsigned(colorNew);
        }
        return;
      } else {
        int numberOfComponents = 0;
        switch (colorSpaceNew) {
        case color_space_rgb:
        case color_space_cmy:  numberOfComponents = 3; break;
        case color_space_cmyk: numberOfComponents = 4; break;
        }

        for (int icomp = 0; icomp < numberOfComponents; icomp++) {
          unsigned const comp = colorNew << icomp * 8 & 0xFF;
          put(sgrcolor.iso8613_separater);
          put_unsigned(comp);
        }

        return;
      }
    }

    // fallback
    // - ToDo: 色をクリアできない場合の対策。
    //   設定できる色の中で最も近い色を設定する。設定できる色が一つ
    //   もない時はそもそも何の色も設定されていない筈だから気にしな
    //   くて良い。
    if (sgrcolor.off)
      sgr_put(sgrcolor.off);
  }

  void apply_attr(board const& w, attribute_t newAttr) {
    if (this->m_attr == newAttr) return;

    if (newAttr == 0) {
      sgr_clear();
      this->m_attr = 0;
      this->m_xattr.clear();
    } else {
      this->sgr_isOpen = false;

      extended_attribute _xattr;
      if (newAttr & has_extended_attribute)
        _xattr = *w.m_xattr_data.get(newAttr);
      else
        _xattr.load(newAttr);

      aflags_t const aremoved = ~_xattr.aflags & m_xattr.aflags;
      xflags_t const xremoved = ~_xattr.xflags & m_xattr.xflags;
      if (aremoved & sgrcap->aflagsNotResettable
        || xremoved & sgrcap->xflagsNotResettable
      ) {
        sgr_put(0);
        m_xattr.aflags = 0;
        m_xattr.xflags &= non_sgr_xflags_mask;
      }

      update_sgrflag2(_xattr.aflags, m_xattr.aflags, sgrcap->cap_bold     );
      update_sgrflag2(_xattr.aflags, m_xattr.aflags, sgrcap->cap_italic   );
      update_sgrflag2(_xattr.aflags, m_xattr.aflags, sgrcap->cap_underline);
      update_sgrflag2(_xattr.aflags, m_xattr.aflags, sgrcap->cap_blink    );

      update_sgrflag1(_xattr.aflags, m_xattr.aflags, sgrcap->cap_inverse  );
      update_sgrflag1(_xattr.aflags, m_xattr.aflags, sgrcap->cap_invisible);
      update_sgrflag1(_xattr.aflags, m_xattr.aflags, sgrcap->cap_strike   );

      if ((newAttr | this->m_attr) & has_extended_attribute) {
        update_sgrflag2(_xattr.xflags, m_xattr.xflags, sgrcap->cap_framed);
        update_sgrflag1(_xattr.xflags, m_xattr.xflags, sgrcap->cap_proportional);
        update_sgrflag1(_xattr.xflags, m_xattr.xflags, sgrcap->cap_overline);
        update_ideogram_decoration(_xattr.xflags, m_xattr.xflags, sgrcap->cap_ideogram);
      }

      update_sgrcolor(
         _xattr.fg_space(),  _xattr.fg,
        m_xattr.fg_space(), m_xattr.fg,
        sgrcap->cap_fg);

      update_sgrcolor(
         _xattr.bg_space(),  _xattr.bg,
        m_xattr.bg_space(), m_xattr.bg,
        sgrcap->cap_bg);

      this->m_attr = newAttr;
      this->m_xattr = _xattr;

      if (this->sgr_isOpen) put(ascii_m);
    }

  }

public:
  // test implementation
  // ToDo: output encoding
  void output_content(board& w) {
    for (curpos_t y = 0; y < w.m_height; y++) {
      board_cell* line = &w.cell(0, y);
      curpos_t wskip = 0;
      for (curpos_t x = 0; x < w.m_width; x++) {
        if (line[x].character&is_wide_extension) continue;
        if (line[x].character == 0) {
          wskip++;
          continue;
        }

        if (wskip > 0) {
          if (this->m_attr == 0 && wskip <= 4) {
            while (wskip--) put(' ');
          } else {
            put(ascii_esc);
            put(ascii_lbracket);
            put_unsigned(wskip);
            put(ascii_C);
          }
          wskip = 0;
        }

        apply_attr(w, line[x].attribute);
        put(line[x].character);
      }

      put('\n');
    }
  }

};

void put_char(board& w, char32_t u) {
  board_cursor& cur = w.cur;
  int const charwidth = 1; // ToDo 文字幅
  if (cur.x + charwidth > w.m_width) {
    cur.x = 0;
    cur.y++;
    if (cur.y >= w.m_height) cur.y--; // 本当は新しい行を挿入する
  }

  if (charwidth <= 0) {
    std::exit(1); // ToDo
  }

  board_cell* c = &w.cell(cur.x, cur.y);
  w.set_character(c, u);
  w.set_attribute(c, cur.attribute);

  for (int i = 1; i < charwidth; i++) {
    w.set_character(c + i, is_wide_extension);
    w.set_attribute(c + i, 0);
  }

  cur.x += charwidth;
}


int main() {
  contra::board w;
  w.resize(10, 10);

  w.cur.attribute = 196 | is_fg_color_set;
  for (int i = 0; i < 26; i++)
    put_char(w, 'a' + i);
  w.cur.attribute = 202 | is_fg_color_set;
  for (int i = 0; i < 26; i++)
    put_char(w, 'a' + i);
  w.cur.attribute = 220 | is_fg_color_set;
  for (int i = 0; i < 26; i++)
    put_char(w, 'a' + i);
  w.cur.attribute = 154 | is_fg_color_set;
  for (int i = 0; i < 26; i++)
    put_char(w, 'a' + i);
  w.cur.attribute = 63 | is_fg_color_set;
  for (int i = 0; i < 26; i++)
    put_char(w, 'a' + i);

  termcap_sgr_type sgrcap;
  sgrcap.initialize();

  tty_observer target(stdout, &sgrcap);
  target.output_content(w);

  return 0;
}
