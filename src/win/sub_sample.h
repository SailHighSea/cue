// The style panel's sample line, drawn by Windows in the subtitle's own font (Cue Omni, Clean or
// Readable), with its opacity, box or outline and letter spacing, at the panel's scale. Made only
// when what it shows changes (PlayView::sample_request's key); the pixel font stands in otherwise.
#pragma once
#include <memory>

#include "../ui/play.h"

std::shared_ptr<const Canvas> render_sub_sample(const PlayView::SampleRequest& q);
