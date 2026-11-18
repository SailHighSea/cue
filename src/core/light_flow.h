#pragma once
// The room's light as a flow (17/11): the colours are sampled a little over a second apart (more on a
// heavy file), never more often. Each sample used to start a glide that landed and stood still until the
// next one: the light stopped a third of the time and moved in surges, a slide show (measured). Now the
// light travels to each sample at an even pace over the whole wait until the next one, from the
// sample's own frame on, so it arrives as the next sample comes and never stands still. (Asking each
// sample one interval ahead instead, to arrive with the film, made the helper decode further from the
// keyframe: the film itself played 35% slower in Wine. Not that.) A cut cannot be told from a fast change
// by two samples (a moving shot 0.15-0.38 apart, a cut 0.45-0.57); the bigger the change, the shorter
// the ramp (kRampCut for a cut). The LightBreath after it only rounds the corners.
#include <algorithm>
#include <cmath>

#include "../ui/atmosphere.h"

class LightFlow {
 public:
  static constexpr int kZones = FrameColors::kZones;
  static constexpr double kRampMax = 3.0;  // film seconds: the longest the light takes to reach a sample
  static constexpr double kLook = 0.2;     // the ramp starts this much before the sample's frame (as LightTiming::kPreroll)
  static constexpr double kTau = 0.1;      // LightBreath's pace on the flow (each stage)
  static constexpr double kTauJump = 0.2;  // ... when it starts over (a new film, a seek, a step while paused)
  static constexpr double kRampCut = 0.8;  // film seconds: the ramp when nearly everything changes
  static constexpr double kFlowFar = 0.30, kCutFar = 0.45;  // OKLab, mean of the zones (measured: moving shots 0.15-0.38, cuts 0.45-0.57)

  void reset() { n_ = 0; }
  bool ready() const { return n_ > 0; }
  // true when the last sample started it over (the light breathes to it instead of flowing)
  bool jumped() const { return jumped_; }

  // a sample for film time `t` arrived while the film is at `now`
  // `every`: film seconds until the next sample is expected
  void add(const Rgb* zone, const double* light, double t, double now, bool paused, double every) {
    Key k;
    for (int z = 0; z < kZones; ++z) { k.c[z] = to_oklab(zone[z]); k.l[z] = light[z]; }
    // starting over: nothing yet, paused (shown at once), or the film was moved away from the flow
    if (n_ == 0 || paused || t < ta_ - 0.01 || t < now - 1.0 || t > now + 30) {
      a_ = b_ = k; ta_ = tb_ = t; n_ = 1; jumped_ = true;
      return;
    }
    // from where it is heading now, to the new sample
    blend(now, a_);
    ta_ = std::min(now, t);
    // how moved it goes (OKLab, mean of the zones): a change in a moving shot flows over the whole wait; when
    // nearly everything changes (a cut, most likely) it goes in a short ramp that ends on the sample's frame,
    // never long before the cut
    double moved = 0;
    for (int z = 0; z < kZones; ++z) moved += std::hypot(k.c[z].L - a_.c[z].L, k.c[z].a - a_.c[z].a, k.c[z].b - a_.c[z].b);
    moved /= kZones;
    const double q = std::clamp((moved - kFlowFar) / (kCutFar - kFlowFar), 0.0, 1.0);
    const double flow = std::clamp(every, kRampCut, kRampMax);
    ramp_ = flow + (kRampCut - flow) * q;
    b_ = k; tb_ = t + ramp_; jumped_ = false;
  }

  // the colours the light heads for while the film is at `now`
  void target(double now, Rgb* zone, double* light) const {
    Key k;
    blend(now, k);
    for (int z = 0; z < kZones; ++z) { zone[z] = from_oklab(k.c[z]); light[z] = k.l[z]; }
  }

