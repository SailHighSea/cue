// Padding and alignment audit of the player's menus (Linux, no engine; only a review tool, not
// part of Cue). Every drawing call the panel makes is traced (the linker's --wrap: Cue's code is
// untouched) and each drawn thing measured by its ink, so the report gives, per row, where the
// icon, the words, the value, the chevron and the switch really land, in screen px.
// Build (from the repository root; the objects of src/ui and src/core built natively first):
//   g++ -std=c++20 -O1 -I. -o build/audit_menus tools/audit_menus.cpp build/nat/*.o \
//     -Wl,--wrap=_Z4textR6CanvasSt17basic_string_viewIcSt11char_traitsIcEEiid3Rgbd \
//     -Wl,--wrap=_Z11action_iconR6CanvasSt17basic_string_viewIcSt11char_traitsIcEERK4Rectd3Rgbd \
//     -Wl,--wrap=_ZN6Canvas6bitmapEPK9BitmapDefiid3Rgbdb -Wl,--wrap=_ZN6Canvas5cellsEPKciiiid3Rgbd \
//     -Wl,--wrap=_ZN6Canvas4fillEdddd3Rgbd -Wl,--wrap=_ZN6Canvas8corneredERK4Recti3Rgbd \
//     -Wl,--wrap=_ZN6Canvas4maskEPK7MaskDefii3Rgbd -Wl,--wrap=_Z11draw_switchR6Canvasdddd3Rgbdbd
//   ./build/audit_menus out [png]     (out/rows.tsv, and the panels as PNG with "png")
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#define private public
#define protected public
#include "../src/ui/play.h"
#undef private
#undef protected
#include "../src/core/i18n.h"
#include "../src/core/media_info.h"
#include "../src/core/ui_scale.h"
#include "../src/core/util.h"

// ── the trace ──────────────────────────────────────────────────────
struct Ink { std::string kind, what; Rect box; int ax = 0, ay = 0; };  // box: the ink, in the panel's canvas
static std::vector<Ink> g_ink;
static const Canvas* g_target = nullptr;
static Canvas g_scratch;

static Rect scan(const Rect& want, const Canvas& c) {
  Rect r{std::max(0, want.x), std::max(0, want.y), 0, 0};
  int x1 = std::min(c.w, want.r()), y1 = std::min(c.h, want.b());
  int ax = 1 << 30, ay = 1 << 30, bx = -1, by = -1;
  for (int y = r.y; y < y1; ++y)
    for (int x = r.x; x < x1; ++x)
      if ((g_scratch.px[(size_t)y * g_scratch.w + x] >> 24) > 40) { ax = std::min(ax, x); ay = std::min(ay, y); bx = std::max(bx, x); by = std::max(by, y); }
  if (bx < 0) return {};
  return {ax, ay, bx - ax + 1, by - ay + 1};
}
template <class F> static void measure(Canvas& c, const char* kind, std::string what, Rect area, F draw) {
  if (&c != g_target) return;
  area = area.grow(6);
  g_scratch.resize(c.w, c.h);
  g_scratch.set_clip(area);
  g_scratch.clear_transparent();
  draw(g_scratch);
  g_scratch.reset_clip();
  Rect b = scan(area, c);
  if (b.w > 0) g_ink.push_back({kind, std::move(what), b});
}

