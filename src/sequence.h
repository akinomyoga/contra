// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef CONTRA_SEQUENCE_H
#define CONTRA_SEQUENCE_H
#include "contradef.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <mwg/except.h>
#include "ansi/enc.utf8.hpp"

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
        process_char(uchar);
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
        process_char(uchar);
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
          process_char(ascii_esc);
          process_char(uchar);
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
        process_char(uchar);
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
            process_char(ascii_esc);
          process_char(uchar);
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
        m_proc->insert_char(uchar);
      }
    }

  public:
    void process_char(char32_t uchar) {
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

    void insert_char(char32_t u) {
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
        m_seqdecoder.process_char(data[i]);
    }

  private:
    virtual void dev_write(char const* data, std::size_t size) override {
      this->write(data, size);
    }
  };
}
#endif
