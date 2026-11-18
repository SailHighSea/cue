// "skip intro" on the phone, the protocol's half: the state carries the part offered and where
// its skip lands (nothing when none is offered), and "skip" is a command the page may send.
#include "../src/core/remote_proto.h"
#include <cassert>
#include <cstdio>
int main() {
  remote::State s; s.playing_view = true;
  assert(remote::state_json(s).find("\"skip\"") == std::string::npos);
  s.skip = "opening"; s.skip_to = 92.5;
  const std::string j = remote::state_json(s);
  assert(j.find("\"skip\":{\"kind\":\"opening\",\"to\":92.5}") != std::string::npos);
  remote::Command c;
  assert(remote::parse_command("{\"c\":\"skip-part\"}", c) && c.c == "skip-part");
  // "skip" stays the relative seek: it needs its seconds (it once took the part's place and jumped by 0)
  assert(!remote::parse_command("{\"c\":\"skip\"}", c));
  assert(!remote::parse_command("{\"c\":\"skipp\"}", c));
  std::puts("remote_skip_test: ok");
}
