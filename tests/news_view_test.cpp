// "Saiu do forno": the entrance, the cards under the pointer, a poke, the calm afterwards (nothing
// drawn again while nothing changes), a resize and a new language mid-entrance, closing and
// opening again.
#define private public
#include "../src/ui/news_view.h"
#undef private
#include "../src/core/i18n.h"
#include <cassert>
#include <cstdio>

static void run(NewsView& v, double& t, double secs, Canvas* c = nullptr, int* drawn = nullptr) {
  for (double k = 0; k < secs; k += 1 / 60.0) {
    t += 1 / 60.0;
    v.tick(t, 1 / 60.0);
    if (c && v.changed(t)) { v.render(*c, t); if (drawn) ++*drawn; }
  }
}

int main() {
  i18n::set_language(i18n::Lang::PtBR);
  for (auto [W, H, S] : {std::tuple{1280, 720, 1.5}, std::tuple{1920, 1080, 2.0}, std::tuple{3840, 2160, 4.5}, std::tuple{800, 520, 1.0}}) {
    NewsView v;
    v.version = "1.4";
    v.set_size(W, H, S);
    double t = 10;
    v.open(t);
    Canvas c;
    int drawn = 0;
    run(v, t, NewsView::kEntrance + 0.1, &c, &drawn);
    assert(drawn > 100);  // the entrance plays at the display's pace
    // every card, its words and the button inside the window
    for (auto& r : v.items_) assert(r.x >= 0 && r.y >= 0 && r.r() <= W && r.b() <= H);
    assert(v.buttons_[0].b() <= H);
    // calm: only the sparkles' twinkle is drawn again, a few times a second, never every frame
    drawn = 0;
    run(v, t, 10.0, &c, &drawn);
    assert(drawn > 0 && drawn < 120);
    assert(!v.fast(t));
    // the cards' glows were made ahead while nothing moved
    for (auto& cl : v.card_light_) assert(cl.w > 0);
    // a card under the pointer rises two pixels of art and glows; away from it, it settles back
    v.pointer_move(v.items_[1].cx(), v.items_[1].cy(), t);
    run(v, t, 0.8, &c);
    assert(v.hover_card_ == 1 && std::fabs(v.lift_[1].x + 2) < 0.05 && v.glow_[1] > 0.99 && v.glow_[0] < 0.01);
    v.pointer_leave(t);
    run(v, t, 0.8, &c);
    assert(std::fabs(v.lift_[1].x) < 0.05 && v.glow_[1] < 0.01);
    // pokes: sparks fly and are gone again, never a pile of them
    for (int i = 0; i < 40; ++i) v.pointer_down(W / 2, (int)v.face_.eye_y(), t);
    assert(v.sparks_.size() <= 34);
    run(v, t, 1.2, &c);
    assert(v.sparks_.empty());
    // Esc goes on
    assert(v.key_down("esc", false, t).a == (int)NewsAct::Continue);
    // closing: gone after its fade, its pictures let go; opened again, all made again
    v.close(t);
    run(v, t, 0.4);
    assert(v.closed(t) && v.light_.w == 0 && v.thumbs_[0].w == 0);
    v.render(c, t);  // (a late frame after it was let go: laid out again, no crash)
    v.open(t);
    run(v, t, 0.5, &c);
    assert(v.light_.w > 0 && v.thumbs_[0].w > 0);
  }
  // a resize and a new language in the middle of the entrance
  {
    NewsView v;
    v.version = "1.4";
    v.set_size(1280, 720, 1.5);
    double t = 1;
    v.open(t);
    Canvas c;
    run(v, t, 0.9, &c);
    v.set_size(960, 600, 1.0);
    i18n::set_language(i18n::Lang::De);
    v.set_size(960, 600, 1.0);
    run(v, t, 2.0, &c);
    for (auto& r : v.items_) assert(r.r() <= 960 && r.b() <= 600);
    i18n::set_language(i18n::Lang::PtBR);
  }
  // reduced motion: everything in place at once, nothing kept moving
  {
    reduce_motion() = true;
    NewsView v;
    v.version = "1.4";
    v.set_size(1280, 720, 1.5);
    double t = 1;
    v.open(t);
    Canvas c;
    run(v, t, 0.1, &c);
    assert(!v.fast(t));
    v.pointer_down(1280 / 2, (int)v.face_.eye_y(), t);
    assert(v.sparks_.empty());
    reduce_motion() = false;
  }
  std::puts("What's new: entrance at the display's pace, calm redraws only on change, cards that rise and glow, bounded pokes, "
            "resize and language mid-entrance, close and reopen, reduced motion passed");
}
