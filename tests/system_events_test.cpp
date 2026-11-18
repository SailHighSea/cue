// Three things the system does under Cue: a song kept only in the cloud is never read for its
// cover (the phone browsing a folder, the "up next" list); full screen fits its monitor again when
// the screens change at the same scale; going to sleep writes where the film is.
#include "../src/core/file_io.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

static std::string read(const char* p) { std::ifstream f(p); std::stringstream b; b << f.rdbuf(); return b.str(); }
static std::string body(const std::string& s, const std::string& head) {
  const size_t a = s.find(head);
  assert(a != std::string::npos);
  return s.substr(a, s.find("\n}\n", a) - a);
}

int main() {
  // the attributes OneDrive (and any cloud provider) gives a file that is not on this PC
  assert(cloud_only_attributes(0x00001000ul) && cloud_only_attributes(0x00040000ul) && cloud_only_attributes(0x00400000ul));
  assert(cloud_only_attributes(0x00400000ul | 0x20ul /*ARCHIVE*/ | 0x00080000ul /*PINNED: no*/));
  assert(!cloud_only_attributes(0x20ul) && !cloud_only_attributes(0x80ul /*NORMAL*/) && !cloud_only_attributes(0x00080000ul /*PINNED*/) &&
         !cloud_only_attributes(0x00100000ul /*UNPINNED, but here*/));
  assert(!cloud_only_attributes(0xFFFFFFFFul));  // not there at all: nothing to bring down
  const std::string app = read("src/win/app.cpp"), server = read("src/win/remote_server.cpp"), thumbs = read("src/win/recent_thumbs.cpp");
  // one rule, used by the home's pictures and by the phone's and the list's thumbnails
  const std::string image = body(server, "static std::string make_image(");
  const size_t cloud = image.find("!cloud_only_attributes(GetFileAttributesW(widen(path).c_str()))");
  assert(cloud != std::string::npos && cloud < image.find("cover_image(path, src, w, h, 512)"));
  assert(image.find("shell_pixels(path, W * 4, H * 4, src, w, h)") != std::string::npos);  // (the shell's thumbnail stays)
  assert(body(app, "static bool home_shot_reachable(").find("return !cloud_only_attributes(a);") != std::string::npos);
  // (16/11: the kept picture answers first; the cloud check still comes before the cover is read)
  assert(thumbs.find("cloud_only_attributes(attrs)") != std::string::npos &&
         thumbs.find("cloud_only_attributes(attrs)") < thumbs.find("cover_image(j.path, px, w, h, kCoverSide)"));
  // full screen: a change of the screens (WM_DISPLAYCHANGE) fits it again, as a change of scale did
  const size_t display = app.find("case WM_DISPLAYCHANGE:");
  assert(display != std::string::npos && app.find("if (fullscreen_) fit_fullscreen();", display) < app.find("case WM_POWERBROADCAST:", display));
  assert(app.find("if (fullscreen_) { fit_fullscreen(); return 0; }", app.find("case WM_DPICHANGED:")) != std::string::npos);
  const std::string fit = body(app, "void App::fit_fullscreen() {");
  assert(fit.find("MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST)") != std::string::npos && fit.find("mi.rcMonitor") != std::string::npos);
  assert(fit.find("if (EqualRect(&now_at, &mi.rcMonitor)) return;") != std::string::npos);
  // sleep: the film's place goes to disk before the PC sleeps, and only if a save was waiting
  const size_t power = app.find("case WM_POWERBROADCAST:");
  const size_t suspend = app.find("if (w == PBT_APMSUSPEND && mode_ == Mode::Play && loaded_) {", power);
  assert(suspend != std::string::npos && suspend < app.find("break;", app.find("save_now(true);", suspend)));
  assert(app.find("save_position();", suspend) < app.find("if (KillTimer(hwnd_, T_PERSIST)) save_now(true);", suspend));
  std::puts("System events: cloud-only songs never read for covers (phone, up next, home), full screen refits on display changes, the film's place saved on sleep passed");
}
