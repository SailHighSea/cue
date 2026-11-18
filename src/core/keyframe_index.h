// Where a film's keyframes are, read from its own index, and which keyframe a timeline preview
// lands on. The preview helper seeks to keyframes ("absolute+keyframes": the last keyframe at or
// before the time asked), so every second of the timeline between two keyframes shows the same
// picture. Knowing the keyframes, the helper answers those seconds with the picture it already
// decoded instead of decoding it again (on a 4K film with a keyframe every 10 s, sweeping 30 s
// of the timeline decoded the same picture 30 times, about 150 ms each).
//
// Two sources, both only ever used to reuse a picture the engine itself returned:
// - the file's index: Matroska/WebM Cues (the points the engine seeks to) or an MP4/MOV video
//   track's sync samples. It is trusted only after the engine has agreed with it (see
//   KeyframeMap), and dropped for the file at the first disagreement;
// - what the engine answered: a request at t that returned the keyframe a proves there is no
//   keyframe in (a, t], so any time in [a, t] lands on a too (no index needed).
// The parsers read untrusted files: every size is bounded and checked before use; anything
// unexpected gives an empty index (the helper then decodes as before).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace keyframes {

// read(offset, length, out) reads exactly length bytes, or fails
using ReadFn = std::function<bool(uint64_t, size_t, void*)>;
constexpr size_t kMaxEntries = 400000;          // a keyframe every 50 ms for 5.5 hours
constexpr uint64_t kMaxIndexBytes = 48u << 20;  // a Cues element or an MP4 moov box

