// Timeline previews: the keyframes of a film from its own index, and which keyframe a preview
// lands on (src/core/keyframe_index.h, used by the preview helper in src/win/thumbhost.cpp).
// - Matroska Cues and MP4 sync samples (stss, stts, ctts) read from files built here byte by
//   byte; the first video track only; a SeekHead pointing at Cues after the clusters.
// - What the engine answered: a request at t returning the keyframe a covers [a, t].
// - The index is used only once two decoded answers agree with it (an MP4 edit list shifts the
//   engine's times: the shift is found), and dropped for the file at the first disagreement.
// - Truncated, oversized and random files give an empty index and never read out of bounds.
// Measured with the real helper and engine under Wine (VALIDATION.md): the same pictures and
// times for every request, and a 4K HEVC sweep of 30 s decoding 0 to 4 pictures instead of 30.
#include "../src/core/keyframe_index.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

using Bytes = std::vector<uint8_t>;
static void put(Bytes& b, const Bytes& x) { b.insert(b.end(), x.begin(), x.end()); }

// ── a Matroska file ─────────────────────────────────────────────────
static Bytes id_bytes(uint32_t id) {  // IDs are written with their length marker, as the specification lists them
  const int n = id > 0xFFFFFF ? 4 : id > 0xFFFF ? 3 : id > 0xFF ? 2 : 1;
  Bytes b; for (int i = n - 1; i >= 0; --i) b.push_back((uint8_t)(id >> (i * 8))); return b;
}
static Bytes size8(uint64_t n) { Bytes b{0x01}; for (int s = 48; s >= 0; s -= 8) b.push_back((uint8_t)(n >> s)); return b; }
static Bytes el(uint32_t id, const Bytes& data) { Bytes b = id_bytes(id); put(b, size8(data.size())); put(b, data); return b; }
static Bytes uint_el(uint32_t id, uint64_t v) { Bytes d; for (int s = 56; s >= 0; s -= 8) if ((v >> s) || !d.empty() || s == 0) d.push_back((uint8_t)(v >> s)); return el(id, d); }
static Bytes cat(std::initializer_list<Bytes> parts) { Bytes b; for (auto& p : parts) put(b, p); return b; }

// tracks: {number, type}; cues: {time, track}
static Bytes mkv(uint64_t scale, std::vector<std::pair<int, int>> tracks, std::vector<std::pair<uint64_t, int>> cues, bool cues_at_end) {
  Bytes entries;
  for (auto [n, t] : tracks) put(entries, el(0xAE, cat({uint_el(0xD7, n), uint_el(0x83, t)})));
  Bytes info = el(0x1549A966, uint_el(0x2AD7B1, scale)), trk = el(0x1654AE6B, entries);
  Bytes points;
  for (auto [time, track] : cues) put(points, el(0xBB, cat({uint_el(0xB3, time), el(0xB7, cat({uint_el(0xF7, track), uint_el(0xF1, 1234)}))})));
  Bytes cue = el(0x1C53BB6B, points);
  Bytes cluster = el(0x1F43B675, Bytes(300, 0xAA));
  Bytes body;
  if (cues_at_end) {
    // SeekHead first; its Cues position (8 bytes wide) is relative to the Segment's data
    Bytes rest = cat({info, trk, cluster});
    auto seekhead = [&](uint64_t position) {
      Bytes pos; for (int i = 7; i >= 0; --i) pos.push_back((uint8_t)(position >> (i * 8)));
      return el(0x114D9B74, el(0x4DBB, cat({el(0x53AB, {0x1C, 0x53, 0xBB, 0x6B}), el(0x53AC, pos)})));
    };
    Bytes seek = seekhead(0);
    seek = seekhead(seek.size() + rest.size());
    body = cat({seek, rest, cue});
  } else body = cat({info, trk, cue, cluster});
  return cat({el(0x1A45DFA3, el(0x4282, {'w', 'e', 'b', 'm'})), el(0x18538067, body)});
}

