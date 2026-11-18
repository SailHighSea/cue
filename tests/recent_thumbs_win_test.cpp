// Windows only (Wine or a PC): the home list's pictures on disk. A frame from the helper is
// checked, kept as a small JPEG (160 x 90 at most) named by a hash of its path, read back as its
// picture (stale when older than the film's last opening), a black frame is never kept, and the
// frames of entries that left the list are deleted.
#include <windows.h>

#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <ctime>
#include <fstream>

#include "../src/core/recents_view.h"
#include "../src/core/store.h"
#include "../src/win/recent_thumbs.h"


static RecentThumbResult* wait_result(HWND hwnd, UINT msg) {
  for (int i = 0; i < 400; ++i) {
    MSG m;
    while (PeekMessageW(&m, hwnd, msg, msg, PM_REMOVE)) return reinterpret_cast<RecentThumbResult*>(m.lParam);
    Sleep(10);
  }
  return nullptr;
}

int main() {
  CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  const UINT msg = WM_APP + 16;
  HWND hwnd = CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);
  assert(hwnd);
  const auto root = std::filesystem::temp_directory_path() / "cue-recent-thumbs-test";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  const auto dir = root / "thumbs";
  const std::string film = (root / "Film (2024).mkv").string(), other = (root / "other.mkv").string();
  { std::ofstream(film) << "x"; std::ofstream(other) << "x"; }

  RecentThumbs rt;
  rt.start(hwnd, msg, dir);
  // a frame (512 x 288, a gradient): kept, and answered as a fresh picture of 256 x 144
  std::vector<uint32_t> px(512 * 288);
  for (int y = 0; y < 288; ++y)
    for (int x = 0; x < 512; ++x) px[(size_t)y * 512 + x] = 0xFF000000u | (uint32_t)(x / 2) << 16 | (uint32_t)(y * 255 / 287) << 8 | 0x40;
  rt.save_frame(film, px, 512, 288);
  RecentThumbResult* r = wait_result(hwnd, msg);
  assert(r && r->frame && r->thumb && r->thumb->w == 160 && r->thumb->h == 90 && !r->thumb->cover);
  delete r;
  const auto jpg = dir / thumb_file_name(normalize_path(film));
  assert(std::filesystem::exists(jpg) && std::filesystem::file_size(jpg) > 500 && std::filesystem::file_size(jpg) < 8000);
  // read back for the home (another run of Cue: nothing in memory)
  RecentThumbs again;
  again.start(hwnd, msg, dir);
  bool known = true;
  assert(!again.cached(film, known) && !known);
  const int64_t now_s = (int64_t)std::time(nullptr);
  again.want({{film, false, now_s - 60}, {other, false, now_s - 60}});
  int got = 0, pictures = 0;
  for (int i = 0; i < 2; ++i) {
    RecentThumbResult* a = wait_result(hwnd, msg);
    assert(a && !a->frame);
    ++got;
    if (a->thumb) {
      ++pictures;
      assert(normalize_path(a->path) == normalize_path(film) && a->thumb->w == 160 && a->thumb->h == 90 && !a->stale);
      const uint32_t mid = a->thumb->px[45 * 160 + 80];
      assert(((mid >> 16) & 255) > 100 && ((mid >> 16) & 255) < 160);  // the gradient survived JPEG
    }
    again.remember(a->path, a->thumb);
    delete a;
  }
  assert(got == 2 && pictures == 1);
  // asked again: known, nothing goes to the worker
  again.cached(other, known);
  assert(known);
  // a black frame is never kept
  std::vector<uint32_t> black(320 * 180, 0xFF000000u);
  again.save_frame(other, black, 320, 180);
  Sleep(300);
  assert(!std::filesystem::exists(dir / thumb_file_name(normalize_path(other))));
  // the list changed: what left it is deleted; cleared: all of it
  again.save_frame(other, px, 512, 288);
  delete wait_result(hwnd, msg);
  assert(std::filesystem::exists(dir / thumb_file_name(normalize_path(other))));
  again.keep_only({film});
  Sleep(300);
  assert(std::filesystem::exists(jpg) && !std::filesystem::exists(dir / thumb_file_name(normalize_path(other))));
  again.keep_only({});
  Sleep(300);
  assert(!std::filesystem::exists(jpg));
  // a frame older than the film's last opening is stale: the helper takes it again
  again.save_frame(film, px, 512, 288);
  delete wait_result(hwnd, msg);
  RecentThumbs fourth;
  fourth.start(hwnd, msg, dir);
  fourth.want({{film, false, now_s + 3600}});
  RecentThumbResult* st = wait_result(hwnd, msg);
  assert(st && st->thumb && st->stale);
  delete st;
  // the kept frame is answered without asking the film itself (16/11: a film on a disk asleep held
  // every picture behind it for seconds): here the film is not there at all, and its frame still is
  {
    const auto away = root / "away.mkv";
    { std::ofstream(away) << "x"; }
    fourth.save_frame(away.string(), px, 512, 288);
    delete wait_result(hwnd, msg);
    std::filesystem::remove(away);
    RecentThumbs later;
    later.start(hwnd, msg, dir);
    later.want({{away.string(), false, now_s - 60}});
    RecentThumbResult* aw = wait_result(hwnd, msg);
    assert(aw && aw->thumb && aw->thumb->w == 160 && !aw->stale && !aw->frame);
    delete aw;
    later.stop();
  }
  // a song's cover taken by the helper is square, and read back as a cover
  std::vector<uint32_t> sq(300 * 300, 0xFF000000u);
  for (size_t i = 0; i < sq.size(); ++i) sq[i] |= (uint32_t)(i % 255) << 8;
  const std::string song = (root / "song.mka").string();
  { std::ofstream(song) << "x"; }
  fourth.save_frame(song, sq, 300, 300, true);
  RecentThumbResult* cv = wait_result(hwnd, msg);
  assert(cv && cv->thumb && cv->thumb->cover && cv->thumb->w == 90 && cv->thumb->h == 90);
  delete cv;
  // read back at the next start from the kept picture, which is not written again
  const auto kept = dir / thumb_file_name(normalize_path(song));
  const auto kept_at = std::filesystem::last_write_time(kept);
  RecentThumbs fifth;
  fifth.start(hwnd, msg, dir);
  fifth.want({{song, true, now_s}});
  RecentThumbResult* cr = wait_result(hwnd, msg);
  assert(cr && cr->thumb && cr->thumb->cover);
  assert(std::filesystem::last_write_time(kept) == kept_at);
  delete cr;
  // the song changed after it (new tags): its cover is looked for in the file again; with none
  // there, the kept picture still answers
  std::filesystem::last_write_time(song, kept_at + std::chrono::seconds(30));
  RecentThumbs sixth;
  sixth.start(hwnd, msg, dir);
  sixth.want({{song, true, now_s}});
  RecentThumbResult* cn = wait_result(hwnd, msg);
  assert(cn && cn->thumb && cn->thumb->cover);
  delete cn;
  sixth.stop();
  // a file opening: pictures not yet read are put off (asked again later)
  fifth.want({{film, false, 0}});
  fifth.pause_loads();
  fourth.stop();
  fifth.stop();
  again.stop();
  rt.stop();
  std::filesystem::remove_all(root);
  std::printf("Recent thumbs (Windows): frames and covers kept as small JPEGs, read back (a cover not written again), black frames refused, pruned with the list passed\n");
}