extern "C" {
int __real__Z4textR6CanvasSt17basic_string_viewIcSt11char_traitsIcEEiid3Rgbd(Canvas&, std::string_view, int, int, double, Rgb, double);
int __wrap__Z4textR6CanvasSt17basic_string_viewIcSt11char_traitsIcEEiid3Rgbd(Canvas& c, std::string_view t, int x, int y, double s, Rgb col, double a) {
  measure(c, "text", std::string(t), {x, y, text_w(t, s) + (int)(2 * s), (int)(13 * s)},
          [&](Canvas& k) { __real__Z4textR6CanvasSt17basic_string_viewIcSt11char_traitsIcEEiid3Rgbd(k, t, x, y, s, {255, 255, 255}, 1); });
  if (&c == g_target && !g_ink.empty() && g_ink.back().kind == "text") { g_ink.back().ax = x; g_ink.back().ay = y; }
  return __real__Z4textR6CanvasSt17basic_string_viewIcSt11char_traitsIcEEiid3Rgbd(c, t, x, y, s, col, a);
}
void __real__Z11action_iconR6CanvasSt17basic_string_viewIcSt11char_traitsIcEERK4Rectd3Rgbd(Canvas&, std::string_view, const Rect&, double, Rgb, double);
void __wrap__Z11action_iconR6CanvasSt17basic_string_viewIcSt11char_traitsIcEERK4Rectd3Rgbd(Canvas& c, std::string_view n, const Rect& b, double s, Rgb col, double a) {
  measure(c, "icon", std::string(n), b, [&](Canvas& k) { __real__Z11action_iconR6CanvasSt17basic_string_viewIcSt11char_traitsIcEERK4Rectd3Rgbd(k, n, b, s, {255, 255, 255}, 1); });
  __real__Z11action_iconR6CanvasSt17basic_string_viewIcSt11char_traitsIcEERK4Rectd3Rgbd(c, n, b, s, col, a);
}
void __real__ZN6Canvas6bitmapEPK9BitmapDefiid3Rgbdb(Canvas*, const BitmapDef*, int, int, double, Rgb, double, bool);
void __wrap__ZN6Canvas6bitmapEPK9BitmapDefiid3Rgbdb(Canvas* c, const BitmapDef* bm, int x, int y, double cell, Rgb col, double a, bool flip) {
  if (bm)
    measure(*c, "bitmap", bm->name ? bm->name : "", {x, y, (int)std::ceil(bm->w * cell), (int)std::ceil(bm->h * cell)},
            [&](Canvas& k) { __real__ZN6Canvas6bitmapEPK9BitmapDefiid3Rgbdb(&k, bm, x, y, cell, {255, 255, 255}, 1, flip); });
  __real__ZN6Canvas6bitmapEPK9BitmapDefiid3Rgbdb(c, bm, x, y, cell, col, a, flip);
}
void __real__ZN6Canvas5cellsEPKciiiid3Rgbd(Canvas*, const char*, int, int, int, int, double, Rgb, double);
void __wrap__ZN6Canvas5cellsEPKciiiid3Rgbd(Canvas* c, const char* g, int gw, int gh, int x, int y, double cell, Rgb col, double a) {
  measure(*c, "cells", std::to_string(gw) + "x" + std::to_string(gh), {x, y, (int)std::ceil(gw * cell), (int)std::ceil(gh * cell)},
          [&](Canvas& k) { __real__ZN6Canvas5cellsEPKciiiid3Rgbd(&k, g, gw, gh, x, y, cell, {255, 255, 255}, 1); });
  __real__ZN6Canvas5cellsEPKciiiid3Rgbd(c, g, gw, gh, x, y, cell, col, a);
}
void __real__ZN6Canvas4maskEPK7MaskDefii3Rgbd(Canvas*, const MaskDef*, int, int, Rgb, double);
void __wrap__ZN6Canvas4maskEPK7MaskDefii3Rgbd(Canvas* c, const MaskDef* m, int x, int y, Rgb col, double a) {
  if (m) measure(*c, "mask", m->label ? m->label : "", {x, y, m->w, m->h}, [&](Canvas& k) { __real__ZN6Canvas4maskEPK7MaskDefii3Rgbd(&k, m, x, y, {255, 255, 255}, 1); });
  __real__ZN6Canvas4maskEPK7MaskDefii3Rgbd(c, m, x, y, col, a);
}
void __real__ZN6Canvas4fillEdddd3Rgbd(Canvas*, double, double, double, double, Rgb, double);
void __wrap__ZN6Canvas4fillEdddd3Rgbd(Canvas* c, double x, double y, double w, double h, Rgb col, double a) {
  if (c == g_target && a > 0.05 && w > 0 && h > 0) {
    measure(*c, "fill", "", {(int)std::floor(x), (int)std::floor(y), (int)std::ceil(w) + 1, (int)std::ceil(h) + 1},
            [&](Canvas& k) { __real__ZN6Canvas4fillEdddd3Rgbd(&k, x, y, w, h, {255, 255, 255}, 1); });
  }
  __real__ZN6Canvas4fillEdddd3Rgbd(c, x, y, w, h, col, a);
}
void __real__ZN6Canvas8corneredERK4Recti3Rgbd(Canvas*, const Rect&, int, Rgb, double);
void __wrap__ZN6Canvas8corneredERK4Recti3Rgbd(Canvas* c, const Rect& r, int cut, Rgb col, double a) {
  if (c == g_target && a > 0.05) g_ink.push_back({"box", std::to_string((int)std::lround(a * 100)), r});
  __real__ZN6Canvas8corneredERK4Recti3Rgbd(c, r, cut, col, a);
}
void __real__Z11draw_switchR6Canvasdddd3Rgbdbd(Canvas&, double, double, double, double, Rgb, double, bool, double);
void __wrap__Z11draw_switchR6Canvasdddd3Rgbdbd(Canvas& c, double x, double y, double cell, double k, Rgb on, double hv, bool en, double a) {
  measure(c, "switch", "", {(int)x, (int)y, (int)std::ceil(kSwitchW * cell), (int)std::ceil(kSwitchH * cell)},
          [&](Canvas& q) { __real__Z11draw_switchR6Canvasdddd3Rgbdbd(q, x, y, cell, k, {255, 255, 255}, 0, true, 1); });
  __real__Z11draw_switchR6Canvasdddd3Rgbdbd(c, x, y, cell, k, on, hv, en, a);
}
}

