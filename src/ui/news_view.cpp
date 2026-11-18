#include "news_view.h"

#include <algorithm>
#include <cmath>

#include "../core/i18n.h"
#include "light.h"
#include "thumb_art.h"

namespace {
// What arrived, in the order it is told: a name and two short lines, each on its own card with its
// colour on top. The cards are equal: the same width, the same height, the same anatomy.
struct Item { str::Id title, line; Rgb edge; };
constexpr int kItemCount = 3;
const Item kItems[kItemCount] = {
    {str::news_home_title, str::news_home_line, {155, 92, 214}},
    {str::news_chapters_title, str::news_chapters_line, {64, 112, 226}},
    {str::news_light_title, str::news_light_line, {232, 138, 60}},
};
constexpr int kCapTop = 2;      // art px: where the capitals start in a line of text
constexpr int kPad = 10;        // inside a card
constexpr int kIllus = 50;      // the miniature's height
constexpr int kGap = 12;        // between cards
constexpr int kGlowReach = 18;  // a card's own light, art px around it
// the entrance, in seconds from the first frame
constexpr double kSparkIn = 0.12, kTitleIn = 0.22, kCharStep = 0.028, kCharDur = 0.22;
constexpr double kCardIn = 0.78, kCardStep = 0.1, kCardRise = 0.42, kStripIn = 0.12, kStripDur = 0.3, kMiniIn = 0.28;
constexpr double kButtonIn = 1.35;
// sparkles around the eyes, from the eyes' centre in art px: big ones and small ones
struct Star { int x, y; int size; double phase; };
const Star kStars[] = {{-62, -2, 2, 0.0}, {-44, -20, 1, 0.5}, {50, -22, 2, 0.9}, {66, 2, 1, 0.3}, {82, 14, 0, 0.7}, {-80, 14, 0, 1.1}};
constexpr int kStarCount = (int)(sizeof kStars / sizeof kStars[0]);

double clamp01d(double t) { return t < 0 ? 0 : t > 1 ? 1 : t; }
// a quick arrival that goes a little past and settles back (Penner's back-out)
double back_out(double t) {
  t = clamp01d(t);
  const double c1 = 1.70158, c3 = c1 + 1, u = t - 1;
  return 1 + c3 * u * u * u + c1 * u * u;
}
int cps(const std::string& s) {
  int n = 0;
  for (size_t i = 0; i < s.size();) { next_cp(s, i); ++n; }
  return n;
}
}  // namespace

void NewsView::set_size(int w_, int h_, double s_) {
  if (w_ <= 0 || h_ <= 0) return;
  w = w_;
  h = h_;
  ui_s = std::max(1.0, s_);
  // a comfortable read, like the welcome: about 720 x 405 art px fill the window
  double fit = std::floor(std::min(w / 720.0, h / 405.0) * 2) / 2;
  s = std::max(ui_s, std::min(std::max(1.0, fit), ui_s * 1.5));
  layout();
  while (overflow_ && s > 1) { s = std::max(1.0, s - 0.5); layout(); }
  changed_ = true;
}

