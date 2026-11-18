// An .ogg is a song or a film: its first pages say which (Theora, Daala, Dirac, VP8 or an OGM
// video stream next to the audio), never a song's cover; broken or cut beginnings say "a song".
#include "../src/core/ogg_kind.h"
#include <cassert>
#include <cstdio>
#include <string>

static std::string le32(uint32_t v) { return {(char)v, (char)(v >> 8), (char)(v >> 16), (char)(v >> 24)}; }
static std::string page(uint32_t serial, bool bos, const std::string& packet) {
  std::string segs;
  size_t left = packet.size();
  while (left >= 255) { segs += (char)255; left -= 255; }
  segs += (char)left;
  return "OggS" + std::string(1, '\0') + std::string(1, bos ? '\x02' : '\0') + std::string(8, '\0') + le32(serial) + le32(0) + le32(0) +
         std::string(1, (char)segs.size()) + segs + packet;
}

int main() {
  using ogg_kind::has_video;
  const std::string vorbis = page(3, true, std::string("\x01vorbis", 7) + std::string(23, '\x02'));
  const std::string opus = page(4, true, "OpusHead" + std::string(11, '\x01'));
  const std::string theora = page(2, true, std::string("\x80theora", 7) + std::string(35, '\x03'));
  const std::string skeleton = page(1, true, std::string("fishead\0", 8) + std::string(56, '\0'));
  const std::string media = page(3, false, std::string(400, 'a'));
  // songs: Vorbis, Opus, with their comments (a cover is a picture that does not play)
  assert(!has_video(vorbis + page(3, false, std::string("\x03vorbis", 7) + "METADATA_BLOCK_PICTURE=...") + media));
  assert(!has_video(opus + media));
  // films: Theora with Vorbis, in either order, behind a Skeleton
  assert(has_video(theora + vorbis + media));
  assert(has_video(vorbis + theora + media));
  assert(has_video(skeleton + vorbis + theora + media));
  assert(has_video(page(5, true, std::string("\x80" "daala", 6) + std::string(20, '\0')) + vorbis));
  assert(has_video(page(5, true, std::string("BBCD\0", 5) + std::string(20, '\0')) + vorbis));
  assert(has_video(vorbis + page(5, true, "OVP80" + std::string(20, '\0'))));
  assert(has_video(vorbis + page(5, true, std::string("\x01video\0\0\0", 9) + std::string(40, '\0'))));
  // a picture stream that begins after the media has started is not looked for (nor is it real)
  assert(!has_video(vorbis + media + theora));
  // broken, cut or empty: a song, as its name says
  assert(!has_video(""));
  assert(!has_video("OggS"));
  assert(!has_video("RIFF....WAVEfmt "));
  assert(!has_video(theora.substr(0, 20)));
  assert(!has_video(vorbis.substr(0, vorbis.size() - 3)));
  std::string bad = theora; bad[26] = (char)200;  // more segments than bytes
  assert(!has_video(bad));
  // only the head is needed: the beginnings fit in the first KB even with a long first page
  assert(has_video((skeleton + vorbis + theora + media).substr(0, 300)));
  std::puts("ogg kind: songs, films behind a Skeleton or after their audio, and broken heads passed");
}
