// The halo coming and going (17/11, ambient_reveal.h): in from the window's edge outward on a curve
// with no bounce, out gathering back into the window, the opacity alone with reduce motion, a change of
// mind half way with no jump, and at rest no extra lookup (the pixels go through no table at all).
#include "../src/ui/ambient_reveal.h"
#include <cassert>
#include <cmath>
#include <cstdio>

int main() {
  const double dt = 1 / 30.0;
  const float* u = AmbientReveal::distance_of_level();
  // the falloff levels map back to their distance: the edge (235) at 0, fainter levels farther out
  assert(u[235] < 0.01f && u[100] > u[200] && u[10] > u[100] && u[2] > 0.6f);
  const int edge = 235, mid = 100, far = 12;

  // in: every level rises without going back or over, the edge ahead of the far part, all lit at 0.9 s
  {
    AmbientReveal r;
    r.begin_in(true);
    assert(r.phase == AmbientReveal::In && r.remapping());  // (nothing has risen yet: the table at 0)
    float last[256]{};
    bool edge_first = false;
    double t = 0, edge_90 = -1, far_90 = -1;
    while (r.phase == AmbientReveal::In) {
      r.step(dt); t += dt;
      for (int m = 0; m < 256; ++m) { assert(r.gain[m] >= last[m] - 1e-6f && r.gain[m] <= 1.0f); last[m] = r.gain[m]; }
      if (r.gain[edge] > 0.5f && r.gain[far] < 0.2f) edge_first = true;
      if (edge_90 < 0 && r.gain[edge] >= 0.9f) edge_90 = t;
      if (far_90 < 0 && r.gain[far] >= 0.9f) far_90 = t;
      assert(r.opacity() == 1.0);  // spreading: the strips stay opaque, the levels carry it
    }
    assert(edge_first && far_90 - edge_90 > 0.2);
    assert(std::fabs(t - AmbientReveal::kIn) < dt + 1e-9 && r.phase == AmbientReveal::On);
    assert(!r.remapping());  // lit: the pixels are painted as before, no table
    uint8_t map[256]; r.remap(map);
    for (int m = 0; m < 256; ++m) assert(map[m] == m);
    // soft start: the first frame shows almost nothing yet
    AmbientReveal s; s.begin_in(true); s.step(dt);
    assert(s.gain[edge] < 0.08f);
  }
  // out: the far part first, the edge last, gone in 0.25 s
  {
    AmbientReveal r; r.lit();
    r.begin_out(AmbientReveal::kOut, true);
    double t = 0; bool far_first = false;
    while (r.phase == AmbientReveal::Out) {
      const float e0 = r.gain[edge], m0 = r.gain[mid];
      r.step(dt); t += dt;
      if (r.phase != AmbientReveal::Out) break;
      assert(r.gain[edge] <= e0 + 1e-6f && r.gain[mid] <= m0 + 1e-6f);
      if (r.gain[far] < 0.3f && r.gain[edge] > 0.7f) far_first = true;
    }
    assert(far_first && t <= AmbientReveal::kOut + dt + 1e-9 && r.phase == AmbientReveal::Off && r.opacity() == 0);
    // into full screen: with the picture, 0.11 s
    AmbientReveal f; f.lit(); f.begin_out(0.11, true);
    int n = 0; while (f.phase == AmbientReveal::Out) { f.step(dt); ++n; }
    assert(n <= 4);
  }
  // a change of mind half way: from where each level is, no jump
  {
    AmbientReveal r; r.begin_in(true);
    for (int i = 0; i < 8; ++i) r.step(dt);
    float before[256]; for (int m = 0; m < 256; ++m) before[m] = r.gain[m];
    r.begin_out(AmbientReveal::kOut, true); r.step(1e-4);
    for (int m = 0; m < 256; ++m) assert(std::fabs(r.gain[m] - before[m]) < 0.02f);
    for (int i = 0; i < 3; ++i) r.step(dt);
    for (int m = 0; m < 256; ++m) before[m] = r.gain[m];
    r.begin_in(true); r.step(1e-4);
    for (int m = 0; m < 256; ++m) assert(std::fabs(r.gain[m] - before[m]) < 0.02f);
  }
  // reduce motion: the opacity alone, on the same curves; the pixels never go through the table
  {
    AmbientReveal r; r.begin_in(false);
    double last = 0;
    while (r.phase == AmbientReveal::In) {
      r.step(dt);
      assert(!r.remapping() && r.opacity() >= last - 1e-9 && r.opacity() <= 1.0);
      last = r.opacity();
    }
    assert(r.opacity() == 1.0);
    r.begin_out(AmbientReveal::kOut, false);
    while (r.phase == AmbientReveal::Out) { r.step(dt); assert(!r.remapping() && r.opacity() <= last + 1e-9); last = r.opacity(); }
    assert(r.opacity() == 0);
  }
  // cut (the window's size changed under it): out at once; then in again from nothing
  {
    AmbientReveal r; r.lit(); r.cut();
    assert(r.phase == AmbientReveal::Off && r.opacity() == 0);
    r.begin_in(true); r.step(dt);
    assert(r.gain[edge] < 0.08f);
  }
  std::puts("ambient_reveal_test: ok");
}
