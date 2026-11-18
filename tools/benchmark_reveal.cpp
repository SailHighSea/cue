// What the halo's entrance and exit cost (17/11), with the real Ambient on real layered windows. Every
// UpdateLayeredWindow is counted (ambient.cpp is built with it renamed to bench_ulw): a "paint" sends
// new pixels (each halo pixel through the colour table first, and through the reveal's falloff table
// while the light comes or goes), an "opacity" step only changes the strips' overall alpha (what the
// old fade sent). The time spent outside UpdateLayeredWindow is the Cue's own work (the pixel loop);
// UpdateLayeredWindow itself is the system's (Wine's here, DWM's on Windows) and is reported apart.
//   build (from the repository):
//   F="-std=c++20 -O2 -DUNICODE -D_UNICODE -DNOMINMAX -D_WIN32_WINNT=0x0A00 -DWIN32_LEAN_AND_MEAN -I."
//   x86_64-w64-mingw32-g++-posix $F -DUpdateLayeredWindow=bench_ulw -c src/win/ambient.cpp -o ambient_bench.o
//   x86_64-w64-mingw32-g++-posix $F tools/benchmark_reveal.cpp ambient_bench.o src/ui/atmosphere.cpp -static -ldwmapi -lgdi32 -luser32 -o benchmark_reveal.exe
#include <windows.h>
#include <chrono>
#include <cstdio>
#include "src/win/ambient.h"
static double now_s() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static int g_paints, g_opacity; static long long g_px; static double g_ulw;
static BOOL WINAPI bench_ulw(HWND h, HDC d, POINT* p, SIZE* s, HDC src, POINT* sp, COLORREF k, BLENDFUNCTION* bf, DWORD f) {
  if (s) { ++g_paints; g_px += (long long)s->cx * s->cy; } else ++g_opacity;
  const double t = now_s();
  BOOL r = UpdateLayeredWindow(h, d, p, s, src, sp, k, bf, f);
  g_ulw += now_s() - t;
  return r;
}
// ambient.cpp calls it through the import slot its renamed declaration expects
extern "C" { BOOL(WINAPI* __imp_bench_ulw)(HWND, HDC, POINT*, SIZE*, HDC, POINT*, COLORREF, BLENDFUNCTION*, DWORD) = bench_ulw; }
#ifndef OLD_AMBIENT
bool system_window_animation() { return true; }
#endif
struct Run { int paints = 0, opacity = 0; long long px = 0; double own = 0, ulw = 0; };
template <class F> Run run(F f) {
  g_paints = g_opacity = 0; g_px = 0; g_ulw = 0;
  const double t = now_s(); f(); const double all = now_s() - t;
  return {g_paints, g_opacity, g_px, all - g_ulw, g_ulw};
}
int main() {
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  HINSTANCE inst = GetModuleHandleW(nullptr);
  WNDCLASSW wc{}; wc.lpfnWndProc = DefWindowProcW; wc.hInstance = inst; wc.lpszClassName = L"bench.owner"; RegisterClassW(&wc);
  struct Size { int w, h; const char* name; } sizes[] = {{1280, 720, "1280x720"}, {1600, 900, "1600x900"}, {1900, 1040, "1900x1040"}};
  Rgb z[26]; double l[26];
  for (int k = 0; k < 26; ++k) { z[k] = {(uint8_t)(40 + k * 7), (uint8_t)(200 - k * 5), 120}; l[k] = 0.8; }
  for (auto& sz : sizes) {
    HWND owner = CreateWindowExW(0, wc.lpszClassName, L"o", WS_POPUP | WS_VISIBLE, (1920 - sz.w) / 2, (1080 - sz.h) / 2, sz.w, sz.h, nullptr, nullptr, inst, nullptr);
    Ambient a; a.create(inst);
    const double tb = now_s();
    a.update(owner, z, l, true, 0.6, 1 / 30.0);  // the first: the shape for this size is made, then painted
    const double build = now_s() - tb;
    for (int rep = 0; rep < 2; ++rep) {
      // the first entrance builds the shape: measured apart from the rest
      Run in = run([&] { for (int i = 0; i < 60; ++i) a.update(owner, z, l, true, 0.6, 1 / 30.0); });
      Run col = run([&] { for (int i = 0; i < 30; ++i) { z[i % 26].g ^= 8; a.update(owner, z, l, true, 0.6, 1 / 30.0); } });
      Run out = run([&] { for (int i = 0; i < 30; ++i) a.update(owner, z, l, false, 0.6, 1 / 30.0); });
      if (rep == 0) continue;
      auto ms = [](double s) { return s * 1000; };
      std::printf("%-9s entrance (2 s of ticks): %2d paints + %2d opacity, own %.1f ms, system %.1f ms | exit (1 s): %2d paints + %2d opacity, own %.1f ms, system %.1f ms | "
                  "a colour paint: own %.2f ms, system %.2f ms, %.0f k px | the shape made: %.1f ms\n",
                  sz.name, in.paints, in.opacity, ms(in.own), ms(in.ulw), out.paints, out.opacity, ms(out.own), ms(out.ulw),
                  ms(col.own) / col.paints, ms(col.ulw) / col.paints, col.px / 1000.0 / col.paints, ms(build));
    }
    a.destroy(); DestroyWindow(owner);
  }
}
