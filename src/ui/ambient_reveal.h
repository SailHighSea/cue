#pragma once
// How the halo comes and goes (17/11). It used to be the strips' opacity on a straight ramp (in in
// 0.8 s, out in 0.45 s). Now:
//  - in (0.9 s): the light switches on like a lamp behind the TV, from the window's edge outward. Each
//    halo pixel gets its own start by its distance from the edge (near first, the far part arrives up
//    to 0.3 s later) and rises on a critically damped curve (no bounce: it starts soft, rises, settles).
//    The halo's shape, reach and proportions never change: only the falloff a pixel already has is
//    scaled, through a 256-entry table (falloff level -> level shown), so the pixels are the same list
//    with one more lookup while the light moves, and none at rest.
//  - out (0.25 s, shorter where the picture itself goes: 0.11 s into full screen): it gathers back into
//    the window, the far part first, the edge last.
//  - reduce motion: the opacity alone, on the same curves, with no spreading.
// A change of mind half way (out while coming in, in while going) starts from where each level is.
#include <algorithm>
#include <cmath>
#include <cstdint>

struct AmbientReveal {
  enum Phase { Off, In, On, Out };
  static constexpr double kIn = 0.9;        // the whole entrance
  static constexpr double kInSpread = 0.30; // how much later the far end starts than the edge
  static constexpr double kOut = 0.25;      // the usual exit
  static constexpr double kOutSpread = 0.32;  // share of an exit the far end goes ahead of the edge
  // the light one can see ends at about 70% of the reach (the falloff is under 4% of the edge there):
  // the spreading spans that part, so its far end arrives visibly later than the edge
  static constexpr double kSeen = 0.7;
  static double seen(double u) { return std::min(1.0, u / kSeen); }

  Phase phase = Off;
  bool spread = true;   // false: reduce motion (the opacity alone)
  double t = 0, len = 0, out_spread = 0;
  float from[256]{}, gain[256]{};   // per falloff level, 0..1
  double level_from = 0, level = 0; // the opacity alone (reduce motion); 1 when spreading and lit

  // the edge-to-far distance (0..1 of the reach) of each falloff level, from the same falloff the shape
  // uses (exp(-2.8 q) (1 - sqrt q) 235, q the squared distance): highest level at the edge
  static const float* distance_of_level() {
    static float u[256];
    static bool made = false;
    if (made) return u;
    double sum[256]{}; int n[256]{};
    for (int i = 0; i <= 8192; ++i) {
      const double d = i / 8192.0, a = std::exp(-2.8 * d * d) * (1 - d) * 235;
      const int m = (int)std::lround(a);
      sum[m] += d; ++n[m];
    }
    double last = 1;
    for (int m = 0; m < 256; ++m) {
      if (m > 235) { u[m] = 0; continue; }
      if (n[m]) last = sum[m] / n[m];
      u[m] = (float)last;
    }
    made = true;
    return u;
  }
  // critically damped (an Apple spring with no bounce): zero speed at the start, no overshoot, lands on 1
  static double rise(double p) {
    p = std::clamp(p, 0.0, 1.0);
    constexpr double k = 5.0;  // (the edge at 90% in about 0.4 s, the far part about 0.3 s after it)
    static const double norm = 1 - (1 + k) * std::exp(-k);
    return std::min(1.0, (1 - (1 + k * p) * std::exp(-k * p)) / norm);
  }
  // leaving: at speed from the first frame, soft at the end
  static double fall(double p) { p = std::clamp(p, 0.0, 1.0); return 1 - (1 - p) * (1 - p); }

  void begin_in(bool motion) {
    if (phase == In || phase == On) return;
    spread = motion;
    if (phase == Off) { for (float& f : from) f = spread ? 0.f : 1.f; level_from = 0; }
    else { for (int m = 0; m < 256; ++m) from[m] = spread ? gain[m] : 1.f; level_from = level; }
    if (spread) level_from = 1;  // (the levels carry it)
    phase = In; t = 0; len = kIn;
  }
  void begin_out(double length, bool motion) {
    if (phase == Off || phase == Out) return;
    spread = motion && spread;
    for (int m = 0; m < 256; ++m) from[m] = gain[m];
    level_from = level;
    phase = Out; t = 0; len = std::max(1e-3, length); out_spread = len * kOutSpread;
  }
  // put out at once (the window's size changed under it: the old halo would frame nothing)
  void cut() { phase = Off; t = 0; level = 0; for (float& g : gain) g = 0; }
  void lit() { phase = On; level = 1; for (float& g : gain) g = 1; }

  // true while it moves. A late tick (the shape made for a new size, a busy moment) moves it one tick's
  // worth at most: the light never jumps ahead, it is late instead
  bool step(double dt) {
    if (phase == Off || phase == On) return false;
    t += std::min(dt, 0.07);
    const float* u = distance_of_level();
    if (phase == In) {
      const double span = spread ? len - kInSpread : len;
      for (int m = 0; m < 256; ++m) {
        const double start = spread ? kInSpread * seen(u[m]) : 0.0;
        gain[m] = spread ? (float)(from[m] + (1 - from[m]) * rise((t - start) / span)) : 1.f;
      }
      level = spread ? 1.0 : level_from + (1 - level_from) * rise(t / len);
      if (t >= len) lit();
      return true;
    }
    const double span = spread ? len - out_spread : len;
    for (int m = 0; m < 256; ++m) {
      const double start = spread ? out_spread * (1 - seen(u[m])) : 0.0;
      gain[m] = spread ? (float)(from[m] * (1 - fall((t - start) / span))) : from[m];
    }
    level = spread ? level_from : level_from * (1 - fall(t / len));
    if (t >= len) cut();
    return true;
  }
  bool remapping() const {
    if (phase == Off) return false;
    for (float g : gain) if (g < 0.999f) return true;
    return false;
  }
  // falloff level -> the level shown now
  void remap(uint8_t out[256]) const {
    for (int m = 0; m < 256; ++m) out[m] = (uint8_t)std::lround(m * std::clamp((double)gain[m], 0.0, 1.0));
  }
  double opacity() const { return phase == Off ? 0.0 : level; }
};
