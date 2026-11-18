// The home's "continuar": the newest entry stands apart only when it is one to go back to
// (started, not finished, not a song); the list under "recentes" goes on without it. Its light is
// made once per picture and size. And the quiet line's "visto" rule.
#include "../src/core/i18n.h"
#include "../src/core/recents_view.h"
#define private public
#include "../src/ui/home.h"
#undef private
#include <cassert>
#include <cstdio>
#include <cstdlib>

static std::shared_ptr<const HomeThumb> pic(uint32_t col, bool cover = false) {
  auto t = std::make_shared<HomeThumb>();
  t->w = cover ? 64 : 128; t->h = cover ? 64 : 72; t->cover = cover;
  t->px.assign((size_t)t->w * t->h, col);
  return t;
}
static HomeRow row(const char* name, double progress, HomeKind kind, std::shared_ptr<const HomeThumb> t = nullptr) {
  HomeRow r; r.name = name; r.progress = progress; r.kind = kind; r.audio = kind == HomeKind::Music; r.seed = name; r.thumb = t;
  r.sub = progress >= 0 ? "faltam 12 min" : "";
  return r;
}

int main() {
  i18n::set_language(i18n::Lang::PtBR);
  for (auto size : {std::pair{1280, 720}, std::pair{900, 520}, std::pair{640, 400}, std::pair{3840, 2160}}) {
    const double s = size.first >= 3000 ? 3 : size.first >= 1200 ? 1.5 : 1;
    HomeView h;
    h.set_size(size.first, size.second, s);
    h.set_rows({row("Ruptura", 0.6, HomeKind::Episode, pic(0xFF336699u)), row("Paris", 0.3, HomeKind::Video), row("Song", -1, HomeKind::Music),
                row("Aftersun", -1, HomeKind::Video)});
    assert(h.continuing());
    assert(h.rows.size() >= 2 && h.r_rows_.size() == h.rows.size());
    // twice the list's picture, standing above "recentes", the title above it
    if (h.thumb_w()) {
      const Rect big = h.thumb_rect(0), small = h.thumb_rect(1);
      assert(big.w >= 2 * small.w - 1 && std::abs(big.w * 9 - big.h * 16) <= 16 * 2);  // (at least twice the list's, 16:9)
    }
    assert(h.r_cont_label_.b() <= h.row_rect(0).y && h.row_rect(0).b() < h.r_title_.y && h.r_title_.b() <= h.row_rect(1).y);
    for (size_t i = 0; i < h.r_rows_.size(); ++i) assert(h.r_rows_[i].y >= 0 && h.r_rows_[i].b() <= size.second);
    // its light: made once for the picture and the size, then only blitted
    Canvas c;
    double now = 5;
    h.enter(now);
    for (int i = 0; i < 60; ++i) { now += 1 / 60.0; h.tick(now, 1 / 60.0); }
    h.render(c, now);
    if (h.thumb_w()) {
      assert(h.cont_light_src_ == h.rows[0].thumb.get() && h.cont_light_.w > h.thumb_rect(0).w);
      const uint32_t* made = h.cont_light_.px.data();
      h.render(c, now + 0.5);
      assert(h.cont_light_.px.data() == made);
      // the light is coloured by the picture (blue here), around it, never over the words
      const Rect tr = h.thumb_rect(0);
      const uint32_t beside = c.px[(size_t)(tr.cy()) * c.w + tr.x - (int)(4 * s)];
      assert((beside & 0xFF) > ((beside >> 16) & 0xFF));
    }
    // the same frame twice: nothing else moves (the eyes-only redraws stay possible)
    assert(h.eyes_only_key(now + 1) == h.eyes_only_key(now + 1.1) || h.eyes_only_key(now + 1) == 0);
  }
  // not a thing to continue: a song, or one not started, or one finished: the plain list
  for (auto first : {row("Song", -1, HomeKind::Music), row("Aftersun", -1, HomeKind::Video), row("Done", 1.0, HomeKind::Video)}) {
    HomeView h;
    h.set_size(1280, 720, 1.5);
    h.set_rows({first, row("Paris", 0.3, HomeKind::Video)});
    assert(!h.continuing() && h.recent_title_ && h.r_cont_label_.w == 0);
  }
  // only the one to continue: no "recentes" under it, its dots beside "continuar"
  {
    HomeView h;
    h.set_size(1280, 720, 1.5);
    h.set_rows({row("Ruptura", 0.6, HomeKind::Episode)});
    assert(h.continuing() && !h.recent_title_ && h.r_title_.y == h.r_cont_label_.y && h.dots_rect().y == h.r_cont_label_.y);
    // clearing: the word leaves with its entry, and "recentes" comes for the empty line
    double now = 3;
    h.clear_animated(now);
    assert(h.ghosts_.size() == 2);
    h.set_rows({});
    assert(!h.continuing() && h.recent_title_);
  }
  // the order: the newest one to go back to leads, even after songs or a quick peek at another file
  {
    auto e = [](const char* t, double pos, RecentKind k) { LiveRecent r; r.title = t; r.path = t; r.pos = pos; r.dur = 3000; r.kind = k; return r; };
    std::vector<LiveRecent> v = {e("song", -1, RecentKind::Music), e("peek", -1, RecentKind::Video), e("film", 1200, RecentKind::Video),
                                 e("old", 300, RecentKind::Episode)};
    home_order(v, 6);
    assert(v[0].title == "film" && v[1].title == "song" && v[2].title == "peek" && v[3].title == "old");
    std::vector<LiveRecent> none = {e("song", 100, RecentKind::Music), e("a", -1, RecentKind::Video)};
    home_order(none, 6);
    assert(none[0].title == "song");  // (a song never leads: it starts over)
    std::vector<LiveRecent> many;
    for (int i = 0; i < 9; ++i) many.push_back(e("x", -1, RecentKind::Video));
    many.push_back(e("deep", 50, RecentKind::Listen));
    home_order(many, 6);
    assert(many.size() == 6 && many[0].title == "deep");
  }
  // the ··· menu's "mostrar imagens" off: a plain list of words, six at most, nothing apart
  {
    HomeView h;
    h.covers = false;
    h.set_size(1280, 720, 1.5);
    std::vector<HomeRow> six;
    for (int i = 0; i < 7; ++i) six.push_back(row("Film", i == 0 ? 0.4 : -1, HomeKind::Video, pic(0xFF336699u)));
    h.set_rows(six);
    assert(!h.continuing() && h.thumb_w() == 0 && h.rows.size() == 6 && h.recent_title_);
    h.covers = true;
    h.set_rows(six);
    assert(h.continuing() && h.thumb_w() > 0 && h.rows.size() <= 4);
  }
  // switching "mostrar imagens" moves nothing but the column's contents: the eyes and "abra um
  // arquivo" stay where they are, and the column starts on the same left edge (its titles and
  // the entries' boxes line up between the two looks), in every window and at every scale
  {
    std::vector<HomeRow> six;
    for (int i = 0; i < 6; ++i) six.push_back(row("Film", i == 0 ? 0.4 : -1, HomeKind::Video, pic(0xFF336699u)));
    const int windows[][2] = {{360, 270}, {640, 360}, {960, 540}, {1280, 720}, {1920, 1080}, {3840, 2160}, {2560, 1071}, {1080, 1080}, {1080, 1920}};
    for (double s : {1.0, 1.5, 2.0, 2.5, 3.0, 4.0})
      for (const auto& w : windows) {
        if (w[0] / s < 240 || w[1] / s < 180) continue;  // (smaller than the window ever gets at that scale)
        HomeView a, b;
        a.covers = true; b.covers = false;
        a.set_size(w[0], w[1], s); b.set_size(w[0], w[1], s);
        a.set_rows(six); b.set_rows(six);
        assert(a.face.cx == b.face.cx && a.face.cy == b.face.cy && a.face.scale == b.face.scale);
        assert(a.r_open_.x == b.r_open_.x && a.r_open_.y == b.r_open_.y && a.r_open_.w == b.r_open_.w);
        const int left_a = a.continuing() ? a.r_cont_label_.x : a.r_title_.x;
        assert(left_a == b.r_title_.x && a.r_title_.x == b.r_title_.x);
        assert(!a.r_rows_.empty() && !b.r_rows_.empty() && a.r_rows_[0].x == b.r_rows_[0].x && a.r_rows_[0].w == b.r_rows_[0].w);
        assert(a.dots_rect().x == b.dots_rect().x);
        // and the same top: "continuar" (or "recentes") where "recentes" is in the other look
        const int top_a = a.continuing() ? a.r_cont_label_.y : a.r_title_.y;
        assert(top_a == b.r_title_.y);
        // ...with the same rows and without a "continuar" too (a short list, a long one)
        for (int k : {1, 3, 6}) {
          std::vector<HomeRow> plain;
          for (int i = 0; i < k; ++i) plain.push_back(row("Film", -1, HomeKind::Video, pic(0xFF336699u)));
          HomeView c, d;
          c.covers = true; d.covers = false;
          c.set_size(w[0], w[1], s); d.set_size(w[0], w[1], s);
          c.set_rows(plain); d.set_rows(plain);
          assert(c.r_title_.y == d.r_title_.y && c.r_title_.x == d.r_title_.x);
          const Rect win{0, 0, w[0], w[1]};
          for (const HomeView* v : {&c, &d})
            for (const Rect& r : v->r_rows_) assert(r.y >= 0 && r.b() <= win.b());
        }
        for (const HomeView* v : {&a, &b})
          for (const Rect& r : v->r_rows_) assert(r.y >= 0 && r.b() <= w[1]);
      }
  }
  // "visto": its place went back to the start at its end, or it was left in its last moments
  assert(recent_seen(-1, 0, 3000) && recent_seen(-1, 2990, 3000) && recent_seen(-1, 2950, 3000));
  assert(!recent_seen(1234, 1234, 3000) && !recent_seen(-1, 5, 3000) && !recent_seen(-1, 0, 0));
  std::puts("Home: \"continuar\" only for one to go back to, twice the picture with its own light made once, the list under \"recentes\" without it; \"mostrar imagens\" moves only the column's contents (same left edge, same top); \"visto\" rule");
}
