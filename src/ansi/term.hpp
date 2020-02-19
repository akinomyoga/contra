// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef CONTRA_ANSI_TERM_HPP
#define CONTRA_ANSI_TERM_HPP
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstdarg>
#include <climits>
#include <iterator>
#include <algorithm>
#include <vector>
#include <sstream>
#include "../sequence.hpp"
#include "line.hpp"
#include "../enc.c2w.hpp"
#include "../enc.utf8.hpp"

namespace contra {
namespace limit {
  constexpr ansi::curpos_t maximal_scroll_buffer_size = 1000000;
}
namespace ansi {

  typedef std::uint32_t mode_t;

  enum mode_spec {
    ansi_mode   = 0, // CSI Ps h
    dec_mode    = 1, // CSI ? Ps h
    contra_mode = 2, // private mode

    mode_index_mask    = 0x0FFFF,
    mode_flag_accessor = 0x10000,
    mode_flag_protect  = 0x20000,
    mode_flag_guarded  = 0x40000,
    mode_flag_const    = 0x80000,

#include "../../out/gen/term.mode_def.hpp"
  };
  constexpr mode_t mode_index(mode_t mode_spec) {
    return mode_spec & mode_index_mask;
  }

  struct tstate_t;
  class term_t;

  void do_insert_graph(term_t& term, char32_t u);
  void do_insert_graphs(term_t& term, char32_t const* beg, char32_t const* end);

  enum mouse_mode_flags {
    mouse_report_mask   = 0xFF,
    mouse_report_down   = 0x01,
    mouse_report_up     = 0x02,
    mouse_report_select = 0x04,
    mouse_report_drag   = 0x08,
    mouse_report_move   = 0x10,

    mouse_report_XtermX10Mouse            = mouse_report_down,
    mouse_report_UrxvtExtModeMouse        = mouse_report_down | mouse_report_up,
    mouse_report_XtermVt200Mouse          = mouse_report_down | mouse_report_select,
    mouse_report_XtermVt200HighlightMouse = mouse_report_UrxvtExtModeMouse | mouse_report_drag,
    mouse_report_XtermBtnEventMouse       = mouse_report_UrxvtExtModeMouse | mouse_report_drag | mouse_report_move,

    mouse_sequence_mask  = 0xFF00,
    mouse_sequence_byte  = 0x0100,
    mouse_sequence_utf8  = 0x0200,
    mouse_sequence_sgr   = 0x0400,
    mouse_sequence_urxvt = 0x0800,
  };

  enum funckey_mode_flags {
    // bit 0-7 は Function-key mode
    funckey_pc       = 0, // xterm
    funckey_terminfo = 1, // xterm Mode ?1050
    funckey_sun      = 2, // xterm Mode ?1051
    funckey_hp       = 3, // xterm Mode ?1052
    funckey_sco      = 4, // xterm Mode ?1053
    funckey_x11r6    = 5, // xterm Mode ?1060
    funckey_vt220    = 6, // xterm Mode ?1061
    funckey_contra   = 7, // contra Mode ?9950
    funckey_mode_mask = 0xFF,

    // bit 8-31 は 4 bit ずつ 6 種類の modifyKeys 値
    funckey_modifyKeyboard_shift     =  8, // unsupported
    funckey_modifyCursorKeys_shift   = 12,
    funckey_modifyFunctionKeys_shift = 16,
    funckey_modifyKeypadKeys_shift   = 20,
    funckey_modifyOtherKeys_shift    = 24,
    funckey_modifyStringKeys_shift   = 28, // unsupported
    funckey_modifyKeys_mask = 0xFFFFFF00,

    funckey_mlevel_mask = 0xF,
    funckey_mlevel_shift_function = 0xE,
    funckey_mlevel_none           = 0xF,
  };

  /*?lwiki
   * @class class term_scoll_buffer_t;
   * 0 個以上 m_capacity 個以下の行を保持する。
   * 最初の要素は data[m_rotate] にある。
   *
   * @remarks
   * `data.size() < m_capacity` の時は常に `m_rotate = 0` である。
   * `data.size() == m_capacity` の時は `m_rotate` は有限の値を取りうる。
   * `data.size() > m_capacity` になる事はない。
   */
  class term_scroll_buffer_t {
    typedef term_scroll_buffer_t self;
    typedef line_t value_type;

    std::vector<value_type> data;
    std::size_t m_rotate = 0;
    std::size_t m_capacity;

  public:
    term_scroll_buffer_t(std::size_t capacity = 0): m_capacity(capacity) {}

