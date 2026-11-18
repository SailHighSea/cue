// The film's player bar over a real picture, at a real window size, DPI and interface size,
// for comparing against a mockup (the scale is the app's own rule: ui_scale + portrait_scale).
// Build (repository root):
//   g++ -std=c++20 -O1 -o preview_dock tools/preview_dock.cpp src/ui/*.cpp src/core/*.cpp
// Use: preview_dock <picture.rgb WxH 8-bit RGB> W H dpi choice free out.png [time dur paused lang volume hover_x hover_y panel]
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "../src/ui/play.h"
#include "../src/core/i18n.h"
#include "../src/core/ui_scale.h"

static void save_canvas(const Canvas& c, const std::string& name) {
  std::vector<unsigned char> buf((size_t)c.w * c.h * 3);
  for (size_t i = 0; i < c.px.size(); ++i) {
    uint32_t v = c.px[i];
    buf[i * 3] = (v >> 16) & 255; buf[i * 3 + 1] = (v >> 8) & 255; buf[i * 3 + 2] = v & 255;
  }
  const std::string raw = name + ".rgb";
  if (FILE* f = std::fopen(raw.c_str(), "wb")) { std::fwrite(buf.data(), 1, buf.size(), f); std::fclose(f); }
  const std::string cmd = "convert -size " + std::to_string(c.w) + "x" + std::to_string(c.h) + " -depth 8 rgb:'" + raw + "' '" + name + "' && rm '" + raw + "'";
  if (std::system(cmd.c_str()) != 0) std::fprintf(stderr, "convert failed\n");
}

int main(int argc, char** argv) {
  if (argc < 8) { std::fprintf(stderr, "use: picture.rgb W H dpi choice free out.png [time dur paused lang volume hover_x hover_y panel]\n"); return 2; }
  const std::string pic = argv[1];
  const int W = std::atoi(argv[2]), H = std::atoi(argv[3]);
  const unsigned dpi = (unsigned)std::atoi(argv[4]);
  const int choice = std::atoi(argv[5]);
  const bool free_style = std::atoi(argv[6]) != 0;
  const std::string out = argv[7];
  const double t = argc > 8 ? std::atof(argv[8]) : 101, dur = argc > 9 ? std::atof(argv[9]) : 362;
  const bool paused = argc > 10 && std::atoi(argv[10]) != 0;
  if (argc > 11) i18n::set_language((i18n::Lang)std::atoi(argv[11]));
  const double s = ui_scale(dpi, W, H, choice);
  const double ps = portrait_scale(s, W, H, choice);
  PlayView pv;
  pv.in.w = W; pv.in.h = H; pv.in.s = ps;
  pv.in.dur = dur; pv.in.time = t; pv.in.volume = argc > 12 ? std::atof(argv[12]) : 70; pv.in.paused = paused;
  pv.in.ui_scale = choice; pv.in.controls_free = free_style;
  Track a{1, "sub", "por", "", "subrip"}, b{2, "sub", "eng", "English SDH", "subrip"};
  a.selected = true;
  pv.in.subs = {a, b}; pv.in.sid = 1; pv.in.kind = SubKind::Text;
  pv.in.loop = true;
  pv.set_base_scale(s);
  pv.resize(W, H, ps);
  double now = 1;
  pv.reset(now);
  pv.changed(now);
  pv.set_hover_for_preview(W / 2, H / 2, now);
  for (int i = 0; i < 90; ++i) { now += 1 / 60.0; pv.tick(now, 1 / 60.0); pv.take_actions(); }
  if (argc > 14) {  // then rest on a button (its hover settled, before its tooltip shows)
    pv.set_hover_for_preview(std::atoi(argv[13]), std::atoi(argv[14]), now);
    for (int i = 0; i < 15; ++i) { now += 1 / 60.0; pv.tick(now, 1 / 60.0); pv.take_actions(); }
  }
  if (argc > 15 && std::atoi(argv[15]) >= 0) {  // a menu open: 0 subtitles, 1 style, 3 "...", 4 speed (open_panel_for_preview)
    pv.open_panel_for_preview(std::atoi(argv[15]), now);
    for (int i = 0; i < 40; ++i) { now += 1 / 60.0; pv.tick(now, 1 / 60.0); pv.take_actions(); }
  }
  Canvas fr; fr.resize(W, H);
  std::vector<unsigned char> buf((size_t)W * H * 3);
  if (FILE* f = std::fopen(pic.c_str(), "rb")) { size_t n = std::fread(buf.data(), 1, buf.size(), f); (void)n; std::fclose(f); }
  for (size_t i = 0; i < fr.px.size(); ++i)
    fr.px[i] = 0xFF000000u | (buf[i * 3] << 16) | (buf[i * 3 + 1] << 8) | buf[i * 3 + 2];
  for (auto& L : pv.layers) if (L.visible) fr.blit(L.c, L.x, L.y);
  save_canvas(fr, out);
  std::printf("scale %.1f (play %.1f)\n", s, ps);
  return 0;
}
