// The resume notice ("▸ 12:34" at the top, a film taken up where it was left): with the pointer on
// it, it reads "from the start" in the same width (it does not jump sideways); a click, or Home
// while it shows, asks to go back to 0:00 (the skip's dark passage) and the notice fades out at
// once. It waits while the pointer is on it. In every language and interface size it fits.
#define private public
#include "../src/ui/play.h"
#undef private
#include "../src/core/i18n.h"
#include <cassert>
#include <cstdio>

static void run(PlayView& p, double& t, int frames) { for (int i = 0; i < frames; ++i) { t += 1. / 60; p.tick(t, 1. / 60); } }
static int zero_skips(PlayView& p) { int n = 0; for (auto& a : p.take_actions()) if (a.a == PA::SkipPart) { assert(a.v == 0); ++n; } return n; }

int main() {
  int checks = 0;
  for (int lang = 0; lang < i18n::kLangCount; ++lang) {
    i18n::set_language((i18n::Lang)lang);
    for (int scale : {1, 2, 3, 4}) for (int width : {320, 1280}) {
      PlayView p;
      p.in.dur = 2400; p.in.time = 754;
      double t = 1;
      p.resize(width * scale / 2, 360 * scale / 2 * 2, scale); p.reset(t);
      p.resume_notice(754, t);
      run(p, t, 20);
      const Layer& T = p.layers[PlayView::L_TOAST];
      assert(T.visible && p.toast_resume_);
      const int w0 = T.c.w, x0 = T.x;
      assert(T.x >= 0 && T.x + T.c.w <= p.w_);
      // the pointer on it: "from the start", same place and width
      const Rect r = p.toast_rect_;
      p.mouse_move(r.cx(), r.cy(), t);
      run(p, t, 2);
      assert(p.toast_hot_ && T.c.w == w0 && T.x == x0);
      // it waits while the pointer stays (well past its 3.2 s)
      run(p, t, 60 * 6);
      assert(T.visible);
      p.take_actions();
      // the click
      p.mouse_down(r.cx(), r.cy(), t); p.mouse_up(r.cx(), r.cy(), t);
      assert(zero_skips(p) == 1 && !p.toast_resume_);
      run(p, t, 40);
      assert(!T.visible);
      // Home, while it shows (and only then)
      p.resume_notice(754, t); run(p, t, 20);
      p.key("home", false, false, t);
      assert(zero_skips(p) == 1);
      run(p, t, 40);
      p.key("home", false, false, t);
      assert(zero_skips(p) == 0);
      // another notice replaces it: nothing of "from the start" stays
      p.resume_notice(754, t); run(p, t, 5);
      p.toast("English", "sound-on", t); run(p, t, 5);
      p.key("home", false, false, t);
      assert(zero_skips(p) == 0);
      ++checks;
    }
  }
  std::printf("resume_notice_test: ok (%d)\n", checks);
}
