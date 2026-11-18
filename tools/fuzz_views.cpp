// Scenes and the onboarding at every size, including tiny and odd ones, with random input.
#include "../src/ui/news_view.h"
#include "../src/ui/onboarding.h"
#include "../src/ui/remote_view.h"
#include "../src/ui/queue_view.h"
#include "../src/core/i18n.h"
#include <cstdio>
#include <cstdlib>
#include <random>
static std::mt19937_64 rng;
static const char* keys[] = {"left","right","up","down","esc","enter","tab","space","home","end","pgup","pgdn","a","1"};
template <class V> static void drive_scene(V& v, int w, int h, double s) {
  v.set_size(w, h, s); double t = 1; v.open(t); Canvas c;
  for (int i = 0; i < 200; ++i) {
    t += 1 / 60.0; int x = (int)(rng() % (w + 20)) - 10, y = (int)(rng() % (h + 20)) - 10;
    switch (rng() % 6) { case 0: v.pointer_move(x, y, t); break; case 1: if (rng() % 6 == 0) v.pointer_down(x, y, t); break; case 2: v.key_down(keys[rng() % 14], false, t); break; case 3: if (rng() % 40 == 0) { w = 1 + rng() % 2000; h = 1 + rng() % 1200; v.set_size(w, h, s); } break; default: break; }
    v.tick(t, 1 / 60.0); if (i % 4 == 0) v.render(c, t);
  }
}
int main(int argc, char** argv) {
  uint64_t seed = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 1; int rounds = argc > 2 ? std::atoi(argv[2]) : 30;
  static const int sizes[][2] = {{1,1},{8,8},{60,40},{240,180},{320,180},{640,360},{1280,720},{300,900},{2000,120}};
  for (int r = 0; r < rounds; ++r) {
    rng.seed(seed * 7919 + r); i18n::set_language((i18n::Lang)(rng() % 8));
    auto z = sizes[rng() % 9]; double s = (rng() % 3 == 0) ? 1 : (rng() % 2) ? 2 : 1.5;
    { NewsView n; drive_scene(n, z[0], z[1], s); }
    { RemoteView v; v.set_link("http://192.168.0.10:4567/#pair"); drive_scene(v, z[0], z[1], s); double t = 10; v.connected(rng() % 2 ? "iPhone" : "", t); Canvas c; for (int i = 0; i < 60; ++i) { t += 1/60.0; v.tick(t, 1/60.0); v.render(c, t); } }
    { OnboardingView o; o.set_size(z[0], z[1], s); double t = 1; o.start(t); Canvas c;
      for (int i = 0; i < 300; ++i) { t += 1 / 60.0; int x = (int)(rng() % (z[0] + 20)) - 10, y = (int)(rng() % (z[1] + 20)) - 10;
        switch (rng() % 7) { case 0: o.mouse_move(x, y, t); break; case 1: if (rng() % 5 == 0) o.mouse_down(x, y, t); break; case 2: o.key(keys[rng() % 14], false, t); break; case 3: o.track(x, y, t); break; case 4: if (rng() % 30 == 0) o.go((int)(rng() % 6), t); break; case 5: if (rng() % 50 == 0) o.set_size(1 + rng() % 1500, 1 + rng() % 900, s); break; default: break; }
        o.tick(t, 1 / 60.0); if (i % 5 == 0) o.render(c, t); } }
    { QueueView q; std::vector<QueueEntry> e; int n = rng() % 30; for (int i = 0; i < n; ++i) e.push_back({(uint32_t)(i + 1), std::string(rng() % 60, 'x') + ".mkv", i == 0, rng() % 2 == 0});
      double qs = std::uniform_real_distribution<double>(0.3, 3)(rng);
      q.set_compact(rng() % 2); q.set_bare(rng() % 2); q.sync(e, 1); q.layout(z[0], z[1], qs); q.opened(); Canvas c; c.resize(std::max(1, z[0]), std::max(1, z[1]));
      for (int i = 0; i < 150; ++i) { int x = (int)(rng() % (z[0] + 20)) - 10, y = (int)(rng() % (z[1] + 20)) - 10;
        switch (rng() % 5) { case 0: q.move(x, y); break; case 1: q.down(x, y); break; case 2: q.up(x, y); break; case 3: q.wheel(rng() % 2 ? 1 : -1); break; default: break; }
        if (rng() % 9 == 0) q.key(keys[rng() % 14], false, false); q.tick(1 / 60.0); if (i % 5 == 0) q.draw(c, false); } }
  }
  std::printf("views fuzz seed %llu: %d rounds clean\n", (unsigned long long)seed, rounds);
}