void NewsView::layout() {
  released_ = false;
  const double W = w / s, H = h / s;
  const double CW = std::max(60.0, std::min(W - 32, 540.0));
  auto R = [&](double x, double y, double ww, double hh) { return Rect(x * s, y * s, ww * s, hh * s); };
  ts_ = std::max(2 * s, std::round(2.5 * s));
  // three cards side by side while each keeps room for its words; one column in a narrow window
  double cw3 = (CW - 2 * kGap) / 3;
  cols_ = cw3 >= 148 ? 3 : 1;
  const double cardw = cols_ == 3 ? std::floor(cw3) : std::min(CW, 300.0);
  const double iw = cardw - 2 * kPad;
  lines_.clear();
  size_t n = 1;
  for (int i = 0; i < kItemCount; ++i) {
    lines_.push_back(wrap_lines(tr(kItems[i].line), (int)std::floor(iw * s), s));
    n = std::max(n, lines_.back().size());
  }
  const double card_h = 2 + kPad + kIllus + kPad + 12 + 4 + 12.0 * n + kPad;
  const int rows = cols_ == 3 ? 1 : kItemCount;
  const double cards_h = rows * card_h + (rows - 1) * kGap;
  const double title_h = 12 * ts_ / s;
  // the eyes, the heading with its badge, its line, the cards, the button
  const double total = 16 + 16 + title_h + 4 + 12 + 22 + cards_h + 22 + 16;
  const double top = std::max(14.0, std::floor((H - total) / 2));
  overflow_ = top + total > H - 8;
  double y = top;
  face_.layout(w / 2.0, (y + 16) * s, s, 8 / 3.0);
  face_.max_look = 2.0;
  y += 16 + 16;
  head_y_ = (int)std::lround(y * s);
  y += title_h + 4;
  line_y_ = (int)std::lround(y * s);
  y += 12 + 22;
  items_.clear();
  const double total_w = cols_ == 3 ? 3 * cardw + 2 * kGap : cardw;
  const double x0 = std::round(W / 2 - total_w / 2);
  for (int i = 0; i < kItemCount; ++i) {
    if (cols_ == 3) items_.push_back(R(x0 + i * (cardw + kGap), y, cardw, card_h));
    else items_.push_back(R(x0, y + i * (card_h + kGap), cardw, card_h));
  }
  y += cards_h + 22;
  double bw = text_w(tr(str::ob_continue), 1) + 9 + 22;
  buttons_[kContinue] = R(std::round(W / 2 - bw / 2), y, bw, 16);

  // the eyes' light, wide behind them and the heading (made once here)
  {
    const double rx = std::min(CW * 0.5, 260.0), ry = 70;
    const double cy = face_.eye_y() / s + 10;
    light_r_ = R(W / 2 - rx, cy - ry, 2 * rx, 2 * ry);
    // (made again only for a new size: opening right after being laid out costs nothing more)
    if (light_.w != light_r_.w || light_.h != light_r_.h) {
      radial_light(light_, light_r_.w, light_r_.h, kEyeCool, kEyeMid, kEyeWarm, 0.42);
      compose_on(light_base_, light_, P().bg, 0.97);
    }
  }
  // the miniatures' pictures: Cue's own stand-ins for a film, an episode and another film
  {
    const int tw = (int)std::lround(56 * s), th = (int)std::lround(32 * s), cut = std::max(1, (int)s);
    if (thumbs_[0].w != tw || thumbs_[0].h != th || thumbs_s_ != s) {
      thumbs_s_ = s;
      thumb_art::make_stand_in(HomeKind::Video, false, "terra", tw, th, s, cut, thumbs_[0]);
      thumb_art::make_stand_in(HomeKind::Episode, false, "orbita", tw, th, s, cut, thumbs_[1]);
      thumb_art::make_stand_in(HomeKind::Video, false, "eclipse", tw, th, s, cut, thumbs_[2]);
    }
  }
  // the cards' glows are made again when a card's size changed (they follow it when it only moves)
  for (int i = 0; i < kCards && i < (int)items_.size(); ++i)
    if (card_light_[i].w != items_[i].w + 2 * (int)std::lround(kGlowReach * s) ||
        card_light_[i].h != items_[i].h + 2 * (int)std::lround(kGlowReach * s))
      card_light_[i] = Canvas{};
    else
      card_light_r_[i] = {items_[i].x - (int)std::lround(kGlowReach * s), items_[i].y - (int)std::lround(kGlowReach * s), card_light_[i].w, card_light_[i].h};
}

// Closed: the pictures it made (the light, the glows, the miniatures) are let go; the next open
// makes them again.
void NewsView::release() {
  if (released_) return;
  released_ = true;
  light_ = Canvas{};
  light_base_ = Canvas{};
  for (auto& t : thumbs_) t = Canvas{};
  for (auto& cl : card_light_) cl = Canvas{};
  sparks_.clear();
  sparks_.shrink_to_fit();
}

