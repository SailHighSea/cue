// The song's window in many sizes (Linux, no engine), framed as App frames it (the same steps as
// tools/preview_music.cpp), with checks: the interface's layers' ink outside the window, layers
// drawn over each other, the words over the cover, and (sweep) jumps while an edge is dragged.
// Build (from the repository root, objects of src/ui and src/core compiled natively):
//   g++ -std=c++20 -O1 -o build/audit_music tools/audit_music.cpp build/pv/*.o
// Run:
//   ./build/audit_music grid <dpi> <size 0..3> <lang 0..7> [shots dir]   every window of the grid
//   ./build/audit_music sweep <dpi> <size> <lang> <h> <w0> <w1> <step>    width dragged at a height
//   ./build/audit_music vsweep <dpi> <size> <lang> <w> <h0> <h1> <step>   height dragged at a width
//   ./build/audit_music one <dpi> <size> <lang> <w> <h> <out.png> [list] [long] [full]
// Window sizes are given in px at 100% and grown to the dpi, like a window on that screen.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "../src/core/audio_mode.h"
#include "../src/core/i18n.h"
#include "../src/core/ui_scale.h"
#include "../src/ui/play.h"

static void save_rgba(const Canvas& c, const std::string& name) {
  std::string raw = "/tmp/audit_music." + std::to_string(::getpid()) + ".rgba";
  FILE* f = std::fopen(raw.c_str(), "wb");
  if (!f) return;
  std::vector<unsigned char> buf((size_t)c.w * c.h * 4);
  for (size_t i = 0; i < c.px.size(); ++i) {
    uint32_t v = c.px[i];
    buf[i * 4 + 0] = (unsigned char)((v >> 16) & 255);
    buf[i * 4 + 1] = (unsigned char)((v >> 8) & 255);
    buf[i * 4 + 2] = (unsigned char)(v & 255);
    buf[i * 4 + 3] = 255;
  }
  std::fwrite(buf.data(), 1, buf.size(), f);
  std::fclose(f);
  std::string cmd = "convert -size " + std::to_string(c.w) + "x" + std::to_string(c.h) + " -depth 8 rgba:" + raw + " '" + name + "'";
  if (std::system(cmd.c_str()) != 0) std::fprintf(stderr, "convert failed\n");
  std::remove(raw.c_str());
}

static void run(PlayView& pv, double& now, double secs) {
  for (double t = 0; t < secs; t += 1 / 60.0) { now += 1 / 60.0; pv.tick(now, 1 / 60.0); pv.take_actions(); }
}

// the scene sketched: the cover (a mid-grey square with a white mark in a corner), its wings
static Canvas scene(const MusicScene& S, const MusicView& v, int w, int h, bool wings) {
  Canvas c;
  c.resize(w, h);
  const double cx = v.ox + (S.x + S.side / 2.0) * v.f, cy = v.oy + (S.y + S.side / 2.0) * v.f, hh = S.side / 2.0 * v.f;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      uint32_t col = 0xFF000000u;
      const double sx = (x - v.ox) / v.f, sy = (y - v.oy) / v.f;
      bool wing = wings && sy >= S.wing_y() && sy < S.wing_y() + S.wing_h &&
                  ((sx >= S.left_x() && sx < S.left_x() + S.wing_w) || (sx >= S.right_x() && sx < S.right_x() + S.wing_w));
      if (wing && (((int)std::floor(sx / 11) + (int)std::floor(sy / 11)) & 1)) col = 0xFF5A5A5Au;
      const double qx = x - cx, qy = y - cy;
      if (std::fabs(qx) < hh && std::fabs(qy) < hh) {
        col = 0xFF2E3440u;
        if (qx > hh * 0.6 && qy < -hh * 0.6) col = 0xFFE0E0E0u;
      }
      c.px[(size_t)y * w + x] = col;
    }
  return c;
}

struct Shot {
  MusicFrame fr{};
  MusicView v;
  int cmode = 0;
  bool listed = false;
  Rect cover;  // the cover on screen
};

static const char* frame_name(MusicFrame f) {
  switch (f) {
    case MusicFrame::Scene: return "Scene";
    case MusicFrame::Tall: return "Tall";
    case MusicFrame::Cover: return "Compacto";
    case MusicFrame::Card: return "Card";
    case MusicFrame::Centre: return "Centre";
    case MusicFrame::Micro: return "Micro";
    case MusicFrame::Strip: return "Strip";
    default: return "?";
  }
}

struct Opts { bool list = false, long_words = false, full = false, hover = true, one_song = false; };

