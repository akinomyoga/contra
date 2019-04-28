// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef CONTRA_ANSI_TERM_HPP
#define CONTRA_ANSI_TERM_HPP
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstdarg>
#include <iterator>
#include <algorithm>
#include <vector>
#include "../sequence.h"
#include "line.hpp"
#include "enc.c2w.hpp"
#include "enc.utf8.hpp"

namespace contra {
namespace ansi {

  typedef std::uint32_t mode_t;

  enum mode_spec {
    ansi_mode   = 0, // CSI Ps h
    dec_mode    = 1, // CSI ? Ps h
    contra_mode = 2, // private mode

    accessor_flag = 0x10000,

#include "../../out/gen/term.mode_def.hpp"
  };

  struct tty_state;
  class term_t;
  typedef std::uint32_t csi_single_param_t;

  void do_pld(term_t& term);
  void do_plu(term_t& term);
  void do_decsc(term_t& term);
  void do_decrc(term_t& term);
  void do_spa(term_t& term);
  void do_epa(term_t& term);
  void do_ssa(term_t& term);
  void do_esa(term_t& term);
  void do_adm3_fs(term_t& term);
  void do_adm3_gs(term_t& term);
  void do_adm3_rs(term_t& term);
  void do_adm3_us(term_t& term);
  void do_deckpam(term_t& term);
  void do_deckpnm(term_t& term);
  void do_s7c1t(term_t& term);
  void do_s8c1t(term_t& term);
  void do_decaln(term_t& term);

  void do_altscreen(term_t& term, bool value);
  int do_rqm_deccolm(term_t& term);
  void do_sm_deccolm(term_t& term, bool value);
  int do_rqm_decscnm(term_t& term);
  void do_sm_decscnm(term_t& term, bool value);
  int do_rqm_decawm(term_t& term);
  void do_sm_decawm(term_t& term, bool value);

  void do_ed(term_t& term, csi_single_param_t param);
  void do_vertical_scroll(term_t& term, curpos_t shift, bool dcsm);
  bool do_decrqss(term_t& term, char32_t const* param, std::size_t len);

  struct tty_state {
    term_t* m_term;

    curpos_t page_home  {-1};
    curpos_t page_limit {-1};
    curpos_t line_home  {-1};
    curpos_t line_limit {-1};

    line_attr_t lflags {0};

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
    color_t m_default_fg = 0;
    color_t m_default_bg = 0;

    tty_state(term_t* term): m_term(term) {
      this->clear();
    }

