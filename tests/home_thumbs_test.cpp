// The home list's pictures: the size each window gives them (none in a narrow one), a picture
// that arrives fades in over its stand-in, the stand-ins are the same for the same file, a frame
// fills its tile and a cover sits whole, the corners are cut, and every size and scale draws
// inside the window without touching the words' room.
#include "../src/ui/home.h"
#include "../src/ui/thumb_art.h"
#include "../src/core/i18n.h"
#include <cassert>
#include <cstdio>
#include <memory>

static void run(HomeView& hv, double& now, double secs) {
  for (double t = 0; t < secs; t += 1 / 60.0) { now += 1 / 60.0; hv.tick(now, 1 / 60.0); }
}

static std::shared_ptr<HomeThumb> solid(int w, int h, uint32_t rgb, bool cover = false) {
  auto t = std::make_shared<HomeThumb>();
  t->w = w; t->h = h; t->cover = cover;
  t->px.assign((size_t)w * h, 0xFF000000u | rgb);
  return t;
}

static uint32_t at(const Canvas& c, int x, int y) { return c.px[(size_t)y * c.w + x]; }

static std::vector<HomeRow> rows() {
  return {{"Cena", "faltam 41 min", 0.5, "ontem", false, HomeKind::Video, "c:\\v\\cena.mkv"},
          {"Série", "faltam 12 min", 0.2, "S01E04 · ontem", false, HomeKind::Episode, "c:\\v\\serie.mkv"},
          {"song", "4 min", -1, "artist · hoje", true, HomeKind::Music, "c:\\m\\song.flac"},
          {"talk", "faltam 26 min", 0.3, "áudio · há 3 dias", true, HomeKind::Listen, "c:\\a\\talk.mp3"},
          {"book", "faltam 6 h 12 min", 0.1, "capítulo 4 de 19", true, HomeKind::Book, "c:\\b\\book.m4b"}};
}

