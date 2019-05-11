#include <windows.h>

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
char szClassNme[] = "ウィンドウクラス・ネーム";

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPreInst,
  LPSTR lpszCmdLine, int nCmdShow)
{
  HWND hWnd;
  MSG msg;
  WNDCLASS myProg;
  if (!hPreInst) {
    myProg.style           =CS_HREDRAW | CS_VREDRAW;
    myProg.lpfnWndProc     =WndProc;
    myProg.cbClsExtra      =0;
    myProg.cbWndExtra      =0;
    myProg.hInstance       =hInstance;
    myProg.hIcon           =NULL;
    myProg.hCursor         =LoadCursor(NULL, IDC_ARROW);
    myProg.hbrBackground   =(HBRUSH) GetStockObject(WHITE_BRUSH);
    myProg.lpszMenuName    =NULL;
    myProg.lpszClassName   =szClassNme;
    if (!RegisterClass(&myProg))
      return FALSE;
  }
  hWnd = CreateWindow(szClassNme,
    "猫でもわかるプログラミング",
    WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    NULL,
    NULL,
    hInstance,
    NULL);
  ShowWindow(hWnd, nCmdShow);
  UpdateWindow(hWnd);
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  return (msg.wParam);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg) {
  case WM_DESTROY:
    PostQuitMessage(0);
    break;
  default:
    return(DefWindowProc(hWnd, msg, wParam, lParam));
  }
  return (0L);
}