static Shot lay(PlayView& pv, unsigned dpi, int ui_size, int cw, int ch, const Opts& o, double& now) {
  Shot r;
  const double s = ui_scale(dpi, cw, ch, ui_size);
  MusicFrame fr = music_frame_for(cw, ch, music_frame_scale(dpi, cw, ch, ui_size), MusicFrame::Card, dpi / 96.0);
  double ps = s;
  if (music_frame_stacked(fr)) ps = std::max(ps, music_compact_scale(s, cw, ch, ui_size, ui_base_scale(dpi, ui_size)));
  if (fr == MusicFrame::Micro || fr == MusicFrame::Strip) ps = std::max(ps, music_small_scale(dpi, cw, ch, ui_size, fr == MusicFrame::Strip));
  if (fr == MusicFrame::Scene) ps = std::max(ps, music_wide_scale(dpi, cw, ch, ui_size));
  pv.in.audio_only = true; pv.in.cover_scene = true; pv.in.cover_art = true; pv.in.has_cover = true;
  pv.in.fullscreen = o.full;
  pv.in.controls_free = std::getenv("CUE_FREE") != nullptr;  // the bar without its background (music's bar along the foot)
  pv.in.dur = 3725; pv.in.time = 1489; pv.in.volume = 70;
  pv.in.music_title = o.long_words ? "Uma música com um título muito, muito longo que não cabe em lugar nenhum da janela" : "Ré";
  pv.in.music_artist = o.long_words ? "Orquestra Sinfônica de São José do Rio Preto e Convidados Especiais" : "A";
  if (!o.one_song) {
    const char* names[] = {"Jane Remover - Beauty Sleep (320 Kbps).mp3", "Ice Palace Adolescence.mp3",
                           "Uma faixa com um nome comprido demais para caber numa linha da lista (versão estendida).mp3",
                           "Jane Remover - Music Baby (audio).mp3", "Number Teddie - DNA.mp3", "x.mp3", "Mais uma.mp3", "E outra.mp3"};
    for (uint32_t i = 0; i < 8; ++i) pv.in.queue.push_back({i + 1, names[i], i == 1, true});
    pv.in.queue_active = true; pv.in.has_next = pv.in.has_prev = true;
    pv.in.queue_revision = 1;
  }
  pv.in.w = cw; pv.in.h = ch;
  pv.set_base_scale(s);
  pv.resize(cw, ch, ps);
  pv.reset(now);
  pv.changed(now);
  if (o.hover) pv.set_hover_for_preview(cw / 2, ch / 2, now);
  MusicView v;
  auto frame_it = [&](bool listing) {
    for (int pass = 0; pass < 3; ++pass) {
      run(pv, now, 0.35);
      int top = ch;
      Rect d = pv.blur_cloud().controls;
      if (d.h > 0 && d.cy() > ch / 2) top = d.y;
      Rect lc = listing ? pv.list_cover() : Rect{};
      if (lc.w > 0) {
        v = MusicView{}; v.frame = fr; v.f = lc.w / (double)pv.in.music_scene.side;
        v.ox = lc.x - pv.in.music_scene.x * v.f; v.oy = lc.y - pv.in.music_scene.y * v.f;
      } else if (Rect cc = pv.card_cover(); pv.music_card_mode() > 0 && cc.w > 0) {
        v = MusicView{}; v.frame = fr; v.f = cc.w / (double)pv.in.music_scene.side;
        v.ox = cc.x - pv.in.music_scene.x * v.f; v.oy = cc.y - pv.in.music_scene.y * v.f;
      } else if (fr == MusicFrame::Centre) v = music_view(pv.in.music_scene, fr, cw, ch, top, pv.music_centre_below(), pv.music_stack_above(), -1, pv.music_list_beside());
      else if (fr != MusicFrame::Scene && pv.music_stacked()) v = music_view(pv.in.music_scene, fr, cw, ch, top, pv.music_stack_below(), pv.music_stack_above());
      else v = music_view(pv.in.music_scene, fr, cw, ch, top, -1, 0, pv.music_words_w());
      v.frame = fr;
      pv.in.music_view = v;
      pv.changed(now);
    }
  };
  frame_it(false);
  run(pv, now, 0.7);
  if (o.list && pv.music_card_mode() == 0) {
    pv.open_panel_for_preview(10, now);
    frame_it(true);
    run(pv, now, 0.7);
    r.listed = true;
  }
  if (o.hover) { pv.set_hover_for_preview(cw / 2, ch / 2, now); run(pv, now, 0.3); }
  r.fr = fr; r.v = v; r.cmode = pv.music_card_mode();
  const MusicScene& S = pv.in.music_scene;
  r.cover = Rect{(int)std::lround(v.ox + S.x * v.f), (int)std::lround(v.oy + S.y * v.f), (int)std::lround(S.side * v.f), (int)std::lround(S.side * v.f)};
  return r;
}

