// Openings and endings found by their sound: in every episode of a season the opening (and the
// ending's song) plays note for note, wherever it starts. Two episodes are compared: a light
// print of each one's first minutes and of its last ones (8 kHz mono, 32 bits every 32 ms: which
// of 33 bands between 300 and 3000 Hz gained on its neighbour over the last quarter second, after
// Haitsma & Kalker's fingerprint), then the longest stretch where the two prints agree. The prints are made by a
// helper process at idle priority (win/sound_parts.cpp); everything here is plain arithmetic.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "skip_parts.h"

namespace intro {

inline constexpr int kRate = 8000;     // samples a second the helper asks the engine for
inline constexpr int kWindow = 2048;   // 256 ms
inline constexpr int kLag = 8;         // a band's change is read over 8 hops (256 ms): a print taken
                                       // a few ms off still agrees
inline constexpr int kHop = 256;       // 32 ms
inline constexpr double kHeadSeconds = 330;  // where an opening is looked for: the first 5.5 min
inline constexpr double kTailSeconds = 270;  // an ending: the last 4.5 min
inline constexpr double kMinPart = 15, kMaxPart = 180;  // shorter repeats are jingles, longer ones not a song

// The stretches read: an opening is looked for in the first minutes, an ending in the last ones
// (a short film's two never overlap). A film under 8 min is not looked at.
inline constexpr double kMinFilm = 480;
inline double head_seconds(double dur) { return dur < kHeadSeconds / 0.45 ? dur * 0.45 : kHeadSeconds; }
inline double tail_start(double dur) { return dur - (dur < kTailSeconds / 0.4 ? dur * 0.4 : kTailSeconds); }

struct Print {
  double t0 = 0;                // where in the film the print starts (s)
  std::vector<uint32_t> code;   // one per hop
  std::vector<uint8_t> quiet;   // 1: too quiet to say anything (silence agrees with every silence)
  double seconds(size_t i) const { return t0 + (double)i * kHop / kRate; }
};

// The print of `n` samples (mono, 16-bit, kRate a second) starting `t0` s into the film.
Print make_print(const int16_t* pcm, size_t n, double t0);

struct Match {
  bool found = false;
  double a0 = 0, a1 = 0, b0 = 0, b1 = 0;  // the stretch in each film (s)
};
// The longest stretch the two prints share (at least kMinPart, at most kMaxPart).
Match longest_shared(const Print& a, const Print& b);

// One episode's prints and length, as the helper reports them.
struct EpisodePrints {
  double dur = 0;
  Print head, tail;
};
// The opening and the ending of two episodes of one show, each in its own times (from_sound set).
// An opening is a shared stretch of the first minutes; an ending, of the last ones.
void shared_parts(const EpisodePrints& a, const EpisodePrints& b, std::vector<SkipPart>& out_a,
                  std::vector<SkipPart>& out_b);

// The helper's report on disk, and back (false for anything not a whole report).
std::string encode(const EpisodePrints& e);
bool decode(const std::string& bytes, EpisodePrints& e);

// Parts kept between runs: one line per file (its identity: path, size, last change), so a
// season is read once. "" parts: looked at, nothing found (not looked at again).
std::string encode_parts(const std::vector<SkipPart>& parts);
bool decode_parts(const std::string& text, std::vector<SkipPart>& parts);

}  // namespace intro