    void clear() {
      this->initialize_mode();
      this->title = U"";
      this->screen_title = U"";
      this->m_decsc_cur.set_x(-1);
      this->m_scosc_x = -1;

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

  public:
    bool get_mode(mode_t modeSpec) const {
      std::uint32_t const index = modeSpec;
      if (!(index & accessor_flag)) {
        unsigned const field = index >> 5;
        std::uint32_t const bit = 1 << (index & 0x1F);
        mwg_assert(field < sizeof(m_mode_flags) / sizeof(m_mode_flags[0]), "invalid modeSpec");;

        std::uint32_t const& flags = m_mode_flags[index >> 5];
        return (flags & bit) != 0;
      } else {
        // Note: 現在は暫定的にハードコーディングしているが、
        //   将来的にはunordered_map か何かで登録できる様にする。
        //   もしくは何らかの表からコードを自動生成する様にする。
        switch (index) {
        case mode_wystcurm1: // Mode 32 (Set Cursor Mode (Wyse))
          return !get_mode(mode_attCursorBlink);
        case mode_wystcurm2: // Mode 33 WYSTCURM (Wyse Set Cursor Mode)
          return !get_mode(resource_cursorBlink);
        case mode_wyulcurm: // Mode 34 WYULCURM (Wyse Underline Cursor Mode)
          return m_cursor_shape > 0;
        case mode_altscreen: // Mode ?47
          return get_mode(mode_altscr);
        case mode_altscreen_clr: // Mode ?1047
          return get_mode(mode_altscr);
        case mode_decsc: // Mode ?1048
          return m_decsc_cur.x() >= 0;
        case mode_altscreen_cur: // Mode ?1049
          return get_mode(mode_altscr);
        case mode_deccolm:
          return 1 & do_rqm_deccolm(*m_term);
        case mode_decscnm:
          return 1 & do_rqm_decscnm(*m_term);
        case mode_decawm:
          return 1 & do_rqm_decawm(*m_term);
        default:
          return false;
        }
      }
    }

  public:
    void set_mode(mode_t modeSpec, bool value = true) {
      std::uint32_t const index = modeSpec;
      if (!(index & accessor_flag)) {
        unsigned const field = index >> 5;
        std::uint32_t const bit = 1 << (index & 0x1F);
        mwg_assert(field < sizeof(m_mode_flags) / sizeof(m_mode_flags[0]), "invalid modeSpec");;

        std::uint32_t& flags = m_mode_flags[index >> 5];
        if (value)
          flags |= bit;
        else
          flags &= ~bit;
      } else {
        // Note: 現在は暫定的にハードコーディングしているが、
        //   将来的にはunordered_map か何かで登録できる様にする。
        //   もしくは何らかの表からコードを自動生成する様にする。
        switch (index) {
        case mode_wystcurm1:
          set_mode(mode_attCursorBlink, !value);
          break;
        case mode_wystcurm2:
          set_mode(resource_cursorBlink, !value);
          break;
        case mode_wyulcurm:
          if (value) {
            if (m_cursor_shape <= 0) m_cursor_shape = 1;
          } else {
            if (m_cursor_shape > 0) m_cursor_shape = 0;
          }
          break;
        case mode_altscreen:
          do_altscreen(*m_term, value);
          break;
        case mode_altscreen_clr:
          if (get_mode(mode_altscr) != value) {
            if (value) do_ed(*m_term, 2);
            set_mode(mode_altscreen, value);
          }
          break;
        case mode_decsc:
          if (value)
            do_decsc(*m_term);
          else
            do_decrc(*m_term);
          break;
        case mode_altscreen_cur:
          if (get_mode(mode_altscr) != value) {
            set_mode(mode_altscreen, value);
            set_mode(mode_decsc, value);
          }
          break;
        case mode_deccolm:
          do_sm_deccolm(*m_term, value);
          break;
        case mode_decscnm:
          do_sm_decscnm(*m_term, value);
          break;
        case mode_decawm:
          do_sm_decawm(*m_term, value);
          break;
        default: ;
        }
      }
    }

    bool dcsm() const { return get_mode(mode_bdsm) || get_mode(mode_dcsm); }

    bool is_cursor_visible() const { return get_mode(mode_dectcem); }
    bool is_cursor_blinking() const {
      bool const st = get_mode(mode_attCursorBlink);
      bool const mn = get_mode(resource_cursorBlink);
      return get_mode(resource_cursorBlinkXOR) ? st != mn : st || mn;
    }
    int get_cursor_shape() const { return m_cursor_shape; }

  public:
    void do_bel() {
      std::fputc('\a', stdout);
    }

  public:
    std::size_t c2w(char32_t u) const {
      // ToDo: 文字幅設定
      return contra::encoding::c2w(u, contra::encoding::c2w_width_emacs);
    }
  };


  class term_t {
  private:
    board_t* m_board;
    tty_state m_state {this};

    contra::idevice* m_response_target = nullptr;
    std::vector<byte> m_response;

  public:
    void initialize_line(line_t& line) const {
      if (line.lflags() & line_attr_t::is_line_used) return;
      line.lflags() = line_attr_t::is_line_used | m_state.lflags;
      line.home() = m_state.line_home;
      line.limit() = m_state.line_limit;
    }

  public:
    term_t(contra::ansi::board_t& board): m_board(&board) {
      this->m_response.reserve(32);
    }

    void set_response_target(contra::idevice& dev) { this->m_response_target = &dev; }

    void respond() {
      if (m_response_target)
        m_response_target->write(reinterpret_cast<char const*>(&m_response[0]), m_response.size());
      m_response.clear();
    }
    void response_put(byte value) {
      if (0x80 <= value && value < 0xA0 && m_state.get_mode(mode_s7c1t)) {
        m_response.push_back(ascii_esc);
        m_response.push_back(value - 0x40);
      } else {
        m_response.push_back(value);
      }
    }
    void response_number(unsigned value) {
      if (value >= 10)
        response_number(value / 10);
      m_response.push_back((byte) (ascii_0 + value % 10));
    }

