// A film opened with Cue closed: the window is made in the film's shape from its file's header
// (src/core/media_header.h, used before the window is first shown) instead of opening in the
// home's 16:9 and gliding a moment later, with the home drawn in the meantime. Checks:
// - Matroska: the first video track's size; its display size for the shape (anamorphic); crop;
//   a sound track before it skipped;
// - MP4: the video track's header size; the matrix of a phone video (90, 180, 270 degrees); no
//   size in the header: the sample description's, with its pixel shape (pasp);
// - files that are neither, damaged or truncated: no shape, no read past the file.
// With the real engine and real files (ffmpeg: plain, 2.39:1, anamorphic in MKV and MP4, a phone
// video turned in its matrix, 9:16, WebM), the shapes agree with ffprobe's display aspect, and
// in the real Cue under Wine the window opens in them (VALIDATION.md).
#include "../src/core/media_header.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

using Bytes = std::vector<uint8_t>;
static void put(Bytes& b, const Bytes& x) { b.insert(b.end(), x.begin(), x.end()); }
static Bytes cat(std::initializer_list<Bytes> parts) { Bytes b; for (auto& p : parts) put(b, p); return b; }
// ── Matroska ──
static Bytes id_bytes(uint32_t id) {
  const int n = id > 0xFFFFFF ? 4 : id > 0xFFFF ? 3 : id > 0xFF ? 2 : 1;
  Bytes b; for (int i = n - 1; i >= 0; --i) b.push_back((uint8_t)(id >> (i * 8))); return b;
}
static Bytes el(uint32_t id, const Bytes& d) { Bytes b = id_bytes(id); b.push_back(0x01); for (int s = 48; s >= 0; s -= 8) b.push_back((uint8_t)((uint64_t)d.size() >> s)); put(b, d); return b; }
static Bytes u(uint32_t id, uint64_t v) { Bytes d; for (int s = 56; s >= 0; s -= 8) if ((v >> s) || !d.empty() || s == 0) d.push_back((uint8_t)(v >> s)); return el(id, d); }
static Bytes mkv(const Bytes& tracks) {
  return cat({el(0x1A45DFA3, el(0x4282, {'w', 'e', 'b', 'm'})), el(0x18538067, cat({el(0x1549A966, u(0x2AD7B1, 1000000)), el(0x1654AE6B, tracks), el(0x1F43B675, Bytes(64, 0))}))});
}
static Bytes video_track(int n, std::initializer_list<Bytes> video) { Bytes v; for (auto& x : video) put(v, x); return el(0xAE, cat({u(0xD7, n), u(0x83, 1), el(0xE0, v)})); }
static Bytes audio_track(int n) { return el(0xAE, cat({u(0xD7, n), u(0x83, 2)})); }
// ── MP4 ──
static Bytes be32(uint32_t v) { return {(uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v}; }
static Bytes box(const char* t, const Bytes& d) { Bytes b = be32((uint32_t)d.size() + 8); b.insert(b.end(), t, t + 4); put(b, d); return b; }
static Bytes full(const char* t, const Bytes& d) { Bytes x{0, 0, 0, 0}; put(x, d); return box(t, x); }
static Bytes tkhd(int32_t a, int32_t b, int32_t c, int32_t d, uint32_t w, uint32_t h) {
  Bytes x = cat({be32(0), be32(0), be32(1), be32(0), be32(0), Bytes(8, 0), Bytes(8, 0)});
  for (int32_t m : {a, b, 0, c, d, 0, 0, 0, 0x40000000}) put(x, be32((uint32_t)m));
  put(x, be32(w << 16)); put(x, be32(h << 16));
  return full("tkhd", x);
}
static Bytes trak(const char* handler, const Bytes& header, const Bytes& stsd_entry = {}) {
  Bytes hd = be32(0); hd.insert(hd.end(), handler, handler + 4); put(hd, Bytes(13, 0));
  Bytes stbl = stsd_entry.empty() ? box("stbl", {}) : box("stbl", full("stsd", cat({be32(1), stsd_entry})));
  return box("trak", cat({header, box("mdia", cat({full("hdlr", hd), box("minf", stbl)}))}));
}
static Bytes avc1(uint16_t w, uint16_t h, uint32_t hs = 0, uint32_t vs = 0) {
  Bytes e = cat({Bytes(6, 0), {0, 1}, Bytes(16, 0), {(uint8_t)(w >> 8), (uint8_t)w, (uint8_t)(h >> 8), (uint8_t)h}, Bytes(50, 0)});
  if (hs) put(e, box("pasp", cat({be32(hs), be32(vs)})));
  return box("avc1", e);
}
static Bytes mp4(const Bytes& traks) { return cat({box("ftyp", {'i', 's', 'o', 'm', 0, 0, 2, 0}), box("moov", traks), box("mdat", Bytes(100, 0))}); }

static int outside = 0;
static VideoShape shape_of(const Bytes& f) {
  auto read = [&](uint64_t at, size_t n, void* out) {
    if (at > f.size() || n > f.size() - at) { ++outside; return false; }
    if (n) std::memcpy(out, f.data() + at, n);
    return true;
  };
  return media_header::shape(read, f.size());
}
static bool is(VideoShape s, int64_t w, int64_t h, int64_t r) { return s.width == w && s.height == h && s.rotation == r; }

int main() {
  // Matroska: the pixel size; the display size gives the shape; crop; the video track after a sound one
  assert(is(shape_of(mkv(video_track(1, {u(0xB0, 1920), u(0xBA, 804)}))), 1920, 804, 0));
  assert(std::fabs(shape_of(mkv(video_track(1, {u(0xB0, 1440), u(0xBA, 1080), u(0x54B0, 1920), u(0x54BA, 1080)}))).aspect() - 16.0 / 9) < 1e-3);
  assert(std::fabs(shape_of(mkv(video_track(1, {u(0xB0, 720), u(0xBA, 576), u(0x54B0, 16), u(0x54BA, 9), u(0x54B2, 3)}))).aspect() - 16.0 / 9) < 1e-2);
  assert(is(shape_of(mkv(video_track(1, {u(0xB0, 1920), u(0xBA, 1080), u(0x54BB, 138), u(0x54AA, 138)}))), 1920, 804, 0));
  assert(is(shape_of(mkv(cat({audio_track(1), video_track(2, {u(0xB0, 1280), u(0xBA, 720)})}))), 1280, 720, 0));
  assert(!shape_of(mkv(audio_track(1))).valid());
  assert(!shape_of(mkv(video_track(1, {u(0xB0, 0), u(0xBA, 720)}))).valid());
  // MP4: the header's size and the turn in its matrix; a sound track first
  const int32_t one = 1 << 16;
  assert(is(shape_of(mp4(trak("vide", tkhd(one, 0, 0, one, 1920, 1080)))), 1920, 1080, 0));
  VideoShape phone = shape_of(mp4(cat({trak("soun", tkhd(one, 0, 0, one, 0, 0)), trak("vide", tkhd(0, one, -one, 0, 1920, 1080))})));
  assert(is(phone, 1920, 1080, 90) && std::fabs(phone.aspect() - 9.0 / 16) < 1e-3);
  assert(is(shape_of(mp4(trak("vide", tkhd(0, -one, one, 0, 1920, 1080)))), 1920, 1080, 270));
  assert(is(shape_of(mp4(trak("vide", tkhd(-one, 0, 0, -one, 1920, 1080)))), 1920, 1080, 180));
  // no size in the header: the sample description's, widened by its pixel shape
  assert(is(shape_of(mp4(trak("vide", tkhd(one, 0, 0, one, 0, 0), avc1(1280, 720)))), 1280, 720, 0));
  assert(is(shape_of(mp4(trak("vide", tkhd(one, 0, 0, one, 0, 0), avc1(1440, 1080, 4, 3)))), 1920, 1080, 0));
  assert(!shape_of(mp4(trak("soun", tkhd(one, 0, 0, one, 0, 0)))).valid());
  // not a film container, damaged, truncated: nothing, and never a read past the file
  assert(!shape_of(Bytes(64, 0)).valid() && !shape_of({'R', 'I', 'F', 'F', 0, 0, 0, 0, 'A', 'V', 'I', ' ', 0, 0, 0, 0}).valid());
  std::mt19937 rng(11);
  for (const Bytes& f : {mkv(video_track(1, {u(0xB0, 1920), u(0xBA, 1080), u(0x54B0, 16), u(0x54BA, 9)})),
                         mp4(cat({trak("soun", tkhd(one, 0, 0, one, 0, 0)), trak("vide", tkhd(one, 0, 0, one, 0, 0), avc1(1440, 1080, 4, 3))}))}) {
    for (size_t n = 0; n < f.size(); ++n) (void)shape_of(Bytes(f.begin(), f.begin() + n));
    for (int i = 0; i < 20000; ++i) {
      Bytes g = f;
      for (int j = 0, k = 1 + (int)(rng() % 6); j < k; ++j) g[rng() % g.size()] = (uint8_t)rng();
      VideoShape s = shape_of(g);
      assert(!s.valid() || (s.aspect() > 0.1 && s.aspect() < 10));
    }
  }
  assert(outside == 0);
  std::printf("Media header: Matroska size, display shape, crop and track; MP4 size, matrix turns, sample description and pasp; damaged and truncated files bounded passed\n");
  return 0;
}
