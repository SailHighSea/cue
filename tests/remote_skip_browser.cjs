// Optional browser regression test: "skip intro" on the phone. Requires playwright and Chromium.
// Run from the source root.
// - While the PC offers a part (state.skip), the card where "up next" goes reads the PC's own
//   words ("pular abertura") and the time left in the part; a tap on it, or on the wheel's centre,
//   sends "skip-part" (no vibration needed: iPhones have none). The card goes at once.
// - It takes the place of "up next" when both would show (an ending near the end), and music
//   never shows it. No state.skip: nothing.
const {chromium} = require('playwright');
const fs = require('fs'), http = require('http'), assert = require('assert'), path = require('path');
process.chdir(path.join(__dirname, '..'));
const html = fs.readFileSync('assets/remote/index.html', 'utf8');
const page_html = html.replace('<script>', '<script nonce="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa">')
  .replace('/*CUE_BOOT*/', 'window.CUE=' + JSON.stringify({lang: 'pt-BR', strings: JSON.parse(fs.readFileSync('i18n/pt-BR.json', 'utf8'))}) + ';');
const server = http.createServer((req, res) => {
  if (req.url !== '/') { res.statusCode = 404; res.end(); return; }
  res.setHeader('Content-Type', 'text/html'); res.end(page_html);
});
(async () => {
  await new Promise(r => server.listen(8147, '127.0.0.1', r));
  const browser = await chromium.launch({headless: true, executablePath: process.env.CUE_CHROMIUM_PATH || undefined, args: ['--no-sandbox', '--disable-dev-shm-usage']});
  const ctx = await browser.newContext({viewport: {width: 390, height: 750}, isMobile: true, hasTouch: true});
  const page = await ctx.newPage(); const errors = []; page.on('pageerror', e => errors.push(e.message));
  await page.addInitScript(() => {
    localStorage.setItem('cue.token.v2', 'a'.repeat(64));
    window.__sent = [];
    window.WebSocket = class {
      constructor() { window.__ws = this; this.readyState = 0; setTimeout(() => { this.readyState = 1; this.onopen?.(); this.onmessage?.({data: '{"t":"chal","n":"0123456789abcdef0123456789abcdef"}'}); }, 5); }
      send(d) {
        const m = JSON.parse(d);
        if (m.t === 'auth') setTimeout(() => this.onmessage?.({data: JSON.stringify({t: 'hello', sid: 'a1b2c3d4e5f60718', pc: 'PC'})}), 5);
        else if (m.c) window.__sent.push(m.c);
      }
      close() {}
    };
    window.__state = (pos, dur, skip, next, audioOnly = false) => window.__ws.onmessage({data: JSON.stringify(Object.assign({t: 'state', view: 'play', file: 'Show - 02.mkv', pos, dur, speed: 1, vol: 100,
      paused: false, ended: false, audioOnly, sub: {has: false, on: false, delay: 0, size: 44, sizeMin: 28, sizeMax: 72, pos: 93, posDefault: 93, posMin: 12, posMax: 97},
      audio: {n: 1, i: 0}, timer: {m: 0, left: 0}, next, prev: '', queueActive: false, queueRevision: 1, color: '#a0508f', cover: 0, title: '', artist: ''}, skip ? {skip} : {}))});
  });
  await page.goto('http://127.0.0.1:8147/'); await page.waitForTimeout(300);
  const sent = () => page.evaluate(() => window.__sent.splice(0));
  const shown = id => page.evaluate(i => !document.getElementById(i).classList.contains('hide'), id);
  const text = id => page.evaluate(i => document.getElementById(i).textContent, id);

  // nothing offered
  await page.evaluate(() => window.__state(30, 1500, null, 'Show - 03.mkv')); await page.waitForTimeout(150);
  assert.equal(await shown('skip'), false, 'no part: no card');
  // the opening: the card, the PC's words and what is left of it
  await page.evaluate(() => window.__state(30, 1500, {kind: 'opening', to: 120}, 'Show - 03.mkv')); await page.waitForTimeout(150);
  assert.equal(await shown('skip'), true, 'opening: the card');
  assert.equal(await text('skipTop'), 'pular abertura');
  assert.equal(await page.evaluate(() => !!document.getElementById('skipBottom')), false, 'no time on it');
  await page.screenshot({path: process.env.CUE_SHOT || '/tmp/remote-skip.png'});
  await page.click('#skip'); await page.waitForTimeout(100);
  assert.deepEqual(await sent(), ['skip-part'], 'the card skips');
  assert.equal(await shown('skip'), false, 'and goes at once');
  // the centre button skips too
  await page.evaluate(() => window.__state(31, 1500, {kind: 'opening', to: 120}, '')); await page.waitForTimeout(150);
  await page.click('#bCenter'); await page.waitForTimeout(100);
  assert.deepEqual(await sent(), ['skip-part'], 'the centre skips');
  // the ending near the end: it stands where "up next" would
  await page.evaluate(() => window.__state(1440, 1500, {kind: 'ending', to: 1499}, 'Show - 03.mkv')); await page.waitForTimeout(150);
  assert.equal(await shown('skip'), true); assert.equal(await shown('next'), false, 'up next waits');
  assert.equal(await text('skipTop'), 'pular encerramento');
  await page.evaluate(() => window.__state(1499, 1500, null, 'Show - 03.mkv')); await page.waitForTimeout(150);
  assert.equal(await shown('skip'), false); assert.equal(await shown('next'), true, 'then up next');
  // music: never
  await page.evaluate(() => window.__state(30, 1500, {kind: 'opening', to: 120}, '', true)); await page.waitForTimeout(150);
  assert.equal(await shown('skip'), false, 'music: no card');
  assert.deepEqual(errors, []);
  await browser.close(); server.close();
  console.log('remote skip browser: card, words, time left, tap and centre send "skip", up next waits, music never');
})().catch(e => { console.error(e); process.exit(1); });