void NewsView::open(double now) {
  open_t0_ = now;
  close_t0_ = -1;
  poke_t0_ = -10;
  hover_ = focus_ = hover_card_ = -1;
  kb_ = false;
  face_ = Face{};
  sparks_.clear();
  for (int i = 0; i < kCards; ++i) {
    lift_[i] = Spring::rd(0.26, 0.72, 0.02);
    lift_[i].snap(0);
    glow_[i] = 0;
    card_t0_[i] = -10;
  }
  layout();
  face_.set_mode(Face::Mode::Follow, now);
  face_.happy = true;
  changed_ = true;
}

bool NewsView::fast(double now) const {
  if (close_t0_ > 0 || !sparks_.empty() || (!reduce_motion() && entrance_t(now) < kEntrance)) return true;
  for (int i = 0; i < kCards; ++i) {
    if (!lift_[i].settled() || (glow_[i] > 0 && glow_[i] < 1)) return true;
    if (!reduce_motion() && now - card_t0_[i] < 1.4) return true;  // a miniature playing again
  }
  return false;
}

// What the clock changes on screen while nothing else moves: the sparkles' twinkle.
uint64_t NewsView::look(double now) const {
  uint64_t k = 0;
  if (!reduce_motion())
    for (const Star& st : kStars) k = k * 2 + (std::fmod(now * 0.75 + st.phase, 1.0) < 0.5 ? 1 : 0);
  return k;
}

bool NewsView::tick(double now, double dt) {
  if (closed(now)) { release(); return false; }
  bool moving = false;
  const double t = entrance_t(now);
  face_.happy = true;  // happy to see you (a new version)
  // the eyes hop as they arrive; then they follow the card under the pointer, or the pointer
  if (t >= 0.05 && t - dt < 0.05 && !reduce_motion()) face_.notice(now);
  if (hover_card_ >= 0 && hover_card_ < (int)items_.size()) face_.look_at(items_[hover_card_].cx(), items_[hover_card_].y, 0.9);
  moving |= face_.tick(now, dt);
  const int active = kb_ ? focus_ : hover_;
  const double hv = hover_step(hov_[kContinue], active == kContinue, dt);
  if (hv != hov_[kContinue]) { hov_[kContinue] = hv; moving = true; }
  for (int i = 0; i < kCards; ++i) {
    lift_[i].target = i == hover_card_ ? -2 : 0;
    moving |= lift_[i].step(dt);
    const double g = hover_step(glow_[i], i == hover_card_, dt);
    if (g != glow_[i]) { glow_[i] = g; moving = true; }
  }
  // a poke's sparks: fly, fall, fade
  if (!sparks_.empty()) {
    sparks_.erase(std::remove_if(sparks_.begin(), sparks_.end(), [&](const Spark& sp) { return now - sp.t0 > sp.life; }), sparks_.end());
    moving = true;
  }
  const uint64_t k = look(now);
  if (k != look_key_) { look_key_ = k; moving = true; }
  if (fast(now)) moving = true;
  // after the entrance, while nothing moves, the cards' glows are made ahead, one per frame, so
  // the first time the pointer comes to a card nothing is computed then
  if (!moving && t > kEntrance && hover_card_ < 0)
    for (int i = 0; i < kCards; ++i)
      if (card_light_[i].w == 0 && i < (int)items_.size()) { make_card_light(i); break; }
  changed_ = changed_ || moving;
  return moving;
}

void NewsView::make_card_light(int i) {
  const int reach = (int)std::lround(kGlowReach * s);
  const Rect& r = items_[i];
  card_light_r_[i] = {r.x - reach, r.y - reach, r.w + 2 * reach, r.h + 2 * reach};
  const Rgb corner[4] = {kItems[i].edge, kItems[i].edge, kItems[i].edge, kItems[i].edge};
  rect_light(card_light_[i], card_light_r_[i].w, card_light_r_[i].h, {reach, reach, r.w, r.h}, reach, corner, 0.42);
}