  public:
    board_t& board() { return *m_board; }
    board_t const& board() const { return *m_board; }
    tty_state& state() {return this->m_state;}
    tty_state const& state() const {return this->m_state;}

    curpos_t tmargin() const {
      curpos_t const b = m_state.dec_tmargin;
      return 0 <= b && b < m_board->m_height ? b : 0;
    }
    curpos_t bmargin() const {
      curpos_t const e = m_state.dec_bmargin;
      return 0 < e && e <= m_board->m_height ? e : m_board->m_height;
    }
    curpos_t lmargin() const {
      if (!m_state.get_mode(mode_declrmm)) return 0;
      curpos_t const l = m_state.dec_lmargin;
      return 0 <= l && l < m_board->m_width ? l : 0;
    }
    curpos_t rmargin() const {
      if (!m_state.get_mode(mode_declrmm)) return m_board->m_width;
      curpos_t const r = m_state.dec_rmargin;
      return 0 < r && r <= m_board->m_width ? r : m_board->m_width;
    }

    curpos_t implicit_sph() const {
      curpos_t sph = 0;
      if (m_state.page_home > 0) sph = m_state.page_home;
      if (m_state.dec_tmargin > sph) sph = m_state.dec_tmargin;
      return sph;
    }
    curpos_t implicit_spl() const {
      curpos_t spl = m_board->m_height - 1;
      if (m_state.page_limit >= 0 && m_state.page_limit < spl)
        spl = m_state.page_limit;
      if (m_state.dec_bmargin > 0 && m_state.dec_bmargin - 1 < spl)
        spl = m_state.dec_bmargin - 1;
      return spl;
    }
    curpos_t implicit_slh(line_t const& line) const {
      curpos_t const width = m_board->m_width;
      curpos_t home = line.home();
      home = home < 0 ? 0 : std::min(home, width - 1);
      if (m_state.get_mode(mode_declrmm))
        home = std::max(home, std::min(m_state.dec_lmargin, width - 1));
      return home;
    }
    curpos_t implicit_sll(line_t const& line) const {
      curpos_t const width = m_board->m_width;
      curpos_t limit = line.limit();
      limit = limit < 0 ? width - 1 : std::min(limit, width - 1);
      if (m_state.get_mode(mode_declrmm) && m_state.dec_rmargin > 0)
        limit = std::min(limit, m_state.dec_rmargin - 1);
      return limit;
    }

    attribute_t fill_attr() const {
      if (m_state.get_mode(mode_bce)) {
        attribute_t const& src = m_board->cur.attribute;
        attribute_t ret;
        ret.set_bg(src.bg_space(), src.bg_color());
        return ret;
      } else {
        return {};
      }
    }

  public:
    void insert_graph(char32_t u) {
      // ToDo: 新しい行に移る時に line_limit, line_home を初期化する
      board_t& b = this->board();
      tty_state& s = this->state();
      line_t& line = b.line();
      initialize_line(line);

      int const char_width = m_state.c2w(u); // ToDo 文字幅
      if (char_width <= 0) {
        std::exit(1); // ToDo: control chars, etc.
      }

      bool const simd = m_state.get_mode(mode_simd);
      curpos_t const dir = simd ? -1 : 1;

      curpos_t slh = implicit_slh(line);
      curpos_t sll = implicit_sll(line);
      if (b.cur.x() < slh)
        slh = 0;
      else if (b.cur.x() > (b.cur.xenl() ? sll + 1 : sll))
        sll = b.m_width - 1;
      if (simd) std::swap(slh, sll);

      // (行頭より後でかつ) 行末に文字が入らない時は折り返し
      curpos_t const x0 = b.cur.x();
      curpos_t const x1 = x0 + dir * (char_width - 1);
      if ((x1 - sll) * dir > 0 && (x0 - slh) * dir > 0) {
        if (!m_state.get_mode(mode_decawm)) return;
        do_nel();
        // Note: do_nel() 後に b.cur.x() in [slh, sll]
        //   は保証されていると思って良いので、
        //   現在位置が範囲外の時の sll の補正は不要。
        sll = simd ? implicit_slh(b.line()) : implicit_sll(b.line());
      }

      curpos_t const xL = simd ? b.cur.x() - (char_width - 1) : b.cur.x();

      cell_t cell;
      cell.character = u;
      cell.attribute = b.cur.attribute;
      cell.width = char_width;
      b.line().write_cells(xL, &cell, 1, 1, dir);

      curpos_t x = b.cur.x() + dir * char_width;
      bool xenl = false;
      if ((x - sll) * dir >= 1) {
        if (m_state.get_mode(mode_decawm)) {
          // 行末を超えた時は折り返し
          // Note: xenl かつ x == sll + 1 ならば の位置にいる事を許容する。
          if (!simd && x == sll + 1 && s.get_mode(mode_xenl)) {
            xenl = true;
          } else {
            do_nel();
            return;
          }
        } else {
          x = sll;
        }
      }
      b.cur.set_x(x, xenl);
    }