// ── the menus ─────────────────────────────────────────────────────
static const char* kind_name(PlayView::RK k) {
  static const char* n[] = {"Header", "Option", "Action", "Submenu", "Slider", "Fonts", "Stepper", "Segmented", "Hint", "Separator", "Links", "Toggle",
                            "SpeedBar", "Chips", "Brand", "Credit", "Profile", "Info", "CloseHeader", "Contacts", "Keys", "Tools", "VSlider", "Preview", "Picker", "Sample"};
  return n[(int)k];
}
static FILE* g_tsv = nullptr;
static bool g_png = false;
static std::string g_out;

static void save_png(const Canvas& c, const std::string& name) {
  std::string raw = "/tmp/audit_menus.rgba";
  FILE* f = std::fopen(raw.c_str(), "wb");
  std::vector<unsigned char> buf((size_t)c.w * c.h * 4);
  for (size_t i = 0; i < c.px.size(); ++i) {
    uint32_t v = c.px[i]; unsigned a = v >> 24;
    auto un = [&](unsigned x) { return a ? std::min(255u, (x * 255 + a / 2) / a) : 0u; };
    buf[i * 4] = (unsigned char)un((v >> 16) & 255); buf[i * 4 + 1] = (unsigned char)un((v >> 8) & 255);
    buf[i * 4 + 2] = (unsigned char)un(v & 255); buf[i * 4 + 3] = (unsigned char)a;
  }
  std::fwrite(buf.data(), 1, buf.size(), f);
  std::fclose(f);
  std::string cmd = "convert -size " + std::to_string(c.w) + "x" + std::to_string(c.h) + " -depth 8 rgba:" + raw + " -background '#203040' -flatten " + name;
  if (std::system(cmd.c_str()) != 0) std::fprintf(stderr, "convert failed\n");
}

static void run(PlayView& pv, double& now, double secs) {
  for (double t = 0; t < secs; t += 1 / 60.0) { now += 1 / 60.0; pv.tick(now, 1 / 60.0); pv.take_actions(); }
}
static std::string clean(std::string t) { for (auto& ch : t) if (ch == '\t' || ch == '\n') ch = ' '; return t; }

