// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef CONTRA_TTY_OBSERVER_H
#define CONTRA_TTY_OBSERVER_H
#include "board.h"
#include <cstdio>

namespace contra {

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

  public:
    void initialize();
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
      termcap_sgrflag1 const& sgrflag);

    void update_sgrflag2(
      aflags_t aflagsNew, aflags_t aflagsOld,
      termcap_sgrflag2 const& sgrflag);

    void update_ideogram_decoration(
      xflags_t xflagsNew, xflags_t xflagsOld,
      termcap_sgrideogram const& cap);

    void update_sgrcolor(
      int colorSpaceNew, color_t colorNew,
      int colorSpaceOld, color_t colorOld,
      termcap_sgrcolor const& sgrcolor);

    void apply_attr(board const& w, attribute_t newAttr);

  public:
    // test implementation
    // ToDo: output encoding
    void print_screen(board& w) {
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
}

#endif
