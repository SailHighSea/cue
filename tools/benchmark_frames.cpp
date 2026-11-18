// Portable per-frame interface cost at 1080p and 4K: the home (resting, eyes following the
// pointer) and the player (controls in view with the pointer moving, controls resting, the
// timeline hovered, music). Counts CPU time per frame, heap allocations, and the bytes the
// player would hand to the engine (dirty, visible layers, at most once per frame).
// Not a native measurement: no Windows, engine, GPU or compositor. Link against the portable
// objects of tools/test.sh:
//   g++ -std=c++20 -O2 tools/benchmark_frames.cpp build/tests/obj/*.o -o build/bench-frames
#include "../src/ui/home.h"
#include "../src/ui/play.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

static size_t allocations = 0;
void* operator new(size_t n) {
  ++allocations;
  if (void* p = std::malloc(n ? n : 1)) return p;
  throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }

using Clock = std::chrono::steady_clock;
struct Result { double ms_per_frame = 0; double allocs_per_frame = 0; double mb_per_s = 0; uint64_t digest = 0; };

static uint64_t fnv(uint64_t d, uint64_t v) { d ^= v; return d * 1099511628211ull; }

static void report(const char* name, int w, int h, const Result& r) {
  std::printf("%-26s %4dx%-4d  %8.3f ms/frame  %7.1f allocs/frame  %8.2f MB/s uploaded  digest %016llx\n", name, w, h,
              r.ms_per_frame, r.allocs_per_frame, r.mb_per_s, (unsigned long long)r.digest);
}

// The home: what App::present_home does each frame (render, then compare with the frame on
// screen to find the rectangle that changed; or, when only the eyes moved, draw their corner of
// the window alone, in place: HomeView::eyes_only_key and face_reach).
static Result home(int w, int h, double s, bool track, int frames, bool fades = false) {
  HomeView v;
  v.set_size(w, h, s);
  v.set_rows({{"Twin.Peaks.S01E01.mkv", "47:12", .5}, {"Twin.Peaks.S01E02.mkv", "46:01", .2}, {"clip.mp4", "00:30", .9}});
  v.enter(0);
  Canvas back, front;
  double t = 0;
  for (int i = 0; i < 240; ++i) { t += 1.0 / 60; v.tick(t, 1.0 / 60); v.render(back, t); std::swap(back, front); }
  const size_t a0 = allocations;
  uint64_t changed_px = 0, digest = 1469598103934665603ull;
  uint64_t key_of_front = 0;
  const auto start = Clock::now();
  for (int i = 0; i < frames; ++i) {
    t += 1.0 / 60;
    if (track) v.track(w / 4 + (i * 7) % (w / 2), h / 3 + (i * 3) % (h / 3), t);
    if (fades && i % 30 == 0) v.enter(t);  // back from a film: the home fades in, again and again
    v.tick(t, 1.0 / 60);
    const uint64_t key = v.eyes_only_key(t);
    if (key && key == key_of_front && front.w == w && front.h == h) {
      const Rect r = v.face_reach();
      std::vector<uint32_t> old((size_t)r.w * r.h);
      for (int y = 0; y < r.h; ++y) std::memcpy(&old[(size_t)y * r.w], &front.px[(size_t)(r.y + y) * w + r.x], (size_t)r.w * 4);
      front.set_clip(r);
      v.render(front, t);
      front.reset_clip();
      int y0 = -1, y1 = -1;
      for (int y = 0; y < r.h; ++y)
        if (std::memcmp(&old[(size_t)y * r.w], &front.px[(size_t)(r.y + y) * w + r.x], (size_t)r.w * 4)) { if (y0 < 0) y0 = y; y1 = y; }
      if (y0 >= 0) changed_px += (uint64_t)(y1 - y0 + 1) * w;  // (counted as whole rows, as below)
      continue;
    }
    key_of_front = key;
    v.render(back, t);
    if (back.w == front.w && back.h == front.h) {
      for (int y = 0; y < back.h; ++y)
        if (std::memcmp(&back.px[(size_t)y * back.w], &front.px[(size_t)y * back.w], (size_t)back.w * 4)) changed_px += back.w;
    }
    std::swap(back, front);
  }
  const double ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
  for (uint32_t p : front.px) digest = fnv(digest, p);
  Result r;
  r.ms_per_frame = ms / frames;
  r.allocs_per_frame = double(allocations - a0) / frames;
  r.mb_per_s = changed_px * 4.0 / (frames / 60.0) / 1e6;  // rows copied to the window
  r.digest = digest;
  return r;
}

enum class PlayCase { DockMoving, DockResting, TimelineHover, Music, MusicResting, DockEnters, PanelOpens };

