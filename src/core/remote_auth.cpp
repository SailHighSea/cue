#include "remote_auth.h"

#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#endif

#include "remote_proto.h"  // hex

namespace remote {

std::string sha256_portable(std::string_view data) {
  static const uint32_t k[64] = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
      0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
      0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
      0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
      0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
      0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
  uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  std::string m(data);
  const uint64_t bits = (uint64_t)data.size() * 8;
  m += (char)0x80;
  while (m.size() % 64 != 56) m += (char)0;
  for (int i = 7; i >= 0; --i) m += (char)((bits >> (i * 8)) & 0xFF);
  auto rotr = [](uint32_t v, int n) { return (v >> n) | (v << (32 - n)); };
  for (size_t off = 0; off < m.size(); off += 64) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i)
      w[i] = (uint32_t)(unsigned char)m[off + i * 4] << 24 | (uint32_t)(unsigned char)m[off + i * 4 + 1] << 16 |
             (uint32_t)(unsigned char)m[off + i * 4 + 2] << 8 | (uint32_t)(unsigned char)m[off + i * 4 + 3];
    for (int i = 16; i < 64; ++i) {
      const uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      const uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; ++i) {
      const uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
      const uint32_t ch = (e & f) ^ (~e & g);
      const uint32_t t1 = hh + S1 + ch + k[i] + w[i];
      const uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
      const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
      const uint32_t t2 = S0 + maj;
      hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
  }
  std::string out;
  out.reserve(32);
  for (uint32_t v : h) for (int i = 3; i >= 0; --i) out += (char)((v >> (i * 8)) & 0xFF);
  return out;
}

std::string hmac_sha256_portable(std::string_view key, std::string_view message) {
  // RFC 2104 with a 64-byte block: a longer key is hashed first, a shorter one padded with zeros
  std::string block(64, '\0');
  if (key.size() > 64) {
    const std::string kh = sha256_portable(key);
    std::memcpy(block.data(), kh.data(), kh.size());
  } else if (!key.empty()) {
    std::memcpy(block.data(), key.data(), key.size());
  }
  std::string inner(block), outer(block);
  for (auto& c : inner) c = (char)(c ^ 0x36);
  for (auto& c : outer) c = (char)(c ^ 0x5c);
  inner.append(message.data(), message.size());
  outer += sha256_portable(inner);
  return sha256_portable(outer);
}

#ifdef _WIN32
// The system's SHA-256 and HMAC (CNG). Empty on a failure, which every caller treats as "no
// match": a check can only fail closed.
static std::string cng_hash(std::string_view key, std::string_view data, bool hmac) {
  BCRYPT_ALG_HANDLE alg = nullptr;
  BCRYPT_HASH_HANDLE h = nullptr;
  std::string out;
  if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, hmac ? BCRYPT_ALG_HANDLE_HMAC_FLAG : 0) >= 0) {
    if (BCryptCreateHash(alg, &h, nullptr, 0, hmac ? (PUCHAR)key.data() : nullptr, hmac ? (ULONG)key.size() : 0, 0) >= 0) {
      unsigned char d[32];
      if (BCryptHashData(h, (PUCHAR)data.data(), (ULONG)data.size(), 0) >= 0 && BCryptFinishHash(h, d, 32, 0) >= 0)
        out.assign(reinterpret_cast<const char*>(d), 32);
      BCryptDestroyHash(h);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
  }
  return out;
}
std::string sha256(std::string_view data) { return cng_hash({}, data, false); }
std::string hmac_sha256(std::string_view key, std::string_view message) { return cng_hash(key, message, true); }
#else
std::string sha256(std::string_view data) { return sha256_portable(data); }
std::string hmac_sha256(std::string_view key, std::string_view message) { return hmac_sha256_portable(key, message); }
#endif

bool same_secret(std::string_view a, std::string_view b) {
  if (a.size() != b.size() || a.empty()) return false;
  unsigned diff = 0;
  for (size_t i = 0; i < a.size(); ++i) diff |= (unsigned char)a[i] ^ (unsigned char)b[i];
  return diff == 0;
}

bool valid_hex(std::string_view s, size_t size) {
  if (s.size() != size) return false;
  for (char c : s) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  return true;
}

std::string credential_kid(std::string_view credential) {
  const std::string h = sha256(credential);
  return h.size() == 32 ? hex(h).substr(0, kKidHex) : std::string();
}

static std::string mac_hex(std::string_view key, std::string_view message) {
  const std::string m = hmac_sha256(key, message);
  return m.size() == 32 ? hex(m) : std::string();
}

std::string auth_proof(std::string_view credential, std::string_view challenge) {
  return mac_hex(credential, "cue-auth\n" + std::string(challenge));
}

std::string session_key(std::string_view credential, std::string_view challenge) {
  return mac_hex(credential, "cue-session\n" + std::string(challenge));
}

std::string request_proof(std::string_view session, std::string_view sid, std::string_view method,
                          std::string_view target, uint64_t n) {
  std::string m;
  m.reserve(sid.size() + method.size() + target.size() + 24);
  m.append(sid).append("\n").append(method).append("\n").append(target).append("\n").append(std::to_string(n));
  return mac_hex(session, m);
}

bool parse_signed_header(std::string_view a, std::string& sid, uint64_t& n, std::string& proof) {
  if (a.substr(0, 4) != "Cue ") return false;
  a.remove_prefix(4);
  const size_t p1 = a.find(':');
  if (p1 == std::string_view::npos) return false;
  const size_t p2 = a.find(':', p1 + 1);
  if (p2 == std::string_view::npos) return false;
  const std::string_view s = a.substr(0, p1), num = a.substr(p1 + 1, p2 - p1 - 1), pr = a.substr(p2 + 1);
  if (!valid_hex(s, kSidHex) || !valid_hex(pr, kProofHex) || num.empty() || num.size() > 15 || num[0] == '0') return false;
  uint64_t v = 0;
  for (char c : num) {
    if (c < '0' || c > '9') return false;
    v = v * 10 + (uint64_t)(c - '0');
  }
  sid.assign(s);
  n = v;
  proof.assign(pr);
  return true;
}

bool ReplayWindow::accept(uint64_t n) {
  if (n == 0) return false;
  if (n + kWindow <= newest_) return false;  // too old: the page has long moved on
  if (!used_.insert(n).second) return false;  // seen already: a replay
  if (n > newest_) newest_ = n;
  // only numbers still inside the window are worth remembering
  while (!used_.empty() && *used_.begin() + kWindow <= newest_) used_.erase(used_.begin());
  return true;
}

}  // namespace remote
