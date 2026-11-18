// Windows integration check against the shipped engine DLL: a film's own audio file and its
// chapters, as Cue uses them. Compile with src/win/mpv.cpp, src/core/i18n.cpp, src/core/json.cpp.
// Usage: tracks_engine_test.exe DLL_DIRECTORY VIDEO AUDIO_FILE CHAPTERS_MKV CHAPTERS_MP4
//   VIDEO: a film with one audio track; AUDIO_FILE: a sound file of the same length (a "dub");
//   CHAPTERS_*: the same film with three chapters ("Abertura" 0 s, "A travessia" 4 s, "Créditos" 8 s)
// Checks: "audio-add <file> auto <title>" adds an external track without switching to it, with
// its file and title in the track list (what App::adopt_added_audio matches); choosing it then
// plays it; a missing file fails the command (App's toast); chapter-list has the titles and
// times from MKV and MP4; "chapter" jumps; "add chapter 1/-1" steps. Also: the same file added
// twice is listed twice (Cue guards against it), and "file-local-options/end" ends a film whose
// added sound is longer than its picture at the film's own length, and the next file starts
// without the added sound or that end.
#ifdef _WIN32
#include "../src/win/mpv.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static std::string utf8(const wchar_t* w) {
  int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
  std::string s(n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
  s.pop_back();
  return s;
}

// read through Cue's own parser (Mpv::parse_chapters, what App uses)
static std::vector<Chapter> chapters(Mpv& mpv) {
  mpv_node node{};
  if (mpv.api().get_property(mpv.handle(), "chapter-list", MPV_FORMAT_NODE, &node) < 0) return {};
  std::vector<Chapter> out = Mpv::parse_chapters(&node);
  mpv.api().free_node_contents(&node);
  return out;
}

