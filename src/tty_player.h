// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef CONTRA_TTY_PLAYER_H
#define CONTRA_TTY_PLAYER_H
#include "board.h"
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

    mode_simd = construct_mode_spec(23, contra_mode, 9201),
    mode_xenl = construct_mode_spec(24, contra_mode, 9202),
  };

  struct tty_state {
    curpos_t page_home_position {0};
    curpos_t line_home  {-1};
    curpos_t line_limit {-1};

    contra::presentation_direction presentation_direction {presentation_direction_default};
    line_attr_t lflags {0};

    tty_state() {
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
      set_mode(mode_dcsm);
      set_mode(mode_grcm);
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

  class sequence {
    byte                  m_type  {0};
    byte                  m_final {0};
    std::vector<char32_t> m_content;
    std::int32_t          m_intermediateStart {-1};

  public:
    void clear() {
      this->m_type = 0;
      this->m_final = 0;
      this->m_content.clear();
      this->m_intermediateStart = -1;
    }
    void append(char32_t uchar) {
      this->m_content.push_back(uchar);
    }
    void start_intermediate() {
      this->m_intermediateStart = this->m_content.size();
    }
    bool has_intermediate() const {
      return m_intermediateStart >= 0;
    }
    void set_type(byte type) {
      this->m_type = type;
    }
    void set_final(byte final) {
      this->m_final = final;
    }

  public:
    byte type() const {return m_type;}
    byte final() const {return m_final;}

    bool is_private_csi() const {
      return !(m_content.size() == 0 || (ascii_0 <= m_content[0] && m_content[0] <= ascii_semicolon));
    }

    char32_t const* parameter() const {
      return &m_content[0];
    }
    std::int32_t parameterSize() const {
      return m_intermediateStart < 0? m_content.size(): m_intermediateStart;
    }
    char32_t const* intermediate() const {
      return m_intermediateStart < 0? nullptr: &m_content[m_intermediateStart];
    }
    std::int32_t intermediateSize() const {
      return m_intermediateStart < 0? 0: m_content.size() - m_intermediateStart;
    }

  private:
    void print_char(std::FILE* file, char32_t ch) const {
      const char* name = get_ascii_name(ch);
      if (name)
        std::fprintf(file, "%s ", name);
      else
        std::fprintf(file, "%c ", ch);
    }

  public:
    void print(std::FILE* file) const {
      switch (m_type) {
      case ascii_csi:
      case ascii_esc:
        print_char(file, m_type);
        for (char32_t ch: m_content)
          print_char(file, ch);
        std::fprintf(file, "%c", m_final & 0xFF);
        break;
      default: // command string & characater string
        print_char(file, m_type);
        for (char32_t ch: m_content)
          print_char(file, ch);
        std::fprintf(file, "ST");
        break;
      }
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
    tty_state* m_state;
    decode_state m_dstate {decode_default};
    bool m_hasPendingESC {false};
    sequence m_seq;

  public:
    sequence_decoder(Processor* proc, tty_state* config): m_proc(proc), m_state(config) {}

  private:
    void clear() {
      this->m_hasPendingESC = false;
      this->m_seq.clear();
      this->m_dstate = decode_default;
    }

    void process_invalid_sequence() {
      m_proc->process_invalid_sequence(this->m_seq);
      clear();
    }

    void process_control_sequence() {
      m_proc->process_control_sequence(this->m_seq);
      clear();
    }

    void process_command_string() {
      m_proc->process_command_string(this->m_seq);
      clear();
    }

    void process_character_string() {
      m_proc->process_character_string(this->m_seq);
      clear();
    }

    void process_char_for_control_sequence(char32_t uchar) {
      if (0x40 <= uchar && uchar <= 0x7E) {
        m_seq.set_final((byte) uchar);
        process_control_sequence();
      } else {
        if (m_seq.has_intermediate()) {
          if (0x20 <= uchar && uchar <= 0x2F) {
            m_seq.append(uchar);
          } else {
            process_invalid_sequence();
          }
        } else {
          if (0x20 <= uchar && uchar <= 0x3F) {
            if (uchar <= 0x2F)
              m_seq.start_intermediate();
            m_seq.append(uchar);
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
        m_seq.append(uchar);
      } else if (uchar == ascii_esc) {
        m_hasPendingESC = true;
      } else if ((uchar == ascii_st && m_state->c1_8bit_representation_enabled)
        || (uchar == ascii_bel && m_seq.type() == ascii_osc && m_state->osc_sequence_terminated_by_bel)
      ) {
        process_command_string();
      } else {
        process_invalid_sequence();
        process_char(uchar);
      }
    }

    void process_char_for_character_string(char32_t uchar) {
      if (m_hasPendingESC) {
        if (uchar == ascii_backslash || (uchar == ascii_st && m_state->c1_8bit_representation_enabled)) {
          // final ST
          if (uchar == ascii_st)
            m_seq.append(ascii_esc);
          process_character_string();
          return;
        } else if (uchar == ascii_X || (uchar == ascii_sos && m_state->c1_8bit_representation_enabled)) {
          // invalid SOS
          process_invalid_sequence();
          if (uchar == ascii_X)
            process_char(ascii_esc);
          process_char(uchar);
          return;
        } else if (uchar == ascii_esc) {
          m_seq.append(ascii_esc);
          m_hasPendingESC = false;
        }
      }

      if (uchar == ascii_esc)
        m_hasPendingESC = true;
      else if (uchar == ascii_st && m_state->c1_8bit_representation_enabled) {
        process_character_string();
      } else if (uchar == ascii_sos && m_state->c1_8bit_representation_enabled) {
        process_invalid_sequence();
        process_char(uchar);
      } else
        m_seq.append(uchar);
    }

    void process_char_default_c1(char32_t c1char) {
      switch (c1char) {
      case ascii_csi:
        m_seq.set_type((byte) c1char);
        m_dstate = decode_control_sequence;
        break;
      case ascii_sos:
        m_seq.set_type((byte) c1char);
        m_dstate = decode_character_string;
        break;
      case ascii_dcs:
      case ascii_osc:
      case ascii_pm:
      case ascii_apc:
        m_seq.set_type((byte) c1char);
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
        if (m_state->c1_8bit_representation_enabled)
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

  typedef std::uint32_t csi_single_param_t;

  struct csi_param_type {
    csi_single_param_t value;
    bool isColon;
    bool isDefault;
  };

  class csi_parameters {
    std::vector<csi_param_type> m_data;
    std::size_t m_index {0};
    bool m_result;

  public:
    csi_parameters(sequence const& seq) {
      extract_parameters(seq.parameter(), seq.parameterSize());
    }

    operator bool() const {return this->m_result;}

  public:
    std::size_t size() const {return m_data.size();}
    void push_back(csi_param_type const& value) {m_data.push_back(value);}

  private:
    bool extract_parameters(char32_t const* str, std::size_t len) {
      m_data.clear();
      m_index = 0;
      m_isColonAppeared = false;
      m_result = false;

      bool isSet = false;
      csi_param_type param {0, false, true};
      for (std::size_t i = 0; i < len; i++) {
        char32_t const c = str[i];
        if (!(ascii_0 <= c && c <= ascii_semicolon)) {
          std::fprintf(stderr, "invalid value of CSI parameter values.\n");
          return false;
        }

        if (c <= ascii_9) {
          std::uint32_t const newValue = param.value * 10 + (c - ascii_0);
          if (newValue < param.value) {
            // overflow
            std::fprintf(stderr, "a CSI parameter value is too large.\n");
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
      return m_result = true;
    }

  public:
    bool m_isColonAppeared;

    bool read_param(csi_single_param_t& result, std::uint32_t defaultValue) {
      while (m_index < m_data.size()) {
        csi_param_type const& param = m_data[m_index++];
        if (!param.isColon) {
          m_isColonAppeared = false;
          if (!param.isDefault)
            result = param.value;
          else
            result = defaultValue;
          return true;
        }
      }
      result = defaultValue;
      return false;
    }

    bool read_arg(csi_single_param_t& result, bool toAllowSemicolon, csi_single_param_t defaultValue) {
      if (m_index <m_data.size()
        && (m_data[m_index].isColon || (toAllowSemicolon && !m_isColonAppeared))
      ) {
        csi_param_type const& param = m_data[m_index++];
        if (param.isColon) m_isColonAppeared = true;
        if (!param.isDefault)
          result = param.value;
        else
          result = defaultValue;
        return true;
      }

      result = defaultValue;
      return false;
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
        x = m_board->line(m_board->cur.y)->to_presentation_position(x, m_state.presentation_direction);

      if (m_board->cur.y + 1 < m_board->m_height)
        m_board->cur.y++;
      else if (toAppendNewLine) {
        // ToDo: 消えてなくなる行を記録する?
        m_board->rotate();
        m_board->clear_line(m_board->cur.y);
      } else
        return;

      if (toAdjustXAsPresentationPosition)
        x = m_board->line(m_board->cur.y)->to_data_position(x, m_state.presentation_direction);
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
        if (m_state.ff_using_home_position) {
          curpos_t x = m_board->cur.x;
          curpos_t y = m_board->cur.y;
          if (!toCallCR)
            x = m_board->line(y)->to_presentation_position(x, m_state.presentation_direction);

          m_board->clear_screen();
          y = m_state.page_home_position;

          if (!toCallCR)
            x = m_board->line(y)->to_data_position(x, m_state.presentation_direction);

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
        x = line->to_data_position(x, m_state.presentation_direction);

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
    decoder_type m_seqdecoder {this, &this->m_state};
    friend decoder_type;

    void process_invalid_sequence(sequence const& seq) {
      // ToDo: 何処かにログ出力
      mwg_printd("%p", &seq);
    }

    void process_control_sequence(sequence const& seq);

    void process_command_string(sequence const& seq) {
      mwg_printd("%p", &seq);
    }
    void process_character_string(sequence const& seq) {
      mwg_printd("%p", &seq);
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
  };

}
#endif
