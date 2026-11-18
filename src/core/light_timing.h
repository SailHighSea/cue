#pragma once
#include <algorithm>
#include <cmath>

// When the room's light (the ambient halo) takes a film's new colours (17/11, Biel: after a cut it
// took ~3-4 s, the scene had already changed in a frantic clip). Two changes, neither of which
// asks the helper for one sample more than before:
//  - the frame is asked for a moment ahead (what the helper takes to decode it), and its colours
//    wait for the film to get there: the light changes with the picture, not after it;
//  - every change is a breath (17/11, LightBreath: four stages in OKLab, it starts without a jolt,
//    gathers and lands softly); a cut (colours far from the ones shown) breathes a little faster
//    (90% in ~1.3 s, as the S of 16r), gradual changes at the pace they always had (~0.9 s on
//    average). The breath begins a moment before the cut's frame (its first 3% are not seen), so
//    what is seen starts with the cut.
struct LightTiming {
  static constexpr double kTauGradual = 0.225;  // each of the breath's four stages: ~0.9 s on average, as before
  static constexpr double kTauCut = 0.2;        // a new scene: 90% in ~1.3 s (see LightBreath)
  static constexpr double kPreroll = 0.2;       // the breath starts this long before its frame (film seconds)
  static constexpr double kCutDistance = 48;  // mean of the zones' largest channel difference (0..255)
  static constexpr double kLeadMax = 0.8;     // never asks further ahead than this (film seconds)
  static constexpr double kStale = 2.5;       // a pending sample this far behind the playhead: dropped
  static constexpr double kAheadMax = 20;     // ... or this far ahead (asked one interval ahead: 15 s at most)

  // how far ahead of the playhead to ask (film seconds): what a sample costs, plus a frame or two and
  // the breath's preroll
  static double lead(double cost_seconds, double speed, bool paused) {
    if (paused) return 0;
    const double c = cost_seconds > 0 && std::isfinite(cost_seconds) ? cost_seconds : 0.1;
    return std::clamp(c + 0.05 + kPreroll, 0.0, kLeadMax) * std::clamp(speed > 0 ? speed : 1.0, 0.25, 4.0);
  }
  // a sample for `sample_t` arrived (or waits) with the playhead at `now_t`: 1 apply now,
  // 0 keep waiting, -1 drop it (the film was moved far from it)
  static int due(double sample_t, double now_t, bool paused) {
    if (!std::isfinite(sample_t) || sample_t < 0) return 1;
    if (sample_t < now_t - kStale || sample_t > now_t + kAheadMax) return -1;
    if (paused) return 1;
    return now_t + kPreroll + 1 / 60.0 >= sample_t ? 1 : 0;
  }
  // the breath's pace for new colours, given how far they are from the ones on screen (`distance`:
  // mean over the zones of the largest channel difference): a cut breathes a little faster
  static bool cut(double distance) { return distance >= kCutDistance; }
  static double tau(double distance) { return cut(distance) ? kTauCut : kTauGradual; }
};
