#include "gfx.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <type_traits>
#if defined(__SSE2__) || defined(_M_X64)
#include <emmintrin.h>
#endif

#include "assets.inc"
#include "action_icons.inc"
#include "fallback_glyphs.inc"

static Palette g_pal;
static bool g_hc = false;
const Palette& P() { return g_pal; }
void set_high_contrast(bool on) {
  g_hc = on;
  g_pal = Palette{};
  if (on) {
    g_pal.faint = {120, 120, 120};
    g_pal.dim = {200, 200, 200};
    g_pal.edge = {120, 120, 120};
  }
}

// ── lookups ─────────────────────────────────────────────────────────
const BitmapDef* bmp(std::string_view name) {
  static std::unordered_map<std::string_view, const BitmapDef*> map = [] {
    std::unordered_map<std::string_view, const BitmapDef*> m;
    for (auto& b : kBitmaps) m[b.name] = &b;
    for (auto& b : kActionIcons) m[b.name] = &b;
    // icons drawn for Cue's own menus, on the same 1-art-px grid as "check" and "clock"
    static const BitmapDef extra[] = {
        {"next-file",5,5,"#...#" "##..#" "###.#" "##..#" "#...#"},  // the weight of "forward": one triangle, one bar
        {"prev-file",5,5,"#...#" "#..##" "#.###" "#..##" "#...#"},
        {"loop-one",3,5,".#." "##." ".#." ".#." "###"},  // repeat just this one: a 1 at the icon's shoulder
        // the note of Cue's music icon: a song without a cover of its own
        {"cue-note",8,10,"...###.." "...#.##." "...#..##" "...#...#" "...#...." "...#...." ".###...." "####...." "####...." ".##....."},
        // the interface size: a small and a large window; the controls' background: a bar
        {"ui-size",9,6,"....#####" "....#...#" "....#...#" "###.#...#" "#.#.#...#" "###.#####"},
        {"dock-bg",9,5,".#######." "#########" "#.#.#.#.#" "#########" ".#######."},
        // music looks: the cover with its note, and the spectrum's bars
        {"look-cover",7,7,"#######" "#.....#" "#..##.#" "#..#..#" "#.##..#" "#.....#" "#######"},
        // the cover between two small wings of the spectrum
        {"look-both",13,7,"...#######..." ".#.#.....#.#." "##.#.....#.##" "##.#.....#.##" "##.#.....#.##" ".#.#.....#.#." "...#######..."},
        {"look-bars",9,6,"....#...." "....#...." "..#.#.#.." "..#.#.#.." "#.#.#.#.#" "#.#.#.#.#"},
        // in colour (a rainbow's arcs) or black and white (a disc half dark, half light)
        {"look-color",9,6,"..#####.." ".#.....#." "#..###..#" "#.#...#.#" "#.#...#.#" "#.#...#.#"},
        {"look-mono",7,7,"..###.." ".#..##." "#...###" "#...###" "#...###" ".#..##." "..###.."},
        // a film's chapters: its table of contents, a point and a line for each part
        {"chapters",9,5,"#.######." "........." "#.#######" "........." "#.#####.."},
        {"queue",9,7,"#........" "##.######" "#........" "........." "...######" "........." "...######"},  // now playing, then what follows
        {"text-style", 9, 7, ".##......" "#..#....." "#..#.###." "####....#" "#..#.####" "#..#.#..#" "#..#..###"},
        // the interface language: a small globe, meridian and equator
        {"globe", 7, 7, "..###.." ".#.#.#." "#..#..#" "#######" "#..#..#" ".#.#.#." "..###.."},
        // the phone remote: a small phone, its home button a gap at the bottom
        {"phone", 5, 8, "#####" "#...#" "#...#" "#...#" "#...#" "#...#" "##.##" "#####"},
        // several audio tracks at once: three faders of a little mixer, each at its own level
        // a choice among several (tick boxes): empty, and ticked
        {"box-off", 7, 7, "#######" "#.....#" "#.....#" "#.....#" "#.....#" "#.....#" "#######"},
        {"box-on", 7, 7, "#######" "#.....#" "#....##" "##..#.#" "#.##..#" "#.....#" "#######"},
        // back to how it was: an arrow going round, anticlockwise (drawn mirrored, it is "undo")
        {"reset", 7, 7, "###.##." "##....#" "#.#...#" "......#" "#.....#" ".#...#." "..###.."},
        // the mini player: a small picture in the corner of a big one
        // back to the full player: the picture grows from the corner it was in
        {"expand", 9, 7, "#########" "#.......#" "#.#####.#" "#.#####.#" "#.#####.#" "#.......#" "#########"},  // (16/11: the frame filled, the pair of "mini")
        {"mini", 9, 7, "#########" "#.......#" "#.......#" "#...###.#" "#...###.#" "#.......#" "#########"},
        // smooth motion: a ball gliding, its trail behind it
        {"smooth", 9, 5, "....###.." "##.#####." "...#####." "##.#####." "....###.."},
        // Settings' two pages: a film (a screen with its play mark) and music (two notes on one beam)
        {"set-video", 9, 7, "#########" "#.......#" "#..#....#" "#..##...#" "#..#....#" "#.......#" "#########"},
        {"set-music", 7, 7, "..#####" "..#...#" "..#...#" "..#...#" ".##..##" "###.###" "##..##."},
        {"mixer", 9, 7, ".#..#..#." ".#.###.#." ".#..#..#." ".#..#.###" "###.#..#." ".#..#..#." ".#..#..#."},
    };
    for (auto& b : extra) m[b.name] = &b;
    return m;
  }();
  auto it = map.find(name);
  return it == map.end() ? nullptr : it->second;
}

