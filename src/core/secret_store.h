// A secret kept in state.json without keeping it in the clear: the phone remote's credentials.
//
// Windows: DPAPI (CryptProtectData, for the current user, no prompt), written as "dpapi:" and
// base64. Only this Windows account on this PC can read it back; a state.json copied elsewhere
// (a backup, another account) holds nothing usable, and its phones simply pair again.
// Elsewhere (the tests, a future port without a keychain yet): "plain:" and the secret as it
// is. That is a marker, not protection, and Windows never accepts it.
#pragma once
#include <string>

namespace secret {
std::string seal(const std::string& plain);                        // "" when it cannot be sealed
bool unseal(const std::string& sealed, std::string& plain);       // false: not ours, or unreadable
}  // namespace secret
