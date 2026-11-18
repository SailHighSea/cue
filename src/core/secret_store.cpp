#include "secret_store.h"

#include <cstdint>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#include <dpapi.h>
#endif

#include "remote_proto.h"  // base64

namespace secret {
namespace {

constexpr std::string_view kDpapi = "dpapi:";
[[maybe_unused]] constexpr std::string_view kPlain = "plain:";

bool unbase64(std::string_view in, std::string& out) {
  auto val = [](char c) -> int {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
  };
  if (in.size() % 4) return false;
  out.clear();
  for (size_t i = 0; i < in.size(); i += 4) {
    int v[4];
    for (int k = 0; k < 4; ++k) {
      const char c = in[i + k];
      if (c == '=') {
        // padding only at the very end, and at most two
        if (i + 4 != in.size() || k < 2 || (k == 2 && in[i + 3] != '=')) return false;
        v[k] = -2;
      } else if ((v[k] = val(c)) < 0) return false;
    }
    const uint32_t n = (uint32_t)v[0] << 18 | (uint32_t)v[1] << 12 | (uint32_t)(v[2] < 0 ? 0 : v[2]) << 6 | (uint32_t)(v[3] < 0 ? 0 : v[3]);
    out += (char)(n >> 16);
    if (v[2] >= 0) out += (char)((n >> 8) & 255);
    if (v[3] >= 0) out += (char)(n & 255);
  }
  return true;
}

#ifdef _WIN32
// a label of our own: another program's DPAPI blob is not read back as one of Cue's
const BYTE kEntropy[] = "tinypeep Cue phone remote credential v1";
#endif

}  // namespace

std::string seal(const std::string& plain) {
  if (plain.empty() || plain.size() > 1024) return {};
#ifdef _WIN32
  DATA_BLOB in{(DWORD)plain.size(), (BYTE*)plain.data()}, entropy{(DWORD)sizeof kEntropy - 1, (BYTE*)kEntropy}, out{};
  if (!CryptProtectData(&in, L"Cue", &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) return {};
  std::string blob(reinterpret_cast<const char*>(out.pbData), out.cbData);
  LocalFree(out.pbData);
  return std::string(kDpapi) + remote::base64(blob);
#else
  return std::string(kPlain) + plain;
#endif
}

bool unseal(const std::string& sealed, std::string& plain) {
  plain.clear();
  if (sealed.size() > 8192) return false;
  const std::string_view s(sealed);
  if (s.substr(0, kDpapi.size()) == kDpapi) {
#ifdef _WIN32
    std::string blob;
    if (!unbase64(s.substr(kDpapi.size()), blob) || blob.empty()) return false;
    DATA_BLOB in{(DWORD)blob.size(), (BYTE*)blob.data()}, entropy{(DWORD)sizeof kEntropy - 1, (BYTE*)kEntropy}, out{};
    if (!CryptUnprotectData(&in, nullptr, &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) return false;
    plain.assign(reinterpret_cast<const char*>(out.pbData), out.cbData);
    SecureZeroMemory(out.pbData, out.cbData);
    LocalFree(out.pbData);
    return !plain.empty();
#else
    std::string blob;
    (void)unbase64(s.substr(kDpapi.size()), blob);
    return false;  // only Windows can read DPAPI
#endif
  }
#ifndef _WIN32
  if (s.substr(0, kPlain.size()) == kPlain) {
    plain.assign(s.substr(kPlain.size()));
    return !plain.empty();
  }
#endif
  return false;
}

}  // namespace secret
