#include "light.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kDither[4] = {-0.375f / 255, 0.125f / 255, 0.375f / 255, -0.125f / 255};
inline uint32_t premul(Rgb c, double a, int x, int y) {
  a = std::clamp(a + (double)kDither[((y & 1) << 1) | (x & 1)], 0.0, 1.0);
  const int A = (int)std::lround(a * 255);
  if (A <= 0) return 0;
  auto ch = [&](uint8_t v) { return (uint32_t)std::min(A, (int)std::lround(v * a)); };
  return ((uint32_t)A << 24) | (ch(c.r) << 16) | (ch(c.g) << 8) | ch(c.b);
}
}  // namespace

void radial_light(Canvas& out, int w, int h, Rgb left, Rgb mid, Rgb right, double peak) {
  out.resize(std::max(1, w), std::max(1, h));  const int W = out.w, H = out.h;
  const float cx = W / 2.0f, cy = H / 2.0f, rx = std::max(1.0f, W / 2.0f), ry = std::max(1.0f, H / 2.0f);
  // per column: its colour and its share of the distance; per row: the other share. The rim's
  // span is found per row, so the pixels outside the ellipse are only zeroed.
  std::vector<float> dx2(W), cr(W), cg(W), cb(W);
  for (int x = 0; x < W; ++x) {
    const float dx = (x + 0.5f - cx) / rx;
    dx2[x] = dx * dx;
    const double u = (x + 0.5) / W;
    const Rgb col = u < 0.5 ? mix(left, mid, u * 2) : mix(mid, right, (u - 0.5) * 2);
    cr[x] = col.r; cg[x] = col.g; cb[x] = col.b;
  }
  const float pk = (float)peak;
  for (int y = 0; y < H; ++y) {
    uint32_t* row = out.px.data() + (size_t)y * W;
    const float dy = (y + 0.5f - cy) / ry, dy2 = dy * dy;
    if (dy2 >= 1) { std::fill(row, row + W, 0u); continue; }
    const float half = std::sqrt(1 - dy2) * rx;
    const int x0 = std::clamp((int)std::floor(cx - half), 0, W), x1 = std::clamp((int)std::ceil(cx + half), 0, W);
    std::fill(row, row + x0, 0u);
    std::fill(row + x1, row + W, 0u);
    for (int x = x0; x < x1; ++x) {
      const float d2 = dx2[x] + dy2;
      if (d2 >= 1) { row[x] = 0; continue; }
      const float f = (1 - d2) * (1 - d2) * pk;
      // a 2 x 2 ordered dither (under one level of 255) against banding in the long soft ramp
      const float a = std::min(1.0f, f + kDither[((y & 1) << 1) | (x & 1)]);
      const uint32_t A = (uint32_t)(a * 255 + 0.5f);
      if (!A) { row[x] = 0; continue; }
      auto ch = [&](float v) { return std::min(A, (uint32_t)(v * a + 0.5f)); };
      row[x] = (A << 24) | (ch(cr[x]) << 16) | (ch(cg[x]) << 8) | ch(cb[x]);
    }
  }
}

void compose_on(Canvas& out, const Canvas& light, Rgb bg, double bg_a) {
  out.resize(light.w, light.h);
  const uint32_t BA = (uint32_t)std::lround(std::clamp(bg_a, 0.0, 1.0) * 255);
  auto pm = [&](uint8_t v) { return (uint32_t)((v * BA + 127) / 255); };
  const uint32_t base = (BA << 24) | (pm(bg.r) << 16) | (pm(bg.g) << 8) | pm(bg.b);
  std::fill(out.px.begin(), out.px.end(), base);
  out.blit(light, 0, 0, 1.0);
}

void paste(Canvas& c, const Canvas& src, int x, int y) {
  int X0, Y0, X1, Y1;
  c.reach(X0, Y0, X1, Y1);
  const int i0 = std::max(0, X0 - x), i1 = std::min(src.w, X1 - x);
  const int j0 = std::max(0, Y0 - y), j1 = std::min(src.h, Y1 - y);
  if (i0 >= i1 || j0 >= j1) return;
  for (int j = j0; j < j1; ++j)
    std::copy(src.px.begin() + (size_t)j * src.w + i0, src.px.begin() + (size_t)j * src.w + i1, c.px.begin() + (size_t)(y + j) * c.w + x + i0);
}

void rect_light(Canvas& out, int w, int h, const Rect& in, int reach, const Rgb corner[4], double peak) {
  out.resize(std::max(1, w), std::max(1, h));
  reach = std::max(1, reach);
  for (int y = 0; y < out.h; ++y) {
    uint32_t* row = out.px.data() + (size_t)y * out.w;
    const double py = y + 0.5;
    const double dy = std::max({in.y - py, 0.0, py - in.b()});
    const double v = std::clamp((py - in.y) / std::max(1, in.h), 0.0, 1.0);
    const Rgb l = mix(corner[0], corner[3], v), r = mix(corner[1], corner[2], v);
    const bool inside_rows = dy == 0;
    if (inside_rows) std::fill(row + std::max(0, in.x), row + std::min(out.w, in.r()), 0u);  // the picture's own place: no light
    for (int x = 0; x < out.w; ++x) {
      if (inside_rows && x >= in.x && x < in.r()) { x = in.r() - 1; continue; }
      const double px = x + 0.5;
      const double dx = std::max({in.x - px, 0.0, px - in.r()});
      if (dx == 0 && dy == 0) { row[x] = 0; continue; }
      const double d = std::sqrt(dx * dx + dy * dy) / reach;
      if (d >= 1) { row[x] = 0; continue; }
      const double f = (1 - d) * (1 - d) * (1 - 0.35 * d);
      const double u = std::clamp((px - in.x) / std::max(1, in.w), 0.0, 1.0);
      row[x] = premul(mix(l, r, u), peak * f, x, y);
    }
  }
}

