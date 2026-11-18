// Portable per-frame cost of the screens the 1.4 redesign drew again, at 1080p and 4K: the home
// with its pictures (the one to continue lit, the pointer on it) and as a list of words, the
// switch between the two, the welcome's three screens, what's new coming in and at rest, and
// the player with its one panel open (settings, the subtitle style). CPU time per frame and
// heap allocations, as tools/benchmark_frames.cpp. Not a native measurement (no Windows,
// engine, GPU or compositor). Link against the portable objects of tools/test.sh:
//   g++ -std=c++20 -O2 tools/benchmark_redesign.cpp build/tests/obj/*.o -o build/bench-redesign
//   build/bench-redesign [frames] [only]
#include "../src/ui/home.h"
#include "../src/ui/news_view.h"
#include "../src/ui/onboarding.h"
#include "../src/ui/play.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
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
struct Result { double ms = 0, allocs = 0; uint64_t digest = 0; };
static uint64_t fnv(uint64_t d, uint64_t v) { d ^= v; return d * 1099511628211ull; }
static void report(const char* name, int w, int h, const Result& r) {
  std::printf("%-22s %4dx%-4d  %8.3f ms/frame  %7.1f allocs/frame  digest %016llx\n", name, w, h, r.ms, r.allocs, (unsigned long long)r.digest);
}
template <class F> static Result measure(int frames, F&& one, const Canvas& out) {
  const size_t a0 = allocations;
  const auto start = Clock::now();
  for (int i = 0; i < frames; ++i) one(i);
  Result r;
  r.ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count() / frames;
  r.allocs = double(allocations - a0) / frames;
  r.digest = 1469598103934665603ull;
  for (uint32_t p : out.px) r.digest = fnv(r.digest, p);
  return r;
}

static std::shared_ptr<const HomeThumb> picture(int w, int h, uint32_t seed, bool cover) {
  auto t = std::make_shared<HomeThumb>();
  t->w = w; t->h = h; t->cover = cover;
  t->px.resize((size_t)w * h);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x)
      t->px[(size_t)y * w + x] = 0xff000000u | ((seed * 53 + x * 3) & 255) << 16 | ((seed * 91 + y * 5) & 255) << 8 | ((x ^ y) & 255);
  return t;
}
static std::vector<HomeRow> rows() {
  std::vector<HomeRow> r;
  const char* n[] = {"Dune Part Two", "Severance", "Music Baby", "The Bear", "Podcast 12", "Arcane"};
  for (int i = 0; i < 6; ++i) {
    HomeRow h;
    h.name = n[i]; h.sub = i == 1 ? "S01E04 · 53 min left" : "Some artist";
    h.progress = i == 0 ? 0.4 : i == 1 ? 0.7 : -1;
    h.kind = i == 2 ? HomeKind::Music : i == 1 || i == 3 ? HomeKind::Episode : HomeKind::Video;
    h.audio = i == 2; h.seed = n[i];
    h.thumb = picture(i == 2 ? 96 : 160, i == 2 ? 96 : 90, (uint32_t)i + 1, i == 2);
    r.push_back(h);
  }
  return r;
}

enum class HomeCase { Covers, CoversHover, List, Swap };
static Result home(int w, int h, double s, HomeCase hc, int frames) {
  HomeView v;
  v.covers = hc != HomeCase::List;
  v.set_size(w, h, s);
  v.set_rows(rows());
  v.enter(0);
  Canvas c, scratch;
  double t = 0;
  for (int i = 0; i < 120; ++i) { t += 1.0 / 60; v.tick(t, 1.0 / 60); v.render(c, t); }
  if (hc == HomeCase::CoversHover) { const Rect r = v.row_rect(0); v.mouse_move(r.cx(), r.cy(), t); }
  return measure(frames, [&](int i) {
    t += 1.0 / 60;
    if (hc == HomeCase::Swap && i % 30 == 0) {  // "mostrar imagens" on and off, again and again
      v.swap_animated(t, &scratch);
      v.covers = !v.covers;
      v.set_size(w, h, s);
      v.set_rows(rows());
    }
    v.tick(t, 1.0 / 60);
    v.render(c, t);
  }, c);
}

