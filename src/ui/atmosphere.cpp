#include "atmosphere.h"

#include <algorithm>
#include <cmath>

namespace {

struct Hsl { double h, s, l; };

Hsl to_hsl(double r, double g, double b) {
  double mx = std::max({r, g, b}), mn = std::min({r, g, b});
  double l = (mx + mn) / 2, d = mx - mn;
  if (d < 1e-6) return {0, 0, l};
  double s = l > 0.5 ? d / (2 - mx - mn) : d / (mx + mn);
  double h;
  if (mx == r) h = (g - b) / d + (g < b ? 6 : 0);
  else if (mx == g) h = (b - r) / d + 2;
  else h = (r - g) / d + 4;
  return {h / 6, s, l};
}

double hue2(double p, double q, double t) {
  if (t < 0) t += 1;
  if (t > 1) t -= 1;
  if (t < 1.0 / 6) return p + (q - p) * 6 * t;
  if (t < 0.5) return q;
  if (t < 2.0 / 3) return p + (q - p) * (2.0 / 3 - t) * 6;
  return p;
}

Rgb from_hsl(Hsl c) {
  double r, g, b;
  if (c.s <= 0) r = g = b = c.l;
  else {
    double q = c.l < 0.5 ? c.l * (1 + c.s) : c.l + c.s - c.l * c.s, p = 2 * c.l - q;
    r = hue2(p, q, c.h + 1.0 / 3); g = hue2(p, q, c.h); b = hue2(p, q, c.h - 1.0 / 3);
  }
  auto u = [](double v) { return (uint8_t)std::lround(std::clamp(v, 0.0, 1.0) * 255); };
  return {u(r), u(g), u(b)};
}

}  // namespace

