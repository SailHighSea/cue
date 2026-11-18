// HDR films seen by the preview helper: the timeline's pictures, the home's frames and the ambient
// light's colours come from the engine's raw frame (vo=null, no GPU, no tone mapping), and an HDR
// film's raw frame is still in PQ or HLG and in BT.2020: read as if it were SDR it is washed out
// (measured against the shipped engine: 56% less saturation for HDR10, 41% less for HLG).
// This turns such a frame, read at 16 bits a channel, into the SDR picture a person would expect:
// the signal to light (SDR white = 1.0, at 203 nits), BT.2020 to BT.709, a gentle roll-off above
// 0.8 of the white, and the gamma of an SDR film (BT.1886, 2.4). Two tables per transfer, made once;
// per pixel: two lookups a channel and a 3x3 matrix. Pixels the matrix puts out of range are clipped.
// Portable (the helper is Windows only; tests/hdr_preview_test.cpp checks it anywhere).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <vector>

namespace hdr_preview {

enum class Transfer { None, PQ, HLG };

// The engine's name for the film's transfer (video-params/gamma): "pq" or "hlg" are HDR.
inline Transfer transfer_of(std::string_view gamma) {
  if (gamma == "pq") return Transfer::PQ;
  if (gamma == "hlg") return Transfer::HLG;
  return Transfer::None;
}

// HLG's scene light for a signal (0..1), 1.0 = the brightest (its inverse OETF).
inline double hlg_scene(double e) {
  e = std::clamp(e, 0.0, 1.0);
  const double a = 0.17883277, b = 0.28466892, c = 0.55991073;
  return e <= 0.5 ? e * e / 3 : (std::exp((e - c) / a) + b) / 12;
}

// HLG on a display: the system gamma (1.2, a 1000-nit display) is applied to the scene's luminance
// (BT.2100's OOTF), which keeps the colour's proportions. Light in nits / 203 for scene light
// (r, g, b) and its luminance ys; the reference white (0.75 of the signal) comes out at 1.0.
inline constexpr double kHlgDisplay = 1000.0 / 203;
inline double hlg_ootf_gain(double ys) { return ys > 0 ? kHlgDisplay * std::pow(ys, 0.2) : 0; }

// Light for a signal (0..1), 1.0 = SDR white (203 nits); for HLG, a grey's (equal channels).
inline double to_light(Transfer t, double e) {
  e = std::clamp(e, 0.0, 1.0);
  if (t == Transfer::PQ) {
    const double m1 = 0.1593017578125, m2 = 78.84375, c1 = 0.8359375, c2 = 18.8515625, c3 = 18.6875;
    const double p = std::pow(e, 1 / m2);
    return 10000.0 / 203 * std::pow(std::max(p - c1, 0.0) / (c2 - c3 * p), 1 / m1);
  }
  if (t == Transfer::HLG) {
    const double l = hlg_scene(e);
    return hlg_ootf_gain(l) * l;
  }
  return e;
}

// The SDR picture's value (0..255) for light (BT.709), after the roll-off.
inline int to_sdr(double v) {
  v = std::max(v, 0.0);
  v = v <= 0.8 ? v : 0.8 + 0.2 * (1 - std::exp(-(v - 0.8) / 0.2));
  return (int)std::lround(std::pow(std::min(v, 1.0), 1 / 2.4) * 255);
}

class Mapper {
 public:
  static const Mapper& get(Transfer t) {
    static const Mapper pq(Transfer::PQ), hlg(Transfer::HLG);
    return t == Transfer::HLG ? hlg : pq;
  }
  // One pixel of the engine's rgba64 frame (R, G, B, A in 16 bits) as 0xAARRGGBB, opaque.
  uint32_t map(const uint16_t* q) const {
    float R = light_[q[0] >> kShift], G = light_[q[1] >> kShift], B = light_[q[2] >> kShift];
    if (hlg_) {  // scene light: the display's gain for its luminance (BT.2020)
      const float ys = 0.2627f * R + 0.6780f * G + 0.0593f * B;
      const float gain = gain_[std::min((int)(std::sqrt(std::max(ys, 0.0f)) * kGain + 0.5f), kGain)];
      R *= gain; G *= gain; B *= gain;
    }
    const float r = 1.6604910f * R - 0.5876411f * G - 0.0728499f * B;
    const float g = -0.1245505f * R + 1.1328999f * G - 0.0083494f * B;
    const float b = -0.0181508f * R - 0.1005789f * G + 1.1187297f * B;
    return 0xFF000000u | (uint32_t)out(r) << 16 | (uint32_t)out(g) << 8 | (uint32_t)out(b);
  }

 private:
  static constexpr int kBits = 16, kShift = 16 - kBits;  // the signal whole (256 KB, made once, only for an HDR film)
  static constexpr int kOut = 8192;                      // the light's table, by its square root
  static constexpr int kGain = 8192;                     // HLG's gain table, by the luminance's square root
  bool hlg_ = false;
  std::vector<float> light_, gain_;
  std::vector<uint8_t> sdr_;
  float max_ = 1, scale_ = 1;
  explicit Mapper(Transfer t) : hlg_(t == Transfer::HLG), light_(1u << kBits), sdr_(kOut + 1) {
    for (size_t i = 0; i < light_.size(); ++i) {
      const double e = (double)(i << kShift) / 65535;
      light_[i] = (float)(hlg_ ? hlg_scene(e) : to_light(t, e));  // (HLG: scene light, the gain comes after)
    }
    if (hlg_) {
      gain_.resize(kGain + 1);
      for (int i = 0; i <= kGain; ++i) gain_[i] = (float)hlg_ootf_gain((double)i * i / ((double)kGain * kGain));
    }
    max_ = (float)to_light(t, 1.0) * 1.7f;  // (the matrix can lift a channel past the brightest signal)
    scale_ = kOut / std::sqrt(max_);
    for (int i = 0; i <= kOut; ++i) {
      const double s = (double)i / kOut;
      sdr_[i] = (uint8_t)to_sdr(s * s * max_);
    }
  }
  uint8_t out(float v) const {
    if (!(v > 0)) return 0;
    const int i = (int)(std::sqrt(std::min(v, max_)) * scale_ + 0.5f);
    return sdr_[std::min(i, kOut)];
  }
};

}  // namespace hdr_preview
