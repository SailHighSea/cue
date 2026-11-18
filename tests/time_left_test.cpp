// What is left of a film (16/11): with both times on the bar, the click goes to the length on the
// right, which turns into what is left (06:02 · −00:43), and the time on the left stays the time
// played, as in Apple's players. A bar with one time alone turns that one, as before.
#define private public
#include "../src/ui/play.h"
#undef private
#include <cassert>
#include <cstdio>

int main() {
  int n = 0, both_seen = 0, alone_seen = 0;
  for (double s : {1.0, 1.5, 2.0, 3.0})
    for (auto [w, h] : {std::pair{1280, 720}, {1920, 1080}, {640, 900}, {420, 300}})
      for (bool audio : {false, true}) {
        PlayView p;
        p.in.dur = 405; p.in.time = 362; p.in.audio_only = audio;
        p.resize((int)(w * s / 2), (int)(h * s / 2), s); p.reset(1);
        double u = 1;
        for (int i = 0; i < 30; ++i) { u += 1 / 60.0; p.tick(u, 1 / 60.0); }
        Rect tr{}, tl = p.tl_;
        for (auto& hb : p.hots_) if (hb.id == PlayView::B_TIME) tr = hb.r;
        if (tr.w <= 0) continue;
        const bool both = p.mini_ && !p.stack_ && !p.card_ ? p.mini_times_ : p.dur_shown_;
        // its place: on the length's side of the timeline when both are shown, else the time's
        if (both) { assert(tr.x >= tl.x + tl.w / 2); ++both_seen; } else { assert(tr.r() <= tl.x + tl.w / 2 + 16 * s); ++alone_seen; }
        p.activate(PlayView::B_TIME, u);
        assert(p.remaining_);
        p.render_dock();
        assert(p.shown_time_ == (both ? "06:02" : "-00:43"));
        p.activate(PlayView::B_TIME, u);
        p.render_dock();
        assert(!p.remaining_ && p.shown_time_ == "06:02");
        ++n;
      }
  assert(both_seen > 0);
  std::printf("time_left_test: ok (%d; %d with both times, %d with one)\n", n, both_seen, alone_seen);
}