void action_icon(Canvas& c, std::string_view name, const Rect& box, double s, Rgb colour, double alpha) {
  if (const auto* b = bmp(name)) {
    double cell = name.starts_with("cue:") ? s / 2 : s;
    c.bitmap(b, box.x + (int)std::lround((box.w - b->w * cell) / 2),
             box.y + (int)std::lround((box.h - b->h * cell) / 2), cell, colour, alpha);
  }
}

const MaskDef* label_mask(std::string_view label, double s) {
  const MaskDef* best = nullptr;
  for (auto& m : kMasks) {
    if (label != m.label) continue;
    if (!best || std::abs(m.s - s) < std::abs(best->s - s)) best = &m;
  }
  return best;
}

uint32_t next_cp(std::string_view s, size_t& i) {
  auto b = [&](size_t k) { return (uint32_t)(uint8_t)s[k]; };
  uint32_t c = b(i);
  if (c < 0x80) { i += 1; return c; }
  if ((c >> 5) == 6 && i + 1 < s.size()) { uint32_t r = ((c & 31) << 6) | (b(i + 1) & 63); i += 2; return r; }
  if ((c >> 4) == 14 && i + 2 < s.size()) { uint32_t r = ((c & 15) << 12) | ((b(i + 1) & 63) << 6) | (b(i + 2) & 63); i += 3; return r; }
  if ((c >> 3) == 30 && i + 3 < s.size()) {
    uint32_t r = ((c & 7) << 18) | ((b(i + 1) & 63) << 12) | ((b(i + 2) & 63) << 6) | (b(i + 3) & 63);
    i += 4; return r;
  }
  i += 1;
  return 0xFFFD;
}

// Characters drawn in the Omni's own grammar that the design-system bundle does not carry:
// Spanish opening marks (the Omni ? and ! turned around) and no-break spaces (French puts one
// before ? : and %). They win over the fallback table.
static const GlyphDef kExtraGlyphs[] = {
    {"\xc2\xbf", 5, "......................#.........#...#...#....#..#..##......."},  // inverted question mark
    {"\xc2\xa1", 2, "........#...#.#.#.#.#..."},  // inverted exclamation mark
    {"\xc2\xa0", 3, "...................................."},  // no-break space
    {"\xe2\x80\xaf", 2, "........................"},  // narrow no-break space
};

// Glyphs missing from the Omni (CJK, kana, Greek, Cyrillic, extra Latin) come from
// fallback_glyphs.inc, decoded once into the same '#' grid the Omni uses and kept.
static const GlyphDef* fallback_glyph(uint32_t cp) {
  static std::mutex mu;
  static std::unordered_map<uint32_t, std::unique_ptr<std::pair<GlyphDef, std::string>>> cache;
  std::lock_guard<std::mutex> lock(mu);
  auto it = cache.find(cp);
  if (it != cache.end()) return it->second ? &it->second->first : nullptr;
  const uint32_t* end = kFallbackCp + kFallbackCount;
  const uint32_t* at = std::lower_bound(kFallbackCp, end, cp, [](uint32_t v, uint32_t c) { return (v & 0xFFFFFF) < c; });
  if (at == end || (*at & 0xFFFFFF) != cp) return nullptr;
  std::unique_ptr<std::pair<GlyphDef, std::string>> g;
  if (at != end && (*at & 0xFFFFFF) == cp) {
    int w = (int)(*at >> 24);
    const uint16_t* rows = kFallbackRows[at - kFallbackCp];
    g = std::make_unique<std::pair<GlyphDef, std::string>>();
    g->second.assign((size_t)w * 12, '.');
    for (int y = 0; y < 12; ++y)
      for (int x = 0; x < w; ++x)
        if (rows[y] >> x & 1) g->second[(size_t)y * w + x] = '#';
    g->first = GlyphDef{"", w, g->second.c_str()};
  }
  auto& slot = cache[cp];
  slot = std::move(g);
  return slot ? &slot->first : nullptr;
}

static const GlyphDef* glyph(uint32_t cp) {
  static std::unordered_map<uint32_t, const GlyphDef*> map = [] {
    std::unordered_map<uint32_t, const GlyphDef*> m;
    for (auto& g : kGlyphs) { size_t i = 0; m[next_cp(g.ch, i)] = &g; }
    for (auto& g : kExtraGlyphs) { size_t i = 0; m[next_cp(g.ch, i)] = &g; }
    return m;
  }();
  auto it = map.find(cp);
  if (it != map.end()) return it->second;
  return cp >= 0xA0 ? fallback_glyph(cp) : nullptr;
}
bool has_glyph(uint32_t cp) { return glyph(cp) != nullptr; }

// ── canvas ──────────────────────────────────────────────────────────
static inline uint32_t premul(Rgb c, double a) {
  if (a <= 0) return 0;
  if (a > 1) a = 1;
  uint32_t A = (uint32_t)(a * 255 + 0.5);
  uint32_t R = (c.r * A + 127) / 255, G = (c.g * A + 127) / 255, B = (c.b * A + 127) / 255;
  return (A << 24) | (R << 16) | (G << 8) | B;
}

