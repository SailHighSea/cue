// 18a: the covers read lately are found by path alone (no disk on the asking thread); the identity
// comes from the read, and is checked again on a worker at most every 10 s; a changed one drops it.
#include "../src/core/cover_cache.h"
#include <cassert>
#include <cstdio>

struct Job { bool cancelled = false; };

int main() {
  CoverCache<Job> c;
  auto usable = [](const Job& j) { return !j.cancelled; };
  bool check = true;
  // nothing yet
  assert(!c.find("E:\\a.mp3", 0, check, usable) && !check);
  auto a = std::make_shared<Job>();
  c.add("E:\\a.mp3", a, 0);
  // the requests of one open: the same job, nothing asked of the disk (not even before the read answers)
  for (double t = 0; t < 9.9; t += 0.5) { assert(c.find("E:\\a.mp3", t, check, usable) == a && !check); }
  c.report(a.get(), "E:\\a.mp3|100|7", 1.0);
  assert(c.find("E:\\a.mp3", 5.0, check, usable) == a && !check);
  // 10 s after the last check: answered at once, one check asked (only one while it runs)
  assert(c.find("E:\\a.mp3", 11.0, check, usable) == a && check);
  assert(c.find("E:\\a.mp3", 12.0, check, usable) == a && !check);
  // the same identity: kept, the next check 10 s later
  assert(c.report(a.get(), "E:\\a.mp3|100|7", 12.5));
  assert(c.find("E:\\a.mp3", 20.0, check, usable) == a && !check);
  assert(c.find("E:\\a.mp3", 23.0, check, usable) == a && check);
  // re-tagged: dropped; the next request reads the song again
  assert(!c.report(a.get(), "E:\\a.mp3|120|9", 23.5));
  assert(!c.find("E:\\a.mp3", 24.0, check, usable));
  // a check that never ran lets another be asked
  auto b = std::make_shared<Job>();
  c.add("E:\\b.mp3", b, 30);
  c.report(b.get(), "", 30);  // (no file: an identity too)
  assert(c.find("E:\\b.mp3", 41, check, usable) == b && check);
  c.unchecked(b.get());
  assert(c.find("E:\\b.mp3", 41.5, check, usable) == b && check);
  // the file appears: dropped
  assert(!c.report(b.get(), "E:\\b.mp3|5|1", 42));
  // a job given up is not found; four kept, the oldest go
  auto d = std::make_shared<Job>();
  d->cancelled = true;
  c.add("E:\\d.mp3", d, 50);
  assert(!c.find("E:\\d.mp3", 50, check, usable));
  for (int i = 0; i < 6; ++i) c.add("E:\\" + std::to_string(i), std::make_shared<Job>(), 60);
  assert(c.size() == CoverCache<Job>::kKept);
  assert(!c.find("E:\\0", 60, check, usable) && c.find("E:\\5", 60, check, usable));
  // a report for a job no longer kept changes nothing
  assert(c.report(a.get(), "x", 70) && c.size() == CoverCache<Job>::kKept);
  std::puts("cover cache: found by path, identity from the read, checked on a worker every 10 s, a re-tagged song read again passed");
}
