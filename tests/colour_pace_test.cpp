// The colours' pace: a film whose samples are cheap keeps the usual interval; one whose
// keyframes are far apart is sampled less often, the helper working at most a tenth of the time.
#include "../src/core/colour_pace.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

static bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }

int main() {
  ColourPace p;
  // nothing measured yet: the usual interval (the room's light, the accent alone, a weak PC's double)
  assert(near(p.interval(1.5), 1.5) && near(p.interval(4.0), 4.0) && near(p.interval(3.0), 3.0));
  // measured in Wine with the shipped engine, 1080p: a keyframe every second, 64 ms a sample
  for (int i = 0; i < 10; ++i) p.measured(0.064);
  assert(near(p.interval(1.5), 1.5) && near(p.interval(4.0), 4.0));
  // a keyframe every 10 s (x264's default), 551 ms a sample: every 5.5 s instead of 1.5 s
  p.reset();
  assert(near(p.interval(1.5), 1.5));
  for (int i = 0; i < 10; ++i) p.measured(0.551);
  assert(std::fabs(p.interval(1.5) - 5.51) < 1e-6);
  assert(p.cost() / p.interval(1.5) <= ColourPace::kDuty + 1e-9);
  // never slower than once every 15 s, whatever a sample costs (4K software decoding)
  for (int i = 0; i < 20; ++i) p.measured(9.0);
  assert(near(p.interval(1.5), 15.0));
  // one slow sample (the first, with a cold disk) does not decide alone
  p.reset(); p.measured(0.06); p.measured(2.0);
  assert(p.interval(1.5) < 10.0 && p.interval(1.5) > 1.5);
  for (int i = 0; i < 12; ++i) p.measured(0.06);
  assert(near(p.interval(1.5), 1.5));
  // nonsense in, nothing changes
  p.reset(); p.measured(NAN); p.measured(-1); p.measured(INFINITY);
  assert(near(p.interval(1.5), 1.5) && p.cost() < 0);
  assert(near(p.interval(0), 1.5) && near(p.interval(NAN), 1.5));  // (a broken usual interval: the room's own)
  // the app: measured from the helper's colour replies, reset for every file, and asked for the interval
  {
    std::ifstream f("src/win/app.cpp"); std::stringstream b; b << f.rdbuf(); const std::string s = b.str();
    assert(s.find("if (thumb_for_accent_) colour_pace_.measured(m->decode_ms / 1000.0);") != std::string::npos);
    assert(s.find("colour_pace_.reset();", s.find("void App::reset_atmosphere(")) < s.find("if (keep_music_light)", s.find("void App::reset_atmosphere(")));
    assert(s.find("now() - accent_req_t_ > colour_pace_.interval(") != std::string::npos);
  }
  std::puts("Colour pace: cheap samples keep the usual interval, far-apart keyframes sample less often (10% of the time at most, 15 s at the longest), a single slow sample and nonsense ignored passed");
}
