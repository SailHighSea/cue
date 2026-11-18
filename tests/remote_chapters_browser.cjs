// Optional browser regression test: a film's chapters on the phone. Requires playwright and
// Chromium. Run from the source root.
// - The PC sends them (state.ch, [start, name]) only when they are on in its settings and there
//   are two or more: the menu gets "capítulos ▸", a list that opens on the playing one (marked),
//   five rows at a time, "5 / 13" in its title; a tap jumps (seek to the chapter's start).
// - The timeline gets a 2 px break where each chapter starts, never two closer than 8 px; in the
//   volume mode none.
// - A drag never snaps: the card says which chapter it lands in ("6 · name"; "capítulo 4"
//   unnamed). The time sent on release is where the finger was.
// - The PC turning them off (no state.ch): the list closes back to the menu, the row and the
//   breaks go.
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
const CH = [[0, 'Abertura'], [212, 'A chegada'], [655, 'O posto'], [1190, ''], [1730, 'Noite no motel'],
  [2310, 'A ligação que ninguém atendeu naquela madrugada fria'], [2905, 'Perseguição'], [3480, 'O lago'], [4020, ''],
  [4570, 'Reencontro'], [5110, 'O acerto'], [5680, 'Epílogo'], [6290, 'Créditos'], [6300, 'colado no anterior']];