static Result play(int w, int h, double s, PlayCase pc, int frames) {
  PlayView p;
  p.in.dur = 7200;
  p.in.time = 600;
  p.in.audio_only = p.in.cover_scene = p.in.has_cover = pc == PlayCase::Music || pc == PlayCase::MusicResting;
  if (p.in.audio_only) {
    p.in.dur = 240;
    p.in.music_title = "A deliberately long song title";
    p.in.music_artist = "Some artist";
    p.in.music_scene = MusicScene::wide();
  }
  p.resize(w, h, s);
  p.reset(0);
  double t = 0;
  auto upload = [&](uint64_t& bytes) {
    for (auto& L : p.layers) {
      if (!L.dirty) continue;
      L.dirty = false;
      if (L.visible) bytes += L.c.px.size() * 4;
    }
  };
  uint64_t warm = 0;
  for (int i = 0; i < 240; ++i) {
    t += 1.0 / 60;
    p.in.time += 1.0 / 60;
    p.changed(t);
    if (pc != PlayCase::DockResting && pc != PlayCase::MusicResting) p.activity(t);
    p.mouse_move(w / 2 + i % 50, h / 2, t);
    p.tick(t, 1.0 / 60);
    upload(warm);
  }
  // resting: the controls leave after their idle time; let them go
  if (pc == PlayCase::DockResting || pc == PlayCase::MusicResting || pc == PlayCase::DockEnters)
    for (int i = 0; i < 600; ++i) { t += 1.0 / 60; p.in.time += 1.0 / 60; p.changed(t); p.tick(t, 1.0 / 60); upload(warm); }
  // the timeline's row: the first row of the dock where the pointer hovers it
  int timeline_y = -1;
  if (pc == PlayCase::TimelineHover) {
    const auto& dock = p.layers[PlayView::L_DOCK];
    for (int y = dock.y; y < dock.y + dock.c.h && timeline_y < 0; ++y) {
      p.activity(t);
      p.mouse_move(dock.x + dock.c.w / 2, y, t);
      if (p.preview_hovering()) timeline_y = y;
    }
    if (timeline_y < 0) { std::printf("timeline not found\n"); std::exit(1); }
    p.set_thumb(std::vector<uint32_t>(p.thumb_hover_width() * p.thumb_hover_width() * 9 / 16, 0xff336699u), p.thumb_hover_width(), p.thumb_hover_width() * 9 / 16, p.in.time);
  }
  const size_t a0 = allocations;
  uint64_t bytes = 0, digest = 1469598103934665603ull;
  const auto start = Clock::now();
  for (int i = 0; i < frames; ++i) {
    t += 1.0 / 60;
    p.in.time += 1.0 / 60;
    p.changed(t);
    switch (pc) {
      case PlayCase::DockMoving: p.activity(t); p.mouse_move(w / 2 + (i * 5) % 300, h / 2 - (i * 3) % 120, t); break;
      case PlayCase::TimelineHover: {
        p.activity(t);
        const auto& dock = p.layers[PlayView::L_DOCK];
        p.mouse_move(dock.x + dock.c.w / 4 + (i * 3) % std::max(1, dock.c.w / 2), timeline_y, t);
        break;
      }
      case PlayCase::Music: p.activity(t); p.mouse_move(w / 2 + (i * 5) % 300, h / 2, t); break;
      case PlayCase::DockEnters:  // the pointer wakes the controls every 3 s; they leave in between
        if (i % 180 == 0) { p.activity(t); p.mouse_move(w / 2 + i % 7, h / 2, t); }
        break;
      case PlayCase::PanelOpens:  // "..." opened and closed again and again (its unfold animation)
        if (i % 40 == 0) p.open_panel_for_preview(3, t);
        if (i % 40 == 20) p.key("esc", false, false, t);
        p.activity(t);
        break;
      default: break;
    }
    p.tick(t, 1.0 / 60);
    upload(bytes);
  }
  const double ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
  for (const auto& L : p.layers) {
    digest = fnv(digest, L.visible);
    if (!L.visible) continue;
    digest = fnv(digest, (uint64_t)L.x << 32 | (uint32_t)L.y);
    for (uint32_t v : L.c.px) digest = fnv(digest, v);
  }
  Result r;
  r.ms_per_frame = ms / frames;
  r.allocs_per_frame = double(allocations - a0) / frames;
  r.mb_per_s = bytes / (frames / 60.0) / 1e6;
  r.digest = digest;
  return r;
}

int main(int argc, char** argv) {
  const int frames = argc > 1 ? std::atoi(argv[1]) : 600;
  const char* only = argc > 2 ? argv[2] : "";
  struct Size { int w, h; double s; } sizes[] = {{1920, 1080, 1.5}, {3840, 2160, 3}};
  for (const auto& z : sizes) {
    auto want = [&](const char* n) { return !*only || std::strstr(n, only); };
    if (want("home-resting")) report("home-resting", z.w, z.h, home(z.w, z.h, z.s, false, frames));
    if (want("home-fade-in")) report("home-fade-in", z.w, z.h, home(z.w, z.h, z.s, false, frames, true));
    if (want("home-eyes")) report("home-eyes", z.w, z.h, home(z.w, z.h, z.s, true, frames));
    if (want("play-dock-moving")) report("play-dock-moving", z.w, z.h, play(z.w, z.h, z.s, PlayCase::DockMoving, frames));
    if (want("play-dock-resting")) report("play-dock-resting", z.w, z.h, play(z.w, z.h, z.s, PlayCase::DockResting, frames));
    if (want("play-timeline-hover")) report("play-timeline-hover", z.w, z.h, play(z.w, z.h, z.s, PlayCase::TimelineHover, frames));
    if (want("music-pointer")) report("music-pointer", z.w, z.h, play(z.w, z.h, z.s, PlayCase::Music, frames));
    if (want("play-dock-enters")) report("play-dock-enters", z.w, z.h, play(z.w, z.h, z.s, PlayCase::DockEnters, frames));
    if (want("play-panel-opens")) report("play-panel-opens", z.w, z.h, play(z.w, z.h, z.s, PlayCase::PanelOpens, frames));
    if (want("music-resting")) report("music-resting", z.w, z.h, play(z.w, z.h, z.s, PlayCase::MusicResting, frames));
  }
}