int wmain(int argc, wchar_t** argv) {
  assert(argc == 6);
  Mpv mpv;
  std::string error;
  assert(mpv.load(argv[1], &error));
  assert(mpv.create(nullptr, 0, 0, &error));
  mpv.option("config", "no"); mpv.option("terminal", "no");
  mpv.option("vo", "null"); mpv.option("ao", "null");
  mpv.option("idle", "yes"); mpv.option("keep-open", "yes"); mpv.option("pause", "yes");
  mpv.option("load-scripts", "no"); mpv.option("msg-level", "all=no");
  assert(mpv.api().initialize(mpv.handle()) >= 0);
  int replies_ok = 0, replies_err = 0;
  bool loaded = false;
  auto pump = [&](DWORD ms) {
    DWORD end = GetTickCount() + ms;
    do {
      while (auto* e = mpv.wait()) {
        if (e->event_id == MPV_EVENT_NONE) break;
        if (e->event_id == MPV_EVENT_FILE_LOADED) loaded = true;
        if (e->event_id == MPV_EVENT_COMMAND_REPLY && e->reply_userdata == 105) (e->error >= 0 ? replies_ok : replies_err)++;
      }
      Sleep(5);
    } while (GetTickCount() < end);
  };
  auto open = [&](const std::string& path) {
    loaded = false;
    assert(mpv.cmd({"loadfile", path}) >= 0);
    for (int i = 0; i < 400 && !loaded; ++i) pump(10);
    assert(loaded);
    pump(100);
  };
  auto audio_tracks = [&] {
    mpv_node node{};
    assert(mpv.api().get_property(mpv.handle(), "track-list", MPV_FORMAT_NODE, &node) >= 0);
    auto all = Mpv::parse_tracks(&node);
    mpv.api().free_node_contents(&node);
    std::vector<Track> out;
    for (auto& t : all) if (t.type == "audio") out.push_back(t);
    return out;
  };

  // a film's own audio file: added, not chosen; then chosen like a track in the menu
  const std::string video = utf8(argv[2]), dub = utf8(argv[3]);
  open(video);
  auto before = audio_tracks();
  assert(before.size() == 1 && before[0].selected && !before[0].external);
  const std::string aid_before = mpv.get_string("aid");
  assert(mpv.cmd({"audio-add", dub, "auto", "dub pt-BR"}, 105) >= 0);
  pump(400);
  assert(replies_ok == 1 && replies_err == 0);
  auto after = audio_tracks();
  assert(after.size() == 2);
  const Track& added = after[1];
  assert(added.external && added.external_file == dub && added.title == "dub pt-BR");
  assert(mpv.get_string("aid") == aid_before);  // "auto": the film's sound keeps playing
  assert(mpv.set("aid", std::to_string(added.id)));
  pump(200);
  assert(mpv.get_string("aid") == std::to_string(added.id));
  // a file that cannot be opened: the command fails (Cue says so in a toast)
  assert(mpv.cmd({"audio-add", dub + ".missing", "auto", "x"}, 105) >= 0);
  pump(400);
  assert(replies_err == 1 && audio_tracks().size() == 2);
  // the same file again: the engine lists it once more (Cue chooses the track it has instead)
  assert(mpv.cmd({"audio-add", dub, "auto", "dub pt-BR"}, 105) >= 0);
  pump(400);
  assert(replies_ok == 2 && audio_tracks().size() == 3);
  // a sound longer than the picture plays on past the film's length; with the film's length as
  // its end (what App::adopt_added_audio sets) the film ends there: here a second before its end
  {
    const double length = mpv.get_double("duration", 0);
    assert(length > 3);
    char end[32];
    std::snprintf(end, sizeof end, "%.3f", length - 1);
    assert(mpv.set("file-local-options/end", end));
    assert(mpv.set("speed", "4") && mpv.set("pause", "no"));
    for (int i = 0; i < 200 && mpv.get_string("eof-reached") != "yes"; ++i) pump(50);
    assert(mpv.get_string("eof-reached") == "yes");
    assert(std::fabs(mpv.get_double("time-pos", -1) - (length - 1)) < 0.3);
    assert(mpv.set("pause", "yes") && mpv.set("speed", "1"));
  }
  // the next file starts without either: the added sounds and the end belonged to that film
  open(video);
  {
    auto next = audio_tracks();
    assert(next.size() == 1 && !next[0].external);
    const double length = mpv.get_double("duration", 0);
    assert(mpv.set("speed", "4") && mpv.set("pause", "no"));
    for (int i = 0; i < 200 && mpv.get_string("eof-reached") != "yes"; ++i) pump(50);
    assert(mpv.get_string("eof-reached") == "yes");
    assert(std::fabs(mpv.get_double("time-pos", -1) - length) < 0.3);
    assert(mpv.set("pause", "yes") && mpv.set("speed", "1"));
  }
  std::printf("external audio: added without switching, file and title listed, chosen on request, a missing file reported, the same file listed again by the engine, the film's end kept, nothing carried to the next file\n");

  // chapters from MKV and MP4: titles and times as the engine reads them; jumps and steps
  for (int f = 4; f <= 5; ++f) {
    open(utf8(argv[f]));
    auto ch = chapters(mpv);
    assert(ch.size() == 3);
    assert(ch[0].title == "Abertura" && ch[1].title == "A travessia" && ch[2].title == "Cr\xc3\xa9" "ditos");
    assert(std::fabs(ch[0].t) < 0.05 && std::fabs(ch[1].t - 4) < 0.05 && std::fabs(ch[2].t - 8) < 0.05);
    assert(mpv.get_string("chapters") == "3");
    assert(mpv.set("chapter", "1"));
    pump(300);
    assert(std::fabs(mpv.get_double("time-pos", -1) - 4) < 0.6 && mpv.get_string("chapter") == "1");
    assert(mpv.cmd({"add", "chapter", "1"}) >= 0);
    pump(300);
    assert(mpv.get_string("chapter") == "2");
    std::printf("chapters (%s): titles, times, jump and step\n", f == 4 ? "MKV" : "MP4");
  }
  mpv.destroy();
  std::printf("tracks engine: external audio and chapters passed\n");
  return 0;
}
#else
int main() { return 0; }
#endif