    void insert_marker(std::uint32_t marker) {
      bool const simd = m_state.get_mode(mode_simd);
      curpos_t const dir = simd ? -1 : 1;

      cell_t cell;
      cell.character = marker | character_t::flag_marker;
      cell.attribute = m_board->cur.attribute;
      cell.width = 0;
      initialize_line(m_board->line());
      m_board->line().write_cells(m_board->cur.x(), &cell, 1, 1, dir);
    }

    void insert_char(char32_t u) {
      u &= character_t::unicode_mask;

      // Unicode bidi formatting characters
      if ((U'\u202A' <= u && u <= U'\u202E') ||
        (U'\u2066' <= u && u <= U'\u2069'))
        return insert_marker(u);

      return insert_graph(u);
    }

  public:
    void do_bel() {
      m_state.do_bel();
    }
    void do_bs() {
      // Note: mode_xenl, mode_decawm, mode_xtBSBackLine が絡んで来た時の振る舞いは適当である。
      board_t& b = this->board();
      line_t const& line = b.line();
      if (m_state.get_mode(mode_simd)) {
        bool const cap_xenl = m_state.get_mode(mode_xenl);
        curpos_t sll = this->implicit_sll(line);
        curpos_t x = b.cur.x(), y = b.cur.y();
        bool xenl = false;
        if (b.cur.xenl() || (!cap_xenl && (x == sll || x == b.m_width - 1))) {
          if (m_state.get_mode(mode_decawm) && m_state.get_mode(mode_xtBSBackLine) && y > 0) {
            y--;
            x = this->implicit_slh(b.line(y));

            // Note: xenl から前行に移った時は更に次の文字への移動を試みる。
            if (b.cur.xenl())
              sll = this->implicit_sll(b.line(y));
            else
              goto update;
          } else {
            // Note: 行末に居て前の行に戻らない時は何もしない。
            return;
          }
        }

        mwg_assert(x < b.m_width);
        if (x != sll && x != b.m_width - 1) {
          x++;
        } else if (cap_xenl) {
          xenl = true;
          x++;
        }

      update:
        b.cur.set(x, y, xenl);
      } else {
        // Note: vttest によると xenl は補正するのが正しいらしい。
        //   しかし xterm で手動で動作を確認すると sll+1 に居ても補正ない様だ。
        //   一方で RLogin では手動で確認すると補正される。
        //   xterm で補正が起こる条件があるのだろうがよく分からないので、
        //   contra では RLogin と同様に常に補正を行う事にする。
        b.cur.adjust_xenl();

        curpos_t x = b.cur.x(), y = b.cur.y();
        curpos_t const slh = this->implicit_slh(line);
        if (x != slh && x > 0) {
          x--;
        } else if (m_state.get_mode(mode_decawm) && m_state.get_mode(mode_xtBSBackLine) && y > 0) {
          y--;
          x = this->implicit_sll(b.line(y));
        }
        b.cur.set(x, y);
      }
    }
    void do_ht() {
      cursor_t& cur = m_board->cur;

      // ToDo: tab stop の管理
      int const tabwidth = 8;
      curpos_t const sll = implicit_sll(m_board->line());
      curpos_t const xdst = std::min((cur.x() + tabwidth) / tabwidth * tabwidth, sll);
      cur.set_x(xdst);
    }

