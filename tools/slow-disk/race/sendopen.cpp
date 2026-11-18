// sendopen.exe  ms:file ms:file ... : hands files to Cue's window as a 2nd instance would (WM_COPYDATA),
// each after waiting `ms` since the one before; also "key:<vk>" (posted WM_KEYDOWN/UP) and "say:<text>".
#include <windows.h>
#include <cstdio>
#include <string>
#include <vector>
static std::string enc(const std::string& f) { std::string p = f; p.push_back(0); p.push_back(0); return p; }
int wmain(int argc, wchar_t** argv) {
  HWND w = FindWindowW(L"tinyoffice.Cue", nullptr);
  if (!w) { std::printf("no Cue window\n"); return 1; }
  LARGE_INTEGER f, t0, t; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&t0);
  for (int i = 1; i < argc; ++i) {
    std::wstring a = argv[i];
    size_t c = a.find(L':');
    int ms = _wtoi(a.substr(0, c).c_str());
    std::wstring what = a.substr(c + 1);
    Sleep(ms);
    QueryPerformanceCounter(&t);
    double at = double(t.QuadPart - t0.QuadPart) * 1000.0 / f.QuadPart;
    if (what.rfind(L"msg:", 0) == 0) {  // msg:<id>:<wparam> posted (e.g. msg:0x112:0xF020 = minimize)
      wchar_t* end = nullptr;
      UINT id = (UINT)wcstoul(what.c_str() + 4, &end, 0);
      WPARAM wp = (end && *end == L':') ? (WPARAM)wcstoull(end + 1, nullptr, 0) : 0;
      PostMessageW(w, id, wp, 0);
      std::printf("%8.1f ms msg 0x%x 0x%llx\n", at, id, (unsigned long long)wp);
      continue;
    }
    if (what.rfind(L"key:", 0) == 0) {
      UINT vk = (UINT)wcstoul(what.c_str() + 4, nullptr, 0);
      PostMessageW(w, WM_KEYDOWN, vk, 1); PostMessageW(w, WM_KEYUP, vk, 0xC0000001);
      std::printf("%8.1f ms key 0x%x\n", at, vk);
      continue;
    }
    char u[2048]; WideCharToMultiByte(CP_UTF8, 0, what.c_str(), -1, u, sizeof u, nullptr, nullptr);
    std::string p = enc(u);
    COPYDATASTRUCT d{0x43554501, (DWORD)p.size(), p.data()};
    DWORD_PTR r = 0;
    SendMessageTimeoutW(w, WM_COPYDATA, 0, (LPARAM)&d, SMTO_BLOCK, 3000, &r);
    std::printf("%8.1f ms open %s -> %d\n", at, u, (int)r);
  }
  return 0;
}
