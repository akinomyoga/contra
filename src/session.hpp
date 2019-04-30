// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef contra_session_hpp
#define contra_session_hpp
#include <cstdint>
#include <cstdio>
#include <sstream>
#include <iterator>
#include <memory>
#include "ansi/line.hpp"
#include "ansi/term.hpp"
#include "enc.utf8.hpp"
#include "pty.hpp"

namespace contra {
namespace term {

  // contra/ttty
  enum key_flags {
    mask_unicode   = 0x0001FFFF,
    mask_keycode   = 0x0003FFFF,

    _modifier_mask = 0x3F000000,
    _modifier_shft = 24,

    key_base      = 0x00020000,
    key_f1        = key_base | 1,
    key_f2        = key_base | 2,
    key_f3        = key_base | 3,
    key_f4        = key_base | 4,
    key_f5        = key_base | 5,
    key_f6        = key_base | 6,
    key_f7        = key_base | 7,
    key_f8        = key_base | 8,
    key_f9        = key_base | 9,
    key_f10       = key_base | 10,
    key_f11       = key_base | 11,
    key_f12       = key_base | 12,
    key_f13       = key_base | 13,
    key_f14       = key_base | 14,
    key_f15       = key_base | 15,
    key_f16       = key_base | 16,
    key_f17       = key_base | 17,
    key_f18       = key_base | 18,
    key_f19       = key_base | 19,
    key_f20       = key_base | 20,
    key_f21       = key_base | 21,
    key_f22       = key_base | 22,
    key_f23       = key_base | 23,
    key_f24       = key_base | 24,
    key_insert    = key_base | 31,
    key_delete    = key_base | 32,
    key_home      = key_base | 33,
    key_end       = key_base | 34,
    key_prior     = key_base | 35,
    key_next      = key_base | 36,
    key_begin     = key_base | 37,
    key_left      = key_base | 38,
    key_right     = key_base | 39,
    key_up        = key_base | 40,
    key_down      = key_base | 41,
    key_kp0       = key_base | 42,
    key_kp1       = key_base | 43,
    key_kp2       = key_base | 44,
    key_kp3       = key_base | 45,
    key_kp4       = key_base | 46,
    key_kp5       = key_base | 47,
    key_kp6       = key_base | 48,
    key_kp7       = key_base | 49,
    key_kp8       = key_base | 50,
    key_kp9       = key_base | 51,
    key_kpdec     = key_base | 52,
    key_kpsep     = key_base | 53,
    key_kpmul     = key_base | 54,
    key_kpadd     = key_base | 55,
    key_kpsub     = key_base | 56,
    key_kpdiv     = key_base | 57,

    modifier_shift       = 0x01000000,
    modifier_meta        = 0x02000000,
    modifier_control     = 0x04000000,
    modifier_super       = 0x08000000,
    modifier_hyper       = 0x10000000,
    modifier_alter       = 0x20000000,
  };

  void print_key(std::uint32_t key, std::FILE* file) {
    std::uint32_t const code = key & mask_keycode;

    std::ostringstream str;
    if (key & modifier_alter) str << "A-";
    if (key & modifier_hyper) str << "H-";
    if (key & modifier_super) str << "s-";
    if (key & modifier_control) str << "C-";
    if (key & modifier_meta) str << "M-";
    if (key & modifier_shift) str << "S-";
    std::fprintf(file, "key: %s", str.str().c_str());

    if (code < key_base) {
      switch (code) {
      case ascii_bs: std::fprintf(file, "BS"); break;
      case ascii_ht: std::fprintf(file, "TAB"); break;
      case ascii_cr: std::fprintf(file, "RET"); break;
      case ascii_esc: std::fprintf(file, "ESC"); break;
      case ascii_sp: std::fprintf(file, "SP"); break;
      default:
        contra::encoding::put_u8((char32_t) code, file);
        break;
      }
      std::putc('\n', file);
    } else {
      static const char* table[] = {
        NULL,
        "f1", "f2", "f3", "f4", "f5", "f6",
        "f7", "f8", "f9", "f10", "f11", "f12",
        "f13", "f14", "f15", "f16", "f17", "f18",
        "f19", "f20", "f21", "f22", "f23", "f24",
        NULL, NULL, NULL, NULL, NULL, NULL,
        "insert", "delete", "home", "end", "prior", "next",
        "begin", "left", "right", "up", "down",
        "kp0", "kp1", "kp2", "kp3", "kp4",
        "kp5", "kp6", "kp7", "kp8", "kp9",
        "kpdec", "kpsep", "kpmul", "kpadd", "kpsub", "kpdiv",
      };
      std::uint32_t const index = code - key_base;
      const char* name = index <= std::size(table) ? table[index] : NULL;
      if (name) {
        std::fprintf(file, "%s\n", name);
      } else {
        std::fprintf(file, "unknown %08x\n", code);
      }
    }
  }

