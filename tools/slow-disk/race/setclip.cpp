// setclip.exe <text> : puts this text on the clipboard (CF_UNICODETEXT)
#include <windows.h>
#include <cwchar>
#include <cstring>
int wmain(int argc, wchar_t** argv) {
  if (argc < 2 || !OpenClipboard(nullptr)) return 1;
  EmptyClipboard();
  size_t n = (wcslen(argv[1]) + 1) * sizeof(wchar_t);
  HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, n);
  std::memcpy(GlobalLock(g), argv[1], n); GlobalUnlock(g);
  SetClipboardData(CF_UNICODETEXT, g);
  CloseClipboard();
  return 0;
}
