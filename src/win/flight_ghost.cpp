#include "flight_ghost.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

static LRESULT CALLBACK ghost_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
  switch (m) {
    case WM_NCHITTEST: return HTTRANSPARENT;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
      // the thumbnail covers it all; under it, black (never the desktop's last pixels)
      PAINTSTRUCT ps;
      HDC dc = BeginPaint(h, &ps);
      FillRect(dc, &ps.rcPaint, (HBRUSH)GetStockObject(BLACK_BRUSH));
      EndPaint(h, &ps);
      return 0;
    }
  }
  return DefWindowProcW(h, m, w, l);
}

bool FlightGhost::begin(HINSTANCE inst, HWND source, const RECT& screen) {
  end();
  if (!source || screen.right <= screen.left || screen.bottom <= screen.top) return false;
  static bool registered = false;
  if (!registered) {
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = ghost_proc;
    wc.hInstance = inst;
    wc.lpszClassName = L"tinyoffice.Cue.flight";
    registered = RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
  }
  wnd_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT | WS_EX_TOPMOST, L"tinyoffice.Cue.flight", L"",
                         WS_POPUP, screen.left, screen.top, screen.right - screen.left, screen.bottom - screen.top, nullptr, nullptr,
                         inst, nullptr);
  if (!wnd_) return false;
  BOOL off = TRUE;  // shown and gone at once (Windows' own fade would double the flight)
  DwmSetWindowAttribute(wnd_, DWMWA_TRANSITIONS_FORCEDISABLED, &off, sizeof off);
  int round = 2;  // DWMWCP_ROUND: the player's own corners
  DwmSetWindowAttribute(wnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &round, sizeof round);
  // no outline: Windows 11 draws a grey 1 px line round every rounded popup, and the picture
  // flew with a frame the player itself never has (the window's own border is off too)
  COLORREF none = 0xFFFFFFFE;  // DWMWA_COLOR_NONE
  DwmSetWindowAttribute(wnd_, 34 /*DWMWA_BORDER_COLOR*/, &none, sizeof none);
  src_ = source;
  if (FAILED(DwmRegisterThumbnail(wnd_, src_, &thumb_)) || !thumb_) { thumb_ = nullptr; end(); return false; }
  last_ = {};
  if (!update(screen)) { end(); return false; }
  SetWindowPos(wnd_, HWND_TOPMOST, screen.left, screen.top, screen.right - screen.left, screen.bottom - screen.top,
               SWP_NOACTIVATE | SWP_SHOWWINDOW);
  last_ = screen;
  return true;
}

// The picture fills the ghost; the source's client area is cropped to the ghost's shape (centred),
// in the source window's own coordinates (its invisible resize borders left out).
bool FlightGhost::update(const RECT& screen) {
  if (!thumb_) return false;
  RECT wr, cr;
  POINT o{0, 0};
  if (!GetWindowRect(src_, &wr) || !GetClientRect(src_, &cr) || !ClientToScreen(src_, &o)) return false;
  const int cw = cr.right, ch = cr.bottom, gw = screen.right - screen.left, gh = screen.bottom - screen.top;
  if (cw <= 0 || ch <= 0 || gw <= 0 || gh <= 0) return false;
  int sw = cw, sh = ch;
  if ((double)cw / ch > (double)gw / gh) sw = std::max(1, (int)std::lround((double)ch * gw / gh));
  else sh = std::max(1, (int)std::lround((double)cw * gh / gw));
  const int sx = (o.x - wr.left) + (cw - sw) / 2, sy = (o.y - wr.top) + (ch - sh) / 2;
  DWM_THUMBNAIL_PROPERTIES p{};
  p.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_RECTSOURCE | DWM_TNP_VISIBLE | DWM_TNP_OPACITY | DWM_TNP_SOURCECLIENTAREAONLY;
  p.rcDestination = {0, 0, gw, gh};
  p.rcSource = {sx, sy, sx + sw, sy + sh};
  p.opacity = 255;
  p.fVisible = TRUE;
  p.fSourceClientAreaOnly = FALSE;
  return SUCCEEDED(DwmUpdateThumbnailProperties(thumb_, &p));
}