  struct terminal_session {
  public:
    struct winsize init_ws;
    struct termios* init_termios = NULL;
    std::size_t init_read_buffer_size = 4096;

  private:
    contra::term::pty_session m_pty; // ユーザ入力書込先
    contra::multicast_device m_dev;  // 受信データ書込先
    std::unique_ptr<contra::ansi::board_t> m_board;
    std::unique_ptr<contra::ansi::term_t> m_term;

  public:
    contra::ansi::board_t& board() { return *m_board; }
    contra::ansi::board_t const& board() const { return *m_board; }
    contra::ansi::term_t& term() { return *m_term; }
    contra::ansi::term_t const& term() const { return *m_term; }

    contra::idevice& input_device() { return m_pty; }
    contra::multicast_device& output_device() { return m_dev; }

  public:
    bool is_active() const { return m_pty.is_active(); }
    bool initialize() {
      if (m_pty.is_active()) return true;

      m_pty.set_read_buffer_size(init_read_buffer_size);
      if (!m_pty.start("/bin/bash", &init_ws, init_termios)) return false;

      m_board = std::make_unique<contra::ansi::board_t>(init_ws.ws_col, init_ws.ws_row);
      m_term = std::make_unique<contra::ansi::term_t>(*m_board);
      m_term->set_response_target(m_pty);
      m_dev.push(m_term.get());
      return true;
    }
    bool process() { return m_pty.read(&m_dev); }
    bool is_alive() { return m_pty.is_alive(); }
    void terminate() { return m_pty.terminate(); }

  private:
    std::vector<byte> input_buffer;
    void put_flush() {
      m_pty.write(reinterpret_cast<char const*>(&input_buffer[0]), input_buffer.size());
      input_buffer.clear();
    }
    void put(byte value) {
      input_buffer.push_back(value);
    }
    void put_c1(std::uint32_t code) {
      if (term().state().get_mode(ansi::mode_s7c1t)) {
        input_buffer.push_back(ascii_esc);
        input_buffer.push_back(code - 0x40);
      } else {
        contra::encoding::put_u8(code, input_buffer);
      }
    }
    void put_unsigned(std::uint32_t value) {
      if (value >= 10) put_unsigned(value / 10);
      input_buffer.push_back(ascii_0 + value % 10);
    }
    void put_modifier(std::uint32_t mod) {
      put_unsigned(1 + ((mod & _modifier_mask) >> _modifier_shft));
    }