// ── an MP4 file ──────────────────────────────────────────────────────
static Bytes u32(uint32_t v) { return {(uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v}; }
static Bytes box(const char* type, const Bytes& data) { Bytes b = u32((uint32_t)data.size() + 8); b.insert(b.end(), type, type + 4); put(b, data); return b; }
static Bytes full(const char* type, uint8_t version, const Bytes& data) { Bytes d{version, 0, 0, 0}; put(d, data); return box(type, d); }
// one video track: timescale, stts runs {count, delta}, ctts runs {count, offset} (none: empty), sync samples
static Bytes trak(const char* handler, uint32_t timescale, std::vector<std::pair<uint32_t, uint32_t>> stts, std::vector<std::pair<uint32_t, int32_t>> ctts,
                  std::vector<uint32_t> sync, bool signed_ctts = false) {
  Bytes hd = u32(0); hd.insert(hd.end(), handler, handler + 4); put(hd, Bytes(13, 0));  // pre_defined, handler_type, reserved, name
  Bytes md = cat({u32(0), u32(0), u32(timescale), u32(0), u32(0)});
  Bytes st = u32((uint32_t)stts.size()); for (auto [c, d] : stts) put(st, cat({u32(c), u32(d)}));
  Bytes ss = u32((uint32_t)sync.size()); for (auto s : sync) put(ss, u32(s));
  Bytes table = cat({full("stts", 0, st), full("stss", 0, ss)});
  if (!ctts.empty()) { Bytes ct = u32((uint32_t)ctts.size()); for (auto [c, o] : ctts) put(ct, cat({u32(c), u32((uint32_t)o)})); put(table, full("ctts", signed_ctts ? 1 : 0, ct)); }
  return box("trak", box("mdia", cat({full("mdhd", 0, md), full("hdlr", 0, hd), box("minf", box("stbl", table))})));
}
static Bytes mp4(const Bytes& moov_children, bool moov_last) {
  Bytes ftyp = box("ftyp", {'i', 's', 'o', 'm', 0, 0, 2, 0}), mdat = box("mdat", Bytes(500, 0x55)), moov = box("moov", moov_children);
  return moov_last ? cat({ftyp, mdat, moov}) : cat({ftyp, moov, mdat});
}

static int reads_out_of_bounds = 0;
static std::vector<double> index_of(const Bytes& f) {
  auto read = [&](uint64_t at, size_t n, void* out) {
    if (at > f.size() || n > f.size() - at) { ++reads_out_of_bounds; return false; }
    if (n) std::memcpy(out, f.data() + at, n);
    return true;
  };
  return keyframes::read_index(read, f.size());
}
static bool near(double a, double b) { return std::fabs(a - b) < 1e-6; }

int main() {
  // Matroska: the video track's cue points only, scaled by the timestamp scale; Cues found
  // through the SeekHead after the clusters, or before them
  for (bool end : {false, true}) {
    auto k = index_of(mkv(1000000, {{1, 2}, {2, 1}}, {{0, 2}, {0, 1}, {4000, 2}, {10417, 2}, {5000, 1}, {20833, 2}}, end));
    assert(k.size() == 4 && near(k[0], 0) && near(k[1], 4) && near(k[2], 10.417) && near(k[3], 20.833));
  }
  {
    auto k = index_of(mkv(100000, {{3, 1}}, {{0, 3}, {125000, 3}}, true));  // 0.1 ms units
    assert(k.size() == 2 && near(k[1], 12.5));
  }
  assert(index_of(mkv(1000000, {{1, 2}}, {{0, 1}, {4000, 1}}, false)).empty());  // no video track
  assert(index_of(mkv(1000000, {{1, 1}}, {}, false)).empty());                  // no cue points

  // MP4: decode times from stts, presentation offsets from ctts (unsigned and signed), the
  // sync samples' times; a sound track before the video track is skipped; moov first or last
  {
    // 24 fps (timescale 24000, 1000 a frame), keyframes at samples 1, 49, 97; B-frames: +2000
    Bytes video = trak("vide", 24000, {{120, 1000}}, {{120, 2000}}, {1, 49, 97});
    Bytes sound = trak("soun", 48000, {{10, 1024}}, {}, {1, 2, 3});
    for (bool last : {false, true}) {
      auto k = index_of(mp4(cat({sound, video}), last));
      assert(k.size() == 3 && near(k[0], 2000.0 / 24000) && near(k[1], (48000.0 + 2000) / 24000) && near(k[2], (96000.0 + 2000) / 24000));
    }
    // several stts and ctts runs; signed offsets (version 1)
    Bytes v2 = trak("vide", 1000, {{2, 40}, {3, 50}, {5, 40}}, {{1, 80}, {2, -40}, {7, 0}}, {1, 3, 6}, true);
    auto k = index_of(mp4(v2, false));
    // sample 1: dts 0 + 80; sample 3: dts 80 - 40; sample 6: dts 230 + 0
    assert(k.size() == 3 && near(k[0], 0.04) && near(k[1], 0.08) && near(k[2], 0.23));
    // no stss (every frame a keyframe), falling sync samples, sync past the table: nothing
    assert(index_of(mp4(trak("vide", 1000, {{4, 40}}, {}, {}), false)).empty());
    assert(index_of(mp4(trak("vide", 1000, {{4, 40}}, {}, {3, 2}), false)).empty());
    assert(index_of(mp4(trak("vide", 1000, {{4, 40}}, {}, {1, 9}), false)).empty());
  }

  // Damaged files: every truncation and random corruption of these files gives an index or
  // nothing, never a read outside the file
  {
    std::vector<Bytes> files = {mkv(1000000, {{1, 1}}, {{0, 1}, {4000, 1}, {8000, 1}}, true),
                                mp4(trak("vide", 24000, {{120, 1000}}, {{120, 2000}}, {1, 49, 97}), true)};
    std::mt19937 rng(7);
    for (const auto& f : files) {
      for (size_t n = 0; n < f.size(); ++n) { Bytes cut(f.begin(), f.begin() + n); (void)index_of(cut); }
      for (int i = 0; i < 20000; ++i) {
        Bytes g = f;
        for (int j = 0, flips = 1 + (int)(rng() % 6); j < flips; ++j) g[rng() % g.size()] = (uint8_t)rng();
        auto k = index_of(g);
        for (size_t j = 1; j < k.size(); ++j) assert(k[j] > k[j - 1]);
      }
    }
    // sizes that claim more than the file holds
    Bytes huge = mp4(trak("vide", 1000, {{4, 40}}, {}, {1}), false);
    huge[8 + 8 + 0] = 0x7F;  // the moov's size
    assert(index_of(huge).empty());
    for (int i = 0; i < 5000; ++i) { Bytes r(16 + rng() % 400); for (auto& c : r) c = (uint8_t)rng(); r[0] = 0x1A; r[1] = 0x45; r[2] = 0xDF; r[3] = 0xA3; (void)index_of(r); }
    assert(reads_out_of_bounds == 0);  // the parsers never even ask past the end
  }

  // What the engine answered: [a, t] lands on a; nothing beyond t is assumed
  {
    keyframes::KeyframeMap m;
    assert(m.lookup(5) < 0);
    m.learned(36, 31.25);
    assert(near(m.lookup(31.25), 31.25) && near(m.lookup(33), 31.25) && near(m.lookup(36), 31.25));
    assert(m.lookup(37) < 0 && m.lookup(31) < 0);
    m.learned(65, 62.5); m.learned(62, 52.083);
    assert(near(m.lookup(60), 52.083) && near(m.lookup(64), 62.5) && m.lookup(66) < 0);
    m.learned(40, 31.25);  // the span widens
    assert(near(m.lookup(39), 31.25) && m.lookup(41) < 0);
    m.learned(0, 0.4);     // an answer after the time asked (before the first keyframe): no span
    assert(m.lookup(0) < 0);
  }
  // The file's index: used once two decoded answers agree, with the engine's shift
  {
    keyframes::KeyframeMap m;
    m.set_index({0.083, 10.458, 20.875, 31.292, 41.708});  // an MP4: ctts puts every time 0.083 later
    assert(m.lookup(15) < 0);                               // not yet
    m.learned(12, 10.375);                                   // the engine: 0.083 earlier
    assert(!m.index_trusted() && m.lookup(15) < 0);
    m.learned(25, 20.792);
    assert(m.index_trusted());
    assert(near(m.lookup(15), 10.375) && near(m.lookup(41.7), 41.625) && near(m.lookup(1), 0) && near(m.lookup(100), 41.625));
    // a disagreement drops it for the file; what the engine said stays
    m.learned(35, 33.0);
    assert(!m.index_trusted() && m.index_dropped() && m.lookup(15) < 0);
    assert(near(m.lookup(34), 33.0) && near(m.lookup(24), 20.792));
  }
  {
    keyframes::KeyframeMap m;
    m.set_index({0, 10, 20});
    m.learned(15, 12);  // no indexed keyframe near: not this file's index
    assert(m.index_dropped());
    keyframes::KeyframeMap dense;
    dense.set_index({0, 0.04, 0.08, 0.12});  // intra-only: nothing to gain
    dense.learned(0.1, 0.08); dense.learned(0.05, 0.04);
    assert(!dense.index_trusted());
    keyframes::KeyframeMap again;
    again.set_index({0, 10, 20});
    again.learned(0.0, 0.0); again.learned(5, 0.0);
    assert(again.index_trusted() && near(again.lookup(19.9), 10));
    again.reset();
    assert(again.lookup(5) < 0 && !again.index_trusted());
  }
  std::printf("Keyframe index: Matroska Cues, MP4 sync samples (stts, ctts, signed, several runs), the video track only, damaged files bounded, spans from the engine's answers, the index trusted after two agreeing answers and dropped at the first disagreement passed\n");
  return 0;
}
