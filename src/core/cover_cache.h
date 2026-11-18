#pragma once
#include <algorithm>
#include <cstddef>
#include <deque>
#include <memory>
#include <string>

// The covers read lately, found by the song's path alone (18a, 17/11). Asking the disk for a
// song's size and date (its identity) can take seconds (an external disk asleep, a network share
// that dropped): never on the asking thread, which is usually the window's. The read itself tells
// the identity it saw; a request for an entry not checked for a while is answered at once and
// the identity is checked again on a worker: a song re-tagged since is read again at its next
// request. Not thread-safe: the caller holds its lock around every call.
template <class Job> class CoverCache {
 public:
  static constexpr size_t kKept = 4;
  static constexpr double kRecheck = 10.0;  // s: the requests of one open (and its read-ahead) ask nothing

  // The entry for `path` (a job not given up), most recent first; `check`: the caller checks its
  // identity on a worker and reports it (one check at a time per entry).
  template <class Usable> std::shared_ptr<Job> find(const std::string& path, double now, bool& check, Usable usable) {
    check = false;
    for (size_t i = 0; i < entries_.size(); ++i) {
      if (entries_[i].path != path || !usable(*entries_[i].job)) continue;
      Entry e = entries_[i];
      entries_.erase(entries_.begin() + (std::ptrdiff_t)i);
      if (!e.checking && now - e.checked_at >= kRecheck) { e.checking = true; check = true; }
      entries_.push_front(e);
      return e.job;
    }
    return {};
  }
  // A new read for `path`: it reports the identity it saw.
  void add(const std::string& path, std::shared_ptr<Job> job, double now) {
    entries_.push_front({path, {}, std::move(job), now, false, true});
    while (entries_.size() > kKept) entries_.pop_back();
  }
  // The identity a read or a check saw (`identity` "" = no file). False: it changed, the entry is
  // gone (the next request reads the song again).
  bool report(const Job* job, const std::string& identity, double now) {
    auto it = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& e) { return e.job.get() == job; });
    if (it == entries_.end()) return true;  // (no longer kept)
    if (it->known && it->identity != identity) { entries_.erase(it); return false; }
    it->identity = identity;
    it->known = true;
    it->checked_at = now;
    it->checking = false;
    return true;
  }
  // A check that never ran (its worker was saturated): another may be asked.
  void unchecked(const Job* job) {
    for (auto& e : entries_) if (e.job.get() == job) e.checking = false;
  }
  size_t size() const { return entries_.size(); }

 private:
  struct Entry {
    std::string path, identity;
    std::shared_ptr<Job> job;
    double checked_at = 0;
    bool known = false, checking = false;
  };
  std::deque<Entry> entries_;  // newest first
};
