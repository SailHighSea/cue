// Light, the interface's other material: soft, never on the grid. It only ever comes from a
// picture (a video, a thumbnail, the welcome's little scene) or from the eyes, and it is made
// once for a size and its colours, then blitted: nothing is computed per frame.
#pragma once
#include "gfx.h"

// The eyes' own light, wherever they are lit: cool on one side, warm on the other.
constexpr Rgb kEyeCool{64, 84, 210}, kEyeMid{118, 86, 178}, kEyeWarm{226, 146, 82};

// An ellipse of light filling w x h, brightest in the middle and gone at the rim, its colour
// running from `left` through `mid` to `right`. Premultiplied, ready for Canvas::blit.
void radial_light(Canvas& out, int w, int h, Rgb left, Rgb mid, Rgb right, double peak);

// The light already laid on a plain ground (`bg` at `bg_a`): a frame that starts with that ground
// copies it in place (paste) instead of blending the light again, pixel by pixel.
void compose_on(Canvas& out, const Canvas& light, Rgb bg, double bg_a);
// Copies src's pixels over c at (x, y), as they are (no blending), within c's clip.
void paste(Canvas& c, const Canvas& src, int x, int y);

// Light around a picture: `inner` (in out's coordinates) stays empty, the light falls off over
// `reach` px outside it. `corner` holds the colour at the top-left, top-right, bottom-right and
// bottom-left of the picture; between them it is mixed by position. `peak` is the alpha at the
// picture's edge.
void rect_light(Canvas& out, int w, int h, const Rect& inner, int reach, const Rgb corner[4], double peak);

// The same light, softer: it comes from behind the picture (a rectangle inside it), so it is
// already fading where the picture ends, with round corners and a long tail. For a picture that
// glows rather than one that is framed (the home's "continuar").
void glow_light(Canvas& out, int w, int h, const Rect& inner, int reach, const Rgb corner[4], double peak);

// Four colours a picture gives its surroundings: the average of each corner's quarter, kept a
// little brighter than the picture so a dark frame still glows. px: 0xAARRGGBB, w x h.
void edge_colours(const uint32_t* px, int w, int h, Rgb out[4]);

// The opening iris over a finished frame: black outside an ellipse with the canvas' proportions,
// in blocks of `cell` px, k from 0 (all black) to 1 (a little past the corners). The same shape
// the video's iris has (PlayView::render_iris, the cue_ir shader pass).
void iris_cover(Canvas& c, double k, int cell);
inline double iris_ease(double t) { t = t < 0 ? 0 : t > 1 ? 1 : t; return 1 - (1 - t) * (1 - t) * (1 - t); }
constexpr double kIrisOpen = 0.36;  // seconds, as the video's
