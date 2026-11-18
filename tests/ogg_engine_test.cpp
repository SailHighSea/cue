// Windows integration check against the shipped engine DLL: Ogg as Cue opens it. Compile with
// src/win/mpv.cpp, src/core/cover_art.cpp, src/core/i18n.cpp and src/core/json.cpp.
// Usage: ogg_engine_test.exe DLL_DIRECTORY VORBIS_OGG OPUS FLAC_OGA THEORA_OGG
//   VORBIS_OGG, OPUS, FLAC_OGA: a few seconds of sound, each with its cover in a Vorbis comment
//     (METADATA_BLOCK_PICTURE); THEORA_OGG: a film (Theora picture, Vorbis sound) named .ogg
// Checks, for each song: the engine decodes it (the clock moves past 1 s), its sound track has
// the right codec, the cover is a picture track marked as album art, so Cue's music view gets it
// (audio_visual_track / audio_cover_track, what App::configure_audio_mode uses), and Cue's own
// reader (cover_art, what the home's pictures and the phone use) finds the same cover. For the
// film: the engine sees a picture that is not album art, so it plays as a film, and Cue's reading
// of the file's beginning (ogg_kind, what the recent list and the resume use) says so too.
#ifdef _WIN32
#include "../src/win/mpv.h"
#include "../src/core/audio_mode.h"
#include "../src/core/cover_art.h"
#include "../src/core/ogg_kind.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

static std::string utf8(const wchar_t* w) {
  int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
  std::string s(n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
  s.pop_back();
  return s;
}

static std::string head_of(const wchar_t* path) {
  std::ifstream f(path, std::ios::binary);
  std::string s(ogg_kind::kHeadBytes, '\0');
  f.read(s.data(), (std::streamsize)s.size());
  s.resize((size_t)f.gcount());
  return s;
}

int wmain(int argc, wchar_t** argv) {
  assert(argc == 6);
  Mpv mpv;
  std::string error;
  assert(mpv.load(argv[1], &error));
  assert(mpv.create(nullptr, 0, 0, &error));
  mpv.option("config", "no"); mpv.option("terminal", "no");
  mpv.option("vo", "null"); mpv.option("ao", "null");
  mpv.option("idle", "yes"); mpv.option("keep-open", "yes");
  mpv.option("load-scripts", "no"); mpv.option("msg-level", "all=no");
  assert(mpv.api().initialize(mpv.handle()) >= 0);
  bool loaded = false, failed = false;
  auto pump = [&](DWORD ms) {
    DWORD end = GetTickCount() + ms;
    do {
      while (auto* e = mpv.wait()) {
        if (e->event_id == MPV_EVENT_NONE) break;
        if (e->event_id == MPV_EVENT_FILE_LOADED) loaded = true;
        if (e->event_id == MPV_EVENT_END_FILE) {
          auto* ef = static_cast<mpv_event_end_file*>(e->data);
          if (ef && ef->reason == MPV_END_FILE_REASON_ERROR) failed = true;
        }
      }
      Sleep(5);
    } while (GetTickCount() < end);
  };
  auto tracks = [&] {
    mpv_node node{};
    assert(mpv.api().get_property(mpv.handle(), "track-list", MPV_FORMAT_NODE, &node) >= 0);
    auto all = Mpv::parse_tracks(&node);
    mpv.api().free_node_contents(&node);
    return all;
  };
  auto open = [&](const std::string& path) {
    loaded = failed = false;
    assert(mpv.cmd({"loadfile", path}) >= 0);
    for (int i = 0; i < 500 && !loaded && !failed; ++i) pump(10);
    assert(loaded && !failed);
    // it plays: the engine's clock moves (decoded, not only opened)
    double t = 0;
    for (int i = 0; i < 300 && t < 1.0; ++i) { pump(10); t = mpv.get_double("time-pos", 0); }
    assert(t >= 1.0);
  };

  const char* codecs[3] = {"vorbis", "opus", "flac"};
  for (int i = 0; i < 3; ++i) {
    const std::string path = utf8(argv[2 + i]);
    open(path);
    auto all = tracks();
    const int64_t aid = audio_visual_track(all);
    assert(aid > 0);  // a song: Cue's music view
    bool codec_ok = false;
    for (auto& t : all) if (t.id == aid && t.type == "audio" && t.codec == codecs[i]) codec_ok = true;
    assert(codec_ok);
    const int64_t cover = audio_cover_track(all);
    assert(cover > 0);  // its cover, from the Vorbis comment, as the engine's album art
    const std::string found = cover_art::embedded_in_file(path);
    assert(found.size() > 1000 && (uint8_t)found[0] == 0xFF && (uint8_t)found[1] == 0xD8);  // and Cue's reader finds it
    assert(!ogg_kind::has_video(head_of(argv[2 + i])));
    std::printf("ogg engine: %s plays, cover %zu bytes\n", codecs[i], found.size());
  }
  {
    const std::string path = utf8(argv[5]);
    open(path);
    auto all = tracks();
    assert(audio_visual_track(all) == 0);  // a film: the player, not the music view
    bool picture = false, sound = false;
    for (auto& t : all) {
      if (t.type == "video" && !t.albumart && t.codec == "theora") picture = true;
      if (t.type == "audio" && t.codec == "vorbis") sound = true;
    }
    assert(picture && sound);
    assert(mpv.get_double("width", 0) > 0);
    assert(ogg_kind::has_video(head_of(argv[5])));
    std::puts("ogg engine: a Theora film named .ogg plays as a film");
  }
  std::puts("ogg engine: passed");
  return 0;
}
#else
int main() { return 0; }
#endif
