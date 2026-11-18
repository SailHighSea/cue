// Windows integration check against the shipped engine DLL: HDR films, as the engine reports them
// and as Cue's preview helper sees them (the timeline's pictures, the home's frames and the
// ambient light's colours all come from it: vo=null, software decoding, scaled, screenshot-raw).
// Compile with src/win/mpv.cpp, src/ui/atmosphere.cpp, src/core/i18n.cpp, src/core/json.cpp.
// Usage: hdr_engine_test.exe DLL_DIRECTORY SDR HDR10 HLG
//   the same picture three times: SDR (BT.709), HDR10 (PQ, BT.2020, mastering metadata, MaxCLL)
//   and HLG (BT.2020); the HDR ones made from the SDR one with the SDR white at 203 nits.
// Checks: the engine decodes all three and names their colours (primaries, transfer, the peak);
// and it prints, for each, the preview's mean colour and saturation next to the SDR one's, so a
// washed-out (or over-dark) HDR preview shows in numbers: read as SDR (as before 1.4's review) and as
// Cue's helper reads it now, tone mapped (src/core/hdr_preview.h), which must be close to the SDR one.
#ifdef _WIN32
#include "../src/win/mpv.h"
#include "../src/ui/atmosphere.h"
#include "../src/core/hdr_preview.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static std::string utf8(const wchar_t* w) {
  int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
  std::string s(n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
  s.pop_back();
  return s;
}

struct Look { double r = 0, g = 0, b = 0, sat = 0, luma = 0; FrameColors colors; };

