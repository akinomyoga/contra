#include <string.h>
#include <tchar.h>
#include <wchar.h>
#include <windows.h>
#include <imm.h>

#include <cstddef>
#include <cstdint>
#include <vector>
#include <sstream>
#include <iomanip>
#include <mwg/except.h>
#include "../contradef.h"
#include "../ansi/enc.utf8.hpp"
#include "win_messages.hpp"

#define _tcslen wcslen

namespace contra {
namespace twin {

  class font_store {
    HFONT m_normal = NULL;

  public:
    font_store() {}

    HFONT normal() {
      if (!m_normal) {
        m_normal = CreateFont(
          13, 7, 0, 0, FW_NORMAL,
          FALSE, FALSE, FALSE,
          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
          CLEARTYPE_QUALITY, FIXED_PITCH | FF_DONTCARE, TEXT("MeiryoKe_Console"));
      }
      return m_normal;
    }

    ~font_store() {
      if (m_normal) ::DeleteObject(m_normal);
    }
  };

  class main_window_t {
    font_store fstore;

    void render_window(HWND hWnd) {
      PAINTSTRUCT paint;
      LPCTSTR str = TEXT("Default Content 日本語");
      HDC hdc = BeginPaint(hWnd, &paint);

      std::size_t const len = _tcslen(str);
      mwg_check(len <= 8192);
      std::vector<INT> progression(len, 7);
      SelectObject(hdc, fstore.normal());
      ExtTextOut(hdc, 1, 1, 0, NULL, str, len, &progression[0]);
      //TextOut(hdc, 0, 0, str, _tcslen(str));
      EndPaint(hWnd, &paint);
    }

    enum modifier_keys {
      mask_modifiers  = 0x03F00000,
      mask_unicode    = 0x0001FFFF,
      mask_keycode    = 0x0003FFFF,

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


      mod_shift       = 0x00100000,
      mod_meta        = 0x00200000,
      mod_control     = 0x00400000,
      mod_super       = 0x00800000,
      mod_hyper       = 0x01000000,
      mod_alter       = 0x02000000,

      mod_application = 0x04000000,
      toggle_capslock   = 0x100,
      toggle_numlock    = 0x200,
      toggle_scrolllock = 0x400,
    };

    void process_input(std::uint32_t key) {
      std::uint32_t const code = key & mask_keycode;

      std::ostringstream str;
      if (key & mod_alter) str << "A-";
      if (key & mod_hyper) str << "H-";
      if (key & mod_super) str << "s-";
      if (key & mod_control) str << "C-";
      if (key & mod_meta) str << "M-";
      if (key & mod_shift) str << "S-";
      std::fprintf(stderr, "key: %s", str.str().c_str());

      if (code < key_base) {
        if (code == ascii_bs)
          std::fprintf(stderr, "BS");
        else if (code == ascii_ht)
          std::fprintf(stderr, "TAB");
        else if (code == ascii_cr)
          std::fprintf(stderr, "RET");
        else if (code == ascii_esc)
          std::fprintf(stderr, "ESC");
        else
          contra::encoding::put_u8((char32_t) code, stderr);
        std::putc('\n', stderr);
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
          std::fprintf(stderr, "%s\n", name);
        } else {
          std::fprintf(stderr, "unknown %08x\n", code);
        }
      }
    }

    std::uint32_t get_modifiers() {
      std::uint32_t ret = 0;
      if (GetKeyState(VK_LSHIFT) & 0x8000) ret |= mod_shift;
      if (GetKeyState(VK_LCONTROL) & 0x8000) ret |= mod_control;
      if (GetKeyState(VK_LMENU) & 0x8000) ret |= mod_meta;
      if (GetKeyState(VK_RMENU) & 0x8000) ret |= mod_alter;
      if (GetKeyState(VK_RCONTROL) & 0x8000) ret |= mod_super;
      if (GetKeyState(VK_RSHIFT) & 0x8000) ret |= mod_hyper;
      if (GetKeyState(VK_APPS) & 0x8000) ret |= mod_application;

      if (GetKeyState(VK_CAPITAL) & 1) ret |= toggle_capslock;
      if (GetKeyState(VK_NUMLOCK) & 1) ret |= toggle_numlock;
      if (GetKeyState(VK_SCROLL) & 1) ret |= toggle_scrolllock;
      return ret;
    }

    static bool is_keypad_shifted(std::uint32_t modifiers) {
      if (modifiers & mod_shift) {
        for (UINT k = VK_NUMPAD0; k <= VK_NUMPAD9; k++)
          if (GetKeyState(k) & 0x8000) return true;
        if (GetKeyState(VK_DECIMAL) & 0x8000) return true;
      }
      return false;
    }

