#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

// A film whose data ran out well before its length (a USB stick pulled out, a network share
// gone, a download that never finished, a cloud file that failed to arrive) has not ended: its
// place is kept and the next episode does not start. Its end is its last 3% (30 s at least), so
// credits, a length rounded in the file and a missing last second still count as the end.
// Songs keep the old rule: a VBR song's length is often only an estimate.
inline bool stopped_short(double time, double duration) {
  if (!std::isfinite(time) || !std::isfinite(duration) || duration <= 60 || time < 0) return false;
  return duration - time > std::max(30.0, duration * 0.03);
}

// Own the destination across EOF/idle/timer events. Duplicate EOF notifications
// must not enqueue the same transition twice or borrow mutable discovery state.
class EpisodeAdvance {
 public:
  bool arm(const std::string& path, bool allowed) {
    if (!allowed || path.empty()) return false;
    if (target_.empty()) target_ = path;
    return true;
  }
  bool return_home(bool idle, bool loaded) const { return idle && !loaded && target_.empty(); }
  bool pending() const { return !target_.empty(); }
  std::string take() { return std::exchange(target_, {}); }
  void clear() { target_.clear(); }
 private:
  std::string target_;
};
