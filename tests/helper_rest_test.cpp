// The preview helper kept from a film does not idle on the home for good: after a minute with
// nothing to do it goes (its engine, decoder and memory), and the next film starts it again, as
// the first film of a session does. Measured in Wine (1.4): it stayed, asleep, 91 MB resident.
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
  const std::string s = read("src/win/app.cpp"), h = read("src/win/app.h");
  assert(h.find("T_HELPER_REST = 21") != std::string::npos && h.find("kHelperRest = 60000") != std::string::npos);
  // armed when the home has nothing left for the helper it kept from a film
  const std::string next = body(s, "void App::next_home_shot() {");
  assert(next.find("else { thumbs_.close_file(); SetTimer(hwnd_, T_HELPER_REST, kHelperRest, nullptr); }") != std::string::npos);
  // a file opening disarms it (the helper is the player's again)
  assert(body(s, "void App::stop_home_shots() {").find("KillTimer(hwnd_, T_HELPER_REST);") != std::string::npos);
  assert(body(s, "void App::open(").find("stop_home_shots();") != std::string::npos);
  // it ends the helper only on a quiet home: no file on its way, no picture being taken or waiting
  const size_t t = s.find("else if (w == T_HELPER_REST) {");
  assert(t != std::string::npos);
  const std::string fire = s.substr(t, s.find("else if (w == T_FOLDER)", t) - t);
  assert(fire.find("mode_ == Mode::Home && pending_open_.empty() && !home_shot_busy_ && !home_shot_checking_ && home_shot_queue_.empty() && thumbs_.running()") != std::string::npos);
  assert(fire.find("thumbs_.shutdown();") != std::string::npos && fire.find("thumb_path_.clear();") != std::string::npos);
  // the next film starts a helper when it has none
  assert(body(s, "void App::thumb_request(").find("if (!thumbs_.running() && !thumbs_.start(hwnd_, WM_APP_THUMB))") != std::string::npos);
  // closing Cue kills the timer with the others
  {  // (in the list of timers killed together; 13t added T_AUDIO_DEVICE after it)
    const std::string q = body(s, "void App::quit_now() {");
    const size_t kill = q.find("}) KillTimer(hwnd_, t);");
    assert(kill != std::string::npos && q.rfind("T_HELPER_REST,", kill) != std::string::npos && q.rfind("for (UINT_PTR t : {", kill) < q.rfind("T_HELPER_REST,", kill));
  }
  std::puts("Helper rest: the helper kept from a film goes after a minute idle on the home, never while a picture or a file needs it, and the next film starts it again passed");
}
