#pragma once
// Out of memory inside one message (a 4K picture, a big cover, a long list on a PC with little
// memory left): that message is given up and Cue carries on, instead of the whole process ending
// through std::terminate with nothing in cue.log. Only std::bad_alloc: any other exception is a
// bug and still ends Cue the usual way (with the crash line, see main.cpp).
#include <new>
#include <utility>

namespace oom_guard {

// f() as usual; when it runs out of memory: note(), then fallback()
template <class F, class Note, class Fallback>
auto run(F&& f, Note&& note, Fallback&& fallback) -> decltype(f()) {
  try {
    return std::forward<F>(f)();
  } catch (const std::bad_alloc&) {
    try { note(); } catch (...) {}  // (the note itself may not find memory: it is only a note)
    return std::forward<Fallback>(fallback)();
  }
}

// how often a note is worth writing: the first, then at most one every `every` seconds
struct Throttle {
  double last = -1e9;
  int skipped = 0;
  bool due(double now, double every = 5.0) {
    if (now - last < every) { ++skipped; return false; }
    last = now;
    return true;
  }
};

}  // namespace oom_guard
