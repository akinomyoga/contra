// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef contra_sequence_hpp
#define contra_sequence_hpp
#include "contradef.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <mwg/except.h>
#include "enc.utf8.hpp"

namespace contra {

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
      return parameter_size() != 0 && ascii_less <= m_content[0] && m_content[0] <= ascii_question;
    }

    char32_t const* content() const {
      return &m_content[0];
    }
    std::int32_t content_size() const {
      return m_content.size();
    }

    char32_t const* parameter() const {
      return &m_content[0];
    }
    std::int32_t parameter_size() const {
      return m_intermediateStart < 0 ? m_content.size() : m_intermediateStart;
    }
    char32_t const* intermediate() const {
      return m_intermediateStart < 0 ? nullptr : &m_content[m_intermediateStart];
    }
    std::int32_t intermediate_size() const {
      return m_intermediateStart < 0 ? 0: m_content.size() - m_intermediateStart;
    }

  private:
    void print_char(std::FILE* file, char32_t ch) const {
      const char* name = get_ascii_name(ch);
      if (name)
        std::fprintf(file, "%s", name);
      else
        contra::encoding::put_u8(ch, file);
    }

  public:
    void print(std::FILE* file) const {
      switch (m_type) {
      case ascii_csi:
      case ascii_esc:
        print_char(file, m_type);
        std::putc(' ', file);
        for (char32_t ch: m_content) {
          print_char(file, ch);
          std::putc(' ', file);
        }
        print_char(file, m_final);
        break;
      default: // command string & characater string
        print_char(file, m_type);
        std::putc(' ', file);
        for (char32_t ch: m_content) {
          print_char(file, ch);
          std::putc(' ', file);
        }
        std::fprintf(file, "ST");
        break;
      }
    }
  };

  struct sequence_decoder_config {
    bool c1_8bit_representation_enabled     = true; // 8bit C1 制御文字を有効にする。
    bool osc_sequence_terminated_by_bel     = true; // OSC シーケンスが BEL で終わっても良い。
    bool command_string_terminated_by_bel   = true; // 任意のコマンド列が BEL で終わっても良い。
    bool character_string_terminated_by_bel = true; // キャラクター列 (SOS, ESC k 等) が BEL で終わっても良い。
    bool title_definition_string_enabled    = true; // GNU Screen ESC k ... ST
  };

  template<typename Processor>
  class sequence_decoder {
    typedef Processor processor_type;

    enum decode_state {
      decode_default,
      decode_escape_sequence,
      decode_control_sequence,
      decode_command_string,
      decode_character_string,
    };

    processor_type* m_proc;
    sequence_decoder_config* m_config;
    decode_state m_dstate {decode_default};
    bool m_hasPendingESC {false};
    sequence m_seq;

  public:
    sequence_decoder(Processor* proc, sequence_decoder_config* config): m_proc(proc), m_config(config) {}

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

    void process_escape_sequence() {
      m_proc->process_escape_sequence(this->m_seq);
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

    void process_char_for_escape_sequence(char32_t uchar) {
      if (0x30 <= uchar && uchar <= 0x7E) {
        m_seq.set_final((byte) uchar);
        process_escape_sequence();
      } else if (0x20 <= uchar && uchar <= 0x2F) {
        m_seq.append(uchar);
      } else {
        process_invalid_sequence();
        decode_char(uchar);
      }
    }

    void process_char_for_control_sequence(char32_t uchar) {
      if (0x40 <= uchar && uchar <= 0x7E) {
        m_seq.set_final((byte) uchar);
        process_control_sequence();
      } else {
        if (m_seq.has_intermediate()) {
          if (0x20 <= uchar && uchar <= 0x2F) {
            m_seq.append(uchar);
            return;
          }
        } else {
          if (0x20 <= uchar && uchar <= 0x3F) {
            if (uchar <= 0x2F)
              m_seq.start_intermediate();
            m_seq.append(uchar);
            return;
          }
        }

        // Note: 事情はよく分からないが vttest は HT, VT, CR をその場で処理する事を要求する。
        //   RLogin はエスケープシーケンス以外の C0 を全てその場で処理している気がする。
        //   SI, SO に関しても文字コード制御の効果を持っている。
        if (0 <= uchar && uchar < 0x20 && uchar != ascii_esc) {
          m_proc->process_control_character(uchar);
          return;
        }

        m_seq.set_final(uchar);
        process_invalid_sequence();
        decode_char(uchar);
      }
    }

    bool is_command_string_terminator(char32_t uchar) const {
      if (uchar == ascii_st)
        return m_config->c1_8bit_representation_enabled;
      else if (uchar == ascii_bel) {
        if (m_config->command_string_terminated_by_bel) return true;
        if (m_seq.type() == ascii_osc) return m_config->osc_sequence_terminated_by_bel;
        return false;
      } else
        return false;
    }

    void process_char_for_command_string(char32_t uchar) {
      if (m_hasPendingESC) {
        if (uchar == ascii_backslash) {
          process_command_string();
        } else {
          process_invalid_sequence();
          decode_char(ascii_esc);
          decode_char(uchar);
        }

        return;
      }

      if ((0x08 <= uchar && uchar <= 0x0D) || (0x20 <= uchar && uchar <= 0x7E)) {
        m_seq.append(uchar);
      } else if (uchar == ascii_esc) {
        m_hasPendingESC = true;
      } else if (is_command_string_terminator(uchar)) {
        process_command_string();
      } else {
        process_invalid_sequence();
        decode_char(uchar);
      }
    }

    bool is_character_string_terminator(char32_t uchar) const {
      if (uchar == ascii_st)
        return m_config->c1_8bit_representation_enabled;
      else if (uchar == ascii_bel)
        return m_config->character_string_terminated_by_bel;
      else
        return false;
    }

    void process_char_for_character_string(char32_t uchar) {
      if (m_hasPendingESC) {
        if (uchar == ascii_backslash || is_character_string_terminator(uchar)) {
          // final ST
          if (uchar == ascii_st)
            m_seq.append(ascii_esc);
          process_character_string();
          return;
        } else if (m_seq.type() == ascii_sos &&
          (uchar == ascii_X || (uchar == ascii_sos && m_config->c1_8bit_representation_enabled))) {
          // invalid SOS
          process_invalid_sequence();
          if (uchar == ascii_X)
            decode_char(ascii_esc);
          decode_char(uchar);
          return;
        } else if (uchar == ascii_esc) {
          m_seq.append(ascii_esc);
          m_hasPendingESC = false;
        }
      }

      if (uchar == ascii_esc) {
        m_hasPendingESC = true;
      } else if (is_character_string_terminator(uchar)) {
        process_character_string();
      } else if (uchar == ascii_sos && m_config->c1_8bit_representation_enabled) {
        process_invalid_sequence();
        decode_char(uchar);
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
        m_proc->process_control_character(c1char);
        break;
      }
    }

    void process_char_default(char32_t uchar) {
      if (m_hasPendingESC) {
        m_hasPendingESC = false;
        if (0x20 <= uchar && uchar < 0x7E) {
          if (uchar < 0x30) {
            // <I> to start an escape sequence
            m_seq.set_type((byte) ascii_esc);
            m_seq.append((byte) uchar);
            m_dstate = decode_escape_sequence;
          } else if (0x40 <= uchar && uchar < 0x60) {
            // <F> to call C1
            process_char_default_c1((uchar & 0x1F) | 0x80);
          } else if (uchar == ascii_k && m_config->title_definition_string_enabled) {
            m_seq.set_type((byte) ascii_k);
            m_dstate = decode_character_string;
          } else {
            // <F>/<P> to complete an escape sequence
            m_seq.set_type((byte) ascii_esc);
            m_seq.set_final((byte) uchar);
            process_escape_sequence();
          }
        } else {
          process_invalid_sequence();
        }
        return;
      }

      if (0 <= uchar && uchar < 0x20) {
        if (uchar == ascii_esc)
          m_hasPendingESC = true;
        else
          m_proc->process_control_character(uchar);
      } else if (0x80 <= uchar && uchar < 0xA0) {
        if (m_config->c1_8bit_representation_enabled)
          process_char_default_c1(uchar);
      } else {
        m_proc->process_char(uchar);
      }
    }

  public:
    void decode_char(char32_t uchar) {
      if (uchar == ascii_nul || uchar == ascii_del) return;
      switch (m_dstate) {
      case decode_default:
        process_char_default(uchar);
        break;
      case decode_escape_sequence:
        process_char_for_escape_sequence(uchar);
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
    static constexpr bool is_ctrl(std::uint32_t uchar) {
      std::uint32_t u = uchar & ~(std::uint32_t) 0x80;
      return u < 0x20;
    }
    void decode(char32_t const* beg, char32_t const* end) {
      while (beg != end) {
        if (m_dstate == decode_default && !m_hasPendingESC && !is_ctrl(*beg)) {
          char32_t const* const batch_begin = beg;
          do beg++; while (beg != end && !is_ctrl(*beg));
          char32_t const* const batch_end = beg;
          m_proc->process_chars(batch_begin, batch_end);
        } else
          decode_char(*beg++);
      }
    }

    void process_end() {
      switch (m_dstate) {
      case decode_escape_sequence:
      case decode_control_sequence:
      case decode_command_string:
      case decode_character_string:
        process_invalid_sequence();
        break;
      default: break;
      }
    }
  };

  class sequence_printer: public idevice {
    const char* fname;
    std::FILE* const file;
  public:
    sequence_printer(std::FILE* file):
      fname(nullptr), file(file) {}
    sequence_printer(const char* fname):
      fname(fname), file(std::fopen(fname, "w")) {}
    ~sequence_printer() {if (fname) std::fclose(this->file);}

  private:
    sequence_decoder_config m_config; // ToDo: should be simpler instance

    typedef sequence_decoder<sequence_printer> decoder_type;
    decoder_type m_seqdecoder {this, &this->m_config};
    friend decoder_type;

    bool m_hasPrecedingChar {false};

    void print_sequence(sequence const& seq) {
      if (m_hasPrecedingChar) {
        m_hasPrecedingChar = false;
        std::fputc('\n', file);
      }

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
      case ascii_k: // screen TITLE DEFINITION STRING
        name = "private string";
        break;
      }

      std::fprintf(file, "%s: ", name);
      seq.print(file);
      std::fputc('\n', file);
      std::fflush(file);
    }

    void process_invalid_sequence(sequence const& seq) {
      // ToDo: 何処かにログ出力
      print_sequence(seq);
    }

    void process_escape_sequence(sequence const& seq) {
      print_sequence(seq);
    }

    void process_control_sequence(sequence const& seq) {
      print_sequence(seq);
    }

    void process_command_string(sequence const& seq) {
      print_sequence(seq);
    }
    void process_character_string(sequence const& seq) {
      print_sequence(seq);
    }

    void process_char(char32_t u) {
      if (m_hasPrecedingChar) {
        std::fputc(' ', file);
      } else
        m_hasPrecedingChar = true;

      if (u > 0xFF)
        std::fprintf(file, "U+%04X", (unsigned) u);
      else if (u == U' ')
        std::fprintf(file, "SP");
      else
        std::fprintf(file, "%c", (char) u);
    }
    void process_chars(char32_t const* beg, char32_t const* end) {
      while (beg < end) process_char(*beg++);
    }

    void process_control_character(char32_t uchar) {
      if (m_hasPrecedingChar) {
        std::fputc(' ', file);
      } else
        m_hasPrecedingChar = true;

      const char* name = get_ascii_name(uchar);
      if (name)
        std::fprintf(file, "%s", name);
      else
        std::fprintf(file, "%c", uchar);
    }

  public:
    void write(const char* data, std::size_t size) {
      for (std::size_t i = 0; i < size; i++)
        m_seqdecoder.decode_char(data[i]);
    }

  private:
    virtual void dev_write(char const* data, std::size_t size) override {
      this->write(data, size);
    }
  };

  //---------------------------------------------------------------------------
  // CSI parameters

  typedef std::uint32_t csi_single_param_t;

  class csi_parameters;

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
    csi_parameters(char32_t const* s, std::size_t n) { extract_parameters(s, n); }
    csi_parameters(sequence const& seq) {
      extract_parameters(seq.parameter(), seq.parameter_size());
    }

    // csi_parameters(std::initializer_list<csi_single_param_t> args) {
    //   for (csi_single_param_t value: args)
    //     this->push_back({value, false, false});
    // }

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

    void debug_print(std::FILE* file) const {
      bool is_first = true;
      for (auto const& entry : m_data) {
        if (entry.isColon)
          std::putc(':', file);
        else if (!is_first)
          std::putc(';', file);
        if (!entry.isDefault)
          std::fprintf(file, "%u", entry.value);
        is_first = false;
      }
    }
  };

  //---------------------------------------------------------------------------
  // input_decoder

  enum input_data_type {
    input_data_paste = 1,
  };

  template<typename Processor>
  class input_decoder {
    Processor* m_proc;

    // requires the following members:
    //   m_proc->process_key(u);
    //   m_proc->process_input_data(type, data);
    //   m_proc->process_invalid_sequence(seq);
    //   m_proc->process_control_sequence(seq);
    //
    // ToDo: use TRIE?
    // ToDo: use terminfo?

  public:
    input_decoder(Processor* proc): m_proc(proc) {}

  private:
    enum decode_state {
      decode_default = 0,
      decode_esc,
      decode_csi,
      decode_paste_default,
      decode_paste_esc,
      decode_paste_csi,
      decode_paste_csi2,
      decode_paste_csi20,
      decode_paste_csi201,
    };

    int m_state = 0;
    sequence m_seq;
    bool has_pending_esc = false;
    int m_paste_suffix = 0;
    std::u32string m_paste_data;
  public:
    void decode(char32_t const* beg, char32_t const* end) {
      while (beg != end) decode_char(*beg++);
    }
    void decode_char(char32_t u) {
      switch (m_state) {
      case decode_default:
        if (u == ascii_esc) {
          m_state = decode_esc;
        } else if (u == ascii_csi) {
          m_state = decode_csi;
          m_seq.set_type(ascii_csi);
        } else {
          process_key(u & _character_mask);
        }
        break;
      case decode_esc:
        if (u == ascii_left_bracket) {
          m_state = decode_csi;
          m_seq.set_type(ascii_csi);
        } else {
          process_key(ascii_esc);
          m_state = decode_default;
          decode_char(u);
        }
        break;
      case decode_csi:
        if (0x40 <= u && u <= 0x7E) {
          m_seq.set_final((byte) u);
          process_control_sequence();
          m_state = decode_default;
          m_seq.clear();
        } else if (0x20 <= u && u <= 0x2F) {
          if (!m_seq.has_intermediate())
            m_seq.start_intermediate();
          m_seq.append(u);
        } else if (0x30 <= u  && u <= 0x3F) {
          if (m_seq.has_intermediate()) {
            m_seq.set_final((byte) u);
            process_invalid_sequence();
            m_state = decode_default;
            m_seq.clear();
            decode_char(u);
          } else {
            m_seq.append(u);
          }
        } else {
          m_seq.set_final((byte) u);
          process_invalid_sequence();
          m_state = decode_default;
          m_seq.clear();
          decode_char(u);
        }
        break;
      case decode_paste_default:
        m_paste_data += u;
        if (u == ascii_esc) {
          m_state = decode_paste_esc;
          m_paste_suffix = 1;
        } else if (u == ascii_csi) {
          m_state = decode_paste_csi;
          m_paste_suffix = 1;
        }
        break;
      case decode_paste_esc:
        m_paste_data += u;
        m_state = u == ascii_left_bracket ? decode_paste_csi : decode_paste_default;
        m_paste_suffix++;
        break;
      case decode_paste_csi:
        m_paste_data += u;
        m_state = u == ascii_2 ? decode_paste_csi2 : decode_paste_default;
        m_paste_suffix++;
        break;
      case decode_paste_csi2:
        m_paste_data += u;
        m_state = u == ascii_0 ? decode_paste_csi20 : decode_paste_default;
        m_paste_suffix++;
        break;
      case decode_paste_csi20:
        m_paste_data += u;
        m_state = u == ascii_1 ? decode_paste_csi201 : decode_paste_default;
        m_paste_suffix++;
        break;
      case decode_paste_csi201:
        m_paste_data += u;
        m_state = u == ascii_1 ? decode_paste_csi201 : decode_paste_default;
        m_paste_suffix++;
        if (u == ascii_tilde) {
          m_paste_data.erase(m_paste_data.end() - m_paste_suffix, m_paste_data.end());
          process_input_data(input_data_paste, m_paste_data);
        }
        break;
      default:
        mwg_assert(false, "BUG unexpected");
        m_state = decode_default;
        process_key(u & _character_mask);
        break;
      }
    }

  private:
    void process_key(std::uint32_t u) {
      if (u == ascii_esc && !has_pending_esc) {
        has_pending_esc = true;
        return;
      }

      if (u < 32) {
        if (1 <= u && u <= 26)
          u |= 0x60 | modifier_control;
        else
          u |= 0x40 | modifier_control;
      }

      if (has_pending_esc) {
        has_pending_esc = false;
        if (u & modifier_meta)
          process_key(ascii_esc);
        else
          u |= modifier_meta;
      }

      m_proc->process_key(u);
    }

    void process_input_data(input_data_type type, std::u32string const& data) {
      m_proc->process_input_data(type, data);
    }

    void process_invalid_sequence() {
      m_proc->process_invalid_sequence(m_seq);
    }

    void process_control_sequence() {
      if (!process_csi_key())
        m_proc->process_control_sequence(m_seq);
    }

    bool process_csi_key() {
      csi_parameters params(m_seq);
      if (!params) return false;

      key_t key = 0;
      switch (m_seq.final()) {
      case ascii_tilde:
        {
          csi_single_param_t param1;
          params.read_param(param1, 0);

          switch (param1) {
          case 1: key = key_home; goto modified_key;
          case 2: key = key_insert; goto modified_key;
          case 3: key = key_delete; goto modified_key;
          case 4: key = key_end; goto modified_key;
          case 5: key = key_prior; goto modified_key;
          case 6: key = key_next; goto modified_key;
          case 7: key = key_home; goto modified_key;
          case 8: key = key_end; goto modified_key;
          case 11: key = key_f1; goto modified_key;
          case 12: key = key_f2; goto modified_key;
          case 13: key = key_f3; goto modified_key;
          case 14: key = key_f4; goto modified_key;
          case 15: key = key_f5; goto modified_key;
          case 17: key = key_f6; goto modified_key;
          case 18: key = key_f7; goto modified_key;
          case 19: key = key_f8; goto modified_key;
          case 20: key = key_f9; goto modified_key;
          case 21: key = key_f10; goto modified_key;
          case 23: key = key_f11; goto modified_key;
          case 24: key = key_f12; goto modified_key;
          case 25: key = key_f13; goto modified_key;
          case 26: key = key_f14; goto modified_key;
          case 28: key = key_f15; goto modified_key;
          case 29: key = key_f16; goto modified_key;
          case 31: key = key_f17; goto modified_key;
          case 32: key = key_f18; goto modified_key;
          case 33: key = key_f19; goto modified_key;
          case 34: key = key_f20; goto modified_key;

          case 27: goto modified_code;
          case 200: goto paste_begin; // paste_begin
            // case 201: // paste_end
          }
        }
        break;
      case ascii_A: key = key_up; goto modified_alpha;
      case ascii_B: key = key_down; goto modified_alpha;
      case ascii_C: key = key_right; goto modified_alpha;
      case ascii_D: key = key_left; goto modified_alpha;
      case ascii_E: key = key_begin; goto modified_alpha;
      case ascii_F: key = key_end; goto modified_alpha;
      case ascii_H: key = key_home; goto modified_alpha;
      case ascii_M: key = key_kpent; goto modified_alpha;
      case ascii_P: key = key_kpf1; goto modified_alpha;
      case ascii_Q: key = key_kpf2; goto modified_alpha;
      case ascii_R: key = key_kpf3; goto modified_alpha;
      case ascii_S: key = key_kpf4; goto modified_alpha;
      case ascii_j: key = key_kpmul; goto modified_alpha;
      case ascii_k: key = key_kpadd; goto modified_alpha;
      case ascii_l: key = key_kpsep; goto modified_alpha;
      case ascii_m: key = key_kpsub; goto modified_alpha;
      case ascii_n: key = key_kpdec; goto modified_alpha;
      case ascii_o: key = key_kpdiv; goto modified_alpha;
      case ascii_p: key = key_kp0; goto modified_alpha;
      case ascii_q: key = key_kp1; goto modified_alpha;
      case ascii_r: key = key_kp2; goto modified_alpha;
      case ascii_s: key = key_kp3; goto modified_alpha;
      case ascii_t: key = key_kp4; goto modified_alpha;
      case ascii_u: key = key_kp5; goto modified_unicode;
      case ascii_v: key = key_kp6; goto modified_alpha;
      case ascii_w: key = key_kp7; goto modified_alpha;
      case ascii_x: key = key_kp8; goto modified_alpha;
      case ascii_y: key = key_kp9; goto modified_alpha;
      case ascii_X: key = key_kpeq; goto modified_alpha;

      paste_begin:
        if (params.size() == 1) {
          m_state = decode_paste_default;
          m_paste_data.clear();
          return true;
        }
        break;
      modified_code:
        {
          csi_single_param_t param2, param3;
          params.read_param(param2, 0);
          params.read_param(param3, 0);
          key = param3 & _character_mask;
          if (param2)
            key |= (param2 - 1) << _modifier_shft & _modifier_mask;
          process_key(key);
          return true;
        }
      modified_unicode:
        {
          csi_single_param_t param1;
          params.read_param(param1, 1);
          if (param1 != 1) key = param1 & _character_mask;
          goto modified_key;
        }
      modified_alpha:
        {
          csi_single_param_t param1;
          params.read_param(param1, 1);
          if (param1 == 1) goto modified_key;
          break;
        }
      modified_key:
        {
          csi_single_param_t param2;
          params.read_param(param2, 0);
          if (param2)
            key |= (param2 - 1) << _modifier_shft & _modifier_mask;
          process_key(key);
          return true;
        }
      }
      return false;
    }

  };

}
#endif
