// Settings' "i" (its examples on or off): the menu glides between its two sizes instead of
// jumping. The corner it hangs from (over the bar: its right end and its foot) never moves, not
// by one pixel, in a landscape window (examples beside the rows) or a portrait one (above them),
// at whole and half-step scales; the box only grows or only shrinks; its last frame is the menu
// itself; it lasts about a quarter of a second; with reduced motion there is no glide.
#define private public
#include "../src/ui/play.h"
#undef private
#include "../src/core/i18n.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>

int main() {
  i18n::set_language(i18n::Lang::PtBR);
  int runs = 0;
  struct Case { int w, h; double s; };
  for (const Case z : {Case{1280, 720, 2}, Case{1280, 720, 1.5}, Case{1920, 1080, 2.5}, Case{1920, 1080, 3}, Case{540, 960, 1.5}, Case{720, 1280, 2}, Case{1000, 560, 1.5}})
    for (int start : {0, 1}) {
      PlayView p; p.in.dur = 7020; p.in.time = 1869; p.in.settings_examples = start;
      p.resize(z.w, z.h, z.s); double now = 1; p.reset(now); p.changed(now);
      for (int i = 0; i < 30; ++i) { now += 1 / 60.0; p.activity(now); p.tick(now, 1 / 60.0); }
      p.open_panel_for_preview(11, now);
      for (int i = 0; i < 40; ++i) { now += 1 / 60.0; p.activity(now); p.tick(now, 1 / 60.0); }
      int hdr = -1;
      for (int i = 0; i < (int)p.rows_.size(); ++i) if (p.rows_[i].k == PlayView::RK::Header) { hdr = i; break; }
      assert(hdr == 0);
      const auto& L = p.layers[PlayView::L_PANEL];
      const Rect before{L.x, L.y, L.c.w, L.c.h};
      // the pointer on the "i" (as when it is clicked)
      const Rect pr = p.panel_rect(), b = p.rows_[0].parts[0];
      p.mouse_move(pr.x + b.cx(), pr.y + b.cy(), now);
      p.panel_click(0, 0, 0, now);
      int frames = 0, last_w = before.w, last_h = before.h, dw = 0, dh = 0;
      Canvas last;
      for (int i = 0; i < 40; ++i) {
        now += 1 / 60.0; p.activity(now); p.tick(now, 1 / 60.0);
        const bool morph = p.morph_t0_ >= 0;
        // the corner it hangs from stays put
        assert(L.x + L.c.w == before.x + before.w && L.y + L.c.h == before.y + before.h);
        // the box only grows or only shrinks (each way)
        const int sw = L.c.w > last_w ? 1 : L.c.w < last_w ? -1 : 0, sh = L.c.h > last_h ? 1 : L.c.h < last_h ? -1 : 0;
        if (sw) { assert(dw == 0 || dw == sw); dw = sw; }
        if (sh) { assert(dh == 0 || dh == sh); dh = sh; }
        last_w = L.c.w; last_h = L.c.h;
        if (morph) { ++frames; last = L.c; }
        else if (!last.px.empty()) {
          // the first frame of the menu itself: the glide's last frame, to the pixel, in place
          const Rect r = p.panel_rect();
          assert(L.x == r.x && L.y == r.y && L.c.w == r.w && L.c.h == r.h);
          assert(last.w == L.c.w && last.h == L.c.h);
          size_t diff = 0;
          for (size_t k = 0; k < last.px.size(); ++k) diff += last.px[k] != L.c.px[k];
          if (diff > last.px.size() / 50) std::printf("case %dx%d s%.1f start %d: %zu of %zu pixels differ at the end\n", z.w, z.h, z.s, start, diff, last.px.size());
          assert(diff <= last.px.size() / 50);  // (the examples' own last fade step at most)
          last = Canvas{};
        }
      }
      assert(frames >= 12 && frames <= 18);  // ~0.26 s at 60 Hz
      ++runs;
    }
  // reduced motion: straight to the new size
  {
    PlayView p; p.in.dur = 7020; p.resize(1280, 720, 2); double now = 1; p.reset(now);
    reduce_motion() = true;
    p.open_panel_for_preview(11, now);
    for (int i = 0; i < 40; ++i) { now += 1 / 60.0; p.activity(now); p.tick(now, 1 / 60.0); }
    p.panel_click(0, 0, 0, now);
    assert(p.morph_t0_ < 0);
    reduce_motion() = false;
  }
  std::printf("Settings examples glide: %d windows and scales, both ways, the hanging corner fixed, one direction, no jump at the end, reduced motion passed\n", runs);
}