static inline uint32_t over(uint32_t src, uint32_t dst) {
  uint32_t sa = src >> 24;
  if (sa == 255 || dst == 0) return src;
  if (sa == 0) return dst;
  uint32_t inv = 255 - sa;
  auto ch = [&](int sh) {
    uint32_t s = (src >> sh) & 255, d = (dst >> sh) & 255;
    uint32_t v = s + (d * inv + 127) / 255;
    return std::min<uint32_t>(v, 255) << sh;
  };
  return ch(24) | ch(16) | ch(8) | ch(0);
}

// The same result as over() for a premultiplied source (every colour at most its alpha, which
// is what premul() and the canvases built from it hold), two channels at a time: the rounded
// division by 255 is exact as (x + 1 + (x >> 8)) >> 8 for every x up to 65535, and a valid
// source cannot carry past 255, so no clamp is needed. Checked against over() for every source
// alpha, channel and destination in tests/raster_test.cpp.
static inline uint32_t over_premul(uint32_t src, uint32_t dst, uint32_t inv) {
  uint32_t rb = (dst & 0x00FF00FFu) * inv + 0x007F007Fu;
  uint32_t ag = ((dst >> 8) & 0x00FF00FFu) * inv + 0x007F007Fu;
  rb = ((rb + 0x00010001u + ((rb >> 8) & 0x00FF00FFu)) >> 8) & 0x00FF00FFu;
  ag = ((ag + 0x00010001u + ((ag >> 8) & 0x00FF00FFu)) >> 8) & 0x00FF00FFu;
  return src + (rb | (ag << 8));
}
static inline bool premultiplied(uint32_t v) {
  const uint32_t a = v >> 24;
  return ((v >> 16) & 255) <= a && ((v >> 8) & 255) <= a && (v & 255) <= a;
}
// One premultiplied colour over a run of pixels (a translucent fill: the home's fades cover the
// whole window with one). Four pixels at a time where SSE2 is there (every x64 processor), with
// the same arithmetic as over_premul: the result is the same, bit for bit.
static void over_span(uint32_t* d, int n, uint32_t v, uint32_t inv) {
  int i = 0;
#if defined(__SSE2__) || defined(_M_X64)
  const __m128i zero = _mm_setzero_si128();
  const __m128i k_inv = _mm_set1_epi16((short)inv);
  const __m128i k_127 = _mm_set1_epi16(127);
  const __m128i k_1 = _mm_set1_epi16(1);
  const __m128i src = _mm_set1_epi32((int)v);
  for (; i + 4 <= n; i += 4) {
    const __m128i px = _mm_loadu_si128(reinterpret_cast<const __m128i*>(d + i));
    __m128i lo = _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(px, zero), k_inv), k_127);
    __m128i hi = _mm_add_epi16(_mm_mullo_epi16(_mm_unpackhi_epi8(px, zero), k_inv), k_127);
    lo = _mm_srli_epi16(_mm_add_epi16(_mm_add_epi16(lo, k_1), _mm_srli_epi16(lo, 8)), 8);
    hi = _mm_srli_epi16(_mm_add_epi16(_mm_add_epi16(hi, k_1), _mm_srli_epi16(hi, 8)), 8);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(d + i), _mm_add_epi8(_mm_packus_epi16(lo, hi), src));
  }
#endif
  for (; i < n; ++i) d[i] = over_premul(v, d[i], inv);
}

// One opaque colour over a run of pixels: what clears every frame (8 million pixels at 4K) and
// fills the solid shapes. Four pixels per store, aligned, where SSE2 is there: about a third
// faster than std::fill's one pixel at a time (measured: 2.3 -> 1.5 ms for a 4K window, also in
// the MinGW build). The same pixels as std::fill.
static void fill_span(uint32_t* d, size_t n, uint32_t v) {
#if defined(__SSE2__) || defined(_M_X64)
  if (n >= 16) {
    while ((reinterpret_cast<uintptr_t>(d) & 15) != 0) { *d++ = v; --n; }
    const __m128i four = _mm_set1_epi32((int)v);
    size_t i = 0;
    for (; i + 4 <= n; i += 4) _mm_store_si128(reinterpret_cast<__m128i*>(d + i), four);
    for (; i < n; ++i) d[i] = v;
    return;
  }
#endif
  std::fill(d, d + n, v);
}

uint32_t blend_over_for_tests(uint32_t src, uint32_t dst) {
  return premultiplied(src) && (src >> 24) != 255 && (src >> 24) != 0 ? over_premul(src, dst, 255 - (src >> 24)) : over(src, dst);
}
uint32_t blend_over_reference_for_tests(uint32_t src, uint32_t dst) { return over(src, dst); }

void Canvas::resize(int nw, int nh) {
  w = std::max(0, nw);
  h = std::max(0, nh);
  px.assign((size_t)w * h, 0);
}
void Canvas::clear(Rgb c, double a) {
  if (!clipped) { fill_span(px.data(), px.size(), premul(c, a)); return; }
  int x0, y0, x1, y1;
  reach(x0, y0, x1, y1);
  const uint32_t v = premul(c, a);
  for (int y = y0; y < y1 && x0 < x1; ++y) fill_span(&px[(size_t)y * w + x0], (size_t)(x1 - x0), v);
}
void Canvas::clear_transparent() {
  if (!clipped) { std::fill(px.begin(), px.end(), 0u); return; }
  int x0, y0, x1, y1;
  reach(x0, y0, x1, y1);
  for (int y = y0; y < y1 && x0 < x1; ++y) std::fill(&px[(size_t)y * w + x0], &px[(size_t)y * w + x1], 0u);
}

