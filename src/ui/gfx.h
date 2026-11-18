// Pixel renderer shared by every surface of Cue.
// Art-pixel geometry accepts half steps for the medium size. Cell boundaries
// snap to physical pixels; integer scales stay uniform, with no image filtering.
#pragma once
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

struct Rgb { uint8_t r, g, b; };

inline Rgb mix(Rgb a, Rgb b, double t) {
  if (t <= 0) return a;
  if (t >= 1) return b;
  auto m = [t](uint8_t x, uint8_t y) { return (uint8_t)(x + (y - x) * t + 0.5); };
  return {m(a.r, b.r), m(a.g, b.g), m(a.b, b.b)};
}

// Omni tokens (dark only). `P()` returns them, boosted when Windows high contrast is on.
struct Palette {
  Rgb bg{3, 3, 3}, screen{0, 0, 0}, band{10, 10, 10}, sheet{11, 11, 11}, tip_bg{20, 20, 20},
      hover{26, 26, 26}, press{38, 38, 38}, edge{35, 35, 35}, faint{48, 48, 48},
      dim{120, 120, 120}, fg{237, 237, 237}, pixel{255, 255, 255};
};
const Palette& P();
// Where there is no film to lend its colour (the welcome, the home), a state that is on (a switch,
// a value changed) takes this warm peach instead.
constexpr Rgb kPeach{247, 186, 140};
void set_high_contrast(bool on);

namespace tok {
constexpr double hover = 0.110, tip_delay = 0.450, tip_in = 0.140, enter = 0.260, blink = 0.110;
constexpr double dock_in = 0.160, dock_out = 0.200, dock_idle = 2.0;
}  // namespace tok

struct Rect {
  int x = 0, y = 0, w = 0, h = 0;
  Rect() = default;
  template<class X, class Y, class W, class H>
  Rect(X xx, Y yy, W ww, H hh)
      : x((int)std::lround(xx)), y((int)std::lround(yy)),
        w((int)std::lround(xx + ww) - x), h((int)std::lround(yy + hh) - y) {}
  bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
  Rect inset(int d) const { return {x + d, y + d, w - 2 * d, h - 2 * d}; }
  Rect grow(int d) const { return inset(-d); }
  int r() const { return x + w; }
  int b() const { return y + h; }
  int cx() const { return x + w / 2; }
  int cy() const { return y + h / 2; }
};

struct BitmapDef { const char* name; int w, h; const char* cells; };
struct GlyphDef { const char* ch; int w; const char* rows; };
struct MaskDef { const char* label; int s, w, h; const char* a; };

const BitmapDef* bmp(std::string_view name);              // icon or "sprite:<name>"
const MaskDef* label_mask(std::string_view label, double s);  // pre-rasterized font labels

// Premultiplied BGRA, the layout both Windows DIBs and mpv "bgra" overlays expect.
struct Canvas {
  int w = 0, h = 0;
  std::vector<uint32_t> px;
  // An optional clip: every drawing call (and clear) touches only pixels inside it. The home uses
  // it to draw again only the eyes' corner of the window when nothing else changed: the same
  // drawing code, the same pixels there, nothing outside touched (tests/home_partial_test.cpp).
  Rect clip{};
  bool clipped = false;
  void set_clip(const Rect& r) { clip = r; clipped = true; }
  void reset_clip() { clipped = false; }
  // the pixels drawing may touch: the canvas, within the clip when there is one
  void reach(int& x0, int& y0, int& x1, int& y1) const {
    x0 = 0; y0 = 0; x1 = w; y1 = h;
    if (!clipped) return;
    x0 = std::max(x0, clip.x); y0 = std::max(y0, clip.y);
    x1 = std::min(x1, clip.x + clip.w); y1 = std::min(y1, clip.y + clip.h);
  }

  void resize(int nw, int nh);
  void clear(Rgb c, double a = 1.0);
  void clear_transparent();
  void fill_pixels(int x, int y, int w, int h, Rgb c, double a = 1.0);
  void fill(double x, double y, double w, double h, Rgb c, double a = 1.0);
  void fill(const Rect& r, Rgb c, double a = 1.0) { fill(r.x, r.y, r.w, r.h, c, a); }
  // Omni `corner`: the corner cell (size `cut`) stays empty.
  void cornered(const Rect& r, int cut, Rgb c, double a = 1.0);
  // Draws a pixel bitmap; each cell is `cell` physical px. `bits` may be a raw '#' grid.
  void bitmap(const BitmapDef* b, int x, int y, double cell, Rgb c, double a = 1.0, bool flip_x = false);
  void cells(const char* grid, int gw, int gh, int x, int y, double cell, Rgb c, double a = 1.0);
  void mask(const MaskDef* m, int x, int y, Rgb c, double a = 1.0);
  // Copies `src` over this canvas at (x, y) with src-over.
  void blit(const Canvas& src, int x, int y, double a = 1.0);
  void scale_alpha(double a);
  // this canvas becomes src with its alpha scaled by a, in one pass (a copy, then scale_alpha,
  // went over a fading 4K list twice a frame); the same rounding as scale_alpha
  void assign_scaled(const Canvas& src, double a);
};

// Source-over of one premultiplied BGRA pixel, as the canvas draws it (the fast path when the
// source is premultiplied) and the plain per-channel reference: tests compare the two.
uint32_t blend_over_for_tests(uint32_t src, uint32_t dst);
uint32_t blend_over_reference_for_tests(uint32_t src, uint32_t dst);

// Supplied action SVGs use a half-art-pixel grid; center their visible ink.
void action_icon(Canvas& c, std::string_view name, const Rect& box, double s, Rgb colour, double alpha = 1.0);

// The switch: 14 x 8 cells of `cell` px (stepped corners), a square knob that slides on its
// spring. k: the knob's place, 0 off .. 1 on (a hair past either end while it overshoots). On, the
// track takes `on` (the film's colour) and the knob is a black stone with a small tick beside it;
// off, an outlined track with a grey knob. One drawing for the welcome and the player's menus.
void draw_switch(Canvas& c, double x, double y, double cell, double k, Rgb on, double hover, bool enabled, double a = 1.0);
constexpr int kSwitchW = 14, kSwitchH = 8;  // cells

// A pixel-language "rounded rectangle": a shape on the art grid whose corners
// follow `profile` (inset per row from the top, mirrored at the bottom),
// filled with `fill` and outlined with a 1-cell `edge`.
void draw_panel(Canvas& c, int x, int y, int aw, int ah, double s, const std::vector<int>& profile,
                Rgb fill, double fill_a, Rgb edge, double edge_a);

// Omni Mono. Line box is 12 art rows; cap height 7.
int text_w(std::string_view utf8, double s);
int text(Canvas& c, std::string_view utf8, int x, int y, double s, Rgb col, double a = 1.0);
// Cuts long names in the middle and keeps the extension: "Pôr do sol em Rio Pr…se).mp4"
std::string fit_middle(std::string_view name, int max_w, double s);
std::string fit_end(std::string_view text, int max_w, double s);
// Characters the Omni lacks (Chinese, kana, Greek, Cyrillic...) are drawn from a pixel fallback
// on the same 12-row grid; anything neither has is skipped, never drawn as a box.
bool has_glyph(uint32_t cp);
// Word wrap for the pixel font: breaks at spaces, and between ideographs (never before a
// closing mark such as ，。：）or after an opening one), so Chinese wraps without spaces.
std::vector<std::string> wrap_lines(std::string_view text, int max_w, double s);

// UTF-8 helpers
uint32_t next_cp(std::string_view s, size_t& i);
