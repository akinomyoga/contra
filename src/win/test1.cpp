#include <string.h>
#include <tchar.h>
#include <wchar.h>
#include <windows.h>
#include <imm.h>

#include <sys/ioctl.h>

#include <cstddef>
#include <cstdint>
#include <vector>
#include <sstream>
#include <iomanip>
#include <memory>
#include <functional>
#include <mwg/except.h>
#include "../contradef.h"
#include "../ansi/enc.utf8.hpp"
#include "../ansi/util.hpp"
#include "../impl.hpp"
#include "win_messages.hpp"

#define _tcslen wcslen
#define _tcscpy_s wcscpy_s
#define _tcscpy wcscpy

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
}
}

namespace contra {
namespace twin {

  using namespace contra::term;

  struct terminal_session {
  public:
    struct winsize ws;
    std::size_t read_buffer_size;

  private:
    bool m_initialized = false;
    contra::session m_fds;
    std::unique_ptr<contra::ansi::board_t> m_board;
    std::unique_ptr<contra::ansi::term_t> m_term;
    std::unique_ptr<contra::tty_player_device> m_dev;

  public:
    contra::ansi::board_t& board() { return *m_board; }
    contra::ansi::board_t const& board() const { return *m_board; }
    contra::ansi::term_t& term() { return *m_term; }
    contra::ansi::term_t const& term() const { return *m_term; }

  public:
    bool is_initialized() const { return m_initialized; }

    bool initialize() {
      if (m_initialized) return true;

      // restrict in the range from 256 bytes to 64 kbytes
      read_buffer_size = contra::clamp(read_buffer_size, 0x100, 0x10000);
      m_read_buffer.resize(read_buffer_size);

      if (!contra::create_session(&m_fds, "/bin/bash", &ws)) return false;
      m_board = std::make_unique<contra::ansi::board_t>(ws.ws_col, ws.ws_row);
      m_term = std::make_unique<contra::ansi::term_t>(*m_board);
      m_dev = std::make_unique<contra::tty_player_device>(m_term.get());

      return m_initialized = true;
    }

    std::vector<char> m_read_buffer;
    bool process() {
      if (!m_initialized) return false;
      return contra::read_from_fd(m_fds.masterfd, m_dev.get(), &m_read_buffer[0], m_read_buffer.size());
    }
    bool is_child_alive() const {
      if (!m_initialized) return false;
      return !contra::is_child_terminated(m_fds.pid);
    }

    void terminate() {
      if (!m_initialized) return;
      m_initialized = false;
      kill(m_fds.pid, SIGTERM);
    }
  private:
    int m_input_busy_wait = 10; // in msec
    std::vector<byte> input_buffer;
    void flush_buffer() {
      const byte* p = &input_buffer[0];
      std::size_t sz = input_buffer.size();
      if (!sz) return;
      for (;;) {
        ssize_t const sz_write = (ssize_t) std::min<std::size_t>(sz, SSIZE_MAX);
        ssize_t const n = ::write(m_fds.masterfd, p, sz_write);
        if (n > 0) {
          if ((std::size_t) n >= sz) break;
          p += n;
          sz -= n;
        } else
          contra::msleep(m_input_busy_wait);
      }
      input_buffer.clear();
    }

    void put_c1(std::uint32_t code) {
      if (term().state().get_mode(ansi::mode_s7c1t)) {
        input_buffer.push_back(ascii_esc);
        input_buffer.push_back(code - 0x40);
      } else
        input_buffer.push_back(code);
    }
    void put_unsigned(std::uint32_t value) {
      if (value >= 10) put_unsigned(value / 10);
      input_buffer.push_back(ascii_0 + value % 10);
    }
    void put_modifier(std::uint32_t mod) {
      put_unsigned(1 + ((mod & _modifier_mask) >> _modifier_shft));
    }

    void send_key_impl(std::uint32_t key) {
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
        return;
      }

      // C0 文字および DEL
      if (mod == modifier_control) {
        if (code == ascii_sp || code == ascii_a ||
          (ascii_a < code && code <= ascii_z) ||
          (ascii_left_bracket <= code && code <= ascii_underscore)
        ) {
          // C-@ ... C-_
          input_buffer.push_back(code & 0x1F);
          return;
        } else if (code == ascii_question) {
          // C-? → ^?
          input_buffer.push_back(0x7F);
          return;
        } else if (code == ascii_bs) {
          // C-back → ^_
          input_buffer.push_back(0x1F);
          return;
        }
      }