void Canvas::fill(double x, double y, double fw, double fh, Rgb c, double a) {
  const int ix = (int)std::lround(x), iy = (int)std::lround(y);
  fill_pixels(ix, iy, (int)std::lround(x + fw) - ix, (int)std::lround(y + fh) - iy, c, a);
}

void Canvas::fill_pixels(int x, int y, int fw, int fh, Rgb c, double a) {
  int x0, y0, x1, y1;
  reach(x0, y0, x1, y1);
  x0 = std::max(x0, x); y0 = std::max(y0, y); x1 = std::min(x1, x + fw); y1 = std::min(y1, y + fh);
  if (x0 >= x1 || y0 >= y1) return;
  uint32_t v = premul(c, a);
  if (!v) return;
  bool opaque = (v >> 24) == 255;
  const uint32_t inv = 255 - (v >> 24);  // premul() never gives a colour above its alpha
  for (int yy = y0; yy < y1; ++yy) {
    uint32_t* row = &px[(size_t)yy * w];
    if (opaque) fill_span(row + x0, (size_t)(x1 - x0), v);
    else over_span(row + x0, x1 - x0, v, inv);
  }
}

void Canvas::cornered(const Rect& r, int cut, Rgb c, double a) {
  if (r.w <= 2 * cut || r.h <= 2 * cut) { fill(r, c, a); return; }
  fill(r.x + cut, r.y, r.w - 2 * cut, cut, c, a);
  fill(r.x, r.y + cut, r.w, r.h - 2 * cut, c, a);
  fill(r.x + cut, r.b() - cut, r.w - 2 * cut, cut, c, a);
}

void Canvas::cells(const char* grid, int gw, int gh, int x, int y, double cell, Rgb c, double a) {
  const int whole = (int)cell;
  if (cell == whole) {
    for (int j = 0; j < gh; ++j) {
      for (int i = 0; i < gw;) {
        if (grid[j * gw + i] != '#') { ++i; continue; }
        const int first = i++;
        while (i < gw && grid[j * gw + i] == '#') ++i;
        fill_pixels(x + first * whole, y + j * whole, (i - first) * whole, whole, c, a);
      }
    }
    return;
  }
  for (int j = 0; j < gh; ++j) {
    const int top = (int)std::lround(y + j * cell);
    const int height = (int)std::lround(y + (j + 1) * cell) - top;
    for (int i = 0; i < gw;) {
      if (grid[j * gw + i] != '#') { ++i; continue; }
      const int first = i++;
      while (i < gw && grid[j * gw + i] == '#') ++i;
      const int left = (int)std::lround(x + first * cell);
      fill_pixels(left, top, (int)std::lround(x + i * cell) - left, height, c, a);
    }
  }
}

void Canvas::bitmap(const BitmapDef* b, int x, int y, double cell, Rgb c, double a, bool flip_x) {
  if (!b) return;
  if (!flip_x) { cells(b->cells, b->w, b->h, x, y, cell, c, a); return; }
  for (int j = 0; j < b->h; ++j)
    for (int i = 0; i < b->w; ++i)
      if (b->cells[j * b->w + i] == '#') {
        int xi = flip_x ? (b->w - 1 - i) : i;
        fill(x + xi * cell, y + j * cell, cell, cell, c, a);
      }
}

void Canvas::mask(const MaskDef* m, int x, int y, Rgb c, double a) {
  if (!m) return;
  int X0, Y0, X1, Y1;
  reach(X0, Y0, X1, Y1);
  for (int j = 0; j < m->h; ++j) {
    int yy = y + j;
    if (yy < Y0 || yy >= Y1) continue;
    for (int i = 0; i < m->w; ++i) {
      int xx = x + i;
      if (xx < X0 || xx >= X1) continue;
      uint8_t k = (uint8_t)m->a[j * m->w + i];
      if (!k) continue;
      uint32_t v = premul(c, a * k / 255.0);
      px[(size_t)yy * w + xx] = over(v, px[(size_t)yy * w + xx]);
    }
  }
}