void glow_light(Canvas& out, int w, int h, const Rect& in, int reach, const Rgb corner[4], double peak) {
  out.resize(std::max(1, w), std::max(1, h));
  reach = std::max(1, reach);
  // the light comes from a smaller rectangle behind the picture: at the picture's edge it is
  // already falling, and its corners are round (no bright frame hugging the picture)
  const double inset = std::min({reach * 0.45, in.w / 2.0, in.h / 2.0});
  const double ix0 = in.x + inset, iy0 = in.y + inset, ix1 = in.r() - inset, iy1 = in.b() - inset;
  const double span = reach + inset;
  // a lamp's falloff: soft at the start, a long quiet tail, nothing at the end (looked up, not
  // computed per pixel: a 4K picture's light is a million of them)
  static double fall[1025];
  static bool made = false;
  if (!made) {
    for (int i = 0; i <= 1024; ++i) { const double t = i / 1024.0; fall[i] = std::exp(-4.5 * t * t) * (1 - t); }
    made = true;
  }
  for (int y = 0; y < out.h; ++y) {
    uint32_t* row = out.px.data() + (size_t)y * out.w;
    const double py = y + 0.5;
    const double dy = std::max({iy0 - py, 0.0, py - iy1});
    const double v = std::clamp((py - in.y) / std::max(1, in.h), 0.0, 1.0);
    const Rgb l = mix(corner[0], corner[3], v), r = mix(corner[1], corner[2], v);
    const bool picture_rows = py > in.y && py < in.b();
    for (int x = 0; x < out.w; ++x) {
      const double px = x + 0.5;
      if (picture_rows && px > in.x && px < in.r()) {  // covered by the picture: none, at once
        const int end = std::min(out.w, in.r());
        std::fill(row + x, row + end, 0u);
        x = end - 1;
        continue;
      }
      const double dx = std::max({ix0 - px, 0.0, px - ix1});
      const double t = std::sqrt(dx * dx + dy * dy) / span;
      if (t >= 1) { row[x] = 0; continue; }
      // (integer premultiplication with the same 2x2 dither as premul(), without its rounding calls)
      const int A = std::clamp((int)((peak * fall[(int)(t * 1024)] + kDither[((y & 1) << 1) | (x & 1)]) * 255 + 0.5f), 0, 255);
      if (!A) { row[x] = 0; continue; }
      const int u = std::clamp((int)((px - in.x) * 256 / std::max(1, in.w)), 0, 256);
      const int cr = (l.r * (256 - u) + r.r * u) >> 8, cg = (l.g * (256 - u) + r.g * u) >> 8, cb = (l.b * (256 - u) + r.b * u) >> 8;
      row[x] = (uint32_t)A << 24 | (uint32_t)((cr * A + 127) / 255) << 16 | (uint32_t)((cg * A + 127) / 255) << 8 | (uint32_t)((cb * A + 127) / 255);
    }
  }
}

void edge_colours(const uint32_t* px, int w, int h, Rgb out[4]) {
  for (int k = 0; k < 4; ++k) out[k] = {128, 128, 128};
  if (!px || w <= 0 || h <= 0) return;
  const int qx[4] = {0, 1, 1, 0}, qy[4] = {0, 0, 1, 1};
  const int step = std::max(1, std::min(w, h) / 24);
  for (int k = 0; k < 4; ++k) {
    double r = 0, g = 0, b = 0, n = 0;
    const int x0 = qx[k] * w / 2, y0 = qy[k] * h / 2;
    for (int y = y0; y < y0 + h / 2; y += step)
      for (int x = x0; x < x0 + w / 2; x += step) {
        const uint32_t p = px[(size_t)y * w + x];
        r += (p >> 16) & 255; g += (p >> 8) & 255; b += p & 255; n += 1;
      }
    if (n <= 0) continue;
    r /= n; g /= n; b /= n;
    // as light, a colour reads at full value: lift it so its brightest channel reaches ~230
    const double m = std::max({r, g, b, 1.0});
    const double k2 = std::min(4.0, 230.0 / m);
    out[k] = {(uint8_t)std::min(255.0, r * k2), (uint8_t)std::min(255.0, g * k2), (uint8_t)std::min(255.0, b * k2)};
  }
}

void iris_cover(Canvas& c, double k, int cell) {
  if (c.w <= 0 || c.h <= 0 || k >= 1) return;
  cell = std::max(1, cell);
  const uint32_t black = 0xFF000000u;
  const double cx = c.w / 2.0, cy = c.h / 2.0;
  const double rx = std::max(1.5 * cell, k * cx * 1.45), ry = std::max(1.5 * cell, k * cy * 1.45);
  for (int y0 = 0; y0 < c.h; y0 += cell) {
    const int y1 = std::min(c.h, y0 + cell);
    const double yc = (y0 + cell / 2.0) - cy;
    int a = c.w, b = c.w;
    if (k > 0 && std::fabs(yc) < ry) {
      const double half = rx * std::sqrt(1 - (yc / ry) * (yc / ry));
      const int hx = (int)std::lround(half / cell) * cell;
      a = std::max(0, (int)std::lround(cx) - hx);
      b = std::min(c.w, (int)std::lround(cx) + hx);
      if (a >= b) a = b = c.w;
    }
    for (int y = y0; y < y1; ++y) {
      uint32_t* row = c.px.data() + (size_t)y * c.w;
      if (a >= b) { std::fill(row, row + c.w, black); continue; }
      std::fill(row, row + a, black);
      std::fill(row + b, row + c.w, black);
    }
  }
}
