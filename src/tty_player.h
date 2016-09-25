// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef CONTRA_TTY_PLAYER_H
#define CONTRA_TTY_PLAYER_H
#include "board.h"
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

  constexpr mode_t construct_mode_spec(mode_t type, mode_t param, mode_t index) {
    return type << mode_type_shift
      | param << mode_param_shift
      | index << mode_index_shift;
  }

  enum mode_spec {
    mode_lnm = construct_mode_spec(0, 20, 0),
  };

  struct tty_config {
    bool xenl {1};

    tty_config() {
      this->initialize_mode();
    }

    bool c1_8bit_representation_enabled {true};
    bool osc_sequence_terminated_by_bel {true};

    bool vt_affected_by_lnm    {true};
    bool vt_appending_newline  {true};
    bool vt_using_line_tabstop {false}; // ToDo: not yet supported

    bool ff_affected_by_lnm     {true};
    bool ff_clearing_screen     {false};
    bool ff_using_home_position {false};

  private:
    std::uint32_t m_mode_flags[1];

    void initialize_mode() {
      std::fill(std::begin(m_mode_flags), std::end(m_mode_flags), 0);
      set_mode(mode_lnm);
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

  template<typename Processor>
  class sequence_decoder {
    typedef Processor processor_type;

    enum decode_state {
      decode_default,
      decode_control_sequence,
      decode_command_string,
      decode_character_string,
    };

    processor_type* m_proc;
    tty_config* m_config;

    decode_state m_dstate {decode_default};
    unsigned char m_seqtype {0};
    std::vector<char32_t> m_seqstr;
    bool m_hasPendingESC {false};
    std::int32_t m_intermediateStart {-1};

  public:
    sequence_decoder(processor_type* proc, tty_config* config): m_proc(proc), m_config(config) {}

  private:
    void clear() {
      this->m_seqtype = 0;
      this->m_seqstr.clear();
      this->m_hasPendingESC = false;
      this->m_intermediateStart = -1;
    }
    void append(char32_t uchar) {
      this->m_seqstr.push_back(uchar);
    }

    void process_invalid_sequence() {
      m_proc->process_invalid_sequence(this);
      clear();
      m_dstate = decode_default;
    }

    void process_control_sequence(char32_t uchar) {
      m_proc->process_control_sequence(this, uchar);
      m_dstate = decode_default;
      clear();
    }

    void process_command_string() {
      m_proc->process_command_string(this);
      m_dstate = decode_default;
      clear();
    }

    void process_character_string() {
      m_proc->process_character_string(this);
      m_dstate = decode_default;
      clear();
    }

    void process_char_for_control_sequence(char32_t uchar) {
      if (0x40 <= uchar && uchar <= 0x7E) {
        process_control_sequence(uchar);
      } else {
        if (m_intermediateStart >= 0) {
          if (0x20 <= uchar && uchar <= 0x2F) {
            append(uchar);
          } else {
            process_invalid_sequence();
          }
        } else {
          if (0x20 <= uchar && uchar <= 0x3F) {
            if (uchar <= 0x2F)
              m_intermediateStart = m_seqstr.size();
            append(uchar);
          } else
            process_invalid_sequence();
        }
      }
    }

    void process_char_for_command_string(char32_t uchar) {
      if (m_hasPendingESC) {
        if (uchar == ascii_backslash) {
          process_command_string();
        } else {
          process_invalid_sequence();
          process_char(ascii_esc);
          process_char(uchar);
        }

        return;
      }

      if ((0x08 <= uchar && uchar <= 0x0D) || (0x20 <= uchar && uchar <= 0x7E)) {
        append(uchar);
      } else if (uchar == ascii_esc) {
        m_hasPendingESC = true;
      } else if ((uchar == ascii_st && m_config->c1_8bit_representation_enabled)
        || (uchar == ascii_bel && m_seqtype == ascii_osc && m_config->osc_sequence_terminated_by_bel)
      ) {
        process_command_string();
      } else {
        process_invalid_sequence();
        process_char(uchar);
      }
    }

    void process_char_for_character_string(char32_t uchar) {
      if (m_hasPendingESC) {
        if (uchar == ascii_backslash || (uchar == ascii_st && m_config->c1_8bit_representation_enabled)) {
          // final ST
          if (uchar == ascii_st)
            append(ascii_esc);
          process_character_string();
          return;
        } else if (uchar == ascii_X || (uchar == ascii_sos && m_config->c1_8bit_representation_enabled)) {
          // invalid SOS
          process_invalid_sequence();
          if (uchar == ascii_X)
            process_char(ascii_esc);
          process_char(uchar);
          return;
        } else if (uchar == ascii_esc) {
          append(ascii_esc);
          m_hasPendingESC = false;
        }
      }

      if (uchar == ascii_esc)
        m_hasPendingESC = true;
      else if (uchar == ascii_st && m_config->c1_8bit_representation_enabled) {
        process_character_string();
      } else if (uchar == ascii_sos && m_config->c1_8bit_representation_enabled) {
        process_invalid_sequence();
        process_char(uchar);
      } else
        append(uchar);
    }

    void process_char_default_c1(char32_t c1char) {
      switch (c1char) {
      case ascii_csi:
        m_seqtype = c1char;
        m_dstate = decode_control_sequence;
        break;
      case ascii_sos:
        m_seqtype = c1char;
        m_dstate = decode_character_string;
        break;
      case ascii_dcs:
      case ascii_osc:
      case ascii_pm:
      case ascii_apc:
        m_seqtype = c1char;
        m_dstate = decode_command_string;
        break;
      default:
        process_invalid_sequence();
        break;
      }
    }

    void process_char_default(char32_t uchar) {
      if (m_hasPendingESC) {
        m_hasPendingESC = false;
        if (0x40 <= uchar && uchar < 0x60) {
          process_char_default_c1((uchar & 0x1F) | 0x80);
        } else {
          process_invalid_sequence();
        }
        return;
      }

      if (uchar < 0x20) {
        if (uchar == ascii_esc)
          m_hasPendingESC = true;
        else
          m_proc->process_control_character(uchar);
      } else if (0x80 <= uchar && uchar < 0xA0) {
        if (m_config->c1_8bit_representation_enabled)
          process_char_default_c1(uchar);
      } else {
        m_proc->insert_char(uchar);
      }
    }

  public:
    void process_char(char32_t uchar) {
      switch (m_dstate) {
      case decode_default:
        process_char_default(uchar);
        break;
      case decode_control_sequence:
        process_char_for_control_sequence(uchar);
        break;
      case decode_command_string:
        process_char_for_command_string(uchar);
        break;
      case decode_character_string:
        process_char_for_character_string(uchar);
        break;
      }
    }

    void process_end() {
      switch (m_dstate) {
      case decode_control_sequence:
      case decode_command_string:
      case decode_character_string:
        process_invalid_sequence();
        break;
      default: break;
      }
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

  private:
    typedef sequence_decoder<tty_player> decoder_type;
    decoder_type m_seqdecoder {this, &this->m_config};
    friend decoder_type;

    void process_invalid_sequence(decoder_type* decoder) {
      // ToDo: 何処かにログ出力
    }
    void process_control_sequence(decoder_type* decoder, char32_t uchar) {
      switch (uchar) {
      case ascii_m:
        break;
      }
    }
    void process_command_string(decoder_type* decoder) {}
    void process_character_string(decoder_type* decoder) {}

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
  };

}
#endif
