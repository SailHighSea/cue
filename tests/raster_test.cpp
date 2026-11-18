#include "../src/ui/gfx.h"
#include <cassert>
#include <cstdio>
#include <cmath>
#include <algorithm>
int main(){
 const char* grid="### #" "## ##" "#####" "# # #";
 for(double scale:{1.,2.,2.5,3.,3.5,4.}) for(double a:{.1,.5,1.}) for(bool opaque:{false,true}) {
  Canvas fast,ref; fast.resize(40,40);ref.resize(40,40);
  if(opaque){fast.clear({30,50,80});ref.clear({30,50,80});}
  fast.cells(grid,5,4,-2,3,scale,{200,100,50},a);
  for(int y=0;y<4;++y)for(int x=0;x<5;++x)if(grid[y*5+x]=='#')
    ref.fill(-2+x*scale,3+y*scale,scale,scale,{200,100,50},a);
  assert(fast.px==ref.px);
 }
 // Src-over on a transparent destination is exactly the source, even with alpha.
 Canvas c;c.resize(1,1);c.fill_pixels(0,0,1,1,{200,100,50},.5);
 assert(c.px[0]==0x80643219u);
 // The fast source-over is the reference, bit for bit, for every premultiplied source alpha and
 // channel over every destination channel (and the alpha byte likewise); a source that is not
 // premultiplied keeps the reference path.
 unsigned long long checked = 0;
 for (uint32_t sa = 1; sa < 255; ++sa)
   for (uint32_t sc = 0; sc <= sa; ++sc)
     for (uint32_t d = 0; d < 256; ++d) {
       // a different channel position each time, the others varied too
       const uint32_t src = sa << 24 | sc << 16 | (sc / 2) << 8 | (sa - sc);
       const uint32_t dst = d << 24 | (255 - d) << 16 | d << 8 | ((d * 7) & 255);
       assert(blend_over_for_tests(src, dst) == blend_over_reference_for_tests(src, dst));
       const uint32_t src2 = sa << 24 | (sa - sc) << 16 | sc << 8 | (sc / 3);
       assert(blend_over_for_tests(src2, dst) == blend_over_reference_for_tests(src2, dst));
       ++checked;
     }
 assert(blend_over_for_tests(0x40FF0000u, 0xFF102030u) == blend_over_reference_for_tests(0x40FF0000u, 0xFF102030u));
 // a whole translucent fill over a varied canvas: the same pixels as blending one by one
 {
   Canvas big; big.resize(97, 31);
   for (size_t i = 0; i < big.px.size(); ++i) big.px[i] = (uint32_t)(i * 2654435761u) | 0xFF000000u;
   Canvas want = big;
   for (double a : {.01, .3, .5, .77, .999}) {
     big.fill_pixels(-3, 2, 120, 40, {17, 200, 90}, a);
     Canvas one; one.resize(1, 1); one.fill_pixels(0, 0, 1, 1, {17, 200, 90}, a);
     for (int y = 2; y < 31; ++y) for (int x = 0; x < 97; ++x)
       want.px[(size_t)y * 97 + x] = blend_over_reference_for_tests(one.px[0], want.px[(size_t)y * 97 + x]);
     assert(big.px == want.px);
   }
 }
 // a scene faded as a whole: every byte scaled with the same rounding as one at a time
 for (double a : {0.0, .004, .25, .5, .73, .996}) {
   Canvas fade; fade.resize(67, 5);
   for (size_t i = 0; i < fade.px.size(); ++i) fade.px[i] = (uint32_t)(i * 2246822519u + 12345u);
   Canvas want = fade;
   const uint32_t A = (uint32_t)(a * 255 + 0.5);
   for (auto& v : want.px) {
     auto ch = [&](int sh) { return ((((v >> sh) & 255) * A + 127) / 255) << sh; };
     v = ch(24) | ch(16) | ch(8) | ch(0);
   }
   fade.scale_alpha(a);
   assert(fade.px == want.px);
 }
 // a whole picture laid over another (blit): four pixels at a time where they allow it (all
 // opaque, all clear, all premultiplied), one at a time otherwise; always the reference's pixels.
 // Every mix of the four kinds in a group of four, at every offset and width, clipped or not
 {
   const uint32_t kinds[] = {0xFF336699u, 0u, 0x80402010u, 0x40FF0000u /* not premultiplied */, 0x01010000u, 0xFEFEFDFCu};
   Canvas src; src.resize(61, 9);
   for (size_t i = 0; i < src.px.size(); ++i) {
     const uint32_t k = kinds[(i * 7 + i / 5) % 6];
     src.px[i] = k ? (k & 0xFF000000u) | ((k & 0x00FFFFFFu) ^ (uint32_t)(i * 131 & 0x0F0F0F)) : 0u;
     if (k && k != 0x40FF0000u) {  // keep premultiplied sources premultiplied after the change
       const uint32_t a = k >> 24;
       uint32_t r = (src.px[i] >> 16) & 255, g = (src.px[i] >> 8) & 255, b = src.px[i] & 255;
       src.px[i] = a << 24 | std::min(r, a) << 16 | std::min(g, a) << 8 | std::min(b, a);
     }
   }
   for (int ox : {-3, 0, 1, 2, 5}) for (int clip : {0, 1}) {
     Canvas dst; dst.resize(70, 14);
     for (size_t i = 0; i < dst.px.size(); ++i) dst.px[i] = (uint32_t)(i * 2654435761u) | (i % 3 ? 0xFF000000u : 0u);
     Canvas want = dst;
     if (clip) { dst.set_clip({7, 2, 33, 9}); }
     dst.blit(src, ox, 3, 1.0);
     for (int y = 0; y < src.h; ++y) for (int x = 0; x < src.w; ++x) {
       const int X = ox + x, Y = 3 + y;
       if (X < 0 || X >= want.w || Y < 0 || Y >= want.h) continue;
       if (clip && (X < 7 || X >= 40 || Y < 2 || Y >= 11)) continue;
       const uint32_t v = src.px[(size_t)y * src.w + x];
       uint32_t& d = want.px[(size_t)Y * want.w + X];
       if (v >> 24) d = blend_over_for_tests(v, d);
     }
     assert(dst.px == want.px);
   }
   // and an opaque fill or clear of any width and alignment: the colour, nothing else
   for (int x0 = 0; x0 < 9; ++x0) for (int n = 0; n < 41; ++n) {
     Canvas f; f.resize(60, 2);
     for (auto& v : f.px) v = 0x12345678u;
     f.fill_pixels(x0, 1, n, 1, {9, 8, 7}, 1.0);
     for (int x = 0; x < 60; ++x) {
       assert(f.px[(size_t)x] == 0x12345678u);
       assert(f.px[(size_t)60 + x] == (x >= x0 && x < x0 + n ? 0xFF090807u : 0x12345678u));
     }
   }
 }
 // a panel's border and a switch's outline, at every scale (half ones too: 1.5, 2.5, 3.5): the
 // same thickness on all four sides, wherever they stand (at a half scale each cell used to round
 // where it fell: 2 px on the left and top, 1 on the right and bottom)
 for (double s : {1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.5}) for (int x0 : {10, 11}) for (int y0 : {7, 8}) {
   Canvas c; c.resize(200, 120); c.clear({0, 0, 0});
   draw_panel(c, x0, y0, 61, 31, s, {2, 1}, {0, 0, 0}, 1.0, {255, 255, 255}, 1.0);
   auto lit = [&](int x, int y) { return (c.px[(size_t)y * c.w + x] & 0xFFFFFFu) != 0; };
   auto run = [&](int x, int y, int dx, int dy) {  // from the outside in: the border's thickness
     while (x >= 0 && y >= 0 && x < c.w && y < c.h && !lit(x, y)) { x += dx; y += dy; }
     int n = 0;
     while (x >= 0 && y >= 0 && x < c.w && y < c.h && lit(x, y)) { x += dx; y += dy; ++n; }
     return n;
   };
   const int mx = x0 + (int)(30 * s), my = y0 + (int)(15 * s);
   const int L = run(0, my, 1, 0), R = run(c.w - 1, my, -1, 0), T = run(mx, 0, 0, 1), B = run(mx, c.h - 1, 0, -1);
   assert(L == R && T == B && L == T && L == std::max(1, (int)s));
   Canvas d; d.resize(120, 80); d.clear({0, 0, 0});
   draw_switch(d, x0, y0, s, 0.0, {255, 255, 255}, 0, true, 1);  // off: an outline, the knob on the left
   // the outline's top and bottom rows above and under the knob's track, and its right side
   const int sw = (int)std::lround(kSwitchW * s), sh = (int)std::lround(kSwitchH * s), t = std::max(1, (int)s);
   const uint32_t rim = d.px[(size_t)y0 * d.w + x0 + sw / 2];
   auto dl = [&](int x, int y) { return d.px[(size_t)y * d.w + x] == rim; };
   for (int k = 0; k < t; ++k) {
     assert(dl(x0 + sw / 2, y0 + k) && dl(x0 + sw / 2, y0 + sh - 1 - k) && dl(x0 + sw - 1 - k, y0 + sh / 2));
   }
   assert(!dl(x0 + sw / 2, y0 + t) && !dl(x0 + sw / 2, y0 + sh - 1 - t) && !dl(x0 + sw - 1 - t, y0 + sh / 2));
 }
 std::printf("Raster: pixel spans match individual cells at integer/half scales, clipping and alpha passed; borders and switches even at half scales; blits four at a time; fast source-over equals the reference for %llu source/destination pairs\n", checked);
}