  public:
    std::size_t capacity() const {
      return this->m_capacity;
    }
    void set_capacity(std::size_t value) {
      value = std::min<std::size_t>(value, limit::maximal_scroll_buffer_size);
      if (data.size() < value) {
        if (m_rotate > 0) {
          std::rotate(data.begin(), data.begin() + m_rotate, data.end());
          m_rotate = 0;
        }
      } else if (data.size() > value) {
        std::size_t const new_begin = (m_rotate - value + m_capacity) % m_capacity;
        if (new_begin)
          contra::util::partial_rotate(data.begin(), data.begin() + new_begin, data.end(), value);
        data.resize(value);
      }
      this->m_capacity = value;
    }

    void transfer(value_type&& line) {
      if (m_capacity == 0) return;
      if (data.size() < m_capacity) {
        data.emplace_back(std::move(line));
      } else {
        auto check = data[m_rotate].cells().capacity();
        data[m_rotate].transfer_from(line);
        mwg_check(check == line.cells().capacity(),"%zu %zu", check, line.cells().capacity());
        m_rotate = (m_rotate + 1) % m_capacity;
      }
    }

    value_type& operator[](std::size_t index) {
      return data[(m_rotate + index) % data.size()];
    }
    value_type const& operator[](std::size_t index) const {
      return data[(m_rotate + index) % data.size()];
    }

    std::size_t size() const { return data.size(); }

    typedef contra::util::indexer_iterator<value_type, self, std::size_t> iterator;
    typedef contra::util::indexer_iterator<const value_type, const self, std::size_t> const_iterator;
    iterator begin() { return {this, (std::size_t) 0}; }
    iterator end() { return {this, data.size()}; }
    const_iterator begin() const { return {this, (std::size_t) 0}; }
    const_iterator end() const { return {this, data.size()}; }
  };

  struct board_t {
    contra::util::ring_buffer<line_t> m_lines;
    cursor_t cur;

  private:
    curpos_t m_width;
    curpos_t m_height;
    coord_t m_xunit;
    coord_t m_yunit;
  public:
    // ToDo: altscreen で直接弄っている。直接触らなくても良い様に設計を見直したい。
    std::uint32_t m_line_count = 0;
  private:
    presentation_direction_t m_presentation_direction { presentation_direction_default };

  public:
    curpos_t width() const { return this->m_width; }
    curpos_t height() const { return this->m_height; }
    coord_t xunit() const { return this->m_xunit; }
    coord_t yunit() const { return this->m_yunit; }
    presentation_direction_t presentation_direction() const { return m_presentation_direction; }
    void set_presentation_direction(presentation_direction_t value) {
      m_presentation_direction = value;
    }

  public:
    board_t(curpos_t width, curpos_t height, curpos_t xunit = 7, curpos_t yunit = 14) {
      m_width = limit::term_col.clamp(width);
      m_height = limit::term_row.clamp(height);
      m_xunit = limit::term_xunit.clamp(xunit);
      m_yunit = limit::term_yunit.clamp(yunit);
      this->cur.set(0, 0);
      this->m_lines.resize(m_height);
      for (line_t& line : m_lines)
        line.set_id(m_line_count++);
    }
    board_t(): board_t(80, 24, 7, 14) {}

  public:
    void reset_size(curpos_t width, curpos_t height) {
      width = limit::term_col.clamp(width);
      height = limit::term_row.clamp(height);
      if (width == this->m_width && height == this->m_height) return;
      if (this->cur.x() >= width) cur.set_x(width - 1);
      if (this->cur.y() >= height) cur.set_y(height - 1);

      m_lines.resize(height);
      for (curpos_t y = this->m_height; y < height; y++)
        m_lines[y].set_id(m_line_count++);

      if (this->m_width > width) {
        for (auto& line : m_lines)
          line.truncate(width, false, true);
      }
      this->m_width = width;
      this->m_height = height;
    }
    void reset_size(curpos_t width, curpos_t height, coord_t xunit, coord_t yunit) {
      this->reset_size(width, height);
      m_xunit = limit::term_xunit.clamp(xunit);
      m_yunit = limit::term_yunit.clamp(yunit);
    }

  public:
    curpos_t x() const { return cur.x(); }
    curpos_t y() const { return cur.y(); }

    line_t& line() { return m_lines[cur.y()]; }
    line_t const& line() const { return m_lines[cur.y()]; }
    line_t& line(curpos_t y) { return m_lines[y]; }
    line_t const& line(curpos_t y) const { return m_lines[y]; }

    bool line_r2l(line_t const& line) const { return line.is_r2l(m_presentation_direction); }
    bool line_r2l(curpos_t y) const { return line_r2l(m_lines[y]); }
    bool line_r2l() const { return line_r2l(cur.y()); }

