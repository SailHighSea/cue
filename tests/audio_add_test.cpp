// "load audio track" (App::add_audio_file, App::adopt_added_audio). Checked against the shipped
// engine (tests/tracks_engine_test.cpp) and in the real Cue under Wine:
// - the same file chosen again is chosen in the list, not added once more (the engine listed it
//   twice, the menu showed "dub, dub");
// - a sound longer than the film (a dub with longer credits, another cut) no longer keeps the
//   film playing on its last frame, its clock past the film's length (00:20 of 00:12): the film
//   ends at its own length (file-local: the next file starts without it).
#include <cassert>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

static std::string read(const char* p) { std::ifstream f(p); std::stringstream b; b << f.rdbuf(); return b.str(); }
static std::string body(const std::string& s, const std::string& head) {
  const size_t a = s.find(head);
  assert(a != std::string::npos);
  return s.substr(a, s.find("\n}\n", a) - a);
}

int main() {
  const std::string s = read("src/win/app.cpp");
  // one comparison of two names of a file, the case and the slashes aside (normalize_path)
  assert(s.find("static bool same_file_path(const std::string& a, const std::string& b) { return normalize_path(a) == normalize_path(b); }") != std::string::npos);
  const std::string add = body(s, "void App::add_audio_file() {");
  const size_t again = add.find("if (t.external && same_file_path(t.external_file, f[0])) {");
  assert(again != std::string::npos && again < add.find("\"audio-add\""));
  assert(add.find("if (!t.selected) select_audio(t.id);", again) < add.find("return;", again));
  // (the dialog is modal and the film plays on: another film since, nothing is added to it)
  assert(add.find("path_ != film) return;") < again);
  const std::string adopt = body(s, "void App::adopt_added_audio() {");
  assert(adopt.find("same_file_path(t.external_file, pending_audio_file_)") != std::string::npos);
  const size_t end = adopt.find("mpv_.set_async(\"file-local-options/end\", end);");
  assert(end != std::string::npos && end < adopt.find("select_audio(t.id);"));
  assert(adopt.find("if (play_.in.dur > 1 && std::isfinite(play_.in.dur)) {") < end);
  // a file that is not a sound: the engine refuses it and Cue says so
  const size_t reply = s.find("if (e->reply_userdata == kReplyAudioAdd) {");
  assert(reply != std::string::npos && s.find("play_.toast(tr(str::toast_audio_failed), nullptr, now());", reply) < s.find("adopt_added_audio();", reply));
  std::puts("Audio add: the same file chosen again picks its track, a longer sound ends with the film, a refused file says so passed");
}
