// Canvas::assign_scaled (15/11): one pass for what was a copy and scale_alpha; the same pixels,
// every alpha, every size (the vector part and the tail)
#include "../src/ui/gfx.h"
#include <cassert>
#include <cstdio>
#include <random>
int main() {
  std::mt19937 rng(7);
  int checked = 0;
  for (int w : {1, 3, 4, 5, 17, 64, 333}) for (int h : {1, 2, 9}) {
    Canvas src; src.resize(w, h);
    for (auto& p : src.px) {
      // premultiplied: each colour no larger than its alpha
      const uint32_t a = rng() & 255;
      auto c = [&] { return a ? (uint32_t)(rng() % (a + 1)) : 0u; };
      p = (a << 24) | (c() << 16) | (c() << 8) | c();
    }
    for (double a : {0.0, 0.001, 0.2, 0.5, 0.73, 0.999, 1.0, 1.5}) {
      Canvas old = src; if (a < 1) old.scale_alpha(a);
      Canvas now; now.resize(2, 2);  // (a canvas that held something else)
      now.assign_scaled(src, a);
      assert(now.w == old.w && now.h == old.h && now.px == old.px);
      ++checked;
    }
  }
  std::printf("canvas_assign_scaled_test: ok (%d cases)\n", checked);
}
