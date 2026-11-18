// (17/11, Biel)
// 1. Play/pause pressed with the pointer on the bar shows no sign over the picture: the button answers
//    under the eye. ⏪ ⏩ keep theirs (their icon does not say "15 s"; clicks that follow add up), at
//    the window's edge as the keys'. A click on the picture, the keys and the phone keep theirs.
// 2. The bar whose width changes in place ("limpar fila" takes ⏮ ⏭ away) keeps its centre: its right
//    edge, and the list hanging from it, used to jump in and slide back out.
#define private public
#include "../src/ui/play.h"
#undef private
#include <cassert>
#include <cstdio>
#include <cstdlib>

int main() {
  int n = 0;
  for (double s : {1.5, 2.0, 3.0})
    for (bool fre : {false, true}) {
      PlayView p;
      double u = 1;
      const int W = (int)(1280 * s / 1.5), H = (int)(720 * s / 1.5);
      p.in.dur = 300; p.in.time = 60; p.in.controls_free = fre;
      p.resize(W, H, s); p.reset(u);
      auto run = [&](double secs) { for (int i = 0; i < (int)(secs * 60); ++i) { u += 1 / 60.0; p.tick(u, 1 / 60.0); p.take_actions(); } };
      Rect d = p.dock_rect();
      p.mouse_move(d.cx(), d.cy(), u);
      run(0.5);
      auto press = [&](int id) {
        Rect dr = p.dock_rect();
        for (auto& hb : p.hots_)
          if (hb.id == id) { p.mouse_move(dr.x + hb.r.cx(), dr.y + hb.r.cy(), u); p.mouse_down(dr.x + hb.r.cx(), dr.y + hb.r.cy(), u); p.mouse_up(dr.x + hb.r.cx(), dr.y + hb.r.cy(), u); return true; }
        return false;
      };
      for (int id : {PlayView::B_PLAY, PlayView::B_FWD, PlayView::B_REW}) {
        const uint64_t g = p.flash_gen_;
        const bool paused = p.in.paused;
        if (!press(id)) continue;
        if (id == PlayView::B_PLAY) assert(p.flash_gen_ == g && p.in.paused != paused);  // no sign; it did its work
        else assert(p.flash_gen_ != g && p.flash_kind_ == 2 && std::abs(p.flash_seek_) == 15);  // "15 s
        run(0.3);
        ++n;
      }
      // the keys and the phone: the sign as before
      uint64_t g = p.flash_gen_;
      p.key("space", false, false, u);
      assert(p.flash_gen_ != g);
      g = p.flash_gen_;
      p.key("right", false, false, u);
      assert(p.flash_gen_ != g && p.flash_kind_ == 2);
      g = p.flash_gen_;
      p.remote_toggle_pause(u);
      assert(p.flash_gen_ != g);
      // a click on the picture: the sign as before
      run(1);
      g = p.flash_gen_;
      p.mouse_move(W / 2, H / 3, u); p.mouse_down(W / 2, H / 3, u); p.mouse_up(W / 2, H / 3, u);
      run(0.6);
      assert(p.flash_gen_ != g);
    }
  // 2. the list cleared with the bar's background on: the bar's right edge moves once, never back
  for (double s : {1.5, 2.0, 3.0}) {
    PlayView p;
    double u = 1;
    const int W = (int)(1280 * s / 1.5), H = (int)(720 * s / 1.5);
    p.in.dur = 300; p.in.time = 10; p.in.controls_free = false;
    for (uint32_t i = 0; i < 3; ++i) p.in.queue.push_back({i + 1, i == 0 ? "a.mp4" : i == 1 ? "b.mp4" : "c.mp4", i == 0, false});
    p.in.queue_active = true; p.in.has_next = p.in.has_prev = true; p.in.queue_revision = 1;
    p.resize(W, H, s); p.reset(u); p.changed(u);
    auto run = [&](int frames, int* lo, int* hi) {
      for (int i = 0; i < frames; ++i) {
        u += 1 / 60.0; p.tick(u, 1 / 60.0); p.take_actions();
        const Rect d = p.dock_rect();
        if (lo) { *lo = std::min(*lo, d.r()); *hi = std::max(*hi, d.r()); }
      }
    };
    p.mouse_move(W / 2, H - 60, u);
    run(30, nullptr, nullptr);
    p.open_panel(PlayView::Panel::Queue, PlayView::B_QUEUE, u);
    run(40, nullptr, nullptr);
    const int w0 = p.dock_rect().w;
    p.in.queue.clear(); p.in.queue_active = false; p.in.has_next = p.in.has_prev = false; ++p.in.queue_revision; p.changed(u);
    int lo = 1 << 30, hi = -1;
    run(40, &lo, &hi);
    const Rect d = p.dock_rect();
    assert(d.w < w0);                                       // ⏮ ⏭ left the bar
    assert(std::abs(d.cx() - W / 2) <= (int)s);              // still centred
    assert(lo == d.r() && hi == d.r());                     // the right edge went there once and stayed
    ++n;
  }
  std::printf("bar_click_sign_test: ok (%d)\n", n);
}
