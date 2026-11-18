#include "../src/core/episode_advance.h"
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
int main() {
  EpisodeAdvance next;
  std::string discovered="C:\\Series\\S01E02.mkv";
  assert(!next.return_home(true,true)); // idle observed before END_FILE: keep discovery until terminal event
  assert(next.arm(discovered,true));
  assert(!next.return_home(true,false)); // END_FILE queued advance before idle

  discovered.clear(); // discovery/idle state is mutable; the queued request owns its name
  assert(next.pending());
  assert(next.arm("C:\\wrong.mkv",true)); // duplicate EOF keeps the original destination
  auto target=next.take();
  assert(target=="C:\\Series\\S01E02.mkv" && !next.pending());
  assert(next.take().empty());
  assert(next.return_home(true,false)); // no pending advance and no file: ordinary Home
  assert(!next.return_home(false,false));
  assert(!next.arm("",true));
  assert(!next.arm(target,false)); // cancellation, loop and end timer don't schedule
  assert(next.arm(target,true));next.clear();assert(!next.pending());
  assert(next.arm("C:\\Series\\S01E03.mkv",true));
  assert(next.take()=="C:\\Series\\S01E03.mkv");
  // a film whose data ran out long before its end has not ended: 47 s into a 2 min file
  // cut at 40%, a stick pulled out at 40 min of 2 h; its last 3% (30 s at least) still is the end
  assert(stopped_short(47.9, 120.02));
  assert(stopped_short(40 * 60, 2 * 3600));
  assert(!stopped_short(120.0, 120.02) && !stopped_short(119.0, 120.02) && !stopped_short(91, 120));
  assert(stopped_short(89, 120));                                  // 31 s left of 2 min
  assert(!stopped_short(2 * 3600 - 200, 2 * 3600));                // the credits of a 2 h film (3% = 216 s)
  assert(stopped_short(2 * 3600 - 300, 2 * 3600));
  assert(!stopped_short(5, 50) && !stopped_short(0, 60));          // short clips: the old rule
  assert(!stopped_short(10, 0) && !stopped_short(10, -1) && !stopped_short(NAN, 100) && !stopped_short(10, INFINITY) && !stopped_short(-1, 100));
  {
    std::ifstream f("src/win/app.cpp"); std::stringstream b; b << f.rdbuf(); const std::string s = b.str();
    const size_t eof = s.find("case O_EOF:");
    assert(eof != std::string::npos);
    const size_t keep = s.find("store_.set_position(path_, short_end ? in.time : 0, in.dur);", eof);
    const size_t next = s.find("else if (!short_end) queue_next_episode();", eof);
    assert(keep != std::string::npos && next != std::string::npos && next < s.find("eof_ = e;", eof));
    assert(s.find("if (eof && !short_end) queue_next_episode();") != std::string::npos && s.find("save_position(eof && !short_end);") != std::string::npos);
  }
  std::cout << "Episode handoff: owned destination, duplicate EOF, one-shot timer and cancellation, a film cut short keeping its place passed\n";
}
