// "..." > settings > "chapters" (17/11): the switch flips on every click, however quick (the row kept the
// state of when the panel opened, so a second click sent the same choice again and nothing moved until the
// panel was opened afresh), with the mouse and with the keyboard's left/right, in both settings panels.
#define private public
#include "../src/ui/play.h"
#undef private
#include "../src/core/i18n.h"
#include <cassert>
#include <cstdio>

static int row_of(const PlayView& p) {
  for (int i = 0; i < (int)p.rows_.size(); ++i) if (p.rows_[i].id == PlayView::R_CHAPTERS_ON) return i;
  return -1;
}

int main() {
  int n = 0;
  for (PlayView::Panel panel : {PlayView::Panel::Settings, PlayView::Panel::VideoSet})
    for (double s : {1.0, 2.0}) {
      PlayView p; double u = 1; p.resize((int)(1280 * s), (int)(720 * s), s); p.reset(u);
      p.in.dur = 600; p.in.chapters_on = true;
      p.open_panel(panel, -1, u);
      for (int i = 0; i < 40; ++i) { u += 1 / 60.0; p.tick(u, 1 / 60.0); }
      p.take_actions();
      if (row_of(p) < 0) continue;  // (not in this panel here)
      bool want = true;
      for (int k = 0; k < 5; ++k) {
        p.panel_click(row_of(p), -1, 0, u);  // quick: no time between the clicks
        want = !want;
        bool seen = false;
        for (auto& a : p.take_actions()) if (a.a == PA::ChaptersOn) { assert((a.v > 0.5) == want); seen = true; }
        assert(seen && p.in.chapters_on == want && p.rows_[row_of(p)].checked == want);
        u += 1 / 60.0; p.tick(u, 1 / 60.0);
        assert(p.rows_[row_of(p)].checked == want);
        ++n;
      }
      for (int dir : {-1, +1, -1, -1}) {
        p.panel_adjust(row_of(p), dir, u);
        bool seen = false;
        for (auto& a : p.take_actions()) if (a.a == PA::ChaptersOn) { assert((a.v > 0.5) == (dir > 0)); seen = true; }
        assert(seen == ((dir > 0) != want));
        want = dir > 0;
        assert(p.rows_[row_of(p)].checked == want);
        ++n;
      }
    }
  assert(n > 0);
  std::printf("chapters_switch_test: ok (%d)\n", n);
}