void NewsView::draw_button(Canvas& c, const Rect& r, const std::string& label, bool primary, double hv, double a,
                           bool chevron) const {
  const Palette& p = P();
  if (primary) c.cornered(r, (int)s, mix(p.hover, p.press, hv), a);
  else if (hv > 0) c.cornered(r, (int)s, p.hover, a * hv);
  int tw = text_w(label, s) + (chevron ? (int)std::lround(9 * s) : 0);
  int x = r.x + (r.w - tw) / 2, y = r.y + (int)std::lround(3 * s);
  Rgb col = primary ? mix(p.fg, p.pixel, hv) : mix(p.dim, p.fg, hv);
  text(c, label, x, y, s, col, a);
  if (chevron) {
    int nudge = (int)std::lround(hv * s);
    c.bitmap(bmp("chev-right5"), x + tw - (int)std::lround(3 * s) + nudge, y + (int)std::lround(2 * s), s, col, a);
  }
}

// Pixel sparkles: they fly out of the eyes to their places as the screen opens (going a little
// past and settling back), then twinkle now and then, by brightness only. A poke throws a few
// more, which fall as they fade.
void NewsView::draw_sparkles(Canvas& c, double now) const {
  const Palette& p = P();
  const int u = std::max(1, (int)std::lround(s));
  const int ex = w / 2, ey = (int)std::lround(face_.eye_y());
  const double t = entrance_t(now);
  const bool poked = now - poke_t0_ < 0.6;
  auto plus = [&](int x, int y, int arm, Rgb ink, double a) {
    if (arm == 0) { c.fill(x, y, u, u, ink, a); return; }
    c.fill(x - arm * u, y, (2 * arm + 1) * u, u, ink, a);
    c.fill(x, y - arm * u, u, (2 * arm + 1) * u, ink, a);
  };
  for (int i = 0; i < kStarCount; ++i) {
    const Star& st = kStars[i];
    const double k = reduce_motion() ? 1.0 : clamp01d((t - kSparkIn - 0.03 * i) / 0.45);
    if (k <= 0) continue;
    const double e = back_out(k);
    // on the art grid all the way there
    const int x = ex + (int)std::lround(st.x * e) * u, y = ey + (int)std::lround(st.y * e) * u;
    const bool on = reduce_motion() || poked || k < 1 || std::fmod(now * 0.75 + st.phase, 1.0) < 0.5;
    const int arm = st.size == 2 ? 3 : st.size == 1 ? 1 : 0;
    plus(x, y, arm, on ? p.pixel : st.size ? p.dim : p.faint, std::min(1.0, k * 3));
  }
  for (const Spark& sp : sparks_) {
    const double kt = now - sp.t0;
    const double x = sp.x + sp.vx * kt, y = sp.y + sp.vy * kt + 0.5 * 140 * kt * kt;
    const double a = 1 - clamp01d((kt - sp.life * 0.5) / (sp.life * 0.5));
    plus(ex + (int)std::lround(x) * u, ey + (int)std::lround(y) * u, sp.big, sp.big ? p.pixel : p.fg, a);
  }
}

