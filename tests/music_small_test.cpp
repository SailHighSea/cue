// The rascunho's last pieces (15/11): the tall window with its queue under the words, Micro at
// rest (the equalizer and the progress line, only without the pointer) and Micro's window buttons
// (full screen in the corner, the pin and close; no minimise, no maximise, no back arrow).
#define private public
#include "../src/ui/play.h"
#undef private
#include "../src/core/audio_mode.h"
#include "../src/core/i18n.h"
#include "../src/core/ui_scale.h"
#include <cassert>
#include <cstdio>
#include <tuple>
#include <vector>

static void run(PlayView& p, double& t, int frames) {
  for (int i = 0; i < frames; ++i) { t += 1. / 60; p.tick(t, 1. / 60); p.take_actions(); p.prepare(t); }
}
// the framing the app gives a card's composition (App::music_view_target)
static void frame(PlayView& p, MusicFrame fr) {
  MusicView v; v.frame = fr; v.f = 1;
  if (Rect c = p.card_cover(); c.w > 0) { v.f = c.w / (double)p.in.music_scene.side; v.ox = c.x - p.in.music_scene.x * v.f; v.oy = c.y - p.in.music_scene.y * v.f; }
  p.in.music_view = v;
}
static PlayView* song(int W, int H, double s, MusicFrame fr, int songs, double& t) {
  PlayView* p = new PlayView;
  p->in.audio_only = true; p->in.cover_scene = true; p->in.dur = 200; p->in.time = 50;
  p->in.music_title = "Ice Palace Adolescence"; p->in.music_artist = "Jane Remover";
  p->in.music_scene = MusicScene::wide();
  for (int i = 0; i < songs; ++i) p->in.queue.push_back({(uint32_t)(i + 1), "Artist - Song " + std::to_string(i + 1) + ".mp3", i == 1, true});
  p->in.queue_active = songs > 1; p->in.has_next = p->in.has_prev = songs > 1; p->in.queue_revision = 1;
  frame(*p, fr);
  p->resize(W, H, s); t = 1; p->reset(t);
  for (int pass = 0; pass < 3; ++pass) { frame(*p, fr); p->changed(t); run(*p, t, 20); }
  return p;
}
static bool overlap(const Rect& a, const Rect& b) { return a.w > 0 && b.w > 0 && a.x < b.r() && b.x < a.r() && a.y < b.b() && b.y < a.b(); }