namespace detail {

inline uint64_t be(const uint8_t* p, int n) { uint64_t v = 0; for (int i = 0; i < n; ++i) v = v << 8 | p[i]; return v; }

// ── Matroska / WebM ──────────────────────────────────────────────────
// An EBML element: its ID (with the length marker kept, as the specification writes IDs), the
// size of its data (~0 when unknown), and where its data starts.
struct Ebml { uint32_t id = 0; uint64_t size = 0, data = 0; };

inline bool ebml_vint(const ReadFn& read, uint64_t at, uint64_t end, bool keep_marker, uint64_t& value, int& length) {
  if (at >= end) return false;
  uint8_t b[8];
  if (!read(at, 1, b)) return false;
  length = 1;
  while (length <= 8 && !(b[0] & (0x80 >> (length - 1)))) ++length;
  if (length > 8 || at + length > end || (length > 1 && !read(at + 1, length - 1, b + 1))) return false;
  value = keep_marker ? b[0] : (b[0] & (0xFF >> length));
  bool all_ones = (b[0] & (0xFF >> length)) == (0xFF >> length);
  for (int i = 1; i < length; ++i) { value = value << 8 | b[i]; all_ones = all_ones && b[i] == 0xFF; }
  if (!keep_marker && all_ones) value = ~0ull;  // unknown size
  return true;
}

inline bool ebml_element(const ReadFn& read, uint64_t at, uint64_t end, Ebml& e) {
  uint64_t id = 0, size = 0; int a = 0, b = 0;
  if (!ebml_vint(read, at, end, true, id, a) || a > 4 || !ebml_vint(read, at + a, end, false, size, b)) return false;
  e.id = (uint32_t)id; e.size = size; e.data = at + a + b;
  if (size != ~0ull && (size > end || e.data > end - size)) return false;
  return true;
}

// The children of a parent's data, read whole into memory (bounded by the caller).
struct Buffer {
  std::vector<uint8_t> b;
  bool vint(size_t& at, bool keep_marker, uint64_t& v) const {
    if (at >= b.size()) return false;
    int n = 1; while (n <= 8 && !(b[at] & (0x80 >> (n - 1)))) ++n;
    if (n > 8 || at + n > b.size()) return false;
    v = keep_marker ? b[at] : (b[at] & (0xFF >> n));
    for (int i = 1; i < n; ++i) v = v << 8 | b[at + i];
    at += n; return true;
  }
  // next child in [at, end): id, data offset, data size
  bool child(size_t& at, size_t end, uint32_t& id, size_t& data, size_t& size) const {
    uint64_t i = 0, s = 0; size_t p = at;
    if (!vint(p, true, i) || i > 0xFFFFFFFFu || !vint(p, false, s) || p > end || s > end - p) return false;
    id = (uint32_t)i; data = p; size = (size_t)s; at = p + (size_t)s; return true;
  }
  uint64_t uint(size_t data, size_t size) const { return size <= 8 ? be(b.data() + data, (int)size) : 0; }
};

inline bool read_whole(const ReadFn& read, uint64_t at, uint64_t size, Buffer& out) {
  if (size > kMaxIndexBytes) return false;
  out.b.resize((size_t)size);
  return size == 0 || read(at, (size_t)size, out.b.data());
}

inline std::vector<double> mkv(const ReadFn& read, uint64_t file_size) {
  Ebml head;
  if (!ebml_element(read, 0, file_size, head) || head.id != 0x1A45DFA3 || head.size == ~0ull) return {};
  Ebml seg;
  uint64_t at = head.data + head.size;
  // (a Void or CRC element may sit between the header and the Segment)
  for (int guard = 0; guard < 8; ++guard) {
    if (!ebml_element(read, at, file_size, seg)) return {};
    if (seg.id == 0x18538067) break;
    if (seg.size == ~0ull) return {};
    at = seg.data + seg.size;
  }
  if (seg.id != 0x18538067) return {};
  const uint64_t seg_start = seg.data, seg_end = seg.size == ~0ull ? file_size : seg.data + seg.size;
  uint64_t scale = 1000000, cues_at = 0;
  int64_t video = -1;
  auto info = [&](const Buffer& b) {
    size_t p = 0; uint32_t id; size_t d, s;
    while (p < b.b.size() && b.child(p, b.b.size(), id, d, s)) if (id == 0x2AD7B1 && s >= 1 && s <= 8) scale = b.uint(d, s);
  };
  auto tracks = [&](const Buffer& b) {
    size_t p = 0; uint32_t id; size_t d, s;
    while (video < 0 && p < b.b.size() && b.child(p, b.b.size(), id, d, s)) {
      if (id != 0xAE) continue;
      size_t q = d; uint32_t cid; size_t cd, cs; int64_t number = -1, type = -1;
      while (q < d + s && b.child(q, d + s, cid, cd, cs)) {
        if (cid == 0xD7 && cs >= 1 && cs <= 8) number = (int64_t)b.uint(cd, cs);
        else if (cid == 0x83 && cs >= 1 && cs <= 8) type = (int64_t)b.uint(cd, cs);
      }
      if (type == 1 && number > 0) video = number;
    }
  };
  auto seekhead = [&](const Buffer& b) {
    size_t p = 0; uint32_t id; size_t d, s;
    while (p < b.b.size() && b.child(p, b.b.size(), id, d, s)) {
      if (id != 0x4DBB) continue;
      size_t q = d; uint32_t cid; size_t cd, cs; uint64_t target = 0, position = ~0ull;
      while (q < d + s && b.child(q, d + s, cid, cd, cs)) {
        if (cid == 0x53AB && cs >= 1 && cs <= 4) target = b.uint(cd, cs);
        else if (cid == 0x53AC && cs >= 1 && cs <= 8) position = b.uint(cd, cs);
      }
      if (target == 0x1C53BB6B && position != ~0ull && position < seg_end - seg_start) cues_at = seg_start + position;
    }
  };
  // the Segment's first elements, up to its first Cluster: SeekHead, Info, Tracks (and Cues, if there)
  at = seg_start;
  Buffer b;
  for (int guard = 0; guard < 64 && at < seg_end; ++guard) {
    Ebml e;
    if (!ebml_element(read, at, seg_end, e) || e.size == ~0ull) break;
    if (e.id == 0x1F43B675) break;  // a Cluster: the media starts
    if (e.id == 0x114D9B74 || e.id == 0x1549A966 || e.id == 0x1654AE6B) {
      if (!read_whole(read, e.data, e.size, b)) return {};
      if (e.id == 0x114D9B74) seekhead(b); else if (e.id == 0x1549A966) info(b); else tracks(b);
    } else if (e.id == 0x1C53BB6B) cues_at = at;
    at = e.data + e.size;
  }
  if (video < 0 || !cues_at || scale == 0 || scale > 1000000000ull) return {};
  Ebml cues;
  if (!ebml_element(read, cues_at, seg_end, cues) || cues.id != 0x1C53BB6B || cues.size == ~0ull || !read_whole(read, cues.data, cues.size, b)) return {};
  std::vector<double> out;
  size_t p = 0; uint32_t id; size_t d, s;
  while (p < b.b.size() && b.child(p, b.b.size(), id, d, s)) {
    if (id != 0xBB) continue;
    size_t q = d; uint32_t cid; size_t cd, cs; uint64_t time = ~0ull; bool ours = false;
    while (q < d + s && b.child(q, d + s, cid, cd, cs)) {
      if (cid == 0xB3 && cs >= 1 && cs <= 8) time = b.uint(cd, cs);
      else if (cid == 0xB7) {
        size_t r = cd; uint32_t tid; size_t td, ts;
        while (r < cd + cs && b.child(r, cd + cs, tid, td, ts))
          if (tid == 0xF7 && ts >= 1 && ts <= 8 && (int64_t)b.uint(td, ts) == video) ours = true;
      }
    }
    if (ours && time != ~0ull) {
      if (out.size() >= kMaxEntries) return {};
      out.push_back((double)time * (double)scale / 1e9);
    }
  }
  return out;
}

// ── MP4 / MOV ────────────────────────────────────────────────────────
struct Box { uint32_t type = 0; uint64_t data = 0, size = 0; };
inline uint32_t fourcc(const char* c) { return (uint32_t)(uint8_t)c[0] << 24 | (uint32_t)(uint8_t)c[1] << 16 | (uint32_t)(uint8_t)c[2] << 8 | (uint8_t)c[3]; }

// boxes inside a buffer
inline bool box_in(const std::vector<uint8_t>& b, size_t& at, size_t end, Box& x) {
  if (at + 8 > end) return false;
  uint64_t size = be(&b[at], 4); size_t head = 8;
  x.type = (uint32_t)be(&b[at + 4], 4);
  if (size == 1) { if (at + 16 > end) return false; size = be(&b[at + 8], 8); head = 16; }
  else if (size == 0) size = end - at;
  if (size < head || size > end - at) return false;
  x.data = at + head; x.size = size - head; at += (size_t)size; return true;
}
inline bool find_in(const std::vector<uint8_t>& b, size_t from, size_t end, uint32_t type, Box& x) {
  size_t at = from;
  while (box_in(b, at, end, x)) if (x.type == type) return true;
  return false;
}

inline std::vector<double> mp4(const ReadFn& read, uint64_t file_size) {
  // the moov box, wherever it is among the top-level boxes
  uint64_t at = 0, moov_at = 0, moov_size = 0;
  for (int guard = 0; guard < 4096 && at + 8 <= file_size; ++guard) {
    uint8_t h[16];
    if (!read(at, 8, h)) return {};
    uint64_t size = be(h, 4); uint64_t head = 8;
    if (size == 1) { if (at + 16 > file_size || !read(at + 8, 8, h + 8)) return {}; size = be(h + 8, 8); head = 16; }
    else if (size == 0) size = file_size - at;
    if (size < head || size > file_size - at) return {};
    if (be(h + 4, 4) == fourcc("moov")) { moov_at = at + head; moov_size = size - head; break; }
    at += size;
  }
  if (!moov_size || moov_size > kMaxIndexBytes) return {};
  std::vector<uint8_t> b((size_t)moov_size);
  if (!read(moov_at, b.size(), b.data())) return {};
  size_t p = 0; Box trak;
  while (box_in(b, p, b.size(), trak)) {
    if (trak.type != fourcc("trak")) continue;
    const size_t t0 = (size_t)trak.data, t1 = (size_t)(trak.data + trak.size);
    Box mdia, hdlr, mdhd, minf, stbl, stss, stts, ctts;
    if (!find_in(b, t0, t1, fourcc("mdia"), mdia)) continue;
    const size_t m0 = (size_t)mdia.data, m1 = (size_t)(mdia.data + mdia.size);
    if (!find_in(b, m0, m1, fourcc("hdlr"), hdlr) || hdlr.size < 12 || be(&b[(size_t)hdlr.data + 8], 4) != fourcc("vide")) continue;
    if (!find_in(b, m0, m1, fourcc("mdhd"), mdhd) || mdhd.size < 24) return {};
    const size_t md = (size_t)mdhd.data;
    const uint64_t timescale = b[md] == 1 ? (mdhd.size >= 32 ? be(&b[md + 20], 4) : 0) : be(&b[md + 12], 4);
    if (!timescale) return {};
    if (!find_in(b, m0, m1, fourcc("minf"), minf) || !find_in(b, (size_t)minf.data, (size_t)(minf.data + minf.size), fourcc("stbl"), stbl)) return {};
    const size_t s0 = (size_t)stbl.data, s1 = (size_t)(stbl.data + stbl.size);
    // no stss: every sample is a keyframe (intra-only video): nothing to gain, nothing to keep
    if (!find_in(b, s0, s1, fourcc("stss"), stss) || !find_in(b, s0, s1, fourcc("stts"), stts) || stss.size < 8 || stts.size < 8) return {};
    const bool has_ctts = find_in(b, s0, s1, fourcc("ctts"), ctts) && ctts.size >= 8;
    const uint64_t n_sync = be(&b[(size_t)stss.data + 4], 4), n_stts = be(&b[(size_t)stts.data + 4], 4);
    if (n_sync == 0 || n_sync > kMaxEntries || 8 + n_sync * 4 > stss.size || 8 + n_stts * 8 > stts.size) return {};
    const uint64_t n_ctts = has_ctts ? be(&b[(size_t)ctts.data + 4], 4) : 0;
    if (has_ctts && 8 + n_ctts * 8 > ctts.size) return {};
    const bool ctts_signed = has_ctts && b[(size_t)ctts.data] == 1;
    // walk the samples once: decode time from stts, its offset from ctts, keep the sync ones
    std::vector<double> out; out.reserve((size_t)n_sync);
    uint64_t sample = 1, dts = 0, si = 0, sn = 0, ci = 0, cn = 0; int64_t coff = 0;
    auto stts_at = [&](uint64_t i, uint64_t& count, uint64_t& delta) { const size_t q = (size_t)stts.data + 8 + (size_t)i * 8; count = be(&b[q], 4); delta = be(&b[q + 4], 4); };
    auto ctts_at = [&](uint64_t i, uint64_t& count, int64_t& off) {
      const size_t q = (size_t)ctts.data + 8 + (size_t)i * 8; count = be(&b[q], 4); const uint32_t raw = (uint32_t)be(&b[q + 4], 4);
      off = ctts_signed ? (int64_t)(int32_t)raw : (int64_t)raw;
    };
    uint64_t run = 0, delta = 0, crun = 0;
    if (n_stts) stts_at(0, run, delta);
    if (n_ctts) ctts_at(0, crun, coff);
    for (uint64_t k = 0; k < n_sync; ++k) {
      const uint64_t want = be(&b[(size_t)stss.data + 8 + (size_t)k * 4], 4);
      if (want < sample) return {};  // sync samples must rise
      while (sample < want) {        // advance to the wanted sample
        uint64_t step = want - sample;
        while (sn >= run) { if (++si >= n_stts) return {}; stts_at(si, run, delta); sn = 0; }
        const uint64_t take = std::min(step, run - sn);
        if (delta && take > (~0ull - dts) / delta) return {};
        dts += take * delta; sn += take;
        if (has_ctts) {
          uint64_t left = take;
          while (left) { while (cn >= crun) { if (++ci >= n_ctts) return {}; ctts_at(ci, crun, coff); cn = 0; } const uint64_t t = std::min(left, crun - cn); cn += t; left -= t; }
        }
        sample += take;
      }
      while (has_ctts && cn >= crun) { if (++ci >= n_ctts) return {}; ctts_at(ci, crun, coff); cn = 0; }
      out.push_back(((double)dts + (double)(has_ctts ? coff : 0)) / (double)timescale);
    }
    return out;  // the first video track: the one the engine plays
  }
  return {};
}

}  // namespace detail