void Canvas::blit(const Canvas& src, int x, int y, double a) {
  uint32_t A = (uint32_t)(std::clamp(a, 0.0, 1.0) * 255 + 0.5);
  if (A == 0) return;
  // clip once, then walk whole rows: no per-pixel bounds checks
  int X0, Y0, X1, Y1;
  reach(X0, Y0, X1, Y1);
  const int i0 = std::max(0, X0 - x), i1 = std::min(src.w, X1 - x);
  const int j0 = std::max(0, Y0 - y), j1 = std::min(src.h, Y1 - y);
  if (i0 >= i1 || j0 >= j1) return;
  for (int j = j0; j < j1; ++j) {
    const uint32_t* s = &src.px[(size_t)j * src.w];
    uint32_t* d = &px[(size_t)(y + j) * w + x];
    if (A == 255) {
      int i = i0;
#if defined(__SSE2__) || defined(_M_X64)
      // four pixels at a time: all opaque, a copy; all clear, nothing; all premultiplied (the
      // lights, the pictures' soft edges), over_premul's own arithmetic on the four. Anything
      // else (a pixel not premultiplied) goes through the one-pixel code below, as before.
      const __m128i zero = _mm_setzero_si128(), k_255 = _mm_set1_epi32(255), k_127 = _mm_set1_epi16(127), k_1 = _mm_set1_epi16(1);
      for (; i + 4 <= i1; i += 4) {
        const __m128i sv = _mm_loadu_si128(reinterpret_cast<const __m128i*>(s + i));
        const __m128i al = _mm_srli_epi32(sv, 24);
        if (_mm_movemask_epi8(_mm_cmpeq_epi32(al, k_255)) == 0xFFFF) { _mm_storeu_si128(reinterpret_cast<__m128i*>(d + i), sv); continue; }
        if (_mm_movemask_epi8(_mm_cmpeq_epi32(sv, zero)) == 0xFFFF) continue;
        __m128i rep = _mm_or_si128(al, _mm_slli_epi32(al, 8));
        rep = _mm_or_si128(rep, _mm_slli_epi32(rep, 16));
        if (_mm_movemask_epi8(_mm_cmpeq_epi8(_mm_max_epu8(sv, rep), rep)) != 0xFFFF) {
          for (int k = i; k < i + 4; ++k) {
            const uint32_t v = s[k], sa = v >> 24;
            if (sa == 255) d[k] = v;
            else if (sa) d[k] = premultiplied(v) ? over_premul(v, d[k], 255 - sa) : over(v, d[k]);
          }
          continue;
        }
        const __m128i inv = _mm_sub_epi32(k_255, al);
        const __m128i inv16 = _mm_or_si128(inv, _mm_slli_epi32(inv, 16));
        const __m128i dv = _mm_loadu_si128(reinterpret_cast<const __m128i*>(d + i));
        __m128i lo = _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(dv, zero), _mm_unpacklo_epi32(inv16, inv16)), k_127);
        __m128i hi = _mm_add_epi16(_mm_mullo_epi16(_mm_unpackhi_epi8(dv, zero), _mm_unpackhi_epi32(inv16, inv16)), k_127);
        lo = _mm_srli_epi16(_mm_add_epi16(_mm_add_epi16(lo, k_1), _mm_srli_epi16(lo, 8)), 8);
        hi = _mm_srli_epi16(_mm_add_epi16(_mm_add_epi16(hi, k_1), _mm_srli_epi16(hi, 8)), 8);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(d + i), _mm_add_epi8(_mm_packus_epi16(lo, hi), sv));
      }
#endif
      for (; i < i1; ++i) {
        uint32_t v = s[i];
        uint32_t sa = v >> 24;
        if (sa == 255) d[i] = v;           // opaque: a plain copy
        else if (sa) d[i] = premultiplied(v) ? over_premul(v, d[i], 255 - sa) : over(v, d[i]);  // transparent pixels are skipped
      }
    } else {
      for (int i = i0; i < i1; ++i) {
        uint32_t v = s[i];
        if (!v) continue;
        auto ch = [&](int sh) { return ((((v >> sh) & 255) * A + 127) / 255) << sh; };
        const uint32_t sv = ch(24) | ch(16) | ch(8) | ch(0), sa = sv >> 24;
        if (sa) d[i] = premultiplied(sv) ? over_premul(sv, d[i], 255 - sa) : over(sv, d[i]);
      }
    }
  }
}

void Canvas::scale_alpha(double a) {
  if (a >= 1) return;
  uint32_t A = (uint32_t)(std::clamp(a, 0.0, 1.0) * 255 + 0.5);
  if (clipped) {  // (a clip: the plain per-pixel code, row by row inside it; the same rounding)
    int x0, y0, x1, y1;
    reach(x0, y0, x1, y1);
    for (int y = y0; y < y1; ++y)
      for (int x = x0; x < x1; ++x) {
        uint32_t& s = px[(size_t)y * w + x];
        auto ch = [&](int sh) { return ((((s >> sh) & 255) * A + 127) / 255) << sh; };
        s = ch(24) | ch(16) | ch(8) | ch(0);
      }
    return;
  }
  // (a fading full-window scene: four pixels at a time, the same rounding as below)
  size_t i = 0;
  uint32_t* d = px.data();
#if defined(__SSE2__) || defined(_M_X64)
  const __m128i zero = _mm_setzero_si128();
  const __m128i k_a = _mm_set1_epi16((short)A);
  const __m128i k_127 = _mm_set1_epi16(127);
  const __m128i k_1 = _mm_set1_epi16(1);
  for (; i + 4 <= px.size(); i += 4) {
    const __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(d + i));
    __m128i lo = _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(v, zero), k_a), k_127);
    __m128i hi = _mm_add_epi16(_mm_mullo_epi16(_mm_unpackhi_epi8(v, zero), k_a), k_127);
    lo = _mm_srli_epi16(_mm_add_epi16(_mm_add_epi16(lo, k_1), _mm_srli_epi16(lo, 8)), 8);
    hi = _mm_srli_epi16(_mm_add_epi16(_mm_add_epi16(hi, k_1), _mm_srli_epi16(hi, 8)), 8);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(d + i), _mm_packus_epi16(lo, hi));
  }
#endif
  for (; i < px.size(); ++i) {
    uint32_t& s = d[i];
    auto ch = [&](int sh) { return ((((s >> sh) & 255) * A + 127) / 255) << sh; };
    s = ch(24) | ch(16) | ch(8) | ch(0);
  }
}

