// HDR previews (src/core/hdr_preview.h): an SDR colour put into PQ or HLG and BT.2020, the way an
// HDR film carries it, comes back as that colour (within a few steps below the roll-off); greys stay
// grey; the brightest signal is not past white; SDR films are not touched (transfer None).
#include "../src/core/hdr_preview.h"

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace hdr_preview;

static double bt1886_to_light(int v) { return std::pow(v / 255.0, 2.4); }

// The signal for light (1.0 = SDR white), inverse of to_light.
static double to_signal(Transfer t, double l) {
  if (t == Transfer::PQ) {
    const double m1 = 0.1593017578125, m2 = 78.84375, c1 = 0.8359375, c2 = 18.8515625, c3 = 18.6875;
    const double y = std::pow(l * 203 / 10000.0, m1);
    return std::pow((c1 + c2 * y) / (1 + c3 * y), m2);
  }
  const double a = 0.17883277, b = 0.28466892, c = 0.55991073;
  const double s = l;  // (scene light: see encode)
  return s <= 1.0 / 12 ? std::sqrt(3 * s) : a * std::log(12 * s - b) + c;
}

// An 8-bit BT.709 colour as an HDR film's 16-bit pixel.
static void encode(Transfer t, int r8, int g8, int b8, uint16_t* q) {
  const double r = bt1886_to_light(r8), g = bt1886_to_light(g8), b = bt1886_to_light(b8);
  double R = 0.6274039 * r + 0.3292830 * g + 0.0433131 * b;  // BT.709 -> BT.2020
  double G = 0.0690973 * r + 0.9195404 * g + 0.0113623 * b;
  double B = 0.0163914 * r + 0.0880133 * g + 0.8955953 * b;
  // at 16 bits (a 10-bit film's own step, next to black, is several of 255: that is the film)
  auto w = [](double e) { return (uint16_t)std::lround(e * 65535); };
  if (t == Transfer::HLG) {  // display light -> scene light (the OOTF's inverse: Yd = 1000/203 * Ys^1.2)
    const double yd = 0.2627 * R + 0.6780 * G + 0.0593 * B;
    const double ys = yd > 0 ? std::pow(yd / kHlgDisplay, 1 / 1.2) : 0;
    const double k = yd > 0 ? ys / yd : 0;
    R *= k; G *= k; B *= k;
  }
  q[0] = w(to_signal(t, R));
  q[1] = w(to_signal(t, G));
  q[2] = w(to_signal(t, B));
  q[3] = 65535;
}

int main() {
  assert(transfer_of("pq") == Transfer::PQ && transfer_of("hlg") == Transfer::HLG);
  assert(transfer_of("bt.1886") == Transfer::None && transfer_of("srgb") == Transfer::None && transfer_of("") == Transfer::None);
  int checked = 0, worst = 0;
  for (Transfer t : {Transfer::PQ, Transfer::HLG}) {
    const Mapper& m = Mapper::get(t);
    // the tables give what the exact sums give, for any pixel
    uint32_t seed = 12345;
    for (int k = 0; k < 200000; ++k) {
      uint16_t q[4];
      for (int c = 0; c < 4; ++c) { seed = seed * 1664525u + 1013904223u; q[c] = (uint16_t)(seed >> 16); }
      double R = to_light(t, q[0] / 65535.0), G = to_light(t, q[1] / 65535.0), B = to_light(t, q[2] / 65535.0);
      if (t == Transfer::HLG) {
        const double r = hlg_scene(q[0] / 65535.0), g = hlg_scene(q[1] / 65535.0), b = hlg_scene(q[2] / 65535.0);
        const double k = hlg_ootf_gain(0.2627 * r + 0.6780 * g + 0.0593 * b);
        R = k * r; G = k * g; B = k * b;
      }
      const int want[3] = {to_sdr(1.6604910021 * R - 0.5876411388 * G - 0.0728498633 * B),
                           to_sdr(-0.1245504745 * R + 1.1328998971 * G - 0.0083494226 * B),
                           to_sdr(-0.0181507634 * R - 0.100578898 * G + 1.1187296614 * B)};
      const uint32_t v = m.map(q);
      const int got[3] = {(int)(v >> 16 & 255), (int)(v >> 8 & 255), (int)(v & 255)};
      for (int c = 0; c < 3; ++c)
        if (std::abs(got[c] - want[c]) > 1) { std::printf("hdr preview: table %d, exact %d\n", got[c], want[c]); return 1; }
    }
    // colours below the roll-off (light under 0.8 of the white: 8-bit values up to ~233) come back;
    // (every channel 20 or more: a channel next to black is the difference of large ones)
    for (int r = 20; r <= 230; r += 10)
      for (int g = 20; g <= 230; g += 10)
        for (int b = 20; b <= 230; b += 10) {
          uint16_t q[4];
          encode(t, r, g, b, q);
          const uint32_t v = m.map(q);
          assert(v >> 24 == 0xFF);
          const int R = v >> 16 & 255, G = v >> 8 & 255, B = v & 255;
          const int d = std::max({std::abs(R - r), std::abs(G - g), std::abs(B - b)});
          worst = std::max(worst, d);
          if (d > 3) { std::printf("hdr preview: %d %d %d came back %d %d %d\n", r, g, b, R, G, B); return 1; }
          ++checked;
        }
    // greys stay grey, and brighter is never darker
    int last = -1;
    for (int i = 0; i <= 65535; i += 97) {
      const uint16_t q[4] = {(uint16_t)i, (uint16_t)i, (uint16_t)i, 65535};
      const uint32_t v = m.map(q);
      const int R = v >> 16 & 255, G = v >> 8 & 255, B = v & 255;
      assert(std::abs(R - G) <= 1 && std::abs(G - B) <= 1);
      assert(G >= last);
      last = G;
    }
    // the brightest signal: just under white (the roll-off keeps a little room), never past it
    const uint16_t top[4] = {65535, 65535, 65535, 65535};
    assert((m.map(top) >> 8 & 255) >= 250);
    // the SDR white sits where an SDR film's white would, softened by the roll-off
    uint16_t w[4];
    encode(t, 255, 255, 255, w);
    const int white = m.map(w) >> 8 & 255;
    assert(white >= 240 && white <= 250);
    // black is black
    const uint16_t black[4] = {0, 0, 0, 65535};
    assert((m.map(black) & 0xFFFFFF) == 0);
  }
  // speed: a 320 x 180 preview, the helper's usual size
  std::vector<uint16_t> frame(320 * 180 * 4);
  for (size_t i = 0; i < frame.size(); ++i) frame[i] = (uint16_t)(i * 2654435761u >> 16);
  std::vector<uint32_t> out(320 * 180);
  const Mapper& m = Mapper::get(Transfer::PQ);
  auto t0 = std::chrono::steady_clock::now();
  for (int k = 0; k < 50; ++k)
    for (size_t i = 0; i < out.size(); ++i) out[i] = m.map(&frame[i * 4]);
  const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / 50;
  std::printf("HDR previews: tables equal to the exact sums (400000 pixels), %d PQ/HLG colours back within %d steps of 255, greys grey, highlights rolled off under white; "
              "a 320x180 frame in %.2f ms passed\n", checked, worst, ms);
  return 0;
}