// The heading drops in letter by letter; the version is stamped beside it (its outline first,
// then filled), with a flash of sparkles at its corners; the line under it comes in last.
void NewsView::draw_heading(Canvas& c, double now) const {
  const Palette& p = P();
  const int u = std::max(1, (int)std::lround(s));
  const double t = entrance_t(now);
  const std::string head = tr(str::news_title);
  const double vs = 2 * s;
  const int hw = text_w(head, ts_);
  const int badge_w = text_w(version, vs) + (int)std::lround(12 * s), gap = (int)std::lround(10 * s);
  const int x0 = w / 2 - (hw + gap + badge_w) / 2, y = head_y_;
  const int n = cps(head);
  if (reduce_motion() || t >= kTitleIn + n * kCharStep + kCharDur) {
    text(c, head, x0, y, ts_, p.pixel);
  } else {
    size_t i = 0;
    for (int k = 0; k < n && i < head.size(); ++k) {
      const size_t from = i;
      next_cp(head, i);
      const double e = clamp01d((t - kTitleIn - k * kCharStep) / kCharDur);
      if (e <= 0) break;
      const int dx = text_w(std::string_view(head).substr(0, from), ts_);
      const int drop = (int)std::lround((1 - back_out(e)) * -6) * u;
      text(c, std::string_view(head).substr(from, i - from), x0 + dx, y + drop, ts_, p.pixel, std::min(1.0, e * 2));
    }
  }
  // the badge: as tall as the heading's capitals and a little more, the capitals in its middle
  const double tb = kTitleIn + n * kCharStep + 0.06;
  const double kb = reduce_motion() ? 1.0 : clamp01d((t - tb) / 0.2);
  if (kb > 0) {
    const int caps_mid = y + (int)std::lround((kCapTop + 3.5) * ts_);
    const int bh = (int)std::lround(7 * vs + 10 * s);
    const int drop = (int)std::lround((1 - back_out(kb)) * -4) * u;
    Rect badge{x0 + hw + gap, caps_mid - bh / 2 + drop, badge_w, bh};
    if (kb < 0.3) {
      // stamped: the outline arrives first
      c.fill(badge.x + u, badge.y, badge.w - 2 * u, u, p.fg);
      c.fill(badge.x + u, badge.b() - u, badge.w - 2 * u, u, p.fg);
      c.fill(badge.x, badge.y + u, u, badge.h - 2 * u, p.fg);
      c.fill(badge.r() - u, badge.y + u, u, badge.h - 2 * u, p.fg);
    } else {
      c.cornered(badge, u, p.fg);
      text(c, version, badge.x + (int)std::lround(6 * s), caps_mid - (int)std::lround((kCapTop + 3.5) * vs) + drop, vs, p.bg);
    }
    // a flash at its corners as it lands
    const double fl = reduce_motion() ? 0.0 : clamp01d((t - tb - 0.06) / 0.3);
    if (fl > 0 && fl < 1) {
      const double fa = 1 - fl;
      const int out = (int)std::lround(2 + fl * 3) * u;
      const int xs[2] = {badge.x - out, badge.r() + out - u}, ys[2] = {badge.y - out, badge.b() + out - u};
      for (int xi : xs) for (int yi : ys) { c.fill(xi - u, yi, 3 * u, u, p.pixel, fa); c.fill(xi, yi - u, u, 3 * u, p.pixel, fa); }
    }
  }
  const double kl = reduce_motion() ? 1.0 : ease_out(clamp01d((t - tb - 0.1) / 0.34));
  if (kl > 0) {
    const std::string line = tr(str::news_line);
    const int lift = (int)std::lround((1 - kl) * 4) * u;
    text(c, line, w / 2 - text_w(line, s) / 2, line_y_ + lift, s, p.dim, kl);
  }
}

