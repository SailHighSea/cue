// write_whole_file (18f): a generated file is written whole or not at all. A write that cannot be
// done leaves the file that was there as it was (the old way truncated it first: a full disk left
// the engine an empty shader and an empty key list).
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "../src/core/file_io.h"

namespace fs = std::filesystem;
static std::string read(const fs::path& p) { std::ifstream f(p, std::ios::binary); return {std::istreambuf_iterator<char>(f), {}}; }

int main() {
  const fs::path dir = fs::temp_directory_path() / "cue-write-whole-test";
  fs::remove_all(dir);
  fs::create_directories(dir);
  const fs::path file = dir / "blur-cloud-1.0.hook";
  // new, then replaced: whole, and nothing left beside it
  assert(write_whole_file(file, "//!HOOK MAIN\nfirst\n"));
  assert(read(file) == "//!HOOK MAIN\nfirst\n");
  assert(write_whole_file(file, "second, longer than the first one\n"));
  assert(read(file) == "second, longer than the first one\n");
  assert(!fs::exists(dir / "blur-cloud-1.0.hook.tmp"));
  // shorter: no tail of the old one
  assert(write_whole_file(file, "3"));
  assert(read(file) == "3");
  // the write cannot happen (its scratch place is taken): false, and the file there is untouched
  fs::create_directories(dir / "input.conf.tmp");
  const fs::path conf = dir / "input.conf";
  { std::ofstream f(conf, std::ios::binary); f << "SPACE script-binding main/space\n"; }
  assert(!write_whole_file(conf, "a newer list\n"));
  assert(read(conf) == "SPACE script-binding main/space\n");
  // a folder where the file should be: false, nothing broken
  fs::create_directories(dir / "taken");
  assert(!write_whole_file(dir / "taken", "x"));
  assert(fs::is_directory(dir / "taken"));
#ifdef __linux__
  // a disk that is full: /dev/full fails every write; its scratch file is in /dev, so this only
  // checks that a failed write is seen (a stream that could not write says so)
  { std::ofstream f("/dev/full", std::ios::binary); f << std::string(1 << 16, 'x'); f.flush(); assert(!f); }
#endif
  fs::remove_all(dir);
  std::puts("write_whole_file: whole or not at all, the old file kept when the write fails, nothing left beside it passed");
}
