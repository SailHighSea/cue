// The installer's face: one small window in the Omni language (dark, pixel type, the eyes).
// Portable (drawn into a Canvas), so it can be previewed as PNG on any OS.
#pragma once
#include <cmath>
#include <string>
#include <vector>

#include "../src/ui/anim.h"
#include "../src/ui/face.h"
#include "../src/ui/gfx.h"

enum class SetupMode { Install, Upgrade, Update, Uninstall };
enum class SetupStage { Ask, Working, Done, Failed };
enum class SetupAct { None, Primary, Secondary, ToggleDesktop, ToggleLaunch, ToggleKeep, ChangeDir, Min, Close };

// The installer's pixel: always a whole one, so every stroke is the same width: 2 px per art px at
// 100% and 125%, 3 at 150% and 175%, 4 at 200%, 5 at 250%. Its art is compact (356 x 208): 712 x 416
// at 100%, 1068 x 624 at 175% (a half step there, 3.5, made it nearly as large as before 1.4).
inline double setup_scale(unsigned dpi) {
  if (dpi < 48) dpi = 96;
  double s = std::floor(dpi / 48.0);
  return s < 2 ? 2 : s > 8 ? 8 : s;
}

struct SetupView {
  static constexpr int kBaseW = 356, kH = 208;  // art px
  // The window is 356 px of art wide, wider only when the current language's words need it
  // (the eyes' panel keeps its size; the content column grows). Fixed for the whole run.
  static int art_w();
  int w = 0, h = 0;
  double s = 2;
  SetupMode mode = SetupMode::Install;
  SetupStage stage = SetupStage::Ask;
  std::string version = "1.0", old_version;  // shown as "0.9 → 1.0" when upgrading
  std::string dir;                            // install folder (shown shortened)
  std::string status;                         // what is happening right now
  std::string error;
  double progress = 0;                        // 0..1, set by the worker
  bool desktop = true, launch = true, keep = true;
  Face face;

  void set_size(int w_, int h_, double s_);
  void start(double now);
  void set_stage(SetupStage st, double now);
  bool tick(double now, double dt);
  void render(Canvas& c, double now);
  SetupAct mouse_move(int x, int y, double now);
  SetupAct mouse_down(int x, int y, double now);
  void mouse_leave(double now);
  SetupAct key(const std::string& k, bool shift, double now);
  bool is_background(int x, int y) const;

 private:
  enum Hit { kNone = -1, kPrimary, kSecondary, kDesktop, kLaunch, kKeep, kDir, kMin, kClose, kCount };
  int hover_ = kNone, focus_ = kNone;
  bool kb_ = false;
  int mx_ = -1, my_ = -1;
  double t0_ = 0, stage_t0_ = 0;
  double hov_[kCount] = {};
  Spring bar_ = Spring::rd(0.45, 1.0, 0.0005);  // the progress bar follows the work smoothly
  Spring content_ = Spring::rd(0.42, 0.86, 0.002);  // stage changes slide the right column
  Rect r_[kCount];
  void layout();
  int hit(int x, int y) const;
  std::vector<int> focus_order() const;
  std::string primary_label() const;
  std::string secondary_label() const;
  void draw_check(Canvas& c, const Rect& r, bool on, const std::string& label, double hv, double a) const;
  void draw_button(Canvas& c, const Rect& r, const std::string& label, bool primary, double hv, double a) const;
};