void Canvas::assign_scaled(const Canvas& src, double a) {
  if (a >= 1 || src.clipped || clipped) { *this = src; if (a < 1) scale_alpha(a); return; }
  w = src.w; h = src.h;
  px.resize(src.px.size());
  const uint32_t A = (uint32_t)(std::clamp(a, 0.0, 1.0) * 255 + 0.5);
  const uint32_t* sp = src.px.data();
  uint32_t* d = px.data();
  size_t i = 0;
#if defined(__SSE2__) || defined(_M_X64)
  const __m128i zero = _mm_setzero_si128();
  const __m128i k_a = _mm_set1_epi16((short)A);
  const __m128i k_127 = _mm_set1_epi16(127);
  const __m128i k_1 = _mm_set1_epi16(1);
  for (; i + 4 <= px.size(); i += 4) {
    const __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(sp + i));
    __m128i lo = _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(v, zero), k_a), k_127);
    __m128i hi = _mm_add_epi16(_mm_mullo_epi16(_mm_unpackhi_epi8(v, zero), k_a), k_127);
    lo = _mm_srli_epi16(_mm_add_epi16(_mm_add_epi16(lo, k_1), _mm_srli_epi16(lo, 8)), 8);
    hi = _mm_srli_epi16(_mm_add_epi16(_mm_add_epi16(hi, k_1), _mm_srli_epi16(hi, 8)), 8);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(d + i), _mm_packus_epi16(lo, hi));
  }
#endif
  for (; i < px.size(); ++i) {
    const uint32_t s = sp[i];
    auto ch = [&](int sh) { return ((((s >> sh) & 255) * A + 127) / 255) << sh; };
    d[i] = ch(24) | ch(16) | ch(8) | ch(0);
  }
}

template<class Scale>
static void draw_panel_impl(Canvas& c, int x, int y, int aw, int ah, Scale s, const std::vector<int>& profile, Rgb fill,
                double fill_a, Rgb edge, double edge_a) {
  auto fill_rect = [&](auto xx, auto yy, auto ww, auto hh, Rgb color, double alpha) {
    if constexpr (std::is_integral_v<Scale>) c.fill_pixels(xx, yy, ww, hh, color, alpha);
    else c.fill(xx, yy, ww, hh, color, alpha);
  };
  if (aw <= 0 || ah <= 0) return;
  auto inset = [&](int row) {
    int k = std::min(row, ah - 1 - row);
    return k < (int)profile.size() ? profile[k] : 0;
  };
  auto inside = [&](int i, int j) {
    if (j < 0 || j >= ah) return false;
    int in = inset(j);
    return i >= in && i < aw - in;
  };
  for (int j = 0; j < ah; ++j) {
    int in = inset(j);
    // run-length: fill the interior span in one go, edges cell by cell
    int i0 = in, i1 = aw - in;
    for (int i = i0; i < i1; ++i) {
      bool border = !inside(i - 1, j) || !inside(i + 1, j) || !inside(i, j - 1) || !inside(i, j + 1);
      if (border) { fill_rect(x + i * s, y + j * s, s, s, edge, edge_a); continue; }
      int run = i;
      while (run < i1 && inside(run, j - 1) && inside(run, j + 1) && inside(run + 1, j)) ++run;
      if (run == i) { fill_rect(x + i * s, y + j * s, s, s, fill, fill_a); continue; }
      fill_rect(x + i * s, y + j * s, (run - i) * s, s, fill, fill_a);
      i = run - 1;
    }
  }
}

// A half scale (1.5, 2.5 px per art px): the same cells, laid on a grid whose outer cells (the
// border, the corners' steps) are all one whole width, the art pixel's whole part, and whose
// inner cells share what is left. Laid at 1.5 px per cell, a border was 2 px on the left and top
// and 1 on the right and bottom (each cell rounded where it fell); now it is the same all round.
static void draw_panel_half(Canvas& c, int x, int y, int aw, int ah, double s, const std::vector<int>& profile, Rgb fill,
                            double fill_a, Rgb edge, double edge_a) {
  if (aw <= 0 || ah <= 0) return;
  const int t = std::max(1, (int)s);
  const int E = (int)profile.size() + 1;  // the cells at each end with a fixed width
  auto axis = [&](int n, int origin) {
    std::vector<int> at((size_t)n + 1);
    const int span = (int)std::lround(n * s);
    if (n <= 2 * E || span <= 2 * E * t) {
      for (int i = 0; i <= n; ++i) at[(size_t)i] = origin + (int)std::lround(i * s);
      return at;
    }
    const int inner = span - 2 * E * t;
    for (int i = 0; i <= n; ++i) {
      if (i <= E) at[(size_t)i] = origin + i * t;
      else if (i >= n - E) at[(size_t)i] = origin + span - (n - i) * t;
      else at[(size_t)i] = origin + E * t + (int)std::lround((double)(i - E) * inner / (n - 2 * E));
    }
    return at;
  };
  const std::vector<int> X = axis(aw, x), Y = axis(ah, y);
  auto inset = [&](int row) {
    int k = std::min(row, ah - 1 - row);
    return k < (int)profile.size() ? profile[k] : 0;
  };
  auto inside = [&](int i, int j) {
    if (j < 0 || j >= ah) return false;
    int in = inset(j);
    return i >= in && i < aw - in;
  };
  for (int j = 0; j < ah; ++j) {
    const int in = inset(j), y0 = Y[(size_t)j], hh = Y[(size_t)j + 1] - y0;
    if (hh <= 0) continue;
    const int i0 = in, i1 = aw - in;
    for (int i = i0; i < i1; ++i) {
      const bool border = !inside(i - 1, j) || !inside(i + 1, j) || !inside(i, j - 1) || !inside(i, j + 1);
      if (border) { c.fill_pixels(X[(size_t)i], y0, X[(size_t)i + 1] - X[(size_t)i], hh, edge, edge_a); continue; }
      int run = i;
      while (run < i1 && inside(run, j - 1) && inside(run, j + 1) && inside(run + 1, j)) ++run;
      if (run == i) run = i + 1;
      c.fill_pixels(X[(size_t)i], y0, X[(size_t)run] - X[(size_t)i], hh, fill, fill_a);
      i = run - 1;
    }
  }
}

