// The large interface sizes (16/11, Biel):
//  - a song's window is told apart at no more than the default size's pixel: in "large" and
//    "larger" it gets the same composition as in "medium" (the Compacto, without the song's
//    name, had taken every window there, and the name came and went while resizing);
//  - Settings with its examples ("i") shows them beside the rows where the picture on top does
//    not fit and the window is wide enough (they were gone in the large sizes).
#define private public
#include "../src/ui/play.h"
#undef private
#include "../src/core/audio_mode.h"
#include "../src/core/ui_scale.h"
#include <cassert>
#include <cstdio>

int main() {
  int n = 0;
  // 1. music's compositions: the same in every size from the default one up
  for (unsigned dpi : {96u, 120u, 144u, 168u, 192u})
    for (int dw = 640; dw <= 2400; dw += 40)
      for (int dh = 360; dh <= 1400; dh += 40) {
        const int cw = dw * (int)dpi / 96, ch = dh * (int)dpi / 96;
        const MusicFrame med = music_frame_for(cw, ch, music_frame_scale(dpi, cw, ch, kDefaultUiSize), MusicFrame::Card, dpi / 96.0);
        for (int choice : {0, 3}) {
          assert(music_frame_scale(dpi, cw, ch, choice) <= ui_scale(dpi, cw, ch, choice));
          assert(music_frame_for(cw, ch, music_frame_scale(dpi, cw, ch, choice), MusicFrame::Card, dpi / 96.0) == med);
          ++n;
        }
        // (the small size keeps its own, finer pixel: it was never short of room)
        assert(music_frame_scale(dpi, cw, ch, -1) == ui_scale(dpi, cw, ch, -1));
      }
  // a 1280 x 720 window keeps the song's name in every size (it was the Compacto in "larger")
  for (unsigned dpi : {96u, 144u, 168u})
    for (int choice : {-1, 2, 0, 3}) {
      const int cw = 1280 * (int)dpi / 96, ch = 720 * (int)dpi / 96;
      assert(music_frame_for(cw, ch, music_frame_scale(dpi, cw, ch, choice), MusicFrame::Card, dpi / 96.0) == MusicFrame::Centre);
    }
  // 2. Settings' examples beside the rows in the large sizes
  int side = 0;
  for (unsigned dpi : {96u, 168u})
    for (auto [dw, dh] : {std::pair{1072, 603}, {1280, 720}, {1600, 900}})
      for (int choice : {0, 3}) {
        const int cw = dw * (int)dpi / 96, ch = dh * (int)dpi / 96;
        const double s = ui_scale(dpi, cw, ch, choice);
        PlayView p;
        double u = 1;
        p.in.dur = 300; p.in.time = 10; p.in.settings_examples = true;
        p.resize(cw, ch, s); p.reset(u);
        auto run = [&](double secs) { for (int i = 0; i < (int)(secs * 60); ++i) { u += 1 / 60.0; p.tick(u, 1 / 60.0); p.take_actions(); } };
        run(0.3);
        p.open_panel_for_preview(11, u);
        run(0.5);
        const bool wide = (p.settings_w_ + 196) * s <= cw - 16 * s;
        // a picture whenever the rows still have their place (stage 6 on: the settings alone)
        if (p.settings_stage_ < 6) {
          assert(p.settings_side_ || !p.settings_compact_);
          if (p.settings_side_) {
            ++side;
            assert(wide);
            const Rect r = p.panel_rect();
            assert(r.x >= 0 && r.r() <= cw && r.y >= 0 && r.b() <= ch);  // all of it in the window
          }
        }
      }
  assert(side >= 6);
  std::printf("large_sizes_test: ok (%d music windows, Settings beside in %d)\n", n, side);
}