    curpos_t to_data_position(line_t const& line, curpos_t x) const {
      return line.to_data_position(x, m_width, m_presentation_direction);
    }
    curpos_t to_presentation_position(line_t const& line, curpos_t x) const {
      return line.to_presentation_position(x, m_width, m_presentation_direction);
    }
    curpos_t to_data_position(curpos_t y, curpos_t x) const {
      return to_data_position(m_lines[y], x);
    }
    curpos_t to_presentation_position(curpos_t y, curpos_t x) const {
      return to_presentation_position(m_lines[y], x);
    }
    curpos_t convert_position(position_type from, position_type to, line_t const& line, curpos_t x, int edge_type) const {
      return line.convert_position(from, to, x, edge_type, m_width, m_presentation_direction);
    }

  private:
    void transfer_lines(curpos_t y1, curpos_t y2, term_scroll_buffer_t& scroll_buffer) {
      for (curpos_t y = y1; y < y2; y++)
        scroll_buffer.transfer(std::move(m_lines[y]));
    }
    void initialize_lines(curpos_t y1, curpos_t y2, attribute_t const& fill_attr) {
      for (curpos_t y = y1; y < y2; y++) {
        m_lines[y].clear(m_width, fill_attr);
        m_lines[y].set_id(m_line_count++);
      }
    }

  public:
    /// @fn void shift_lines(curpos_t y1, curpos_t y2, curpos_t count, attribute_t const& fill_attr);
    /// 指定した範囲の行を前方または後方に移動します。
    /// @param[in] y1 範囲の開始位置を指定します。
    /// @param[in] y2 範囲の終端位置を指定します。
    /// @param[in] count 移動量を指定します。正の値を指定した時、
    ///   下方向にシフトします。負の値を指定した時、上方向にシフトします。
    /// @param[in] fill_attr 新しく現れた行に適用する描画属性を指定します。
    void shift_lines(curpos_t y1, curpos_t y2, curpos_t count, attribute_t const& fill_attr, term_scroll_buffer_t* scroll_buffer = nullptr) {
      y1 = contra::clamp(y1, 0, m_height);
      y2 = contra::clamp(y2, 0, m_height);
      if (y1 >= y2 || count == 0) return;
      if (y1 == 0 && y2 == m_height) {
        if (count < 0) {
          count = -count;
          if (count > m_height) count = m_height;
          if (scroll_buffer)
            transfer_lines(0, count, *scroll_buffer);
          initialize_lines(0, count, fill_attr);
          m_lines.rotate(count);
        } else if (count > 0) {
          if (count > m_height) count = m_height;
          m_lines.rotate(m_height - count);
          initialize_lines(0, count, fill_attr);
        }
        return;
      }

      if (std::abs(count) >= y2 - y1) {
        if (count < 0 && scroll_buffer)
          transfer_lines(y1, y2, *scroll_buffer);
        initialize_lines(y1, y2, fill_attr);
      } else if (count > 0) {
        auto const it1 = m_lines.begin() + y1;
        auto const it2 = m_lines.begin() + y2;
        std::move_backward(it1, it2 - count, it2);
        initialize_lines(y1, y1 + count, fill_attr);
      } else {
        count = -count;
        auto const it1 = m_lines.begin() + y1;
        auto const it2 = m_lines.begin() + y2;
        if (scroll_buffer)
          transfer_lines(y1, y1 + count, *scroll_buffer);
        std::move(it1 + count, it2, it1);
        initialize_lines(y2 - count, y2, fill_attr);
      }
    }
    void clear_screen() {
      for (auto& line : m_lines) {
        line.clear();
        line.set_id(m_line_count++);
      }
    }

    // FF 等によって "次ページに移動" した時に使う。
    void clear_screen(term_scroll_buffer_t& scroll_buffer) {
      for (auto& line : m_lines) {
        scroll_buffer.transfer(std::move(line));
        line.clear();
        line.set_id(m_line_count++);
      }
    }

  public:
    void debug_print(std::FILE* file) {
      for (auto const& line : m_lines) {
        if (line.lflags() & line_attr_t::is_line_used) {
          for (auto const& cell : line.cells()) {
            if (cell.character.is_wide_extension()) continue;
            char32_t c = (char32_t) (cell.character.value & unicode_mask);
            if (c == 0) c = '~';
            contra::encoding::put_u8(c, file);
          }
          std::putc('\n', file);
        }
      }
    }
  };

  struct tstate_t {
    term_t* m_term;

    curpos_t page_home  {-1};
    curpos_t page_limit {-1};
    curpos_t line_home  {-1};
    curpos_t line_limit {-1};

    line_attr_t lflags {0};

