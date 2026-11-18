// The window's poll rests when nothing can change on screen, and is back at full pace the moment
// anything does. Measured in Wine with a counting build (1.4): paused, controls away, pointer
// still: 30.3 wake-ups a second, every one of them this poll; the home in view: the same.
#include "../src/core/poll_pace.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

using namespace poll_pace;

static std::string read(const char* p) { std::ifstream f(p); std::stringstream b; b << f.rdbuf(); return b.str(); }
static std::string body(const std::string& s, const std::string& head) {
  const size_t a = s.find(head);
  assert(a != std::string::npos);
  return s.substr(a, s.find("\n}\n", a) - a);
}

int main() {
  // the home
  {
    Home h;
    assert(home(h) == kFast);  // the pointer just moved
    h.pointer_still = 2.9; assert(home(h) == kFast);
    h.pointer_still = 3.0; assert(home(h) == kStill && kStill == 100);  // the eyes start following within 0.1 s
    h.animating = true; assert(home(h) == kFast);                       // a blink, a fade, a hover
    h.animating = false; h.under_active_window = true; assert(home(h) == kRest);
    h.pointer_still = 0; assert(home(h) == kRest);                      // a pointer moving in the app in front
    h.under_active_window = false; h.minimized = true; assert(home(h) == kRest && kRest == 250);
  }
  // the player
  {
    Play p;
    assert(play(p) == kFast);                     // playing
    p.paused = true; assert(play(p) == kRest);   // paused, controls away, nothing moving
    p.controls_shown = true; assert(play(p) == kFast);
    p.controls_shown = false; p.dragging = true; assert(play(p) == kFast);
    p.dragging = false; p.animating = true; assert(play(p) == kFast);  // a frame loop
    p.animating = false; p.light_moving = true; assert(play(p) == kFast);  // the room's light still gliding
    p.light_moving = false; p.work_pending = true; assert(play(p) == kFast);  // a phone's preview, a folder
    p.work_pending = false; assert(play(p) == kRest);
    p.paused = false; p.minimized = true; assert(play(p) == kRest);     // minimized: as before
    // (19g) a film playing with no light around it, its controls away, nothing moving: 10 a second
    p.minimized = false; assert(play(p) == kFast);                       // the light is stepped by the poll
    p.light = false; assert(play(p) == kStill);
    p.light_moving = true; assert(play(p) == kStill);                    // the hidden controls' colour gliding
    p.light_moving = false;
    p.controls_shown = true; assert(play(p) == kFast);
    p.controls_shown = false; p.animating = true; assert(play(p) == kFast);
    p.animating = false; p.work_pending = true; assert(play(p) == kFast);
    p.work_pending = false; p.dragging = true; assert(play(p) == kFast);
    p.dragging = false; p.paused = true; assert(play(p) == kRest);       // paused: rest, as before
    p.paused = false; p.minimized = true; assert(play(p) == kRest);
  }
  // the app: one place sets the pace, everything that can change the screen brings it back
  {
    const std::string s = read("src/win/app.cpp");
    assert(s.find("SetTimer(hwnd_, T_POLL") == s.find("SetTimer(hwnd_, T_POLL, ms, nullptr);"));  // only set_poll
    assert(s.find("SetTimer(hwnd_, T_POLL", s.find("SetTimer(hwnd_, T_POLL") + 1) == std::string::npos);
    const std::string ensure = body(s, "void App::ensure_anim() {");
    assert(ensure.find("poll_fast();") < ensure.find("if (anim_on_) return;"));
    const std::string drain = body(s, "void App::drain_mpv() {");
    assert(drain.find("if (news) poll_fast();") != std::string::npos);  // (19g: any news but the clock's)
    assert(drain.find("news |= e->reply_userdata != O_TIME;") != std::string::npos);
    assert(drain.find("news |= e->event_id != MPV_EVENT_PROPERTY_CHANGE;") != std::string::npos);
    assert(s.find("case WM_APP_REMOTE: poll_fast(); remote_events(); return 0;") != std::string::npos);
    assert(s.find("if(w) { set_hidden(false); poll_fast(); }") != std::string::npos);
    // the home: the eyes follow only when they can be seen
    const size_t home = s.find("else if (w == T_POLL && mode_ == Mode::Home");
    const size_t behind = s.find("const bool behind = under_active_window();", home);
    const size_t track = s.find("if (!behind && !home_enter_pending_ && (onboarding_ ? ob_.track(", home);
    const size_t pace = s.find("set_poll(poll_pace::home(h));", home);
    assert(home != std::string::npos && behind < track && track < pace && pace < s.find("else if (w == T_POLL && mode_ == Mode::Play)", home));
    // the player: decided at the end of each poll, also while covered
    const size_t play = s.find("else if (w == T_POLL && mode_ == Mode::Play)");
    assert(s.find("set_poll(play_pace(true));", play) < s.find("return 0;", s.find("set_poll(play_pace(true));", play)));
    assert(s.find("set_poll(play_pace(false));", play) < s.find("case WM_MOUSEMOVE:", play));
    const std::string pace_fn = body(s, "UINT App::play_pace(bool hidden) const {");
    for (const char* k : {"play_.dock_visible()", "atmo_moving_", "remote_preview_pending()", "preview_.busy", "!first_frame_", "anim_on_"})
      assert(pace_fn.find(k) != std::string::npos);
    // what is drawn counts, not the invisible borders; Cue on top or cloaked by itself is in view
    const std::string under = body(s, "bool App::under_active_window() const {");
    assert(under.find("DWMWA_EXTENDED_FRAME_BOUNDS") != std::string::npos && under.find("WS_EX_TOPMOST") != std::string::npos && under.find("cloaked_") != std::string::npos);
  }
  std::puts("Poll pace: home in view 30/s then 10/s after 3 s still, home under the active window and a calm paused film 4/s, any animation, input, engine or phone event back to 30/s at once passed");
}
