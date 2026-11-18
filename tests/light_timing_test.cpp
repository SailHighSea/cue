// The room's light after a cut (17/11): asked a moment ahead, applied when the film gets there,
// every change a breath (four stages in OKLab: no jolt at the start, no dark middle, a soft landing),
// the zones blended round the ring in OKLab, and nothing asked or painted more often.
#include "../src/core/light_timing.h"
#include "../src/core/light_flow.h"
#include "../src/ui/ambient_ring.h"
#include "../src/ui/atmosphere.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>

static double lin(int c) { const double v = c / 255.0; return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); }
static double luma(Rgb c) { return 0.2126 * lin(c.r) + 0.7152 * lin(c.g) + 0.0722 * lin(c.b); }
static int dist(Rgb a, Rgb b) { return std::max({std::abs(a.r - b.r), std::abs(a.g - b.g), std::abs(a.b - b.b)}); }

int main() {
  // ahead by what a sample costs (and the speed) plus the breath's preroll, never past the cap;
  // nothing ahead when paused
  assert(LightTiming::lead(0.3, 1, false) > 0.3 + LightTiming::kPreroll && LightTiming::lead(0.3, 1, false) <= LightTiming::kLeadMax);
  assert(LightTiming::lead(-1, 1, false) > 0 && LightTiming::lead(5, 1, false) == LightTiming::kLeadMax);
  assert(LightTiming::lead(0.2, 2, false) > LightTiming::lead(0.2, 1, false));
  assert(LightTiming::lead(0.2, 1, true) == 0);
  // the colours wait for the film (the breath begins kPreroll before their frame); a seek far
  // from them drops them; paused: at once
  assert(LightTiming::due(10.5, 10.0, false) == 0);
  assert(LightTiming::due(10.5, 10.5 - LightTiming::kPreroll, false) == 1);
  assert(LightTiming::due(10.3, 10.3, false) == 1);
  assert(LightTiming::due(10.3, 40.0, false) == -1 && LightTiming::due(40.0, 10.0, false) == -1);
  assert(LightTiming::due(10.3, 10.0, true) == 1);
  assert(LightTiming::due(-1, 10.0, false) == 1);
  assert(LightTiming::cut(120) && !LightTiming::cut(10) && LightTiming::tau(120) < LightTiming::tau(10));

  // OKLab: a code goes there and back unchanged
  for (int i = 0; i < 4000; ++i) {
    const Rgb c{(uint8_t)(std::rand() & 255), (uint8_t)(std::rand() & 255), (uint8_t)(std::rand() & 255)};
    assert(dist(from_oklab(to_oklab(c)), c) <= 1);
  }

  // a cut, on a breath: red to green
  const double dt = 1 / 30.0;
  const Rgb red{230, 40, 30}, green{30, 220, 80};
  LightBreath b;
  b.step(red, 0.8, dt, LightTiming::kTauCut);
  assert(b.rgb().r == red.r && b.rgb().g == red.g);
  double t = 0, t10 = -1, t90 = -1, at_015 = -1, peak = 0, peak_t = 0, last_v = 0, min_luma = 1;
  Rgb prev = b.rgb();
  int biggest = 0;
  bool landed = false;
  for (int i = 0; i < 120; ++i) {
    b.step(green, 0.85, dt, LightTiming::kTauCut);
    t += dt;
    const Rgb c = b.rgb();
    // how far, as it is seen (OKLab: the perceptual distance from the start over the whole way)
    const Lab o = to_oklab(c), s0 = to_oklab(red), s1 = to_oklab(green);
    const double p = std::hypot(o.L - s0.L, o.a - s0.a, o.b - s0.b) / std::hypot(s1.L - s0.L, s1.a - s0.a, s1.b - s0.b);
    if (t10 < 0 && p >= 0.1) t10 = t;
    if (t90 < 0 && p >= 0.9) t90 = t;
    if (at_015 < 0 && t >= 0.15) at_015 = p;
    const double v = p - last_v;
    last_v = p;
    if (v > peak) { peak = v; peak_t = t; }
    biggest = std::max(biggest, dist(c, prev));
    // never darker on the way than where it starts or lands (a mix of sRGB numbers dipped to ~45%)
    min_luma = std::min(min_luma, luma(c) / std::min(luma(red), luma(green)));
    // it only goes forward (no overshoot), channel by channel
    assert(c.g >= prev.g && c.r <= prev.r);
    prev = c;
    if (b.resting) { landed = true; assert(c.r == green.r && c.g == green.g && c.b == green.b); break; }
  }
  assert(landed && t < 3.0);                // it lands, exactly on the colour, within 3 s
  assert(at_015 >= 0 && at_015 < 0.03);     // no jolt: under 3% after 0.15 s
  assert(t90 > 1.0 && t90 < 2.0);           // a cut takes about as long as before (16r: 1.2 s)
  assert(peak_t > 0.3);                     // its speed peaks in the middle, not at the start
  assert(min_luma > 0.97);
  assert(biggest <= 12);                    // the largest step between two ticks

  // a new sample half way (the next colour, or the same scene a bit different) only retargets it:
  // the speed carries on, never a jump, never a restart from rest
  {
    LightBreath c;
    c.step(red, 0.8, dt, LightTiming::kTauCut);
    for (int i = 0; i < 18; ++i) c.step(green, 0.85, dt, LightTiming::kTauCut);
    Rgb a = c.rgb();
    c.step(green, 0.85, dt, LightTiming::kTauCut);
    const int before = dist(c.rgb(), a);
    a = c.rgb();
    c.step(Rgb{40, 200, 120}, 0.85, dt, LightTiming::kTauGradual);  // a gradual sample right then
    const int after = dist(c.rgb(), a);
    assert(before > 2 && after > 0 && std::abs(after - before) <= 3);
  }

  // the ring: two neighbouring zones blend without a darker seam, and round the corner it joins up
  {
    Rgb z[FrameColors::kZones];
    double l[FrameColors::kZones];
    for (int k = 0; k < FrameColors::kZones; ++k) { z[k] = k < 4 ? Rgb{230, 40, 30} : Rgb{30, 220, 80}; l[k] = 0.8; }
    Rgb ring[kAmbientRing];
    uint8_t ra[kAmbientRing];
    assert(ambient_ring(z, l, 1280, 720, ring, ra));
    double lo = 1;
    for (int k = 0; k < kAmbientRing; ++k) lo = std::min(lo, luma(ring[k]) / luma(Rgb{230, 40, 30}));
    assert(lo > 0.97);
    int step = 0;
    for (int k = 0; k < kAmbientRing; ++k) { step = std::max(step, dist(ring[k], ring[(k + 1) % kAmbientRing])); assert(std::abs(ra[k] - 204) <= 1); }
    assert(step <= 10 && dist(ring[kAmbientRing - 1], ring[0]) <= 10);  // a gradient, no edge (also past the corner)
    assert(!ambient_ring(z, l, 0, 720, ring, ra));
  }
  // the flow (17/11): from a sample's frame on, at an even pace over the wait until the next one; never
  // standing still between samples; a cut in a short ramp; between distant hues, never darker or greyer
  {
    Rgb za[FrameColors::kZones], zb[FrameColors::kZones], zc[FrameColors::kZones], out[FrameColors::kZones];
    double la[FrameColors::kZones], lo[FrameColors::kZones];
    for (int k = 0; k < FrameColors::kZones; ++k) { za[k] = {200, 60, 40}; zb[k] = {170, 120, 40}; zc[k] = {40, 70, 230}; la[k] = 0.7; }
    LightFlow f;
    f.add(za, la, 10.0, 9.8, false, 1.5);
    assert(f.jumped());
    f.target(10.0, out, lo);
    assert(dist(out[0], za[0]) <= 1);
    f.add(zb, la, 11.5, 11.3, false, 1.5);  // a small change: flows over the whole wait
    assert(!f.jumped());
    Rgb prev = za[0];
    int moves = 0;
    for (double t = 11.3; t < 13.2; t += 1 / 30.0) {
      f.target(t, out, lo);
      if (dist(out[0], prev)) ++moves;
      assert(dist(out[0], prev) <= 3 && out[0].g >= prev.g);  // even, small steps, one way
      prev = out[0];
    }
    assert(moves > 25 && dist(prev, zb[0]) <= 1);  // it moved on most ticks and landed
    // a cut (blue): a short ramp, ending ~kRampCut after its frame; the middle neither darker nor grey
    f.add(zc, la, 14.0, 13.8, false, 1.5);
    f.target(13.7, out, lo);
    assert(dist(out[0], zb[0]) <= 1);
    double lmin = 1, cmin = 1;
    double landed = -1;
    for (double t = 13.8; t <= 14.0 + LightFlow::kRampMax; t += 1 / 30.0) {
      f.target(t, out, lo);
      const Lab o = to_oklab(out[0]);
      lmin = std::min(lmin, luma(out[0]) / std::min(luma(zb[0]), luma(zc[0])));
      cmin = std::min(cmin, std::hypot(o.a, o.b));
      if (landed < 0 && dist(out[0], zc[0]) <= 1) landed = t;
    }
    assert(landed > 0 && landed < 14.0 + 1.5 && lmin > 0.97 && cmin > 0.04);  // sooner than the flow's 1.5 s
    // a seek back starts over
    f.add(za, la, 3.0, 2.9, false, 1.5);
    assert(f.jumped());
  }
  std::printf("light_timing_test: ok (a cut breathes: under 3%% at 0.15 s, 90%% in %.2f s, lands at %.2f s; no dark middle)\n", t90, t);
}
