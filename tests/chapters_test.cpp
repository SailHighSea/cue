// Chapters on the timeline: the track breaks where each chapter begins (one art pixel, never two
// breaks closer than four), only with two chapters or more; the timeline's preview names the
// chapter of the moment it shows, above the time; a long name ends in "…" within the window.
// Page Up / Page Down step through them (back to this one's start, or the one before within
// 3 s of it; presses in a row count from the last jump) with a note naming where it went; "…"
// lists them (only when there are some), the one playing marked, a click jumps there.
#define private public
#include "../src/ui/play.h"
#undef private
#include "../src/core/i18n.h"
#include "../src/core/store.h"
#include <filesystem>
#include <cassert>
#include <cstdio>

// the timeline's row: a break is where the bar's own background shows through the line (the
// pixel is the one just above the track)
static bool clear_at(const PlayView& p, int x) {
  const auto& L = p.layers[PlayView::L_DOCK];
  const int y = p.tl_.y, above = p.tl_.y - (int)(3 * p.s_);
  return L.c.px[(size_t)y * L.c.w + x] == L.c.px[(size_t)above * L.c.w + x];
}
static int gaps_in_track(const PlayView& p) {
  int gaps = 0;
  bool in_gap = false;
  for (int x = p.tl_.x; x < p.tl_.r(); ++x) {
    const bool clear = clear_at(p, x);
    if (clear && !in_gap) ++gaps;
    in_gap = clear;
  }
  return gaps;
}

