// Pings Cue's window every 10 ms (SendMessageTimeout WM_NULL from another process) and prints each
// time its UI thread did not answer within 50 ms: when and for how long.
#include <windows.h>
#include <cstdio>
int main(int argc, char** argv) {
  double secs = argc > 1 ? atof(argv[1]) : 60;
  LARGE_INTEGER f, t0, a, b; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&t0);
  auto ms = [&](LARGE_INTEGER x) { return (x.QuadPart - t0.QuadPart) * 1000.0 / f.QuadPart; };
  HWND w = nullptr; double worst = 0; int over50 = 0, over100 = 0, pings = 0;
  for (;;) {
    QueryPerformanceCounter(&a);
    if (ms(a) > secs * 1000) break;
    if (!w || !IsWindow(w)) { w = FindWindowW(L"tinyoffice.Cue", nullptr); if (!w) { Sleep(20); continue; } }
    DWORD_PTR r; SendMessageTimeoutW(w, WM_NULL, 0, 0, SMTO_NORMAL, 10000, &r);
    QueryPerformanceCounter(&b);
    double d = ms(b) - ms(a); ++pings;
    if (d > worst) worst = d;
    if (d >= 50) { ++over50; printf("stall %.0f ms at %.2f s\n", d, ms(a) / 1000); fflush(stdout); }
    if (d >= 100) ++over100;
    Sleep(10);
  }
  if (w && IsWindow(w)) { PostMessageW(w, WM_CLOSE, 0, 0); Sleep(3000); }
  printf("pings %d, worst %.0f ms, >=50ms %d, >=100ms %d\n", pings, worst, over50, over100);
}
