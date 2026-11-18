// focusdeadlock.exe <focus 0|1> <capture 0|1>: does a child window owned by another thread (like the
// engine's, WS_EX_NOPARENTNOTIFY) get destroyed while its parent's thread is blocked (not pumping)?
#include <windows.h>
#include <cstdio>
static HWND parent, child; static HANDLE created, destroyed; static int focus, capture;
static DWORD WINAPI worker(void*) {
  child = CreateWindowExW(WS_EX_NOPARENTNOTIFY, L"STATIC", L"engine", WS_CHILD | WS_VISIBLE, 0, 0, 200, 200, parent, nullptr, nullptr, nullptr);
  if (focus) SetFocus(child);
  if (capture) SetCapture(child);
  SetEvent(created);
  MSG m; DWORD t0 = GetTickCount();
  while (GetTickCount() - t0 < 300) { while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) DispatchMessageW(&m); Sleep(5); }
  DestroyWindow(child);  // the engine letting its window go
  SetEvent(destroyed);
  return 0;
}
int main(int argc, char** argv) {
  focus = argc > 1 ? atoi(argv[1]) : 0; capture = argc > 2 ? atoi(argv[2]) : 0;
  parent = CreateWindowExW(0, L"STATIC", L"Cue", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 50, 50, 400, 300, nullptr, nullptr, nullptr, nullptr);
  created = CreateEventW(nullptr, TRUE, FALSE, nullptr); destroyed = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  SetForegroundWindow(parent); SetFocus(parent);
  CreateThread(nullptr, 0, worker, nullptr, 0, nullptr);
  // pump until the child exists (as the window does while the engine starts)
  MSG m; while (MsgWaitForMultipleObjects(1, &created, FALSE, INFINITE, QS_ALLINPUT) != WAIT_OBJECT_0) while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) DispatchMessageW(&m);
  std::printf("focus on %s\n", GetFocus() == parent ? "parent" : "child or other");
  // now blocked without pumping (a synchronous call into the engine), at most 5 s
  DWORD t0 = GetTickCount(); DWORD r = WaitForSingleObject(destroyed, 5000);
  std::printf("focus=%d capture=%d: child destroyed while the parent was blocked: %s (%lu ms)\n", focus, capture, r == WAIT_OBJECT_0 ? "yes" : "NO (stuck 5 s)", GetTickCount() - t0);
  return 0;
}
