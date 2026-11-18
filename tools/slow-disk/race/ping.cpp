#include <windows.h>
#include <cstdio>
int main() {
  HWND w = FindWindowW(L"tinyoffice.Cue", nullptr);
  if (!w) { std::printf("no window\n"); return 0; }
  DWORD_PTR r = 0;
  std::printf(SendMessageTimeoutW(w, WM_NULL, 0, 0, SMTO_BLOCK, 3000, &r) ? "ok\n" : "HUNG\n");
  return 0;
}