// A card: black, a thin edge, stepped corners, its colour drawn along the top; the feature in
// miniature, its name, its lines. Under the pointer its edge takes its colour.
void NewsView::draw_card(Canvas& c, int i, const Rect& r, double a, double now) {
  const Palette& p = P();
  const Rgb col = kItems[i].edge;
  const int u = std::max(1, (int)std::lround(s));
  const double t = entrance_t(now) - (kCardIn + kCardStep * i);  // since this card arrived
  const Rgb edge = mix(p.edge, mix(col, p.fg, 0.2), 0.6 * glow_[i]);
  draw_panel(c, r.x, r.y, (int)std::lround(r.w / s), (int)std::lround(r.h / s), s, {2, 1}, p.screen, a, edge, a);
  // its colour, drawn from the left as it arrives
  const double sk = reduce_motion() ? 1.0 : ease_out(clamp01d((t - kStripIn) / kStripDur));
  if (sk > 0) {
    const int sw = (int)std::lround((r.w - 2 * s) * sk / u) * u;
    c.fill(r.x + 2 * s, r.y, std::max(0, std::min(sw, r.w - 4 * (int)s)), s, col, a);
    c.fill(r.x + s, r.y + s, sw, s, col, a);
  }
  const Rect il{r.x + (int)std::lround(kPad * s), r.y + (int)std::lround((2 + kPad) * s), r.w - (int)std::lround(2 * kPad * s),
                (int)std::lround(kIllus * s)};
  // its miniature plays once the card is in, and again each time the pointer comes to it
  const double lt = reduce_motion() ? 10.0 : std::min(t - kMiniIn, now - card_t0_[i]);
  auto bar = [&](int x, int y, int w_, double f) {
    c.fill(x, y, w_, 2 * u, p.faint, a);
    c.fill(x, y, (int)std::lround(w_ * f / u) * u, 2 * u, p.fg, a);
  };
  if (i == 0) {
    // the home's list: two pictures, how far each one is, and what is left
    const Canvas& t0 = thumbs_[0];
    const int gap = 12 * u, gw = 2 * t0.w + gap, x0 = il.x + (il.w - gw) / 2;
    const double fr[2] = {0.62, 0.3};
    const int left[2] = {32, 12};
    for (int k = 0; k < 2; ++k) {
      const int x = x0 + k * (t0.w + gap), y = il.y;
      const double e = ease_out(clamp01d((lt - 0.12 * k) / 0.7));
      c.blit(thumbs_[k], x, y, a);
      bar(x, y + t0.h + u, t0.w, fr[k] * e);
      const std::string l = i18n::format(str::home_left, {{"time", i18n::format(str::home_minutes, {{"m", std::to_string(left[k])}})}});
      const std::string fit = fit_end(l, t0.w + gap - 2 * u, s);
      text(c, fit, x + (t0.w - text_w(fit, s)) / 2, y + t0.h + 4 * u, s, p.dim, a * clamp01d((lt - 0.12 * k - 0.35) / 0.3));
    }
  } else if (i == 1) {
    // chapters: a picture with the chapter's name on it, the timeline broken into chapters; the
    // playhead walks into the second one, the name turning as it crosses
    const Canvas& pic = thumbs_[2];
    const int x = il.x + (il.w - pic.w) / 2, y = il.y;
    const double k = ease_out(clamp01d((lt - 0.1) / 1.1));
    const double play = 0.12 + 0.43 * k;
    const double cuts[2] = {0.3, 0.7};
    const int nchap = play < cuts[0] ? 1 : play < cuts[1] ? 2 : 3;
    c.blit(pic, x, y, a);
    const std::string cap = i18n::format(str::toast_chapter, {{"n", std::to_string(nchap)}});
    const int band = 12 * u;
    c.fill(x + u, y + pic.h - band, pic.w - 2 * u, band, p.screen, a * 0.72);
    text(c, fit_end(cap, pic.w - 4 * u, s), x + (pic.w - std::min(text_w(cap, s), pic.w - 4 * u)) / 2, y + pic.h - band - kCapTop * u + u, s, p.fg, a);
    const int lw = std::min(il.w, 120 * u), lx = il.x + (il.w - lw) / 2, ly = y + pic.h + 8 * u;
    const int c0 = lx + (int)std::lround(lw * cuts[0] / u) * u, c1 = lx + (int)std::lround(lw * cuts[1] / u) * u;
    const int px = lx + (int)std::lround(lw * play / u) * u;
    const int seg[3][2] = {{lx, c0 - u}, {c0 + u, c1 - u}, {c1 + u, lx + lw}};
    for (auto& sg : seg) {
      c.fill(sg[0], ly, sg[1] - sg[0], 2 * u, p.faint, a);
      if (px > sg[0]) c.fill(sg[0], ly, std::min(px, sg[1]) - sg[0], 2 * u, p.fg, a);
    }
    c.fill(px - u, ly - 2 * u, 2 * u, 6 * u, p.pixel, a);
  } else {
    // lighter and faster: blocks that light up one after the other, the last one in the card's colour
    const int nb = 6, bw = 9 * u, gap = 4 * u;
    const int hs[nb] = {8, 10, 12, 15, 18, 26};
    const int gw = nb * bw + (nb - 1) * gap + 5 * u;
    int x = il.x + (il.w - gw) / 2;
    const int base = il.y + il.h - 10 * u;
    for (int k = 0; k < nb; ++k) {
      const double on = clamp01d((lt - 0.07 * k) / 0.12);
      const int bh = hs[k] * u, ww = k == nb - 1 ? bw + 5 * u : bw;
      const Rgb bc = mix(mix(p.band, p.faint, 0.6), col, k / (nb - 1.0) * 0.85 + (k == nb - 1 ? 0.15 : 0));
      c.fill(x, base - bh, ww, bh, bc, a * on);
      if (k == nb - 1) {
        c.fill(x, base - bh - 5 * u, ww, u, col, a * on);
        c.fill(x, base + 4 * u, ww, u, col, a * on);
      }
      x += bw + gap;
    }
  }
  const int tx = r.x + (int)std::lround(kPad * s);
  const int ty = r.y + (int)std::lround((2 + kPad + kIllus + kPad) * s);
  text(c, fit_end(tr(kItems[i].title), r.w - (int)std::lround(2 * kPad * s), s), tx, ty, s, p.pixel, a);
  int ly = ty + (int)std::lround(16 * s);
  for (auto& l : lines_[i]) { text(c, l, tx, ly, s, p.dim, a); ly += (int)std::lround(12 * s); }
}

