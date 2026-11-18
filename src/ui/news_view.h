// "Saiu do forno": shown once to people who updated, over the home, before anything else.
// Cue's eyes, happy, lit from both sides; pixel sparkles fly out of them; the heading drops in
// letter by letter and the version is stamped beside it; then what arrived, on three equal
// cards (a coloured edge drawn on top, the feature in miniature, its name, two lines). A card
// under the pointer rises a pixel or two, glows in its own colour and plays its miniature again.
// One button, "continuar" (Enter or Esc too).
// Everything soft (the eyes' light, a card's glow) is made once per size and only blitted; after
// the entrance the screen is drawn again only when something on it changed.
#pragma once
#include <string>
#include <vector>

#include "anim.h"
#include "face.h"
#include "gfx.h"
#include "scene_view.h"

enum class NewsAct { None = 0, Continue = 1 };

struct NewsView : SceneView {
  int w = 0, h = 0;
  double s = 2;        // its own comfortable pixel scale (like the welcome)
  double ui_s = 2;     // the interface scale (the floor)
  std::string version;

  void set_size(int w_, int h_, double s_) override;
  void open(double now) override;
  void close(double now) override { if (close_t0_ < 0) { close_t0_ = now; changed_ = true; } }
  bool closing() const override { return close_t0_ > 0; }
  bool closed(double now) const override { return close_t0_ > 0 && now - close_t0_ >= kCloseDur; }
  bool tick(double now, double dt) override;
  void render(Canvas& c, double now) override;
  void pointer_move(int x, int y, double now) override;
  void pointer_leave(double now) override;
  SceneAct pointer_down(int x, int y, double now) override;
  SceneAct key_down(const std::string& k, bool shift, double now) override;
  int cursor() const override { return hover_ >= 0 ? 1 : 0; }
  bool fast(double now) const override;
  bool changed(double now) const override { (void)now; return changed_; }
  bool draggable(int x, int y) const override { return hit(x, y) < 0; }

  // the entrance's length: until the last miniature has played (tests, previews)
  static constexpr double kEntrance = 2.6;

 private:
  static constexpr double kCloseDur = 0.24;
  enum : int { kContinue = 0 };
  static constexpr int kCards = 3;
  Face face_;
  Rng rng_{14};
  double open_t0_ = -10, close_t0_ = -1, poke_t0_ = -10;
  int hover_ = -1, focus_ = -1;
  bool kb_ = false;
  double hov_[1] = {0};
  bool overflow_ = false;
  int cols_ = 3;
  bool changed_ = true;    // something on screen moved since the last frame drawn
  uint64_t look_key_ = 0;  // what the clock-driven parts (sparkles) showed last
  bool released_ = false;  // closed: its pictures are let go until it opens again
  // layout, physical px
  double ts_ = 4;          // the heading's pixel
  int head_y_ = 0, line_y_ = 0;
  Rect buttons_[1];
  std::vector<Rect> items_;                      // the cards
  std::vector<std::vector<std::string>> lines_;  // each card's lines (never cut)
  Canvas light_;       // the eyes' light (made once per layout)
  Canvas light_base_;  // the same, already laid on the screen's ground: copied in place once it is fully in
  Rect light_r_;
  double thumbs_s_ = 0;
  Canvas thumbs_[3];  // the miniatures' pictures, Cue's own stand-ins (made once per layout)
  // the cards: the one under the pointer, how far each is lifted, how much it glows, when its
  // miniature last started playing; its glow is made the first time it is needed
  int hover_card_ = -1;
  Spring lift_[kCards];
  double glow_[kCards] = {0, 0, 0};
  double card_t0_[kCards] = {-10, -10, -10};
  Canvas card_light_[kCards];
  Rect card_light_r_[kCards];
  // a poke: sparks out of the eyes, falling as they fade
  struct Spark { double x, y, vx, vy, t0, life; int big; };
  std::vector<Spark> sparks_;
  void layout();
  void release();
  void make_card_light(int i);
  int hit(int x, int y) const;
  int card_at(int x, int y) const;
  uint64_t look(double now) const;
  double entrance_t(double now) const { return now - open_t0_; }
  void draw_sparkles(Canvas& c, double now) const;
  void draw_heading(Canvas& c, double now) const;
  void draw_card(Canvas& c, int i, const Rect& r, double a, double now);
  void draw_button(Canvas& c, const Rect& r, const std::string& label, bool primary, double hv, double a, bool chevron) const;
};