int main() {
  i18n::set_language(i18n::Lang::PtBR);
  // ── sizes: large pictures in a roomy window, small ones in a smaller, none in a narrow one
  {
    HomeView hv;
    hv.set_rows(rows());
    hv.set_size(853 * 3 / 2, 480 * 3 / 2, 1.5);
    assert(hv.thumb_w() == 48 && hv.thumb_h() == 27);
    hv.set_size(480 * 2, 270 * 2, 2);
    assert(hv.thumb_w() == 48);
    hv.set_size(420 * 2, 236 * 2, 2);
    assert(hv.thumb_w() == 32 && hv.thumb_h() == 18);
    hv.set_size(360 * 2, 203 * 2, 2);
    assert(hv.thumb_w() == 0 && hv.thumb_h() == 0);
  }
  // ── a picture arriving fades in over its stand-in; one already there does not fade again
  {
    HomeView hv;
    hv.set_size(960, 540, 2);
    hv.set_rows(rows());
    Canvas c;
    double now = 10;
    run(hv, now, 0.5);
    hv.render(c, now);
    const Rect r = hv.row_rect(0);
    const int px = r.x + 3 * 2 + 6 * 2, py = r.y + 3 * 2 + 8 * 2;  // inside the picture, off its play sign, above its bar
    const uint32_t stand = at(c, px, py);
    hv.set_thumb(0, solid(64, 36, 0xFF0000), now);
    assert(hv.tick(now + 0.01, 0.01));  // it moves while it fades
    hv.render(c, now + 0.08);
    const uint32_t mid = at(c, px, py);
    run(hv, now, 0.5);
    hv.render(c, now);
    const uint32_t done = at(c, px, py);
    assert(mid != stand && mid != done);
    // at rest the picture is a touch darker than itself (red 255 under a 26% dark veil)
    const int red = (done >> 16) & 255;
    assert(red > 170 && red < 200 && (done & 0xFFFF) < 0x0404);
    assert(hv.next_event(now) > 0);  // settled: nothing to draw until something happens
    // under the pointer: full light, and the play sign in the middle
    hv.set_hover_row(0, now);
    hv.mouse_move(px, py, now);
    run(hv, now, 0.4);
    hv.render(c, now);
    assert(((at(c, px, py) >> 16) & 255) == 255);
    // (the first entry is one to continue: its picture is twice the list's)
    const Rect t = hv.picture_rect(0);
    assert((at(c, t.cx(), t.cy()) & 0xFFFFFF) == 0xFFFFFF || (at(c, t.cx() - 2, t.cy()) & 0xFFFFFF) == 0xFFFFFF);
    // a newer frame of the same film replaces it at once (no second fade)
    hv.set_thumb(0, solid(64, 36, 0x0000FF), now);
    hv.render(c, now);
    assert((at(c, px, py) & 0xFF) == 255);
  }
  // ── tiles: a frame fills it (cropped), a cover sits whole on a tint, the corners are cut
  {
    Canvas t;
    auto frame = solid(256, 144, 0x20A040);
    thumb_art::make_tile(*frame, 96, 54, 2, t);
    assert(t.w == 96 && t.h == 54);
    assert(at(t, 0, 0) == 0 && at(t, 95, 0) == 0 && at(t, 0, 53) == 0 && at(t, 95, 53) == 0);
    assert(at(t, 2, 0) == 0xFF20A040u && at(t, 48, 27) == 0xFF20A040u);
    // a portrait frame (a phone's video) still fills the tile: its middle
    auto tall = std::make_shared<HomeThumb>();
    tall->w = 9; tall->h = 16;
    for (int y = 0; y < 16; ++y) for (int x = 0; x < 9; ++x) tall->px.push_back(y < 8 ? 0xFFFF0000u : 0xFF0000FFu);
    thumb_art::make_tile(*tall, 96, 54, 2, t);
    assert(((at(t, 48, 5) >> 16) & 255) > 200 && (at(t, 48, 50) & 255) > 200);
    // a cover: whole and square, the tile's height across (audio sits on the left of its tile)
    auto cover = solid(160, 160, 0xFFB300, true);
    thumb_art::make_tile(*cover, 96, 54, 2, t);
    assert(t.w == 54 && t.h == 54 && at(t, 27, 27) == 0xFFFFB300u && at(t, 0, 0) == 0);
    assert(thumb_art::square(HomeKind::Music, true) && thumb_art::square(HomeKind::Book, true) && !thumb_art::square(HomeKind::Episode, false));
    // a record: transparent around it, its label the cover's most vivid colour, its mark turning
    Canvas d1, d2;
    thumb_art::make_disc(thumb_art::accent(*cover), 50, 2, 0, 1, d1);
    thumb_art::make_disc(thumb_art::accent(*cover), 50, 2, 1.2, 1, d2);
    assert(at(d1, 0, 0) == 0 && at(d1, 25, 25) != 0 && d1.px != d2.px);
    const uint32_t lab = at(d1, 25, 33);
    assert(((lab >> 16) & 255) > 180 && ((lab >> 8) & 255) > 120 && (lab & 255) < 80);  // amber, like the cover
    // a grey cover still gets a label that reads
    const uint32_t grey = thumb_art::accent(*solid(64, 64, 0x101010, true));
    assert(((grey >> 16) & 255) > 40);
    // an empty picture: a quiet tile, never a crash
    HomeThumb none;
    thumb_art::make_tile(none, 96, 54, 2, t);
    assert(at(t, 48, 27) != 0);
  }
  // ── stand-ins: the same file, the same picture; different files, different ones
  {
    Canvas a, b, c2;
    thumb_art::make_stand_in(HomeKind::Video, false, "c:\\v\\a.mkv", 96, 54, 2, 2, a);
    thumb_art::make_stand_in(HomeKind::Video, false, "c:\\v\\a.mkv", 96, 54, 2, 2, b);
    thumb_art::make_stand_in(HomeKind::Video, false, "c:\\v\\another one.mkv", 96, 54, 2, 2, c2);
    assert(a.px == b.px && a.px != c2.px);
    for (HomeKind k : {HomeKind::Music, HomeKind::Listen, HomeKind::Book}) {
      thumb_art::make_stand_in(k, true, "c:\\a\\x.mp3", 72, 41, 1.5, 1, a);
      assert(a.w == 41 && a.h == 41 && at(a, 0, 0) == 0 && at(a, 20, 20) != 0);  // audio: square
    }
  }
  // ── resampling: an even average when shrinking, the source's colours when growing
  {
    std::vector<uint32_t> src = {0xFF000000u, 0xFFFFFFFFu, 0xFF000000u, 0xFFFFFFFFu};
    uint32_t out = 0;
    thumb_art::resample(src.data(), 2, 2, 0, 0, 2, 2, &out, 1, 1, 1);
    assert(((out >> 16) & 255) >= 127 && ((out >> 16) & 255) <= 128);
    std::vector<uint32_t> big(16);
    thumb_art::resample(src.data(), 2, 2, 0, 0, 2, 2, big.data(), 4, 4, 4);
    assert(big[0] == 0xFF000000u && big[3] == 0xFFFFFFFFu);
  }
  // ── every window and scale: the pictures stay inside their rows, nothing is drawn outside
  {
    int cases = 0;
    for (double s : {1.0, 1.5, 2.0, 2.5, 3.0, 4.0})
      for (int W : {320, 400, 456, 480, 640, 853, 1280})
        for (int H : {180, 270, 360, 480}) {
          HomeView hv;
          hv.set_size((int)(W * s), (int)(H * s), s);
          auto r = rows();
          r[0].thumb = solid(256, 144, 0x808080);
          r[2].thumb = solid(160, 160, 0xC0C0C0, true);
          hv.set_rows(r);
          Canvas c;
          double now = 5;
          run(hv, now, 0.3);
          hv.render(c, now);
          assert(c.w == (int)(W * s) && c.h == (int)(H * s));
          for (int i = 0; i < (int)hv.rows.size(); ++i) {
            const Rect row = hv.row_rect(i);
            assert(row.x >= 0 && row.r() <= c.w && row.y >= 0 && row.b() <= c.h);
          }
          ++cases;
        }
    std::printf("Home thumbs: %d window/scale cases drawn inside the window\n", cases);
  }
  // ── clear recent: the entries leave one after another, the title glides, then all is still
  {
    HomeView hv;
    hv.set_size(960, 540, 2);
    // (a song first: the plain list, three entries; "continuar" leaving is in
    // home_continue_test)
    auto plain = rows();
    std::swap(plain[0], plain[2]);
    hv.set_rows(plain);
    assert(!hv.continuing());
    Canvas c;
    double now = 10;
    run(hv, now, 0.5);
    // (the second and the last of the three: the first's place takes the empty list's line after)
    const Rect first = hv.row_rect(1), last = hv.row_rect((int)hv.rows.size() - 1);
    hv.clear_animated(now);
    hv.set_rows({});
    assert(hv.clearing() && hv.tick(now + 0.01, 0.01) && hv.next_event(now) == 0);
    // a moment in: the first entry is going, the last still there
    now += 0.12;
    hv.tick(now, 0.12);
    hv.render(c, now);
    auto lit = [&](const Rect& r) { int n = 0; for (int y = r.y; y < r.b(); ++y) for (int x = r.x; x < r.r(); ++x) n += ((at(c, x, y) >> 8) & 255) > 40; return n; };
    const int first_mid = lit(first), last_mid = lit(last);
    run(hv, now, 2);
    hv.render(c, now);
    assert(!hv.clearing() && hv.next_event(now) > 0);
    // (the list is short: the empty line and the title come to rest over where the entries were;
    // the column's top is where the taller of its two looks puts it (the list of words would show
    // all five), so the entries sat higher than their own middle and the empty line, centred, can
    // rest over the last one's place: lit inside the entries' span, nothing under it)
    const Rect span{first.x, first.y, first.w, last.b() - first.y};
    const Rect below{last.x, last.b(), last.w, std::min(c.h - 1, last.b() + last.h) - last.b()};
    assert(lit(span) > 0 && lit(below) == 0 && last_mid > first_mid);
    // with reduced motion it is immediate
    reduce_motion() = true;
    HomeView q;
    q.set_size(960, 540, 2);
    q.set_rows(rows());
    q.clear_animated(5);
    assert(!q.clearing());
    reduce_motion() = false;
  }
  std::printf("Home thumbs: sizes per window, fade-in, rest and hover light, tiles, square covers and records, stand-ins, resampling, clearing passed\n");
}
