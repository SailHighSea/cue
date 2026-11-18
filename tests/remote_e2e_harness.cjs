// End to end against the real Windows server (tools/remote_harness.exe, natively or under Wine)
// with the real page in Chromium, and with raw sockets for what a browser would not send.
//   remote_harness.exe <font.ttf> <shared folder>       (pairing code 481234, port 7878)
//   CUE_CHROMIUM_PATH=... node tests/remote_e2e_harness.cjs [host:port]
// Checks: pairing by the link's code; the credential never sent again (challenge answered);
// every folder/thumbnail request signed and accepted by the C++ server; a captured request
// replayed or moved to another path refused; a wrong proof refused (4001); a page from before
// 1.4 (credential sent in the clear) told to reload; an unsigned request refused.
const {chromium} = require('playwright');
const net = require('net'), crypto = require('crypto'), assert = require('assert');
const host = process.argv[2] || '127.0.0.1:7878';
const [hostname, port] = host.split(':');

// a minimal WebSocket client: the upgrade, masked text frames out, plain frames in
function rawSocket() {
  return new Promise((resolve, reject) => {
    const s = net.connect(+port, hostname);
    let buf = Buffer.alloc(0), open = false;
    const frames = [], waiters = [];
    const push = f => { const w = waiters.shift(); if (w) w(f); else frames.push(f); };
    s.on('data', d => {
      buf = Buffer.concat([buf, d]);
      if (!open) {
        const end = buf.indexOf('\r\n\r\n');
        if (end < 0) return;
        const head = buf.slice(0, end).toString();
        if (!/^HTTP\/1\.1 101/.test(head)) return reject(new Error('no upgrade: ' + head));
        buf = buf.slice(end + 4); open = true;
        resolve(api);
      }
      for (;;) {
        if (buf.length < 2) return;
        let len = buf[1] & 127, at = 2;
        if (len === 126) { if (buf.length < 4) return; len = buf.readUInt16BE(2); at = 4; }
        if (buf.length < at + len) return;
        const op = buf[0] & 15, payload = buf.slice(at, at + len);
        buf = buf.slice(at + len);
        push(op === 8 ? {close: payload.length >= 2 ? payload.readUInt16BE(0) : 1005} : {text: payload.toString()});
      }
    });
    s.on('close', () => push({ended: true}));
    s.on('error', reject);
    const key = crypto.randomBytes(16).toString('base64');
    s.write(`GET /ws HTTP/1.1\r\nHost: ${host}\r\nOrigin: http://${host}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: ${key}\r\nSec-WebSocket-Version: 13\r\n\r\n`);
    const api = {
      next: (ms = 3000) => frames.length ? Promise.resolve(frames.shift()) : new Promise((r, j) => { waiters.push(r); setTimeout(() => j(new Error('timeout')), ms); }),
      send(text) {
        const p = Buffer.from(text), mask = crypto.randomBytes(4);
        const head = p.length < 126 ? Buffer.from([0x81, 0x80 | p.length]) : Buffer.from([0x81, 0x80 | 126, p.length >> 8, p.length & 255]);
        const body = Buffer.from(p.map((b, i) => b ^ mask[i % 4]));
        s.write(Buffer.concat([head, mask, body]));
      },
      end() { s.destroy(); },
    };
  });
}
const hmac = (k, t) => crypto.createHmac('sha256', k).update(t).digest('hex');
const get = (path, authorization) => new Promise(resolve => {
  const s = net.connect(+port, hostname, () => s.write(`GET ${path} HTTP/1.1\r\nHost: ${host}\r\n${authorization ? 'Authorization: ' + authorization + '\r\n' : ''}\r\n`));
  let d = ''; s.on('data', x => d += x); s.on('close', () => resolve(+(/^HTTP\/1\.1 (\d+)/.exec(d) || [])[1]));
});

