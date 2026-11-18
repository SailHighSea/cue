// A button under a pointer at rest (16/11): the bar hid, the pointer went onto the place of a
// button in a single move (the bar still hidden: nothing there yet), and stopped. The bar rose in
// under it, but only a move looked again: the button stayed grey until the mouse twitched.
// Now each frame looks again under a still pointer. The timeline still waits for a real move,
// and the bar's ground coming under a resting pointer does not hold the bar up.
#define private public
#include "../src/ui/play.h"
#undef private
#include <cassert>
#include <cstdio>

int main() {
  int n = 0;
  for (double s : {1.0, 1.5, 2.0, 3.0})
    for (auto [w, h] : {std::pair{1280, 720}, {1920, 1080}, {900, 900}})
      for (int style : {0, 1}) {
        PlayView p;
        p.in.dur = 405; p.in.time = 62; p.in.controls_free = style == 1;
        p.resize((int)(w * s / 2), (int)(h * s / 2), s); p.reset(1);
        double u = 1;
        auto run = [&](double secs) { for (int i = 0; i < (int)(secs * 60); ++i) { u += 1 / 60.0; p.tick(u, 1 / 60.0); } };
        p.mouse_move(p.w_ / 2, p.h_ / 3, u);
        run(1);
        // a button of the bar, where it rests
        Rect d = p.dock_rect(), r{};
        for (auto& hb : p.hots_) if (hb.id == PlayView::B_FS) r = hb.r;
        if (r.w <= 0) continue;
        const int bx = d.x + r.cx(), by = d.y + r.cy();
        // the pointer above it, still, until the bar hides
        p.mouse_move(bx, p.h_ / 3, u);
        run(6);
        assert(p.vis_.v <= 0.0);
        // one move onto the button: the bar is not there yet
        p.mouse_move(bx, by, u);
        assert(p.hover_ == PlayView::B_NONE);
        run(1);  // the bar rises in under the still pointer
        assert(p.hover_ == PlayView::B_FS);
        assert(p.hov_[PlayView::B_FS] >= 1.0);
        // the pointer left aside, at rest on the picture: the bar hides as before
        p.mouse_move(bx, p.h_ / 3, u);
        run(6);
        assert(p.hover_ == PlayView::B_NONE && p.vis_.v <= 0.0);
        // a key brings the bar back under a pointer resting on its ground: it still hides
        Rect dr = p.dock_rect();
        const int gx = dr.x + 2, gy = dr.y + 2;
        p.mouse_move(gx, gy - (int)(40 * s), u);
        run(6);
        p.mouse_move(gx, gy, u);  // (one move: the bar is hidden)
        run(0.5);
        assert(p.hover_ != PlayView::B_DOCKBG);
        ++n;
      }
  assert(n > 0);
  std::printf("hover_at_rest_test: ok (%d)\n", n);
}