// one open menu: its rows and what was drawn in each
static void audit(PlayView& pv, double& now, const std::string& scene, const std::string& tag) {
  run(pv, now, 0.6);
  if (pv.panel_ == PlayView::Panel::None) { std::fprintf(stderr, "%s %s: no panel\n", scene.c_str(), tag.c_str()); return; }
  pv.phover_ = -1; pv.pfocus_ = -1; pv.kb_ = false;
  for (auto& h : pv.phov_) h = 0;
  g_ink.clear();
  g_target = &pv.panel_base_;
  pv.render_panel();
  g_target = nullptr;
  const double s = pv.panel_ == PlayView::Panel::Keys ? pv.keys_s_ : pv.list_panel() ? pv.cc_scale_ : pv.panel_draw_s();
  const Rect pr = pv.panel_rect();
  const Canvas& c = pv.panel_base_;
  // the panel's own line: scale, size, place in the window
  std::fprintf(g_tsv, "P\t%s\t%s\t%.2f\t%d\t%d\t%d\t%d\t%d\t%d\n", tag.c_str(), scene.c_str(), s, c.w, c.h, pr.x, pr.y, pv.w_, pv.h_);
  for (int i = 0; i < (int)pv.rows_.size(); ++i) {
    const auto& r = pv.rows_[i];
    std::fprintf(g_tsv, "R\t%s\t%s\t%d\t%s\t%d\t%d\t%d\t%d\t%d\t%s\t%s\n", tag.c_str(), scene.c_str(), i, kind_name(r.k), r.id, r.r.x, r.r.y, r.r.w, r.r.h,
                 clean(r.label).c_str(), clean(r.right).c_str());
  }
  for (auto& k : g_ink)
    std::fprintf(g_tsv, "I\t%s\t%s\t%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\n", tag.c_str(), scene.c_str(), k.kind.c_str(), clean(k.what).c_str(), k.box.x, k.box.y, k.box.w, k.box.h, k.ax, k.ay);
  if (g_png) save_png(c, g_out + "/" + tag + "-" + scene + ".png");
}

static void film_inputs(PlayView& pv, bool music) {
  pv.in.dur = 5000; pv.in.time = 1869; pv.in.volume = 70;
  if (music) {
    pv.in.audio_only = true; pv.in.cover_scene = true; pv.in.cover_art = true; pv.in.has_cover = true;
    pv.in.music_title = "Dream Sequence"; pv.in.music_artist = "Jane Remover"; pv.in.dur = 250; pv.in.time = 35;
    return;
  }
  Track a{1, "sub", "por", "", "subrip"}, b{2, "sub", "eng", "English SDH", "subrip"}, k{3, "sub", "jpn", "", "ass"};
  pv.in.subs = {a, b, k}; pv.in.sid = 1; pv.in.kind = SubKind::Text;
  Track x{1, "audio", "eng", "", "aac"}, y{2, "audio", "fre", "", "ac3"};
  pv.in.audios = {x, y}; pv.in.aid = 1;
  pv.in.smooth_available = true;
  pv.in.chapters = {{0, "Abertura"}, {95, "Capítulo 1"}, {1400, "O encontro na estação de trem"}, {3100, ""}, {4700, "Créditos"}};
  pv.in.chapters_revision = 1;
  MediaDetails md;
  md.name = "Paisagem ao pôr do sol (versão final).mkv"; md.bytes = 1834567890; md.duration = 5000;
  md.container = "matroska,webm"; md.width = 3840; md.height = 2160; md.fps = 23.976; md.video_codec = "hevc";
  md.audio_codec = "eac3"; md.channels = 6; md.sample_rate = 48000;
  pv.in.file_info = media_info_fields(md);
}

using PN = PlayView::Panel;
static void open(PlayView& pv, PN p, int btn, double& now) {
  pv.activity(now);
  pv.open_panel(p, btn, now);
}
static void open_page(PlayView& pv, int row_id, double& now) {  // a page of Settings, from its row
  open(pv, PN::Settings, PlayView::B_MORE, now);
  run(pv, now, 0.3);
  for (int i = 0; i < (int)pv.rows_.size(); ++i)
    if (pv.rows_[i].id == row_id) { pv.panel_click(i, -1, pv.rows_[i].r.cx(), now); return; }
  std::fprintf(stderr, "row %d not in Settings\n", row_id);
}

