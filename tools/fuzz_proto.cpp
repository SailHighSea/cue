// Mutation fuzzing of everything the phone (or anyone on the network) can send: HTTP requests,
// WebSocket frames, JSON commands, the auth header, and file names in listings.
#include "../src/core/remote_proto.h"
#include "../src/core/remote_auth.h"
#include "../src/core/json.h"
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>
static std::mt19937_64 rng;
static std::string mutate(std::string s) {
  int n = 1 + rng() % 8;
  for (int i = 0; i < n; ++i) {
    size_t at = s.empty() ? 0 : rng() % (s.size() + 1);
    switch (rng() % 7) {
      case 0: if (!s.empty() && at < s.size()) s[at] = (char)rng(); break;
      case 1: s.insert(at, 1, (char)rng()); break;
      case 2: if (at < s.size()) s.erase(at, 1 + rng() % 8); break;
      case 3: { static const char* t[] = {"%", "%0", "%zz", "%00", "\r\n", "\r\n\r\n", "\\", "\"", "\\u", "\\ud800", "{", "[", "]", "}", ":", ",", "..", "/../", "1e999", "-", "\xff", "\xc3", "\xe2\x80"}; s.insert(at, t[rng() % 23]); break; }
      case 4: if (!s.empty()) { size_t a = rng() % s.size(), l = 1 + rng() % 16; s.insert(at, s.substr(a, l)); } break;
      case 5: s.resize(rng() % (s.size() + 1)); break;
      case 6: s.insert(at, std::string(rng() % 3000, (char)('a' + rng() % 26))); break;
    }
  }
  return s;
}
static std::string frame(int op, const std::string& payload, bool mask) {
  std::string f; f += (char)(0x80 | op);
  size_t n = payload.size();
  if (n < 126) f += (char)((mask ? 0x80 : 0) | n);
  else if (n < 65536) { f += (char)((mask ? 0x80 : 0) | 126); f += (char)(n >> 8); f += (char)n; }
  else { f += (char)((mask ? 0x80 : 0) | 127); for (int i = 7; i >= 0; --i) f += (char)(n >> (i * 8)); }
  std::string k = "\x12\x34\x56\x78"; if (mask) f += k;
  for (size_t i = 0; i < n; ++i) f += mask ? (char)(payload[i] ^ k[i % 4]) : payload[i];
  return f;
}
int main(int argc, char** argv) {
  rng.seed(argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 1);
  long iters = argc > 2 ? std::atol(argv[2]) : 200000;
  std::vector<std::string> http = {
    "GET /ws HTTP/1.1\r\nHost: 192.168.0.10:4567\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n",
    "GET /list?dir=C%3A%5CVideos&x=1 HTTP/1.1\r\nHost: 10.0.0.2:4567\r\nAuthorization: Cue 0123456789abcdef:42:00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff\r\n\r\n",
    "GET /thumb?path=%2F..%2F..%2Fetc&size=200 HTTP/1.1\r\nHost: a\r\n\r\n",
    "GET / HTTP/1.1\r\nHost: cue.local\r\nUser-Agent: Mozilla/5.0 (iPhone; CPU iPhone OS 17_0 like Mac OS X)\r\n\r\n"};
  std::vector<std::string> json = {
    R"({"t":"auth","k":"0123456789abcdef","r":"00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff","i":"abcdefgh12"})",
    R"({"t":"auth","c":"123456","i":"abcdefgh"})",
    R"({"t":"cmd","a":"seek","v":12.5,"id":3})", R"({"t":"cmd","a":"open","path":"C:\\Videos\\a.mkv"})",
    R"({"t":"cmd","a":"queue-move","from":1,"to":-5})", R"({"a":[1,2,{"b":[[[[[]]]]]}],"c":"\u00e9\ud83c\udfb5"})"};
  std::vector<std::string> names = {"Show.S01E02.1080p.mkv", "Show - 1x02 - Title.mp4", "[Group] Anime - 12 [1080p].mkv", "song.flac", "a.b.c", "S99E999", "Ep 1000000000000000000000.mkv"};
  long ok = 0;
  for (long it = 0; it < iters; ++it) {
    { remote::Request r; size_t hl = 0; std::string in = mutate(http[rng() % http.size()]);
      if (remote::parse_request(in, r, hl) == 1) { ++ok; (void)remote::url_decode(r.target); (void)remote::valid_authority(r.headers["host"]); (void)remote::valid_ws_key(r.headers["sec-websocket-key"]); (void)remote::device_name(r.headers["user-agent"]);
        std::string sid, proof; uint64_t n = 0; (void)remote::parse_signed_header(r.headers["authorization"], sid, n, proof); } }
    { std::string p = mutate(json[rng() % json.size()]);
      std::string f = frame((int)(rng() % 16), p, rng() % 8 != 0);
      if (rng() % 3 == 0) f = mutate(f);
      remote::Frame fr; size_t used = 0; std::string_view rest(f);
      for (int k = 0; k < 4 && !rest.empty(); ++k) { int r = remote::ws_decode(rest, fr, used); if (r != 1) break; if (used == 0 || used > rest.size()) { std::printf("bad consumed\n"); return 1; } rest.remove_prefix(used); }
      JVal j; if (json_parse(p, j)) { (void)j["t"].str(); (void)j["k"].str(); (void)j["v"].num(); (void)j["v"].integer<int>(); (void)j["v"].integer<int64_t>(); }
      remote::Command c; (void)remote::parse_command(p, c); }
    { std::string sid, proof; uint64_t n = 0; (void)remote::parse_signed_header(mutate("Cue 0123456789abcdef:42:00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"), sid, n, proof); }
    { std::string a = mutate(names[rng() % names.size()]), b = mutate(names[rng() % names.size()]);
      int s = 0, e = 0; (void)remote::episode_of(a, s, e); (void)remote::natural_less(a, b); (void)remote::same_series(a, b); (void)remote::series_title(a); (void)remote::episode_label(a);
      std::string t, sub; remote::card_words(a, t, sub); (void)remote::series_id(a); (void)remote::path_within(a, b); (void)remote::escape(a); (void)remote::url_encode(a);
      std::vector<std::string> sib = {b, a, mutate(b)}; (void)remote::next_episode(a, sib); (void)remote::next_file(a, sib); (void)remote::next_in_series(a, sib); }
  }
  std::printf("proto fuzz: %ld iterations, %ld well-formed requests, clean\n", iters, ok);
}
