// sysmsg.exe <what>: sends system notifications to Cue's window as Windows would
#include <windows.h>
#include <dbt.h>
#include <cstdio>
#include <cstring>
int main(int argc, char** argv) {
  HWND w = FindWindowW(L"tinyoffice.Cue", nullptr); if (!w) { puts("no window"); return 1; }
  const char* k = argc > 1 ? argv[1] : "";
  DWORD_PTR r = 0;
  auto send = [&](UINT m, WPARAM a, LPARAM b) { LRESULT ok = SendMessageTimeoutW(w, m, a, b, SMTO_NORMAL, 5000, &r); printf("%s -> %s\n", k, ok ? "answered" : "TIMEOUT"); };
  if (!strcmp(k, "suspend")) send(WM_POWERBROADCAST, PBT_APMSUSPEND, 0);
  else if (!strcmp(k, "resume")) { send(WM_POWERBROADCAST, PBT_APMRESUMEAUTOMATIC, 0); send(WM_POWERBROADCAST, PBT_APMRESUMESUSPEND, 0); }
  else if (!strcmp(k, "display")) send(WM_DISPLAYCHANGE, 32, MAKELPARAM(1024, 768));
  else if (!strcmp(k, "display0")) send(WM_DISPLAYCHANGE, 0, 0);
  else if (!strcmp(k, "dpi")) { RECT rc{100, 100, 1400, 900}; send(WM_DPICHANGED, MAKEWPARAM(192, 192), (LPARAM)&rc); }
  else if (!strcmp(k, "dpi96")) { RECT rc{100, 100, 900, 600}; send(WM_DPICHANGED, MAKEWPARAM(96, 96), (LPARAM)&rc); }
  else if (!strcmp(k, "dpi480")) { RECT rc{0, 0, 4000, 3000}; send(WM_DPICHANGED, MAKEWPARAM(480, 480), (LPARAM)&rc); }
  else if (!strcmp(k, "dpitiny")) { RECT rc{0, 0, 2, 2}; send(WM_DPICHANGED, MAKEWPARAM(144, 144), (LPARAM)&rc); }
  else if (!strcmp(k, "setting")) send(WM_SETTINGCHANGE, 0, (LPARAM)L"ImmersiveColorSet");
  else if (!strcmp(k, "device")) send(WM_DEVICECHANGE, DBT_DEVNODES_CHANGED, 0);
  else if (!strcmp(k, "theme")) send(WM_THEMECHANGED, 0, 0);
  else if (!strcmp(k, "endsession")) { send(WM_QUERYENDSESSION, 0, 0); }
  else puts("?");
}