(async () => {
  // (a page the test itself serves in place of the PC's counts as public to Chrome's local
  // network checks; a real phone loads the page from the PC, so they do not apply there)
  const browser = await chromium.launch({headless: true, executablePath: process.env.CUE_CHROMIUM_PATH || undefined,
    args: ['--no-sandbox', '--disable-dev-shm-usage', '--disable-features=LocalNetworkAccessChecks,BlockInsecurePrivateNetworkRequests']});
  const context = await browser.newContext({viewport: {width: 390, height: 844}, isMobile: true, hasTouch: true});
  const page = await context.newPage();
  const errors = [], wire = [], signedRequests = [];
  page.on('pageerror', e => errors.push(e.message));
  page.on('websocket', ws => ws.on('framesent', f => wire.push(String(f.payload))));
  page.on('request', r => { const a = r.headers()['authorization']; if (a) signedRequests.push({url: new URL(r.url()).pathname + new URL(r.url()).search, a}); });
  page.on('response', r => { if (r.status() === 403 && !r.url().endsWith('favicon.ico')) errors.push('403 ' + r.url()); });

  // 1. pairing from the QR code's link
  await page.goto(`http://${host}/#c=481234`);
  await page.waitForFunction(() => { try { return /^[0-9a-f]{64}$/.test(localStorage.getItem('cue.token.v2') || ''); } catch (e) { return false; } }, null, {timeout: 8000});
  const token = await page.evaluate(() => localStorage.getItem('cue.token.v2'));
  assert(wire.some(w => w.includes('"c":"481234"')), 'the code was sent');
  // 2. a fresh page: the challenge answered, the credential itself never on the wire
  wire.length = 0;
  await page.goto(`http://${host}/`);
  await page.waitForTimeout(1200);
  assert(wire.some(w => /"k":"[0-9a-f]{16}","r":"[0-9a-f]{64}"/.test(w)), 'answered the challenge: ' + wire.join(' | '));
  assert(!wire.some(w => w.includes(token)), 'the credential never travels after pairing');
  // 3. folders and thumbnails through the C++ server's signature check
  await page.locator('#bFolder').click();
  await page.waitForTimeout(2500);
  const tiles = await page.locator('#grid .ftile, #grid .tile').count();
  assert(tiles >= 3, 'the shared folder was listed by the server: ' + tiles);
  assert(signedRequests.some(r => r.url.startsWith('/list')) && signedRequests.some(r => r.url.startsWith('/thumb')), 'signed list and thumbnails');
  assert.deepStrictEqual(errors, [], errors.join('\n'));
  // 4. what a listener on the network saw cannot be used again, nor moved to another file
  const seen = signedRequests.find(r => r.url.startsWith('/list'));
  assert.strictEqual(await get(seen.url, seen.a), 403, 'replay refused');
  assert.strictEqual(await get('/list?p=Z%3A%5C', seen.a.replace(/:(\d+):/, (m, n) => ':' + (+n + 1000) + ':')), 403, 'moved request refused');
  assert.strictEqual(await get('/queue'), 403, 'unsigned request refused');
  assert.strictEqual(await get('/queue', 'Bearer ' + token), 403, 'the old bearer form is refused');
  // 5. a wrong answer to the challenge: 4001
  {
    const s = await rawSocket();
    const chal = JSON.parse((await s.next()).text);
    assert.strictEqual(chal.t, 'chal');
    const kid = crypto.createHash('sha256').update(token).digest('hex').slice(0, 16);
    s.send(JSON.stringify({t: 'auth', k: kid, r: hmac(token, 'cue-auth\n' + '0'.repeat(32)), i: 'e2etestpage'}));
    assert.strictEqual((await s.next()).close, 4001, 'a proof for another challenge is refused');
    s.end();
  }
  // 6. the right answer from a raw client: hello names a session, never carries a key
  {
    const s = await rawSocket();
    const chal = JSON.parse((await s.next()).text);
    const kid = crypto.createHash('sha256').update(token).digest('hex').slice(0, 16);
    s.send(JSON.stringify({t: 'auth', k: kid, r: hmac(token, 'cue-auth\n' + chal.n), i: 'e2etestpage2'}));
    const hello = JSON.parse((await s.next()).text);
    assert(hello.t === 'hello' && /^[0-9a-f]{16}$/.test(hello.sid) && !hello.token && !('sk' in hello), JSON.stringify(hello));
    // its derived key signs a request the server accepts
    const sk = hmac(token, 'cue-session\n' + chal.n);
    assert.strictEqual(await get('/queue', `Cue ${hello.sid}:1:${hmac(sk, hello.sid + '\nGET\n/queue\n1')}`), 200, 'derived session key accepted');
    assert.strictEqual(await get('/queue', `Cue ${hello.sid}:1:${hmac(sk, hello.sid + '\nGET\n/queue\n1')}`), 403, 'its number is spent');
    s.end();
    await new Promise(r => setTimeout(r, 400));
    assert.strictEqual(await get('/queue', `Cue ${hello.sid}:2:${hmac(sk, hello.sid + '\nGET\n/queue\n2')}`), 403, 'the session ended with its connection');
  }
  // 7. a page from before 1.4 (it sends the credential itself): told to reload, nothing else
  {
    const s = await rawSocket();
    await s.next();  // the challenge, which that page ignores
    s.send(JSON.stringify({t: 'auth', k: token, i: 'oldpage123'}));
    const hello = JSON.parse((await s.next()).text), nudge = JSON.parse((await s.next()).text);
    assert(hello.t === 'hello' && !hello.sid, JSON.stringify(hello));
    assert(nudge.t === 'state' && /^reload-[0-9a-f]{8}$/.test(nudge.lang) && !('file' in nudge), JSON.stringify(nudge));
    const last = await s.next();
    assert(last.ended || last.close, 'and the connection ends');
  }
  // 8. the same with a real page from before 1.4 (CUE_OLD_PAGE: its index.html), open across
  // the update: it reloads into the new page and stays paired (no code asked again)
  if (process.env.CUE_OLD_PAGE) {
    const fs = require('fs');
    const current = await (await fetch(`http://${host}/`)).text();
    const boot = current.split('\n').find(l => l.startsWith('window.CUE=')) || '';
    let old = fs.readFileSync(process.env.CUE_OLD_PAGE, 'utf8').replace('/*CUE_BOOT*/', boot);
    let served = 0;
    await page.route(`http://${host}/`, route => { if (served++) return route.continue(); route.fulfill({status: 200, contentType: 'text/html', body: old}); });
    wire.length = 0;
    await page.goto(`http://${host}/`);
    await page.waitForTimeout(2500);
    assert(served >= 2, 'the old page reloaded itself: ' + served);
    assert(wire.some(w => w.includes('"k":"' + token + '"')), 'the old page spoke its old way');
    assert(wire.some(w => /"k":"[0-9a-f]{16}","r":"[0-9a-f]{64}"/.test(w)), 'then the new page answered the challenge');
    const pairShown = await page.evaluate(() => !document.getElementById('pair').classList.contains('hide'));
    assert(!pairShown, 'still paired: no code asked');
    assert.strictEqual(await page.evaluate(() => localStorage.getItem('cue.token.v2')), token, 'the same credential kept');
  }
  await browser.close();
  console.log('remote e2e (real server): pairing, challenge without the credential, ' + signedRequests.length + ' signed requests, replay/moved/unsigned/bearer refused, wrong proof 4001, derived session key, session ends with its socket, legacy page reload' + (process.env.CUE_OLD_PAGE ? ', a real 1.3.2 page reloading into 1.4 and staying paired' : '') + ' passed');
})().catch(e => { console.error(e); process.exit(1); });
