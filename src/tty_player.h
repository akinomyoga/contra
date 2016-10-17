// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef CONTRA_TTY_PLAYER_H
#define CONTRA_TTY_PLAYER_H
#include "board.h"
#include "sequence.h"
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <iterator>

namespace contra {

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

  constexpr std::uint32_t compose_bytes(byte major, byte minor) {
    return minor << 8 | major;
  }

  enum mode_spec {
    ansi_mode   = 0, // CSI Ps h
    dec_mode    = 1, // CSI ? Ps h
    contra_mode = 2, // private mode

    mode_dcsm = construct_mode_spec( 9, ansi_mode,  9),
    mode_lnm  = construct_mode_spec(20, ansi_mode, 20),
    mode_grcm = construct_mode_spec(21, ansi_mode, 21),
    mode_zdm  = construct_mode_spec(22, ansi_mode, 22),

    mode_simd = construct_mode_spec(23, contra_mode, 9201),
    mode_xenl = construct_mode_spec(24, contra_mode, 9202),
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
  };

  class tty_player {
  private:
    contra::board* m_board;

    tty_state m_state;

  public:
    contra::board* board() {return this->m_board;}
    contra::board const* board() const {return this->m_board;}
    tty_state* state() {return &this->m_state;}
    tty_state const* state() const {return &this->m_state;}

  public:
    tty_player(contra::board& board): m_board(&board) {}

    void apply_line_attribute(board_line* line) const {
      if (line->lflags & is_line_used) return;
      line->lflags = is_line_used | m_state.lflags;
      line->home = m_state.line_home;
      line->limit = m_state.line_limit;
    }
    // void apply_line_attribute(curpos_t y) {
    //   apply_line_attribute(m_board->line(y));
    // }
    // void apply_line_attribute() {
    //   apply_line_attribute(m_board->cur.y);
    // }

    // ToDo: IRM
    void insert_char(char32_t u) {
      board_cursor& cur = m_board->cur;
      board_line* line = m_board->line(cur.y);
      apply_line_attribute(line);

      int const charWidth = 1; // ToDo 文字幅
      if (charWidth <= 0) {
        std::exit(1); // ToDo: control chars, etc.
      }

      if (m_state.get_mode(mode_simd)) {
        // 折り返し1
        curpos_t beg = line->home < 0? 0: line->home;
        curpos_t end = line->limit < 0? m_board->m_width: line->limit + 1;
        if (cur.x - charWidth + 1 < beg && cur.x < end) {
          do_nel();
          line = m_board->line(cur.y);
          apply_line_attribute(line);
          beg = line->home < 0? 0: line->home;
        }

        board_cell* const cells = m_board->cell(0, cur.y);
        m_board->update_cursor_attribute();
        m_board->put_character(cells + cur.x - charWidth + 1, u, cur.attribute, charWidth);
        cur.x -= charWidth;

        // 折り返し2
        // Note: xenl かつ cur.x >= 0 ならば beg - 1 の位置にいる事を許容する。
        if (cur.x <= beg - 1 && !(cur.x == beg - 1 && cur.x >= 0 && m_state.get_mode(mode_xenl))) do_nel();
      } else {
        // 折り返し1
        curpos_t beg = line->home < 0? 0: line->home;
        curpos_t end = line->limit < 0? m_board->m_width: line->limit + 1;
        if (cur.x + charWidth > end && cur.x > beg) {
          do_nel();
          line = m_board->line(cur.y);
          apply_line_attribute(line);
          end = line->limit < 0? m_board->m_width: line->limit + 1;
        }

        board_cell* const cells = m_board->cell(0, cur.y);
        m_board->update_cursor_attribute();
        m_board->put_character(cells + cur.x, u, cur.attribute, charWidth);
        cur.x += charWidth;

        // 折り返し2
        if (cur.x >= end && !(cur.x == end && m_state.get_mode(mode_xenl))) do_nel();
      }
    }

