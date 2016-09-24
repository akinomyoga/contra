// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef CONTRA_TTY_PLAYER_H
#define CONTRA_TTY_PLAYER_H
#include "board.h"
#include <cstdlib>
#include <algorithm>

namespace contra {

  typedef std::uint32_t mode_t;

  enum mode_spec_layout {
    mode_field_mask = 0xF0000000u, mode_field_shift = 28,
    mode_index_mask = 0x0FF00000u, mode_index_shift = 20,
    mode_type_mask  = 0x000F0000u, mode_type_shift  = 16,
    mode_param_mask = 0x0000FFFFu, mode_param_shift = 0,
  };

  constexpr mode_t construct_mode_spec(mode_t type, mode_t param, mode_t field, mode_t index) {
    return type << mode_type_shift
      | param << mode_param_shift
      | field << mode_field_shift
      | index << mode_index_shift;
  }

  enum mode_spec {
    mode_lnm = construct_mode_spec(0, 20, 0, 0),
  };

  struct tty_config {
    bool xenl {1};

    tty_config() {
      this->initialize_mode();
    }

    bool vt_affected_by_lnm    {true};
    bool vt_appending_newline  {true};
    bool vt_using_line_tabstop {false}; // ToDo: not yet supported

    bool ff_affected_by_lnm     {true};
    bool ff_clearing_screen     {false};
    bool ff_using_home_position {false};

  private:
    std::uint32_t m_mode_flags0;

    void initialize_mode() {
      this->m_mode_flags0 = 0;
      set_mode(mode_lnm);
    }

  public:
    bool get_mode(mode_t modeSpec) {
      int const field = (modeSpec & mode_field_mask) >> mode_field_shift;
      int const bitIndex = (modeSpec & mode_index_mask) >> mode_index_shift;
      if (field == 0) {
        return (m_mode_flags0 & 1 << bitIndex) != 0;
      } else {
        mwg_check(false, "invalid modeSpec");
        std::exit(1);
      }
    }

    void set_mode(mode_t modeSpec, bool value = true) {
      int const field = (modeSpec & mode_field_mask) >> mode_field_shift;
      int const bitIndex = (modeSpec & mode_index_mask) >> mode_index_shift;
      if (field == 0) {
        if (value)
          m_mode_flags0 |= 1 << bitIndex;
        else
          m_mode_flags0 &= ~(1 << bitIndex);
      } else {
        mwg_check(false, "invalid modeSpec");
        std::exit(1);
      }
    }

  public:
    void do_bel() {
      std::fputc('\a', stdout);
    }
  };

  struct tty_state {
    int page_home_position {0};
  };

  struct tty_player {
  private:
    board* m_board;

    tty_config m_config;
    tty_state  m_state;
  public:
    tty_player(board& board): m_board(&board) {}

    void insert_char(char32_t u) {
      board_cursor& cur = m_board->cur;
      int const charwidth = 1; // ToDo 文字幅
      if (cur.x + charwidth > m_board->m_width) do_crlf();

      if (charwidth <= 0) {
        std::exit(1); // ToDo
      }

      board_cell* c = &m_board->cell(cur.x, cur.y);
      m_board->set_character(c, u);
      m_board->set_attribute(c, cur.attribute);

      for (int i = 1; i < charwidth; i++) {
        m_board->set_character(c + i, is_wide_extension);
        m_board->set_attribute(c + i, 0);
      }

      cur.x += charwidth;

      if (!m_config.xenl && cur.x == m_board->m_width) do_crlf();
    }

    void set_fg(int index) {
      if (index < 256) {
        m_board->cur.attribute
          = (m_board->cur.attribute & ~(attribute_t) fg_color_mask)
          | (index << fg_color_shift & fg_color_mask)
          | is_fg_color_set;
      }
    }
    void reset_fg() {
      m_board->cur.attribute &= ~(attribute_t) (is_fg_color_set | fg_color_mask);
    }

    void set_bg(int index) {
      if (index < 256) {
        m_board->cur.attribute
          = (m_board->cur.attribute & ~(attribute_t) bg_color_mask)
          | (index << bg_color_shift & bg_color_mask)
          | is_bg_color_set;
      }
    }
    void reset_bg() {
      m_board->cur.attribute &= ~(attribute_t) (is_bg_color_set | bg_color_mask);
    }

  public:
    void do_bel() {
      m_config.do_bel();
    }
    void do_bs () {
      if (m_board->cur.x > 0)
        m_board->cur.x--;
    }
    void do_ht() {
      board_cursor& cur = m_board->cur;

      // ToDo: tab stop の管理
      int const tabwidth = 8;
      curpos_t const xdst = std::min((cur.x + tabwidth - 1) / tabwidth * tabwidth, m_board->m_width - 1);

      if (cur.x >= xdst) return;

      char32_t const fillChar = U' ';
      board_cell* cell = &m_board->cell(cur.x, cur.y);
      for (; cur.x < xdst; cur.x++) {
        m_board->set_character(cell, fillChar);
        m_board->set_attribute(cell, cur.attribute);
      }
    }

    void do_plain_vt(bool toAppendNewLine = true) {
      m_board->cur.y++;
      if (m_board->cur.y >= m_board->m_height) {
        m_board->cur.y--;
        if (toAppendNewLine) {
          // ToDo: 消えてなくなる行を記録する?
          m_board->rotate();
          m_board->clear_line(m_board->cur.y);
        }
      }
    }
    void do_lf () {
      do_plain_vt(true);
      if (m_config.get_mode(mode_lnm)) do_cr();
    }
    void do_crlf() {
      do_cr();
      do_lf();
    }
    void do_ff() {
      if (m_config.ff_clearing_screen) {
        m_board->clear_screen();
        if (m_config.ff_using_home_position)
          m_board->cur.y = m_state.page_home_position;
      } else {
        do_plain_vt(true);
      }
      if (m_config.ff_affected_by_lnm && m_config.get_mode(mode_lnm)) do_cr();
    }
    void do_vt() {
      do_plain_vt(m_config.vt_appending_newline);
      if (m_config.vt_affected_by_lnm && m_config.get_mode(mode_lnm)) do_cr();
    }
    void do_cr() {
      m_board->cur.x = 0;
    }

  public:
    void process_char(char32_t uchar) {
      if (uchar < 0x20) {
        switch (uchar) {
        case ascii_bel: do_bel(); break;
        case ascii_bs:  do_bs();  break;
        case ascii_ht:  do_ht();  break;
        case ascii_lf:  do_lf();  break;
        case ascii_ff:  do_ff();  break;
        case ascii_vt:  do_vt();  break;
        case ascii_cr:  do_cr();  break;
        }
      } else if (0x80 <= uchar && uchar < 0xA0) {
        // todo
      } else {
        insert_char(uchar);
      }
    }

    void printt(const char* text) {
      while (*text) process_char(*text++);
    }

  };

}
#endif
