// A panel over the subtitle: the line stays where it is and dims (colours only), and comes back
// when it closes. The style's footer holds the way back, which leaves the line's place alone.
#include "../src/core/i18n.h"
#include "../src/core/sub_style.h"
#define private public
#include "../src/ui/play.h"
#undef private
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

static double prop_alpha(const std::vector<PropSet>& v, const char* name) {
  for (auto& p : v)
    if (p.names[0] == name) { double r, g, b, a; std::sscanf(p.value.c_str(), "%lf/%lf/%lf/%lf", &r, &g, &b, &a); return a; }
  return -1;
}

int main() {
  i18n::set_language(i18n::Lang::PtBR);
  // the colours scale with the dim; nothing else of the style changes
  {
    SubStyle st;
    auto full = sub_style_props(st, SubKind::Text), dim = sub_style_props(st, SubKind::Text, 1.0, 0.25);
    assert(full.size() == dim.size());
    assert(std::fabs(prop_alpha(dim, "sub-color") - 0.25 * prop_alpha(full, "sub-color")) < 1e-3);
    assert(std::fabs(prop_alpha(dim, "sub-back-color") - 0.25 * prop_alpha(full, "sub-back-color")) < 1e-3);
    for (size_t i = 0; i < full.size(); ++i)
      if (full[i].names[0] != "sub-color" && full[i].names[0] != "sub-back-color" && full[i].names[0] != "sub-shadow-color" &&
          full[i].names[0] != "sub-outline-color")
        assert(full[i].value == dim[i].value);
    st.bg_op = 0;
    auto o = sub_style_props(st, SubKind::Text, 1.0, 0.5), of = sub_style_props(st, SubKind::Text);
    assert(std::fabs(prop_alpha(o, "sub-outline-color") - 0.5 * prop_alpha(of, "sub-outline-color")) < 1e-3);
  }
  int dimmed = 0;
  for (auto size : {std::pair{640, 360}, std::pair{960, 540}, std::pair{1920, 1080}}) {
    PlayView p;
    p.in.dur = 3000; p.in.time = 100;
    Track a{1, "sub", "por", "", "subrip"};
    a.selected = true;
    p.in.subs = {a}; p.in.sid = 1; p.in.kind = SubKind::Text; p.in.sub_shown = true; p.in.sub_lines = 2; p.in.sub_chars = 60;
    p.resize(size.first, size.second, size.first > 1000 ? 2 : 1);
    double t = 1;
    p.reset(t);
    auto run = [&](double secs) { for (double k = 0; k < secs; k += 1 / 60.0) { t += 1 / 60.0; p.activity(t); p.tick(t, 1 / 60.0); } };
    run(0.5);
    p.take_actions();
    const double lift_before = p.subtitle_lift_.target;
    // the subtitles' menu (a big panel at the bar's right end, standing on it)
    p.open_panel(PlayView::Panel::CC, PlayView::B_CC, t);
    run(0.6);
    const double want = p.wanted_subtitle_dim();
    double last = -1;
    int dims = 0;
    for (auto& ac : p.take_actions()) {
      assert(ac.a != PA::SubLift || std::fabs(p.subtitle_lift_.target - lift_before) < 1e-9);  // never rises for a panel
      if (ac.a == PA::SubDim) { last = ac.v; ++dims; }
    }
    if (want < 1) {
      ++dimmed;
      assert(std::fabs(last - PlayView::kSubDim) < 1e-9 && dims >= 2 && dims <= 20);  // a few visible steps, then exact
    } else {
      assert(dims == 0);  // not under the panel: nothing written
    }
    p.close_panel();
    run(0.6);
    last = -1;
    for (auto& ac : p.take_actions()) if (ac.a == PA::SubDim) last = ac.v;
    if (want < 1) assert(last == 1);
    // the picture's own menu never dims it
    p.open_panel(PlayView::Panel::Context, PlayView::B_NONE, t);
    run(0.4);
    assert(p.wanted_subtitle_dim() == 1);
    p.close_panel();
    run(0.4);
    // original ASS styling and image subtitles: the engine does not colour them, nothing to dim
    p.in.kind = SubKind::Bitmap;
    p.open_panel(PlayView::Panel::CC, PlayView::B_CC, t);
    run(0.4);
    assert(p.wanted_subtitle_dim() == 1);
    p.close_panel();
    run(0.4);
    p.in.kind = SubKind::Text;
  }
  assert(dimmed >= 2);  // (the menu does stand over the line in these windows)
  // the restore leaves the line where the person put it, and is in the style's footer
  {
    PlayView p;
    p.in.dur = 3000;
    Track a{1, "sub", "por", "", "subrip"};
    p.in.subs = {a}; p.in.sid = 1; p.in.kind = SubKind::Text;
    p.resize(1280, 720, 2);
    double t = 1;
    p.reset(t);
    p.in.style.size = 60; p.in.style.pos = 40;
    p.open_panel(PlayView::Panel::Style, PlayView::B_CC, t);
    int links = -1;
    for (int i = 0; i < (int)p.rows_.size(); ++i) if (p.rows_[i].id == PlayView::R_LINKS) links = i;
    assert(links >= 0 && p.rows_[links].segs.size() == 2 && (p.rows_[links].seg & 2));
    assert(p.rows_[links].parts[1].x > p.rows_[links].parts[0].r());  // (the way back on the right)
    p.panel_click(links, 1, 0, t);
    assert(p.in.style.size == SubStyle{}.size && std::fabs(p.in.style.pos - 40) < 1e-9);
    // pressed again right after: undone
    for (int i = 0; i < (int)p.rows_.size(); ++i) if (p.rows_[i].id == PlayView::R_LINKS) links = i;
    p.panel_click(links, 1, 0, t);
    assert(p.in.style.size == 60 && std::fabs(p.in.style.pos - 40) < 1e-9);
    // the subtitles' menu no longer has its own restore row
    p.open_panel(PlayView::Panel::CC, PlayView::B_CC, t);
    for (auto& r : p.rows_) assert(r.id != PlayView::R_STYLE_RESET);
    // only the line's place moved: nothing to restore
    p.in.style = SubStyle{}; p.in.style.pos = 30; p.in.default_style = SubStyle{};
    assert(!p.style_customized());
  }
  std::puts("Subtitle under a panel: stays put and dims in a few colour steps, comes back on close; restore lives in the style footer and keeps the line's place");
}
