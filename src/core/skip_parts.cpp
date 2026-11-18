#include "skip_parts.h"

#include <algorithm>
#include <cmath>

namespace {

// UTF-8 to code points (a broken byte counts as itself)
std::u32string decode(std::string_view s) {
  std::u32string out;
  for (size_t i = 0; i < s.size();) {
    unsigned char c = (unsigned char)s[i];
    int n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 1;
    if (i + n > s.size()) n = 1;
    char32_t cp = n == 1 ? c : n == 2 ? (c & 0x1F) : n == 3 ? (c & 0x0F) : (c & 0x07);
    for (int k = 1; k < n; ++k) cp = (cp << 6) | ((unsigned char)s[i + k] & 0x3F);
    out.push_back(cp);
    i += n;
  }
  return out;
}

char32_t lower(char32_t c) {
  if (c >= 0xFF01 && c <= 0xFF5E) c -= 0xFF01 - 0x21;  // full-width ＯＰ: OP
  if (c >= 'A' && c <= 'Z') return c + 32;
  if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return c + 32;  // À-Þ
  if (c >= 0x410 && c <= 0x42F) return c + 32;             // А-Я
  if (c >= 0x400 && c <= 0x40F) return c + 80;             // Ё…
  return c;
}

bool separator(char32_t c) {
  if (c < 0x80) return !((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'));
  return c == 0x3000 || c == 0x30FB || c == 0x00B7 || c == 0x2013 || c == 0x2014 || c == 0xFF5E;  // 　・·–—～
}

struct Key {
  const char* text;  // lower case; words apart by one space
  SkipKind kind;
};
// Latin and Cyrillic: whole words, in this order. CJK: anywhere in the name.
constexpr Key kWords[] = {
    // opening
    {"opening", SkipKind::Opening}, {"openings", SkipKind::Opening}, {"intro", SkipKind::Opening},
    {"opening credits", SkipKind::Opening}, {"title sequence", SkipKind::Opening}, {"theme song", SkipKind::Opening},
    {"abertura", SkipKind::Opening}, {"apertura", SkipKind::Opening}, {"vorspann", SkipKind::Opening},
    {"générique de début", SkipKind::Opening}, {"générique d ouverture", SkipKind::Opening},
    {"sigla", SkipKind::Opening}, {"sigla iniziale", SkipKind::Opening}, {"опенинг", SkipKind::Opening},
    {"заставка", SkipKind::Opening},
    // ending
    {"ending", SkipKind::Ending}, {"endings", SkipKind::Ending}, {"outro", SkipKind::Ending},
    {"credits", SkipKind::Ending}, {"end credits", SkipKind::Ending}, {"closing credits", SkipKind::Ending},
    {"encerramento", SkipKind::Ending}, {"créditos", SkipKind::Ending}, {"créditos finais", SkipKind::Ending},
    {"abspann", SkipKind::Ending}, {"générique de fin", SkipKind::Ending}, {"sigla finale", SkipKind::Ending},
    {"titoli di coda", SkipKind::Ending}, {"эндинг", SkipKind::Ending}, {"титры", SkipKind::Ending},
    // preview
    {"preview", SkipKind::Preview}, {"next episode", SkipKind::Preview}, {"next time", SkipKind::Preview},
    {"prévia", SkipKind::Preview}, {"próximo episódio", SkipKind::Preview}, {"avance", SkipKind::Preview},
    {"adelanto", SkipKind::Preview}, {"próximo episodio", SkipKind::Preview}, {"aperçu", SkipKind::Preview},
    {"prochain épisode", SkipKind::Preview}, {"vorschau", SkipKind::Preview}, {"anteprima", SkipKind::Preview},
    {"prossimo episodio", SkipKind::Preview}, {"превью", SkipKind::Preview}, {"анонс", SkipKind::Preview},
    // recap
    {"recap", SkipKind::Recap}, {"previously", SkipKind::Recap}, {"resumo", SkipKind::Recap},
    {"anteriormente", SkipKind::Recap}, {"resumen", SkipKind::Recap}, {"résumé", SkipKind::Recap},
    {"précédemment", SkipKind::Recap}, {"rückblick", SkipKind::Recap}, {"was bisher geschah", SkipKind::Recap},
    {"riassunto", SkipKind::Recap}, {"ранее", SkipKind::Recap},
};
constexpr Key kAnywhere[] = {
    {"オープニング", SkipKind::Opening}, {"片头", SkipKind::Opening}, {"片頭", SkipKind::Opening},
    {"오프닝", SkipKind::Opening},
    {"エンディング", SkipKind::Ending}, {"片尾", SkipKind::Ending}, {"엔딩", SkipKind::Ending},
    {"予告", SkipKind::Preview}, {"预告", SkipKind::Preview}, {"預告", SkipKind::Preview}, {"예고", SkipKind::Preview},
    {"前回", SkipKind::Recap}, {"あらすじ", SkipKind::Recap}, {"前情提要", SkipKind::Recap}, {"지난 이야기", SkipKind::Recap},
};

// "op", "op2", "ed", "ed01": the anime releases' short names
SkipKind short_name(const std::u32string& w) {
  if (w.size() < 2 || w.size() > 4) return SkipKind::None;
  for (size_t i = 2; i < w.size(); ++i) if (w[i] < '0' || w[i] > '9') return SkipKind::None;
  if (w[0] == 'o' && w[1] == 'p') return SkipKind::Opening;
  if (w[0] == 'e' && w[1] == 'd') return SkipKind::Ending;
  return SkipKind::None;
}

}  // namespace

SkipKind chapter_skip_kind(std::string_view title) {
  std::u32string all = decode(title);
  for (auto& c : all) c = lower(c);
  // the words, each with where it starts (in code points)
  std::vector<std::u32string> words;
  std::vector<size_t> at;
  for (size_t i = 0; i < all.size();) {
    if (separator(all[i])) { ++i; continue; }
    size_t j = i;
    while (j < all.size() && !separator(all[j])) ++j;
    words.push_back(all.substr(i, j - i));
    at.push_back(i);
    i = j;
  }
  // the earliest mention wins ("Opening credits" is the opening, "Credits, then preview" the
  // ending); at one place the longer name
  size_t best_at = SIZE_MAX, best_len = 0;
  SkipKind best = SkipKind::None;
  auto offer = [&](size_t pos, size_t len, SkipKind k) {
    if (pos < best_at || (pos == best_at && len > best_len)) { best_at = pos; best_len = len; best = k; }
  };
  for (size_t i = 0; i < words.size(); ++i)
    if (SkipKind k = short_name(words[i]); k != SkipKind::None) offer(at[i], words[i].size(), k);
  for (const Key& key : kWords) {
    std::vector<std::u32string> parts;
    std::u32string phrase = decode(key.text);
    for (size_t i = 0; i <= phrase.size();) {
      size_t j = phrase.find(U' ', i);
      if (j == std::u32string::npos) j = phrase.size();
      parts.push_back(phrase.substr(i, j - i));
      i = j + 1;
    }
    for (size_t i = 0; i + parts.size() <= words.size(); ++i) {
      bool same = true;
      for (size_t k = 0; k < parts.size() && same; ++k) same = words[i + k] == parts[k];
      if (same) { offer(at[i], phrase.size(), key.kind); break; }
    }
  }
  for (const Key& key : kAnywhere) {
    std::u32string k = decode(key.text);
    size_t p = all.find(k);
    if (p != std::u32string::npos) offer(p, k.size(), key.kind);
  }
  return best;
}

std::vector<SkipPart> skip_parts_from_chapters(const std::vector<Chapter>& c, double dur) {
  std::vector<SkipPart> out;
  if (c.size() < 2) return out;  // one chapter covering the film is no part of it
  for (size_t i = 0; i < c.size(); ++i) {
    SkipKind k = chapter_skip_kind(c[i].title);
    if (k == SkipKind::None) continue;
    double end = i + 1 < c.size() ? c[i + 1].t : dur;
    if (!(end > c[i].t)) continue;  // the last chapter before the length is known
    if (!out.empty() && out.back().kind == k && std::abs(out.back().end - c[i].t) < 0.5) out.back().end = end;
    else out.push_back({k, c[i].t, end, false});
  }
  std::erase_if(out, [&](const SkipPart& p) {
    double len = p.end - p.start;
    return len < 5 || len > 360 || (dur > 0 && len > dur / 3);
  });
  return out;
}

std::vector<SkipPart> merge_skip_parts(std::vector<SkipPart> out, const std::vector<SkipPart>& sound) {
  for (const SkipPart& s : sound) {
    bool have = false, overlaps = false;
    for (const SkipPart& p : out) {
      have = have || p.kind == s.kind;
      overlaps = overlaps || (s.start < p.end && p.start < s.end);
    }
    if (!have && !overlaps) out.push_back(s);
  }
  std::sort(out.begin(), out.end(), [](const SkipPart& a, const SkipPart& b) { return a.start < b.start; });
  return out;
}

int skip_part_at(const std::vector<SkipPart>& parts, double t) {
  if (!std::isfinite(t)) return -1;
  for (int i = 0; i < (int)parts.size(); ++i)
    if (t >= parts[i].start - 0.05 && t < parts[i].end - 1.0) return i;
  return -1;
}

double skip_target(const std::vector<SkipPart>& parts, int i, double dur) {
  if (i < 0 || i >= (int)parts.size()) return -1;
  double end = parts[i].end;
  // the credits, then straight into the preview: one click past both
  if (parts[i].kind == SkipKind::Ending && i + 1 < (int)parts.size() && parts[i + 1].kind == SkipKind::Preview &&
      parts[i + 1].start - end < 1.0)
    end = parts[i + 1].end;
  if (dur > 0 && end > dur - 0.5) end = dur - 0.5;
  return end;
}
