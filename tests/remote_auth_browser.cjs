// Optional browser regression test (playwright + Chromium): the phone's signed requests as a real
// browser sends them. A strict fake PC refuses any request whose signature is wrong for the exact
// target the browser put on the wire, any number used twice and any request without a session.
// Folder and file names with apostrophes, brackets, spaces, accents, CJK and emoji must all
// verify (the browser percent-encodes some of them itself; the page signs what it will send).
const {chromium} = require('playwright');
const auth = require('./remote_auth_fixture.cjs');
const fs = require('fs'), http = require('http'), assert = require('assert'), path = require('path');
process.chdir(path.join(__dirname, '..'));
const html = fs.readFileSync('assets/remote/index.html', 'utf8');
const catalog = JSON.parse(fs.readFileSync('i18n/pt-BR.json', 'utf8'));
const pageHTML = html.replace('<script>', '<script nonce="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa">')
  .replace('/*CUE_BOOT*/', 'window.CUE=' + JSON.stringify({lang: 'pt-BR', strings: catalog}) + ';');
const token = 'c'.repeat(64);
const names = ["Ocean's Eleven (2001) [1080p].mkv", 'Cidade de Deus – versão estendida.mp4', 'Sen to Chihiro 千と千尋.mkv',
  'emoji 🎬 #1 & more!.mkv', 'a+b=c; 100%.mkv', "it's (not) *really* ~fine~.webm"];
const folder = "C:\\Vídeos\\Filmes d'Ouro [HD]";
const seen = new Set(), refused = [], verified = [], captured = [];
const server = http.createServer((req, res) => {
  if (req.url === '/font.ttf') { res.setHeader('Content-Type', 'font/ttf'); res.end(fs.readFileSync('assets/fonts/CueOmni.ttf')); return; }
  if (req.url === '/favicon.ico') { res.statusCode = 404; res.end(); return; }  // the browser's own, unsigned
  if (req.url === '/' || req.url.startsWith('/?') || req.url === '/index.html') { res.setHeader('Content-Type', 'text/html'); res.end(pageHTML); return; }
  const n = (/^Cue [0-9a-f]{16}:([0-9]+):/.exec(req.headers.authorization || '') || [])[1];
  if (!auth.signed(req, token) || !n || seen.has(n)) { refused.push(req.url); res.statusCode = 403; res.end(); return; }
  seen.add(n); verified.push(req.url); captured.push({url: req.url, authorization: req.headers.authorization});
  const url = new URL(req.url, 'http://x');
  if (url.pathname === '/list') {
    const p = url.searchParams.get('p') || '';
    const body = p === '' ? {path: '', parent: '', title: '', items: [{dir: true, name: "Filmes d'Ouro [HD]", path: folder}]}
                          : {path: folder, parent: '', title: "Filmes d'Ouro [HD]", items: names.map(nm => ({dir: false, name: nm, path: folder + '\\' + nm}))};
    res.setHeader('Content-Type', 'application/json'); res.end(JSON.stringify(body)); return;
  }
  if (url.pathname === '/thumb') { res.statusCode = 404; res.end(); return; }
  res.statusCode = 404; res.end();
});
(async () => {
  await new Promise(r => server.listen(8131, '127.0.0.1', r));
  const browser = await chromium.launch({headless: true, executablePath: process.env.CUE_CHROMIUM_PATH || undefined, args: ['--no-sandbox', '--disable-dev-shm-usage']});
  const context = await browser.newContext({viewport: {width: 390, height: 844}, isMobile: true, hasTouch: true});
  const page = await context.newPage();
  const errors = [];
  page.on('pageerror', e => errors.push(e.message));
  await page.addInitScript(([token, ch, sid, proof, kid]) => {
    localStorage.setItem('cue.token.v2', token);
    window.__auth = [];
    window.WebSocket = class {
      constructor() { this.readyState = 0; setTimeout(() => { this.readyState = 1; this.onopen?.(); this.onmessage?.({data: JSON.stringify({t: 'chal', n: ch})}); }, 5); }
      send(data) {
        const m = JSON.parse(data);
        if (m.t !== 'auth') return;
        window.__auth.push(m);
        // the PC checks the answer exactly as remote_server.cpp does
        if (m.k !== kid || m.r !== proof) { this.readyState = 3; this.onclose?.({code: 4001}); return; }
        setTimeout(() => {
          this.onmessage?.({data: JSON.stringify({t: 'hello', sid, pc: 'Cue PC'})});
          this.onmessage?.({data: JSON.stringify({t: 'state', view: 'home', file: '', pos: 0, dur: 0, speed: 1, vol: 100, paused: true, ended: false, audioOnly: false,
            sub: {has: false, on: false, delay: 0, size: 44, sizeMin: 28, sizeMax: 72, pos: 93, posDefault: 93, posMin: 12, posMax: 97}, audio: {n: 0, i: 0}, timer: {m: 0, left: 0}, next: '', color: '#a0508f'})});
        }, 5);
      }
      close() { this.readyState = 3; }
    };
  }, [token, auth.CHALLENGE, auth.SID, auth.proof(token), auth.kid(token)]);
  await page.goto('http://127.0.0.1:8131');
  await page.waitForTimeout(250);
  const answers = await page.evaluate(() => window.__auth);
  assert.strictEqual(answers.length, 1);
  assert(!JSON.stringify(answers).includes(token), 'the credential itself never leaves the phone');
  // the folders: the list, the folder, then every thumbnail; all signed, none refused
  // (one shared folder: it opens straight away)
  await page.locator('#bFolder').click(); await page.waitForTimeout(1200);
  assert.deepStrictEqual(refused, [], 'refused: ' + refused.join(' | '));
  const thumbs = verified.filter(u => u.startsWith('/thumb'));
  assert(verified.some(u => u.startsWith('/list?p=C%3A')), 'the folder was listed');
  assert(thumbs.length === names.length, 'every file asked for its thumbnail: ' + thumbs.length);
  for (const t of thumbs) assert(!t.includes(' '), 'what the browser sent is what was signed: ' + t);
  // a captured request sent again is refused (its number is spent); a forged one too
  const get = (url, authorization) => new Promise(resolve =>
    http.get({host: '127.0.0.1', port: 8131, path: url, headers: {authorization}}, r => { r.resume(); resolve(r.statusCode); }));
  const replay = await get(captured[0].url, captured[0].authorization);  // exactly as seen on the network
  const moved = await get('/list?p=C%3A%5C', captured[0].authorization.replace(/:[0-9]+:/, ':999:'));  // another target, another number
  const forged = await get(captured[0].url, 'Cue ' + auth.SID + ':1000:' + '0'.repeat(64));
  assert.strictEqual(moved, 403);
  assert.strictEqual(forged, 403);
  assert.strictEqual(replay, 403);
  // after the session's connection is replaced, the page signs with the new session only
  assert.deepStrictEqual(errors, []);
  await browser.close();
  server.close();
  console.log('remote auth browser: proof without the credential, ' + verified.length + ' signed requests (apostrophes, brackets, accents, CJK, emoji) verified, replay/forgery refused');
})().catch(e => { console.error(e); process.exit(1); });