      // BS CR(RET) HT(TAB) ESC
      // CSI <Unicode> ; <Modifier> u 形式
      if (code < key_base) {
        put_c1(ascii_csi);
        put_unsigned(code);
        if (mod) {
          input_buffer.push_back(ascii_semicolon);
          put_modifier(mod);
        }
        input_buffer.push_back(ascii_u);
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
          input_buffer.push_back(ascii_semicolon);
          put_modifier(mod);
        }
        input_buffer.push_back(ascii_tilde);
        break;
      case key_up   : a = ascii_A; goto alpha;
      case key_down : a = ascii_B; goto alpha;
      case key_right: a = ascii_C; goto alpha;
      case key_left : a = ascii_D; goto alpha;
      case key_begin: a = ascii_E; goto alpha;
      alpha:
        if (mod) {
          put_c1(ascii_csi);
          input_buffer.push_back(ascii_1);
          input_buffer.push_back(ascii_semicolon);
          put_modifier(mod);
        } else {
          put_c1(term().state().get_mode(ansi::mode_decckm) ? ascii_ss3 : ascii_csi);
        }
        input_buffer.push_back(a);
        break;
      default: break;
        input_buffer.push_back(ascii_tilde);
      }

      // // 修飾キー (未実装)
      // if (mod == modifier_control && ascii_a < code && code <= ascii_z) {
      //   input_buffer.push_back(code & 0x1F);
      //   return;
      // }
    }
  public:
    void send_key(std::uint32_t key) {
      send_key_impl(key);
      flush_buffer();
    }
  };

  class font_store {
    LOGFONT log_normal;
    HFONT m_normal = NULL;

    LONG m_width;
    LONG m_height;

  public:
    font_store() {
      m_height = 13;
      m_width = 7;
      log_normal.lfHeight = m_height;
      log_normal.lfWidth  = m_width;
      log_normal.lfEscapement = 0;
      log_normal.lfOrientation = 0;
      log_normal.lfWeight = FW_NORMAL;
      log_normal.lfItalic = FALSE;
      log_normal.lfUnderline = FALSE;
      log_normal.lfStrikeOut = FALSE;
      log_normal.lfCharSet = DEFAULT_CHARSET;
      log_normal.lfOutPrecision = OUT_DEFAULT_PRECIS;
      log_normal.lfClipPrecision = CLIP_DEFAULT_PRECIS;
      log_normal.lfQuality = CLEARTYPE_QUALITY;
      log_normal.lfPitchAndFamily = FIXED_PITCH | FF_DONTCARE;
      //::_tcscpy_s(log_normal.lfFaceName, LF_FACESIZE, TEXT("MeiryoKe_Console"));
      ::_tcscpy(log_normal.lfFaceName, TEXT("MeiryoKe_Console"));
    }

    LONG width() const { return m_width; }
    LONG height() const { return m_height; }

    LOGFONT const& logfont_normal() { return log_normal; }

    HFONT normal() {
      if (!m_normal)
        m_normal = CreateFontIndirect(&log_normal);
      return m_normal;
    }

    ~font_store() {
      if (m_normal) ::DeleteObject(m_normal);
    }
  };

  LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

  class main_window_t {
    font_store fstore;

    terminal_session sess;

    HWND hWnd = NULL;

    static constexpr LPCTSTR szClassName = TEXT("Contra/twin.Main");

  public:
    HWND create_window(HINSTANCE hInstance) {
      static bool is_window_class_registered = false;
      if (!is_window_class_registered) {
        is_window_class_registered = true;

        WNDCLASS myProg;
        myProg.style         = CS_HREDRAW | CS_VREDRAW;
        myProg.lpfnWndProc   = contra::twin::WndProc;
        myProg.cbClsExtra    = 0;
        myProg.cbWndExtra    = 0;
        myProg.hInstance     = hInstance;
        myProg.hIcon         = NULL;
        myProg.hCursor       = LoadCursor(NULL, IDC_ARROW);
        myProg.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
        myProg.lpszMenuName  = NULL;
        myProg.lpszClassName = szClassName;
        if (!RegisterClass(&myProg)) return NULL;
      }

      this->hWnd = CreateWindow(
        szClassName,
        TEXT("Contra/Twin"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        80 * 7, 30 * 13,
        NULL,
        NULL,
        hInstance,
        NULL);
      return hWnd;
    }

  private:
    void render_window(HDC hdc) {
      if (!hWnd || !sess.is_initialized()) return;

      auto deleter = [&] (auto p) { SelectObject(hdc, p); };
      util::raii hOldFont((HFONT) SelectObject(hdc, fstore.normal()), deleter);
      RECT rcClient;
      if (::GetClientRect(hWnd, &rcClient)) {
        util::raii hOldPen((HBRUSH) SelectObject(hdc, GetStockObject(NULL_PEN)), deleter);
        util::raii hOldBrush((HBRUSH) SelectObject(hdc, GetStockObject(WHITE_BRUSH)), deleter);
        Rectangle(hdc, 0, 0, rcClient.right - rcClient.left, rcClient.bottom - rcClient.top);
      }

      {
        using namespace contra::ansi;
        std::vector<cell_t> cells;
        std::vector<TCHAR> characters;
        std::vector<INT> progress;

        board_t& b = sess.board();

        for (curpos_t y = 0; y < b.m_height; y++) {
          line_t const& line = b.m_lines[y];
          line.get_cells_in_presentation(cells, b.line_r2l(line));

          characters.clear();
          progress.clear();
          characters.reserve(cells.size());
          progress.reserve(cells.size());
          for (auto const& cell : cells) {
            std::uint32_t code = cell.character.value;
            if (code & character_t::flag_cluster_extension)
              code &= ~character_t::flag_cluster_extension;
            if (code == (code & character_t::unicode_mask)) {
              characters.push_back(code);
              progress.push_back(cell.width * sess.ws.ws_xpixel);
            }
            // ToDo: その他の文字・マーカーに応じた処理。
          }
          if (characters.size())
            ExtTextOut(hdc, 0, y * sess.ws.ws_ypixel, 0, NULL, &characters[0], characters.size(), &progress[0]);
        }
      }
      //SelectObject(hdc, hOldFont);
    }

    enum extended_key_flags {
      modifier_application = 0x04000000,
      toggle_capslock   = 0x100,
      toggle_numlock    = 0x200,
      toggle_scrolllock = 0x400,
    };

    void process_input(std::uint32_t key) {
      //contra::term::print_key(key, stderr);
      sess.send_key(key);
    }

    std::uint32_t get_modifiers() {
      std::uint32_t ret = 0;
      if (GetKeyState(VK_LSHIFT) & 0x8000) ret |= modifier_shift;
      if (GetKeyState(VK_LCONTROL) & 0x8000) ret |= modifier_control;
      if (GetKeyState(VK_LMENU) & 0x8000) ret |= modifier_meta;
      if (GetKeyState(VK_RMENU) & 0x8000) ret |= modifier_alter;
      if (GetKeyState(VK_RCONTROL) & 0x8000) ret |= modifier_super;
      if (GetKeyState(VK_RSHIFT) & 0x8000) ret |= modifier_hyper;
      if (GetKeyState(VK_APPS) & 0x8000) ret |= modifier_application;

      if (GetKeyState(VK_CAPITAL) & 1) ret |= toggle_capslock;
      if (GetKeyState(VK_NUMLOCK) & 1) ret |= toggle_numlock;
      if (GetKeyState(VK_SCROLL) & 1) ret |= toggle_scrolllock;
      return ret;
    }

    static bool is_keypad_shifted(std::uint32_t modifiers) {
      if (modifiers & modifier_shift) {
        for (UINT k = VK_NUMPAD0; k <= VK_NUMPAD9; k++)
          if (GetKeyState(k) & 0x8000) return true;
        if (GetKeyState(VK_DECIMAL) & 0x8000) return true;
      }
      return false;
    }

    void process_key(WPARAM wParam, std::uint32_t modifiers) {
      static BYTE kbstate[256];

      if (ascii_A <= wParam && wParam <= ascii_Z) {
        if (modifiers & ~modifier_shift & _modifier_mask) {
          wParam += ascii_a - ascii_A;
        } else {
          bool const capslock = modifiers & toggle_capslock;
          bool const shifted = modifiers & modifier_shift;
          if (capslock == shifted) wParam += ascii_a - ascii_A;
          modifiers &= ~modifier_shift;
        }
        process_input(wParam | (modifiers & _modifier_mask));
      } else if (VK_F1 <= wParam && wParam < VK_F24) {
        process_input((wParam -VK_F1 + key_f1) | (modifiers & _modifier_mask));
      } else {
        std::uint32_t code = 0;
        switch (wParam) {
        case VK_BACK:   code = ascii_bs;     goto function_key;
        case VK_TAB:    code = ascii_ht;    goto function_key;
        case VK_RETURN: code = ascii_cr;    goto function_key;
        case VK_ESCAPE: code = ascii_esc;    goto function_key;
        case VK_DELETE:    code = key_delete; goto keypad_function_key;
        case VK_INSERT:    code = key_insert; goto keypad_function_key;
        case VK_PRIOR:     code = key_prior;  goto keypad_function_key;
        case VK_NEXT:      code = key_next;   goto keypad_function_key;
        case VK_END:       code = key_end;    goto keypad_function_key;
        case VK_HOME:      code = key_home;   goto keypad_function_key;
        case VK_LEFT:      code = key_left;   goto keypad_function_key;
        case VK_UP:        code = key_up;     goto keypad_function_key;
        case VK_RIGHT:     code = key_right;  goto keypad_function_key;
        case VK_DOWN:      code = key_down;   goto keypad_function_key;
        case VK_CLEAR:     code = key_begin;  goto keypad_function_key;
        keypad_function_key:
          if ((modifiers & toggle_numlock) && is_keypad_shifted(modifiers))
            modifiers &= ~modifier_shift;
          goto function_key;
        case VK_NUMPAD0:   code = key_kp0;   goto keypad_number_key;
        case VK_NUMPAD1:   code = key_kp1;   goto keypad_number_key;
        case VK_NUMPAD2:   code = key_kp2;   goto keypad_number_key;
        case VK_NUMPAD3:   code = key_kp3;   goto keypad_number_key;
        case VK_NUMPAD4:   code = key_kp4;   goto keypad_number_key;
        case VK_NUMPAD5:   code = key_kp5;   goto keypad_number_key;
        case VK_NUMPAD6:   code = key_kp6;   goto keypad_number_key;
        case VK_NUMPAD7:   code = key_kp7;   goto keypad_number_key;
        case VK_NUMPAD8:   code = key_kp8;   goto keypad_number_key;
        case VK_NUMPAD9:   code = key_kp9;   goto keypad_number_key;
        case VK_DECIMAL:   code = key_kpdec; goto keypad_number_key;
        keypad_number_key:
          modifiers &= ~modifier_shift;
          goto function_key;
        case VK_MULTIPLY:  code = key_kpmul; goto function_key;
        case VK_ADD:       code = key_kpadd; goto function_key;
        case VK_SEPARATOR: code = key_kpsep; goto function_key;
        case VK_SUBTRACT:  code = key_kpsub; goto function_key;
        case VK_DIVIDE:    code = key_kpdiv; goto function_key;
        function_key:
          process_input(code | (modifiers & _modifier_mask));
          break;
        case VK_PROCESSKEY: // IME 処理中 (無視)
          return;
        default:
          {
            kbstate[VK_SHIFT] = modifiers & modifier_shift ? 0x80 : 0x00;
            WCHAR buff = 0;
            int const count = ::ToUnicode(wParam, ::MapVirtualKey(wParam, MAPVK_VK_TO_VSC), kbstate, &buff, 1, 0);
            if (count == 1 || count == -1) {
              // Note: dead-char (count == 2) の場合には今の所は対応しない事にする。

              // shift によって文字が変化していたら shift 修飾を消す。
              UINT const unshifted = ::MapVirtualKey(wParam, MAPVK_VK_TO_CHAR);
              int const unshifted_count = (INT) unshifted < 0 ? 2 : 1;
              if (count != unshifted_count || buff != (unshifted & mask_unicode))
                modifiers &= ~modifier_shift;

              process_input((buff & mask_unicode) | (modifiers & _modifier_mask));
            } else if (UINT const code = ::MapVirtualKey(wParam, MAPVK_VK_TO_CHAR); (INT) code > 0) {
              process_input((code & mask_unicode) | (modifiers & _modifier_mask));
            } else {
              //std::fprintf(stderr, "key (unknown): wparam=%08x flags=%x\n", wParam, modifiers);
            }
          }
          break;
        }
      }
    }

    void process_char(WPARAM wParam, std::uint32_t modifiers) {
      // ToDo: Process surrogate pair
      process_input((wParam & mask_unicode) | (modifiers & _modifier_mask));
    }

  public:
    LRESULT process_message(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
      switch (msg) {
      case WM_CREATE:
        ::SetClassLongPtr(hWnd, GCLP_HBRBACKGROUND, (LONG) CreateSolidBrush(RGB(0xFF,0xFF,0xFF)));
        return 0L;
      case WM_DESTROY:
        //::PostQuitMessage(0);
        this->hWnd = NULL;
        return 0L;
      case WM_PAINT:
        if (this->hWnd && this->sess.is_initialized()) {
          PAINTSTRUCT paint;
          HDC hdc = BeginPaint(hWnd, &paint);
          this->render_window(hdc);
          EndPaint(hWnd, &paint);
        }
        return 0L;
      case WM_KEYDOWN:
      case WM_SYSKEYDOWN:
        process_key(wParam, get_modifiers());
        return 0L;
      case WM_IME_CHAR:
        process_char(wParam, get_modifiers());
        return 0L;
      case WM_CHAR:
      case WM_DEADCHAR:
      case WM_KEYUP:
      case WM_SYSCHAR:
      case WM_SYSDEADCHAR:
      case WM_SYSKEYUP:
        // 余分な操作をしない様に無視
        // DEADCHAR は accent や diacritic などの入力の時に発生する。
        return 0L;

      case WM_IME_STARTCOMPOSITION:
        {
          LRESULT const result = ::DefWindowProc(hWnd, msg, wParam, lParam);

          util::raii hIMC(::ImmGetContext(hWnd), [hWnd] (auto hIMC) { ::ImmReleaseContext(hWnd, hIMC); });

          ::ImmSetCompositionFont(hIMC, const_cast<LOGFONT*>(&fstore.logfont_normal()));

          RECT rcClient;
          if (::GetClientRect(hWnd, &rcClient)) {
            COMPOSITIONFORM form;
            form.dwStyle = CFS_POINT;
            form.ptCurrentPos.x = rcClient.left;
            form.ptCurrentPos.y = rcClient.top+13;
            ::ImmSetCompositionWindow(hIMC, &form);
          }

          return result;
        }
      default:
        // {
        //   const char* name = get_window_message_name(msg);
        //   if (name)
        //     std::fprintf(stderr, "message: %s\n", name);
        //   else
        //     std::fprintf(stderr, "message: %08x\n", msg);
        // }
        return ::DefWindowProc(hWnd, msg, wParam, lParam);
      }
    }

  public:
    int do_loop() {
      sess.ws.ws_col = 80;
      sess.ws.ws_row = 30;
      sess.ws.ws_xpixel = fstore.width();
      sess.ws.ws_ypixel = fstore.height();
      sess.read_buffer_size = 4096;
      if (!sess.initialize()) return 2;

      int exit_code;
      MSG msg;
      while (hWnd) {
        bool processed = false;
        clock_t time0 = clock();
        while (sess.process()) {
          processed = true;
          clock_t const time1 = clock();
          clock_t const msec = (time1 - time0) * 1000 / CLOCKS_PER_SEC;
          if (msec > 20) break;
        }
        if (processed) {
          HDC hdc = GetDC(hWnd);
          render_window(hdc);
          ReleaseDC(hWnd, hdc);
        }

        while (::PeekMessage(&msg, hWnd, 0, 0, PM_NOREMOVE)) {
          if (!GetMessage(&msg, hWnd, 0, 0)) {
            exit_code = msg.wParam;
            goto exit;
          }

          TranslateMessage(&msg);
          DispatchMessage(&msg);
        }

        // if (contra::read_from_fd(STDIN_FILENO, &devIn, buff, sizeof(buff))) continue;
        if (!sess.is_child_alive()) break;
        if (!processed)
          contra::msleep(10);
      }
    exit:
      sess.terminate();
      return exit_code;
    }
  };

  main_window_t main_window;

  LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return main_window.process_message(hWnd, msg, wParam, lParam);
  }
}
}

// from http://www.kumei.ne.jp/c_lang/index_sdk.html
extern "C" int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE hPreInst, LPTSTR lpszCmdLine, int nCmdShow) {
  contra_unused(lpszCmdLine);
  contra_unused(hPreInst);

  auto& win = contra::twin::main_window;
  HWND const hWnd = win.create_window(hInstance);
  ::ShowWindow(hWnd, nCmdShow);
  ::UpdateWindow(hWnd);
  return win.do_loop();
}

// from https://cat-in-136.github.io/2012/04/unicodemingw32twinmainwwinmain.html
#if defined(_UNICODE) && !defined(_tWinMain)
int main(int argc, char** argv) {
  contra_unused(argc);
  contra_unused(argv);
  //ShowWindow(GetConsoleWindow(), SW_HIDE);
  HINSTANCE const hInstance = GetModuleHandle(NULL);
  int const retval = _tWinMain(hInstance, NULL, (LPTSTR) TEXT("") /* lpCmdLine is not available*/, SW_SHOW);
  return retval;
}
#endif
