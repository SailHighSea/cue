// Cue · .tinypeep
#include <windows.h>
#include <shellapi.h>
#include <timeapi.h>

#include <string>
#include <algorithm>
#include <vector>

#include "app.h"
#include "platform.h"
#include "single_instance.h"
#include "sound_parts.h"
#include "thumbhost.h"
#include "ui_language.h"

#include <cstdio>
#include <cstring>
#include <exception>
#include <fstream>
#include <new>

// If anything goes wrong, leave a trace instead of vanishing silently.
static DWORD g_ui_thread = 0;

// Which module an address belongs to, and the offset inside it ("mpv.dll+0x1a2b3").
static std::string where(void* addr) {
  HMODULE m = nullptr;
  char name[MAX_PATH] = "?";
  if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         (LPCSTR)addr, &m) && m) {
    GetModuleFileNameA(m, name, MAX_PATH);
    const char* base = std::strrchr(name, '\\');
    char out[MAX_PATH + 32];
    std::snprintf(out, sizeof out, "%s+0x%llx", base ? base + 1 : name,
                  (unsigned long long)((uintptr_t)addr - (uintptr_t)m));
    return out;
  }
  return "unknown";
}

// the crash's line in cue.log (after what led to it), then the message telling where it is
static void crash_report(const char* line) {
  cue_log_flush(true);  // what led here first (the log keeps lines in memory between writes)
  std::ofstream f(std::filesystem::path(data_dir()) / "cue.log", std::ios::app);
  f << line << "\n";
  f.close();
  MessageBoxW(nullptr, widen(i18n::format(str::win_crash, {{"path", "%APPDATA%\\tinypeep\\Cue\\cue.log"}})).c_str(),
              L"Cue", MB_OK | MB_ICONWARNING);
}

// A C++ exception nobody caught (on any thread), or out of memory outside a window message: it
// ends Cue as before, but no longer silently (std::terminate went straight to abort: no line
// in cue.log, no message)
[[noreturn]] static void on_terminate() {
  static volatile LONG once = 0;
  if (InterlockedExchange(&once, 1) == 0) {
    std::string what = "unknown";
    try {
      if (auto e = std::current_exception()) std::rethrow_exception(e);
      what = "no exception";
    } catch (const std::bad_alloc&) { what = "out of memory";
    } catch (const std::exception& e) { what = e.what();
    } catch (...) {}
    char line[400];
    std::snprintf(line, sizeof line, "crash: %.300s, %s thread", what.c_str(), GetCurrentThreadId() == g_ui_thread ? "ui" : "engine/other");
    crash_report(line);
  }
  ExitProcess(3);
}

static LONG WINAPI on_crash(EXCEPTION_POINTERS* ep) {
  char line[600];
  const EXCEPTION_RECORD* er = ep->ExceptionRecord;
  std::string access;
  if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && er->NumberParameters >= 2) {
    char b[96];
    std::snprintf(b, sizeof b, " (%s %p)", er->ExceptionInformation[0] == 0 ? "read" : er->ExceptionInformation[0] == 1 ? "write" : "exec",
                  (void*)er->ExceptionInformation[1]);
    access = b;
  }
  std::snprintf(line, sizeof line, "crash: code 0x%08lx at %p in %s%s, %s thread", (unsigned long)er->ExceptionCode,
                er->ExceptionAddress, where(er->ExceptionAddress).c_str(), access.c_str(),
                GetCurrentThreadId() == g_ui_thread ? "ui" : "engine/other");
  crash_report(line);
  return EXCEPTION_EXECUTE_HANDLER;
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
  g_ui_thread = GetCurrentThreadId();
  SetUnhandledExceptionFilter(on_crash);
  std::set_terminate(on_terminate);
  // a recent on an empty card reader or DVD drive: Windows' own "there is no disk in the drive"
  // box would pop up out of nowhere and hold the thread that asked until someone clicks it; the
  // question just fails instead, as it does for a pulled pendrive (the helpers already did this)
  SetErrorMode(GetErrorMode() | SEM_FAILCRITICALERRORS);
  // Per-Monitor v2: every surface is drawn in physical pixels at an integer art scale.
  using SetCtxFn = BOOL(WINAPI*)(HANDLE);
  if (auto fn = reinterpret_cast<SetCtxFn>(reinterpret_cast<void*>(
          GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext"))))
    fn((HANDLE)-4 /*DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2*/);
  SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);

  int argc = 0;
  LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i) args.push_back(narrow(argv[i]));
  LocalFree(argv);
  for (auto& a : args) {
    if (a == "--thumb-host") {
      // the preview helper: silent, no window; if it fails it just ends
      SetUnhandledExceptionFilter([](EXCEPTION_POINTERS*) -> LONG { ExitProcess(3); });
      SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
      return thumb_host_main(std::find(args.begin(), args.end(), "--thumb-hwdec=auto-copy") != args.end());
    }
  }
  for (size_t i = 0; i + 4 < args.size(); ++i) {
    if (args[i] == "--sound-print") {
      // the episodes' sound helper (see sound_parts.h): silent, no window, ends with its answer
      SetUnhandledExceptionFilter([](EXCEPTION_POINTERS*) -> LONG { ExitProcess(9); });
      SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
      return sound_print_main(args[i + 2], widen(args[i + 1]), args[i + 3] == "tail", std::atof(args[i + 4].c_str()) / 1000);
    }
  }
  // the interface language, once per start: the saved choice, else Windows' display languages
  apply_ui_language(saved_language_pref(data_dir() + L"\\state.json"), windows_ui_languages());
  for (auto& a : args) {
    if (a == "--register") {
      std::string err;
      bool ok = register_open_with(&err);
      return ok ? 0 : 1;
    }
  }

  SingleInstance instance;
  auto route = instance.route(args);
  if (route == SingleInstance::Result::Forwarded) return 0;
  if (route == SingleInstance::Result::Error) {
    MessageBoxW(nullptr, widen(tr(str::win_forward_failed)).c_str(), L"Cue", MB_OK | MB_ICONWARNING);
    return 1;
  }

  if (FAILED(OleInitialize(nullptr))) return 1;  // drag & drop + file dialog
  static App app;  // lives for the whole process
  app.set_instance(&instance);
  int code = 1;
  if (app.init(inst, args, show)) code = app.run();
  OleUninitialize();
  return code;
}
