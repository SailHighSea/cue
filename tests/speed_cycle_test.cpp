// The speed's button (16/11): a click steps through 1 → 1.25 → 1.5 → 2 → 0.5 → 1 (from a speed
// in between, the next one up); the right click (and Shift+Enter on it) opens its whole menu; a
// click while the menu is open only closes it; the number rolls to the next one in 0.18 s, up when
// faster and down when slower, and simply changes with "reduce motion".
#define private public
#include "../src/ui/play.h"
#undef private
#include "../src/ui/gfx.h"
#include <cassert>
#include <cmath>
#include <cstdio>

static bool near(double a, double b) { return std::fabs(a - b) < 1e-6; }

int main() {
  // the loop
  const double want[][2] = {{1, 1.25}, {1.25, 1.5}, {1.5, 2}, {2, 0.5}, {0.5, 1}, {0.25, 1}, {0.75, 1}, {1.1, 1.25},
                            {1.35, 1.5}, {1.75, 2}, {2.5, 0.5}, {3, 0.5}};
  for (auto& w : want) assert(near(PlayView::next_speed(w[0]), w[1]));
  int n = 0;
  for (bool audio : {false, true})
    for (bool rm : {false, true}) {
      reduce_motion() = rm;
      PlayView p;
      p.in.dur = 600; p.in.time = 30; p.in.audio_only = audio;
      p.resize(1280, 720, 2); p.reset(1);
      double u = 1;
      p.mouse_move(640, 700, u);
      for (int i = 0; i < 40; ++i) { u += 1 / 60.0; p.tick(u, 1 / 60.0); }
      Rect sr{};
      for (auto& hb : p.hots_) if (hb.id == PlayView::B_SPEED) sr = hb.r;
      assert(sr.w > 0);
      { const Rect d = p.dock_rect(); sr.x += d.x; sr.y += d.y; }  // (window coordinates)
      assert(p.hit_dock(sr.cx(), sr.cy()) == PlayView::B_SPEED);
      // clicks: 1 → 1.25 → 1.5 → 2 → 0.5 → 1, one step each (a click a little later each time)
      const double seq[] = {1.25, 1.5, 2, 0.5, 1};
      for (double v : seq) {
        p.take_actions();
        u += 0.7;
        p.mouse_down(sr.cx(), sr.cy(), u); p.mouse_up(sr.cx(), sr.cy(), u);
        auto a = p.take_actions();
        bool said = false;
        for (auto& x : a) if (x.a == PA::Speed) { assert(near(x.v, v)); said = true; }
        assert(said && near(p.in.speed, v));
        assert(p.panel_ == PlayView::Panel::None);
        // the number: rolling (or not, with reduce motion), then still
        u += 1 / 60.0; p.tick(u, 1 / 60.0);
        const bool rolling = u - p.speed_roll_t0_ < 0.18;
        assert(rolling == !rm);
        for (int i = 0; i < 20; ++i) { u += 1 / 60.0; p.tick(u, 1 / 60.0); }
        assert(u - p.speed_roll_t0_ > 0.18);
        ++n;
      }
      // the direction: 2 → 0.5 rolls down
      p.in.speed = 2; for (int i = 0; i < 30; ++i) { u += 1 / 60.0; p.mouse_move(sr.cx(), sr.cy() + (i & 1), u); p.tick(u, 1 / 60.0); }
      p.mouse_down(sr.cx(), sr.cy(), u); p.mouse_up(sr.cx(), sr.cy(), u);
      u += 1 / 60.0; p.tick(u, 1 / 60.0);
      if (!rm) assert(p.speed_roll_dir_ == -1 && p.speed_roll_from_ == "2");
      // the right click: the whole menu; again: closed; a click with it open only closes it
      p.take_actions();
      for (int i = 0; i < 20; ++i) { u += 1 / 60.0; p.mouse_move(sr.cx(), sr.cy() + (i & 1), u); p.tick(u, 1 / 60.0); }
      p.right_click(sr.cx(), sr.cy(), u);
      assert(p.panel_ == PlayView::Panel::Speed);
      for (int i = 0; i < 20; ++i) { u += 1 / 60.0; p.tick(u, 1 / 60.0); }
      p.right_click(sr.cx(), sr.cy(), u);
      assert(p.panel_ == PlayView::Panel::None);
      for (int i = 0; i < 20; ++i) { u += 1 / 60.0; p.tick(u, 1 / 60.0); }
      p.right_click(sr.cx(), sr.cy(), u);
      for (int i = 0; i < 20; ++i) { u += 1 / 60.0; p.tick(u, 1 / 60.0); }
      const double before = p.in.speed;
      p.take_actions();
      p.mouse_down(sr.cx(), sr.cy(), u); p.mouse_up(sr.cx(), sr.cy(), u);
      assert(p.panel_ == PlayView::Panel::None && near(p.in.speed, before));
      for (auto& x : p.take_actions()) assert(x.a != PA::Speed);
      // the keyboard: Enter on the button steps, Shift+Enter opens the menu
      for (int i = 0; i < 20; ++i) { u += 1 / 60.0; p.tick(u, 1 / 60.0); }
      p.kb_ = true; p.focus_ = PlayView::B_SPEED;
      p.key("enter", true, false, u);
      assert(p.panel_ == PlayView::Panel::Speed);
      ++n;
    }
  reduce_motion() = false;
  std::printf("speed_cycle_test: ok (%d)\n", n);
}