void FlightGhost::place(const RECT& screen) {
  if (!thumb_ || !wnd_) return;
  if (EqualRect(&screen, &last_)) { update(screen); return; }  // (the source may have changed size)
  const bool grows = (screen.right - screen.left) * (screen.bottom - screen.top) > (last_.right - last_.left) * (last_.bottom - last_.top);
  // a frame where the two disagree crops the picture, never shows the black under it: growing,
  // the picture first; shrinking, the window first
  if (grows) update(screen);
  SetWindowPos(wnd_, HWND_TOPMOST, screen.left, screen.top, screen.right - screen.left, screen.bottom - screen.top,
               SWP_NOACTIVATE | SWP_NOCOPYBITS);
  if (!grows) update(screen);
  last_ = screen;
}

void FlightGhost::end() {
  if (thumb_) { DwmUnregisterThumbnail(thumb_); thumb_ = nullptr; }
  if (wnd_) { DestroyWindow(wnd_); wnd_ = nullptr; }
  src_ = nullptr;
  last_ = {};
}

// ── the stage cover ─────────────────────────────────────────────────
// Black, click-through, above everything, never activated: it holds the screen still while the
// window changes size for full screen. Windows otherwise composes a frame or two of the window
// at its new place with its old contents (an L of black over the desktop, the rest of the new
// size not painted yet) before the engine draws at the new size.
static LRESULT CALLBACK cover_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
  switch (m) {
    case WM_NCHITTEST: return HTTRANSPARENT;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_ERASEBKGND: {
      RECT r;
      GetClientRect(h, &r);
      FillRect((HDC)w, &r, (HBRUSH)GetStockObject(BLACK_BRUSH));
      return 1;
    }
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC dc = BeginPaint(h, &ps);
      FillRect(dc, &ps.rcPaint, (HBRUSH)GetStockObject(BLACK_BRUSH));
      EndPaint(h, &ps);
      return 0;
    }
  }
  return DefWindowProcW(h, m, w, l);
}

bool StageCover::show(HINSTANCE inst, const RECT& screen, bool rounded, bool wait) {
  if (screen.right <= screen.left || screen.bottom <= screen.top) return false;
  static bool registered = false;
  if (!registered) {
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = cover_proc;
    wc.hInstance = inst;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"tinyoffice.Cue.stage";
    registered = RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
  }
  if (!wnd_) {
    wnd_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT | WS_EX_TOPMOST, L"tinyoffice.Cue.stage", L"", WS_POPUP,
                           screen.left, screen.top, screen.right - screen.left, screen.bottom - screen.top, nullptr, nullptr, inst, nullptr);
    if (!wnd_) return false;
    BOOL off = TRUE;  // there and gone at once: Windows' own fade would show the desktop through it
    DwmSetWindowAttribute(wnd_, DWMWA_TRANSITIONS_FORCEDISABLED, &off, sizeof off);
    COLORREF none = 0xFFFFFFFE;
    DwmSetWindowAttribute(wnd_, 34 /*DWMWA_BORDER_COLOR*/, &none, sizeof none);
  }
  int corners = rounded ? 2 : 1;  // over a window: its rounded corners; over the screen: square
  DwmSetWindowAttribute(wnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof corners);
  SetWindowPos(wnd_, HWND_TOPMOST, screen.left, screen.top, screen.right - screen.left, screen.bottom - screen.top,
               SWP_NOACTIVATE | SWP_SHOWWINDOW);
  // painted now, not when the message loop next comes round: a window shown and not yet painted
  // is composed empty (the screen showed through it while the player changed size)
  RedrawWindow(wnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
  // and on screen before the caller moves anything under it: one composition pass
  if (wait) DwmFlush();
  return true;
}

void StageCover::end() {
  if (wnd_) { DestroyWindow(wnd_); wnd_ = nullptr; }
}

// ── the grow cover ──────────────────────────────────────────────────
// (15/11, 21:10) A window that grows is shown by Windows at its new size, shadow and all, before
// it has given Windows the pixels of the new area: that area came out transparent (a film of 4:3
// opening from the home, a wide film going back to it), however soon Cue painted it. This cover
// is a layered window: its place, size and pixels go to Windows in the same call
// (UpdateLayeredWindow), so it is never on screen bigger than its picture. It is moved first
// and the window after it, so the window's new area is always under it. Opaque black, corners
// cut in hard pixels to the window's own (rounded on Windows 11, square otherwise); owned by the
// window (just above it, never above other windows); never clicked or activated.
static LRESULT CALLBACK grow_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
  switch (m) {
    case WM_NCHITTEST: return HTTRANSPARENT;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
  }
  return DefWindowProcW(h, m, w, l);
}