    color_t m_rgba256[256];

    // OSC(0), scrTDS
    std::u32string title; // xterm title
    std::u32string screen_title; // GNU screen title

    // DECSC, SCOSC
    cursor_t m_decsc_cur;
    bool m_decsc_decawm;
    bool m_decsc_decom;
    curpos_t m_scosc_x;
    curpos_t m_scosc_y;
    bool m_scosc_xenl;

    // Alternate Screen Buffer
    board_t altscreen;

    // DECSTBM, DECSLRM
    curpos_t dec_tmargin = -1;
    curpos_t dec_bmargin = -1;
    curpos_t dec_lmargin = -1;
    curpos_t dec_rmargin = -1;

    // conformance level (61-65)
    int m_decscl = 65;

    // 画面サイズ
    curpos_t cfg_decscpp_enabled = true;
    curpos_t cfg_decscpp_min = 20;
    curpos_t cfg_decscpp_max = 400;

    // 既定の前景色・背景色
    byte m_default_fg_space = 0;
    byte m_default_bg_space = 0;
    color_t m_default_fg_color = 0;
    color_t m_default_bg_color = 0;

    std::uint32_t mouse_mode = 0;
    std::uint32_t m_funckey_flags = 0x22222200;

    tstate_t(term_t* term): m_term(term) {
      this->clear();
    }

    void clear() {
      this->initialize_mode();
      this->initialize_palette();

      this->title = U"";
      this->screen_title = U"";
      this->m_decsc_cur.set_x(-1);
      this->m_scosc_x = -1;

      this->lflags = 0;
      this->mouse_mode = 0;
      this->m_funckey_flags = 0x22222200;
      this->m_cursor_shape = 1;

      this->page_home = -1;
      this->page_limit = -1;
      this->line_home = -1;
      this->line_limit = -1;
      this->clear_margin();

      this->m_decscl = 65;
    }
    void clear_margin() {
      this->dec_tmargin = -1;
      this->dec_bmargin = -1;
      this->dec_lmargin = -1;
      this->dec_rmargin = -1;
    }
    void initialize_palette() {
      color_t A = 0xFF000000;

      // rosaterm palette (rgba format)
      int index = 0;
      m_rgba256[index++] = contra::dict::rgba(0x00, 0x00, 0x00);
      m_rgba256[index++] = contra::dict::rgba(0x80, 0x00, 0x00);
      m_rgba256[index++] = contra::dict::rgba(0x00, 0x80, 0x00);
      m_rgba256[index++] = contra::dict::rgba(0x80, 0x80, 0x00);
      m_rgba256[index++] = contra::dict::rgba(0x00, 0x00, 0x80);
      m_rgba256[index++] = contra::dict::rgba(0x80, 0x00, 0x80);
      m_rgba256[index++] = contra::dict::rgba(0x00, 0x80, 0x80);
      m_rgba256[index++] = contra::dict::rgba(0xC0, 0xC0, 0xC0);
      m_rgba256[index++] = contra::dict::rgba(0x80, 0x80, 0x80);
      m_rgba256[index++] = contra::dict::rgba(0xFF, 0x00, 0x00);
      m_rgba256[index++] = contra::dict::rgba(0x32, 0xCD, 0x32);
      m_rgba256[index++] = contra::dict::rgba(0xFF, 0xD7, 0x00);
      m_rgba256[index++] = contra::dict::rgba(0x00, 0x00, 0xFF);
      m_rgba256[index++] = contra::dict::rgba(0xFF, 0x00, 0xFF);
      m_rgba256[index++] = contra::dict::rgba(0x40, 0xE0, 0xD0);
      m_rgba256[index++] = contra::dict::rgba(0xFF, 0xFF, 0xFF);

      // 6x6x6 cube
      //   xterm は i == 0 ? 0 : i * 40 + 55 という式を使っている。
      //   RLogin は恐らく i * 51 という式を使った時と同じに見える。
      auto _intensity = [] (int i) { return i == 0 ? 0 : i * 40 + 55; };
      //auto _intensity = [] (int i) { return i * 51; };
      for (int r = 0; r < 6; r++) {
        color_t const R = A | _intensity(r);
        for (int g = 0; g < 6; g++) {
          color_t const RG = R | _intensity(g) << 8;
          for (int b = 0; b < 6; b++) {
            color_t const RGB = RG | _intensity(b) << 16;
            m_rgba256[index++] = RGB;
          }
        }
      }

      // 24 grayscale
      for (int k = 0; k < 24; k++) {
        color_t const K = k * 10 + 8;
        m_rgba256[index++] = A | K << 16 | K << 8 | K;
      }
    }

