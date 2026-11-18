// The home's Settings > "mostrar imagens" (16/11): the switch flips every time it is clicked (it
// used to keep the row's first state, so a second click sent the same choice again and nothing
// moved until Settings was opened afresh), and the list's swap is drawn only in the list's band
// (so it can move under the panel too): the old list kept for the fade is exactly what a whole
// frame shows there.
#define private public
#include "../src/ui/home.h"
#include "../src/ui/play.h"
#undef private
#include "../src/core/i18n.h"
#include <cassert>
#include <cstdio>

static int covers_row(const PlayView& p) {
  for (int i = 0; i < (int)p.rows_.size(); ++i) if (p.rows_[i].id == PlayView::R_HOME_COVERS) return i;
  return -1;
}

int main() {
  int n = 0;
  for (double s : {1.0, 1.5, 2.0, 3.5}) {
    const int W = (int)(640 * s), H = (int)(360 * s);
    PlayView p; p.in.dur = 0; double u = 1; p.resize(W, H, s); p.reset(u);
    p.in.home_covers = true;
    p.open_settings_over_home(Rect{(int)(80 * s), (int)(200 * s), (int)(90 * s), (int)(12 * s)}, u);
    for (int i = 0; i < 60; ++i) { u += 1 / 60.0; p.tick(u, 1 / 60.0); }
    p.take_actions();
    // click, click, click (the mouse) and the keyboard's left/right: each one a new choice
    bool want = true;
    for (int k = 0; k < 4; ++k) {
      const int row = covers_row(p);
      assert(row >= 0);
      p.panel_click(row, -1, 0, u);
      want = !want;
      auto acts = p.take_actions();
      bool seen = false;
      for (auto& a : acts) if (a.a == PA::HomeCovers) { assert((a.v > 0.5) == want); seen = true; }
      assert(seen && p.in.home_covers == want && p.rows_[covers_row(p)].checked == want);
      p.in.home_covers = want;  // (what the App hands back)
      for (int i = 0; i < 20; ++i) { u += 1 / 60.0; p.tick(u, 1 / 60.0); }
      assert(p.rows_[covers_row(p)].checked == want);
      ++n;
    }
    for (int dir : {-1, +1, -1}) {
      const int row = covers_row(p);
      p.panel_adjust(row, dir, u);
      auto acts = p.take_actions();
      bool seen = false;
      for (auto& a : acts) if (a.a == PA::HomeCovers) { assert((a.v > 0.5) == (dir > 0)); seen = true; }
      assert(seen == ((dir > 0) != want));
      want = dir > 0;
      p.in.home_covers = want;
      assert(p.rows_[covers_row(p)].checked == want);
      ++n;
    }

    // the swap: the band drawn alone is the same as the band of a whole frame
    for (bool covers : {true, false}) {
      HomeView hv; hv.covers = covers; hv.set_size(W, H, s);
      hv.set_rows({{"Twin Peaks", "faltam 12 min", 0.62, "S01E04", false, HomeKind::Episode, "C:\\v\\tp.mkv"},
                   {"Cena", "", -1, "", false, HomeKind::Video, "C:\\v\\c.mkv"},
                   {"Song", "Artist", -1, "", true, HomeKind::Music, "C:\\m\\s.flac"}});
      double t = 10; for (int i = 0; i < 90; ++i) { t += 1 / 60.0; hv.tick(t, 1 / 60.0); }
      Canvas whole; hv.render(whole, t);
      Canvas scratch;
      hv.swap_animated(t, &scratch);
      assert(hv.swapping(t + 0.01) && hv.swap_old_.w > 0);
      const Rect b = hv.swap_r_;
      for (int y = 0; y < b.h; ++y)
        for (int x = 0; x < b.w; ++x)
          assert(hv.swap_old_.px[(size_t)y * b.w + x] == whole.px[(size_t)(b.y + y) * W + b.x + x]);
      assert(!scratch.clipped);
      ++n;
    }
  }
  std::printf("home_covers_switch_test: ok (%d)\n", n);
}