 private:
  struct Key { Lab c[kZones]; double l[kZones] = {}; };
  void blend(double now, Key& out) const {
    double u = 1;
    if (tb_ > ta_) {
      const double d = std::min(tb_ - ta_, ramp_);
      u = std::clamp((now + kLook - (tb_ - d)) / d, 0.0, 1.0);
    }
    Key k;
    for (int z = 0; z < kZones; ++z) {
      k.c[z] = mix(a_.c[z], b_.c[z], u);
      k.l[z] = a_.l[z] + (b_.l[z] - a_.l[z]) * u;
    }
    out = k;
  }
  // between two colours as lights mix: lightness and colourfulness go evenly from one to the other and
  // the hue turns the short way round (red to green by yellow). A straight line in OKLab passed near
  // grey between distant hues, and the flow, always on its way, turned the light pale (measured: half
  // the range of colour). Opposite hues (more than 90 degrees apart) lean toward the straight line, so
  // no colour that is in neither is made up (blue to orange does not go by magenta); the colourfulness
  // is kept to what the screen can show (outside sRGB the light darkened), and some of it always stays
  // (three quarters of the way to the line at most: never a dead grey). A colour with almost no hue
  // takes the other's.
  static bool shown(double L, double a, double b) {
    const double l = L + 0.3963377774 * a + 0.2158037573 * b, m = L - 0.1055613458 * a - 0.0638541728 * b,
                 s = L - 0.0894841775 * a - 1.2914855480 * b;
    const double l3 = l * l * l, m3 = m * m * m, s3 = s * s * s;
    const double r = 4.0767416621 * l3 - 3.3077115913 * m3 + 0.2309699292 * s3;
    const double g = -1.2684380046 * l3 + 2.6097574011 * m3 - 0.3413193965 * s3;
    const double bb = -0.0041960863 * l3 - 0.7034186147 * m3 + 1.7076147010 * s3;
    constexpr double e = 0.002;
    return r > -e && r < 1 + e && g > -e && g < 1 + e && bb > -e && bb < 1 + e;
  }
  static Lab mix(const Lab& a, const Lab& b, double u) {
    if (u <= 0) return a;
    if (u >= 1) return b;
    const double ca = std::hypot(a.a, a.b), cb = std::hypot(b.a, b.b);
    const double L = a.L + (b.L - a.L) * u;
    const Lab line{L, a.a + (b.a - a.a) * u, a.b + (b.b - a.b) * u};
    if (ca < 0.02 || cb < 0.02) {  // (one nearly grey: its hue means nothing)
      const double h = ca < 0.02 ? std::atan2(b.b, b.a) : std::atan2(a.b, a.a), C = ca + (cb - ca) * u;
      return {L, C * std::cos(h), C * std::sin(h)};
    }
    const double ha = std::atan2(a.b, a.a);
    double dh = std::atan2(b.b, b.a) - ha;
    while (dh > kPi) dh -= 2 * kPi;
    while (dh < -kPi) dh += 2 * kPi;
    const double h = ha + dh * u, w = 0.75 * std::clamp((std::fabs(dh) - kPi / 2) / (kPi / 2), 0.0, 1.0);
    double C = ca + (cb - ca) * u;
    // kept on screen (the ends are; between them, a few halvings find the most it can be)
    if (!shown(L, C * std::cos(h), C * std::sin(h))) {
      double lo = 0, hi = C;
      for (int i = 0; i < 7; ++i) { const double mid = (lo + hi) / 2; (shown(L, mid * std::cos(h), mid * std::sin(h)) ? lo : hi) = mid; }
      C = lo;
    }
    const Lab arc{L, C * std::cos(h), C * std::sin(h)};
    return {L, arc.a + (line.a - arc.a) * w, arc.b + (line.b - arc.b) * w};
  }
  static constexpr double kPi = 3.14159265358979323846;
  Key a_, b_;
  double ta_ = 0, tb_ = 0;
  double ramp_ = kRampMax;
  int n_ = 0;
  bool jumped_ = true;
};