    sequence_decoder_config m_sequence_decoder_config;

    bool vt_affected_by_lnm    { true  };
    bool vt_appending_newline  { true  };
    bool vt_using_line_tabstop { false }; // ToDo: not yet supported

    bool ff_affected_by_lnm { true  };
    bool ff_clearing_screen { false };
    bool ff_using_page_home { false };

    /// @var cursor_shape
    ///   0 の時ブロックカーソル、1 の時下線、-1 の時縦線。
    ///   2 以上の時カーソルの下線の高さを百分率で表現。
    int m_cursor_shape;

  private:
    std::uint32_t m_mode_flags[2];

    void initialize_mode();

  private:
    int rqm_mode_with_accessor(mode_t modeSpec) const;
  public:
    bool get_mode(mode_t modeSpec) const {
      if (!(modeSpec & mode_flag_accessor)) {
        std::uint32_t const index = mode_index(modeSpec);
        mwg_assert(index < CHAR_BIT * sizeof m_mode_flags,
          "modeSpec %05x overflow (m_mode_flags capacity 0x%x)",
          modeSpec, CHAR_BIT * sizeof m_mode_flags);

        std::uint32_t const bit = 1 << (index & 0x1F);
        std::uint32_t const& flags = m_mode_flags[index >> 5];
        return (flags & bit) != 0;
      } else {
        return rqm_mode_with_accessor(modeSpec) & 1;
      }
    }
    int rqm_mode(mode_t modeSpec) const {
      if (modeSpec & mode_flag_guarded) return 0;
      if (!(modeSpec & mode_flag_accessor)) {
        std::uint32_t const index = mode_index(modeSpec);
        std::uint32_t const bit = 1 << (index & 0x1F);
        mwg_assert(index < CHAR_BIT * sizeof m_mode_flags,
          "modeSpec %05x overflow (m_mode_flags capacity 0x%x)",
          modeSpec, CHAR_BIT * sizeof m_mode_flags);

        std::uint32_t const& flags = m_mode_flags[index >> 5];
        int result = (flags & bit) != 0 ? 1 : 2;
        if (modeSpec & mode_flag_const) result += 2;
        return result;
      } else {
        int result = rqm_mode_with_accessor(modeSpec);
        if ((modeSpec & mode_flag_const) && (result == 1 || result == 2)) result += 2;
        return result;
      }
    }

  private:
    void set_mode_with_accessor(mode_t modeSpec, bool value);
  public:
    void set_mode(mode_t modeSpec, bool value = true) {
      if (!(modeSpec & mode_flag_accessor)) {
        std::uint32_t const index = mode_index(modeSpec);
        mwg_assert(index < CHAR_BIT * sizeof m_mode_flags,
          "modeSpec %05x overflow (m_mode_flags capacity 0x%x)",
          modeSpec, CHAR_BIT * sizeof m_mode_flags);

        std::uint32_t const bit = 1 << (index & 0x1F);
        std::uint32_t& flags = m_mode_flags[index >> 5];
        if (value)
          flags |= bit;
        else
          flags &= ~bit;
      } else {
        set_mode_with_accessor(modeSpec, value);
      }
    }
    void sm_mode(mode_t modeSpec, bool value) {
      if (modeSpec & mode_flag_protect) return;
      set_mode(modeSpec, value);
    }

    bool dcsm() const { return get_mode(mode_bdsm) || get_mode(mode_dcsm); }

    bool is_cursor_visible() const { return get_mode(mode_dectcem); }
    bool is_cursor_blinking() const {
      bool const st = get_mode(mode_Att610Blink);
      bool const mn = get_mode(mode_XtermCursorBlinkOps);
      return get_mode(mode_XtermXorCursorBlinks) ? st != mn : st || mn;
    }
    int get_cursor_shape() const { return m_cursor_shape; }

  public:
    void do_bel() {
      //std::fputc('\a', stdout);
    }

  public:
    std::size_t c2w(char32_t u) const {
      // ToDo: 文字幅設定
      return contra::encoding::c2w(u, contra::encoding::c2w_width_emacs);
    }
  };

  class term_t: public contra::idevice {
  private:
    tstate_t m_state {this};
    board_t m_board;
  public:
    void reset_size(curpos_t width, curpos_t height) {
      m_board.reset_size(width, height);
    }
    void reset_size(curpos_t width, curpos_t height, coord_t xunit, coord_t yunit) {
      m_board.reset_size(width, height, xunit, yunit);
    }

  public: // todo: make private
    term_scroll_buffer_t m_scroll_buffer;
  public:
    term_scroll_buffer_t const& scroll_buffer() const {
      return this->m_scroll_buffer;
    }
    void set_scroll_capacity(std::size_t value) {
      this->m_scroll_buffer.set_capacity(value);
    }