static Canvas compose(PlayView& pv, const Canvas& base) {
  Canvas fr = base;
  for (auto& L : pv.layers) if (L.visible) fr.blit(L.c, L.x, L.y);
  return fr;
}

// ink: a layer's pixels drawn at least this opaque
static inline bool ink(uint32_t p) { return (p >> 24) > 200; }  // (solid: glyphs and lines, not veils or shadows)

struct Box { int x0 = 1 << 30, y0 = 1 << 30, x1 = -(1 << 30), y1 = -(1 << 30); bool any() const { return x1 >= x0; } };

static Box ink_box(const Layer& L) {
  Box b;
  if (!L.visible) return b;
  for (int y = 0; y < L.c.h; ++y)
    for (int x = 0; x < L.c.w; ++x)
      if (ink(L.c.px[(size_t)y * L.c.w + x])) {
        b.x0 = std::min(b.x0, L.x + x); b.x1 = std::max(b.x1, L.x + x);
        b.y0 = std::min(b.y0, L.y + y); b.y1 = std::max(b.y1, L.y + y);
      }
  return b;
}

static int outside(const Layer& L, int w, int h) {
  if (!L.visible) return 0;
  int n = 0;
  for (int y = 0; y < L.c.h; ++y)
    for (int x = 0; x < L.c.w; ++x) {
      const int X = L.x + x, Y = L.y + y;
      if ((X < 0 || Y < 0 || X >= w || Y >= h) && ink(L.c.px[(size_t)y * L.c.w + x])) ++n;
    }
  return n;
}

static int overlap(const Layer& A, const Layer& B) {
  if (!A.visible || !B.visible) return 0;
  const int x0 = std::max(A.x, B.x), y0 = std::max(A.y, B.y);
  const int x1 = std::min(A.x + A.c.w, B.x + B.c.w), y1 = std::min(A.y + A.c.h, B.y + B.c.h);
  int n = 0;
  for (int y = y0; y < y1; ++y)
    for (int x = x0; x < x1; ++x)
      if (ink(A.c.px[(size_t)(y - A.y) * A.c.w + (x - A.x)]) && ink(B.c.px[(size_t)(y - B.y) * B.c.w + (x - B.x)])) ++n;
  return n;
}

static int over_rect(const Layer& A, const Rect& r) {
  if (!A.visible || r.w <= 0) return 0;
  int n = 0;
  for (int y = std::max(r.y, A.y); y < std::min(r.y + r.h, A.y + A.c.h); ++y)
    for (int x = std::max(r.x, A.x); x < std::min(r.x + r.w, A.x + A.c.w); ++x)
      if (ink(A.c.px[(size_t)(y - A.y) * A.c.w + (x - A.x)])) ++n;
  return n;
}

static const char* lname(int i) {
  static const char* n[] = {"ART", "MUSIC", "REST", "NEXT", "DOCK", "PANEL", "TOAST", "SKIP", "TIP", "G0", "G1", "G2", "HINT", "WIN", "FLASH", "BACK", "IRIS"};
  return i < 17 ? n[i] : "?";
}

static int report(PlayView& pv, const Shot& r, int cw, int ch, const char* tag) {
  int issues = 0;
  const int watch[] = {PlayView::L_MUSIC, PlayView::L_REST, PlayView::L_DOCK, PlayView::L_PANEL, PlayView::L_WIN, PlayView::L_BACK, PlayView::L_NEXT, PlayView::L_TIP};
  for (int i : watch) {
    int n = outside(pv.layers[i], cw, ch);
    if (n > 0) { std::printf("%s OUT %s %d px\n", tag, lname(i), n); ++issues; }
  }
  for (int a = 0; a < 8; ++a)
    for (int b = a + 1; b < 8; ++b) {
      int n = overlap(pv.layers[watch[a]], pv.layers[watch[b]]);
      if (n > 2) { std::printf("%s OVER %s/%s %d px\n", tag, lname(watch[a]), lname(watch[b]), n); ++issues; }
    }
  // the words never over the cover (the close compositions draw the bar on it on purpose)
  if (!r.listed && r.cmode == 0) {
    Rect inner{r.cover.x + 2, r.cover.y + 2, r.cover.w - 4, r.cover.h - 4};
    int n = over_rect(pv.layers[PlayView::L_MUSIC], inner);
    if (n > 2) { std::printf("%s COVER/MUSIC %d px\n", tag, n); ++issues; }
    if (r.fr == MusicFrame::Centre || r.fr == MusicFrame::Scene || r.fr == MusicFrame::Tall) {
      n = over_rect(pv.layers[PlayView::L_DOCK], inner);
      if (n > 2) { std::printf("%s COVER/DOCK %d px\n", tag, n); ++issues; }
    }
  }
  // the cover inside the window (the scene's own crop aside: the cover itself should show whole)
  if (r.cover.w > 0 && (r.cover.x < -1 || r.cover.y < -1 || r.cover.x + r.cover.w > cw + 1 || r.cover.y + r.cover.h > ch + 1)) {
    std::printf("%s COVER-CUT %d,%d %dx%d\n", tag, r.cover.x, r.cover.y, r.cover.w, r.cover.h); ++issues;
  }
  return issues;
}