FrameColors analyze_frame(const uint32_t* px, int w, int h) {
  FrameColors out;
  if (!px || w < 4 || h < 4) return out;
  constexpr int kBins = 24;
  double wsum[kBins] = {}, rs[kBins] = {}, gs[kBins] = {}, bs[kBins] = {};
  double neutral = 0, total = 0;
  // edge bands (outer 22% of each side) for the ambient light
  double er[4] = {}, eg[4] = {}, eb[4] = {}, ew[4] = {}, el[4] = {}, en[4] = {};
  int step = std::max(1, std::min(w, h) / 48);
  for (int y = 0; y < h; y += step)
    for (int x = 0; x < w; x += step) {
      uint32_t v = px[(size_t)y * w + x];
      double r = ((v >> 16) & 255) / 255.0, g = ((v >> 8) & 255) / 255.0, b = (v & 255) / 255.0;
      double lum = 0.2126 * r + 0.7152 * g + 0.0722 * b;
      Hsl c = to_hsl(r, g, b);
      double fx = (double)x / w, fy = (double)y / h;
      int sides[4] = {fy < 0.22, fx > 0.78, fy > 0.78, fx < 0.22};
      for (int k = 0; k < 4; ++k)
        if (sides[k]) {
          double ww = 0.15 + c.s;  // colour counts more than grey, but grey still lights
          er[k] += r * ww; eg[k] += g * ww; eb[k] += b * ww; ew[k] += ww; el[k] += lum; en[k] += 1;
        }
      total += 1;
      if (lum < 0.07) continue;                 // deep blacks say nothing about the mood
      if (lum > 0.93 && c.s < 0.15) continue;   // blown highlights neither
      if (c.s < 0.12) { neutral += 0.35 + lum; continue; }
      // colourful mid tones weigh most; tiny saturated specks do not win on their own
      double wgt = std::pow(c.s, 1.3) * (0.35 + std::min(lum, 0.7));
      int bin = std::clamp((int)(c.h * kBins), 0, kBins - 1);
      wsum[bin] += wgt; rs[bin] += r * wgt; gs[bin] += g * wgt; bs[bin] += b * wgt;
    }
  if (total <= 0) return out;
  // the most representative hue family (a bin with its neighbours)
  int best = -1;
  double best_score = 0, colored = 0;
  for (int i = 0; i < kBins; ++i) {
    colored += wsum[i];
    double sc = wsum[i] + 0.5 * (wsum[(i + 1) % kBins] + wsum[(i + kBins - 1) % kBins]);
    if (sc > best_score) { best_score = sc; best = i; }
  }
  out.valid = true;
  if (best >= 0 && best_score > 0) {
    double R = 0, G = 0, B = 0, W = 0;
    for (int d = -1; d <= 1; ++d) {
      int i = (best + d + kBins) % kBins;
      double f = d == 0 ? 1.0 : 0.5;
      R += rs[i] * f; G += gs[i] * f; B += bs[i] * f; W += wsum[i] * f;
    }
    Hsl c = to_hsl(R / W, G / W, B / W);
    // tame it: never neon, always readable on the black dock
    c.s = std::min(c.s, 0.55);
    c.l = std::clamp(c.l, 0.62, 0.74);
    out.accent = from_hsl(c);
    // how much of the picture carries that colour decides how much the UI takes it
    double share = best_score / std::max(1e-6, colored + neutral * 0.6);
    double presence = std::min(1.0, colored / (total * 0.12));
    out.strength = std::clamp(share * presence * 1.4, 0.0, 1.0);
  }
  // zones: the outer band of the picture, cut into segments around it
  {
    double zr[FrameColors::kZones] = {}, zg[FrameColors::kZones] = {}, zb[FrameColors::kZones] = {}, zn[FrameColors::kZones] = {};
    auto zone_of = [&](double fx, double fy, int* out) {
      int n = 0;
      if (fy < 0.18) out[n++] = std::clamp((int)(fx * 8), 0, 7);
      if (fx > 0.82) out[n++] = 8 + std::clamp((int)(fy * 5), 0, 4);
      if (fy > 0.82) out[n++] = 13 + std::clamp((int)((1 - fx) * 8), 0, 7);
      if (fx < 0.18) out[n++] = 21 + std::clamp((int)((1 - fy) * 5), 0, 4);
      return n;
    };
    for (int y = 0; y < h; y += step)
      for (int x = 0; x < w; x += step) {
        double fx = (x + 0.5) / w, fy = (y + 0.5) / h;
        int zs[2];
        int n = zone_of(fx, fy, zs);
        if (!n) continue;
        uint32_t v = px[(size_t)y * w + x];
        double r = ((v >> 16) & 255) / 255.0, g = ((v >> 8) & 255) / 255.0, b = (v & 255) / 255.0;
        for (int i = 0; i < n; ++i) { zr[zs[i]] += r; zg[zs[i]] += g; zb[zs[i]] += b; zn[zs[i]] += 1; }
      }
    for (int z = 0; z < FrameColors::kZones; ++z) {
      if (zn[z] <= 0) continue;
      double r = zr[z] / zn[z], g = zg[z] / zn[z], b = zb[z] / zn[z];
      double mx = std::max({r, g, b});
      Hsl c = to_hsl(r, g, b);
      // like the TV: the hue at full strength, its brightness carried separately
      if (c.s > 0.06) c.s = std::min(0.92, c.s * 1.35 + 0.08);
      c.l = c.s > 0.06 ? 0.55 : std::min(0.92, 0.5 + c.l * 0.5);
      out.zone[z] = from_hsl(c);
      out.zone_light[z] = std::clamp(std::pow(mx, 0.8) * 1.15, 0.0, 1.0);
    }
  }
  for (int k = 0; k < 4; ++k) {
    if (ew[k] <= 0) continue;
    Hsl c = to_hsl(er[k] / ew[k], eg[k] / ew[k], eb[k] / ew[k]);
    c.s = std::min(c.s, 0.6);
    c.l = std::clamp(c.l, 0.35, 0.6);
    out.edge[k] = from_hsl(c);
    out.edge_light[k] = std::clamp(el[k] / std::max(1.0, en[k]) * 1.6, 0.0, 1.0);
  }
  return out;
}

bool ColorSmoother::step(Rgb t, double tk, double dt, double tau) {
  if (!primed) { r = t.r; g = t.g; b = t.b; k = tk; primed = true; return true; }
  double a = 1 - std::exp(-dt / std::max(1e-3, tau));
  double nr = r + (t.r - r) * a, ng = g + (t.g - g) * a, nb = b + (t.b - b) * a, nk = k + (tk - k) * a;
  bool moved = std::fabs(nr - r) + std::fabs(ng - g) + std::fabs(nb - b) > 0.02 || std::fabs(nk - k) > 0.0005;
  r = nr; g = ng; b = nb; k = nk;
  return moved;
}

Rgb ColorSmoother::rgb() const {
  auto u = [](double v) { return (uint8_t)std::lround(std::clamp(v, 0.0, 255.0)); };
  return {u(r), u(g), u(b)};
}

