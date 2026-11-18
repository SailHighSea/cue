// Renders Cue screens to PNG on any OS (uses ImageMagick `convert` to encode).
// Build: see README.md (Desenvolvimento).
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include "../src/core/i18n.h"
#include <vector>

#include "../src/ui/gfx.h"
#include "../src/ui/home.h"
#ifdef WITH_PLAY
#include "../src/ui/play.h"
#endif

void save_canvas(const Canvas& c, const std::string& name);
static void save(const Canvas& c, const std::string& name) { save_canvas(c, name); }
void save_canvas(const Canvas& c, const std::string& name) {
  std::string raw = "/tmp/preview.rgba";
  FILE* f = std::fopen(raw.c_str(), "wb");
  std::vector<unsigned char> buf((size_t)c.w * c.h * 4);
  for (size_t i = 0; i < c.px.size(); ++i) {
    uint32_t v = c.px[i];
    unsigned a = v >> 24;
    auto un = [&](unsigned x) { return a ? std::min(255u, (x * 255 + a / 2) / a) : 0u; };
    buf[i * 4 + 0] = (unsigned char)un((v >> 16) & 255);
    buf[i * 4 + 1] = (unsigned char)un((v >> 8) & 255);
    buf[i * 4 + 2] = (unsigned char)un(v & 255);
    buf[i * 4 + 3] = (unsigned char)a;
  }
  std::fwrite(buf.data(), 1, buf.size(), f);
  std::fclose(f);
  std::string cmd = "convert -size " + std::to_string(c.w) + "x" + std::to_string(c.h) + " -depth 8 rgba:" + raw +
                    " " + name;
  if (std::system(cmd.c_str()) != 0) std::fprintf(stderr, "convert failed\n");
  std::printf("%s\n", name.c_str());
}


// sample pictures for the list (raw 0xFFRRGGBB files made by the caller, e.g. ImageMagick):
// /tmp/cue-thumb-<name>.rgba with its size in the first two lines of /tmp/cue-thumb-<name>.txt
static std::shared_ptr<const HomeThumb> load_thumb(const std::string& name, bool cover = false) {
  FILE* m = std::fopen(("/tmp/cue-thumb-" + name + ".txt").c_str(), "r");
  if (!m) return nullptr;
  int w = 0, h = 0;
  if (std::fscanf(m, "%d %d", &w, &h) != 2) { std::fclose(m); return nullptr; }
  std::fclose(m);
  auto t = std::make_shared<HomeThumb>();
  t->w = w; t->h = h; t->cover = cover;
  t->px.resize((size_t)w * h);
  FILE* f = std::fopen(("/tmp/cue-thumb-" + name + ".rgba").c_str(), "rb");
  if (!f) return nullptr;
  std::vector<unsigned char> b((size_t)w * h * 4);
  size_t got = std::fread(b.data(), 1, b.size(), f);
  std::fclose(f);
  if (got != b.size()) return nullptr;
  for (size_t i = 0; i < t->px.size(); ++i) t->px[i] = 0xFF000000u | b[i * 4] << 16 | b[i * 4 + 1] << 8 | b[i * 4 + 2];
  return t;
}

static std::vector<HomeRow> sample_rows() {
  std::vector<HomeRow> r = {
      {"Cena de exemplo", "faltam 41 min", 0.51, "ontem", false, HomeKind::Video, "C:\\v\\cena.mkv", load_thumb("frame1")},
      {"Série de exemplo", "faltam 12 min", 0.62, "S01E04 · ontem", false, HomeKind::Episode, "C:\\v\\serie.mkv", load_thumb("frame2")},
      {"entrevista", "faltam 26 min", 0.3, "há 3 dias", true, HomeKind::Listen, "C:\\a\\entrevista.mp3"},
      {"Pela Janela", "4 min", -1, "Maria Bethânia · há 1 semana", true, HomeKind::Music, "C:\\m\\janela.flac"},
      {"O Hobbit", "faltam 6 h 12 min", 0.2, "capítulo 4 de 19 · há 2 semanas", true, HomeKind::Book, "C:\\b\\hobbit.m4b"},
      {"Landscape at sunset (final revision)", "12 min", -1, "há 2 meses", false, HomeKind::Video, "C:\\v\\landscape.mp4"},
  };
  return r;
}

