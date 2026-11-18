// The home list's pictures (see HomeView): a film's frame where it was left, a song's or a book's
// cover. Everything here is small and slow on purpose: one background thread at low priority
// (reading, decoding, writing), each answer reaching the window as a message.
//
// Where a picture comes from:
//   film   a small frame Cue's own helper took (App::next_home_shot: on the home only, never while
//          anything plays), kept as a JPEG of at most 160 x 90 in %APPDATA%\tinypeep\Cue\thumbs,
//          named by a hash of the path. Older than the film's last opening, it is answered as
//          stale: the helper takes it again where the film was left.
//   audio  the cover in the file's tags or beside it (see core/cover_art), kept the first time it
//          is read (a small JPEG like a film's frame, read again only when the file changes),
//          else the cover the helper took once (kept the same way), else none
// One file per recent entry at most, removed with the entry (and all of them with "clear recents").
#pragma once
#include <windows.h>

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "../ui/home.h"

struct RecentThumbResult {
  std::string path;
  std::shared_ptr<const HomeThumb> thumb;  // null: none found (the stand-in stays)
  bool frame = false;                      // a new frame from the helper (replaces a picture already shown)
  bool stale = false;                      // a film's frame from before its last opening: take it again
};

class RecentThumbs {
 public:
  RecentThumbs() = default;
  ~RecentThumbs() { stop(); }
  RecentThumbs(const RecentThumbs&) = delete;
  RecentThumbs& operator=(const RecentThumbs&) = delete;

  // `msg` arrives with lParam = RecentThumbResult* (the receiver deletes it)
  void start(HWND hwnd, UINT msg, std::filesystem::path dir);
  void stop();

  // What is already known in memory (UI thread). found: whether the path was looked up before
  // (a null picture then means there is none).
  std::shared_ptr<const HomeThumb> cached(const std::string& path, bool& found) const;
  // The answer arrived (UI thread): kept in memory for the next time the home is shown.
  void remember(const std::string& path, std::shared_ptr<const HomeThumb> t);

  // Looks for the pictures of these entries (newest first), skipping what is known or asked.
  // last: when each was last opened (unix seconds), to tell a film's frame that is out of date.
  struct Want { std::string path; bool audio = false; int64_t last = 0; };
  void want(const std::vector<Want>& entries);
  // A frame or a cover the helper took (opaque, small): reduced, written, answered as fresh.
  void save_frame(const std::string& path, std::vector<uint32_t> px, int w, int h, bool cover = false);
  // a file is opening: pictures still waiting to be read wait for the next time the home is shown
  // (the one being read finishes), so they never compete with the film for the disk
  void pause_loads();
  // The recent list changed: pictures of entries no longer in it are deleted (all, when empty).
  void keep_only(const std::vector<std::string>& paths);

 private:
  struct Job {
    enum Kind { Load, Save, Prune } kind = Load;
    std::string path;
    bool audio = false;
    bool cover = false;  // (a song's cover taken by Cue's helper: shown square)
    int64_t last = 0;
    std::vector<uint32_t> px;
    int w = 0, h = 0;
    std::vector<std::string> keep;
  };
  struct Shared {
    HWND hwnd = nullptr;
    UINT msg = 0;
    std::filesystem::path dir;
    std::mutex mu;
    std::condition_variable cv;
    std::deque<Job> jobs;
    bool quit = false;
  };
  static bool write_picture(const std::filesystem::path& dir, const std::string& path, const HomeThumb& t);
  static void run(std::shared_ptr<Shared> sh);
  void push(Job j);
  // (the worker owns its state with this object: on quit it is let go, never waited for, so a
  // slow answer from Windows about a sleeping drive cannot hold Cue's closing)
  std::shared_ptr<Shared> sh_;
  // UI thread only
  std::map<std::string, std::shared_ptr<const HomeThumb>> memo_;  // normalized path -> picture (null: none)
  std::vector<std::string> memo_order_;
  std::map<std::string, bool> asked_;  // normalized paths on their way
 public:
  // forget what is known of one entry (a film just left: its frame is out of date)
  void forget(const std::string& path);
};
