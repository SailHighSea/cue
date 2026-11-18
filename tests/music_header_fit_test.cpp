// The list's header (15/11, the Biel's capture of a narrow window at 175%): the words beside the
// thumbnail stay beside it and within its height, at every window size, scale and bar style.
#define private public
#include "../src/ui/play.h"
#undef private
#include "../src/core/audio_mode.h"
#include "../src/core/ui_scale.h"
#include <cstdio>
static MusicView appview(PlayView& p, MusicFrame fr, int W, int H) {
  const Rect c = p.list_cover();
  if (c.w > 0) { MusicView v; v.frame = fr; v.f = c.w / (double)p.in.music_scene.side; v.ox = c.x - p.in.music_scene.x * v.f; v.oy = c.y - p.in.music_scene.y * v.f; return v; }
  if (p.music_card_mode() > 0 && p.card_cover().w > 0) { MusicView v; v.frame = fr; Rect cc = p.card_cover(); v.f = cc.w / (double)p.in.music_scene.side; v.ox = cc.x - p.in.music_scene.x * v.f; v.oy = cc.y - p.in.music_scene.y * v.f; return v; }
  int top = H; Rect d = p.blur_cloud().controls; if (d.h > 0 && d.cy() > H / 2) top = d.y;
  if (fr != MusicFrame::Scene && p.music_stacked()) return music_view(p.in.music_scene, fr, W, H, top, p.music_stack_below(), p.music_stack_above());
  return music_view(p.in.music_scene, fr, W, H, top, -1, 0, p.music_words_w());
}
int main() {
  int bad = 0;
  for (int freebar = 0; freebar < 2; ++freebar) for (double bs : {1.5, 1.75, 2.0, 2.5, 3.0}) for (int W = 300; W <= 640; W += 20) for (int H = 700; H <= 1200; H += 50) {
    const double s = std::max(bs, music_compact_scale(bs, W, H, kDefaultUiSize, bs));
    const MusicFrame fr = music_frame_for(W, H, bs);
    PlayView p; p.in.audio_only = true; p.in.cover_scene = true; p.in.has_cover = true; p.in.cover_art = true; p.in.dur = 300; p.in.time = 40;
    p.in.music_title = "Music Baby"; p.in.music_artist = "Jane Remover"; p.in.music_scene = MusicScene::wide();
    for (uint32_t i = 1; i <= 4; ++i) p.in.queue.push_back({i, "s" + std::to_string(i) + ".mp3", i == 3, true});
    p.in.queue_active = true; p.in.has_next = true; p.in.queue_revision = 1; p.in.controls_free = freebar;
    p.set_base_scale(bs); p.resize(W, H, s); double t = 1; p.reset(t);
    auto step = [&](int n) { for (int i = 0; i < n; ++i) { p.in.music_view = appview(p, fr, W, H); p.in.music_view.frame = fr; p.changed(t); t += 1. / 60; p.tick(t, 1. / 60); p.take_actions(); p.prepare(t); } };
    step(40);
    p.open_panel(PlayView::Panel::Queue, PlayView::B_QUEUE, t); step(90);
    const Rect c = p.list_cover(); const Layer& M = p.layers[PlayView::L_MUSIC];
    const bool wrong = c.w > 0 && M.visible && (M.x < c.r() || M.y < c.y - 2 * s || M.y + M.c.h > c.b() + 2 * s);
    if (wrong && bad++ < 12) std::printf("bs %.2f W %d H %d s %.2f frame %d mini %d stack %d hdr %d mix %.2f key %c  words y %d h %d cover y %d b %d wordsrect h %d T\n", bs, W, H, s, (int)fr, p.mini_, p.stack_, p.list_header_, p.list_mix(), (char)p.music_key_.state[3], M.y, M.c.h, c.y, c.b(), p.list_lay_.words.h);
  }
  std::printf("music_header_fit_test: %s (%d)\n", bad ? "FAIL" : "ok", bad);
  return bad ? 1 : 0;
}
