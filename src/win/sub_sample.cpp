#include "sub_sample.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "../core/sub_style.h"
#include "platform.h"

namespace {
// The three fonts for this process only (the engine gets them through its fonts folder; a build
// without the per-user install would otherwise draw the sample in a fallback face).
void load_fonts_once() {
  static bool done = false;
  if (done) return;
  done = true;
  const wchar_t* files[3] = {L"CueOmni.ttf", L"CueClean.ttf", L"CueReadable.ttf"};
  for (auto f : files) AddFontResourceExW((exe_dir() + L"\\fonts\\" + f).c_str(), FR_PRIVATE, nullptr);
}

// Grey coverage of the words, white on black, anti-aliased (never ClearType: no colour fringes).
bool coverage(const PlayView::SampleRequest& q, double unit, std::vector<uint8_t>& out, int& x0, int& x1, int& y0, int& y1) {
  // drawn at twice the size, then each 2x2 averaged: small words come out smooth and even (GDI's
  // own grey smoothing alone left them blocky and hard to read at the small sizes)
  constexpr int K = 2;
  const int w = q.w * K, h = q.h * K;
  BITMAPINFO bi{};
  bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
  bi.bmiHeader.biWidth = w;
  bi.bmiHeader.biHeight = -h;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HDC dc = CreateCompatibleDC(nullptr);
  HBITMAP bmp = dc ? CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0) : nullptr;
  if (!dc || !bmp || !bits) { if (bmp) DeleteObject(bmp); if (dc) DeleteDC(dc); return false; }
  HGDIOBJ old_bmp = SelectObject(dc, bmp);
  std::memset(bits, 0, (size_t)w * h * 4);
  const int font = std::clamp(q.style.font, 0, 2);
  const std::wstring face = widen(kSubFontFamily[font]);
  HFONT f = CreateFontW(-std::max(6 * K, (int)std::lround(q.px * K)), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                        OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, face.c_str());
  HGDIOBJ old_font = SelectObject(dc, f);
  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, RGB(255, 255, 255));
  SetTextCharacterExtra(dc, (int)std::lround(q.style.tracking * q.style.size / 44.0 * unit * K));
  // room for the outline or the box on both sides; words that do not fit end in an ellipsis
  const int pad = ((int)std::ceil(std::max(q.style.outline * 0.9, 4.0) * unit) + 2) * K;
  RECT r{pad, 0, w - pad, h};
  std::wstring words = widen(q.text);
  DrawTextW(dc, words.c_str(), (int)words.size(), &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
  GdiFlush();
  const int ow = q.w, oh = q.h;
  out.assign((size_t)ow * oh, 0);
  x0 = ow; x1 = -1; y0 = oh; y1 = -1;
  const uint32_t* src = static_cast<const uint32_t*>(bits);
  for (int y = 0; y < oh; ++y)
    for (int x = 0; x < ow; ++x) {
      unsigned sum = 0;
      for (int dy = 0; dy < K; ++dy)
        for (int dx = 0; dx < K; ++dx) sum += (src[(size_t)(y * K + dy) * w + x * K + dx] >> 8) & 255;
      const uint8_t v = (uint8_t)((sum + K * K / 2) / (K * K));
      if (!v) continue;
      out[(size_t)y * ow + x] = v;
      x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); y1 = std::max(y1, y);
    }
  SelectObject(dc, old_font);
  DeleteObject(f);
  SelectObject(dc, old_bmp);
  DeleteObject(bmp);
  DeleteDC(dc);
  return x1 >= x0;
}

uint32_t over(uint32_t dst, double a, uint8_t v) {  // premultiplied: grey v at alpha a over dst
  const double da = (dst >> 24) / 255.0;
  auto ch = [&](int sh) { return (uint32_t)std::lround(std::min(255.0, v * a + ((dst >> sh) & 255) * (1 - a))); };
  const uint32_t A = (uint32_t)std::lround(std::min(1.0, a + da * (1 - a)) * 255);
  return A << 24 | ch(16) << 16 | ch(8) << 8 | ch(0);
}
}  // namespace

std::shared_ptr<const Canvas> render_sub_sample(const PlayView::SampleRequest& q) {
  if (q.w <= 0 || q.h <= 0 || q.w > 4096 || q.h > 1024 || q.text.empty()) return nullptr;
  load_fonts_once();
  // one libass unit (a 720 px tall window's px) in the sample's px: a size-44 line is q.px tall
  const double unit = q.px / std::max(1, q.style.size);
  std::vector<uint8_t> cov;
  int x0, x1, y0, y1;
  if (!coverage(q, unit, cov, x0, x1, y0, y1)) return nullptr;
  auto c = std::make_shared<Canvas>();
  c->resize(q.w, q.h);
  c->clear_transparent();
  const int w = q.w, h = q.h;
  const double text_a = q.style.text_op / 100.0;
  if (q.style.bg_op > 0) {
    // the box hugs the words; libass pads it by the shadow offset (4 units)
    const int pad = std::max(1, (int)std::lround(4 * unit));
    c->fill(std::max(0, x0 - pad), std::max(0, y0 - pad), std::min(w, x1 + pad + 1) - std::max(0, x0 - pad),
            std::min(h, y1 + pad + 1) - std::max(0, y0 - pad), {0, 0, 0}, q.style.bg_op / 100.0);
  } else if (q.style.outline > 0) {
    // the outline: the words grown by its thickness (a round brush), 90% black
    const double rad = std::max(0.6, q.style.outline * 0.9 * unit);
    const int R = (int)std::ceil(rad);
    std::vector<uint8_t> grown((size_t)w * h, 0);
    // (the brush once: its offsets and soft edge; then each covered pixel stamps it)
    struct Tap { int dx, dy; uint16_t k; };
    std::vector<Tap> brush;
    for (int dy = -R; dy <= R; ++dy)
      for (int dx = -R; dx <= R; ++dx) {
        const double d = std::sqrt((double)dx * dx + dy * dy);
        const double edge = std::clamp(rad + 0.5 - d, 0.0, 1.0);
        if (edge > 0) brush.push_back({dx, dy, (uint16_t)std::lround(edge * 256)});
      }
    for (int y = y0; y <= y1; ++y)
      for (int x = x0; x <= x1; ++x) {
        const int v = cov[(size_t)y * w + x];
        if (!v) continue;
        for (const Tap& t : brush) {
          const int xx = x + t.dx, yy = y + t.dy;
          if ((unsigned)xx >= (unsigned)w || (unsigned)yy >= (unsigned)h) continue;
          uint8_t& g = grown[(size_t)yy * w + xx];
          const int m = v * t.k >> 8;
          if (m > g) g = (uint8_t)m;
        }
      }
    for (size_t i = 0; i < grown.size(); ++i)
      if (grown[i]) c->px[i] = over(c->px[i], grown[i] / 255.0 * 0.9 * text_a, 0);
  }
  for (size_t i = 0; i < cov.size(); ++i)
    if (cov[i]) c->px[i] = over(c->px[i], cov[i] / 255.0 * text_a, 255);
  return c;
}
