// Settings over the home: two pages, "video" and "music", first (each opens its own page with its
// choices and pictures), then the interface, the language and about Cue. In a player, Settings
// keeps that player's own choices (a film's; a song's), as before. Every language, the four interface sizes, small and large windows:
// the rows in their order, each page whole inside the window, back and Esc to Settings, the
// music page open for choices away from a song.
#define private public
#include "../src/ui/home.h"
#include "../src/ui/play.h"
#undef private
#include "../src/core/i18n.h"
#include <cassert>
#include <cstdio>
#include <vector>

using PV = PlayView;
static std::vector<int> ids(const PV& p) {
  std::vector<int> v;
  for (auto& r : p.rows_) if (r.k != PV::RK::Separator && r.k != PV::RK::Preview && r.k != PV::RK::Header) v.push_back(r.id);
  return v;
}
static int row_of(const PV& p, int id) { for (int i = 0; i < (int)p.rows_.size(); ++i) if (p.rows_[i].id == id) return i; return -1; }
static void run(PV& p, double& t, double secs) { for (double k = 0; k < secs; k += 1 / 60.0) { t += 1 / 60.0; p.tick(t, 1 / 60.0); } }
static void inside(const PV& p, int W, int H) {
  const Layer& L = p.layers[PV::L_PANEL];
  assert(L.visible && L.x >= 0 && L.y >= 0 && L.x + L.c.w <= W && L.y + L.c.h <= H);
  for (auto& r : p.rows_) assert(r.r.x >= 0 && r.r.r() <= p.pw_ + 1);
}
static const PV::Row& row(const PV& p, int id) { int i = row_of(p, id); assert(i >= 0); return p.rows_[i]; }

int main() {
  int n = 0;
  for (int lang = 0; lang < i18n::kLangCount; ++lang) {
    i18n::set_language((i18n::Lang)lang);
    for (double s : {1.5, 2.0, 3.0, 4.0})
      for (auto [aw, ah] : {std::pair{480, 300}, {640, 360}, {960, 540}, {760, 294}, {1280, 720}})
        for (int where = 0; where < 3; ++where)      // 0 a film, 1 a song, 2 over the home
          for (int ex = 0; ex < 2; ++ex) {          // the examples off and on
            const int W = (int)(aw * s), H = (int)(ah * s);
            PV p;
            p.in.settings_examples = ex;
            if (where == 1) { p.in.audio_only = true; p.in.cover_scene = true; p.in.cover_art = true; p.in.has_cover = true; p.in.music_title = "Dream Sequence"; }
            if (where == 2) { p.in.audio_only = true; p.in.cover_scene = false; p.in.dur = 0; }  // (the last file was a song: nothing plays now)
            else p.in.dur = 3000;
            p.in.w = W; p.in.h = H; p.set_base_scale(s);
            double t = 1; p.resize(W, H, s); p.reset(t); p.changed(t);
            HomeView hv;
            if (where == 2) { hv.set_size(W, H, s); hv.set_rows({}); p.open_settings_over_home(hv.settings_anchor(), t); }
            else p.open_panel(PV::Panel::Settings, PV::B_MORE, t);
            run(p, t, 0.6);
            assert(p.panel_ == PV::Panel::Settings);
            if (where < 2) {
              // a player: its own choices, as before (a film's; a song's), no pages
              const std::vector<int> film = {PV::R_AMBIENT, PV::R_AMBIENT_STR, PV::R_SMOOTH, PV::R_CHAPTERS_ON, PV::R_INTERFACE, PV::R_LANGUAGE, PV::R_ABOUT};
              const std::vector<int> song = {PV::R_MUSIC_LOOK, PV::R_MUSIC_COLOR, PV::R_COVER_TILT, PV::R_CHAPTERS_ON, PV::R_INTERFACE, PV::R_LANGUAGE, PV::R_ABOUT};
              assert(ids(p) == (where == 0 ? film : song));
              inside(p, W, H);
              ++n;
              continue;
            }
            // (15/11: "show pictures", the home's own choice, under the interface)
            const std::vector<int> want = {PV::R_VIDEO_SET, PV::R_MUSIC_SET, PV::R_INTERFACE, PV::R_HOME_COVERS, PV::R_LANGUAGE, PV::R_ABOUT};
            assert(ids(p) == want);
            assert(!row(p, PV::R_VIDEO_SET).label.empty() && std::string(row(p, PV::R_VIDEO_SET).icon) == "set-video" && std::string(row(p, PV::R_MUSIC_SET).icon) == "set-music");
            inside(p, W, H);
            // the video page
            p.panel_click(row_of(p, PV::R_VIDEO_SET), -1, 0, t); run(p, t, 0.4);
            assert(p.panel_ == PV::Panel::VideoSet);
            assert(ids(p) == (std::vector<int>{PV::R_AMBIENT, PV::R_AMBIENT_STR, PV::R_SMOOTH, PV::R_CHAPTERS_ON}));
            inside(p, W, H);
            assert(p.pw_ >= (int)std::lround(p.settings_w_ * p.panel_draw_s()) - 2 || p.panel_fit_ > 0);  // Settings' width
            p.key("esc", false, false, t); run(p, t, 0.3);
            assert(p.panel_ == PV::Panel::Settings);
            // the music page: open for choices on the home and in a film; a song's follow its scene
            p.panel_click(row_of(p, PV::R_MUSIC_SET), -1, 0, t); run(p, t, 0.4);
            assert(p.panel_ == PV::Panel::MusicSet);
            assert(ids(p) == (std::vector<int>{PV::R_MUSIC_LOOK, PV::R_MUSIC_COLOR, PV::R_COVER_TILT, PV::R_CHAPTERS_ON}));
            assert(row(p, PV::R_MUSIC_LOOK).enabled);
            inside(p, W, H);
            // back by its header, then the about page and back
            p.panel_click(row_of(p, PV::R_BACK_SETTINGS), -1, 0, t); run(p, t, 0.3);
            assert(p.panel_ == PV::Panel::Settings);
            p.key("esc", false, false, t); assert(!p.over_home());
            ++n;
          }
  }
  // a song whose scene cannot be drawn: its looks say why, as before
  {
    PV p; p.in.audio_only = true; p.in.cover_scene = false; p.in.dur = 200;
    double t = 1; p.resize(1280, 720, 2); p.reset(t);
    p.open_panel(PV::Panel::MusicSet, PV::B_MORE, t);
    assert(!row(p, PV::R_MUSIC_LOOK).enabled && !row(p, PV::R_MUSIC_LOOK).reason.empty());
  }
  std::printf("settings_pages_test: ok (%d)\n", n);
}
