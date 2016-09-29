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

  struct csi_param_type {
    std::uint32_t value;
    bool isColon;
    bool isDefault;
  };

  class csi_parameters {
    std::vector<csi_param_type> m_data;
    std::size_t m_index {0};

  public:
    std::size_t size() const {return m_data.size();}
    void push_back(csi_param_type const& value) {m_data.push_back(value);}

  public:
    bool extract_parameters(char32_t const* str, std::size_t len, char32_t uchar) {
      m_data.clear();
      m_index = 0;
      m_isColonAppeared = false;

      bool isSet = false;
      csi_param_type param {0, false, true};
      for (std::size_t i = 0; i < len; i++) {
        char32_t const c = str[i];
        if (!(ascii_0 <= c && c <= ascii_semicolon)) {
          std::fprintf(stderr, "invalid value of CSI parameter values.\n");
          mwg_printd("CSI ... %c", uchar);
          // todo print sequences
          return false;
        }

        if (c <= ascii_9) {
          std::uint32_t const newValue = param.value * 10 + (c - ascii_0);
          if (newValue < param.value) {
            // overflow
            std::fprintf(stderr, "a CSI parameter value is too large.\n");
            mwg_printd("CSI ... %c", uchar);
            // todo print sequences
            return false;
          }

          isSet = true;
          param.value = newValue;
          param.isDefault = false;
        } else {
          m_data.push_back(param);
          isSet = true;
          param.value = 0;
          param.isColon = c == ascii_colon;
          param.isDefault = true;
        }
      }

      if (isSet) m_data.push_back(param);
      return true;
    }

  public:
    bool m_isColonAppeared;

    bool read_param(std::uint32_t& result, std::uint32_t defaultValue) {
      while (m_index < m_data.size()) {
        csi_param_type const& param = m_data[m_index++];
        if (!param.isColon) {
          m_isColonAppeared = false;
          if (param.isDefault)
            result = param.value;
          else
            result = defaultValue;
          return true;
        }
      }
      result = defaultValue;
      return false;
    }

    bool read_arg(std::uint32_t& result, bool toAllowSemicolon, std::uint32_t defaultValue) {
      if (m_index <m_data.size()
        && (m_data[m_index].isColon || (toAllowSemicolon && !m_isColonAppeared))
      ) {
        csi_param_type const& param = m_data[m_index++];
        if (param.isColon) m_isColonAppeared = true;
        if (param.isDefault)
          result = param.value;
        else
          result = defaultValue;
        return true;
      }

      result = defaultValue;
      return false;
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
      m_board->update_cursor_attribute();
      m_board->set_character(c, u);
      m_board->set_attribute(c, cur.attribute);

      for (int i = 1; i < charwidth; i++) {
        m_board->set_character(c + i, is_wide_extension);
        m_board->set_attribute(c + i, 0);
      }

      cur.x += charwidth;

      if (!m_config.xenl && cur.x == m_board->m_width) do_crlf();
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
      mwg_printd("%p", &decoder);
    }
    void process_control_sequence(decoder_type* decoder, char32_t uchar) {
      if (decoder->intermediateStart() < 0) {
        switch (uchar) {
        case ascii_m: do_sgr(decoder, uchar); break;
        }
      }
    }
    void process_command_string(decoder_type* decoder) {
      mwg_printd("%p", &decoder);
    }
    void process_character_string(decoder_type* decoder) {
      mwg_printd("%p", &decoder);
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
    void do_sgr_iso8613_colors(csi_parameters& params, bool isfg) {
      std::uint32_t colorSpace;
      params.read_arg(colorSpace, true, 0);
      color_t color;
      switch (colorSpace) {
      case color_spec_default:
      default:
        if (isfg)
          reset_fg();
        else
          reset_bg();
        break;

      case color_spec_transparent:
        color = 0;
        goto set_color;

      case color_spec_rgb:
      case color_spec_cmy:
      case color_spec_cmyk:
        {
          int const ncomp = colorSpace == color_spec_cmyk ? 4: 3;
          std::uint32_t comp;
          for (int i = 0; i < ncomp; i++) {
            params.read_arg(comp, true, 0);
            if (comp > 255) comp = 255;
            color |= comp << i * 8;
          }

          if (params.read_arg(comp, false, 0)) {
            // If there are more than `ncomp` colon-separated arguments,
            // switch to ISO 8613-6 compatible mode expecting sequences like
            //
            //     CSI 38:2:CS:R:G:B:dummy:tol:tolCS m
            //
            // In this mode, discard the first byte (CS = color space)
            // and append a new byte containing the last color component.
            color = color >> 8 | comp << (ncomp - 1) * 8;
          } else {
            // If there are no more than `ncomp` arguments,
            // switch to konsole/xterm compatible mode expecting sequences like
            //
            //   CSI 38;2;R;G;B m
            //
            // In this mode, do nothing here; We have already correct `color`.
          }
          goto set_color;
        }

      case color_spec_indexed:
        if (params.read_arg(color, true, 0))
          goto set_color;
        else
          std::fprintf(stderr, "missing argument for SGR 38:5\n");
        break;

      set_color:
        if (isfg)
          set_fg(color, colorSpace);
        else
          set_bg(color, colorSpace);
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

      csi_parameters params;
      {
        std::vector<char32_t> const& str = decoder->seqstr();
        std::size_t const len = decoder->intermediateStart() < 0? str.size(): decoder->intermediateStart();
        if (!params.extract_parameters(&str[0], len, uchar)) return;
      }

      if (params.size() == 0)
        params.push_back(csi_param_type {0, false, true});

      std::uint32_t value;
      while (params.read_param(value, 0)) {
        if (30 <= value && value < 40) {
          if (value < 38) {
            set_fg(value - 30);
          } else if (value == 38) {
            do_sgr_iso8613_colors(params, true);
          } else {
            reset_fg();
          }
        } else if (40 <= value && value < 50) {
          if (value < 48) {
            set_bg(value - 40);
          } else if (value == 48) {
            do_sgr_iso8613_colors(params, false);
          } else {
            reset_bg();
          }
        } else if (90 <= value && value <= 97) {
          set_fg(8 + (value - 90));
        } else if (100 <= value && value <= 107) {
          set_bg(8 + (value - 100));
        } else {
          aflags_t& aflags = m_board->cur.xattr_edit.aflags;
          xflags_t& xflags = m_board->cur.xattr_edit.xflags;

          switch (value) {
          case 0:
            m_board->cur.xattr_edit.aflags = 0;
            m_board->cur.xattr_edit.xflags &= non_sgr_xflags_mask;
            m_board->cur.xattr_edit.fg = 0;
            m_board->cur.xattr_edit.bg = 0;
            break;

          case 1 : aflags = (aflags & ~(attribute_t) is_faint_set) | is_bold_set; break;
          case 2 : aflags = (aflags & ~(attribute_t) is_bold_set) | is_faint_set; break;
          case 22: aflags = aflags & ~(attribute_t) (is_bold_set | is_faint_set); break;

          case 3 : aflags = (aflags & ~(attribute_t) is_fraktur_set) | is_italic_set; break;
          case 20: aflags = (aflags & ~(attribute_t) is_italic_set) | is_fraktur_set; break;
          case 23: aflags = aflags & ~(attribute_t) (is_italic_set | is_fraktur_set); break;

          case 4 : aflags = (aflags & ~(attribute_t) is_double_underline_set) | is_underline_set; break;
          case 21: aflags = (aflags & ~(attribute_t) is_underline_set) | is_double_underline_set; break;
          case 24: aflags = aflags & ~(attribute_t) (is_underline_set | is_double_underline_set); break;

          case 5 : aflags = (aflags & ~(attribute_t) is_rapid_blink_set) | is_blink_set; break;
          case 6 : aflags = (aflags & ~(attribute_t) is_blink_set) | is_rapid_blink_set; break;
          case 25: aflags = aflags & ~(attribute_t) (is_blink_set | is_rapid_blink_set); break;

          case 7 : aflags = aflags | is_inverse_set; break;
          case 27: aflags = aflags & ~(attribute_t) is_inverse_set; break;

          case 8 : aflags = aflags | is_invisible_set; break;
          case 28: aflags = aflags & ~(attribute_t) is_invisible_set; break;

          case 9 : aflags = aflags | is_strike_set; break;
          case 29: aflags = aflags & ~(attribute_t) is_strike_set; break;

          case 26: xflags = xflags | is_proportional_set; break;
          case 50: xflags = xflags & ~(attribute_t) is_proportional_set; break;

          case 51: xflags = (xflags & ~(attribute_t) is_circle_set) | is_frame_set; break;
          case 52: xflags = (xflags & ~(attribute_t) is_frame_set) | is_circle_set; break;
          case 54: xflags = xflags & ~(attribute_t) (is_frame_set | is_circle_set); break;

          case 53: xflags = xflags | is_overline_set; break;
          case 55: xflags = xflags & ~(attribute_t) is_overline_set; break;

          case 60: xflags = (xflags & ~(attribute_t) is_ideogram_decoration_mask) | is_ideogram_single_rb_set; break;
          case 61: xflags = (xflags & ~(attribute_t) is_ideogram_decoration_mask) | is_ideogram_double_rb_set; break;
          case 62: xflags = (xflags & ~(attribute_t) is_ideogram_decoration_mask) | is_ideogram_single_lt_set; break;
          case 63: xflags = (xflags & ~(attribute_t) is_ideogram_decoration_mask) | is_ideogram_double_lt_set; break;
          case 66: xflags = (xflags & ~(attribute_t) is_ideogram_decoration_mask) | is_ideogram_single_lb_set; break;
          case 67: xflags = (xflags & ~(attribute_t) is_ideogram_decoration_mask) | is_ideogram_double_lb_set; break;
          case 68: xflags = (xflags & ~(attribute_t) is_ideogram_decoration_mask) | is_ideogram_single_rt_set; break;
          case 69: xflags = (xflags & ~(attribute_t) is_ideogram_decoration_mask) | is_ideogram_double_rt_set; break;
          case 64: xflags = (xflags & ~(attribute_t) is_ideogram_decoration_mask) | is_ideogram_stress_set   ; break;
          case 65: xflags = xflags & ~(attribute_t) is_ideogram_decoration_mask; break;

          default:
            std::fprintf(stderr, "unrecognized SGR value\n");
            break;
          }

          m_board->cur.xattr_dirty = true;
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