  private:
    contra::idevice* m_send_target = nullptr;

    void report_error(const char* message) {
      std::fprintf(stderr, "%s", message);
    }
  public:
    void initialize_line(line_t& line) const {
      if (line.lflags() & line_attr_t::is_line_used) return;
      line.lflags() = line_attr_t::is_line_used | m_state.lflags;
      line.home() = m_state.line_home;
      line.limit() = m_state.line_limit;
    }

  public:
    // do_insert_graphs() で使う為だけの変数
    std::vector<cell_t> m_buffer;

  public:
    term_t(curpos_t width, curpos_t height, coord_t xunit = 7, coord_t yunit = 13):
      m_board(width, height, xunit, yunit) {}

  public:
    curpos_t width() const { return this->m_board.width(); }
    curpos_t height() const { return this->m_board.height(); }
    line_t& line(curpos_t y) { return this->m_board.m_lines[y]; }
    line_t const& line(curpos_t y) const { return this->m_board.m_lines[y]; }
    cursor_t& cursor() { return this->m_board.cur; }
    cursor_t const& cursor() const { return this->m_board.cur; }

  public:
    bool clear_selection() {
      bool dirty = false;
      for (auto& line: m_board.m_lines)
        dirty |= line.clear_selection();
      for (auto& line: m_scroll_buffer)
        dirty |= line.clear_selection();
      return true;
    }

  public:
    board_t& board() { return this->m_board; }
    board_t const& board() const { return this->m_board; }
    tstate_t& state() { return this->m_state; }
    tstate_t const& state() const { return this->m_state; }

    curpos_t tmargin() const {
      curpos_t const b = m_state.dec_tmargin;
      return 0 <= b && b < m_board.height() ? b : 0;
    }
    curpos_t bmargin() const {
      curpos_t const e = m_state.dec_bmargin;
      return 0 < e && e <= m_board.height() ? e : m_board.height();
    }
    curpos_t lmargin() const {
      if (!m_state.get_mode(mode_declrmm)) return 0;
      curpos_t const l = m_state.dec_lmargin;
      return 0 <= l && l < m_board.width() ? l : 0;
    }
    curpos_t rmargin() const {
      if (!m_state.get_mode(mode_declrmm)) return m_board.width();
      curpos_t const r = m_state.dec_rmargin;
      return 0 < r && r <= m_board.width() ? r : m_board.width();
    }

    curpos_t implicit_sph() const {
      curpos_t sph = 0;
      if (m_state.page_home > 0) sph = m_state.page_home;
      if (m_state.dec_tmargin > sph) sph = m_state.dec_tmargin;
      return sph;
    }
    curpos_t implicit_spl() const {
      curpos_t spl = m_board.height() - 1;
      if (m_state.page_limit >= 0 && m_state.page_limit < spl)
        spl = m_state.page_limit;
      if (m_state.dec_bmargin > 0 && m_state.dec_bmargin - 1 < spl)
        spl = m_state.dec_bmargin - 1;
      return spl;
    }
    curpos_t implicit_slh(line_t const& line) const {
      curpos_t const width = m_board.width();
      curpos_t home = line.home();
      home = home < 0 ? 0 : std::min(home, width - 1);
      if (m_state.get_mode(mode_declrmm))
        home = std::max(home, std::min(m_state.dec_lmargin, width - 1));
      return home;
    }
    curpos_t implicit_sll(line_t const& line) const {
      curpos_t const width = m_board.width();
      curpos_t limit = line.limit();
      limit = limit < 0 ? width - 1 : std::min(limit, width - 1);
      if (m_state.get_mode(mode_declrmm) && m_state.dec_rmargin > 0)
        limit = std::min(limit, m_state.dec_rmargin - 1);
      return limit;
    }
    curpos_t implicit_sll() const {
      return implicit_sll(m_board.line(m_board.cur.y()));
    }
    curpos_t implicit_slh() const {
      return implicit_slh(m_board.line(m_board.cur.y()));
    }

    attribute_t fill_attr() const {
      if (m_state.get_mode(mode_bce)) {
        attribute_t const& src = m_board.cur.attribute;
        attribute_t ret;
        ret.set_bg(src.bg_space(), src.bg_color());
        return ret;
      } else {
        return {};
      }
    }

