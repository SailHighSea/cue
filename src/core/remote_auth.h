// How a phone proves it is one of ours, without its secret ever crossing the network again.
//
// Pairing (the six digits on the PC's screen) hands the phone a credential: 32 random bytes,
// written as 64 lowercase hex characters. That is the only time it travels. From then on:
//
//   1. On every connection the PC sends a fresh challenge (16 random bytes, as hex).
//   2. The phone answers with the credential's name (kid: the first 16 hex characters of its
//      SHA-256) and   proof = HMAC-SHA256(credential, "cue-auth\n" + challenge).
//   3. Both sides then derive the session's key, which is never sent:
//      session = HMAC-SHA256(credential, "cue-session\n" + challenge).
//   4. Each HTTP request of that page (folders, thumbnails, covers, the queue) is signed:
//      Authorization: Cue <sid>:<n>:<HMAC-SHA256(session, sid\nGET\n<target>\nn)>, where target
//      is the request's path and query exactly as sent, and n counts up. A number already used,
//      or far behind the newest one, is refused: a request seen on the network cannot be sent
//      again, and the session dies with its connection.
//
// HMAC keys are the 64-character hex strings themselves (their ASCII bytes). Every digest is
// compared in constant time. On Windows SHA-256 and HMAC come from the system (BCrypt); the
// portable code below is what the tests (and other platforms) use, checked against the
// published test vectors. The phone's page (assets/remote/index.html) computes the same values
// in JavaScript, because a page served over http:// has no crypto.subtle.
//
// What this does not do: the link is not encrypted. Someone who can watch the network at the
// moment of pairing sees the credential, and someone who can change traffic can still act on an
// open connection. See docs/SECURITY.md.
#pragma once
#include <cstdint>
#include <set>
#include <string>
#include <string_view>

namespace remote {

std::string sha256(std::string_view data);                                // 32 raw bytes
std::string hmac_sha256(std::string_view key, std::string_view message);  // 32 raw bytes
// The portable implementations, whatever the platform (tests compare the system's with them).
std::string sha256_portable(std::string_view data);
std::string hmac_sha256_portable(std::string_view key, std::string_view message);

// Same length and every byte looked at: the time taken says nothing about where they differ.
bool same_secret(std::string_view a, std::string_view b);

constexpr size_t kChallengeHex = 32;  // 16 random bytes
constexpr size_t kKidHex = 16;
constexpr size_t kSidHex = 16;        // 8 random bytes
constexpr size_t kProofHex = 64;

bool valid_hex(std::string_view s, size_t size);  // exactly `size` lowercase hex characters
std::string credential_kid(std::string_view credential);
std::string auth_proof(std::string_view credential, std::string_view challenge);   // hex
std::string session_key(std::string_view credential, std::string_view challenge);  // hex
std::string request_proof(std::string_view session, std::string_view sid, std::string_view method,
                          std::string_view target, uint64_t n);                     // hex

// "Cue <sid>:<n>:<proof>" -> its parts. False when it is not that shape.
bool parse_signed_header(std::string_view header, std::string& sid, uint64_t& n, std::string& proof);

// One session's request numbers: each is accepted once, and only within a window of the newest
// (pages ask for a dozen thumbnails at once; they may arrive in any order).
class ReplayWindow {
 public:
  static constexpr uint64_t kWindow = 128;
  bool accept(uint64_t n);
 private:
  uint64_t newest_ = 0;
  std::set<uint64_t> used_;
};

}  // namespace remote
