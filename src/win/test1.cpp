#include <string.h>
#include <tchar.h>
#include <wchar.h>
#include <windows.h>
#include <wingdi.h>
#include <mwg/except.h>
#include <vector>
#include <cstddef>
#include "../contradef.h"

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

  public:
    LRESULT process_message(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
      switch (msg) {
      case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0L;
      case WM_PAINT:
        this->render_window(hWnd);
        return 0L;
      default:
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