  public:
    void insert_marker(std::uint32_t marker) {
      if (m_board.line().cells().size() >= contra::limit::maximal_cells_per_line) {
        this->report_error("insert_marker: Exceeded the maximal number of cells per line.\n");
        return;
      }

      bool const simd = m_state.get_mode(mode_simd);
      curpos_t const dir = simd ? -1 : 1;

      cell_t cell;
      cell.character = marker | charflag_marker;
      cell.attribute = m_board.cur.attribute;
      cell.width = 0;
      initialize_line(m_board.line());
      m_board.line().write_cells(m_board.cur.x(), &cell, 1, 1, dir);
    }

    static constexpr bool is_marker(char32_t u) {
      // Unicode bidi formatting characters
      return (U'\u202A' <= u && u <= U'\u202E') ||
        (U'\u2066' <= u && u <= U'\u2069');
    }

    void process_char(char32_t u) {
      if (is_marker(u))
        return insert_marker(u);

      do_insert_graph(*this, u);

#ifndef NDEBUG
      board_t& b = board();
      mwg_assert(b.cur.is_sane(b.width()),
        "cur: {x=%d, xenl=%d, width=%d} after Insert U+%04X",
        b.cur.x(), b.cur.xenl(), b.width(), u);
#endif
    }
    void process_chars(char32_t const* beg, char32_t const* end) {
      while (beg < end) {
        if (!is_marker(*beg)) {
          char32_t const* graph_beg = beg;
          do beg++; while (beg != end && !is_marker(*beg));
          char32_t const* graph_end = beg;
          do_insert_graphs(*this, graph_beg, graph_end);
        } else
          insert_marker(*beg++);

#ifndef NDEBUG
        board_t& b = board();
        mwg_assert(b.cur.is_sane(b.width()),
          "cur: {x=%d, xenl=%d, width=%d} after InsertLength=#%zu",
          b.cur.x(), b.cur.xenl(), b.width(), end - beg);
#endif
      }
    }

  public:
    void do_bel() {
      m_state.do_bel();
    }

  private:
    typedef sequence_decoder<term_t> decoder_type;
    decoder_type m_seqdecoder {this, &this->m_state.m_sequence_decoder_config};
    friend decoder_type;

    void print_unrecognized_sequence(sequence const& seq);

    void process_invalid_sequence(sequence const& seq) {
      // ToDo: 何処かにログ出力
      print_unrecognized_sequence(seq);
    }

    void process_escape_sequence(sequence const& seq);
    void process_control_sequence(sequence const& seq);
    void process_command_string(sequence const& seq);

    void process_character_string(sequence const& seq) {
      if (seq.type() == ascii_k) {
        m_state.screen_title = std::u32string(seq.parameter(), (std::size_t) seq.parameter_size());
        return;
      } else
        print_unrecognized_sequence(seq);
    }

    void process_control_character(char32_t uchar);

  public:
    csi_parameters w_csiparams;

  public:
    std::uint64_t w_printt_state = 0;
    std::vector<char32_t> w_printt_buff;
    void write(const char* data, std::size_t const size) {
      w_printt_buff.resize(size);
      char32_t* const q0 = &w_printt_buff[0];
      char32_t* q1 = q0;
      contra::encoding::utf8_decode(data, data + size, q1, q0 + size, w_printt_state);
      m_seqdecoder.decode(q0, q1);
    }
    void printt(const char* text) {
      write(text, std::strlen(text));
    }
    void putc(char32_t uchar) {
      m_seqdecoder.decode_char(uchar);
    }

  private:
    virtual void dev_write(char const* data, std::size_t size) override {
      this->write(data, size);
    }

  public:
    void set_input_device(contra::idevice& dev) { this->m_send_target = &dev; }
    contra::idevice& input_device() const { return *this->m_send_target; }

  private:
    std::vector<byte> input_buffer;
  public:
    void input_flush(bool local_echo = false) {
      if (local_echo)
        this->write(reinterpret_cast<char const*>(&input_buffer[0]), input_buffer.size());
      if (m_send_target)
        m_send_target->dev_write(reinterpret_cast<char const*>(&input_buffer[0]), input_buffer.size());
      input_buffer.clear();
    }
    void input_byte(byte value) {
      input_buffer.push_back(value);
    }
    void input_bytes(byte const* beg, byte const* end) {
      input_buffer.insert(input_buffer.end(), beg, end);
    }
    void input_bytes(byte const* beg, std::size_t count) {
      input_bytes(beg, beg + count);
    }
    void input_c1(std::uint32_t code) {
      if (m_state.get_mode(ansi::mode_s7c1t)) {
        input_buffer.push_back(ascii_esc);
        input_buffer.push_back(code - 0x40);
      } else {
        contra::encoding::put_u8(code, input_buffer);
      }
    }
    void input_byte_maybe_c1(byte code) {
      if (0x80 <= code && code < 0xA0)
        input_c1(code);
      else
        input_byte(code);
    }
    void input_uchar_maybe_c1(char32_t u) {
      if (0x80 <= u && u < 0xA0)
        input_c1(u);
      else
        input_uchar(u);
    }
    void input_uchar(char32_t u) {
      contra::encoding::put_u8(u, input_buffer);
    }
    void input_unsigned(std::uint32_t value) {
      if (value >= 10) input_unsigned(value / 10);
      input_buffer.push_back(ascii_0 + value % 10);
    }
    void input_modifier(key_t mod) {
      input_unsigned(1 + ((mod & _modifier_mask) >> _modifier_shft));
    }

