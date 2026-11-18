// An episode's opening and ending, found by their sound (see core/intro_match.h), for files
// whose chapters do not name them. Light on the PC by design:
//  * the sound is read by a helper process (Cue.exe --sound-print) at idle priority and in
//    Windows' background mode (its disk reads wait for everything else), one file at a time,
//    only a few minutes of it, at 8 kHz in one channel, and only once the episode has played
//    a while
//  * what was found is kept (sound-parts.txt in Cue's folder, one short line a file), so each
//    episode of a season is read once: comparing episodes 3 and 4 answers for both
//  * nothing leaves the PC, nothing is written next to the films
#pragma once
#include <windows.h>

#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "../core/intro_match.h"

// The helper's own work: the prints of `path` written to `out` (0 when it did).
// head: the first minutes (and the length); tail: the last ones, of a film `dur` s long.
int sound_print_main(const std::string& path, const std::wstring& out, bool tail, double dur);

class SoundFinder {
 public:
  SoundFinder() = default;
  ~SoundFinder();
  SoundFinder(const SoundFinder&) = delete;
  SoundFinder& operator=(const SoundFinder&) = delete;
  // `msg` is posted to `hwnd` when parts are known for a file asked about
  void init(const std::wstring& folder, HWND hwnd, UINT msg);
  // what an earlier look found for this file (true: it was looked at; parts may be empty). Asks no
  // disk (18d): only for a file look_up or a request has reached (its identity is kept), else false
  bool known(const std::string& path, std::vector<SkipPart>& parts);
  // whether `path` is there on this PC's disks (not only in the cloud), and its identity, asked on
  // a thread of its own: `msg` is posted with wParam 1 and a new LookUp* (the receiver deletes it)
  struct LookUp { std::string path; bool reachable = false; };
  void look_up(const std::string& path);
  // look for `path`'s opening and ending by comparing it with `partner`, another episode of the
  // show; a newer request replaces one not started yet
  void request(const std::string& path, const std::string& partner);
  void stop();  // closing: the helper ends now

 private:
  struct Job { std::string path, partner; };
  void run();
  bool prints_of(const std::string& path, bool tail, double dur, intro::EpisodePrints& out);
  void load_locked();
  void save_locked();
  static std::string identity(const std::string& path);

  std::wstring folder_;
  HWND hwnd_ = nullptr;
  UINT msg_ = 0;
  std::mutex mu_;
  std::condition_variable cv_;
  std::thread worker_;
  bool quit_ = false, loaded_ = false;
  std::unique_ptr<Job> next_;
  HANDLE helper_ = nullptr;  // the running helper, ended by stop()
  void keep_identity_locked(const std::string& path, const std::string& id);
  std::map<std::string, std::string> ids_;    // path -> identity, as last read by a worker
  std::map<std::string, std::string> found_;  // identity -> encoded parts ("" none)
  std::map<std::string, std::string> early_;  // the openings, while the endings are read
  std::set<std::string> failed_;              // could not be read this run: not asked again
  std::deque<std::pair<std::string, std::shared_ptr<intro::EpisodePrints>>> prints_;  // the last few, by identity
};