void draw_panel(Canvas& c, int x, int y, int aw, int ah, double s, const std::vector<int>& profile,
                Rgb fill, double fill_a, Rgb edge, double edge_a) {
  if (s == (int)s) draw_panel_impl(c, x, y, aw, ah, (int)s, profile, fill, fill_a, edge, edge_a);
  else draw_panel_half(c, x, y, aw, ah, s, profile, fill, fill_a, edge, edge_a);
}

// ── text ────────────────────────────────────────────────────────────
int text_w(std::string_view str, double s) {
  int w = 0;
  for (size_t i = 0; i < str.size();) {
    const GlyphDef* g = glyph(next_cp(str, i));
    if (g) w += g->w;
  }
  return (int)std::lround(w * s);
}

int text(Canvas& c, std::string_view str, int x, int y, double s, Rgb col, double a) {
  double cx = x;
  for (size_t i = 0; i < str.size();) {
    const GlyphDef* g = glyph(next_cp(str, i));
    if (!g) continue;  // what the font does not draw disappears (Omni rule)
    c.cells(g->rows, g->w, 12, cx, y, s, col, a);
    cx += g->w * s;
  }
  return (int)std::lround(cx - x);
}

static std::vector<std::string_view> split_cps(std::string_view s) {
  std::vector<std::string_view> out;
  for (size_t i = 0; i < s.size();) {
    size_t st = i;
    next_cp(s, i);
    out.push_back(s.substr(st, i - st));
  }
  return out;
}

std::string fit_end(std::string_view t, int max_w, double s) {
  if (text_w(t, s) <= max_w) return std::string(t);
  auto cps = split_cps(t);
  int ell = text_w("…", s);
  std::string out;
  int w = 0;
  for (auto cp : cps) {
    int cw = text_w(cp, s);
    if (w + cw + ell > max_w) break;
    out += cp;
    w += cw;
  }
  while (!out.empty() && out.back() == ' ') out.pop_back();
  return out + "…";
}

std::string fit_middle(std::string_view name, int max_w, double s) {
  if (text_w(name, s) <= max_w) return std::string(name);
  auto cps = split_cps(name);
  size_t dot = name.rfind('.');
  size_t keep_tail_bytes = (dot != std::string_view::npos && name.size() - dot <= 6) ? name.size() - dot : 0;
  // grow the tail a bit beyond the extension so "S02E03" style endings survive
  int ell = text_w("…", s);
  std::string tail(name.substr(name.size() - keep_tail_bytes));
  auto tail_cps = split_cps(name.substr(0, name.size() - keep_tail_bytes));
  int budget = max_w - ell - text_w(tail, s);
  if (budget <= 0) return fit_end(name, max_w, s);
  int head_budget = (budget * 2) / 3, tail_budget = budget - head_budget;
  std::string head, extra;
  int w = 0;
  size_t hi = 0;
  for (; hi < tail_cps.size(); ++hi) {
    int cw = text_w(tail_cps[hi], s);
    if (w + cw > head_budget) break;
    head += tail_cps[hi];
    w += cw;
  }
  int tw = 0;
  for (size_t k = tail_cps.size(); k > hi; --k) {
    int cw = text_w(tail_cps[k - 1], s);
    if (tw + cw > tail_budget) break;
    extra = std::string(tail_cps[k - 1]) + extra;
    tw += cw;
  }
  return head + "…" + extra + tail;
}

// ── wrapping ────────────────────────────────────────────────────────
static bool breaks_around(uint32_t c) {  // ideographs and kana stand alone: a line may break beside them
  return (c >= 0x3040 && c <= 0x30FF) || (c >= 0x3400 && c <= 0x4DBF) || (c >= 0x4E00 && c <= 0x9FFF) ||
         (c >= 0xF900 && c <= 0xFAFF);
}
static bool closing_mark(uint32_t c) {  // stays with what comes before it
  switch (c) {
    case 0x3001: case 0x3002: case 0xFF0C: case 0xFF0E: case 0xFF1A: case 0xFF1B: case 0xFF1F: case 0xFF01:
    case 0xFF09: case 0x300D: case 0x300F: case 0x201D: case 0x2019: case 0x2026: case 0x00BB: case 0xFF05:
    case ',': case '.': case ':': case ';': case '?': case '!': case ')': case '%': case 0x00A0: case 0x202F:
      return true;
  }
  return false;
}
static bool opening_mark(uint32_t c) {  // stays with what comes after it
  return c == 0xFF08 || c == 0x300C || c == 0x300E || c == 0x201C || c == 0x2018 || c == 0x00AB || c == '(' ||
         c == 0x00BF || c == 0x00A1;
}

