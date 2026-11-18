#pragma once

// How often the window's poll (T_POLL) runs. It reads the pointer for the home's eyes, notices
// a pointer that left the player, steps the room's light and the taskbar's buttons. Nothing it
// does needs 30 a second when nothing can change on screen:
// - minimized: 4 a second (as before; the media card and the phone still hear in time);
// - the home under the active window (the whole of it covered): 4 a second, the eyes do not
//   follow a pointer nobody can see them follow;
// - the home in view with the pointer still for 3 s: 10 a second (a pointer that moves again
//   outside the window is seen within 0.1 s; inside, its moves arrive at once anyway);
// - a paused film, its controls away, nothing moving: 4 a second (a pointer coming over the
//   film arrives from the video's window at once, a key or the engine wakes it as well);
// - (19g) a film playing with no light around it (the light off, or the mini player's), its controls
//   away, nothing moving: 10 a second. The poll steps the light: with none it only notices a pointer
//   that left the window and the window being covered. The engine's clock no longer brings it back to
//   full pace (its other news still does), nor does anything else than before.
// Anything that animates, any input and any engine event put it back to 30 a second.
namespace poll_pace {

constexpr unsigned kFast = 33, kStill = 100, kRest = 250;
constexpr double kStillAfter = 3.0;  // seconds the pointer must have stayed put on the home

struct Home {
  bool minimized = false;
  bool under_active_window = false;  // entirely covered by the window in front
  bool animating = false;            // a frame loop, the light fading out, the video window leaving
  double pointer_still = 0;          // seconds since the pointer last moved
};

struct Play {
  bool minimized = false;
  bool paused = false;
  bool controls_shown = false;  // the dock (or a menu) is on screen
  bool dragging = false;
  bool animating = false;       // a frame loop, the window moving, a transition
  bool light_moving = false;    // the room's light (or the controls' colour) still changing
  bool work_pending = false;    // a folder being read, a phone's preview, a hover's picture, a first log
  bool light = true;            // a light stepped by the poll (the room's around a film, the music's)
};

inline unsigned home(const Home& h) {
  if (h.minimized || h.under_active_window) return kRest;
  if (h.animating) return kFast;
  return h.pointer_still >= kStillAfter ? kStill : kFast;
}

inline unsigned play(const Play& p) {
  if (p.minimized) return kRest;
  if (p.paused && !p.controls_shown && !p.dragging && !p.animating && !p.light_moving && !p.work_pending) return kRest;
  // (with no light shown, what still glides is the controls' colour, for controls away: it goes on at 10 a second)
  if (!p.paused && !p.light && !p.controls_shown && !p.dragging && !p.animating && !p.work_pending) return kStill;
  return kFast;
}

}  // namespace poll_pace
