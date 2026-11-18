// Runs the GitHub edition's real updater code once, without Cue and without installing anything:
// it reads the latest release from api.github.com, downloads its installer through GitHub's
// redirect to its download host (each hop checked against the allowed hosts), checks the
// SHA-256 GitHub publishes for it, prints what happened and deletes the file.
//   x86_64-w64-mingw32-g++ -std=c++20 -O1 -municode -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN
//     -D_WIN32_WINNT=0x0A00 -DNOMINMAX -DCUE_OFFICIAL_BUILD=1 -DCUE_STORE_BUILD=0 tools/update_probe.cpp
//     src/win/updater.cpp src/win/platform.cpp src/core/*.cpp -static -lwinhttp -lbcrypt -lole32 -lshell32
//     -luuid -lshcore -lcomdlg32 -ldwmapi -lgdi32 -lcrypt32 -o update_probe.exe
#include <windows.h>

#include <cstdio>
#include <string>

#include "../src/core/version.h"
#include "../src/win/platform.h"
#include "../src/win/updater.h"

static int g_result = 2;

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) {
  if (m != WM_APP + 1) return DefWindowProcW(h, m, w, l);
  auto* u = reinterpret_cast<UpdateMsg*>(l);
  switch (u->kind) {
    case UpdateMsg::Checked:
      if (!u->found) { std::printf("check: no usable release (%s)\n", u->error.c_str()); g_result = 1; PostQuitMessage(0); break; }
      std::printf("check ok: latest release %s, asset %s (%lld bytes)\n  from %s\n  sha256 %s\n",
                  u->rel.version.c_str(), u->rel.asset_name.c_str(), (long long)u->rel.size, u->rel.asset_url.c_str(),
                  u->rel.sha256.empty() ? "(none published)" : u->rel.sha256.c_str());
      std::printf("downloading through GitHub's redirect...\n");
      update_download_async(h, WM_APP + 1, u->rel);
      break;
    case UpdateMsg::Progress:
      if (u->percent % 10 == 0) std::printf("  %d%%\n", u->percent);
      break;
    case UpdateMsg::Downloaded:
      std::printf("download ok: %s\n  %s\n", narrow(u->file).c_str(), u->error.c_str());
      DeleteFileW(u->file.c_str());
      RemoveDirectoryW(u->file.substr(0, u->file.find_last_of(L'\\')).c_str());
      std::printf("(file deleted: nothing was installed)\n");
      g_result = 0;
      PostQuitMessage(0);
      break;
    case UpdateMsg::Failed:
      std::printf("FAILED: %s\n", u->error.c_str());
      g_result = 1;
      PostQuitMessage(0);
      break;
  }
  std::fflush(stdout);
  delete u;
  return 0;
}

int wmain() {
  std::printf("Cue %s updater probe (GitHub edition code)\n", CUE_VERSION);
  WNDCLASSW wc{};
  wc.lpfnWndProc = proc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = L"cue-update-probe";
  RegisterClassW(&wc);
  HWND hwnd = CreateWindowW(wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
  SetTimer(hwnd, 1, 180000, nullptr);  // three minutes at most
  update_check_async(hwnd, WM_APP + 1, true);
  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    if (msg.message == WM_TIMER) { std::printf("FAILED: no answer in three minutes\n"); g_result = 1; break; }
    DispatchMessageW(&msg);
  }
  std::printf(g_result == 0 ? "updater: passed\n" : "updater: did NOT pass\n");
  return g_result;
}
