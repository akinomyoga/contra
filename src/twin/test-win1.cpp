#include <string.h>
#include <tchar.h>
#include <wchar.h>
#include <windows.h>
#include <imm.h>

#include <sys/ioctl.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <sstream>
#include <iomanip>
#include <memory>
#include <functional>
#include <mwg/except.h>
#include "../contradef.hpp"

#define _tcslen wcslen
#define _tcscpy_s wcscpy_s
#define _tcscpy wcscpy

namespace contra {
namespace twin {

  template<typename XCH>
  int xcscpy_s(XCH* dst, std::size_t sz, const XCH* src) {
    if (!sz--) return 1;
    char c;
    while (sz-- && (c = *src++)) *dst++ = c;
    *dst = '\0';
    return 0;
  }

  LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

  class font_store {
    LOGFONT log_normal;
    HFONT m_normal = NULL;

    LONG m_width;
    LONG m_height;

    void release() {
      if (m_normal) {
        ::DeleteObject(m_normal);
        m_normal = NULL;
      }
    }
  public:
    font_store() {
      // default settings
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
      xcscpy_s(log_normal.lfFaceName, LF_FACESIZE, TEXT("MeiryoKe_Console"));
    }

    void set_size(LONG width, LONG height) {
      if (m_width == width && m_height == height) return;
      release();
      this->m_width = width;
      this->m_height = height;
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
      release();
    }
  };

  class main_window_t {
    HWND hWnd = NULL;
    font_store fstore;

    static constexpr LPCTSTR szClassName = TEXT("Contra.Twin.Test1");

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
        0, //WS_EX_COMPOSITED,
        szClassName,
        TEXT("Contra/Twin test1"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        600,
        400,
        NULL,
        NULL,
        hInstance,
        NULL);
      return hWnd;
    }

  private:
    void test_surrogate_pair(HDC hdc, std::size_t xorigin , std::size_t yorigin) {
      // Surrogate pair  のテスト
      //   そもそも現在のフォントで表示できる文字を探すのが大変。
      //   U+10384 は駄目だった。U+10400 は出来た。
      std::size_t const line_height = fstore.height();
      TextOut(hdc, xorigin, yorigin, TEXT("Test1: ExtTextOut and surrogate pair"), 36);
      TextOut(hdc, xorigin, yorigin += line_height, TEXT("\U00010400X"), 3);
      INT progress[3] = {0, 0, 7};
      progress[0] = 0;
      progress[1] = 14;
      ExtTextOut(hdc, xorigin, yorigin += line_height, 0, NULL, TEXT("\U00010400X"), 3, progress);
      progress[0] = 7;
      progress[1] = 7;
      ExtTextOut(hdc, xorigin, yorigin += line_height, 0, NULL, TEXT("\U00010400X"), 3, progress);
      progress[0] = 14;
      progress[1] = 0;
      ExtTextOut(hdc, xorigin, yorigin += line_height, 0, NULL, TEXT("\U00010400X"), 3, progress);

      // 結果: 実は surrogate pair の場合には
      // progress に指定する値の割当はどうでも良かった。
      // 二つの値が加算された値が実際には次の文字の表示に使われる。
    }
    void test_grapheme_cluster(HDC hdc, std::size_t x , std::size_t y) {
      std::size_t const line_height = fstore.height();
      TextOut(hdc, x, y, TEXT("Test2: ExtTextOut and grapheme cluster"), 38);
      //LPCTSTR text = TEXT("P\u0338D\u0338D\u0338\u0305X");
      //LPCTSTR text = TEXT("P\u0305D\u0305D\u0305\u0324XYZ");
      LPCTSTR text = TEXT("P\u0302D\u0302D\u0302\u0324XYZ");
      //LPCTSTR text = TEXT("P\u0324D\u0324D\u0324\u0302XYZ");
      TextOut(hdc, x, y += line_height, text, 8);
      INT progress1[8] = {7, 0, 7, 0, 7, 0, 0, 7};
      ExtTextOut(hdc, x, y += line_height, 0, NULL, text, 8, progress1);
      INT progress2[8] = {0, 7, 0, 7, 0, 0, 7, 7};
      ExtTextOut(hdc, x, y += line_height, 0, NULL, text, 8, progress2);
      INT progress3[8] = {3, 4, 2, 5, 0, 7, 0, 7};
      ExtTextOut(hdc, x, y += line_height, 0, NULL, text, 8, progress3);

      // 結果: やはり progress は全て加算されて処理される様に見える。
      // なので結局総和だけが重要という事になる。
      // 一方で、TextOut は全然駄目だ。ちゃんと計算できていない。
      // ExtTextOut の方がちゃんとした出力結果になっている気がする。
    }
    bool render_window(HDC hdc) {
      if (!hWnd) return false;

      SelectObject(hdc, fstore.normal());
      SetBkMode(hdc, TRANSPARENT);
      test_surrogate_pair(hdc, 2, 2);
      test_grapheme_cluster(hdc, 2, 100);

      // // Grapheme cluster のテスト
      // INT progress[3] = {10, 10, 10};
      // ExtTextOut(hdc, 10, 10, 0, NULL, u"", 3, );

      return true;
    }

  public:
    LRESULT process_message(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
      switch (msg) {
      case WM_CREATE:
        ::SetClassLongPtr(hWnd, GCLP_HBRBACKGROUND, (LONG_PTR) CreateSolidBrush(RGB(0xFF,0xFF,0xFF)));
        return 0L;
      case WM_DESTROY:
        this->hWnd = NULL;
        return 0L;
      case WM_PAINT:
        if (this->hWnd) {
          PAINTSTRUCT paint;
          HDC hdc = BeginPaint(hWnd, &paint);
          this->render_window(hdc);
          EndPaint(hWnd, &paint);
        }
        return 0L;
      default:
        return ::DefWindowProc(hWnd, msg, wParam, lParam);
      }
    }

  public:
    int do_loop() {
      MSG msg;
      while (hWnd && GetMessage(&msg, hWnd, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
      return msg.wParam;
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
