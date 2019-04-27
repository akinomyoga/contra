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
  private:
    term_t* term;
    std::FILE* file;
    termcap_sgr_type const* sgrcap;

    struct line_buffer_t {
      std::uint32_t id = (std::uint32_t) -1;
      std::uint32_t version = 0;
      std::vector<cell_t> content;
      int delta;
    };
    std::vector<line_buffer_t> screen_buffer;

  public:
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
          put_u32(cell.character.value);
        }

        put('\n');
      }
      apply_attr(attribute_t {});
    }

  private:
    void put_csiseq_pn1(unsigned param, char ch) const {
      put('\x1b');
      put('[');
      if (param != 1) put_unsigned(param);
      put(ch);
    }
  private:
    curpos_t x = 0, y = 0;
    void move_to_line(curpos_t newy) {
      curpos_t const delta = newy - y;
      if (delta > 0) {
        put_csiseq_pn1(delta, 'B');
      } else if (delta < 0) {
        put_csiseq_pn1(-delta, 'A');
      }
      y = newy;
    }
    void move_to_column(curpos_t newx) {
      curpos_t const delta = newx - x;
      if (delta > 0) {
        put_csiseq_pn1(delta, 'C');
      } else if (delta < 0) {
        put_csiseq_pn1(-delta, 'D');
      }
      x = newx;
    }
    void move_to(curpos_t newx, curpos_t newy) {
      move_to_line(newy);
      move_to_column(newx);
    }
    void put_dl(curpos_t delta) const {
      if (delta > 0) put_csiseq_pn1(delta, 'M');
    }
    void put_il(curpos_t delta) const {
      if (delta > 0) put_csiseq_pn1(delta, 'L');
    }
    void put_ech(curpos_t delta) const {
      if (delta > 0) put_csiseq_pn1(delta, 'X');
    }

    void trace_line_scroll() {
      board_t const& w = term->board();

      curpos_t const height = w.m_height;
      curpos_t j = 0;
      for (curpos_t i = 0; i < height; i++) {
        screen_buffer[i].delta = height;
        for (curpos_t k = j; k < height; k++) {
          if (w.line(k).id() == screen_buffer[i].id) {
            screen_buffer[i].delta = k - i;
            j = k;
            break;
          }
        }
      }

      // DL による行削除
      int previous_shift = 0;
      int total_shift = 0;
      for (curpos_t i = 0; i < height; i++) {
        if (screen_buffer[i].delta == height) continue;
        int const new_shift = screen_buffer[i].delta - previous_shift;
        if (new_shift < 0) {
          move_to_line(i + new_shift + total_shift);
          put_dl(-new_shift);
          total_shift += new_shift;
        }
        previous_shift = screen_buffer[i].delta;
      }
      if (previous_shift > 0) {
        move_to_line(height - previous_shift + total_shift);
        put_dl(previous_shift);
      }

      // IL による行追加
      previous_shift = 0;
      for (curpos_t i = 0; i < height; i++) {
        if (screen_buffer[i].delta == height) continue;
        int const new_shift = screen_buffer[i].delta - previous_shift;
        if (new_shift > 0) {
          move_to_line(i + previous_shift);
          put_il(new_shift);
        }
        previous_shift = screen_buffer[i].delta;
      }

      // screen_buffer の更新
      previous_shift = 0;
      for (curpos_t i = 0; i < height; i++) {
        if (screen_buffer[i].delta == height) {
          screen_buffer[i].delta = previous_shift;
          continue;
        }
        int const new_shift = screen_buffer[i].delta - previous_shift;
        for (curpos_t j = i + new_shift; j < i; j++)
          screen_buffer[j].delta = height;
        previous_shift = screen_buffer[i].delta;
      }
      if (previous_shift > 0) {
        for (curpos_t j = height - previous_shift; j < height; j++)
          screen_buffer[j].delta = height;
      }
      for (curpos_t i = 0; i < height; i++) {
        curpos_t const i0 = i;
        for (curpos_t j = i0; j <= i; j++) {
          if (screen_buffer[j].delta == height) continue;
          if (j + screen_buffer[j].delta >= i)
            i = j + screen_buffer[j].delta;
        }
        for (curpos_t j = i; i0 <= j; j--) {
          int const delta = screen_buffer[j].delta;
          if (delta == height || delta == 0) continue;
          screen_buffer[j + delta] = std::move(screen_buffer[j]);
          screen_buffer[j].id = -1;
        }
      }
    }
    void erase_until_eol() {
      tty_state const& s = term->state();
      board_t const& w = term->board();
      if (x >= w.m_width) return;
      apply_attr(attribute_t {});
      if (s.m_default_fg_space || s.m_default_bg_space) {
        // ToDo: 実は ECH で SGR が適用される端末では put_ech で良い。
        for (; x < w.m_width; x++) put(' ');
      } else {
        put_ech(w.m_width - x);
      }
    }

  public:
    void update() {
      //std::fprintf(file, "\x1b[?25l");
      std::vector<cell_t> buff;

      board_t const& w = term->board();
      screen_buffer.resize(w.m_height);
      trace_line_scroll();
      move_to(0, 0);
      for (curpos_t y = 0; y < w.m_height; y++) {
        line_t const& line = w.m_lines[y];
        line_buffer_t& line_buffer = screen_buffer[y];
        if (line_buffer.id == line.id() &&
          line_buffer.version == line.version()) {
          put('\n');
          continue;
        }

        line.get_cells_in_presentation(buff, w.line_r2l(line));

        //for (std::size_t i = 0; i < buff.size8); i++

        curpos_t wskip = 0;
        x = 0;
        bool is_skipping = line_buffer.id == line.id();
        curpos_t x1 = 0;
        for (std::size_t i = 0; i < buff.size(); i++) {
          auto const& cell = buff[i];
          if (is_skipping) {
            if (i < line_buffer.content.size()) {
              cell_t& cell_buffer = line_buffer.content[i];
              if (cell == cell_buffer) {
                x1 += cell.width;
                continue;
              }
            } else {
              if (cell.character == ascii_nul && cell.attribute.is_default()) {
                x1 += cell.width;
                continue;
              }
            }
            move_to_column(x1);
            is_skipping = false;
          }

          std::uint32_t const code = cell.character.value;
          if (cell.attribute.is_default() && code == ascii_nul) {
            wskip++;
          } else {
            if (wskip > 0) {
              apply_attr(attribute_t {});
              for (curpos_t c = wskip; c--; ) put(' ');
            }
            apply_attr(cell.attribute);
            put_u32(code);
            x += wskip + cell.width;
            wskip = 0;
          }
        }
        if (is_skipping) move_to_column(x1);
        erase_until_eol();
        move_to_column(0);

        line_buffer.version = line.version();
        line_buffer.id = line.id();
        line_buffer.content.swap(buff);
        put('\n');
      }
      apply_attr(attribute_t {});

      y = w.m_height;
      move_to(w.cur.x, w.cur.y);
      //std::fprintf(file, "\x1b[?25h");
      std::fflush(file);
    }
  };

}
}
#endif