static void player(const std::string& tag, unsigned dpi, int ui, bool music, int W, int H) {
  const double s = ui_scale(dpi, W, H, ui);
  std::string t = tag + "-" + std::to_string(dpi) + "-ui" + std::to_string(ui_size_index(ui));
  struct Item { const char* name; PN p; int btn; int page; };
  std::vector<Item> items = {{"context", PN::Context, PlayView::B_NONE, 0}, {"more", PN::More, PlayView::B_MORE, 0},
                             {"settings", PN::Settings, PlayView::B_MORE, 0}, {"interface", PN::Interface, 0, PlayView::R_INTERFACE},
                             {"language", PN::Language, PlayView::B_MORE, 0}, {"about", PN::About, PlayView::B_MORE, 0},
                             {"keys", PN::Keys, PlayView::B_NONE, 0}};
  if (!music) {
    std::vector<Item> film = {{"cc", PN::CC, PlayView::B_CC, 0}, {"style", PN::Style, PlayView::B_CC, 0}, {"audio", PN::Audio, PlayView::B_AUDIO, 0},
                              {"speed", PN::Speed, PlayView::B_SPEED, 0}, {"fileinfo", PN::FileInfo, PlayView::B_MORE, 0},
                              {"chapters", PN::Chapters, PlayView::B_MORE, 0}};
    items.insert(items.end(), film.begin(), film.end());
  }
  for (auto& it : items) {
    PlayView pv;
    film_inputs(pv, music);
    pv.in.w = W; pv.in.h = H;
    pv.set_base_scale(ui_base_scale(dpi, ui));
    pv.resize(W, H, s);
    double now = 1;
    pv.reset(now); pv.changed(now);
    pv.set_hover_for_preview(W / 2, H - 20, now);
    run(pv, now, 0.3);
    if (it.page) open_page(pv, it.page, now);
    else open(pv, it.p, it.btn, now);
    audit(pv, now, std::string(music ? "music-" : "film-") + it.name, t);
  }
}

static void mini(const std::string& tag, unsigned dpi, int ui, bool music) {
  const double k = dpi / 96.0;
  const int cw = (int)std::lround((music ? 480 : 540) * k), ch = (int)std::lround(music ? 480 * k / 1.9 : 540 * k * 9 / 16);
  const double s = mini_scale(dpi, cw, ch, ui, music);
  std::string t = tag + "-" + std::to_string(dpi) + "-ui" + std::to_string(ui_size_index(ui));
  for (int which = 0; which < (music ? 1 : 2); ++which) {
    PlayView pv;
    film_inputs(pv, music);
    pv.in.mini_player = true;
    if (!music) { pv.in.mini_sub_size = 1; }
    else { for (uint32_t i = 0; i < 4; ++i) pv.in.queue.push_back({i + 1, "faixa " + std::to_string(i + 1) + ".mp3", i == 1, true}); pv.in.queue_active = true; pv.in.queue_revision = 1; }
    pv.in.w = cw; pv.in.h = ch;
    pv.set_base_scale(s);
    pv.resize(cw, ch, s);
    double now = 1;
    pv.reset(now); pv.changed(now);
    pv.set_hover_for_preview(cw / 2, ch - 10, now);
    run(pv, now, 0.3);
    pv.open_panel_for_preview(which == 0 ? 3 : 0, now);
    audit(pv, now, std::string(music ? "mini-music-" : "mini-film-") + (which == 0 ? "more" : "subs"), t);
  }
}

int main(int argc, char** argv) {
  g_out = argc > 1 ? argv[1] : "out";
  g_png = argc > 2 && std::string(argv[2]) == "png";
  std::system(("mkdir -p '" + g_out + "'").c_str());
  g_tsv = std::fopen((g_out + "/rows.tsv").c_str(), "w");
  for (int li = 0; li < i18n::kLangCount; ++li) {
    i18n::set_language((i18n::Lang)li);
    const std::string tag = i18n::tag((i18n::Lang)li);
    for (unsigned dpi : {96u, 168u})
      for (int idx = 0; idx < kUiSizes; ++idx) {
        const int ui = ui_size_choice(idx);
        const int W = (int)std::lround(1280 * dpi / 96.0), H = (int)std::lround(720 * dpi / 96.0);
        player(tag, dpi, ui, false, W, H);
        player(tag, dpi, ui, true, W, H);
        mini(tag, dpi, ui, false);
        mini(tag, dpi, ui, true);
      }
    std::fprintf(stderr, "%s\n", tag.c_str());
  }
  std::fclose(g_tsv);
}
