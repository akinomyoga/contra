// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef CONTRA_ANSI_OBSERVER_TTY_HPP
#define CONTRA_ANSI_OBSERVER_TTY_HPP
#include <cstdio>
#include <vector>
#include "line.hpp"
#include "term.hpp"

namespace contra {
namespace ansi {

  enum termcap_constants {
    color_space_default_bit     = 1 << attribute_t::color_space_default    ,
    color_space_transparent_bit = 1 << attribute_t::color_space_transparent,
    color_space_rgb_bit         = 1 << attribute_t::color_space_rgb        ,
    color_space_cmy_bit         = 1 << attribute_t::color_space_cmy        ,
    color_space_cmyk_bit        = 1 << attribute_t::color_space_cmyk       ,
    color_space_indexed_bit     = 1 << attribute_t::color_space_indexed    ,
  };

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
    struct {
      unsigned      sgr         ; //!< SGR number of ISO 8613-6 color specification
      unsigned char color_spaces; //!< supported color specs specified by bits
      char          separater   ; //!< the separator character of ISO 8613-6 SGR arguments
      bool          exact       ; //!< the separator character of ISO 8613-6 SGR arguments
      unsigned      max_index   ; //!< the number of colors available through 38:5:*
    } iso8613;

    // 1,22 / 5,25 で明るさを切り替える方式
    unsigned high_intensity_on ;
    unsigned high_intensity_off;
  };

  struct termcap_sgr_type {
    typedef attribute_t _at;
    // aflags
    termcap_sgrflag2 cap_bold         {_at::is_bold_set     , 1, _at::is_faint_set           ,  2, 22};
    termcap_sgrflag2 cap_italic       {_at::is_italic_set   , 3, _at::is_fraktur_set         , 20, 23};
    termcap_sgrflag2 cap_underline    {_at::is_underline_set, 4, _at::is_double_underline_set, 21, 24};
    termcap_sgrflag2 cap_blink        {_at::is_blink_set    , 5, _at::is_rapid_blink_set     ,  6, 25};
    termcap_sgrflag1 cap_inverse      {_at::is_inverse_set  , 7, 27};
    termcap_sgrflag1 cap_invisible    {_at::is_invisible_set, 8, 28};
    termcap_sgrflag1 cap_strike       {_at::is_strike_set   , 9, 29};

    // xflags
    termcap_sgrflag2 cap_framed       {_at::is_frame_set       , 51, _at::is_circle_set, 52, 54};

    termcap_sgrflag1 cap_proportional {_at::is_proportional_set, 26, 50};
    termcap_sgrflag1 cap_overline     {_at::is_overline_set    , 53, 55};

    termcap_sgrideogram cap_ideogram;

    // colors
    termcap_sgrcolor cap_fg {
      _at::is_fg_color_set, 30, 39,  90,
      {38, color_space_indexed_bit | color_space_rgb_bit, ascii_semicolon, false, 255},
      0, 0
    };
    termcap_sgrcolor cap_bg {
      _at::is_bg_color_set, 40, 49, 100,
      {48, color_space_indexed_bit | color_space_rgb_bit, ascii_semicolon, false, 255},
      0, 0
    };

    aflags_t aflagsNotResettable {0};
    xflags_t xflagsNotResettable {0};

  public:
    void initialize();
  };

  //-----------------------------------------------------------------------------

  struct tty_observer {
    term_t* term;
    std::FILE* file;
    termcap_sgr_type const* sgrcap;
    curpos_t x = 0, y = 0;

    tty_observer(term_t& term, std::FILE* file, termcap_sgr_type* sgrcap):
      term(&term), file(file), sgrcap(sgrcap)
    {
      m_attr.clear();
    }

    void put_u32(char32_t c) const { contra::encoding::put_u8(c, file); }
    void put(char c) const { std::fputc(c, file); }
    void put_str(const char32_t* s) const {
      while (*s) put(*s++);
    }
    void put_unsigned(unsigned value) const {
      if (value >= 10)
        put_unsigned(value/10);
      put((byte) (ascii_0 + value % 10));
    }

  private:
    void sgr_clear() const {
      put(ascii_esc);
      put(ascii_left_bracket);
      put(ascii_m);
    }

    void sgr_put(unsigned value) {
      if (this->sgr_isOpen) {
        put(ascii_semicolon);
      } else {
        this->sgr_isOpen = true;
        put(ascii_esc);
        put(ascii_left_bracket);
      }

      if (value) put_unsigned(value);
    }

    attribute_t m_attr;
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

    void apply_attr(attribute_t newAttr);

    void put_skip(curpos_t& wskip) {
      if (m_attr.is_default() && wskip <= 4) {
        while (wskip--) put(' ');
      } else {
        put(ascii_esc);
        put(ascii_left_bracket);
        put_unsigned(wskip);
        put(ascii_C);
      }
      wskip = 0;
    }

  public:
    // test implementation
    // ToDo: output encoding
    void print_screen(board_t const& w) {
      for (curpos_t y = 0; y < w.m_height; y++) {
        line_t const& line = w.m_lines[y];
        curpos_t wskip = 0;
        curpos_t const ncell = (curpos_t) line.cells().size();
        for (curpos_t x = 0; x < ncell; x++) {
          cell_t const& cell = line.cells()[x];
          if (cell.character.is_wide_extension()) continue;
          if (cell.character.is_marker()) continue;
          if (cell.character.value == ascii_nul) {
            wskip += cell.width;
            continue;
          }

          if (wskip > 0) put_skip(wskip);
          apply_attr(cell.attribute);
          put(cell.character.value);
        }

        put('\n');
      }
      apply_attr(attribute_t {});
    }

    void update() {
      //std::fprintf(file, "\x1b[?25l");
      if (y > 0) std::fprintf(file, "\x1b[%dA", y);
      if (x > 0) std::fprintf(file, "\x1b[%dD", x);
      std::vector<cell_t> buff;

      board_t const& w = term->board();
      for (curpos_t y = 0; y < w.m_height; y++) {
        line_t const& line = w.m_lines[y];
        line.get_cells_in_presentation(buff, w.line_r2l(line));

        curpos_t wskip = 0;
        curpos_t x = 0;
        std::fprintf(file, "\x1b[%dX", w.m_width);
        for (auto const& cell : buff) {
          std::uint32_t const code = cell.character.value;
          if (code == ascii_nul) {
            wskip++;
          } else {
            if (wskip > 0) put_skip(wskip);
            apply_attr(cell.attribute);
            put_u32(code);
          }
          x += cell.width;
        }
        if (wskip > 0) put_skip(wskip);
        if (x > 0) std::fprintf(file, "\x1b[%dD", x);

        put('\n');
      }
      apply_attr(attribute_t {});

      x = w.cur.x;
      y = w.cur.y;
      if (x > 0) std::fprintf(file, "\x1b[%dC", x);
      std::fprintf(file, "\x1b[%dA", w.m_height - y);
      //std::fprintf(file, "\x1b[?25h");
      std::fflush(file);
    }
  };

}
}
#endif
