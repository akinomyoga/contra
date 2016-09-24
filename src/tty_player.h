// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef CONTRA_TTY_PLAYER_H
#define CONTRA_TTY_PLAYER_H
#include "board.h"
#include <algorithm>

namespace contra {

  struct tty_config {
    bool xenl {1};

    void do_bel() {
      std::fputc('\a', stdout);
    }
  };

  struct tty_player {
  private:
    board* m_board;

    tty_config m_config;

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
    void do_crlf () {
      do_cr();
      do_vt();
    }
    void do_vt () {
      m_board->cur.y++;
      if (m_board->cur.y >= m_board->m_height) {
        // ToDo: 消えてなくなる行を記録する?
        m_board->rotate();
        m_board->cur.y--;
        m_board->clear_line(m_board->cur.y);
      }
    }
    void do_cr () {
      m_board->cur.x = 0;
    }
    void do_bel() {
      m_config.do_bel();
    }

  public:
    void process_char(char32_t uchar) {
      if (uchar < 0x20) {
        switch (uchar) {
        case ascii_bel: do_bel();  break;
        case ascii_bs:  do_bs();   break;
        case ascii_ht:  do_ht();   break;
        case ascii_lf:  do_crlf(); break;
        case ascii_ff:  break;
        case ascii_vt: do_vt(); break;
        case ascii_cr: do_cr(); break;
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