  public:
    void send_key(std::uint32_t key) {
      std::uint32_t mod = key & _modifier_mask;
      std::uint32_t code = key & mask_keycode;

      // テンキーの文字 (!DECNKM の時)
      if (!term().state().get_mode(ansi::mode_decnkm)) {
        switch (code) {
        case key_kp0  : code = ascii_0; break;
        case key_kp1  : code = ascii_1; break;
        case key_kp2  : code = ascii_2; break;
        case key_kp3  : code = ascii_3; break;
        case key_kp4  : code = ascii_4; break;
        case key_kp5  : code = ascii_5; break;
        case key_kp6  : code = ascii_6; break;
        case key_kp7  : code = ascii_7; break;
        case key_kp8  : code = ascii_8; break;
        case key_kp9  : code = ascii_9; break;
        case key_kpdec: code = ascii_dot     ; break;
        case key_kpsep: code = ascii_comma   ; break;
        case key_kpmul: code = ascii_asterisk; break;
        case key_kpadd: code = ascii_plus    ; break;
        case key_kpsub: code = ascii_minus   ; break;
        case key_kpdiv: code = ascii_slash   ; break;
        default: ;
        }
      }

      // 通常文字
      if (mod == 0 && code < key_base) {
        // Note: ESC, RET, HT はそのまま (C-[, C-m, C-i) 送信される。
        if (code == ascii_bs) code = ascii_del;
        contra::encoding::put_u8(code, input_buffer);
        put_flush();
        return;
      }

      // C0 文字および DEL
      if (mod == modifier_control) {
        if (code == ascii_sp || code == ascii_a ||
          (ascii_a < code && code <= ascii_z) ||
          (ascii_left_bracket <= code && code <= ascii_underscore)
        ) {
          // C-@ ... C-_
          put(code & 0x1F);
          put_flush();
          return;
        } else if (code == ascii_question) {
          // C-? → ^?
          put(0x7F);
          put_flush();
          return;
        } else if (code == ascii_bs) {
          // C-back → ^_
          put(0x1F);
          put_flush();
          return;
        }
      }

      // BS CR(RET) HT(TAB) ESC
      // CSI <Unicode> ; <Modifier> u 形式
      if (code < key_base) {
        put_c1(ascii_csi);
        put_unsigned(code);
        if (mod) {
          put(ascii_semicolon);
          put_modifier(mod);
        }
        put(ascii_u);
        put_flush();
        return;
      }

      // application key mode の時修飾なしの関数キーは \eOA 等にする。
      char a = 0;
      switch (code) {
      case key_f1 : a = 11; goto tilde;
      case key_f2 : a = 12; goto tilde;
      case key_f3 : a = 13; goto tilde;
      case key_f4 : a = 14; goto tilde;
      case key_f5 : a = 15; goto tilde;
      case key_f6 : a = 17; goto tilde;
      case key_f7 : a = 18; goto tilde;
      case key_f8 : a = 19; goto tilde;
      case key_f9 : a = 20; goto tilde;
      case key_f10: a = 21; goto tilde;
      case key_f11: a = 23; goto tilde;
      case key_f12: a = 24; goto tilde;
      case key_f13: a = 25; goto tilde;
      case key_f14: a = 26; goto tilde;
      case key_f15: a = 28; goto tilde;
      case key_f16: a = 29; goto tilde;
      case key_f17: a = 31; goto tilde;
      case key_f18: a = 32; goto tilde;
      case key_f19: a = 33; goto tilde;
      case key_f20: a = 34; goto tilde;
      case key_f21: a = 36; goto tilde;
      case key_f22: a = 37; goto tilde;
      case key_f23: a = 38; goto tilde;
      case key_f24: a = 39; goto tilde;
      case key_home  : a = 1; goto tilde;
      case key_insert: a = 2; goto tilde;
      case key_delete: a = 3; goto tilde;
      case key_end   : a = 4; goto tilde;
      case key_prior : a = 5; goto tilde;
      case key_next  : a = 6; goto tilde;
      tilde:
        put_c1(ascii_csi);
        put_unsigned(a);
        if (mod) {
          put(ascii_semicolon);
          put_modifier(mod);
        }
        put(ascii_tilde);
        put_flush();
        return;
      case key_up   : a = ascii_A; goto alpha;
      case key_down : a = ascii_B; goto alpha;
      case key_right: a = ascii_C; goto alpha;
      case key_left : a = ascii_D; goto alpha;
      case key_begin: a = ascii_E; goto alpha;
      alpha:
        if (mod) {
          put_c1(ascii_csi);
          put(ascii_1);
          put(ascii_semicolon);
          put_modifier(mod);
        } else {
          put_c1(term().state().get_mode(ansi::mode_decckm) ? ascii_ss3 : ascii_csi);
        }
        put(a);
        put_flush();
        return;
      default: break;
      }
    }
  };
}
}
#endif