    void set_fg(color_t index, aflags_t colorSpace = color_spec_indexed) {
      extended_attribute& xattr = m_board->cur.xattr_edit;
      xattr.fg = index;
      xattr.aflags
        = (xattr.aflags & ~(attribute_t) fg_color_mask)
        | (colorSpace << fg_color_shift & fg_color_mask)
        | is_fg_color_set;
      m_board->cur.xattr_dirty = true;
    }
    void reset_fg() {
      m_board->cur.xattr_edit.fg = 0;
      m_board->cur.xattr_edit.aflags &= ~(attribute_t) (is_fg_color_set | fg_color_mask);
      m_board->cur.xattr_dirty = true;
    }

    void set_bg(color_t index, aflags_t colorSpace = color_spec_indexed) {
      extended_attribute& xattr = m_board->cur.xattr_edit;
      xattr.bg = index;
      xattr.aflags
        = (xattr.aflags & ~(attribute_t) bg_color_mask)
        | (colorSpace << bg_color_shift & bg_color_mask)
        | is_bg_color_set;
      m_board->cur.xattr_dirty = true;
    }
    void reset_bg() {
      m_board->cur.xattr_edit.bg = 0;
      m_board->cur.xattr_edit.aflags &= ~(attribute_t) (is_bg_color_set | bg_color_mask);
      m_board->cur.xattr_dirty = true;
    }

    void reset_attribute() {
      m_board->cur.xattr_edit.clear();
      m_board->cur.xattr_dirty = true;
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
      board_cursor& cur = m_board->cur;

      // ToDo: tab stop の管理
      int const tabwidth = 8;
      curpos_t const xdst = std::min((cur.x + tabwidth - 1) / tabwidth * tabwidth, m_board->m_width - 1);

      if (cur.x >= xdst) return;

      char32_t const fillChar = U' ';
      board_cell* cell = m_board->cell(cur.x, cur.y);
      for (; cur.x < xdst; cur.x++) {
        m_board->set_character(cell, fillChar);
        m_board->set_attribute(cell, cur.attribute);
      }
    }

    void do_generic_ff(bool toAppendNewLine, bool toAdjustXAsPresentationPosition) {
      curpos_t x = m_board->cur.x;
      if (toAdjustXAsPresentationPosition)
        x = m_board->to_presentation_position(m_board->cur.y, x);

      if (m_board->cur.y + 1 < m_board->m_height)
        m_board->cur.y++;
      else if (toAppendNewLine) {
        // ToDo: 消えてなくなる行を記録する?
        m_board->rotate();
        m_board->clear_line(m_board->cur.y);
      } else
        return;

      if (toAdjustXAsPresentationPosition)
        x = m_board->to_data_position(m_board->cur.y, x);
      m_board->cur.x = x;
    }
    void do_lf() {
      bool const toCallCR = m_state.get_mode(mode_lnm);
      do_generic_ff(true, !toCallCR && !m_state.get_mode(mode_dcsm));
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
        do_generic_ff(true, !toCallCR);
      }

      if (toCallCR) do_cr();
    }
    void do_vt() {
      bool const toCallCR = m_state.vt_affected_by_lnm && m_state.get_mode(mode_lnm);
      do_generic_ff(m_state.vt_appending_newline, !toCallCR);
      if (toCallCR) do_cr();
    }
    void do_cr() {
      board_line* const line = m_board->line(m_board->cur.y);
      apply_line_attribute(line);

      curpos_t x;
      if (m_state.get_mode(mode_simd)) {
        x = line->limit < 0? m_board->m_width - 1:
          std::min(line->limit, m_board->m_width - 1);
      } else {
        x = line->home < 0? 0:
          std::min(line->home, m_board->m_width - 1);
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
    typedef sequence_decoder<tty_player> decoder_type;
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
      }
    }

  public:
    void printt(const char* text) {
      while (*text) m_seqdecoder.process_char(*text++);
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
#endif
