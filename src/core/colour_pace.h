#pragma once
#include <algorithm>
#include <cmath>

// How often the preview helper may take a film's colours (the room's light, the accent).
// Each sample is an exact seek: the helper decodes from the keyframe before that moment up to
// it. A film with a keyframe every second costs a few tens of milliseconds a sample; a rip with
// one every ten seconds (x264's default) costs ten times that, and taken at the usual pace the
// helper decoded a quarter of a core for the whole film. The pace follows what a sample really
// costs: the helper works at most a tenth of the time, never more often than the usual interval
// and never less than once every 15 s. Light films (most of them) keep the usual interval.
class ColourPace {
 public:
  static constexpr double kDuty = 0.10;  // the share of the time the helper may spend on colours
  static constexpr double kLongest = 15.0;

  void reset() { cost_ = -1; }
  // what one colour sample took in the helper (seek and decode), in seconds
  void measured(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0) return;
    seconds = std::min(seconds, 30.0);
    cost_ = cost_ < 0 ? seconds : cost_ * 0.6 + seconds * 0.4;  // one slow sample does not decide alone
  }
  // seconds between two samples, given the usual interval
  double interval(double usual) const {
    if (!(usual > 0)) usual = 1.5;
    if (cost_ <= 0) return usual;
    return std::clamp(cost_ / kDuty, usual, std::max(usual, kLongest));
  }
  double cost() const { return cost_; }

 private:
  double cost_ = -1;
};
