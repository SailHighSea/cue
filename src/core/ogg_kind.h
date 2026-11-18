// An .ogg file is a song or a film: its name does not say (Vorbis and Opus songs are .ogg, and so
// are many Theora films), its first pages do. Every stream of an Ogg file begins on a page of its
// own, all of them before any media, so the first few KB tell whether a picture stream is there.
// Untrusted input: only the bytes given are looked at; anything unexpected says "no picture"
// (the file stays what its name says, a song).
#pragma once
#include <algorithm>
#include <cstdint>
#include <string_view>

namespace ogg_kind {

// How much of the file's start to read for has_video (a beginning page is small: ~100 bytes).
inline constexpr size_t kHeadBytes = 8192;

// `head`: the file's first bytes. True when one of its streams is a picture that plays
// (Theora, Daala, Dirac, VP8, an OGM video stream); never for a song with its cover.
inline bool has_video(std::string_view head) {
  size_t p = 0;
  for (int pages = 0; pages < 32 && head.size() - p >= 27; ++pages) {
    if (head.compare(p, 4, "OggS") != 0) return false;
    const bool bos = (uint8_t)head[p + 5] & 0x02;
    const size_t nseg = (uint8_t)head[p + 26];
    if (head.size() - p - 27 < nseg) return false;
    size_t total = 0;
    for (size_t i = 0; i < nseg; ++i) total += (uint8_t)head[p + 27 + i];
    const size_t body = p + 27 + nseg;
    if (!bos) return false;  // past the beginnings: every stream has said what it is
    const std::string_view b = head.substr(body, std::min(total, head.size() - body));
    auto starts = [&](std::string_view m) { return b.size() >= m.size() && b.compare(0, m.size(), m) == 0; };
    if (starts(std::string_view("\x80theora", 7)) || starts(std::string_view("\x80" "daala", 6)) ||
        starts(std::string_view("BBCD\0", 5)) || starts("OVP80") || starts(std::string_view("\x01video\0\0\0", 9)))
      return true;
    if (head.size() - body < total) return false;
    p = body + total;
  }
  return false;
}

}  // namespace ogg_kind
