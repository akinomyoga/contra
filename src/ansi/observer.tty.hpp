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
    bool termcap_bce = false;

    struct line_buffer_t {
      std::uint32_t id = (std::uint32_t) -1;
      std::uint32_t version = 0;
      std::vector<cell_t> content;
      int delta;
    };
    std::vector<line_buffer_t> screen_buffer;
    bool prev_decscnm = false;

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
    void put_ich(curpos_t count) const {
      if (count > 0) put_csiseq_pn1(count, '@');
    }
    void put_dch(curpos_t count) const {
      if (count > 0) put_csiseq_pn1(count, 'P');
    }

    /*?lwiki @var bool is_terminal_bottom;
     * 外側端末の一番下にいる時に `true` を設定する。
     * 一番下にいる時には一番下の行での IL を省略できる。
     * 一番下にいない時には DL の数と IL の数を合わせないと、
     * 下にある内容を破壊してしまう事になる。
     */
    static constexpr bool is_terminal_bottom = false;
    static constexpr bool is_terminal_fullwidth = true;

    /*?lwiki
     * @fn void trace_line_scroll();
     *   行の移動を追跡し、それに応じて IL, DL を用いたスクロールを実施します。
     *   同時に screen_buffer の内容も実施したスクロールに合わせて更新します。
     */
    void trace_line_scroll() {
      board_t const& b = term->board();

      curpos_t const height = b.m_height;
      curpos_t j = 0;
      for (curpos_t i = 0; i < height; i++) {
        screen_buffer[i].delta = height;
        for (curpos_t k = j; k < height; k++) {
          if (b.line(k).id() == screen_buffer[i].id) {
            screen_buffer[i].delta = k - i;
            j = k;
            break;
          }
        }
      }

      // DL による行削除
#ifndef NDEBUG
      int dbgD = 0, dbgI = 0, dbgC = 0;
#endif
      int previous_shift = 0;
      int total_shift = 0;
      for (curpos_t i = 0; i < height; i++) {
        if (screen_buffer[i].delta == height) continue;
        int const new_shift = screen_buffer[i].delta - previous_shift;
        if (new_shift < 0) {
          move_to_line(i + new_shift + total_shift);
          put_dl(-new_shift);
          total_shift += new_shift;
#ifndef NDEBUG
          dbgD -= new_shift;
#endif
        }
        previous_shift = screen_buffer[i].delta;
      }
      if (previous_shift > 0) {
        move_to_line(height - previous_shift + total_shift);
        put_dl(previous_shift);
#ifndef NDEBUG
        dbgD += previous_shift;
#endif
      }

      // IL による行追加
      previous_shift = 0;
      for (curpos_t i = 0; i < height; i++) {
        if (screen_buffer[i].delta == height) continue;
        int const new_shift = screen_buffer[i].delta - previous_shift;
        if (new_shift > 0) {
          move_to_line(i + previous_shift);
          put_il(new_shift);
#ifndef NDEBUG
          dbgI += new_shift;
#endif
        }
        previous_shift = screen_buffer[i].delta;
      }
      if (!is_terminal_bottom && previous_shift < 0) {
        move_to_line(height + previous_shift);
        put_il(-previous_shift);
#ifndef NDEBUG
        dbgI += -previous_shift;
#endif
      }

      // screen_buffer の更新
      previous_shift = 0;
      for (curpos_t i = 0; i < height; i++) {
        if (screen_buffer[i].delta == height) {
          screen_buffer[i].delta = previous_shift;
          continue;
        }
        int const new_shift = screen_buffer[i].delta - previous_shift;
        for (curpos_t j = i + new_shift; j < i; j++) {
          screen_buffer[j].delta = height;
          screen_buffer[j].content.clear();
#ifndef NDEBUG
          dbgC++;
#endif
        }
        previous_shift = screen_buffer[i].delta;
      }
      if (previous_shift > 0) {
        for (curpos_t j = height - previous_shift; j < height; j++) {
          screen_buffer[j].delta = height;
          screen_buffer[j].content.clear();
#ifndef NDEBUG
          dbgC++;
#endif
        }
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
          // j -> j + delta に移動して j にあった物は消去する
          std::swap(screen_buffer[j + delta], screen_buffer[j]);
          screen_buffer[j].id = -1;
          //screen_buffer[j].content.clear();
        }
      }
#ifndef NDEBUG
      mwg_assert(dbgD == dbgI && dbgI == dbgC,
        "total_delete=%d total_insert=%d clear=%d", dbgD, dbgI, dbgC);
