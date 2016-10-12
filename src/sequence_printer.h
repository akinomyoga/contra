// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef CONTRA_SEQUENCE_PRINTER_H
#define CONTRA_SEQUENCE_PRINTER_H
#include "board.h"
#include "tty_player.h"
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <iterator>

namespace contra {

  class sequence_printer {
    const char* fname;
    std::FILE* const file;
  public:
    sequence_printer(std::FILE* file):
      fname(nullptr), file(file) {}
    sequence_printer(const char* fname):
      fname(fname), file(std::fopen(fname, "w")) {}
    ~sequence_printer() {if (fname) std::fclose(this->file);}

  private:
    tty_state m_state; // ToDo: should be simpler instance

    typedef sequence_decoder<sequence_printer> decoder_type;
    decoder_type m_seqdecoder {this, &this->m_state};
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
      }

      std::fprintf(file, "%s: ", name);
      seq.print(file);
      std::fputc('\n', file);
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
  };
}
#endif
