// The film's atmosphere, read from a small frame: not the RGB average, but the colour that
// is actually present and representative, with deep blacks ignored and neon tamed.
#pragma once
#include <cstdint>

#include "gfx.h"

struct FrameColors {
  bool valid = false;
  Rgb accent{255, 255, 255};  // for the timeline, handle and active states (readable on black)
  double strength = 0;        // 0 = a grey scene (stay neutral) .. 1 = a clearly coloured one
  Rgb edge[4]{};              // top, right, bottom, left: soft light for the ambient glow
  double edge_light[4]{};     // how much light each side gives (dark sides glow less)
  // Ambilight-style zones around the picture, clockwise from the top-left corner:
  // top 8 (left to right), right 5 (top to bottom), bottom 8 (right to left), left 5 (bottom to top)
  static constexpr int kZones = 26;
  Rgb zone[kZones]{};         // vivid hue of that stretch of the border (full value)
  double zone_light[kZones]{};  // how bright that stretch is (0 = dark, no glow)
};

// px: BGRA (0xAARRGGBB), w x h.
FrameColors analyze_frame(const uint32_t* px, int w, int h);

// Moves `cur` toward `target` with a time constant `tau` (seconds): never a jump at a cut (the
// dock's accent; the halo breathes, see LightBreath).
struct ColorSmoother {
  double r = 255, g = 255, b = 255, k = 0;
  bool primed = false;
  bool step(Rgb target, double target_k, double dt, double tau);
  Rgb rgb() const;
};

// Colours in OKLab (Björn Ottosson's perceptual space): mixing there keeps the lightness and the hue
// even on the way, where a mix of sRGB numbers dims the middle (red to green passed through a dark
// olive) and a mix in linear light front-loads the colour that arrives.
struct Lab { double L = 0, a = 0, b = 0; };
Lab to_oklab(Rgb c);
Rgb from_oklab(const Lab& c);

// The room's light (the halo's zones), 17/11: every change is a breath, four stages in a row in OKLab
// (an S that starts with no jolt, gathers and lands softly). One curve for all: a new sample only
// retargets it, so nothing restarts or changes speed at once (the S of 16r was switched off half way
// by the next gradual sample, and each gradual sample used to set off at full speed). `tau` is each
// stage's time; when it changes (a cut is shorter), the glide eases into the new pace.
struct LightBreath {
  static constexpr int kStages = 4;
  double s[kStages][4]{};     // each stage: L, a, b (OKLab) and the light
  double tau = 0, tau_to = 0;
  Lab target_lab{};
  Rgb target_rgb{};
  bool primed = false, resting = true;
  Rgb out{};
  double k = 0;
  // true while it still moves (the frame loop keeps running for it)
  bool step(Rgb target, double target_k, double dt, double stage_tau);
  Rgb rgb() const { return out; }
};