(async () => {
  await new Promise(r => server.listen(8149, '127.0.0.1', r));
  const browser = await chromium.launch({headless: true, executablePath: process.env.CUE_CHROMIUM_PATH || undefined, args: ['--no-sandbox', '--disable-dev-shm-usage']});
  const ctx = await browser.newContext({viewport: {width: 390, height: 780}, isMobile: true, hasTouch: false});
  const page = await ctx.newPage(); const errors = []; page.on('pageerror', e => errors.push(e.message));
  await page.addInitScript(() => {
    localStorage.setItem('cue.token.v2', 'a'.repeat(64));
    window.__sent = [];
    window.WebSocket = class {
      constructor() { window.__ws = this; this.readyState = 0; setTimeout(() => { this.readyState = 1; this.onopen?.(); this.onmessage?.({data: '{"t":"chal","n":"0123456789abcdef0123456789abcdef"}'}); }, 5); }
      send(d) {
        const m = JSON.parse(d);
        if (m.t === 'auth') setTimeout(() => this.onmessage?.({data: JSON.stringify({t: 'hello', sid: 'a1b2c3d4e5f60718', pc: 'PC'})}), 5);
        else if (m.c && m.c !== 'peek') window.__sent.push([m.c, m.v]);
      }
      close() {}
    };
    window.__state = (pos, ch) => window.__ws.onmessage({data: JSON.stringify(Object.assign({t: 'state', view: 'play', file: 'Filme.mkv', pos, dur: 6720, speed: 1, vol: 100,
      paused: true, ended: false, audioOnly: false, sub: {has: true, on: true, delay: 0, size: 44, sizeMin: 28, sizeMax: 72, pos: 93, posDefault: 93, posMin: 12, posMax: 97},
      audio: {n: 2, i: 0}, timer: {m: 0, left: 0}, next: '', prev: '', queueActive: false, queueRevision: 1, color: '#a0508f', cover: 0, title: '', artist: '', media: 3}, ch ? {ch} : {}))});
  });
  await page.goto('http://127.0.0.1:8149/'); await page.waitForTimeout(300);
  const sent = () => page.evaluate(() => window.__sent.splice(0));
  const rowTexts = () => page.evaluate(() => Array.from(document.querySelectorAll('#rows .row')).map(r => r.textContent));
  const cuts = () => page.evaluate(() => Array.from(document.querySelectorAll('#chCuts .chcut')).map(c => parseInt(c.style.left)));

  // no chapters (one alone counts as none): nothing anywhere
  await page.evaluate(() => window.__state(1950, [[0, 'só um']])); await page.waitForTimeout(150);
  assert.deepEqual(await cuts(), [], 'one chapter: no breaks');
  await page.click('#bMenu'); await page.waitForTimeout(150);
  assert.ok(!(await rowTexts()).some(t => t.startsWith('capítulos')), 'one chapter: no menu row');
  await page.click('#menuTitle'); await page.waitForTimeout(150);

  // the timeline: a break at each start but the first, none closer than 8 px
  await page.evaluate(ch => window.__state(1950, ch), CH); await page.waitForTimeout(150);
  const c = await cuts();
  assert.ok(c.length === CH.length - 2, 'breaks: all but the first and the one 2 px after another (' + c + ')');
  for (let i = 1; i < c.length; ++i) assert.ok(c[i] - c[i - 1] >= 8, 'breaks 8 px apart at least');
  assert.ok(c.every(x => x % 2 === 0 && x > 0 && x < 330), 'on whole art pixels, inside the line');

  // the menu row and the list, opened on the playing chapter (5th: 1730 <= 1950 < 2310)
  await page.click('#bMenu'); await page.waitForTimeout(150);
  assert.ok((await rowTexts())[1].startsWith('capítulos'), 'menu: capítulos under legendas');
  await page.click('[data-row="1"]'); await page.waitForTimeout(200);
  const rows = await rowTexts();
  assert.equal(rows.length, CH.length, 'every chapter listed');
  assert.ok(rows[3].startsWith('capítulo 4') && rows[3].endsWith('19:50'), 'unnamed: "capítulo 4", its time');
  assert.equal(await page.evaluate(() => document.querySelector('#rows .row.now').dataset.row), '4', 'the playing one marked');
  assert.equal(await page.evaluate(() => document.getElementById('menuCount').textContent), '5 / 14', 'opens on it');
  const box = await page.evaluate(() => { const b = document.getElementById('rowsBox'); return [b.clientHeight, b.scrollHeight, b.scrollTop]; });
  assert.ok(box[0] < box[1] && box[2] > 0, 'five rows, scrolled to show it');
  // the wheel's down button moves the choice; the centre jumps there
  await page.click('#bPlay'); await page.waitForTimeout(80); await page.click('#bPlay'); await page.waitForTimeout(80);
  assert.equal(await page.evaluate(() => document.getElementById('menuCount').textContent), '7 / 14');
  await sent(); await page.click('#bCenter'); await page.waitForTimeout(120);
  assert.deepEqual(await sent(), [['seek', 2905]], 'the centre jumps to its start');
  // a tap on another row jumps too
  await page.evaluate(() => document.getElementById('rowsBox').scrollTop = 0); await page.waitForTimeout(100);
  await page.click('[data-row="1"]'); await page.waitForTimeout(120);
  assert.deepEqual(await sent(), [['seek', 212]], 'a tap jumps');

  // the PC turns them off while the list is open: back to the menu, no row, no breaks
  await page.evaluate(() => window.__state(220, null)); await page.waitForTimeout(150);
  assert.equal(await page.evaluate(() => document.getElementById('menuTitleText').textContent), 'menu', 'list closed');
  assert.ok(!(await rowTexts()).some(t => t.startsWith('capítulos')), 'row gone');
  assert.deepEqual(await cuts(), [], 'breaks gone');
  await page.click('#menuTitle'); await page.waitForTimeout(150);

  // a drag on the line never snaps; the card names the chapter it lands in
  await page.evaluate(ch => window.__state(1950, ch), CH); await page.waitForTimeout(150);
  const bar = await page.locator('#bar').boundingBox(), y = bar.y + bar.height / 2, at = t => bar.x + bar.width * t / 6720;
  await page.mouse.move(at(1950), y); await page.mouse.down();
  await page.mouse.move(at(2400), y, {steps: 4}); await page.waitForTimeout(120);
  assert.equal(await page.evaluate(() => document.getElementById('scrubC').textContent), '6 · A ligação que ninguém atendeu naquela madrugada fria');
  await page.mouse.move(at(1300), y, {steps: 4}); await page.waitForTimeout(120);
  assert.equal(await page.evaluate(() => document.getElementById('scrubC').textContent), 'capítulo 4');
  await page.mouse.move(at(1195), y, {steps: 2}); await page.waitForTimeout(80);
  await sent(); await page.mouse.up(); await page.waitForTimeout(150);
  const s = await sent();
  assert.ok(s.length === 1 && s[0][0] === 'seek' && Math.abs(s[0][1] - 1195) < 30 && s[0][1] !== 1190, 'released where the finger was, not pulled to 19:50 (' + JSON.stringify(s) + ')');
  assert.equal(await page.evaluate(() => document.getElementById('scrubC').classList.contains('hide')), true, 'the card goes');

  assert.deepEqual(errors, []);
  await page.screenshot({path: process.env.CUE_SHOT || '/tmp/remote-chapters.png'});
  await browser.close(); server.close();
  console.log('remote chapters browser: list (opens on the playing one, 5 rows, tap and wheel jump), breaks 8 px apart, drag names the chapter and never snaps, PC off: gone');
})().catch(e => { console.error(e); process.exit(1); });
