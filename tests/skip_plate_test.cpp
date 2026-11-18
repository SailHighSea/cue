// The "skip intro" plate: drawn again at once in a new interface scale (it kept its old size in
// fullscreen until the pointer touched it), and its click fades it out as it looked, never a
// one-frame disappearance or a hover redrawn mid-click (the blink).
#define private public
#include "../src/ui/play.h"
#undef private
#include <cassert>
#include <cstdio>

static void run(PlayView& p, double& t, int frames) { for (int i = 0; i < frames; ++i) { t += 1. / 60; p.tick(t, 1. / 60); } }

int main() {
  for (bool reduced : {false, true}) {
    PlayView p;
    p.in.dur = 1500; p.in.time = 100;
    reduce_motion() = reduced;
    p.in.skip_parts = {SkipPart{SkipKind::Opening, 90, 180, false}};
    double t = 1;
    p.resize(1280, 720, 3); p.reset(t); p.changed(t);
    p.mouse_move(640, 360, t);
    run(p, t, 30);
    const Layer& SL = p.layers[PlayView::L_SKIP];
    assert(p.skip_i_ == 0 && SL.visible);
    const int w3 = SL.c.w, h3 = SL.c.h;
    assert(h3 == 24 * 3);
    // fullscreen: a bigger window and scale 4; the plate is redrawn in the very next frame
    p.resize(1920, 1080, 4);
    run(p, t, 1);
    assert(SL.visible && SL.c.h == 24 * 4 && SL.c.w > w3);
    assert(p.skip_rect_.r() <= 1920 && p.skip_rect_.b() <= 1080);
    // and back
    p.resize(1280, 720, 3);
    run(p, t, 1);
    assert(SL.c.w == w3 && SL.c.h == h3);

    // the click: pointer on it (hot picture), then the press
    run(p, t, 20);
    const Rect r = p.skip_rect_;
    p.mouse_move(r.cx(), r.cy(), t);
    run(p, t, 2);
    assert(p.skip_hot_);
    const uint64_t gen = p.skip_gen_;
    const std::vector<uint32_t> hot_picture = p.skip_base_.px;
    p.mouse_down(r.cx(), r.cy(), t); p.mouse_up(r.cx(), r.cy(), t);
    auto acts = p.take_actions();
    bool skipped = false;
    for (auto& a : acts) if (a.a == PA::SkipPart) { skipped = true; assert(a.v == 180); }
    assert(skipped && p.skip_i_ < 0);
    int last = 256;
    int shown = 0;
    for (int i = 0; i < 20; ++i) {
      run(p, t, 1);
      p.mouse_move(r.cx() + (i & 1), r.cy(), t);  // the pointer still there: no hover redrawn
      if (SL.visible) {
        ++shown;
        assert(p.skip_gen_ == gen && p.skip_base_.px == hot_picture);  // the same picture, not redrawn
        assert(SL.la <= last); last = SL.la;                           // only ever fading
      }
    }
    if (reduced) assert(shown == 0);       // a plain cut
    else assert(shown >= 8 && shown <= 14);  // about 0.2 s
    assert(!SL.visible);
    // the same part is not offered again while its time lags the jump
    p.changed(t); run(p, t, 10);
    assert(!SL.visible);
  }
  std::puts("skip_plate_test: ok");
}
