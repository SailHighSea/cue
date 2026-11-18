#include "../src/core/recents_view.h"
#include <cassert>
#include <iostream>
int main() {
  using namespace remote;
  assert(series_title("Twin.Peaks.S01E03.1080p.mkv") == "Twin Peaks");
  assert(series_title("[Group] RuPaul's Drag Race - S18E15.mkv") == "RuPaul's Drag Race");
  assert(series_title("Mother Mary.mkv").empty());
  assert(episode_label("Twin Peaks - 1x02 - Title.mkv") == "S01E02");
  assert(episode_label("Show ep 7.mp4") == "E07");
  std::vector<RecentFile> files = {
      {"C:\\v\\RuPaul's Drag Race - S18E15.mkv", 1200, 2078},
      {"C:\\m\\Mother Mary.mkv", 4052, 6746},
      {"C:\\v\\RuPaul's Drag Race - S18E16.mkv", 0, 3562},
      {"C:\\t\\Twin Peaks - S01E03.mkv", 1289, 2900},
      {"C:\\t\\Twin Peaks - S01E01.mkv", 0, 5642},
      {"C:\\a\\song.flac", 30, 200},
      {"C:\\v\\RuPaul's Drag Race - S18E14.mkv", 0, 3000},
  };
  auto live = live_recents(files, 6);
  assert(live.size() == 4);
  assert(live[0].title == "RuPaul's Drag Race" && live[0].episode == "S18E15" && live[0].episodes == 3);
  assert(live[0].path == files[0].path && live[0].pos == 1200);  // the latest episode opens
  assert(live[1].title == "Mother Mary" && live[1].episode.empty() && live[1].episodes == 1);
  assert(live[2].title == "Twin Peaks" && live[2].episodes == 2);
  assert(live[3].title == "song" && live[3].audio);
  // the cap counts entries, and episodes of shown entries keep counting past it
  auto two = live_recents(files, 2);
  assert(two.size() == 2 && two[0].episodes == 3);
  int h = 0, m = 0;
  split_minutes(24 * 60 + 10, h, m); assert(h == 0 && m == 25);
  split_minutes(3900, h, m); assert(h == 1 && m == 5);
  split_minutes(5, h, m); assert(h == 0 && m == 1);
  // what each entry is: films, episodes, songs, long listens, books
  assert(live[0].kind == RecentKind::Episode && live[1].kind == RecentKind::Video && live[3].kind == RecentKind::Music);
  assert(recent_kind("talk.mp3", true, false, 45 * 60, 0) == RecentKind::Listen);
  assert(recent_kind("song.mp3", true, false, 4 * 60, 0) == RecentKind::Music);
  assert(recent_kind("song.mp3", true, false, 0, 0) == RecentKind::Music);  // length unknown: a song
  assert(recent_kind("Hobbit.M4B", true, false, 0, 0) == RecentKind::Book);
  assert(recent_kind("book.mp3", true, false, 6 * 3600, 19) == RecentKind::Book);
  assert(recent_kind("mix.mp3", true, false, 3600, 2) == RecentKind::Listen);  // two chapters: not a book
  assert(is_audio_name("x.m4b") && is_book_name("x.aax") && !is_book_name("x.m4a"));
  assert(!resumes_audio(19 * 60) && resumes_audio(20 * 60));
  // titles as the home shows them (1.4): technical brackets out, a song's artist out of the
  // front; anything that might be part of the title stays
  assert(clean_title("Jane Remover - Music Baby (audio)", "Jane Remover") == "Music Baby");
  assert(clean_title("Hunnygloss - Crimson - (320 Kbps)", "Hunnygloss") == "Crimson");
  assert(clean_title("Number Teddie - DNA", "Number Teddie") == "DNA");
  assert(clean_title("ABBA \xe2\x80\x93 Dancing Queen", "abba") == "Dancing Queen");
  assert(clean_title("Song (Official Audio)", "") == "Song" && clean_title("Clip [4K HDR]", "") == "Clip");
  assert(clean_title("Mother Mary [1080p x265 10bit]", "") == "Mother Mary");
  assert(clean_title("Beyonc\xc3\xa9 - Halo (HQ)", "Beyonc\xc3\xa9") == "Halo");
  assert(clean_title("Song (Live at Wembley)", "") == "Song (Live at Wembley)");
  assert(clean_title("Song (2019 Remaster)", "") == "Song (2019 Remaster)");
  assert(clean_title("Song (feat. Someone)", "") == "Song (feat. Someone)");
  assert(clean_title("Song (Official Video)", "") == "Song (Official Video)");
  assert(clean_title("Film (2019)", "") == "Film (2019)" && clean_title("Mix (128)", "") == "Mix (128)");
  assert(clean_title("Abbaesque - Thing", "ABBA") == "Abbaesque - Thing");   // the artist's name, not a word starting with it
  assert(clean_title("Daft Punk - Daft Punk", "Daft Punk") == "Daft Punk");  // (the song's own name)
  assert(clean_title("Help -", "") == "Help -" && clean_title("Mr.", "") == "Mr.");  // nothing taken: its own ending stays
  assert(clean_title("(audio)", "") == "(audio)" && clean_title("Track 01", "Track") == "Track 01");  // never empty
  assert(clean_title("Prince - 1999 (Remix)", "") == "Prince - 1999 (Remix)");  // no artist known: as it is
  // carried through: when, who, the chapters
  {
    std::vector<RecentFile> f2 = {{"C:\\b\\Hobbit.m4b", 900, 20000, 1700000000, "Tolkien", 19, 3}};
    auto l2 = live_recents(f2, 6);
    assert(l2.size() == 1 && l2[0].kind == RecentKind::Book && l2[0].artist == "Tolkien" && l2[0].chapters == 19 && l2[0].chapter == 3 && l2[0].last == 1700000000);
    assert(l2[0].title == "Hobbit");
  }
  // how long ago, the way a person says it
  assert(ago_from_days(0).unit == Ago::Today && ago_from_days(1).unit == Ago::Yesterday);
  assert(ago_from_days(3).unit == Ago::Days && ago_from_days(3).n == 3);
  assert(ago_from_days(7).unit == Ago::Weeks && ago_from_days(7).n == 1 && ago_from_days(29).n == 4);
  assert(ago_from_days(30).unit == Ago::Months && ago_from_days(30).n == 1 && ago_from_days(364).n == 11);
  assert(ago_from_days(365).unit == Ago::Years && ago_from_days(800).n == 2);
  assert(ago_from_days(-2).unit == Ago::Unknown && ago(0, 1700000000).unit == Ago::Unknown);
  // calendar days, not 24-hour spans: same moment is today, a day later is yesterday
  assert(ago(1700000000, 1700000000).unit == Ago::Today && ago(1700000000, 1700000000 + 86400).unit == Ago::Yesterday);
  assert(local_day_number(86400 * 365) - local_day_number(0) == 365);
  // the picture's file: stable, private, the same for the same normalized path
  assert(thumb_file_name("c:\\v\\film.mkv") == thumb_file_name("c:\\v\\film.mkv"));
  assert(thumb_file_name("c:\\v\\film.mkv") != thumb_file_name("c:\\v\\film2.mkv"));
  assert(thumb_file_name("x").size() == 20 && thumb_file_name("x").substr(16) == ".jpg");
  assert(thumb_file_name("") == "cbf29ce484222325.jpg");  // FNV-1a 64 of nothing
  // the home's picture of a film: where it was left, else a tenth in, never past the middle
  assert(home_shot_time(120, 3600) == 120);
  assert(home_shot_time(0, 3600) == 300);   // (5 min at most)
  assert(home_shot_time(0, 600) == 60);
  assert(home_shot_time(0, 30) == 5);
  assert(home_shot_time(0, 3) == 1.5);      // a 3 s clip: inside it (it was asked at 5 s, past its end)
  assert(home_shot_time(0, 0) == 30);       // unknown length
  assert(home_shot_time(9, 5) == 2.5);      // a place past a length that changed: inside it
  for (double d = 0.1; d < 7200; d *= 1.37) assert(home_shot_time(0, d) < d);
  std::cout << "Recents view: the home's picture of a film inside it (where it was left, a tenth in, never past the middle) passed\n";
  std::cout << "Recents view: shows grouped under their latest episode, titles without extensions, audio, caps and spans passed\n";
  std::cout << "Recents view: kinds (film, episode, song, long listen, book), how long ago, picture names, titles without technical brackets or the artist in front passed\n";
}
