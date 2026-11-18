// Random inputs and events against PlayView and HomeView under ASan/UBSan.
#define private public
#include "../src/ui/play.h"
#undef private
#include "../src/ui/home.h"
#include "../src/core/i18n.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <random>
static std::mt19937_64 rng;
static double weird() {
  switch (rng() % 12) {
    case 0: return std::numeric_limits<double>::quiet_NaN();
    case 1: return std::numeric_limits<double>::infinity();
    case 2: return -std::numeric_limits<double>::infinity();
    case 3: return -1e9;
    case 4: return 1e12;
    case 5: return 0;
    case 6: return -0.5;
    case 7: return 1e300;
    default: return std::uniform_real_distribution<double>(0, 20000)(rng);
  }
}
static std::string name() {
  static const char* n[] = {"", "a", "Português", "日本語のタイトル", "\xff\xfe broken utf8 \xc3", "A deliberately very long track title that goes on and on and on and on and on and on and on",
                            "x\ny", "🎵🎶", "\x01\x02"};
  return n[rng() % 9];
}
static const char* keys[] = {"pageup","pagedown","space","left","right","up","down","esc","f","m","c","a","s","v","j","k","l","enter","tab","home","end","pgup","pgdn","plus","minus","zero","1","5","9","question","comma","period","backspace","delete","q","i"};
int main(int argc, char** argv) {
  uint64_t seed = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 1;
  int rounds = argc > 2 ? std::atoi(argv[2]) : 200;
  for (int round = 0; round < rounds; ++round) {
    rng.seed(seed * 1000003 + round); 
    i18n::set_language((i18n::Lang)(rng() % 8));
    PlayView p;
    static const int sizes[][2] = {{1,1},{40,30},{300,169},{320,180},{640,360},{1280,720},{1920,1080},{300,900},{400,225}};
    auto z = sizes[rng() % 9];
    double sc = (rng() % 2) ? 1 : (rng() % 2) ? 2 : 1.5;
    p.in.audio_only = rng() % 3 == 0;
    p.in.dur = weird(); p.in.time = weird(); p.in.volume = weird(); p.in.speed = (rng()%4==0)? weird() : 1.0;
    p.in.sub_delay = (rng()%3==0) ? weird() : 0;
    int na = rng() % 6, ns = rng() % 8;
    for (int i = 0; i < na; ++i) { Track t; t.id = i + 1; t.type = "audio"; t.lang = name(); t.title = name(); t.codec = name(); t.external = rng()%2; p.in.audios.push_back(t); }
    for (int i = 0; i < ns; ++i) { Track t; t.id = i + 1; t.type = "sub"; t.lang = name(); t.title = name(); t.codec = name(); p.in.subs.push_back(t); }
    p.in.aid = na ? (int64_t)(rng() % (na + 1)) : 0; p.in.sid = ns ? (int64_t)(rng() % (ns + 1)) : 0;
    p.in.music_title = name(); p.in.music_artist = name(); p.in.next_title = name();
    p.in.has_cover = p.in.cover_scene = p.in.audio_only && rng()%2;
    if (p.in.cover_scene) p.in.music_scene = MusicScene::wide();
    p.in.mini_player = rng() % 3 == 0; p.in.fullscreen = rng() % 6 == 0;
    { static const int ui[] = {-1, 0, 1, 2}; p.in.ui_scale = ui[rng() % 4]; }
    p.in.music_look = (int)(rng() % 3); p.in.controls_free = rng() % 4 == 0;
    int nq = rng() % 5; for (int i = 0; i < nq; ++i) { PlayInput::QueueItem q{(uint32_t)(i + 1), name(), i == 0, rng()%2==0}; p.in.queue.push_back(q); }
    p.in.queue_active = nq > 0; p.in.queue_revision = 1;
    { int nc = rng() % 4 == 0 ? 0 : (int)(rng() % 140); double t = rng() % 3 == 0 ? weird() : 0;
      for (int i = 0; i < nc; ++i) { if (std::isfinite(t)) p.in.chapters.push_back({std::max(0.0, t), rng() % 5 == 0 ? std::string() : name()}); t += rng() % 6 == 0 ? 0.0 : std::uniform_real_distribution<double>(0, 400)(rng); }
      p.in.chapters_revision = 1; }
    p.resize(z[0], z[1], sc);
    double t = 1; p.reset(t); p.changed(t);
    for (int step = 0; step < 400; ++step) {
      t += 1.0 / 60;
      int x = (int)(rng() % (z[0] + 40)) - 20, y = (int)(rng() % (z[1] + 40)) - 20;
      switch (rng() % 16) {
        case 0: case 1: case 2: p.activity(t); p.mouse_move(x, y, t); break;
        case 3: p.mouse_down(x, y, t); break;
        case 4: p.mouse_up(x, y, t); break;
        case 5: p.right_click(x, y, t); break;
        case 6: p.wheel((rng()%2)?1:-1, t); break;
        case 7: p.key(keys[rng() % (sizeof keys / sizeof *keys)], rng()%4==0, rng()%5==0, t); break;
        case 8: if (rng() % 4 == 0) p.open_panel(PlayView::Panel::Chapters, PlayView::B_MORE, t); else p.open_panel_for_preview((int)(rng() % 12), t); break;
        case 9: p.in.time = weird(); p.changed(t); break;
        case 10: if (rng()%20==0) { auto z2 = sizes[rng() % 9]; p.resize(z2[0], z2[1], sc); z = z2; } break;
        case 11: if (rng()%10==0) { p.in.dur = weird(); p.changed(t); } break;
        case 15: if (rng()%20==0) { if (p.in.chapters.empty()) p.in.chapters = {{0, name()}, {50, name()}}; else p.in.chapters.clear(); ++p.in.chapters_revision; p.changed(t); } break;
        case 12: { p.in.paused = !p.in.paused; p.changed(t); } break;
        case 13: if (rng()%25==0) { p.in.mini_player = !p.in.mini_player; p.resize(z[0], z[1], sc); p.changed(t); } break;
        case 14: if (rng()%25==0) { static const int ui[] = {-1, 0, 1, 2}; p.in.ui_scale = ui[rng() % 4]; p.resize(z[0], z[1], sc); p.changed(t); } break;
        default: break;
      }
      p.tick(t, 1.0 / 60);
      p.take_actions();
    }
    // the home
    HomeView h;
    auto zh = sizes[rng() % 9];
    h.set_size(zh[0], zh[1], sc);
    std::vector<HomeRow> rows; int nr = rng() % 12;
    for (int i = 0; i < nr; ++i) rows.push_back({name(), name(), (rng()%3==0) ? weird() : 0.4, name(), rng()%2==0});
    h.set_rows(rows);
    h.enter(0);
    Canvas c; double th = 0;
    for (int step = 0; step < 150; ++step) {
      th += 1.0 / 60;
      int x = (int)(rng() % (zh[0] + 40)) - 20, y = (int)(rng() % (zh[1] + 40)) - 20;
      switch (rng() % 6) { case 0: h.mouse_move(x, y, th); break; case 1: if (rng()%8==0) h.mouse_down(x, y, th); break; case 2: h.key(keys[rng() % (sizeof keys / sizeof *keys)], false, th); break; case 3: if (rng()%30==0) h.force_egg((int)(rng()%5), th); break; default: h.track(x, y, th); }
      h.tick(th, 1.0 / 60);
      if (step % 5 == 0) h.render(c, th);
    }
  }
  std::printf("fuzz seed %llu: %d rounds clean\n", (unsigned long long)seed, rounds);
}