int wmain(int argc, wchar_t** argv) {
  assert(argc == 5);
  Mpv m;
  std::string error;
  assert(m.load(argv[1], &error));
  assert(m.create(nullptr, 0, 0, &error));
  auto o = [&](const char* k, const char* v) { m.option(k, v); };
  // the preview helper's own options (src/win/thumbhost.cpp)
  o("config", "no"); o("terminal", "no"); o("idle", "yes"); o("pause", "yes"); o("keep-open", "always");
  o("vo", "null"); o("audio", "no"); o("hwdec", "no"); o("hr-seek", "no");
  o("vd-lavc-skiploopfilter", "all"); o("vd-lavc-fast", "yes"); o("sws-scaler", "fast-bilinear");
  o("load-scripts", "no"); o("msg-level", "all=no");
  assert(m.api().initialize(m.handle()) >= 0);
  bool loaded = false, failed = false;
  auto pump = [&](DWORD ms) {
    DWORD end = GetTickCount() + ms;
    do {
      while (auto* e = m.wait()) {
        if (e->event_id == MPV_EVENT_NONE) break;
        if (e->event_id == MPV_EVENT_FILE_LOADED) loaded = true;
        if (e->event_id == MPV_EVENT_END_FILE) {
          auto* ef = static_cast<mpv_event_end_file*>(e->data);
          if (ef && ef->reason == MPV_END_FILE_REASON_ERROR) failed = true;
        }
      }
      Sleep(5);
    } while (GetTickCount() < end);
  };
  auto look = [&](const std::string& path, const char* name) {
    loaded = failed = false;
    m.set("vf", "scale=160:-2");
    assert(m.cmd({"loadfile", path}) >= 0);
    for (int i = 0; i < 500 && !loaded && !failed; ++i) pump(10);
    assert(loaded && !failed);
    m.cmd({"seek", "1", "absolute"});
    pump(400);
    const std::string prim = m.get_string("video-params/primaries"), gamma = m.get_string("video-params/gamma");
    const double peak = m.get_double("video-params/sig-peak", 0), cll = m.get_double("video-params/max-cll", 0);
    std::printf("hdr engine: %-5s %s / %s, peak %.2f (x203 nits), MaxCLL %.0f\n", name, prim.c_str(), gamma.c_str(), peak, cll);
    mpv_node res{};
    const char* sh[] = {"screenshot-raw", "video", nullptr};
    assert(m.api().command_ret(m.handle(), sh, &res) >= 0 && res.format == MPV_FORMAT_NODE_MAP);
    int64_t w = 0, h = 0, stride = 0;
    const mpv_byte_array* ba = nullptr;
    for (int i = 0; i < res.u.list->num; ++i) {
      const char* k = res.u.list->keys[i];
      const mpv_node& v = res.u.list->values[i];
      if (!strcmp(k, "w")) w = v.u.int64;
      else if (!strcmp(k, "h")) h = v.u.int64;
      else if (!strcmp(k, "stride")) stride = v.u.int64;
      else if (!strcmp(k, "data")) ba = (const mpv_byte_array*)v.u.ba;
    }
    assert(w > 0 && h > 0 && ba);
    std::vector<uint32_t> px((size_t)w * h);
    Look L;
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x) {
        const uint32_t v = ((const uint32_t*)((const uint8_t*)ba->data + y * stride))[x] | 0xFF000000u;
        px[(size_t)y * w + x] = v;
        const double r = (v >> 16 & 255) / 255.0, g = (v >> 8 & 255) / 255.0, b = (v & 255) / 255.0;
        L.r += r; L.g += g; L.b += b;
        const double mx = std::max({r, g, b}), mn = std::min({r, g, b});
        L.sat += mx > 0 ? (mx - mn) / mx : 0;
        L.luma += 0.2126 * r + 0.7152 * g + 0.0722 * b;
      }
    const double n = (double)w * h;
    L.r /= n; L.g /= n; L.b /= n; L.sat /= n; L.luma /= n;
    L.colors = analyze_frame(px.data(), (int)w, (int)h);
    m.api().free_node_contents(&res);
    std::printf("hdr engine: %-5s preview %lldx%lld: mean rgb %.3f %.3f %.3f, luma %.3f, saturation %.3f\n", name, (long long)w,
                (long long)h, L.r, L.g, L.b, L.luma, L.sat);
    if (std::string(name) == "SDR") assert(prim == "bt.709" || prim == "auto");
    else assert(prim == "bt.2020");
    if (std::string(name) == "HDR10") assert(gamma == "pq" && cll >= 900);
    if (std::string(name) == "HLG") assert(gamma == "hlg");
    return L;
  };
  // Cue's HDR preview (src/core/hdr_preview.h, as the helper does it): the frame read at 16 bits
  // (screenshot-raw ... rgba64) and tone mapped; its look is compared with the SDR one's.
  auto mapped = [&](const char* name) {
    Look L;
    mpv_node res{};
    const char* sh[] = {"screenshot-raw", "video", "rgba64", nullptr};
    m.set("sws-scaler", "bicubic");  // as the helper does for an HDR film
    assert(m.api().command_ret(m.handle(), sh, &res) >= 0 && res.format == MPV_FORMAT_NODE_MAP);
    m.set("sws-scaler", "fast-bilinear");
    int64_t w = 0, h = 0, stride = 0;
    const mpv_byte_array* ba = nullptr;
    std::string format;
    for (int i = 0; i < res.u.list->num; ++i) {
      const char* k = res.u.list->keys[i];
      const mpv_node& v = res.u.list->values[i];
      if (!strcmp(k, "w")) w = v.u.int64; else if (!strcmp(k, "h")) h = v.u.int64;
      else if (!strcmp(k, "stride")) stride = v.u.int64; else if (!strcmp(k, "data")) ba = (const mpv_byte_array*)v.u.ba;
      else if (!strcmp(k, "format") && v.format == MPV_FORMAT_STRING) format = v.u.string;
    }
    assert(format == "rgba64" && w > 0 && h > 0 && ba && stride >= w * 8);
    const auto transfer = hdr_preview::transfer_of(m.get_string("video-params/gamma"));
    assert(transfer != hdr_preview::Transfer::None);
    const auto& mapper = hdr_preview::Mapper::get(transfer);
    std::vector<uint32_t> px((size_t)w * h);
    LARGE_INTEGER f, t0, t1;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t0);
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x) {
        uint16_t q[4];
        std::memcpy(q, (const uint8_t*)ba->data + y * stride + x * 8, sizeof q);
        px[(size_t)y * w + x] = mapper.map(q);
      }
    QueryPerformanceCounter(&t1);
    // no stripes: in these flat colour bars each row is like the one above it (fast-bilinear made
    // every other row another colour)
    int striped = 0;
    for (int y = 1; y < h; ++y) {
      const uint32_t a = px[(size_t)y * w + w / 3], b = px[(size_t)(y - 1) * w + w / 3];
      for (int c = 0; c < 24; c += 8) striped += std::abs((int)(a >> c & 255) - (int)(b >> c & 255)) > 24;
    }
    assert(striped <= 6);
    for (uint32_t v : px) {
      const double r = (v >> 16 & 255) / 255.0, g = (v >> 8 & 255) / 255.0, b = (v & 255) / 255.0;
      L.r += r; L.g += g; L.b += b;
      const double mx = std::max({r, g, b}), mn = std::min({r, g, b});
      L.sat += mx > 0 ? (mx - mn) / mx : 0;
      L.luma += 0.2126 * r + 0.7152 * g + 0.0722 * b;
    }
    const double n = (double)w * h;
    L.r /= n; L.g /= n; L.b /= n; L.sat /= n; L.luma /= n;
    L.colors = analyze_frame(px.data(), (int)w, (int)h);
    m.api().free_node_contents(&res);
    std::printf("hdr engine: %-5s Cue's HDR preview: mean rgb %.3f %.3f %.3f, luma %.3f, saturation %.3f (mapped in %.2f ms)\n", name,
                L.r, L.g, L.b, L.luma, L.sat, (t1.QuadPart - t0.QuadPart) * 1000.0 / f.QuadPart);
    return L;
  };
  const Look sdr = look(utf8(argv[2]), "SDR");
  const Look pq = look(utf8(argv[3]), "HDR10");
  const Look pq_mapped = mapped("HDR10");
  const Look hlg = look(utf8(argv[4]), "HLG");
  const Look hlg_mapped = mapped("HLG");
  auto vs = [&](const Look& L, const char* what) {
    const double dl = (L.luma / sdr.luma - 1) * 100, ds = (L.sat / sdr.sat - 1) * 100;
    std::printf("hdr engine: %s vs SDR: luma %+.0f%%, saturation %+.0f%%, accent %d %d %d vs %d %d %d\n", what, dl, ds,
                L.colors.accent.r, L.colors.accent.g, L.colors.accent.b, sdr.colors.accent.r, sdr.colors.accent.g, sdr.colors.accent.b);
    assert(L.luma > 0.05);  // a picture, not black
  };
  vs(pq, "HDR10 read as SDR (before)");
  vs(pq_mapped, "HDR10, Cue's HDR preview");
  vs(hlg, "HLG read as SDR (before)");
  vs(hlg_mapped, "HLG, Cue's HDR preview");
  // the tone mapped previews are close to the SDR picture (they were 41-56% less saturated, and their
  // accent another colour). HDR10 comes back whole; HLG is decoded as BT.2100 says (a 1000-nit display's
  // system gamma), and this sample was made by a chain that does not quite: it comes much closer.
  auto close_to = [&](const Look& L, double sat, double luma) {
    assert(std::fabs(L.sat / sdr.sat - 1) < sat && std::fabs(L.luma / sdr.luma - 1) < luma);
    const Rgb a = L.colors.accent, b = sdr.colors.accent;
    assert(std::abs(a.r - b.r) <= 24 && std::abs(a.g - b.g) <= 24 && std::abs(a.b - b.b) <= 24);
  };
  close_to(pq_mapped, 0.15, 0.15);
  close_to(hlg_mapped, 0.3, 0.2);
  assert(std::fabs(hlg_mapped.sat / sdr.sat - 1) < std::fabs(hlg.sat / sdr.sat - 1) - 0.1);
  std::puts("hdr engine: passed");
  return 0;
}
#else
int main() { return 0; }
#endif