static void run(HomeView& hv, double& now, double secs) {
  for (double t = 0; t < secs; t += 1 / 60.0) { now += 1 / 60.0; hv.tick(now, 1 / 60.0); }
}

int main(int argc, char** argv) {
  // CUE_LANG=en|es|fr|zh-Hans picks the interface language (Portuguese by default)
  { i18n::Lang l = i18n::Lang::PtBR; if (const char* e = std::getenv("CUE_LANG")) i18n::parse_tag(e, l); i18n::set_language(l); }
  std::string out = argc > 1 ? argv[1] : "out";
  double s = argc > 2 ? std::atof(argv[2]) : 2;
  // optional window size in art px (argv 3 and 4): a maximized window, an ultrawide
  int W = (int)std::lround((argc > 3 ? std::atoi(argv[3]) : 480) * s), H = (int)std::lround((argc > 4 ? std::atoi(argv[4]) : 270) * s);
  HomeView hv;
  hv.set_size(W, H, s);
  hv.set_rows(sample_rows());
  hv.set_thumb(3, load_thumb("cover", true), -100);
  Canvas c;
  double now = 10;
  run(hv, now, 0.1);
  hv.render(c, now); save(c, out + "/home.png");

  hv.mouse_move(W * 0.7, hv.face.cy - 0, now);
  hv.set_hover_row(0, now);
  run(hv, now, 0.6);
  hv.render(c, now); save(c, out + "/home-hover.png");
  hv.mouse_leave(now);
  run(hv, now, 0.6);
  // the ··· menu next to Recentes
  {
    const Rect d = hv.dots_rect();
    hv.mouse_move(d.x + d.w / 2, d.y + d.h / 2, now);
    run(hv, now, 0.3);
    hv.mouse_down(d.x + d.w / 2, d.y + d.h / 2, now);
    run(hv, now, 0.4);
    hv.render(c, now); save(c, out + "/home-menu.png");
    hv.key("esc", false, now);
    hv.mouse_leave(now);
    run(hv, now, 0.6);
  }

  // the survey's invitation: the card in the corner, a few frames of its little scene
  {
    hv.survey = true;
    run(hv, now, 0.6);
    for (double at : {0.3, 0.9, 1.6, 2.4, 3.1}) {
      run(hv, now, at - (at == 0.3 ? 0 : 0));
      hv.render(c, now);
      char nm[64]; std::snprintf(nm, sizeof nm, "/home-survey-%.1f.png", at);
      save(c, out + nm);
    }
    const Rect g = hv.survey_answer_rect();
    hv.mouse_move(g.x + g.w / 2, g.y + g.h / 2, now);
    run(hv, now, 0.4);
    hv.render(c, now); save(c, out + "/home-survey-hover.png");
    hv.mouse_leave(now);
    hv.survey = false;
    run(hv, now, 0.6);
  }

  hv.drag_over(W * 0.8, H * 0.2, now);
  run(hv, now, 0.6);
  hv.render(c, now); save(c, out + "/home-drag.png");
  hv.drag_leave(now);
  run(hv, now, 1);

  for (int v = 1; v <= 3; ++v) {
    hv.force_egg(v, now);
    double mid[4] = {0, 3.0, 1.5, 2.6};
    run(hv, now, mid[v]);
    hv.render(c, now); save(c, out + "/egg" + std::to_string(v) + ".png");
    run(hv, now, 5);
  }

  HomeView empty;
  empty.set_size(W, H, s);
  empty.render(c, 1); save(c, out + "/home-empty.png");

#ifdef WITH_PLAY
  extern void preview_play(const std::string&, int);
  preview_play(out, (int)s);
#endif
  return 0;
}