  private:
    curpos_t m_mouse_prev_x = -1, m_mouse_prev_y = -1;
  public:
    bool input_key(key_t key);
    bool input_mouse(key_t key, coord_t px, coord_t py, curpos_t x, curpos_t y);
    bool input_paste(std::u32string const& data);
  };

  class term_view_t {
  private:
    term_t* m_term = nullptr;
  public:
    term_view_t() {}
    term_view_t(term_t* term): m_term(term) {}

  private:
    curpos_t m_scroll_amount = 0;
    curpos_t m_x, m_y;
    bool m_xenl;
    byte m_fg_space, m_bg_space;
    color_t m_fg_color, m_bg_color;
  public:
    void set_term(term_t* term) { this->m_term = term; }
    bool scroll(curpos_t delta) {
      curpos_t new_value = contra::clamp(m_scroll_amount - delta, 0, m_term->scroll_buffer().size());
      bool const dirty = new_value != m_scroll_amount;
      m_scroll_amount = new_value;
      return dirty;
    }
    void update() {
      m_scroll_amount = std::min(m_scroll_amount, (curpos_t) m_term->scroll_buffer().size());
      m_x = m_term->cursor().x();
      m_y = m_term->cursor().y() + m_scroll_amount;
      m_xenl = m_term->cursor().xenl();

      auto const& s = m_term->state();
      m_fg_space = s.m_default_fg_space;
      m_bg_space = s.m_default_bg_space;
      m_fg_color = s.m_default_fg_color;
      m_bg_color = s.m_default_bg_color;
    }

  public:
    curpos_t height() const { return m_term->height(); }
    curpos_t width() const { return m_term->width(); }
    curpos_t scroll_amount() const {
      return this->m_scroll_amount;
    }
    presentation_direction_t presentation_direction() const {
      return m_term->board().presentation_direction();
    }
    curpos_t x() const { return m_x; }
    curpos_t y() const { return m_y; }
    bool xenl() const { return m_xenl; }
    curpos_t client_x() const {
      auto const& b = m_term->board();
      return b.convert_position(position_data, position_client, b.line(m_y), m_x, 0);
    }
    curpos_t client_y() const { return m_y; }
    bool cur_r2l() const { return m_term->board().line_r2l(m_y); }

    bool is_cursor_visible() const {
      return m_term->state().is_cursor_visible() &&
        0 <= m_y && m_y < height();
    }
    bool is_cursor_blinking() const {
      return m_term->state().is_cursor_blinking();
    }
    int cursor_shape() const {
      return m_term->state().get_cursor_shape();
    }

    // Note: インターフェイスを小さくする為に将来的には廃止したい。
    tstate_t const& state() const { return m_term->state(); }

  public:
    byte fg_space() const { return m_fg_space; }
    byte bg_space() const { return m_bg_space; }
    color_t fg_color() const { return m_fg_color; }
    color_t bg_color() const { return m_bg_color; }

  public:
    line_t const& lline(curpos_t y) const {
      if (y >= 0) {
        mwg_assert(y < (curpos_t) m_term->board().m_lines.size());
        return m_term->board().m_lines[y];
      } else {
        auto const& scroll_buffer = m_term->scroll_buffer();
        return scroll_buffer[scroll_buffer.size() + y];
      }
    }
    line_t& lline(curpos_t y) {
      return const_cast<line_t&>(const_cast<term_view_t const*>(this)->lline(y));
    }
    curpos_t logical_ybeg() const {
      return -(curpos_t) m_term->scroll_buffer().size();
    }
    curpos_t logical_yend() const {
      return m_term->height();
    }

    line_t const& line(curpos_t y) const {
      return this->lline(y - m_scroll_amount);
    }
    line_t& line(curpos_t y) {
      return this->lline(y - m_scroll_amount);
    }
    void order_cells_in(std::vector<cell_t>& buffer, position_type to, line_t const& line) const {
      bool const r2l = m_term->board().line_r2l(line);
      line.order_cells_in(buffer, to, this->width(), r2l);
    }
  };

}
}
#endif
