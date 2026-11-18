// The signs (14e) in the music player: no sign for play/pause or for a jump (space, ← →; 16/11),
// nor for the volume, the speed or the repeat (17/11, Biel: signs only on films), in the full
// player, full screen and the mini player. A film keeps them all.
#define private public
#include "../src/ui/play.h"
#undef private
#include <cassert>
#include <cstdio>

int main() {
  int n = 0;
  for (bool audio : {false, true})
    for (bool mini : {false, true})
      for (bool full : {false, true}) {
        if (mini && full) continue;
        PlayView p;
        p.in.dur = 300; p.in.time = 60; p.in.audio_only = audio; p.in.mini_player = mini; p.in.fullscreen = full;
        p.resize(mini ? 480 : 1280, mini ? 270 : 720, 2); p.reset(1);
        double u = 1;
        for (int i = 0; i < 20; ++i) { u += 1 / 60.0; p.tick(u, 1 / 60.0); }
        const int g0 = p.flash_gen_;
        p.flash_pause(true, u);
        assert((p.flash_gen_ != g0) == !audio);
        const int g1 = p.flash_gen_;
        p.flash_seek(5, u); p.flash_seek(5, u);
        assert((p.flash_gen_ != g1) == !audio);
        if (!audio) assert(p.flash_kind_ == 2 && p.flash_seek_ == 10);
        const int g2 = p.flash_gen_;
        p.flash_volume(70, u);
        assert((p.flash_gen_ != g2) == !audio);
        if (!audio) assert(p.flash_kind_ == 3);
        const int g3 = p.flash_gen_;
        p.flash("1.25×", u);
        assert((p.flash_gen_ != g3) == !audio);
        // the keys too: ↑ ↓, [ ], the repeat
        const int g4 = p.flash_gen_;
        p.key("up", false, false, u); p.key("]", false, false, u); p.key("l", false, false, u);
        if (audio) assert(p.flash_gen_ == g4);
        ++n;
      }
  std::printf("music_notices_test: ok (%d)\n", n);
}
