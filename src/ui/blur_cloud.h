#pragma once
#include "gfx.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// The video shader runs before OSD composition; glyphs never enter the blur.
struct BlurCloud {
  Rect controls;
  double alpha = 0;
};

// Pure-white ink with the native equivalent of drop-shadow(0 1px 2px #0009).
// Only the shadow is filtered. Pixel glyphs are copied without resampling.
inline void white_control_shadow(Canvas& c, double strength) {
  // The same 7-tap Gaussian, one pixel down, as before; in whole numbers (weights in 1/65536,
  // one rounding at the end) over the ink's alpha only, and only over the rows and columns the ink
  // reaches (+3 px). It is drawn on every frame of the bar's change of style: at 4K the doubles
  // over the whole bar took ~9 ms a frame and the change stuttered (16/11).
  const int w = c.w, h = c.h;
  if (w <= 0 || h <= 0) return;
  static constexpr uint32_t k7[] = {4598, 8590, 12499, 14163, 12499, 8590, 4598};  // (0.070159 ... x 65536)
  int x0 = w, x1 = -1, y0 = h, y1 = -1;
  for (int y = 0; y < h; ++y) {
    const uint32_t* row = &c.px[(size_t)y * w];
    int l = 0; while (l < w && !(row[l] >> 24)) ++l;
    if (l == w) continue;
    int r = w - 1; while (!(row[r] >> 24)) --r;
    x0 = std::min(x0, l); x1 = std::max(x1, r); y0 = std::min(y0, y); y1 = std::max(y1, y);
  }
  Canvas shadow; shadow.resize(w, h);
  if (x1 >= 0) {
    const int bx0 = std::max(0, x0 - 3), bx1 = std::min(w - 1, x1 + 3), by0 = std::max(0, y0 - 2), by1 = std::min(h - 1, y1 + 4);
    const int bw = bx1 - bx0 + 1;
    std::vector<uint32_t> tmp((size_t)bw * h);  // horizontal pass: alpha x 65536
    std::vector<uint32_t> al((size_t)bw + 6, 0);  // one row's alpha, 3 px of nothing each side
    for (int y = y0; y <= y1; ++y) {
      const uint32_t* row = &c.px[(size_t)y * w];
      for (int x = bx0; x <= bx1; ++x) al[(size_t)(x - bx0) + 3] = row[x] >> 24;
      // (outside the band the ink is clear; the canvas' own edges were already zeros)
      uint32_t* out = &tmp[(size_t)y * bw];
      for (int x = 0; x < bw; ++x) {
        const uint32_t* a = &al[(size_t)x];
        out[x] = a[0] * k7[0] + a[1] * k7[1] + a[2] * k7[2] + a[3] * k7[3] + a[4] * k7[4] + a[5] * k7[5] + a[6] * k7[6];
      }
    }
    const uint64_t scale = (uint64_t)std::llround(0.6 * std::clamp(strength, 0.0, 1.0) * 65536.0);
    std::vector<uint64_t> acc((size_t)bw);
    for (int y = by0; y <= by1; ++y) {
      std::fill(acc.begin(), acc.end(), 0);
      for (int k = -3; k <= 3; ++k) {
        const int yy = y - 1 + k;
        if (yy < y0 || yy > y1) continue;  // (rows without ink are zeros)
        const uint32_t* t = &tmp[(size_t)yy * bw];
        const uint64_t wk = k7[k + 3];
        for (int x = 0; x < bw; ++x) acc[x] += t[x] * wk;
      }
      // acc / 65536^2 is the alpha; x 0.6 x strength, rounded once
      uint32_t* dst = &shadow.px[(size_t)y * w + bx0];
      for (int x = 0; x < bw; ++x) {
        const uint64_t v = (acc[x] * scale + (1ull << 47)) >> 48;
        dst[x] = (uint32_t)std::min<uint64_t>(v, 255) << 24;
      }
    }
  }
  shadow.blit(c, 0, 0);
  c = std::move(shadow);
}