// the four corner squares of a w x h cover: `v` (0: cut, black: back to plain) outside the circle
void GrowCover::corners(int w, int h, int r, uint32_t v) {
  if (r <= 0 || !px_) return;
  r = std::min({r, w / 2, h / 2});
  for (int y = 0; y < r; ++y)
    for (int x = 0; x < r; ++x) {
      const double dx = r - (x + 0.5), dy = r - (y + 0.5);
      if (dx * dx + dy * dy <= (r - 0.5) * (r - 0.5)) continue;  // inside: stays black
      px_[(size_t)y * bw_ + x] = v;
      px_[(size_t)y * bw_ + (w - 1 - x)] = v;
      px_[(size_t)(h - 1 - y) * bw_ + x] = v;
      px_[(size_t)(h - 1 - y) * bw_ + (w - 1 - x)] = v;
    }
}

bool GrowCover::place(HINSTANCE inst, HWND owner, const RECT& screen, int radius) {
  const int w = screen.right - screen.left, h = screen.bottom - screen.top;
  if (w <= 0 || h <= 0) return false;
  static bool registered = false;
  if (!registered) {
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = grow_proc;
    wc.hInstance = inst;
    wc.lpszClassName = L"tinyoffice.Cue.grow";
    registered = RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
  }
  if (w > bw_ || h > bh_) {  // a bigger bitmap, with room for the rest of the glide
    const int nw = std::max(bw_, w + w / 4), nh = std::max(bh_, h + h / 4);
    if (!dc_) dc_ = CreateCompatibleDC(nullptr);
    if (!dc_) return false;
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = nw;
    bi.bmiHeader.biHeight = -nh;  // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP b = CreateDIBSection(dc_, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!b || !bits) { if (b) DeleteObject(b); return false; }
    if (bmp_) { SelectObject(dc_, old_); DeleteObject(bmp_); }
    old_ = (HBITMAP)SelectObject(dc_, b);
    bmp_ = b;
    px_ = static_cast<uint32_t*>(bits);
    bw_ = nw; bh_ = nh;
    std::fill(px_, px_ + (size_t)nw * nh, 0xFF000000u);  // opaque black (premultiplied)
    cw_ = ch_ = cr_ = 0;
  }
  if (w != cw_ || h != ch_ || radius != cr_) {
    if (cw_ > 0) corners(cw_, ch_, cr_, 0xFF000000u);
    corners(w, h, radius, 0u);
    cw_ = w; ch_ = h; cr_ = radius;
  }
  const bool fresh = !wnd_;
  if (fresh) {
    wnd_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"tinyoffice.Cue.grow", L"",
                           WS_POPUP, screen.left, screen.top, w, h, owner, nullptr, inst, nullptr);
    if (!wnd_) return false;
    BOOL off = TRUE;  // there and gone at once (Windows' own fade would show the desktop through it)
    DwmSetWindowAttribute(wnd_, DWMWA_TRANSITIONS_FORCEDISABLED, &off, sizeof off);
    int square = 1;   // its corners are its pixels' own
    DwmSetWindowAttribute(wnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &square, sizeof square);
  }
  POINT at{screen.left, screen.top}, from{0, 0};
  SIZE size{w, h};
  BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
  if (!UpdateLayeredWindow(wnd_, nullptr, &at, &size, dc_, &from, 0, &blend, ULW_ALPHA)) { end(); return false; }
  if (fresh) {
    ShowWindow(wnd_, SW_SHOWNOACTIVATE);
    DwmFlush();  // on screen before the window under it starts to grow (once, as the glide starts)
  }
  return true;
}

// gone, and its bitmap with it (a few MB, not kept between glides)
void GrowCover::end() {
  if (wnd_) { DestroyWindow(wnd_); wnd_ = nullptr; }
  if (bmp_) { SelectObject(dc_, old_); DeleteObject(bmp_); bmp_ = old_ = nullptr; }
  if (dc_) { DeleteDC(dc_); dc_ = nullptr; }
  px_ = nullptr;
  bw_ = bh_ = cw_ = ch_ = cr_ = 0;
}

GrowCover::~GrowCover() { end(); }
