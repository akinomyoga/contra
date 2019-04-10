// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef CONTRA_ANSI_TERM_HPP
#define CONTRA_ANSI_TERM_HPP
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <cstdlib>
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

  enum mode_spec_layout {
    mode_index_mask = 0xFFF00000u, mode_index_shift = 20,
    mode_type_mask  = 0x000F0000u, mode_type_shift  = 16,
    mode_param_mask = 0x0000FFFFu, mode_param_shift = 0,
  };

  constexpr mode_t construct_mode_spec(mode_t index, mode_t type, mode_t param) {
    return type << mode_type_shift
      | param << mode_param_shift
      | index << mode_index_shift;
  }

  enum mode_spec {
    ansi_mode   = 0, // CSI Ps h
    dec_mode    = 1, // CSI ? Ps h
    contra_mode = 2, // private mode

    mode_vem  = construct_mode_spec( 7, ansi_mode,  7),
    mode_dcsm = construct_mode_spec( 9, ansi_mode,  9),
    mode_hem  = construct_mode_spec(10, ansi_mode, 10),
    mode_lnm  = construct_mode_spec(20, ansi_mode, 20),
    mode_grcm = construct_mode_spec(21, ansi_mode, 21),
    mode_zdm  = construct_mode_spec(22, ansi_mode, 22),

    mode_simd     = construct_mode_spec(23, contra_mode, 9201),
    mode_xenl     = construct_mode_spec(24, contra_mode, 9202),
    /// @var mode_xenl_ech
    /// 行末にカーソルがある時に ECH, ICH, DCH は行の最後の文字に作用します。
    mode_xenl_ech = construct_mode_spec(25, contra_mode, 9203),
    /// @var mode_home_il
    /// ICH, IL, DL の後にカーソルを SPH で設定される行頭に移動します。
    mode_home_il  = construct_mode_spec(26, contra_mode, 9204),
  };

  struct tty_state {
    curpos_t page_home  {-1};
    curpos_t page_limit {-1};
    curpos_t line_home  {-1};
    curpos_t line_limit {-1};

    line_attr_t lflags {0};

    tty_state() {
      this->initialize_mode();
    }

    sequence_decoder_config m_sequence_decoder_config;

    bool vt_affected_by_lnm    {true};
    bool vt_appending_newline  {true};
    bool vt_using_line_tabstop {false}; // ToDo: not yet supported

    bool ff_affected_by_lnm {true};
    bool ff_clearing_screen {false};
    bool ff_using_page_home {false};

  private:
    std::uint32_t m_mode_flags[1];

    void initialize_mode() {
      std::fill(std::begin(m_mode_flags), std::end(m_mode_flags), 0);
      set_mode(mode_lnm);
      set_mode(mode_dcsm);
      set_mode(mode_grcm);
      set_mode(mode_zdm);
      set_mode(mode_xenl);
      set_mode(mode_xenl_ech);
    }

  public:
    bool get_mode(mode_t modeSpec) {
      int const index = (modeSpec & mode_index_mask) >> mode_index_shift;
      unsigned const field = index >> 5;
      std::uint32_t const bit = 1 << (index & 0x1F);
      mwg_assert(field < sizeof(m_mode_flags)/sizeof(m_mode_flags[0]), "invalid modeSpec");;

      std::uint32_t& flags = m_mode_flags[index >> 5];
      return (flags & bit) != 0;
    }

    void set_mode(mode_t modeSpec, bool value = true) {
      int const index = (modeSpec & mode_index_mask) >> mode_index_shift;
      unsigned const field = index >> 5;
      std::uint32_t const bit = 1 << (index & 0x1F);
      mwg_assert(field < sizeof(m_mode_flags)/sizeof(m_mode_flags[0]), "invalid modeSpec");;

      std::uint32_t& flags = m_mode_flags[index >> 5];
      if (value)
        flags |= bit;
      else
        flags &= ~bit;
    }

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
    tty_state m_state;

  public:
    void initialize_line(line_t& line) const {
      if (line.m_lflags & line_attr_t::is_line_used) return;
      line.m_lflags = line_attr_t::is_line_used | m_state.lflags;
      line.m_home = m_state.line_home;
      line.m_limit = m_state.line_limit;
    }

  public:
    term_t(contra::ansi::board_t& board): m_board(&board) {}

  public:
    board_t& board() { return *m_board; }
    board_t const& board() const { return *m_board; }
    tty_state& state() {return this->m_state;}
    tty_state const& state() const {return this->m_state;}

    // void set_fg(color_t index, aflags_t colorSpace = color_spec_indexed);
    // void reset_fg();
    // void set_bg(color_t index, aflags_t colorSpace = color_spec_indexed);
    // void reset_bg();
    // void reset_attribute() {

  public:
    void insert_graph(char32_t u) {
      // ToDo: 新しい行に移る時に line_limit, line_home を初期化する
      cursor_t& cur = m_board->cur;
      initialize_line(m_board->line());

      int const char_width = m_state.c2w(u); // ToDo 文字幅
      if (char_width <= 0) {
        std::exit(1); // ToDo: control chars, etc.
      }

      bool const simd = m_state.get_mode(mode_simd);
      curpos_t const dir = simd ? -1 : 1;

      curpos_t slh = m_board->line_home();
      curpos_t sll = m_board->line_limit();
      if (simd) std::swap(slh, sll);

      // (行頭より後でかつ) 行末に文字が入らない時は折り返し
      curpos_t const x0 = cur.x;
      curpos_t const x1 = x0 + dir * (char_width - 1);
      if ((x1 - sll) * dir > 0 && (x0 - slh) * dir > 0) {
        do_nel();
        sll = simd ? m_board->line_home() : m_board->line_limit();
      }

      curpos_t const xL = simd ? cur.x - (char_width - 1) : cur.x;

      cell_t cell;
      cell.character = u;
      cell.attribute = cur.attribute;
      cell.width = char_width;
      m_board->line().write_cells(xL, &cell, 1, 1, dir);
      cur.x += dir * char_width;

      // 行末を超えた時は折り返し
      // Note: xenl かつ cur.x >= 0 ならば sll + dir の位置にいる事を許容する。
      if ((cur.x - sll) * dir >= 1 &&
        !(cur.x == sll + dir && cur.x >= 0 && m_state.get_mode(mode_xenl))) do_nel();
    }

    void insert_marker(std::uint32_t marker) {
      bool const simd = m_state.get_mode(mode_simd);
      curpos_t const dir = simd ? -1 : 1;

      cell_t cell;
      cell.character = marker | character_t::flag_marker;
      cell.attribute = m_board->cur.attribute;
      cell.width = 0;
      initialize_line(m_board->line());
      m_board->line().write_cells(m_board->cur.x, &cell, 1, 1, dir);
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
      if (m_state.get_mode(mode_simd)) {
        int limit = m_board->m_width;
        if (!m_state.get_mode(mode_xenl)) limit--;
        if (m_board->cur.x < limit)
          m_board->cur.x++;
      } else {
        if (m_board->cur.x > 0)
          m_board->cur.x--;
      }
    }
    void do_ht() {
      cursor_t& cur = m_board->cur;

      // ToDo: tab stop の管理
      int const tabwidth = 8;
      curpos_t const xdst = std::min((cur.x + tabwidth - 1) / tabwidth * tabwidth, m_board->m_width - 1);

      curpos_t const count = xdst - cur.x;
      if (count > 0) {
        cell_t fill;
        fill.character = ascii_nul;
        fill.attribute = cur.attribute;
        fill.width = 1;
        m_board->line().write_cells(cur.x, &fill, 1, count, 1);
        cur.x += count;
      }
    }

    void do_generic_ff(int delta, bool toAppendNewLine, bool toAdjustXAsPresentationPosition) {
      if (!delta) return;

      curpos_t x = m_board->cur.x;
      if (toAdjustXAsPresentationPosition)
        x = m_board->to_presentation_position(m_board->cur.y, x);

      if (delta > 0) {
        if (m_board->cur.y + delta < m_board->m_height)
          m_board->cur.y += delta;
        else {
          curpos_t const y = m_board->cur.y;
          m_board->cur.y = m_board->m_height - 1;
          delta -= m_board->cur.y - y;
          if (toAppendNewLine)
            m_board->rotate(delta);
        }
      } else {
        if (m_board->cur.y + delta >= 0)
          m_board->cur.y += delta;
        else {
          delta += m_board->cur.y;
          m_board->cur.y = 0;
          if (toAppendNewLine)
            m_board->rotate(delta);
        }
      }

      if (toAdjustXAsPresentationPosition)
        x = m_board->to_data_position(m_board->cur.y, x);
      m_board->cur.x = x;
    }
    void do_ind() {
      do_generic_ff(1, true, !m_state.get_mode(mode_dcsm));
    }
    void do_ri() {
      do_generic_ff(-1, true, !m_state.get_mode(mode_dcsm));
    }
    void do_lf() {
      bool const toCallCR = m_state.get_mode(mode_lnm);
      do_generic_ff(1, true, !toCallCR && !m_state.get_mode(mode_dcsm));
      if (toCallCR) do_cr();
    }
    void do_ff() {
      bool const toCallCR = m_state.ff_affected_by_lnm && m_state.get_mode(mode_lnm);

      if (m_state.ff_clearing_screen) {
        if (m_state.ff_using_page_home) {
          curpos_t x = m_board->cur.x;
          curpos_t y = m_board->cur.y;
          if (!toCallCR)
            x = m_board->to_presentation_position(y, x);

          m_board->clear_screen();
          y = std::max(m_state.page_home, 0);

          if (!toCallCR)
            x = m_board->to_data_position(y, x);

          m_board->cur.x = x;
          m_board->cur.y = y;
        } else
          m_board->clear_screen();
      } else {
        do_generic_ff(1, true, !toCallCR);
      }

      if (toCallCR) do_cr();
    }
    void do_vt() {
      bool const toCallCR = m_state.vt_affected_by_lnm && m_state.get_mode(mode_lnm);
      do_generic_ff(1, m_state.vt_appending_newline, !toCallCR);
      if (toCallCR) do_cr();
    }
    void do_cr() {
      line_t& line = m_board->line();
      initialize_line(line);

      curpos_t x;
      if (m_state.get_mode(mode_simd)) {
        x = m_board->line_limit();
      } else {
        x = m_board->line_home();
      }

      if (!m_state.get_mode(mode_dcsm))
        x = m_board->to_data_position(m_board->cur.y, x);

      m_board->cur.x = x;
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
      print_unrecognized_sequence(seq);
    }

    void process_control_sequence(sequence const& seq);

    void process_command_string(sequence const& seq) {
      print_unrecognized_sequence(seq);
    }
    void process_character_string(sequence const& seq) {
      print_unrecognized_sequence(seq);
    }

    void process_control_character(char32_t uchar) {
      switch (uchar) {
      case ascii_bel: do_bel(); break;
      case ascii_bs:  do_bs();  break;
      case ascii_ht:  do_ht();  break;
      case ascii_lf:  do_lf();  break;
      case ascii_ff:  do_ff();  break;
      case ascii_vt:  do_vt();  break;
      case ascii_cr:  do_cr();  break;

      case ascii_ind: do_ind();  break;
      case ascii_nel: do_nel();  break;
      case ascii_ri:  do_ri();  break;
      }
    }

  public:
    std::uint64_t printt_state = 0;
    std::vector<char32_t> printt_buff;
    void printt(const char* text) {
      std::size_t len = std::strlen(text);
      printt_buff.resize(len);
      char32_t* const q0 = &printt_buff[0];
      char32_t* q1 = q0;
      contra::encoding::utf8_decode(text, text + len, q1, q0 + len, printt_state);
      for (char32_t const* q = q0; q < q1; q++)
        m_seqdecoder.process_char(*q);
    }
    void putc(char32_t uchar) {
      m_seqdecoder.process_char(uchar);
    }
    void write(const char* data, std::size_t size) {
      for (std::size_t i = 0; i < size; i++)
        m_seqdecoder.process_char(data[i]);
    }
  };

}
}

#endif