int main(int argc, char** argv) {
  if (argc < 5) { std::fprintf(stderr, "usage: see the header\n"); return 2; }
  const std::string mode = argv[1];
  const unsigned dpi = (unsigned)std::atoi(argv[2]);
  const int ui_size = ui_size_choice(std::atoi(argv[3]));
  i18n::set_language((i18n::Lang)std::clamp(std::atoi(argv[4]), 0, i18n::kLangCount - 1));
  auto phys = [&](int px) { return (int)std::lround(px * dpi / 96.0); };
  if (mode == "one") {
    const int cw = phys(std::atoi(argv[5])), ch = phys(std::atoi(argv[6]));
    Opts o;
    for (int i = 8; i < argc; ++i) {
      if (!std::strcmp(argv[i], "list")) o.list = true;
      if (!std::strcmp(argv[i], "long")) o.long_words = true;
      if (!std::strcmp(argv[i], "full")) o.full = true;
      if (!std::strcmp(argv[i], "rest")) o.hover = false;
      if (!std::strcmp(argv[i], "single")) o.one_song = true;
    }
    auto pv = std::make_unique<PlayView>();
    double now = 1;
    Shot r = lay(*pv, dpi, ui_size, cw, ch, o, now);
    char tag[96];
    std::snprintf(tag, sizeof tag, "%dx%d %s", cw, ch, frame_name(r.fr));
    report(*pv, r, cw, ch, tag);
    if (std::getenv("LAYERS")) for (int i = 0; i < PlayView::L_COUNT; ++i) if (pv->layers[i].visible) {
      Box b = ink_box(pv->layers[i]);
      std::printf("layer %s at %d,%d %dx%d ink %d,%d-%d,%d\n", lname(i), pv->layers[i].x, pv->layers[i].y, pv->layers[i].c.w, pv->layers[i].c.h, b.x0, b.y0, b.x1, b.y1);
      Canvas one; one.resize(cw, ch); for (auto& q : one.px) q = 0xFF202020u; one.blit(pv->layers[i].c, pv->layers[i].x, pv->layers[i].y);
      save_rgba(one, std::string(argv[7]) + "." + lname(i) + ".png");
    }
    if (std::getenv("FOOTER") && pv->layers[PlayView::L_PANEL].visible) {  // the list's foot alone
      const Layer& P = pv->layers[PlayView::L_PANEL];
      const int fh = std::min(P.c.h, (int)std::lround(40 * dpi / 96.0 * 1.5));
      Canvas f; f.resize(P.c.w, fh); for (auto& q : f.px) q = 0xFF000000u;
      Canvas tmp = P.c; f.blit(tmp, 0, fh - P.c.h);
      save_rgba(f, argv[7]); return 0;
    }
    save_rgba(compose(*pv, scene(pv->in.music_scene, r.v, cw, ch, !(r.listed && pv->list_cover().w > 0) && !(r.cmode > 0 && !pv->music_card_wings()))), argv[7]);
    return 0;
  }
  if (mode == "grid") {
    const std::string shots = argc > 5 ? argv[5] : "";
    struct W { int w, h; };
    const W wins[] = {
        {220, 220}, {250, 250}, {280, 280}, {300, 300}, {340, 340}, {300, 260}, {260, 300},  // Micro and around it
        {460, 96}, {520, 110}, {700, 120}, {900, 96}, {1200, 120}, {1600, 110}, {460, 140}, {800, 150},  // strips
        {320, 340}, {400, 420}, {480, 400}, {420, 480},  // Compacto
        {600, 160}, {800, 200}, {1000, 260}, {1400, 300}, {1920, 360}, {700, 300},  // wide and low
        {300, 600}, {360, 800}, {500, 900}, {420, 1040}, {600, 1000},  // tall
        {540, 540}, {720, 720}, {1000, 1000},  // square
        {640, 400}, {760, 480}, {820, 620}, {960, 540}, {1100, 700}, {1280, 720}, {1600, 900},  // ordinary
        {1920, 1009}, {1707, 912}, {2194, 1186}, {1920, 1032},  // maximised on 1080p 100%, 1440p 150%, 4K 175%, 4K 200%
    };
    int total = 0, n = 0;
    for (const W& wn : wins)
      for (int lst = 0; lst < 2; ++lst)
        for (int lw = 0; lw < 2; ++lw) {
          const int cw = phys(wn.w), ch = phys(wn.h);
          Opts o; o.list = lst; o.long_words = lw;
          auto pv = std::make_unique<PlayView>();
          double now = 1;
          Shot r = lay(*pv, dpi, ui_size, cw, ch, o, now);
          if (lst && !r.listed) continue;  // (the small windows' cards: no list)
          char tag[128];
          std::snprintf(tag, sizeof tag, "d%u s%s l%s %dx%d(%dx%d) %s%s%s", dpi, argv[3], argv[4], wn.w, wn.h, cw, ch, frame_name(r.fr), lst ? " list" : "", lw ? " long" : " short");
          total += report(*pv, r, cw, ch, tag);
          ++n;
          if (!shots.empty()) {
            char name[256];
            std::snprintf(name, sizeof name, "%s/%04dx%04d%s%s.png", shots.c_str(), wn.w, wn.h, lst ? "-list" : "", lw ? "-long" : "-short");
            save_rgba(compose(*pv, scene(pv->in.music_scene, r.v, cw, ch, !(r.listed && pv->list_cover().w > 0) && !(r.cmode > 0 && !pv->music_card_wings()))), name);
          }
        }
    // full screen (1080p at 100%, 4K at 175% and 200%)
    std::printf("d%u s%s l%s: %d layouts, %d issues\n", dpi, argv[3], argv[4], n, total);
    return 0;
  }
  if (mode == "sweep" || mode == "vsweep") {
    const bool vert = mode == "vsweep";
    const int fixed = std::atoi(argv[5]), a0 = std::atoi(argv[6]), a1 = std::atoi(argv[7]), st = std::atoi(argv[8]);
    struct Prev { bool ok = false; MusicFrame fr{}; Box words, dock, win; Rect cover; } prev;
    for (int a = a0; a <= a1; a += st) {
      const int cw = phys(vert ? fixed : a), ch = phys(vert ? a : fixed);
      if (ch < phys(220) && cw < phys(460)) continue;
      Opts o;
      auto pv = std::make_unique<PlayView>();
      double now = 1;
      Shot r = lay(*pv, dpi, ui_size, cw, ch, o, now);
      char tag[128];
      std::snprintf(tag, sizeof tag, "d%u s%s l%s %dx%d %s", dpi, argv[3], argv[4], cw, ch, frame_name(r.fr));
      report(*pv, r, cw, ch, tag);
      Box words = ink_box(pv->layers[PlayView::L_MUSIC]), dock = ink_box(pv->layers[PlayView::L_DOCK]), win = ink_box(pv->layers[PlayView::L_WIN]);
      if (prev.ok && prev.fr == r.fr) {
        // within one composition a step of the edge should move things by about the step
        const int step = phys(st);
        auto jump = [&](const char* what, const Box& p, const Box& q) {
          if (!p.any() || !q.any()) return;
          const int d = std::max({std::abs(q.x0 - p.x0), std::abs(q.y0 - p.y0), std::abs(q.x1 - p.x1), std::abs(q.y1 - p.y1)});
          if (d > step * 2 + 6) std::printf("%s JUMP %s %d px (step %d)\n", tag, what, d, step);
        };
        jump("words", prev.words, words); jump("dock", prev.dock, dock);
        const int dc = std::max({std::abs(r.cover.x - prev.cover.x), std::abs(r.cover.y - prev.cover.y), std::abs(r.cover.w - prev.cover.w)});
        if (dc > step * 2 + 6) std::printf("%s JUMP cover %d px (step %d)\n", tag, dc, step);
      }
      std::printf("%s cover %d,%d %d words %d,%d-%d,%d dock %d,%d-%d,%d\n", tag, r.cover.x, r.cover.y, r.cover.w, words.x0, words.y0, words.x1, words.y1, dock.x0, dock.y0, dock.x1, dock.y1);
      prev = {true, r.fr, words, dock, win, r.cover};
    }
    return 0;
  }
  return 2;
}
