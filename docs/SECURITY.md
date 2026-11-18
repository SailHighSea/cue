# Security review: 1.1.1, phone remote and updater revised in 1.4

Scope: first-party application code in Cue, especially the LAN remote, file boundary, phone page and updater. This is a targeted review and hardening change, not an independent penetration test or a review of all vendored media libraries.

## Threat model and changes

| Boundary | Previous behavior | 1.1.1 |
|---|---|---|
| Browser origins | Literal Host only; no Origin check | Host must be the socket’s actual local IPv4 endpoint; WebSocket Origin must match it exactly; cross-site HTTP fetches refused |
| Pairing | Four digits; accepted while the screen was open | Six digits, two-minute lifetime, one use, automatic refresh while waiting; existing six-failure/minute lock retained |
| Credentials | RNG failure fell back to predictable state; tokens in URLs | BCrypt only, failure refuses credentials; 256-bit tokens, first WebSocket message / HTTP Bearer header |
| Revocation | Token checked at connection; off retained phones | Tokens rechecked on connections/commands; turning off clears remembered phones; old credentials rejected |
| Files | Case-insensitive string prefix; only exact dot segments rejected | Normal drive paths only; reject NUL, ADS, device names, trailing dots/spaces, UNC/device namespace, traversal and descendant reparse points; compare resolved handles |
| Untrusted input | Permissive JSON, open command names and numeric values | Bounded payloads, complete strict JSON, duplicate-key rejection, command allowlist and numeric ranges; WebSocket opcode/control checks |
| Resource use | Unbounded UI command queue; up to 48 file threads | 128 queued commands, 256 total events and authentication backpressure at 192 events; 20 frames/s with burst 40 per socket; 512 KiB outgoing cap; 5s unauthenticated timeout; six file workers, three thumbnail jobs |
| Browser content | No CSP / frame restriction | Fresh script nonce, restrictive CSP, frame denial, no-referrer, no-store for private responses, nosniff and permissions restrictions |
| Updates | Predictable temp path; missing digest could fall back to file size | Unique CSPRNG temp directory; create-new file; SHA-256 required before successful download completion |

The old token requirement already prevented ordinary unauthenticated remote commands. Missing Origin validation is defense in depth; this review did not demonstrate an unauthenticated account takeover. The path boundary issues concern an already paired client escaping an enabled share.

## 1.4: the phone proves itself without sending its credential

Reviewed with pull request #20 (omnizs38), whose challenge-response idea, DPAPI storage and
updater host list are kept; the parts below marked *changed* differ from that pull request.
Protocol details: `src/core/remote_auth.h`.