int main() {
  i18n::set_language(i18n::Lang::PtBR);
  // chapter_at: the last one starting at or before the moment
  std::vector<Chapter> c = {{0, "a"}, {10, "b"}, {20, "c"}};
  assert(chapter_at(c, -1) == -1 && chapter_at(c, 0) == 0 && chapter_at(c, 9.99) == 0 && chapter_at(c, 10) == 1 && chapter_at(c, 99) == 2);
  assert(chapter_at({}, 5) == -1 && !chapters_shown({{0, "only"}}) && chapters_shown(c));
  std::vector<Chapter> late = {{30, "x"}, {60, "y"}};
  assert(chapter_at(late, 10) == -1);  // before the first: none

  for (double s : {1.5, 2.0, 3.0}) {
    const int W = (int)(640 * s), H = (int)(360 * s);
    auto make = [&](std::vector<Chapter> chapters) {
      PlayView p;
      p.in.dur = 7020; p.in.time = 2500;  // (the playhead away from every break)
      p.in.chapters = std::move(chapters);
      p.in.chapters_revision = 1;
      p.resize(W, H, s);
      double now = 1;
      p.reset(now); p.changed(now);
      for (int i = 0; i < 40; ++i) { now += 1 / 60.0; p.activity(now); p.tick(now, 1 / 60.0); }
      return p;
    };
    // none, one, eight
    assert(gaps_in_track(make({})) == 0);
    assert(gaps_in_track(make({{0, "Abertura"}})) == 0);
    const std::vector<Chapter> eight = {{0, "Abertura"}, {370, "A estrada"}, {1180, "O farol"}, {1860, "Tempestade"},
                                        {3000, "A travessia"}, {4350, "Do outro lado"}, {5460, "Reencontro"}, {6550, "Créditos"}};
    PlayView p = make(eight);
    assert(gaps_in_track(p) == 7);  // the first starts at 0: no break there
    // a chapter list that arrives later (after the file opened) redraws the bar
    {
      PlayView q = make({});
      q.in.chapters = eight; ++q.in.chapters_revision;
      double now = 3;
      q.changed(now);
      for (int i = 0; i < 4; ++i) { now += 1 / 60.0; q.activity(now); q.tick(now, 1 / 60.0); }
      assert(gaps_in_track(q) == 7);
    }
    // two hundred chapters a few seconds apart: breaks are thinned, the line stays readable
    {
      std::vector<Chapter> dense;
      for (int i = 0; i < 200; ++i) dense.push_back({i * 35.0, "c"});
      PlayView d = make(dense);
      const int g = gaps_in_track(d);
      assert(g > 0 && g * 4 * s <= d.tl_.w + 4 * s);
      // and between breaks, at least three art pixels of line
      int run = 0, shortest = 1 << 30;
      bool started = false;
      for (int x = d.tl_.x; x < d.tl_.r(); ++x) {
        const bool clear = clear_at(d, x);
        if (clear) { if (started && run) shortest = std::min(shortest, run); run = 0; started = true; } else ++run;
      }
      assert(shortest >= 3 * s);
    }
    // the preview over the timeline: the chapter of that moment, above the time
    {
      double now = 2;
      const auto& dock = p.layers[PlayView::L_DOCK];
      const int y = dock.y + p.tl_.y;
      const int x = dock.x + p.tl_.x + (int)(p.tl_.w * 3100.0 / 7020);  // inside "A travessia"
      for (int i = 0; i < 60; ++i) { now += 1 / 60.0; p.mouse_move(x, y, now); p.tick(now, 1 / 60.0); }
      assert(p.preview_hovering());
      assert(p.tip_chapter_ == "5 \xc2\xb7 A travessia");
      int w0 = 0, h0 = 0;
      p.tip_size(w0, h0);
      PlayView plain = make({});
      for (int i = 0; i < 60; ++i) { now += 1 / 60.0; plain.mouse_move(x, y, now); plain.tick(now, 1 / 60.0); }
      int w1 = 0, h1 = 0;
      plain.tip_size(w1, h1);
      assert(plain.tip_chapter_.empty() && h0 == h1 + (int)(12 * s));
      // the picture from a keyframe far back, in another chapter (a file with few keyframes):
      // the name still says where a click goes
      p.set_thumb(std::vector<uint32_t>(p.thumb_hover_width() * p.thumb_hover_width() * 9 / 16, 0xff336699u), p.thumb_hover_width(), p.thumb_hover_width() * 9 / 16, 100);
      for (int i = 0; i < 10; ++i) { now += 1 / 60.0; p.mouse_move(x + (i % 2), y, now); p.tick(now, 1 / 60.0); }
      assert(p.tip_chapter_ == "5 \xc2\xb7 A travessia");
      // before the first chapter (a file whose first chapter starts later): no name
      PlayView l = make({{1000, "x"}, {2000, "y"}});
      const int xl = dock.x + p.tl_.x + (int)(p.tl_.w * 100.0 / 7020);
      for (int i = 0; i < 60; ++i) { now += 1 / 60.0; l.mouse_move(xl, y, now); l.tick(now, 1 / 60.0); }
      assert(l.tip_chapter_.empty());
    }
    // a very long name: the card stays inside the window, and the name inside the card
    {
      PlayView n = make({{0, "a"}, {3000, std::string(300, 'w')}});
      double now = 2;
      const auto& dock = n.layers[PlayView::L_DOCK];
      const int y = dock.y + n.tl_.y, x = dock.x + n.tl_.x + (int)(n.tl_.w * 3100.0 / 7020);
      for (int i = 0; i < 60; ++i) { now += 1 / 60.0; n.mouse_move(x, y, now); n.tick(now, 1 / 60.0); }
      const auto& tip = n.layers[PlayView::L_TIP];
      assert(tip.visible && tip.c.w <= W && tip.x >= 0 && tip.x + tip.c.w <= W);
    }
  }
  // stepping, the list and the menu
  {
    const std::vector<Chapter> eight = {{0, "Abertura"}, {370, "A estrada"}, {1180, "O farol"}, {1860, "Tempestade"},
                                        {3000, "A travessia"}, {4350, "Do outro lado"}, {5460, ""}, {6550, "Créditos"}};
    auto fresh = [&](double time, std::vector<Chapter> ch) {
      PlayView p; p.in.dur = 7020; p.in.time = time; p.in.chapters = std::move(ch); p.in.chapters_revision = 1;
      p.resize(1280, 720, 2); p.reset(1); p.changed(1);
      for (int i = 0; i < 30; ++i) p.tick(1 + i / 60.0, 1 / 60.0);
      p.take_actions();
      return p;
    };
    auto jump = [](PlayView& p) {
      double to = -1;
      for (const auto& a : p.take_actions()) if (a.a == PA::SeekAbsExact) to = a.v;
      return to;
    };
    PlayView p = fresh(2000, eight);  // in "Tempestade" (1860), 140 s in
    p.key("pagedown", false, false, 2); assert(jump(p) == 3000 && p.toast_text_ == "cap\xc3\xadtulo 5 \xc2\xb7 A travessia");
    p.key("pagedown", false, false, 2.3); assert(jump(p) == 4350);  // (in.time has not moved yet: counts from the last jump)
    p.key("pagedown", false, false, 2.6); assert(jump(p) == 5460 && p.toast_text_ == "cap\xc3\xadtulo 7");  // unnamed: its number
    p.key("pagedown", false, false, 2.9); assert(jump(p) == 6550);
    p.key("pagedown", false, false, 3.2); assert(jump(p) == -1);  // the last one: nowhere further
    PlayView b = fresh(2000, eight);
    b.key("pageup", false, false, 2); assert(jump(b) == 1860);   // well into it: its own start
    b.key("pageup", false, false, 2.3); assert(jump(b) == 1180); // right at its start: the one before
    PlayView e = fresh(1862, eight);
    e.key("pageup", false, false, 2); assert(jump(e) == 1180);   // under 3 s in: the one before
    PlayView z = fresh(100, eight);
    z.key("pageup", false, false, 2); assert(jump(z) == 0);      // the first one: its start
    // a jump of another kind in between: the next press counts from the film again
    PlayView m = fresh(2000, eight);
    m.key("pagedown", false, false, 2); assert(jump(m) == 3000);
    m.key("right", false, false, 2.1); m.take_actions();
    m.key("pagedown", false, false, 2.2); assert(jump(m) == 3000);
    // without chapters (or with one): the keys do nothing
    PlayView none = fresh(2000, {});
    none.key("pagedown", false, false, 2); assert(jump(none) == -1);
    PlayView one = fresh(2000, {{0, "only"}});
    one.key("pageup", false, false, 2); assert(jump(one) == -1);

    // "…": the row is there only with chapters
    auto has_row = [](const PlayView& v, PlayView::RowId id) { for (const auto& r : v.rows_) if (r.id == id) return true; return false; };
    PlayView q = fresh(2000, eight);
    q.open_panel_for_preview(3, 2); assert(has_row(q, PlayView::R_CHAPTERS));
    none.open_panel_for_preview(3, 2); assert(!has_row(none, PlayView::R_CHAPTERS));
    one.open_panel_for_preview(3, 2); assert(!has_row(one, PlayView::R_CHAPTERS));
    // the list: every chapter with where it begins, the one playing marked
    q.open_panel(PlayView::Panel::Chapters, PlayView::B_MORE, 2);
    assert(q.panel_ == PlayView::Panel::Chapters && q.cc_count_ == 8);
    int marked = -1;
    for (int i = q.cc_first_; i < q.cc_first_ + q.cc_count_; ++i) if (q.rows_[i].checked) marked = q.rows_[i].track;
    assert(marked == 3 && q.rows_[q.cc_first_ + 6].label == "cap\xc3\xadtulo 7" && q.rows_[q.cc_first_ + 5].right == "1:12:30" && q.rows_[q.cc_first_ + 1].right == "6:10");
    // playback moves on: the mark follows
    q.in.time = 3100; q.changed(2.1); marked = -1;
    for (int i = q.cc_first_; i < q.cc_first_ + q.cc_count_; ++i) if (q.rows_[i].checked) marked = q.rows_[i].track;
    assert(marked == 4);
    // a click: the film goes there, the menu closes
    q.panel_click(q.cc_first_ + 2, -1, 0, 2.2);
    assert(jump(q) == 1180 && q.panel_ == PlayView::Panel::None);
    // esc goes back to "…"; a file without chapters closes the list
    q.open_panel(PlayView::Panel::Chapters, PlayView::B_MORE, 2.3);
    q.key("esc", false, false, 2.4); assert(q.panel_ == PlayView::Panel::More);
    q.open_panel(PlayView::Panel::Chapters, PlayView::B_MORE, 2.5);
    q.in.chapters.clear(); ++q.in.chapters_revision; q.changed(2.6); assert(q.panel_ == PlayView::Panel::None);
    q.open_panel(PlayView::Panel::Chapters, PlayView::B_MORE, 2.7); assert(q.panel_ != PlayView::Panel::Chapters);
    // an open menu keeps Page Up / Page Down (no jump behind it); the list's own scroll them
    PlayView k = fresh(2000, eight);
    k.open_panel_for_preview(3, 2); k.key("pagedown", false, false, 2.1); assert(jump(k) == -1 && k.panel_ == PlayView::Panel::More);
    k.open_panel(PlayView::Panel::Chapters, PlayView::B_MORE, 2.2); k.key("pagedown", false, false, 2.3); assert(jump(k) == -1 && k.panel_ == PlayView::Panel::Chapters);
    // the keyboard: Tab, the list opens on the playing chapter, Down and Enter go to the next
    PlayView kb = fresh(2000, eight);
    kb.key("tab", false, false, 2); kb.open_panel(PlayView::Panel::Chapters, PlayView::B_MORE, 2);
    assert(kb.pfocus_ >= 0 && kb.rows_[kb.pfocus_].checked);
    kb.key("down", false, false, 2.1); kb.key("enter", false, false, 2.2); assert(jump(kb) == 3000 && kb.panel_ == PlayView::Panel::None);
    // the list goes when the window becomes the mini player (its menu stays short)
    PlayView mi = fresh(2000, eight);
    mi.open_panel(PlayView::Panel::Chapters, PlayView::B_MORE, 2);
    mi.in.mini_player = true; mi.resize(400, 225, 1); mi.changed(2.1);
    for (int i = 0; i < 10; ++i) mi.tick(2.1 + i / 60.0, 1 / 60.0);
    assert(mi.panel_ == PlayView::Panel::None);
    // a long list scrolls, with the playing chapter in view when it opens
    std::vector<Chapter> many;
    for (int i = 0; i < 120; ++i) many.push_back({i * 50.0, "Cena " + std::to_string(i + 1)});
    PlayView l = fresh(5000, many);
    l.open_panel(PlayView::Panel::Chapters, PlayView::B_MORE, 2);
    assert(l.cc_count_ == 120 && l.cc_scroll_max() > 0);
    const int row = 100 * l.cc_row_h_;  // chapter 101 plays
    assert(row >= l.cc_scroll_.x && row + l.cc_row_h_ <= l.cc_scroll_.x + l.cc_viewport_.h);
    const Rect r = l.panel_rect();
    assert(r.x >= 0 && r.y >= 0 && r.r() <= 1280 && r.b() <= 720);
    for (int i = 0; i < 40; ++i) l.tick(2 + i / 60.0, 1 / 60.0);
    l.render_panel();
    // "…" with the chapters row, and the list itself, whole inside every window, in every
    // language and interface size, film or song (an audiobook has chapters too)
    int menus = 0;
    for (int lang = 0; lang < i18n::kLangCount; ++lang)
      for (double sc : {1., 1.5, 2., 3.})
        for (auto sz : {std::pair{480, 270}, std::pair{640, 360}, std::pair{960, 540}, std::pair{1280, 720}, std::pair{1920, 1080}, std::pair{540, 960}, std::pair{2560, 1080}})
          for (bool music : {false, true}) {
            if (sz.first < 320 * sc || sz.second < 180 * sc) continue;
            i18n::set_language((i18n::Lang)lang);
            for (int ui : {-1, 2, 0, 3}) {
              PlayView v; v.in.dur = 7020; v.in.time = 2000; v.in.chapters = eight; v.in.chapters_revision = 1; v.in.audio_only = music; v.in.cover_scene = music; v.in.ui_scale = ui;
              v.resize(sz.first, sz.second, sc); v.reset(1); v.changed(1);
              for (int i = 0; i < 20; ++i) v.tick(1 + i / 60.0, 1 / 60.0);
              for (auto pn : {PlayView::Panel::More, PlayView::Panel::Chapters}) {
                v.open_panel(pn, PlayView::B_MORE, 2);
                for (int i = 0; i < 30; ++i) v.tick(2 + i / 60.0, 1 / 60.0);
                if (v.panel_ != pn) continue;  // (the mini player's own short menu)
                const Rect r = v.panel_rect();
                assert(r.x >= 0 && r.y >= 0 && r.r() <= sz.first && r.b() <= sz.second);
                for (const auto& row : v.rows_) if (!(v.list_panel() && row.id == v.list_row())) assert(row.r.x >= 0 && row.r.r() <= r.w && row.r.y >= 0 && row.r.b() <= r.h);
                ++menus;
              }
            }
          }
    assert(menus > 300);
    i18n::set_language(i18n::Lang::PtBR);
  }
  // Settings: the choice, on by default, kept between starts; its row in both looks (a film's
  // and a song's: audiobooks), and a click asks the App to turn them off or on
  {
    const std::string file = (std::filesystem::temp_directory_path() / "cue_chapters_choice.json").string();
    Store a; assert(a.chapters); a.chapters = false; assert(a.save(file));
    Store b; assert(b.load(file) && !b.chapters);
    std::filesystem::remove(file);
    for (bool music : {false, true}) {
      PlayView v; v.in.dur = 7020; v.in.audio_only = music; v.in.cover_scene = music; v.in.chapters_on = true;
      v.resize(1280, 720, 2); v.reset(1); v.changed(1);
      v.open_panel_for_preview(11, 2);
      int row = -1;
      for (int i = 0; i < (int)v.rows_.size(); ++i) if (v.rows_[i].id == PlayView::R_CHAPTERS_ON) row = i;
      assert(row >= 0 && v.rows_[row].k == PlayView::RK::Toggle && v.rows_[row].checked);
      v.take_actions();
      v.panel_click(row, -1, 0, 2.1);
      bool off = false;
      for (const auto& a : v.take_actions()) off |= a.a == PA::ChaptersOn && a.v == 0;
      assert(off && !v.in.chapters_on);
    }
  }
  std::puts("Chapters: breaks on the timeline (none without two chapters, thinned when dense, redrawn when they arrive later), the preview's chapter name and its bounds, Page Up / Page Down (back within 3 s, presses in a row, other jumps, the ends), the list in \"…\" (only with chapters, marked, following playback, click, esc, scrolling), the Settings choice (saved, both looks) passed");
}
