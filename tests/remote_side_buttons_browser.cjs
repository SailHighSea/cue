// Optional browser regression test: the wheel's side buttons. Requires playwright and Chromium.
// Run from the source root.
// - Songs in a queue: back asks the PC for its previous file ("prev") and forward for its next,
//   however long the page has been open. Before 1.4 the page stepped through its own copy of the
//   queue, loaded once: from the third song, back went to the second and then asked for the
//   second again, which the PC took as play/pause (the song paused); at other times it did nothing
//   or went to the wrong song. Found with the real Cue under Wine; this is the page's half.
// - At either end of the queue nothing is sent (the wheel shakes).
// - A film in up-next mode goes back to the song before it the same way.
// - Volume mode: the side buttons are - and +, 5 a step, 0 to 125, for films and songs; time
//   mode on a film keeps 15 s.
const {chromium} = require('playwright');
const fs = require('fs'), http = require('http'), assert = require('assert'), path = require('path');
process.chdir(path.join(__dirname, '..'));
const html = fs.readFileSync('assets/remote/index.html', 'utf8');
const page_html = html.replace('<script>', '<script nonce="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa">')
  .replace('/*CUE_BOOT*/', 'window.CUE=' + JSON.stringify({lang: 'pt-BR', strings: JSON.parse(fs.readFileSync('i18n/pt-BR.json', 'utf8'))}) + ';');
// the list as it was when the page first asked (the first song current): stale on purpose
const queue = {items: ['Song 1.mp3', 'Song 2.mp3', 'Song 3.mp3', 'Film.mkv'].map((name, i) => ({id: i + 1, name, current: i === 0, audio: i < 3}))};
const server = http.createServer((req, res) => {
  if (req.url.startsWith('/queue')) { res.setHeader('Content-Type', 'application/json'); res.end(JSON.stringify(queue)); return; }
  if (req.url !== '/') { res.statusCode = 404; res.end(); return; }
  res.setHeader('Content-Type', 'text/html'); res.end(page_html);
});
(async () => {
  await new Promise(r => server.listen(8143, '127.0.0.1', r));
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
        else if (m.c) window.__sent.push(m.c + (m.v !== undefined ? ' ' + m.v : ''));
      }
      close() {}
    };
    // what the PC publishes: the playing file, its neighbours in the queue, the volume
    window.__state = (file, audioOnly, prev, next, vol = 100, rev = 1) => window.__ws.onmessage({data: JSON.stringify({t: 'state', view: 'play', file, pos: 5, dur: 200, speed: 1, vol,
      paused: false, ended: false, audioOnly, sub: {has: false, on: false, delay: 0, size: 44, sizeMin: 28, sizeMax: 72, pos: 93, posDefault: 93, posMin: 12, posMax: 97},
      audio: {n: 1, i: 0}, timer: {m: 0, left: 0}, next, prev, queueActive: true, queueRevision: rev, color: '#a0508f', cover: 0, title: file, artist: 'Artist'})});
  });
  await page.goto('http://127.0.0.1:8143/'); await page.waitForTimeout(300);
  const sent = async () => { const s = await page.evaluate(() => window.__sent.splice(0)); return s; };
  const press = async id => { await page.click('#' + id); await page.waitForTimeout(120); };
  const songs = ['Song 1.mp3', 'Song 2.mp3', 'Song 3.mp3'];
  const at = (i, rev) => page.evaluate(([f, p, n, r]) => window.__state(f, true, p, n, 100, r), [songs[i], songs[i - 1] || '', songs[i + 1] || 'Film.mkv', rev]);

  // the page loads the list on the first song (forward), then the songs move on
  await at(0, 1); await page.waitForTimeout(150);
  await press('bFwd'); assert.deepEqual(await sent(), ['next'], 'forward from song 1');
  await at(1, 2); await page.waitForTimeout(100);
  await press('bFwd'); assert.deepEqual(await sent(), ['next'], 'forward from song 2');
  await at(2, 3); await page.waitForTimeout(100);
  // back, back: the PC's previous each time, never the playing song (which would pause it)
  await press('bBack'); assert.deepEqual(await sent(), ['prev'], 'back from song 3');
  await at(1, 4); await page.waitForTimeout(100);
  await press('bBack'); assert.deepEqual(await sent(), ['prev'], 'back from song 2');
  await at(0, 5); await page.waitForTimeout(100);
  await press('bBack'); assert.deepEqual(await sent(), [], 'back on the first song: nothing to go back to');
  // the last one: nothing after it
  await page.evaluate(() => window.__state('Film.mkv', false, 'Song 3.mp3', '', 100, 6)); await page.waitForTimeout(150);
  await press('bFwd'); assert.deepEqual(await sent(), ['skip 15'], 'a film in time mode: forward is 15 s');
  await press('bBack'); assert.deepEqual(await sent(), ['skip -15'], 'a film in time mode: back is 15 s');
  // up-next mode on the film (time → volume → up next): back is the song before it
  await press('bCenter'); await press('bCenter');
  await press('bFwd'); assert.deepEqual(await sent(), [], 'up next on the last film: nothing after it');
  await press('bBack'); assert.deepEqual(await sent(), ['prev'], 'up next on the film: back to the song');
  // volume mode on the film: - and +
  await press('bCenter'); await press('bCenter');   // up next → time → volume
  const icons = () => page.evaluate(() => [document.getElementById('bBack').getAttribute('aria-label'), document.getElementById('bFwd').getAttribute('aria-label')]);
  assert.deepEqual(await icons(), ['diminuir o volume', 'aumentar o volume'], 'volume mode: the side buttons say what they do');
  await press('bBack'); await press('bBack'); await press('bFwd');
  assert.deepEqual(await sent(), ['vol 95', 'vol 90', 'vol 95'], 'volume mode on a film: 5 a step');
  // a song in volume mode: the same keys (not the songs either side), up to 125 and down to 0
  await page.evaluate(() => window.__state('Song 3.mp3', true, 'Song 2.mp3', 'Film.mkv', 122, 7)); await page.waitForTimeout(150);
  await press('bFwd'); await press('bFwd');
  assert.deepEqual(await sent(), ['vol 125'], 'a song in volume mode: up to 125, then nothing');
  // (the PC confirms 125, as it does; then someone turns it down on the PC)
  await page.evaluate(() => window.__state('Song 3.mp3', true, 'Song 2.mp3', 'Film.mkv', 125, 7)); await page.waitForTimeout(100);
  await page.evaluate(() => window.__state('Song 3.mp3', true, 'Song 2.mp3', 'Film.mkv', 3, 7)); await page.waitForTimeout(150);
  await press('bBack'); await press('bBack');
  assert.deepEqual(await sent(), ['vol 0'], 'a song in volume mode: down to 0, then nothing');
  // back to time mode on the song: the songs either side again
  await press('bCenter');
  await press('bBack'); assert.deepEqual(await sent(), ['prev'], 'a song in time mode: back is the song before');
  assert.equal(errors.length, 0, errors.join('\n'));
  await browser.close(); server.close();
  console.log('remote side buttons: passed');
})().catch(e => { console.error(e); process.exit(1); });
