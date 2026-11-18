// The page's own SHA-256 and HMAC-SHA256 (assets/remote/index.html), as the phone runs them,
// against Node's crypto: published vectors, every length across the padding boundaries, keys
// shorter, equal to and longer than a block, text outside ASCII, and the protocol's values.
const fs = require('fs'), vm = require('vm'), assert = require('assert'), path = require('path'), crypto = require('crypto');
const root = process.env.CUE_SOURCE_ROOT || path.join(__dirname, '..');
const html = fs.readFileSync(path.join(root, 'assets/remote/index.html'), 'utf8');
const code = html.slice(html.indexOf('function utf8Bytes('), html.indexOf('// ── link to the PC'));
const ctx = {};
vm.createContext(ctx);
vm.runInContext(code, ctx);
const sha = b => crypto.createHash('sha256').update(Buffer.from(b)).digest('hex');
const mac = (k, m) => crypto.createHmac('sha256', Buffer.from(k)).update(Buffer.from(m)).digest('hex');
// FIPS 180-2 and RFC 4231
assert.strictEqual(ctx.sha256Hex(''), 'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855');
assert.strictEqual(ctx.sha256Hex('abc'), 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad');
assert.strictEqual(ctx.hmacHex('Jefe', 'what do ya want for nothing?'), '5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843');
assert.strictEqual(ctx.toHex(ctx.hmacBytes(Array(131).fill(0xaa), ctx.utf8Bytes('Test Using Larger Than Block-Size Key - Hash Key First'))),
                   '60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54');
// random bytes: every length to 300, then longer ones
let seed = 12345;
const rnd = () => (seed = (seed * 1103515245 + 12345) >>> 0) >>> 16 & 255;
let checked = 0;
for (let n = 0; n <= 300; ++n) {
  const msg = Array.from({length: n}, rnd);
  assert.strictEqual(ctx.toHex(ctx.sha256Bytes(msg)), sha(msg), 'sha256 length ' + n);
  for (const kl of [0, 1, 32, 63, 64, 65, 129]) {
    const key = Array.from({length: kl}, rnd);
    assert.strictEqual(ctx.toHex(ctx.hmacBytes(key, msg)), mac(key, msg), 'hmac ' + kl + '/' + n);
    ++checked;
  }
}
for (const n of [1000, 4096, 10007]) { const msg = Array.from({length: n}, rnd); assert.strictEqual(ctx.toHex(ctx.sha256Bytes(msg)), sha(msg)); }
// text: UTF-8, as the PC reads it
for (const s of ['Vídeos', 'Ocean\'s 11 (2001) [x].mkv', '日本語の映画', 'emoji 🎬', 'a\nb']) {
  assert.strictEqual(ctx.sha256Hex(s), crypto.createHash('sha256').update(s, 'utf8').digest('hex'));
  assert.strictEqual(ctx.hmacHex('k' + s, s), crypto.createHmac('sha256', 'k' + s).update(s, 'utf8').digest('hex'));
}
// the protocol's values, as tests/remote_auth_test.cpp has them (computed independently)
const cred = '3f'.repeat(32), chal = '0123456789abcdef0123456789abcdef';
assert.strictEqual(ctx.credentialKid(cred), '67ef9de3b81d374f');
assert.strictEqual(ctx.hmacHex(cred, 'cue-auth\n' + chal), '6737f15c0e648bd948bf61504e2895e7001a75695af3defae16b47977cca62b7');
const sk = ctx.hmacHex(cred, 'cue-session\n' + chal);
assert.strictEqual(sk, 'e7f1ca3b7235ae06e82088b507a11caed0d10ba00beb5d00743e7789a5abe6b8');
assert.strictEqual(ctx.hmacHex(sk, 'a1b2c3d4e5f60718\nGET\n/thumb?p=C%3A%5CV%C3%ADdeos%5COcean%27s.mkv\n7'),
                   '4b18a60abba074bf1646eead44a4fd974d3a6f35528d960ac62a730fe96831fb');
// cost on the phone: one request's signature
const t0 = process.hrtime.bigint();
for (let i = 0; i < 1000; ++i) ctx.hmacHex(sk, 'a1b2c3d4e5f60718\nGET\n/thumb?p=C%3A%5CVideos%5Csome%20long%20file%20name.mkv\n' + i);
const us = Number(process.hrtime.bigint() - t0) / 1000 / 1000;
console.log('remote page auth: SHA-256/HMAC equal Node crypto for ' + checked + ' random pairs, every length 0-300, UTF-8 text and protocol values; ' + us.toFixed(1) + ' us per signed request (Node)');
