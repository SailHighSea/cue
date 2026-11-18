// A film's shape from the first bytes of its file, before the engine has even started: a few
// small reads of a Matroska/WebM Tracks element or an MP4/MOV video track's header. Opening a
// film from Explorer with Cue closed, the window is then made in the film's shape from the start,
// instead of the home's 16:9 reshaped a moment later (a glide in front of the person). The
// engine's own measure still comes and corrects the window if the file said something else.
// Untrusted input: bounded like keyframe_index.h; anything unexpected gives no shape (the window
// then opens as before).
#pragma once
#include "keyframe_index.h"
#include "media_geometry.h"

namespace media_header {
namespace detail {
using namespace keyframes::detail;

inline VideoShape mkv(const keyframes::ReadFn& read, uint64_t file_size) {
  Ebml head;
  if (!ebml_element(read, 0, file_size, head) || head.id != 0x1A45DFA3 || head.size == ~0ull) return {};
  Ebml seg;
  uint64_t at = head.data + head.size;
  for (int guard = 0; guard < 8; ++guard) {
    if (!ebml_element(read, at, file_size, seg)) return {};
    if (seg.id == 0x18538067) break;
    if (seg.size == ~0ull) return {};
    at = seg.data + seg.size;
  }
  if (seg.id != 0x18538067) return {};
  const uint64_t seg_end = seg.size == ~0ull ? file_size : seg.data + seg.size;
  at = seg.data;
  Buffer b;
  for (int guard = 0; guard < 64 && at < seg_end; ++guard) {
    Ebml e;
    if (!ebml_element(read, at, seg_end, e) || e.size == ~0ull || e.id == 0x1F43B675) break;  // (a Cluster: the media)
    if (e.id == 0x1654AE6B) {
      if (e.size > (4u << 20) || !read_whole(read, e.data, e.size, b)) return {};
      size_t p = 0; uint32_t id; size_t d, s;
      while (p < b.b.size() && b.child(p, b.b.size(), id, d, s)) {
        if (id != 0xAE) continue;
        size_t q = d; uint32_t cid; size_t cd, cs; int64_t type = -1; size_t vd = 0, vs = 0;
        while (q < d + s && b.child(q, d + s, cid, cd, cs)) {
          if (cid == 0x83 && cs >= 1 && cs <= 8) type = (int64_t)b.uint(cd, cs);
          else if (cid == 0xE0) { vd = cd; vs = cs; }
        }
        if (type != 1 || !vs) continue;  // the first video track: the one the engine plays
        uint64_t pw = 0, ph = 0, dw = 0, dh = 0, unit = 0, crop[4] = {0, 0, 0, 0};
        size_t r = vd; uint32_t vid; size_t xd, xs;
        while (r < vd + vs && b.child(r, vd + vs, vid, xd, xs)) {
          if (xs < 1 || xs > 8) continue;
          const uint64_t v = b.uint(xd, xs);
          switch (vid) {
            case 0xB0: pw = v; break;
            case 0xBA: ph = v; break;
            case 0x54B0: dw = v; break;
            case 0x54BA: dh = v; break;
            case 0x54B2: unit = v; break;
            case 0x54AA: crop[0] = v; break;  // bottom
            case 0x54BB: crop[1] = v; break;  // top
            case 0x54CC: crop[2] = v; break;  // left
            case 0x54DD: crop[3] = v; break;  // right
          }
        }
        if (pw == 0 || ph == 0 || pw > 65536 || ph > 65536) return {};
        if (crop[2] + crop[3] < pw) pw -= crop[2] + crop[3];
        if (crop[0] + crop[1] < ph) ph -= crop[0] + crop[1];
        // the display size gives the shape (anamorphic films, an aspect ratio stored as such)
        if (dw > 0 && dh > 0 && dw <= 65536 && dh <= 65536 && (unit == 0 || unit == 3)) {
          const double ar = (double)dw / (double)dh;
          return {(int64_t)(ph * ar + 0.5), (int64_t)ph, 0};
        }
        return {(int64_t)pw, (int64_t)ph, 0};
      }
      return {};
    }
    at = e.data + e.size;
  }
  return {};
}

inline VideoShape mp4(const keyframes::ReadFn& read, uint64_t file_size) {
  uint64_t at = 0, moov_at = 0, moov_size = 0;
  for (int guard = 0; guard < 4096 && at + 8 <= file_size; ++guard) {
    uint8_t h[16];
    if (!read(at, 8, h)) return {};
    uint64_t size = be(h, 4), head = 8;
    if (size == 1) { if (at + 16 > file_size || !read(at + 8, 8, h + 8)) return {}; size = be(h + 8, 8); head = 16; }
    else if (size == 0) size = file_size - at;
    if (size < head || size > file_size - at) return {};
    if (be(h + 4, 4) == fourcc("moov")) { moov_at = at + head; moov_size = size - head; break; }
    at += size;
  }
  if (!moov_size || moov_size > keyframes::kMaxIndexBytes) return {};
  std::vector<uint8_t> b((size_t)moov_size);
  if (!read(moov_at, b.size(), b.data())) return {};
  size_t p = 0; Box trak;
  while (box_in(b, p, b.size(), trak)) {
    if (trak.type != fourcc("trak")) continue;
    const size_t t0 = (size_t)trak.data, t1 = (size_t)(trak.data + trak.size);
    Box mdia, hdlr, tkhd;
    if (!find_in(b, t0, t1, fourcc("mdia"), mdia)) continue;
    const size_t m0 = (size_t)mdia.data, m1 = (size_t)(mdia.data + mdia.size);
    if (!find_in(b, m0, m1, fourcc("hdlr"), hdlr) || hdlr.size < 12 || be(&b[(size_t)hdlr.data + 8], 4) != fourcc("vide")) continue;
    if (!find_in(b, t0, t1, fourcc("tkhd"), tkhd) || tkhd.size < 84) return {};
    const size_t k = (size_t)tkhd.data;
    const size_t matrix = k + (b[k] == 1 ? 52 : 40), size_at = matrix + 36;
    if (size_at + 8 > k + tkhd.size) return {};
    auto fixed = [&](size_t q) { return (int32_t)be(&b[q], 4); };
    int64_t w = (int64_t)((uint32_t)fixed(size_at) >> 16), h = (int64_t)((uint32_t)fixed(size_at + 4) >> 16);
    // the matrix turns a phone video: (0, 1, -1, 0) is 90 degrees, and so on
    const int32_t a = fixed(matrix), bb = fixed(matrix + 4), c = fixed(matrix + 12), d = fixed(matrix + 16);
    int64_t rotation = 0;
    const int32_t one = 1 << 16;
    if (a == 0 && d == 0 && bb == one && c == -one) rotation = 90;
    else if (a == -one && d == -one && bb == 0 && c == 0) rotation = 180;
    else if (a == 0 && d == 0 && bb == -one && c == one) rotation = 270;
    if (w <= 0 || h <= 0 || w > 65536 || h > 65536) {
      // no size in the header: the sample description's, with its pixel shape (pasp)
      Box minf, stbl, stsd;
      if (!find_in(b, m0, m1, fourcc("minf"), minf) || !find_in(b, (size_t)minf.data, (size_t)(minf.data + minf.size), fourcc("stbl"), stbl) ||
          !find_in(b, (size_t)stbl.data, (size_t)(stbl.data + stbl.size), fourcc("stsd"), stsd) || stsd.size < 8 + 8 + 28) return {};
      const size_t e = (size_t)stsd.data + 8;  // the first sample entry (its size and type, then the visual fields)
      const size_t entry_end = e + (size_t)std::min<uint64_t>(be(&b[e], 4), stsd.size - 8);
      if (entry_end < e + 8 + 78) return {};
      w = (int64_t)be(&b[e + 8 + 24], 2); h = (int64_t)be(&b[e + 8 + 26], 2);
      Box pasp;
      if (find_in(b, e + 8 + 78, entry_end, fourcc("pasp"), pasp) && pasp.size >= 8) {
        const uint64_t hs = be(&b[(size_t)pasp.data], 4), vs = be(&b[(size_t)pasp.data + 4], 4);
        if (hs > 0 && vs > 0 && hs < 1000 && vs < 1000) w = (int64_t)((double)w * hs / vs + 0.5);
      }
      if (w <= 0 || h <= 0 || w > 65536 || h > 65536) return {};
    }
    return {w, h, rotation};
  }
  return {};
}
}  // namespace detail

// The shape of a film's first video track from its file's header, or none.
inline VideoShape shape(const keyframes::ReadFn& read, uint64_t file_size) {
  uint8_t magic[8];
  if (file_size < 16 || !read(0, 8, magic)) return {};
  VideoShape s;
  try {
    const uint32_t box = (uint32_t)detail::be(magic + 4, 4);
    if (detail::be(magic, 4) == 0x1A45DFA3) s = detail::mkv(read, file_size);
    else if (box == detail::fourcc("ftyp") || box == detail::fourcc("moov") || box == detail::fourcc("free") ||
             box == detail::fourcc("wide") || box == detail::fourcc("mdat")) s = detail::mp4(read, file_size);
  } catch (...) { return {}; }
  const double ar = s.aspect();
  if (!s.valid() || !(ar > 0.1 && ar < 10)) return {};
  return s;
}
}  // namespace media_header
