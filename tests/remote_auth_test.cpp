// The phone's proof and signatures: SHA-256 and HMAC-SHA256 against the published vectors and
// 200 digests from Python's hashlib, the system implementation (Windows: CNG) against the
// portable one, the protocol's values against values computed independently in Python, and
// the replay window. Built for Windows too (tools/native-tests), where sha256()/hmac_sha256()
// are the system's.
#include "../src/core/remote_auth.h"
#include "../src/core/remote_proto.h"
#include <cassert>
#include <cstdio>
#include <string>

#include "remote_auth_vectors.inc"

using namespace remote;

static std::string unhex(const char* h) {
  std::string out;
  for (size_t i = 0; h[i] && h[i + 1]; i += 2) out += (char)std::stoi(std::string(h + i, 2), nullptr, 16);
  return out;
}

int main() {
  // FIPS 180-2 examples
  assert(hex(sha256("")) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  assert(hex(sha256("abc")) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  assert(hex(sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")) ==
         "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
  assert(hex(sha256(std::string(1000000, 'a'))) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
  // RFC 4231, test cases 1-4, 6 and 7 (5 is a truncated output)
  struct { std::string key, data; const char* mac; } rfc[] = {
      {std::string(20, '\x0b'), "Hi There", "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"},
      {"Jefe", "what do ya want for nothing?", "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843"},
      {std::string(20, '\xaa'), std::string(50, '\xdd'), "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe"},
      {unhex("0102030405060708090a0b0c0d0e0f10111213141516171819"), std::string(50, '\xcd'),
       "82558a389a443c0ea4cc819899f2083a85f0faa3e578f8077a2e3ff46729665b"},
      {std::string(131, '\xaa'), "Test Using Larger Than Block-Size Key - Hash Key First",
       "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54"},
      {std::string(131, '\xaa'),
       "This is a test using a larger than block-size key and a larger than block-size data. The key needs to be hashed before being used by the HMAC algorithm.",
       "9b09ffa71b942fcb27635fbcd5b0e944bfdc63644f0713938a7f51535c3a35e2"},
  };
  for (const auto& v : rfc) {
    assert(hex(hmac_sha256(v.key, v.data)) == v.mac);
    assert(hex(hmac_sha256_portable(v.key, v.data)) == v.mac);
  }
  // random lengths across every padding boundary, keys shorter, equal to and longer than a block
  size_t n = 0;
  for (const auto& v : kAuthVectors) {
    const std::string_view key(v.key, v.key_size), msg(v.msg, v.msg_size);
    assert(hex(sha256_portable(msg)) == v.sha && hex(sha256(msg)) == v.sha);
    assert(hex(hmac_sha256_portable(key, msg)) == v.mac);
    if (!key.empty()) assert(hex(hmac_sha256(key, msg)) == v.mac);  // (CNG wants a key)
    ++n;
  }

  // the protocol, with values computed separately (Python's hmac)
  std::string c;
  for (int i = 0; i < 32; ++i) c += "3f";
  const std::string chal = "0123456789abcdef0123456789abcdef";
  assert(credential_kid(c) == "67ef9de3b81d374f");
  assert(auth_proof(c, chal) == "6737f15c0e648bd948bf61504e2895e7001a75695af3defae16b47977cca62b7");
  const std::string sk = session_key(c, chal);
  assert(sk == "e7f1ca3b7235ae06e82088b507a11caed0d10ba00beb5d00743e7789a5abe6b8");
  assert(request_proof(sk, "a1b2c3d4e5f60718", "GET", "/thumb?p=C%3A%5CV%C3%ADdeos%5COcean%27s.mkv", 7) ==
         "4b18a60abba074bf1646eead44a4fd974d3a6f35528d960ac62a730fe96831fb");
  // proof and session key are different values of the same secret (separate labels)
  assert(auth_proof(c, chal) != sk && auth_proof(c, chal).size() == kProofHex);
  assert(valid_hex(sk, 64) && !valid_hex(sk, 63) && !valid_hex("ABCDEF0123456789", 16) && valid_hex("abcdef0123456789", 16));

  // constant-time comparison: equal only when every byte is equal, never for an empty secret
  assert(same_secret("abc", "abc") && !same_secret("abc", "abd") && !same_secret("abc", "ab") && !same_secret("", ""));

  // the signed header
  std::string sid, proof;
  uint64_t num = 0;
  const std::string good = "Cue a1b2c3d4e5f60718:42:" + sk;
  assert(parse_signed_header(good, sid, num, proof) && sid == "a1b2c3d4e5f60718" && num == 42 && proof == sk);
  for (std::string bad : {std::string("Bearer ") + sk, "Cue a1b2c3d4e5f6071:42:" + sk, "Cue a1b2c3d4e5f60718:042:" + sk,
                          "Cue a1b2c3d4e5f60718::" + sk, "Cue a1b2c3d4e5f60718:4x:" + sk, "Cue a1b2c3d4e5f60718:42:" + sk.substr(1),
                          "Cue A1B2C3D4E5F60718:42:" + sk, "Cue a1b2c3d4e5f60718:1234567890123456:" + sk, std::string("Cue ")})
    assert(!parse_signed_header(bad, sid, num, proof));

  // replay window: each number once; late arrivals inside the window are fine; old ones are not
  ReplayWindow w;
  assert(!w.accept(0));
  assert(w.accept(1) && !w.accept(1));
  assert(w.accept(3) && w.accept(2) && !w.accept(2) && !w.accept(3));
  assert(w.accept(500));  // a page that let many requests go unsent
  assert(!w.accept(500 - ReplayWindow::kWindow));
  assert(w.accept(500 - ReplayWindow::kWindow + 1) && !w.accept(500 - ReplayWindow::kWindow + 1));
  for (uint64_t i = 501; i < 5000; ++i) assert(w.accept(i));
  for (uint64_t i = 4990; i < 5000; ++i) assert(!w.accept(i));

  std::printf("Remote auth: SHA-256 and HMAC (FIPS, RFC 4231, %zu Python digests, system = portable), proof, session key, "
              "request signature, header parsing, constant-time compare and replay window passed\n", n);
}