void NewsView::render(Canvas& c, double now) {
  const Palette& p = P();
  if (c.w != w || c.h != h) c.resize(w, h);
  changed_ = false;
  if (released_) layout();  // (drawn again after it was let go: a late frame)
  const double t = entrance_t(now);
  const double enter = reduce_motion() ? 1.0 : ease_out(t / 0.28);
  const double leave = close_t0_ > 0 ? 1.0 - clamp01d((now - close_t0_) / kCloseDur) : 1.0;
  c.clear(p.bg, 0.97);
  auto at = [&](double delay, double dur = 0.34) { return reduce_motion() ? 1.0 : ease_out(clamp01d((t - delay) / dur)); };
  const int u = std::max(1, (int)std::lround(s));

  // the eyes' light blooms first (then it is copied in place, already on the ground); the eyes;
  // the sparkles out of them
  const double la = at(0.0, 0.4);
  if (la >= 0.999 && light_base_.w == light_r_.w && light_base_.h == light_r_.h) paste(c, light_base_, light_r_.x, light_r_.y);
  else c.blit(light_, light_r_.x, light_r_.y, la);
  draw_sparkles(c, now);
  face_.now_hint = now;
  face_.render(c, at(0.05, 0.2));
  draw_heading(c, now);

  // the cards: their own light first (only under the one being pointed at), then the cards,
  // rising into place one after the other
  for (size_t i = 0; i < items_.size() && i < (size_t)kCards; ++i) {
    if (glow_[i] <= 0.003) continue;
    if (card_light_[i].w == 0) make_card_light((int)i);
    const int dy = (int)std::lround(lift_[i].x) * u;
    c.blit(card_light_[i], card_light_r_[i].x, card_light_r_[i].y + dy, glow_[i]);
  }
  for (size_t i = 0; i < items_.size() && i < (size_t)kCards; ++i) {
    const double ct = t - (kCardIn + kCardStep * i);
    const double k = reduce_motion() ? 1.0 : clamp01d(ct / kCardRise);
    if (k <= 0.003) continue;
    Rect r = items_[i];
    r.y += (int)std::lround((1 - back_out(k)) * 10 + lift_[i].x) * u;
    draw_card(c, (int)i, r, std::min(1.0, k * 2.5), now);
  }

  const double kb = at(kButtonIn);
  draw_button(c, buttons_[kContinue], tr(str::ob_continue), true, hov_[kContinue], kb, true);
  if (kb_ && focus_ >= 0) {
    // keyboard focus: a 1 px ring on the art grid
    Rect r = buttons_[focus_].grow((int)s);
    int a = (int)s;
    c.fill(r.x + a, r.y, r.w - 2 * a, a, p.dim, kb);
    c.fill(r.x + a, r.b() - a, r.w - 2 * a, a, p.dim, kb);
    c.fill(r.x, r.y + a, a, r.h - 2 * a, p.dim, kb);
    c.fill(r.r() - a, r.y + a, a, r.h - 2 * a, p.dim, kb);
  }
  const double alpha = enter * leave;
  if (alpha < 0.999) c.scale_alpha(alpha);
}

