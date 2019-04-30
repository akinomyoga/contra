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
#include "../contradef.hpp"
#include "../enc.utf8.hpp"
#include "../util.hpp"
#include "../pty.hpp"
#include "../session.hpp"
#include "win_messages.hpp"

#define _tcslen wcslen
#define _tcscpy_s wcscpy_s
#define _tcscpy wcscpy

namespace contra {
namespace twin {

  using namespace contra::term;

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

      this->hWnd = CreateWindowEx(
        WS_EX_COMPOSITED,
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
      if (!hWnd || !sess.is_active()) return;

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
              progress.push_back(cell.width * sess.init_ws.ws_xpixel);
            }
            // ToDo: その他の文字・マーカーに応じた処理。
          }
          if (characters.size())
            ExtTextOut(hdc, 0, y * sess.init_ws.ws_ypixel, 0, NULL, &characters[0], characters.size(), &progress[0]);
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
        if (this->hWnd && this->sess.is_active()) {
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
      sess.init_ws.ws_col = 80;
      sess.init_ws.ws_row = 30;
      sess.init_ws.ws_xpixel = fstore.width();
      sess.init_ws.ws_ypixel = fstore.height();
      sess.init_read_buffer_size = 4096;
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
        if (!sess.is_alive()) break;
        if (!processed)
          contra::term::msleep(10);
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