| Boundary | Before 1.4 | 1.4 |
|---|---|---|
| Proving a paired phone | The 256-bit credential sent in clear on every WebSocket connection and as `Authorization: Bearer` on every HTTP request | Sent once, at pairing. Each connection answers a fresh 128-bit challenge with HMAC-SHA256(credential, "cue-auth\n" + challenge); a wrong answer closes with 4001 |
| Private HTTP requests (folders, thumbnails, covers, queue, previews) | Bearer credential | Signed by the connection's session: HMAC-SHA256(session, sid, method, exact target, n). The session key is *derived on both sides* (HMAC of the credential and the challenge, label "cue-session") and never sent (*changed*: the pull request sent it in the hello). Each number is accepted once inside a 128-number window; the session ends with its connection |
| What the signature covers | — | The request target exactly as the browser sends it, path and query (*changed*: the pull request signed the path without the query and the server compared it with the decoded path, so folders, thumbnails, covers and previews were refused) |
| Credentials at rest | Plain in `state.json` | Sealed with DPAPI for the current Windows user ("dpapi:" + base64, own entropy label). Unsealed only when the remote starts, never while Cue opens. A credential keeps its sealed form between saves, so an unchanged state is not rewritten |
| Upgrading from 1.3.2 | — | Phones stay paired (*changed*: the pull request made every phone pair again). The old file's credentials are kept until sealed; a 1.3.2 page left open on a phone is let in only to be told to reload (no state, no session, no commands), and comes back as the new page |
| Comparisons | `==` on the pairing code, `std::set` lookup on the credential | Constant-time comparison for the code, proofs and signatures |
| Pairing codes | 32-bit value modulo one million (a 0.02% bias) | Rejection sampling: every code equally likely |
| SHA-256 / HMAC | — | PC: Windows CNG (BCrypt). Phone page: a short hand-written implementation (http:// pages have no `crypto.subtle`), checked against Node's crypto for every message length 0–300, keys of 0–129 bytes and UTF-8 text (`tests/remote_auth_page_test.cjs`); the portable C++ version used by the tests is checked against FIPS/RFC 4231 vectors and 200 Python digests |
| Updates (GitHub edition) | WinHTTP followed any https redirect | Only `github.com`, `api.github.com` and `*.githubusercontent.com`, redirects followed one at a time and checked (*changed*: the pull request read the Location header as a number, which fails, so every download would stop at GitHub's redirect) |

Not taken from the pull request, and why:

- **Listening on the LAN address only.** A PC whose address changes (another Wi-Fi, a cable
  plugged in, a VPN, Hyper-V/WSL adapters) would stop answering its phone until Cue restarts,
  while the existing filter already refuses every peer outside private IPv4 ranges.
- **Authenticode check before running an update.** Releases are not signed yet, so it adds no
  protection today; as written it could refuse unsigned installers on some Windows versions
  (it read `GetLastError()` instead of WinVerifyTrust's result) and its thumbprint dropped the
  last byte. Worth adding, with those fixes, when releases are signed.
- **Flushing `state.json` to the disk on every save.** Not a security matter, and more disk
  work on every save; the file is already swapped atomically with a backup.
- **Distrusting the shared folders read from `state.json`.** Only the person's own account can
  write that file; the checks refused lower-case drive letters and names with "..", which would
  have silently unshared folders. Paths are already validated when the phone uses them.
- **Per-address pairing locks and a per-connection HTTP limit.** The per-address lock used the
  same limit as the global one (no effect), and each HTTP request is its own connection, so a
  per-connection limit never triggers. Worker and thumbnail limits already bound the work.

Still true in 1.4 (see the limits below): the link is not encrypted. Someone watching the
network at the moment of pairing sees the credential; someone able to change traffic can act on
an open connection. The gain is against listening later: past traffic no longer contains the
credential, and a captured request cannot be sent again.

Verification: `tests/remote_auth_test.cpp`, `tests/remote_auth_page_test.cjs`,
`tests/remote_auth_browser.cjs` (Chromium, names with apostrophes, brackets, accents, CJK and
emoji), `tests/remote_e2e_harness.cjs` (the real Windows server under Wine: pairing, challenge,
replay, wrong proof, session lifetime, and a real 1.3.2 page reloading into 1.4 while staying
paired), `tests/remote_test.cpp` (sealing, migration, stable saves) and `tools/wine-tests.sh`.

## Limits that remain

- **HTTP/WebSocket traffic is not encrypted.** A PIN is access control, not transport protection. A compromised LAN/router or on-path attacker remains a risk. Use a trusted LAN; do not expose the port to the internet. TLS with a trustworthy local certificate or a secure companion transport is the next security project. A self-signed warning is not a complete solution.
- Remembered credentials stay in the phone’s localStorage (a browser offers nothing better to a page on http://). On the PC they are sealed with DPAPI since 1.4. There is no per-device management screen; turning the remote off revokes all devices.
- Private IPv4 filtering is not proof that a device is on the same Wi-Fi: routed private/VPN networks can also reach a listening endpoint. Access still requires pairing/a remembered token.
- Resolved-path and reparse checks restrict a remote-only attacker. A hostile local process with write access to shared directories can race path-based operations after validation. No claim of protection against a compromised Windows account is made.
- Explorer thumbnail providers and filesystem calls can stall. Work is bounded and responses invalidated on stop/share/credential changes; blocked OS calls are not forcibly terminated.
- The libmpv/FFmpeg DLL is unchanged. Codec vulnerabilities, native GPU performance and media-format coverage require their own review/testing.
- The application/installer remains unsigned. SHA-256 verification is not an Authenticode publisher signature.

## Verification

Current release validation is recorded in [VALIDATION.md](VALIDATION.md).
Portable tests do not establish native GPU performance or whole-process leak freedom.

## References

- [OWASP WebSocket Security Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/WebSocket_Security_Cheat_Sheet.html): Origin checks, authentication, limits and backpressure.
- [Microsoft GetFinalPathNameByHandle](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfinalpathnamebyhandlew): resolved paths.
- [Microsoft reparse point operations](https://learn.microsoft.com/en-us/windows/win32/fileio/reparse-point-operations): file attribute/reparse behavior.