    void process_key(WPARAM wParam, std::uint32_t modifiers) {
      static BYTE kbstate[256];

      if (ascii_A <= wParam && wParam <= ascii_Z) {
        if (modifiers & ~mod_shift & mask_modifiers) {
          wParam += ascii_a - ascii_A;
        } else {
          bool const capslock = modifiers & toggle_capslock;
          bool const shifted = modifiers & mod_shift;
          if (capslock == shifted) wParam += ascii_a - ascii_A;
          modifiers &= ~mod_shift;
        }
        process_input(wParam | (modifiers & mask_modifiers));
      } else if (VK_F1 <= wParam && wParam < VK_F24) {
        process_input((wParam -VK_F1 + key_f1) | (modifiers & mask_modifiers));
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
            modifiers &= ~mod_shift;
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
          modifiers &= ~mod_shift;
          goto function_key;
        case VK_MULTIPLY:  code = key_kpmul; goto function_key;
        case VK_ADD:       code = key_kpadd; goto function_key;
        case VK_SEPARATOR: code = key_kpsep; goto function_key;
        case VK_SUBTRACT:  code = key_kpsub; goto function_key;
        case VK_DIVIDE:    code = key_kpdiv; goto function_key;
        function_key:
          process_input(code | (modifiers & mask_modifiers));
          break;
        case VK_PROCESSKEY: // IME 処理中 (無視)
          return;
        default:
          {
            kbstate[VK_SHIFT] = modifiers & mod_shift ? 0x80 : 0x00;
            WCHAR buff = 0;
            int const count = ::ToUnicode(wParam, ::MapVirtualKey(wParam, MAPVK_VK_TO_VSC), kbstate, &buff, 1, 0);
            if (count == 1 || count == -1) {
              // Note: dead-char (count == 2) の場合には今の所は対応しない事にする。

              // shift によって文字が変化していたら shift 修飾を消す。
              UINT const unshifted = ::MapVirtualKey(wParam, MAPVK_VK_TO_CHAR);
              int const unshifted_count = (INT) unshifted < 0 ? 2 : 1;
              if (count != unshifted_count || buff != (unshifted & mask_unicode))
                modifiers &= ~mod_shift;

              process_input((buff & mask_unicode) | (modifiers & mask_modifiers));
            } else if (UINT const code = ::MapVirtualKey(wParam, MAPVK_VK_TO_CHAR); (INT) code > 0) {
              process_input((code & mask_unicode) | (modifiers & mask_modifiers));
            } else {
              std::fprintf(stderr, "key (unknown): wparam=%08x flags=%x\n", wParam, modifiers);
            }
          }
          break;
        }
      }
    }

    void process_char(WPARAM wParam, std::uint32_t modifiers) {
      // ToDo: Process surrogate pair
      process_input((wParam & mask_unicode) | (modifiers & mask_modifiers));
    }

  public:
    LRESULT process_message(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
      switch (msg) {
      case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0L;
      case WM_PAINT:
        this->render_window(hWnd);
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
      default:
        {
          const char* name = get_window_message_name(msg);
          if (name)
            std::fprintf(stderr, "message: %s\n", name);
          else
            std::fprintf(stderr, "message: %08x\n", msg);
        }
        return ::DefWindowProc(hWnd, msg, wParam, lParam);
      }
    }
  };

  main_window_t main_window;

  LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return main_window.process_message(hWnd, msg, wParam, lParam);
  }
}
}

// from http://www.kumei.ne.jp/c_lang/index_sdk.html
TCHAR szClassName[] = TEXT("Contra/twin.Main");
extern "C" int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE hPreInst, LPTSTR lpszCmdLine, int nCmdShow) {
  contra_unused(lpszCmdLine);
  if (!hPreInst) {
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
    if (!RegisterClass(&myProg))
      return FALSE;
  }

  HWND hWnd = CreateWindow(szClassName,
    TEXT("[user@host] Main"),
    WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    80 * 7, 30 * 13,
    NULL,
    NULL,
    hInstance,
    NULL);

  ::ShowWindow(hWnd, nCmdShow);
  ::UpdateWindow(hWnd);

  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  return msg.wParam;
}

// from https://cat-in-136.github.io/2012/04/unicodemingw32twinmainwwinmain.html
#if defined(_UNICODE) && !defined(_tWinMain)
int main(int argc, char** argv) {
  contra_unused(argc);
  contra_unused(argv);
  ShowWindow(GetConsoleWindow(), SW_HIDE);
  HINSTANCE const hInstance = GetModuleHandle(NULL);
  int const retval = _tWinMain(hInstance, NULL, (LPTSTR) TEXT("") /* lpCmdLine is not available*/, SW_SHOW);
  return retval;
}
#endif
