#include "../src/core/skip_parts.h"

#include <cassert>
#include <cmath>
#include <iostream>

int main() {
  using K = SkipKind;
  // the names releases use
  assert(chapter_skip_kind("Opening") == K::Opening);
  assert(chapter_skip_kind("OP") == K::Opening);
  assert(chapter_skip_kind("OP2") == K::Opening);
  assert(chapter_skip_kind("ＯＰ") == K::Opening);
  assert(chapter_skip_kind("Intro") == K::Opening);
  assert(chapter_skip_kind("Opening Credits") == K::Opening);
  assert(chapter_skip_kind("オープニング") == K::Opening);
  assert(chapter_skip_kind("Abertura") == K::Opening);
  assert(chapter_skip_kind("Опенинг") == K::Opening);
  assert(chapter_skip_kind("Sigla iniziale") == K::Opening);
  assert(chapter_skip_kind("Ending") == K::Ending);
  assert(chapter_skip_kind("ED") == K::Ending);
  assert(chapter_skip_kind("ed01") == K::Ending);
  assert(chapter_skip_kind("End Credits") == K::Ending);
  assert(chapter_skip_kind("Credits") == K::Ending);
  assert(chapter_skip_kind("エンディング") == K::Ending);
  assert(chapter_skip_kind("Sigla finale") == K::Ending);
  assert(chapter_skip_kind("Générique de fin") == K::Ending);
  assert(chapter_skip_kind("Créditos finais") == K::Ending);
  assert(chapter_skip_kind("Preview") == K::Preview);
  assert(chapter_skip_kind("Next Episode Preview") == K::Preview);
  assert(chapter_skip_kind("次回予告") == K::Preview);
  assert(chapter_skip_kind("Prévia") == K::Preview);
  assert(chapter_skip_kind("Recap") == K::Recap);
  assert(chapter_skip_kind("Previously on...") == K::Recap);
  assert(chapter_skip_kind("前回のあらすじ") == K::Recap);
  // the rest of a film is not a part to skip
  for (const char* other : {"", "Part A", "Part B", "Chapter 3", "Prologue", "Avant", "Eyecatch", "Opera", "Edge",
                            "Topography", "The Open Road", "Credit Card", "Development", "Media", "Stop", "Red"})
    assert(chapter_skip_kind(other) == K::None);
  // the earliest mention wins
  assert(chapter_skip_kind("Credits + Preview") == K::Ending);
  assert(chapter_skip_kind("Preview & Credits") == K::Preview);

  // an anime episode of 24 min with its release's chapters
  std::vector<Chapter> ep = {{0, "Avant"}, {95, "Opening"}, {185, "Part A"}, {720, "Part B"},
                             {1290, "Ending"}, {1380, "Preview"}};
  auto parts = skip_parts_from_chapters(ep, 1410);
  assert(parts.size() == 3);
  assert(parts[0].kind == K::Opening && parts[0].start == 95 && parts[0].end == 185 && !parts[0].from_sound);
  assert(parts[1].kind == K::Ending && parts[1].end == 1380);
  assert(parts[2].kind == K::Preview && parts[2].end == 1410);
  // where a skip lands: the opening's end; the ending runs into the preview: past both, short of the end
  assert(skip_target(parts, 0, 1410) == 185);
  assert(std::abs(skip_target(parts, 1, 1410) - 1409.5) < 1e-9);
  // which part plays when: none in its last second, none outside
  assert(skip_part_at(parts, 94) == -1);
  assert(skip_part_at(parts, 95) == 0);
  assert(skip_part_at(parts, 183.9) == 0);
  assert(skip_part_at(parts, 184.5) == -1);
  assert(skip_part_at(parts, 600) == -1);
  assert(skip_part_at(parts, 1300) == 1);
  assert(skip_part_at(parts, NAN) == -1);
  // the last chapter waits for the film's length
  assert(skip_parts_from_chapters(ep, 0).size() == 2);
  // neighbours of one kind join
  auto joined = skip_parts_from_chapters({{0, "Cold open"}, {60, "OP"}, {100, "OP (cont.)"}, {150, "Part A"}}, 1400);
  assert(joined.size() == 1 && joined[0].start == 60 && joined[0].end == 150);
  // too short, too long, a single chapter: nothing
  assert(skip_parts_from_chapters({{0, "Intro"}, {3, "Main"}}, 1000).empty());
  assert(skip_parts_from_chapters({{0, "Main"}, {500, "Credits"}}, 1400).empty());   // 15 min of "credits"
  assert(skip_parts_from_chapters({{0, "Main"}, {600, "Credits"}}, 1000).empty());  // 400 s: over 6 min
  assert(skip_parts_from_chapters({{0, "Opening"}}, 1000).empty());

  // the sound fills in a kind the chapters do not give; never over a chapter's part
  std::vector<SkipPart> sound = {{K::Opening, 90, 180, true}, {K::Ending, 1300, 1390, true}};
  auto only_op = skip_parts_from_chapters({{0, "Main"}, {80, "Opening"}, {170, "Main"}}, 1400);
  auto merged = merge_skip_parts(only_op, sound);
  assert(merged.size() == 2 && merged[0].start == 80 && !merged[0].from_sound && merged[1].from_sound);
  assert(merge_skip_parts({}, sound).size() == 2);
  std::cout << "skip_parts_test ok\n";
}
