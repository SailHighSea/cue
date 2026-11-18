// The parts of an episode a viewer usually skips: the recap at the start ("previously on…"), the
// opening, the ending (its credits) and the preview of the next episode. Found two ways:
//  * the file's own chapters, by name ("Opening", "OP", "Ending", "ED", "Preview", "オープニング",
//    "Abertura"…): free, the chapter list is already in memory
//  * the sound (see core/intro_match.h): the opening and the ending repeat, note for note, in
//    every episode of a season; found by comparing two episodes, for files without such chapters
// Only offered, never skipped without a click.
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "model.h"

enum class SkipKind : uint8_t { None, Recap, Opening, Ending, Preview };

struct SkipPart {
  SkipKind kind = SkipKind::None;
  double start = 0, end = 0;
  bool from_sound = false;  // found by the sound, not by a chapter's name
  bool operator==(const SkipPart& o) const {
    return kind == o.kind && start == o.start && end == o.end && from_sound == o.from_sound;
  }
};

// What a chapter's name says it is (None for anything else: "Part A", "Chapter 3", "").
SkipKind chapter_skip_kind(std::string_view title);

// The skippable chapters of a film of `dur` seconds (0: not known yet), in time order. Neighbours
// of one kind join ("Opening", "Opening (cont.)"). A part of under 5 s, over 6 min or over a third
// of the film is not offered (a "Credits" chapter holding half a film is not its credits).
std::vector<SkipPart> skip_parts_from_chapters(const std::vector<Chapter>& chapters, double dur);

// Parts found by the sound join those from chapters: a kind the chapters already give is kept
// from the chapters (a release's own marks win).
std::vector<SkipPart> merge_skip_parts(std::vector<SkipPart> chapters, const std::vector<SkipPart>& sound);

// The part playing at `t`, -1 for none. The last second of a part offers nothing (a click there
// would barely move), nor does a part already left.
int skip_part_at(const std::vector<SkipPart>& parts, double t);

// Where a skip lands: the end of the part. An ending that runs into the preview skips both;
// a part reaching the end of the film lands just before it (the next episode's notice decides).
double skip_target(const std::vector<SkipPart>& parts, int i, double dur);