static Result welcome(int w, int h, double s, int page, bool pointer, int frames) {
  OnboardingView ob;
  ob.set_size(w, h, s);
  double t = 10;
  ob.start(t);
  Canvas c;
  for (int i = 0; i < 260; ++i) { t += 1.0 / 60; ob.tick(t, 1.0 / 60); }
  if (page > 0) { ob.go(page, t); for (int i = 0; i < 90; ++i) { t += 1.0 / 60; ob.tick(t, 1.0 / 60); } }
  ob.render(c, t);
  return measure(frames, [&](int i) {
    t += 1.0 / 60;
    if (pointer) ob.track(w / 4 + (i * 7) % (w / 2), h / 3 + (i * 3) % (h / 3), t);
    ob.tick(t, 1.0 / 60);
    ob.render(c, t);
  }, c);
}

static Result news(int w, int h, double s, bool entering, int frames) {
  NewsView n;
  n.version = "1.4";
  n.set_size(w, h, s);
  double t = 5;
  n.open(t);
  Canvas c;
  if (!entering) for (int i = 0; i < 200; ++i) { t += 1.0 / 60; n.tick(t, 1.0 / 60); }
  n.render(c, t);
  return measure(frames, [&](int i) {
    t += 1.0 / 60;
    if (entering && i % 150 == 0) n.open(t);  // its 2.6 s entrance, again and again
    n.tick(t, 1.0 / 60);
    n.render(c, t);
  }, c);
}

static Result panel(int w, int h, double s, int which, int frames) {
  PlayView p;
  p.in.dur = 7200;
  p.in.time = 600;
  p.resize(w, h, s);
  p.reset(0);
  double t = 0;
  for (int i = 0; i < 120; ++i) { t += 1.0 / 60; p.in.time += 1.0 / 60; p.changed(t); p.activity(t); p.tick(t, 1.0 / 60); }
  p.open_panel_for_preview(which, t);
  Canvas none;
  Result r = measure(frames, [&](int i) {
    t += 1.0 / 60;
    p.in.time += 1.0 / 60;
    p.changed(t);
    p.activity(t);
    p.mouse_move(w - w / 6 + (i % 40), h / 2 + (i % 30), t);  // the pointer over the panel's rows
    p.tick(t, 1.0 / 60);
    for (auto& L : p.layers) L.dirty = false;
  }, none);
  for (const auto& L : p.layers)
    if (L.visible) for (uint32_t v : L.c.px) r.digest = fnv(r.digest, v);
  return r;
}

int main(int argc, char** argv) {
  const int frames = argc > 1 ? std::atoi(argv[1]) : 300;
  const char* only = argc > 2 ? argv[2] : "";
  struct Size { int w, h; double s; } sizes[] = {{1920, 1080, 1.5}, {3840, 2160, 3}};
  for (const auto& z : sizes) {
    auto want = [&](const char* n) { return !*only || std::strstr(n, only); };
    if (want("home-covers")) report("home-covers", z.w, z.h, home(z.w, z.h, z.s, HomeCase::Covers, frames));
    if (want("home-covers-hover")) report("home-covers-hover", z.w, z.h, home(z.w, z.h, z.s, HomeCase::CoversHover, frames));
    if (want("home-list")) report("home-list", z.w, z.h, home(z.w, z.h, z.s, HomeCase::List, frames));
    if (want("home-swap")) report("home-swap", z.w, z.h, home(z.w, z.h, z.s, HomeCase::Swap, frames));
    if (want("welcome-1")) report("welcome-1", z.w, z.h, welcome(z.w, z.h, z.s, 0, false, frames));
    if (want("welcome-1-pointer")) report("welcome-1-pointer", z.w, z.h, welcome(z.w, z.h, z.s, 0, true, frames));
    if (want("welcome-2")) report("welcome-2", z.w, z.h, welcome(z.w, z.h, z.s, 1, false, frames));
    if (want("welcome-3")) report("welcome-3", z.w, z.h, welcome(z.w, z.h, z.s, 2, false, frames));
    if (want("news-entering")) report("news-entering", z.w, z.h, news(z.w, z.h, z.s, true, frames));
    if (want("news-resting")) report("news-resting", z.w, z.h, news(z.w, z.h, z.s, false, frames));
    if (want("panel-settings")) report("panel-settings", z.w, z.h, panel(z.w, z.h, z.s, 11, frames));
    if (want("panel-style")) report("panel-style", z.w, z.h, panel(z.w, z.h, z.s, 1, frames));
  }
}