    void do_generic_ff(int delta, bool toAppendNewLine, bool toAdjustXAsPresentationPosition) {
      if (!delta) return;

      board_t& b = this->board();
      b.cur.adjust_xenl();
      curpos_t x = b.cur.x();
      if (toAdjustXAsPresentationPosition)
        x = b.to_presentation_position(b.cur.y(), x);

      curpos_t const beg = implicit_sph();
      curpos_t const end = implicit_spl() + 1;
      curpos_t const y = b.cur.y();
      if (delta > 0) {
        if (y < end) {
          if (y + delta < end) {
            b.cur.set_y(b.cur.y() + delta);
          } else {
            b.cur.set_y(end - 1);
            if (toAppendNewLine) {
              delta -= b.cur.y() - y;
              do_vertical_scroll(*this, -delta, true);
            }
          }
        }
      } else {
        if (y >= beg) {
          if (y + delta >= beg) {
            b.cur.set_y(b.cur.y() + delta);
          } else {
            b.cur.set_y(beg);
            if (toAppendNewLine) {
              delta += y - b.cur.y();
              do_vertical_scroll(*this, -delta, true);
            }
          }
        }
      }

      if (toAdjustXAsPresentationPosition)
        x = b.to_data_position(b.cur.y(), x);
      b.cur.set_x(x);
    }
    void do_ind() {
      do_generic_ff(1, true, !m_state.dcsm());
    }
    void do_ri() {
      do_generic_ff(-1, true, !m_state.dcsm());
    }
    void do_lf() {
      bool const toCallCR = m_state.get_mode(mode_lnm);
      do_generic_ff(1, true, !toCallCR && !m_state.dcsm());
      if (toCallCR) do_cr();
    }
    void do_ff() {
      bool const toCallCR = m_state.ff_affected_by_lnm && m_state.get_mode(mode_lnm);

      if (m_state.ff_clearing_screen) {
        board_t& b = this->board();
        if (m_state.ff_using_page_home) {
          b.cur.adjust_xenl();
          curpos_t x = b.cur.x();
          curpos_t y = b.cur.y();
          if (!toCallCR && !m_state.get_mode(mode_bdsm))
            x = b.to_presentation_position(y, x);

          b.clear_screen();
          y = std::max(implicit_sph(), 0);

          if (!toCallCR && !m_state.get_mode(mode_bdsm))
            x = b.to_data_position(y, x);

          b.cur.set(x, y);
        } else
          b.clear_screen();
      } else {
        do_generic_ff(1, true, !toCallCR && !m_state.get_mode(mode_bdsm));
      }

      if (toCallCR) do_cr();
    }
    void do_vt() {
      bool const toCallCR = m_state.vt_affected_by_lnm && m_state.get_mode(mode_lnm);
      do_generic_ff(1, m_state.vt_appending_newline, !toCallCR && !m_state.get_mode(mode_bdsm));
      if (toCallCR) do_cr();
    }
    void do_cr() {
      board_t& b = this->board();
      line_t& line = b.line();
      initialize_line(line);

      curpos_t x;
      if (m_state.get_mode(mode_simd)) {
        x = this->implicit_sll(line);
      } else {
        x = this->implicit_slh(line);
      }

      if (!m_state.dcsm())
        x = b.to_data_position(b.cur.y(), x);

      b.cur.set_x(x);
    }
    void do_nel() {
      // Note: 新しい行の SLH/SLL に移動したいので、
      // LF を実行した後に CR を実行する必要がある。
      do_lf();
      do_cr();
    }

  private:
    typedef sequence_decoder<term_t> decoder_type;
    decoder_type m_seqdecoder {this, &this->m_state.m_sequence_decoder_config};
    friend decoder_type;

