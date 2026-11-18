#include <windows.h>
LRESULT CALLBACK wp(HWND h, UINT m, WPARAM w, LPARAM l) {
  if (m == WM_PAINT) { PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT r; GetClientRect(h, &r); FillRect(dc, &r, (HBRUSH)GetStockObject(WHITE_BRUSH)); EndPaint(h, &ps); return 0; }
  if (m == WM_ERASEBKGND) return 1;
  if (m == WM_DESTROY) { PostQuitMessage(0); return 0; }
  return DefWindowProcW(h, m, w, l);
}
int WINAPI wWinMain(HINSTANCE i, HINSTANCE, PWSTR, int) {
  WNDCLASSW c{}; c.lpfnWndProc = wp; c.hInstance = i; c.lpszClassName = L"whitewin"; c.style = CS_HREDRAW | CS_VREDRAW; RegisterClassW(&c);
  HWND h = CreateWindowExW(0, L"whitewin", L"Cuewhite", WS_POPUP, 0, 0, 300, 300, 0, 0, i, 0); ShowWindow(h, SW_SHOW); UpdateWindow(h);
  MSG msg; while (GetMessageW(&msg, 0, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
}