// The keyframe times (seconds, sorted, unique) of a file's first video track, from its own
// index; empty when the format is not one of these, has no index, or anything is out of bounds.
inline std::vector<double> read_index(const ReadFn& read, uint64_t file_size) {
  std::vector<double> k;
  uint8_t magic[8];
  if (file_size < 16 || !read(0, 8, magic)) return k;
  try {
    if (detail::be(magic, 4) == 0x1A45DFA3) k = detail::mkv(read, file_size);
    else if (detail::be(magic + 4, 4) == detail::fourcc("ftyp") || detail::be(magic + 4, 4) == detail::fourcc("moov") ||
             detail::be(magic + 4, 4) == detail::fourcc("free") || detail::be(magic + 4, 4) == detail::fourcc("wide") ||
             detail::be(magic + 4, 4) == detail::fourcc("mdat")) k = detail::mp4(read, file_size);
  } catch (...) { k.clear(); }
  for (double t : k) if (!std::isfinite(t) || t < -86400 || t > 1e7) return {};
  std::sort(k.begin(), k.end());
  k.erase(std::unique(k.begin(), k.end(), [](double a, double b) { return std::fabs(a - b) < 1e-6; }), k.end());
  return k;
}

// Which keyframe a navigation preview at time t lands on, when that is known for sure.
class KeyframeMap {
 public:
  static constexpr double kTolerance = 0.003;  // seconds: the engine's time against the index's
  static constexpr int kConfirm = 2;           // decoded answers that must agree before trusting the index
  void reset() { *this = KeyframeMap(); }
  // the file's own index (empty: none). Too dense to matter (an intra-only film) is ignored.
  void set_index(std::vector<double> k) {
    index_.clear(); state_ = Off;
    if (k.size() < 2) return;
    double gap = 1e9;
    for (size_t i = 1; i < k.size(); ++i) gap = std::min(gap, k[i] - k[i - 1]);
    if (gap < 0.25) return;
    index_ = std::move(k); state_ = Calibrating; agreed_ = 0;
  }
  bool index_trusted() const { return state_ == Trusted; }
  bool index_dropped() const { return state_ == Dropped; }
  // The keyframe (the engine's time for it) that a keyframe seek to t returns, or a negative
  // number when it is not known without asking the engine.
  double lookup(double t) const {
    if (!std::isfinite(t) || t < 0) return -1;
    // what the engine answered before: a request at r returned a, so [a, r] all land on a
    auto it = spans_.upper_bound(t + 1e-9);
    if (it != spans_.begin()) { --it; if (t >= it->first - 1e-9 && t <= it->second + 1e-9) return it->first; }
    if (state_ == Trusted) {
      auto k = std::upper_bound(index_.begin(), index_.end(), t - offset_ + 1e-9);
      if (k != index_.begin()) return *(k - 1) + offset_;
    }
    return -1;
  }
  // A decoded answer: a keyframe seek to t returned the picture at `actual`.
  void learned(double t, double actual) {
    if (!std::isfinite(t) || !std::isfinite(actual) || actual < 0) return;
    if (actual <= t + 1e-6) {
      auto& r = spans_[actual];
      r = std::max(r, t);
      // a later answer can only widen a span up to the next keyframe already seen
      auto here = spans_.find(actual), next = std::next(here);
      if (next != spans_.end() && r >= next->first) r = next->first - 1e-6;
      if (here != spans_.begin()) { auto before = std::prev(here); if (before->second >= actual) before->second = actual - 1e-6; }
      if (spans_.size() > kMaxEntries) spans_.erase(spans_.begin());
    }
    if (state_ == Calibrating || state_ == Trusted) check(t, actual);
  }
 private:
  enum State { Off, Calibrating, Trusted, Dropped };
  void check(double t, double actual) {
    // before the first keyframe the engine gives the first one, after t: says nothing either way
    if (actual > t + kTolerance) return;
    if (state_ == Calibrating && agreed_ == 0) {
      // the engine's times can be shifted from the file's (an MP4 edit list, a start time
      // moved to zero): the shift is the one that puts this answer on an indexed keyframe
      auto k = std::lower_bound(index_.begin(), index_.end(), actual);
      double best = 1e9;
      if (k != index_.end()) best = *k - actual;
      if (k != index_.begin() && std::fabs(*(k - 1) - actual) < std::fabs(best)) best = *(k - 1) - actual;
      if (std::fabs(best) > 0.5) { state_ = Dropped; index_.clear(); return; }
      offset_ = -best;
    }
    auto k = std::upper_bound(index_.begin(), index_.end(), t - offset_ + 1e-9);
    const bool agrees = k != index_.begin() && std::fabs(*(k - 1) + offset_ - actual) <= kTolerance;
    if (!agrees) { state_ = Dropped; index_.clear(); return; }
    if (state_ == Calibrating && ++agreed_ >= kConfirm) state_ = Trusted;
  }
  std::vector<double> index_;
  std::map<double, double> spans_;  // keyframe the engine returned -> the latest time that returned it
  State state_ = Off;
  int agreed_ = 0;
  double offset_ = 0;
};

}  // namespace keyframes