std::vector<std::string> wrap_lines(std::string_view t, int max_w, double s) {
  // units: a run that must stay together, and whether a space separated it from the previous one
  struct Unit { std::string text; bool space_before; };
  std::vector<Unit> units;
  bool pending_space = false;
  uint32_t prev = 0;
  for (size_t i = 0; i < t.size();) {
    size_t st = i;
    uint32_t c = next_cp(t, i);
    std::string_view ch = t.substr(st, i - st);
    if (c == ' ') { pending_space = true; prev = c; continue; }
    bool start_new = units.empty() || pending_space ||
                     ((breaks_around(c) || breaks_around(prev)) && !closing_mark(c) && !opening_mark(prev));
    if (start_new) units.push_back({std::string(ch), pending_space && !units.empty()});
    else units.back().text += ch;
    pending_space = false;
    prev = c;
  }
  std::vector<std::string> lines;
  std::string line;
  for (auto& u : units) {
    std::string cand = line.empty() ? u.text : line + (u.space_before ? " " : "") + u.text;
    if (!line.empty() && text_w(cand, s) > max_w) { lines.push_back(line); line = u.text; }
    else line = cand;
  }
  if (!line.empty()) lines.push_back(line);
  return lines;
}

void draw_switch(Canvas& c, double x, double y, double cell, double k, Rgb on, double hover, bool enabled, double a) {
  if (a <= 0.003) return;
  const Palette& p = P();
  k = std::clamp(k, -0.12, 1.12);
  const double kc = enabled ? std::clamp(k, 0.0, 1.0) : 0.0, hv = enabled ? std::clamp(hover, 0.0, 1.0) : 0.0;
  // in whole device pixels from a whole origin: every shape keeps its size wherever it stands
  // (the knob never grows or shrinks a pixel as it slides)
  auto box = [&](int ox, int oy, double gx, double gy, double gw, double gh, Rgb col, double al) {
    const int x0 = ox + (int)std::lround(gx * cell), y0 = oy + (int)std::lround(gy * cell);
    const int x1 = ox + (int)std::lround((gx + gw) * cell), y1 = oy + (int)std::lround((gy + gh) * cell);
    if (x1 > x0 && y1 > y0 && al > 0.003) c.fill(x0, y0, x1 - x0, y1 - y0, col, al);
  };
  auto stepped = [&](int ox, int oy, double gw, double gh, Rgb col, double al) {
    box(ox, oy, 1, 0, gw - 2, 1, col, al);
    box(ox, oy, 0, 1, gw, gh - 2, col, al);
    box(ox, oy, 1, gh - 1, gw - 2, 1, col, al);
  };
  const int X = (int)std::lround(x), Y = (int)std::lround(y), in = (int)std::lround(cell);
  const Rgb rim = enabled ? mix(p.faint, p.dim, hv * 0.6) : p.edge;
  if (cell != std::floor(cell)) {
    // a half scale: the track's own size, its outline and steps one whole width all round (the
    // art pixel's whole part), the knob a square filling the track's inside; laid cell by cell,
    // the outline was 2 px on the left and 1 on the right
    const int t = std::max(1, (int)cell);
    const int W = (int)std::lround(kSwitchW * cell), H = (int)std::lround(kSwitchH * cell);
    auto steps = [&](int ox, int oy, int w, int h, Rgb col, double al) {
      if (al <= 0.003 || w <= 2 * t || h <= 2 * t) return;
      c.fill_pixels(ox + t, oy, w - 2 * t, t, col, al);
      c.fill_pixels(ox, oy + t, w, h - 2 * t, col, al);
      c.fill_pixels(ox + t, oy + h - t, w - 2 * t, t, col, al);
    };
    steps(X, Y, W, H, mix(rim, on, kc), a);
    if (kc < 1) steps(X + t, Y + t, W - 2 * t, H - 2 * t, p.bg, a * (1 - kc));
    const int knob = H - 2 * t, travel = W - 2 * t - knob;
    if (kc > 0.5) {
      const int th = std::max(2 * t, (int)std::lround(2 * cell));
      c.fill_pixels(X + t + (knob - t) / 2 + std::max(0, (int)std::lround(2 * cell) - t), Y + (H - th) / 2, t, th, p.bg, a * (kc - 0.5) * 2);
    }
    const Rgb knob_col = enabled ? mix(mix(p.dim, p.fg, hv), p.bg, kc) : p.faint;
    steps(X + t + (int)std::lround(travel * k), Y + t, knob, knob, knob_col, a);
    return;
  }
  stepped(X, Y, kSwitchW, kSwitchH, mix(rim, on, kc), a);
  if (kc < 1) stepped(X + in, Y + in, kSwitchW - 2, kSwitchH - 2, p.bg, a * (1 - kc));  // off: an outlined track
  if (kc > 0.5) box(X, Y, 3, 3, 1, 2, p.bg, a * (kc - 0.5) * 2);                       // on: a small tick
  const Rgb knob = enabled ? mix(mix(p.dim, p.fg, hv), p.bg, kc) : p.faint;
  stepped(X + in + (int)std::lround(6 * k * cell), Y + in, 6, 6, knob, a);
}