    void print_unrecognized_sequence(sequence const& seq) {
      const char* name = "sequence";
      switch (seq.type()) {
      case ascii_csi:
        name = "control sequence";
        break;
      case ascii_esc:
        name = "escape sequence";
        break;
      case ascii_sos:
        name = "character string";
        break;
      case ascii_pm:
      case ascii_apc:
      case ascii_dcs:
      case ascii_osc:
        name = "command string";
        break;
      }

      std::fprintf(stderr, "unrecognized %s: ", name);
      seq.print(stderr);
      std::fputc('\n', stderr);
    }
    void process_invalid_sequence(sequence const& seq) {
      // ToDo: 何処かにログ出力
      print_unrecognized_sequence(seq);
    }

    void process_escape_sequence(sequence const& seq) {
      if (seq.parameter_size() == 0) {
        switch (seq.final()) {
        case ascii_7: do_decsc(*this); return;
        case ascii_8: do_decrc(*this); return;
        case ascii_equals : do_deckpam(*this); return;
        case ascii_greater: do_deckpnm(*this); return;
        }
      } else if (seq.parameter_size() == 1) {
        switch (seq.parameter()[0]) {
        case ascii_sp:
          switch (seq.final()) {
          case ascii_F: do_s7c1t(*this); return;
          case ascii_G: do_s8c1t(*this); return;
          }
          break;
        case ascii_number:
          switch (seq.final()) {
          case ascii_8: do_decaln(*this); return;
          }
          break;
        }
      }
      print_unrecognized_sequence(seq);
    }

    void process_control_sequence(sequence const& seq);

    void process_command_string(sequence const& seq) {
      auto _check2 = [&seq] (char32_t a, char32_t b) {
        return seq.parameter_size() >= 2 && seq.parameter()[0] == a && seq.parameter()[1] == b;
      };

      if (seq.type() == ascii_osc) {
        if (_check2(ascii_0, ascii_semicolon)) {
          m_state.title = std::u32string(seq.parameter() + 2, (std::size_t) seq.parameter_size() - 2);
          return;
        }
      }
      if (seq.type() == ascii_dcs) {
        if (_check2(ascii_dollar, ascii_q)) {
          if (do_decrqss(*this, seq.parameter() + 2, seq.parameter_size() - 2)) return;
        }
      }
      print_unrecognized_sequence(seq);
    }
    void process_character_string(sequence const& seq) {
      if (seq.type() == ascii_k) {
        m_state.screen_title = std::u32string(seq.parameter(), (std::size_t) seq.parameter_size());
        return;
      } else
        print_unrecognized_sequence(seq);
    }

    void process_control_character(char32_t uchar) {
      switch (uchar) {
      case ascii_bel: do_bel(); break;
      case ascii_bs : do_bs();  break;
      case ascii_ht : do_ht();  break;
      case ascii_lf : do_lf();  break;
      case ascii_ff : do_ff();  break;
      case ascii_vt : do_vt();  break;
      case ascii_cr : do_cr();  break;

      case ascii_fs: do_adm3_fs(*this); break;
      case ascii_gs: do_adm3_gs(*this); break;
      case ascii_rs: do_adm3_rs(*this); break;
      case ascii_us: do_adm3_us(*this); break;

      case ascii_ind: do_ind(); break;
      case ascii_nel: do_nel(); break;
      case ascii_ri : do_ri() ; break;

      case ascii_pld: do_pld(*this); break;
      case ascii_plu: do_plu(*this); break;
      case ascii_spa: do_spa(*this); break;
      case ascii_epa: do_epa(*this); break;
      case ascii_ssa: do_ssa(*this); break;
      case ascii_esa: do_esa(*this); break;
      }
    }

  public:
    std::uint64_t printt_state = 0;
    std::vector<char32_t> printt_buff;
    void write(const char* data, std::size_t const size) {
      printt_buff.resize(size);
      char32_t* const q0 = &printt_buff[0];
      char32_t* q1 = q0;
      contra::encoding::utf8_decode(data, data + size, q1, q0 + size, printt_state);
      for (char32_t const* q = q0; q < q1; q++)
        m_seqdecoder.process_char(*q);
    }
    void printt(const char* text) {
      write(text, std::strlen(text));
    }
    void putc(char32_t uchar) {
      m_seqdecoder.process_char(uchar);
    }
  };

}
}

#endif
