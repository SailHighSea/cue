// The PC's side of the phone's proof, for the browser tests' fake servers (src/core/remote_auth.h).
// The fake WebSocket sends CHALLENGE and answers with SID; HTTP requests are checked here with
// Node's own crypto, so these tests also confirm the page's handwritten SHA-256/HMAC.
const crypto = require('crypto');
const CHALLENGE = '0123456789abcdef0123456789abcdef';
const SID = 'a1b2c3d4e5f60718';
const hmac = (key, text) => crypto.createHmac('sha256', Buffer.from(key, 'utf8')).update(Buffer.from(text, 'utf8')).digest('hex');
function sessionKey(token) { return hmac(token, 'cue-session\n' + CHALLENGE); }
// true when the request carries this session's signature for exactly this target
function signed(req, token) {
  const m = /^Cue ([0-9a-f]{16}):([1-9][0-9]{0,14}):([0-9a-f]{64})$/.exec(req.headers.authorization || '');
  if (!m || m[1] !== SID) return false;
  return hmac(sessionKey(token), SID + '\nGET\n' + req.url + '\n' + m[2]) === m[3];
}
// what a page with this credential answers to the challenge
function proof(token) { return hmac(token, 'cue-auth\n' + CHALLENGE); }
function kid(token) { return crypto.createHash('sha256').update(token).digest('hex').slice(0, 16); }
module.exports = { CHALLENGE, SID, sessionKey, signed, proof, kid };