namespace {
double srgb_to_linear(double v) { return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); }
double linear_to_srgb(double v) { return v <= 0.0031308 ? v * 12.92 : 1.055 * std::pow(v, 1 / 2.4) - 0.055; }
const double* decode_table() {  // the 256 sRGB codes in linear light, made once
  static double t[256];
  static bool made = false;
  if (!made) { for (int i = 0; i < 256; ++i) t[i] = srgb_to_linear(i / 255.0); made = true; }
  return t;
}
// linear light (0..1) to an sRGB code, looked up by its square root (fine near black, where the
// codes are dense): within a twentieth of a code of the exact curve, without a pow per channel
constexpr int kEnc = 4096;
const uint8_t* encode_table() {
  static uint8_t t[kEnc + 1];
  static bool made = false;
  if (!made) {
    for (int i = 0; i <= kEnc; ++i) { const double u = (double)i / kEnc; t[i] = (uint8_t)std::lround(std::clamp(linear_to_srgb(u * u), 0.0, 1.0) * 255); }
    made = true;
  }
  return t;
}
inline uint8_t encode8(double v) { return encode_table()[(int)std::lround(std::sqrt(std::clamp(v, 0.0, 1.0)) * kEnc)]; }
}  // namespace

Lab to_oklab(Rgb c) {
  const double* d = decode_table();
  const double r = d[c.r], g = d[c.g], b = d[c.b];
  const double l = std::cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
  const double m = std::cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
  const double s = std::cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
  return {0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s, 1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
          0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s};
}

Rgb from_oklab(const Lab& c) {
  const double l = c.L + 0.3963377774 * c.a + 0.2158037573 * c.b, m = c.L - 0.1055613458 * c.a - 0.0638541728 * c.b,
               s = c.L - 0.0894841775 * c.a - 1.2914855480 * c.b;
  const double l3 = l * l * l, m3 = m * m * m, s3 = s * s * s;
  const double r = 4.0767416621 * l3 - 3.3077115913 * m3 + 0.2309699292 * s3;
  const double g = -1.2684380046 * l3 + 2.6097574011 * m3 - 0.3413193965 * s3;
  const double b = -0.0041960863 * l3 - 0.7034186147 * m3 + 1.7076147010 * s3;
  return {encode8(r), encode8(g), encode8(b)};
}

bool LightBreath::step(Rgb t, double tk, double dt, double stage_tau) {
  if (!primed || t.r != target_rgb.r || t.g != target_rgb.g || t.b != target_rgb.b) { target_rgb = t; target_lab = to_oklab(t); }
  const double want[4] = {target_lab.L, target_lab.a, target_lab.b, tk};
  if (!primed) {
    for (auto& st : s) for (int c = 0; c < 4; ++c) st[c] = want[c];
    tau = tau_to = stage_tau; primed = true; resting = true; out = t; k = tk;
    return true;
  }
  tau_to = stage_tau;
  // (a new pace is eased into, ~0.25 s: changing it at once would change the speed at once)
  // (the 26 zones step with the same dt and pace: the exponentials are worked out once per tick)
  static double c_dt = -1, c_tau = -1, c_ease = 0, c_a = 0;
  if (dt != c_dt) { c_dt = dt; c_ease = 1 - std::exp(-dt / 0.25); c_tau = -1; }
  if (tau != tau_to) tau += (tau_to - tau) * c_ease;
  if (std::fabs(tau - tau_to) < 1e-4) tau = tau_to;
  if (tau != c_tau) { c_tau = tau; c_a = 1 - std::exp(-dt / std::max(1e-3, tau)); }
  const double a = c_a;
  double far = 0;
  for (int c = 0; c < 4; ++c) {
    double prev = want[c];
    for (int i = 0; i < kStages; ++i) { s[i][c] += (prev - s[i][c]) * a; prev = s[i][c]; }
    for (int i = 0; i < kStages; ++i) far = std::max(far, std::fabs(want[c] - s[i][c]) * (c == 3 ? 0.25 : 1.0));
  }
  // landed (under a tenth of what an 8-bit step is): it rests exactly on the target
  if (far < 0.0004) {
    for (auto& st : s) for (int c = 0; c < 4; ++c) st[c] = want[c];
    const bool was = resting;
    resting = true;
    const bool changed = out.r != t.r || out.g != t.g || out.b != t.b || std::fabs(k - tk) > 1e-9;
    out = t; k = tk;
    return changed || !was;
  }
  resting = false;
  const double* o = s[kStages - 1];
  out = from_oklab({o[0], o[1], o[2]});
  k = std::clamp(o[3], 0.0, 1.0);
  return true;
}
