#pragma once
// The halo's ring: the 26 zone colours (FrameColors, clockwise from the top-left corner) spread
// around the window's edge into 512 entries, each lighting the halo pixels that face that point.
// 17/11: blended in OKLab (a mix of sRGB numbers left a darker seam between two hues: the middle of
// red and green was a dark olive), and softened around the ring twice (a triangle about a zone wide
// on each side, where it was a box): neighbouring zones flow into each other like LEDs behind a
// diffuser, with no edge where one ends. The same 512 entries as before; nothing more per pixel.
#include <algorithm>
#include <cmath>
#include <cstdint>

#include "atmosphere.h"

constexpr int kAmbientRing = 512;

// false: no size yet (nothing written)
inline bool ambient_ring(const Rgb* zone, const double* light, int ww, int wh, Rgb* out, uint8_t* out_a) {
  constexpr int kZones = FrameColors::kZones, kLut = kAmbientRing;
  const double P = 2.0 * (ww + wh);
  if (ww <= 0 || wh <= 0) return false;
  double pos[kZones];
  for (int i = 0; i < 8; ++i) pos[i] = (i + 0.5) / 8 * ww;
  for (int i = 0; i < 5; ++i) pos[8 + i] = ww + (i + 0.5) / 5 * wh;
  for (int i = 0; i < 8; ++i) pos[13 + i] = ww + wh + (i + 0.5) / 8 * ww;
  for (int i = 0; i < 5; ++i) pos[21 + i] = 2.0 * ww + wh + (i + 0.5) / 5 * wh;
  Lab zl[kZones];
  for (int z = 0; z < kZones; ++z) zl[z] = to_oklab(zone[z]);
  double ch[2][4][kLut];  // (L, a, b in OKLab; the light), and a second buffer for the softening
  for (int k = 0; k < kLut; ++k) {
    const double p = (k + 0.5) / kLut * P;
    int j = kZones - 1;
    for (int z = 0; z < kZones; ++z) if (pos[z] <= p) j = z;
    const int n = (j + 1) % kZones;
    double a = pos[j], bpos = n == 0 ? pos[0] + P : pos[n];
    if (p < pos[0]) { a = pos[kZones - 1] - P; bpos = pos[0]; }  // wrapping past the top-left corner
    double t = std::clamp((p - a) / std::max(1.0, bpos - a), 0.0, 1.0);
    t = t * t * (3 - 2 * t);
    ch[0][0][k] = zl[j].L + (zl[n].L - zl[j].L) * t;
    ch[0][1][k] = zl[j].a + (zl[n].a - zl[j].a) * t;
    ch[0][2][k] = zl[j].b + (zl[n].b - zl[j].b) * t;
    ch[0][3][k] = light[j] + (light[n] - light[j]) * t;
  }
  // softened round the ring twice (a running sum: the same cost for any width)
  constexpr int R = 10;
  for (int pass = 0; pass < 2; ++pass) {
    const int from = pass, to = 1 - pass;
    for (int c = 0; c < 4; ++c) {
      const double* src = ch[from][c];
      double* dst = ch[to][c];
      double sum = 0;
      for (int d = -R; d <= R; ++d) sum += src[(d + kLut) % kLut];
      for (int k = 0; k < kLut; ++k) {
        dst[k] = sum / (2 * R + 1);
        sum += src[(k + R + 1) % kLut] - src[(k - R + kLut) % kLut];
      }
    }
  }
  for (int k = 0; k < kLut; ++k) {
    out[k] = from_oklab({ch[0][0][k], ch[0][1][k], ch[0][2][k]});
    out_a[k] = (uint8_t)std::lround(std::clamp(ch[0][3][k], 0.0, 1.0) * 255);
  }
  return true;
}
