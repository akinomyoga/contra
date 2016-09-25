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
    decode_state dstate() const {return m_dstate;}
    unsigned char seqtype() const {return m_seqtype;}
    std::vector<char32_t> const& seqstr() const {return m_seqstr;}
    bool hasPendingESC() const {return m_hasPendingESC;}
    std::int32_t intermediateStart() const {return m_intermediateStart;}

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

    void reset_attribute() {
      // todo: extended attribute
      m_board->cur.attribute = 0;
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
      mwg_printd();
    }
    void process_control_sequence(decoder_type* decoder, char32_t uchar) {
      if (decoder->intermediateStart() < 0) {
        switch (uchar) {
        case ascii_m: do_sgr(decoder, uchar); break;
        }
      }
    }
    void process_command_string(decoder_type* decoder) {
      mwg_printd();
    }
    void process_character_string(decoder_type* decoder) {
      mwg_printd();
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

  private:
    struct param_type {
      std::uint32_t value;
      bool isColon;
    };

    bool extract_parameters(std::vector<param_type>& params, decoder_type const* decoder, char32_t uchar) {
      std::vector<char32_t> const& str = decoder->seqstr();
      std::size_t const len = decoder->intermediateStart() < 0? str.size(): decoder->intermediateStart();

      params.clear();

      bool isSet = false;
      param_type param {0, false};
      for (std::size_t i = 0; i < len; i++) {
        char32_t const c = str[i];
        if (!(ascii_0 <= c && c < ascii_semicolon)) {
          std::fprintf(stderr, "invalid value of CSI parameter values.\n");
          // todo print sequences
          return false;
        }

        if (c <= ascii_9) {
          std::uint32_t const newValue = param.value * 10 + (c - ascii_0);
          if (newValue < param.value) {
            // overflow
            std::fprintf(stderr, "a CSI parameter value is too large.\n");
            // todo print sequences
            return false;
          }

          isSet = true;
          param.value = newValue;
        } else {
          params.push_back(param);
          isSet = true;
          param.value = 0;
          param.isColon = c == ascii_colon;
        }
      }

      if (isSet) params.push_back(param);
      return true;
    }

    static bool read_arg(std::uint32_t& result, std::vector<param_type> const& params, std::size_t& index, bool& isColonAppeared) {
      if (index <params.size() && (!isColonAppeared || params[index].isColon)) {
        if (params[index].isColon) isColonAppeared = true;
        result = params[index++].value;
        return true;
      } else
        return false;
    }

    void do_sgr_iso8613_colors(std::vector<param_type>& params, std::size_t& index, bool isfg) {
      bool isColonAppeared = false;

      std::uint32_t colorSpace = 0;
      read_arg(colorSpace, params, index, isColonAppeared);
      switch (colorSpace) {
      case 0:
      default:
        if (isfg)
          reset_fg();
        else
          reset_bg();
        break;

      case 1:
      case 2:
      case 3:
      case 4:
        // ToDo
        std::fprintf(stderr, "NYI\n");
        break;

      case 5:
        {
          std::uint32_t colorIndex;
          if (read_arg(colorIndex, params, index, isColonAppeared)) {
            if (isfg)
              set_fg(colorIndex);
            else
              set_bg(colorIndex);
          } else
            std::fprintf(stderr, "missing argument for SGR 38:5\n");
        }
        break;
      }
    }

    void do_sgr(decoder_type* decoder, char32_t uchar) {
      if (decoder->seqstr().size() > 0) {
        char32_t const head = decoder->seqstr()[0];
        if (!(ascii_0 <= head && head <= ascii_semicolon)) {
          // private form
          std::fprintf(stderr, "private SGR not supported\n");
          // todo print escape sequences
          return;
        }
      }

      std::vector<param_type> params;
      if (!extract_parameters(params, decoder, uchar)) return;

      if (params.size() == 0)
        params.push_back(param_type {0, false});

      std::size_t index = 0;
      while (index < params.size()) {
        param_type const& param = params[index++];
        if (param.isColon) continue;

        std::uint32_t const value = param.value;
        if (30 <= value && value < 40) {
          if (value < 38) {
            set_fg(value - 30);
          } else if (value == 38) {
            do_sgr_iso8613_colors(params, index, true);
          } else {
            reset_fg();
          }
        } else if (40 <= value && value < 50) {
          if (value < 48) {
            set_bg(value - 40);
          } else if (value == 48) {
            do_sgr_iso8613_colors(params, index, false);
          } else {
            reset_bg();
          }
        } else if (90 <= value && value <= 97) {
          set_fg(8 + (value - 90));
        } else if (100 <= value && value <= 107) {
          set_bg(8 + (value - 100));
        } else {
          attribute_t& attr = m_board->cur.attribute;
          switch (params[index].value) {
          case 0:
            reset_attribute();
            break;

          case 1 : attr = (attr & ~(attribute_t) is_faint_set) | is_bold_set; break;
          case 2 : attr = (attr & ~(attribute_t) is_bold_set) | is_faint_set; break;
          case 22: attr = attr & ~(attribute_t) (is_bold_set | is_faint_set); break;

          case 3 : attr = (attr & ~(attribute_t) is_fraktur_set) | is_italic_set; break;
          case 20: attr = (attr & ~(attribute_t) is_italic_set) | is_fraktur_set; break;
          case 23: attr = attr & ~(attribute_t) (is_italic_set | is_fraktur_set); break;

          case 4 : attr = (attr & ~(attribute_t) is_double_underline_set) | is_underline_set; break;
          case 21: attr = (attr & ~(attribute_t) is_underline_set) | is_double_underline_set; break;
          case 24: attr = attr & ~(attribute_t) (is_underline_set | is_double_underline_set); break;

          case 5 : attr = (attr & ~(attribute_t) is_rapid_blink_set) | is_blink_set; break;
          case 6 : attr = (attr & ~(attribute_t) is_blink_set) | is_rapid_blink_set; break;
          case 25: attr = attr & ~(attribute_t) (is_blink_set | is_rapid_blink_set); break;

          case 7 : attr = attr | is_inverse_set; break;
          case 27: attr = attr & ~(attribute_t) is_inverse_set; break;

          case 8 : attr = attr | is_invisible_set; break;
          case 28: attr = attr & ~(attribute_t) is_invisible_set; break;

          case 9 : attr = attr | is_strike_set; break;
          case 29: attr = attr & ~(attribute_t) is_strike_set; break;

          default:
            std::fprintf(stderr, "unrecognized SGR value\n");
            break;
          }
        }
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