#endif
    }
    void erase_until_eol() {
      board_t const& w = term->board();
      if (x >= w.m_width) return;

      attribute_t attr;
      apply_attr(attr);
      if (termcap_bce || attr.is_default()) {
        put_ech(w.m_width - x);
      } else {
        for (; x < w.m_width; x++) put(' ');
      }
    }

    void render_line(std::vector<cell_t> const& new_content, std::vector<cell_t> const& old_content) {
      // 更新の必要のある範囲を決定する

      // 一致する先頭部分の長さを求める。
      std::size_t i1 = 0;
      curpos_t x1 = 0;
      for (; i1 < new_content.size(); i1++) {
        cell_t const& cell = new_content[i1];
        if (i1 < old_content.size()) {
          if (cell != old_content[i1]) break;
        } else {
          if (!(cell.character == ascii_nul && cell.attribute.is_default())) break;
        }
        x1 += cell.width;
      }
      // Note: cluster_extension や marker に違いがある時は
      //   その前の有限幅の文字まで後退する。
      //   (出力先の端末がどの様に零幅文字を扱うのかに依存するが、)
      //   contra では古い marker を消す為には前の有限幅の文字を書く必要がある為。
      while (i1 && new_content[i1].width == 0) i1--;

      // 一致する末端部分のインデックスと長さを求める。
      auto _find_upper_bound_non_empty = [] (std::vector<cell_t> const& cells, std::size_t lower_bound) {
        std::size_t ret = cells.size();
        while (ret > lower_bound && cells[ret - 1].character == ascii_nul && cells[ret - 1].attribute.is_default()) ret--;
        return ret;
      };
      curpos_t w3 = 0;
      std::size_t i2 = _find_upper_bound_non_empty(new_content, i1);
      std::size_t j2 = _find_upper_bound_non_empty(old_content, i1);
      for (; j2 > i1 && i2 > i1; j2--, i2--) {
        cell_t const& cell = new_content[i2 - 1];
        if (new_content[i2 - 1] != old_content[j2 - 1]) break;
        w3 += cell.width;
      }
      // Note: cluster_extension や marker も本体の文字と共に再出力する。
      //   (出力先の端末がどの様に零幅文字を扱うのかに依存するが、)
      //   contra ではcluster_extension や marker は暗黙移動で潰される為。
      while (i2 < new_content.size() && new_content[i2].width == 0) i2++;

      // 間の部分の幅を求める。
      curpos_t new_w2 = 0, old_w2 = 0;
      for (std::size_t i = i1; i < i2; i++) new_w2 += new_content[i].width;
      for (std::size_t j = i1; j < j2; j++) old_w2 += old_content[j].width;

      move_to_column(x1);
      if (new_w2 || old_w2) {
        if (w3) {
          if (new_w2 > old_w2)
            put_ich(new_w2 - old_w2);
          else if (new_w2 < old_w2)
            put_dch(old_w2 - new_w2);
        }

        for (std::size_t i = i1; i < i2; i++) {
          cell_t const& cell = new_content[i];
          std::uint32_t code = cell.character.value;
          if (code == ascii_nul) code = ascii_sp;
          apply_attr(cell.attribute);
          put_u32(code);
          x += cell.width;
        }
      }

      move_to_column(x + w3);
      erase_until_eol();
    }

    void apply_default_attribute(std::vector<cell_t>& content) {
      tty_state& s = term->state();
      if (!s.m_default_fg_space && !s.m_default_bg_space) return;
      for (auto& cell : content) {
        if (s.m_default_fg_space && cell.attribute.is_fg_default())
          cell.attribute.set_fg(s.m_default_fg, s.m_default_fg_space);
        if (s.m_default_bg_space && cell.attribute.is_bg_default())
          cell.attribute.set_bg(s.m_default_bg, s.m_default_bg_space);
      }
      board_t& b = term->board();
      if (content.size() < (std::size_t) b.m_width && (s.m_default_fg_space || s.m_default_bg_space)) {
        cell_t fill = ascii_nul;
        fill.attribute.set_fg(s.m_default_fg, s.m_default_fg_space);
        fill.attribute.set_bg(s.m_default_bg, s.m_default_bg_space);
        fill.width = 1;
        content.resize(b.m_width, fill);
      }
    }

  public:
    void update() {
      std::fprintf(file, "\x1b[?25l");
      std::vector<cell_t> buff;

      bool full_update = false;
      if (prev_decscnm != term->state().get_mode(mode_decscnm)) {
        full_update = true;
        screen_buffer.clear();
        prev_decscnm = !prev_decscnm;
      }

      board_t const& b = term->board();
      screen_buffer.resize(b.m_height);
      if (is_terminal_fullwidth)
        trace_line_scroll();
      move_to(0, 0);
      for (curpos_t y = 0; y < b.m_height; y++) {
        line_t const& line = b.m_lines[y];
        line_buffer_t& line_buffer = screen_buffer[y];
        if (full_update || line_buffer.id != line.id() || line_buffer.version != line.version()) {
          line.get_cells_in_presentation(buff, b.line_r2l(line));
          this->apply_default_attribute(buff);
          this->render_line(buff, line_buffer.content);

          line_buffer.version = line.version();
          line_buffer.id = line.id();
          line_buffer.content.swap(buff);
          move_to_column(0);
        }

        if (y + 1 == b.m_height) break;
        put('\n');
        this->y++;
      }
      apply_attr(attribute_t {});

      move_to(b.cur.x(), b.cur.y());
      if (term->state().get_mode(mode_dectcem))
        std::fprintf(file, "\x1b[?25h");
      std::fflush(file);
    }
  };

}
}
#endif