int main() {
  // 1. tall: the cover, the words, the queue and the bar, in that order, none over another; in
  //    the 8 languages, the 4 interface sizes and five windows
  int cases = 0;
  for (int lang = 0; lang < i18n::kLangCount; ++lang) {
    i18n::set_language((i18n::Lang)lang);
    for (int size = 0; size < kUiSizes; ++size)
      for (auto [W, H] : std::vector<std::pair<int, int>>{{900, 1300}, {650, 1530}, {600, 1000}, {1100, 1000}, {1790, 1620}}) {
        const double s = ui_scale(144, W, H, ui_size_choice(size));
        if (music_frame_for(W, H, s, MusicFrame::Card, 1.5) != MusicFrame::Tall) continue;
        const double ps = std::max(s, music_compact_scale(s, W, H, ui_size_choice(size), ui_base_scale(144, ui_size_choice(size))));
        double t;
        PlayView* p = song(W, H, ps, MusicFrame::Tall, 5, t);
        assert(p->music_card_mode() == 5 && p->music_card_wings());
        const Rect cv = p->card_cover(), d = p->dock_rect(), q = p->music_queue_rect();
        const Layer& M = p->layers[PlayView::L_MUSIC];
        assert(cv.w >= 40 * ps && cv.x >= 0 && cv.r() <= W && cv.y >= 0);
        assert(M.visible && M.x >= 0 && M.x + M.c.w <= W && M.y >= cv.b());
        const Rect words{M.x, M.y, M.c.w, M.c.h};
        assert(d.x >= 0 && d.r() <= W && d.b() <= H && d.y > words.b());
        for (int id : {PlayView::B_VOL, PlayView::B_PREV, PlayView::B_PLAY, PlayView::B_NEXT, PlayView::B_MORE, PlayView::B_TIMELINE}) assert(p->in_bar(id));
        if (q.w > 0) {
          const Layer& Q = p->layers[PlayView::L_REST];
          assert(Q.visible && Q.x == q.x && Q.y == q.y && Q.c.w == q.w && Q.c.h == q.h);
          assert(q.x >= 0 && q.r() <= W && !overlap(q, words) && !overlap(q, d) && !overlap(q, cv));
          if (p->fila_shown() < 3) std::fprintf(stderr, "W %d H %d ps %.2f q.h %d shown %d cover %d\n", W, H, ps, q.h, p->fila_shown(), cv.w);
          assert(p->fila_shown() >= 3);  // the heading and at least three songs, or no queue at all
        }
        ++cases;
        delete p;
      }
  }
  assert(cases >= 8 * 4 * 3);
  i18n::set_language(i18n::Lang::PtBR);
  // 2. the queue's rows: a click plays that song; on the playing one it pauses; the pointer lights
  //    a row; the wheel moves a list that holds more than fits (never the volume there), and the
  //    volume everywhere else
  {
    double t;
    PlayView* p = song(900, 1300, 2.5, MusicFrame::Tall, 40, t);
    const Rect q = p->music_queue_rect();
    assert(q.w > 0 && p->fila_shown() < 40 && p->fila_top_ == 0);
    const int rh = (int)std::lround(PlayView::kFilaRow * 2.5), head = (int)std::lround(PlayView::kFilaHead * 2.5);
    auto row_y = [&](int r) { return q.y + head + r * rh + rh / 2; };
    assert(p->fila_row_at(q.x + 40, q.y + 2) == -1);                       // the heading is not a song
    assert(p->fila_row_at(q.x + 40, row_y(0)) == 0 && p->fila_row_at(q.x + 40, row_y(3)) == 3);
    p->mouse_move(q.x + 40, row_y(3), t); assert(p->fila_hot_ == 3 && p->cursor_kind() == 1);
    const uint64_t gen = p->fila_gen_; run(*p, t, 2); assert(p->fila_gen_ == gen + 1);  // drawn again once for the lit row
    run(*p, t, 5); assert(p->fila_gen_ == gen + 1);                        // and not again while nothing changes
    p->mouse_down(q.x + 40, row_y(3), t); p->mouse_up(q.x + 40, row_y(3), t);
    { bool play = false; for (auto& a : p->take_actions()) play |= a.a == PA::QueuePlay && a.id == 4; assert(play); }
    p->mouse_move(q.x + 41, row_y(1), t); p->mouse_down(q.x + 41, row_y(1), t); p->mouse_up(q.x + 41, row_y(1), t);
    { bool pause = false, other = false; for (auto& a : p->take_actions()) { pause |= a.a == PA::TogglePause; other |= a.a == PA::QueuePlay; } assert(pause && !other); }
    p->wheel(-1, t); assert(p->fila_top_ == 2);
    { for (auto& a : p->take_actions()) assert(a.a != PA::Volume); }
    for (int i = 0; i < 60; ++i) p->wheel(-1, t);
    assert(p->fila_top_ == 40 - p->fila_shown());                          // stops at the end
    p->take_actions();
    p->mouse_move(q.x + 40, q.y - 60, t); p->wheel(1, t);                  // over the words: the volume, as everywhere
    { bool vol = false; for (auto& a : p->take_actions()) vol |= a.a == PA::Volume; assert(vol); }
    // a new song far down the list: its row comes into view
    for (auto& e : p->in.queue) e.current = e.id == 30;
    ++p->in.queue_revision; p->fila_top_ = 0; p->changed(t); run(*p, t, 3);
    assert(p->fila_top_ <= 29 && 29 < p->fila_top_ + p->fila_shown());
    // the list's key: the full list in the composition of before (its header), and back
    p->open_panel(PlayView::Panel::Queue, PlayView::B_QUEUE, t); run(*p, t, 3);
    assert(p->music_card_mode() == 0 && p->music_queue_rect().w == 0 && !p->layers[PlayView::L_REST].visible);
    p->close_panel(); run(*p, t, 3);
    assert(p->music_card_mode() == 5 && p->music_queue_rect().w > 0);
    delete p;
  }
  // 3. one song alone: no queue, the cover and its words in the room over the bar
  {
    double t;
    PlayView* p = song(900, 1300, 2.5, MusicFrame::Tall, 1, t);
    assert(p->music_card_mode() == 5 && p->music_queue_rect().w == 0 && !p->layers[PlayView::L_REST].visible);
    const Rect cv = p->card_cover(), d = p->dock_rect();
    assert(cv.b() < d.y && cv.y > 0);
    delete p;
  }
  // 4. Micro at rest: without the pointer the equalizer and the progress line are up and the
  //    controls are not; with it, the other way round. The bars move only with a measured level,
  //    never paused or with reduced motion; a frame is due only when a pixel would change
  for (bool reduced : {false, true}) {
    reduce_motion() = reduced;
    double t;
    PlayView* p = song(390, 390, 1, MusicFrame::Micro, 3, t);
    assert(p->music_card_mode() == 2);
    p->mouse_move(200, 200, t); run(*p, t, 30);
    const Layer& R = p->layers[PlayView::L_REST];
    assert(p->vis_.v > 0.9 && !R.visible);
    p->pointer_left(); p->mx_ = p->my_ = -1;
    run(*p, t, 600);                                                       // the controls leave, as a film's
    assert(p->vis_.v <= 0.01 && R.visible && R.c.w == p->card_cover().w && R.y + R.c.h == p->card_cover().b());
    const auto still = p->micro_rest_now();
    assert(still[0] == 3 && still[1] == 7 && still[2] == 5 && still[3] == 2);  // nothing measured: the rascunho's shape
    assert(!p->micro_rest_due());
    p->in.music_level = 0.9; p->in.music_level_t = 2.0;
    if (reduced) assert(!p->micro_rest_due() && p->micro_rest_now() == still);
    else {
      assert(p->micro_rest_due());
      const uint64_t gen = p->rest_gen_; run(*p, t, 1); assert(p->rest_gen_ == gen + 1 && !p->micro_rest_due());
      p->in.music_level_t = 2.0005; assert(!p->micro_rest_due());        // no pixel moved: no frame
      p->in.paused = true; assert(p->micro_rest_now() == still);          // paused: at rest
      p->in.paused = false;
    }
    // the progress line: a frame when it grows by a pixel, none before
    run(*p, t, 1);
    p->in.time = 50.01; assert(!p->micro_rest_due());
    p->in.time = 60; assert(p->micro_rest_due());
    delete p;
  }
  reduce_motion() = false;
  // 5. Micro's window buttons: the pin and close on the right, side by side; no minimise or
  //    maximise; the corner on the left is full screen (not the way home)
  {
    double t;
    PlayView* p = song(390, 390, 1, MusicFrame::Micro, 3, t);
    p->mouse_move(200, 200, t); run(*p, t, 30);
    assert(p->chrome.micro);
    assert(p->chrome.rect(WindowChrome::Min, 390).w == 0 && p->chrome.rect(WindowChrome::Max, 390).w == 0);
    const Rect pin = p->chrome.rect(WindowChrome::Pin, 390), close = p->chrome.rect(WindowChrome::Close, 390);
    assert(pin.w > 0 && close.w > 0 && pin.r() <= close.x && close.r() <= 390 && close.x - pin.r() <= 8);
    for (int x = 0; x < 390; ++x) { const int h = p->chrome.hit(x, pin.cy(), 390); assert(h == WindowChrome::None || h == WindowChrome::Pin || h == WindowChrome::Close); }
    const Rect back = p->back_rect();
    p->mouse_move(back.cx(), back.cy(), t); run(*p, t, 5);
    p->mouse_down(back.cx(), back.cy(), t); p->mouse_up(back.cx(), back.cy(), t);
    { bool fs = false, home = false; for (auto& a : p->take_actions()) { fs |= a.a == PA::Fullscreen; home |= a.a == PA::CloseVideo; } assert(fs && !home); }
    // a larger window: the usual four, and the corner is the way home again
    frame(*p, MusicFrame::Cover); p->resize(720, 780, 2); p->changed(t); frame(*p, MusicFrame::Cover); p->changed(t);
    p->mouse_move(300, 300, t); run(*p, t, 30);
    assert(!p->chrome.micro && p->chrome.rect(WindowChrome::Min, 720).w > 0 && p->chrome.rect(WindowChrome::Max, 720).w > 0);
    const Rect b2 = p->back_rect();
    p->mouse_move(b2.cx(), b2.cy(), t); run(*p, t, 5);
    p->mouse_down(b2.cx(), b2.cy(), t); p->mouse_up(b2.cx(), b2.cy(), t);
    { bool fs = false, home = false; for (auto& a : p->take_actions()) { fs |= a.a == PA::Fullscreen; home |= a.a == PA::CloseVideo; } assert(home && !fs); }
    delete p;
  }
  // 6. every small-window composition is taken as soon as the app's frame asks for it (not only
  //    at the next resize), and at rest the bar is never laid out again and again
  for (auto [W, H, fr, mode] : std::vector<std::tuple<int, int, MusicFrame, int>>{{720, 780, MusicFrame::Cover, 1}, {390, 390, MusicFrame::Micro, 2},
           {1000, 150, MusicFrame::Strip, 3}, {2100, 680, MusicFrame::Scene, 4}, {900, 1300, MusicFrame::Tall, 5}}) {
    PlayView p;
    p.in.audio_only = true; p.in.cover_scene = true; p.in.dur = 200; p.in.time = 50;
    p.in.music_title = "Song"; p.in.music_artist = "Artist"; p.in.music_scene = MusicScene::wide();
    for (uint32_t i = 1; i <= 4; ++i) p.in.queue.push_back({i, "s", i == 1, true});
    p.in.queue_active = true; p.in.has_next = true; p.in.queue_revision = 1;
    double t = 1;
    p.resize(W, H, 2); p.reset(t); run(p, t, 5);
    assert(p.music_card_mode() == 0);                       // no frame given yet
    p.in.music_view = music_view(p.in.music_scene, fr, W, H, H); p.in.music_view.frame = fr;
    p.changed(t);
    assert(p.music_card_mode() == mode);                    // taken at once
    for (int pass = 0; pass < 3; ++pass) { frame(p, fr); p.changed(t); run(p, t, 10); }
    const uint32_t n = p.layouts_;
    for (int i = 0; i < 50; ++i) { p.in.time += 0.03; p.changed(t); run(p, t, 1); }
    assert(p.layouts_ == n);                                // at rest: laid out once, not at every change
    assert(p.music_card_mode() == mode);
  }
  // 7. the smallest windows (15/11), at every display scale and interface size: Micro at its floor
  //    (220 px at 100%) and the strip at its floor (460 x 96) keep controls one can hit: inside the
  //    window, apart from each other, and at the standard size a play button of 40 px at 100% or more
  for (unsigned dpi : {96u, 120u, 144u, 168u, 192u, 240u, 288u})
    for (int size = 0; size < kUiSizes; ++size) {
      const double k = dpi / 96.0;
      {
        const int W = (int)std::lround(kMusicMicroMin * k);
        const double ps = music_small_scale(dpi, W, W, ui_size_choice(size), false);
        double t; PlayView* p = song(W, W, ps, MusicFrame::Micro, 3, t);
        p->mouse_move(W / 2, W / 2, t); run(*p, t, 30);
        assert(p->music_card_mode() == 2);
        const Rect d = p->dock_rect();
        Rect r[3]; int n = 0;
        for (auto& hb : p->hots_) if (hb.id == PlayView::B_PREV || hb.id == PlayView::B_PLAY || hb.id == PlayView::B_NEXT) { assert(n < 3); r[n++] = {d.x + (int)hb.r.x, d.y + (int)hb.r.y, (int)hb.r.w, (int)hb.r.h}; }
        assert(n == 3);
        for (int i = 0; i < 3; ++i) { assert(r[i].x >= 0 && r[i].r() <= W && r[i].y >= 0 && r[i].b() <= W); for (int j = i + 1; j < 3; ++j) assert(!overlap(r[i], r[j])); }
        if (size >= 2) for (int i = 0; i < 3; ++i) assert(r[i].w >= (r[i].w == std::max({r[0].w, r[1].w, r[2].w}) ? 40 : 28) * k - 2);
        // the window's corner buttons and the playback never meet
        assert(p->back_rect().b() <= r[0].y && p->chrome.rect(WindowChrome::Close, W).b() <= r[2].y);
        delete p;
      }
      {
        const int W = (int)std::lround(kMusicStripMinW * k), H = (int)std::lround(kMusicMinH * k);
        const double ps = music_small_scale(dpi, W, H, ui_size_choice(size), true);
        double t; PlayView* p = song(W, H, ps, MusicFrame::Strip, 3, t);
        assert(p->music_card_mode() == 3);
        const Rect d = p->dock_rect(), cv = p->card_cover();
        assert(cv.x >= p->back_rect().r() && cv.y >= 0 && cv.b() <= H);       // the thumbnail clear of the corner arrow
        assert(p->card_col_.w >= 60 * ps && p->card_col_.x >= cv.r());        // room for a title beside it
        for (auto& hb : p->hots_) { const Rect q{d.x + (int)hb.r.x, d.y + (int)hb.r.y, (int)hb.r.w, (int)hb.r.h}; assert(q.x >= p->card_col_.r() && q.r() <= W && q.y >= 0 && q.b() <= H); }
        delete p;
      }
    }
  // 8. the cover stays still under the pointer in the small windows (no tilt), and tilts as before in the tall one
  for (auto [W, H, fr, tilts] : std::vector<std::tuple<int, int, MusicFrame, bool>>{{390, 390, MusicFrame::Micro, false}, {720, 780, MusicFrame::Cover, false}, {1000, 150, MusicFrame::Strip, false}, {900, 1300, MusicFrame::Tall, true}}) {
    double t; PlayView* p = song(W, H, 2, fr, 3, t);
    p->in.cover_art = true; p->in.cover_tilt = true; p->in.controls_free = true; p->changed(t);
    const Rect cv = p->card_cover();
    p->mouse_move(cv.x + cv.w / 4, cv.y + cv.h / 4, t);
    assert(p->cov_hot_ == tilts);
    delete p;
  }
  // 9. Micro and the strip have no list (15/11): its key does nothing there, and a list open in a
  //    larger window closes as the window is sized down to them
  for (auto [W, H, fr, mode] : std::vector<std::tuple<int, int, MusicFrame, int>>{{390, 390, MusicFrame::Micro, 2}, {1000, 150, MusicFrame::Strip, 3}}) {
    double t; PlayView* p = song(W, H, 1.5, fr, 4, t);
    assert(p->music_card_mode() == mode);
    p->open_panel(PlayView::Panel::Queue, PlayView::B_QUEUE, t); run(*p, t, 10);
    assert(p->panel_ != PlayView::Panel::Queue && p->music_card_mode() == mode);
    frame(*p, MusicFrame::Cover); p->resize(720, 780, 2); p->changed(t); frame(*p, MusicFrame::Cover); p->changed(t); run(*p, t, 10);
    p->open_panel(PlayView::Panel::Queue, PlayView::B_QUEUE, t); run(*p, t, 20);
    assert(p->panel_ == PlayView::Panel::Queue);
    p->in.music_view = music_view(p->in.music_scene, fr, W, H, H); p->in.music_view.frame = fr;
    p->resize(W, H, 1.5); p->changed(t); run(*p, t, 30);
    assert(p->panel_ != PlayView::Panel::Queue && p->music_card_mode() == mode);
    delete p;
  }
  // 10. the wide and low window (15/11, the Biel's capture at 760 x 450): the list opened steps
  //     to the large window's composition; the words are in the header's place only while the
  //     header's layout holds, never on top of a list that lies over the cover
  for (auto [W, H, s] : std::vector<std::tuple<int, int, double>>{{760, 450, 1.5}, {760, 450, 2.0}, {1300, 430, 1.5}, {2100, 680, 2.5}, {1500, 520, 2.0}}) {
    double t; PlayView* p = song(W, H, s, MusicFrame::Scene, 4, t);
    if (p->music_card_mode() != 4) { delete p; continue; }
    p->open_panel(PlayView::Panel::Queue, PlayView::B_QUEUE, t);
    for (int i = 0; i < 90; ++i) {
      p->in.music_view = music_view(p->in.music_scene, MusicFrame::Scene, W, H, p->dock_rect().y, -1, 0, p->music_words_w());  // (the app's framing, every frame)
      p->changed(t); run(*p, t, 1);
      // (the first frame may lag by one: the list is still invisible then, its fade at 0)
      if (i > 0 && p->list_header_ != p->list_layout().on) std::fprintf(stderr, "W %d H %d s %.1f frame %d hdr %d on %d mode %d mix %.2f\n", W, H, s, i, p->list_header_, p->list_layout().on, p->music_card_mode(), p->list_mix());
      if (i > 0) assert(p->list_header_ == p->list_layout().on);
    }
    assert(p->music_card_mode() == 0);
    delete p;
  }
  std::printf("music_small_test: ok (%d tall cases)\n", cases);
  return 0;
}
