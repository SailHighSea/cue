#include "intro_match.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <complex>
#include <cstring>

namespace intro {
namespace {

constexpr int kBands = 33;

// A 2048-point FFT (radix 2, in place) with its tables made once.
struct Fft {
  std::array<std::complex<float>, kWindow / 2> twiddle;
  std::array<uint16_t, kWindow> reversed;
  std::array<float, kWindow> hann;
  std::array<int, kBands + 1> edge;  // the bands' first bins
  Fft() {
    const double pi = 3.14159265358979323846;
    for (int k = 0; k < kWindow / 2; ++k) twiddle[k] = std::polar(1.0f, (float)(-2 * pi * k / kWindow));
    int bits = 0;
    while ((1 << bits) < kWindow) ++bits;
    for (int i = 0; i < kWindow; ++i) {
      int r = 0;
      for (int b = 0; b < bits; ++b) if (i & (1 << b)) r |= 1 << (bits - 1 - b);
      reversed[i] = (uint16_t)r;
      hann[i] = (float)(0.5 - 0.5 * std::cos(2 * pi * i / (kWindow - 1)));
    }
    for (int b = 0; b <= kBands; ++b) {
      double f = 300 * std::pow(10.0, (double)b / kBands);  // 300 to 3000 Hz, evenly in pitch
      edge[b] = std::clamp((int)std::lround(f * kWindow / kRate), 1, kWindow / 2 - 1);
    }
    for (int b = 1; b <= kBands; ++b) edge[b] = std::max(edge[b], edge[b - 1] + 1);  // every band a bin at least
  }
  void run(std::array<std::complex<float>, kWindow>& x) const {
    for (int i = 0; i < kWindow; ++i) if (i < reversed[i]) std::swap(x[i], x[reversed[i]]);
    for (int len = 2; len <= kWindow; len <<= 1) {
      const int step = kWindow / len;
      for (int i = 0; i < kWindow; i += len)
        for (int k = 0; k < len / 2; ++k) {
          const std::complex<float> t = twiddle[k * step] * x[i + k + len / 2];
          x[i + k + len / 2] = x[i + k] - t;
          x[i + k] += t;
        }
    }
  }
};
const Fft& fft() { static const Fft f; return f; }

void put_u32(std::string& s, uint32_t v) { for (int i = 0; i < 4; ++i) s.push_back((char)(v >> (8 * i))); }
void put_f64(std::string& s, double d) { uint64_t v; std::memcpy(&v, &d, 8); for (int i = 0; i < 8; ++i) s.push_back((char)(v >> (8 * i))); }
struct Reader {
  const std::string& s;
  size_t at = 0;
  bool ok = true;
  uint32_t u32() {
    if (at + 4 > s.size()) { ok = false; return 0; }
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= (uint32_t)(unsigned char)s[at + i] << (8 * i);
    at += 4;
    return v;
  }
  double f64() {
    if (at + 8 > s.size()) { ok = false; return 0; }
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= (uint64_t)(unsigned char)s[at + i] << (8 * i);
    at += 8;
    double d;
    std::memcpy(&d, &v, 8);
    return d;
  }
};

void put_print(std::string& s, const Print& p) {
  put_f64(s, p.t0);
  put_u32(s, (uint32_t)p.code.size());
  for (uint32_t c : p.code) put_u32(s, c);
  s.append(reinterpret_cast<const char*>(p.quiet.data()), p.quiet.size());
}
bool get_print(Reader& r, Print& p) {
  p.t0 = r.f64();
  const uint32_t n = r.u32();
  if (!r.ok || n > 200000 || !std::isfinite(p.t0) || r.at + (size_t)n * 5 > r.s.size()) return false;
  p.code.resize(n);
  for (auto& c : p.code) c = r.u32();
  p.quiet.assign(r.s.begin() + (ptrdiff_t)r.at, r.s.begin() + (ptrdiff_t)(r.at + n));
  r.at += n;
  return r.ok;
}

}  // namespace

Print make_print(const int16_t* pcm, size_t n, double t0) {
  Print p;
  p.t0 = t0;
  if (!pcm || n < (size_t)kWindow) return p;
  const Fft& f = fft();
  const size_t frames = (n - kWindow) / kHop + 1;
  p.code.reserve(frames);
  p.quiet.reserve(frames);
  std::array<std::complex<float>, kWindow> x;
  std::vector<std::array<float, kBands>> bands;
  bands.reserve(frames);
  std::array<float, kBands> band{};
  for (size_t fr = 0; fr < frames; ++fr) {
    const int16_t* s = pcm + fr * kHop;
    double power = 0;
    for (int i = 0; i < kWindow; ++i) {
      power += (double)s[i] * s[i];
      x[i] = {s[i] * f.hann[i], 0.0f};
    }
    f.run(x);
    for (int b = 0; b < kBands; ++b) {
      double e = 0;
      for (int k = f.edge[b]; k < f.edge[b + 1]; ++k) e += std::norm(x[k]);
      band[b] = (float)std::log(e + 1.0);
    }
    uint32_t code = 0;
    const bool have_prev = bands.size() >= (size_t)kLag;
    if (have_prev) {
      const auto& prev = bands[bands.size() - kLag];
      for (int m = 0; m < 32; ++m)
        if ((band[m] - band[m + 1]) - (prev[m] - prev[m + 1]) > 0) code |= 1u << m;
    }
    p.code.push_back(code);
    // under -50 dB of full scale: silence, or near it
    p.quiet.push_back(!have_prev || std::sqrt(power / kWindow) < 100.0 ? 1 : 0);
    bands.push_back(band);
  }
  return p;
}

Match longest_shared(const Print& a, const Print& b) {
  Match best;
  const int na = (int)a.code.size(), nb = (int)b.code.size();
  if (na < 2 || nb < 2) return best;
  // Where the two can line up: at each distance, how many frames (one in three) agree. A
  // shared song stands out as a peak; the best few are then read frame by frame.
  auto agrees = [&](int i, int j) {
    return !a.quiet[i] && !b.quiet[j] && std::popcount(a.code[i] ^ b.code[j]) <= 10;
  };
  std::vector<int> votes(na + nb, 0);
  for (int d = -(nb - 1); d < na; ++d) {
    const int i0 = std::max(0, d), i1 = std::min(na, nb + d);
    int v = 0;
    for (int i = i0; i < i1; i += 3) v += agrees(i, i - d);
    votes[d + nb] = v;
  }
  std::vector<int> picks;
  for (int round = 0; round < 4; ++round) {
    int top = -1;
    for (int d = 0; d < (int)votes.size(); ++d) if (votes[d] >= 40 && (top < 0 || votes[d] > votes[top])) top = d;
    if (top < 0) break;
    picks.push_back(top - nb);
    for (int d = std::max(0, top - 8); d < std::min((int)votes.size(), top + 9); ++d) votes[d] = 0;
  }
  const double fps = (double)kRate / kHop;
  const int gap = (int)(0.6 * fps);  // a short miss (a sound effect over the song) keeps the stretch
  int best_len = 0;
  const int window = (int)fps;  // a stretch starts and ends where a whole second mostly agrees
  std::vector<uint8_t> g;
  std::vector<int> sum;
  for (int pick : picks)
    for (int d = pick - 2; d <= pick + 2; ++d) {
      // a[i] beside b[i - d], along this distance
      const int i0 = std::max(0, d), i1 = std::min(na, nb + d), n = i1 - i0;
      if (n <= window) continue;
      g.assign(n, 0);
      sum.assign(n + 1, 0);
      for (int k = 0; k < n; ++k) { g[k] = agrees(i0 + k, i0 + k - d); sum[k + 1] = sum[k] + g[k]; }
      auto dense = [&](int k) { return k + window <= n && (sum[k + window] - sum[k]) * 10 >= window * 6; };
      int start = -1, last = -1, good = 0, misses = 0;
      auto close = [&] {
        if (start < 0) return;
        // the stretch's edges: chance agreements just outside it (a few frames, apart) do not count
        int s0 = start, s1 = last;
        while (s0 < s1 && !dense(s0)) ++s0;
        while (s1 > s0 && !dense(std::max(0, s1 - window + 1))) --s1;
        const int len = s1 - s0 + 1;
        if (len > best_len && (sum[s1 + 1] - sum[s0]) * 2 >= len) {
          best_len = len;
          // (a song's last notes ring on, alike in both, for a moment after it: the end is
          // taken a little early, so a skip never lands past the first words that follow)
          const double ring = 0.5;
          best.a0 = a.seconds(i0 + s0); best.a1 = a.seconds(i0 + s1 + 1) - ring;
          best.b0 = b.seconds(i0 + s0 - d); best.b1 = b.seconds(i0 + s1 + 1 - d) - ring;
        }
        start = last = -1; good = misses = 0;
      };
      for (int k = 0; k < n; ++k) {
        if (g[k]) {
          if (start < 0) start = k;
          last = k; ++good; misses = 0;
        } else if (start >= 0 && ++misses > gap) close();
      }
      close();
    }
  const double len = best_len / fps;
  best.found = best_len > 0 && len >= kMinPart && len <= kMaxPart;
  return best;
}

void shared_parts(const EpisodePrints& a, const EpisodePrints& b, std::vector<SkipPart>& out_a,
                  std::vector<SkipPart>& out_b) {
  out_a.clear();
  out_b.clear();
  if (a.dur < kMinFilm || b.dur < kMinFilm) return;
  auto keep = [](std::vector<SkipPart>& out, SkipKind k, double s, double e, double dur) {
    if (s < 2) s = 0;                  // from the very first second
    if (e > dur - 3) e = dur;          // to the very last
    out.push_back({k, s, e, true});
  };
  Match op = longest_shared(a.head, b.head);
  if (op.found) { keep(out_a, SkipKind::Opening, op.a0, op.a1, a.dur); keep(out_b, SkipKind::Opening, op.b0, op.b1, b.dur); }
  Match ed = longest_shared(a.tail, b.tail);
  if (ed.found) { keep(out_a, SkipKind::Ending, ed.a0, ed.a1, a.dur); keep(out_b, SkipKind::Ending, ed.b0, ed.b1, b.dur); }
}

std::string encode(const EpisodePrints& e) {
  std::string s = "CUEP";
  put_u32(s, 1);
  put_f64(s, e.dur);
  put_print(s, e.head);
  put_print(s, e.tail);
  return s;
}

bool decode(const std::string& bytes, EpisodePrints& e) {
  if (bytes.size() < 12 || bytes.compare(0, 4, "CUEP") != 0) return false;
  Reader r{bytes};
  r.at = 4;
  if (r.u32() != 1) return false;
  e.dur = r.f64();
  if (!r.ok || !std::isfinite(e.dur) || e.dur < 0) return false;
  return get_print(r, e.head) && get_print(r, e.tail) && r.at == bytes.size();
}

// (times in whole milliseconds: plain integers, read the same whatever the PC's number format)
std::string encode_parts(const std::vector<SkipPart>& parts) {
  std::string s;
  for (const SkipPart& p : parts) {
    const char k = p.kind == SkipKind::Opening ? 'O' : p.kind == SkipKind::Ending ? 'E' : p.kind == SkipKind::Preview ? 'P'
                 : p.kind == SkipKind::Recap ? 'R' : 0;
    if (!k || !(p.start >= 0) || !(p.end > p.start) || p.end > 1e9) continue;
    if (!s.empty()) s += ' ';
    s += k;
    s += std::to_string((long long)std::llround(p.start * 1000));
    s += ',';
    s += std::to_string((long long)std::llround(p.end * 1000));
  }
  return s;
}

bool decode_parts(const std::string& text, std::vector<SkipPart>& parts) {
  parts.clear();
  size_t i = 0;
  auto number = [&](long long& v) {
    const size_t from = i;
    v = 0;
    while (i < text.size() && text[i] >= '0' && text[i] <= '9' && i - from < 13) v = v * 10 + (text[i++] - '0');
    return i > from;
  };
  while (i < text.size()) {
    if (text[i] == ' ') { ++i; continue; }
    SkipPart p;
    p.from_sound = true;
    switch (text[i]) {
      case 'O': p.kind = SkipKind::Opening; break;
      case 'E': p.kind = SkipKind::Ending; break;
      case 'P': p.kind = SkipKind::Preview; break;
      case 'R': p.kind = SkipKind::Recap; break;
      default: parts.clear(); return false;
    }
    ++i;
    long long a = 0, b = 0;
    if (!number(a) || i >= text.size() || text[i] != ',') { parts.clear(); return false; }
    ++i;
    if (!number(b) || b <= a || (i < text.size() && text[i] != ' ')) { parts.clear(); return false; }
    p.start = a / 1000.0;
    p.end = b / 1000.0;
    parts.push_back(p);
  }
  return true;
}

}  // namespace intro