int NewsView::hit(int x, int y) const {
  if (buttons_[kContinue].grow((int)(2 * s)).contains(x, y)) return kContinue;
  return -1;
}

int NewsView::card_at(int x, int y) const {
  for (int i = 0; i < (int)items_.size() && i < kCards; ++i)
    if (items_[i].contains(x, y)) return i;
  return -1;
}

void NewsView::pointer_move(int x, int y, double now) {
  kb_ = false;
  const int hv = hit(x, y);
  if (hv != hover_) changed_ = true;
  hover_ = hv;
  // a card is only alive once it has arrived
  int cd = card_at(x, y);
  if (cd >= 0 && entrance_t(now) < kCardIn + kCardStep * cd + kCardRise) cd = -1;
  if (cd != hover_card_) {
    hover_card_ = cd;
    if (cd >= 0) card_t0_[cd] = now;  // its miniature plays again
    changed_ = true;
  }
  if (cd < 0) face_.look_at(x, y, 0.8);
}

void NewsView::pointer_leave(double now) {
  (void)now;
  hover_ = -1;
  hover_card_ = -1;
  changed_ = true;
}

SceneAct NewsView::pointer_down(int x, int y, double now) {
  const int id = hit(x, y);
  if (id == kContinue) return {(int)NewsAct::Continue, -1};
  if (face_.bounds().contains(x, y)) {
    // a poke: it hops, the sparkles all light up and a few more fly out (never a pile of them)
    face_.notice(now);
    poke_t0_ = now;
    changed_ = true;
    if (!reduce_motion() && sparks_.size() < 24)
      for (int k = 0; k < 10; ++k) {
        const double ang = rng_.range(-3.1, 0.0) + rng_.range(-0.3, 0.3), sp = rng_.range(40.0, 95.0);
        sparks_.push_back({0, 0, std::cos(ang) * sp, std::sin(ang) * sp, now, rng_.range(0.5, 0.8), rng_.pick(3) == 0 ? 1 : 0});
      }
    return {};
  }
  const int cd = card_at(x, y);
  if (cd >= 0 && cd == hover_card_) { card_t0_[cd] = now; changed_ = true; }  // a click on a card: once more
  return {};
}

SceneAct NewsView::key_down(const std::string& k, bool shift, double now) {
  (void)now; (void)shift;
  if (k == "esc") return {(int)NewsAct::Continue, -1};
  if (k == "tab" || k == "left" || k == "right") {
    kb_ = true;
    focus_ = kContinue;
    changed_ = true;
    return {};
  }
  if (k == "enter" || k == "space") return {(int)NewsAct::Continue, -1};
  return {};
}
