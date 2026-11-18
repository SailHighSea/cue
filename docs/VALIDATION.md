# Validation · Cue 1.4

## Closing the release: music shapes, the film's bar, the light, slow disks (2026-11-17)

Reviewed by the author on his PC (Windows, 4K at 175%) from test builds of the real Cue, step by
step: the music player's five shapes, the film's bar, the keyboard notices, speed and time left,
skip intro, the ambient light's breath, flow and reveal, the mini player, the home's settings.
His last build in normal use (video and music): fluid, no problems found.

- `tools/test.sh` passed on the release source (every portable test, the catalogs in 8 languages,
  the phone page's JavaScript tests).
- Monkey under Wine and Xvfb: about 95 minutes, ~19,000 actions (files, songs, folders, the home,
  next, resizing, full screen). No hang from Cue: the stops over 3 s were Wine's own
  `DestroyWindow` wait on a child window of another thread and the engine drawing slowly with
  llvmpipe. **One crash, once** (seed 208): an access violation reading 0 at the start of a
  `std::thread` (`execute_native_thread_routine+6`); the same seed again and 30 more minutes: not
  reproduced. Left open, not changed.
- Memory and handles (Wine, 13–16 min sessions): the Windows side (Cue, engine, ffmpeg)
  175 → 197 MB and flat for the last 7 min; threads 24–25, stable; +2 handles a round, the COM
  RPC pipe Wine keeps per apartment (every `CoInitializeEx` has its `CoUninitialize`).
- Messages per second on the UI thread (Wine, relay): home at rest 18.7, film paused 6.9, song
  paused 7.6, paused and minimised 10.5; a film minimised 26.3 → 10.7; a film playing with no
  ambient light and nothing moving: poll 30.3 → 10.0/s.
- The hang on Windows when the engine's window was destroyed with the keyboard focus in it:
  confirmed on the author's PC with `focusdeadlock.exe` (focus in it: stuck 5 s; mouse capture
  only: fine) and fixed (the click on the video no longer moves the focus).
- Slow disk simulator (`tools/slow-disk/`, an `LD_PRELOAD` under Wine: 14 ms a seek, 25 MB/s, a
  drive that sleeps after 30 s and wakes in 2.5 s): no stop after a pause on an external drive
  (it stopped 1.95 s); opening with Cue closed from a sleeping drive: the window held 2645 → 349 ms;
  a disk at 5 s a query with Cue open: ~20 s → 156 ms at worst; the latest choice wins.
- `tools/audit_music.cpp` (~26,000 compositions of the music player) and `tools/audit_menus`
  (1,472 menus, 8 languages, 4 sizes, 100 and 175%): no ink outside the window, no layers over one
  another, no cut words after the fixes. Left as it is: the list's bar in German and Japanese at
  the large sizes still shortens "+ Ordner" / "+ フォルダー" (a shorter text would be needed).
- Ambient light, measured before and after under Wine (borders recorded at 60 fps,
  `tools/ambient_measure.sh`, `tools/ambient_analyze.py`, `tools/benchmark_reveal.cpp`): the same
  frames read; no darkening in the middle of a change; the halo's paints about the same.
- The release files: `tools/verify_release.py` on the installer (every file against `build/Cue`,
  the engine's manifest and its sources), the engine source archive byte for byte the 1.4 one
  (the same engine as 1.3.2), the installer run in Wine, **about Cue** showing v1.4 in the GitHub
  edition; the Store layout built with `STORE=1` (`cue-edition:microsoft-store`).
- Only Windows shows (not measured here): the sound coming back when headphones are unplugged or
  plugged in (no audio hardware here), the taskbar thumbnail's buttons in the light theme (Wine
  has no taskbar), frame pacing on a real GPU, dragging the window's edge live (Wine shows isolated
  black frames from the engine recreating its surface; a plain GDI window does not).

## Review of the redesign: Ogg, HDR, borders, speed (2026-11-14)

- `tools/test.sh` passed (with the new `ogg_kind_test` and the new checks in `raster_test`,
  `cover_art_test` and `home_continue_test`); `tools/sanitize.sh` (ASan, LSan, UBSan) and
  `tools/fuzz.sh 4`: clean. `tools/wine-tests.sh`: all 115 Windows builds passed.
- Against the shipped engine under Wine: `tests/ogg_engine_test.cpp` (new): Vorbis, Opus and FLAC
  in Ogg play and their cover (METADATA_BLOCK_PICTURE) is the engine's album art, and Cue's own
  reader finds it; a Theora film named .ogg is a film for the engine and for `ogg_kind`.
  `tests/hdr_engine_test.cpp` (new): HDR10 and HLG are named (BT.2020, PQ / HLG, peak, MaxCLL);
  the preview helper's pictures of them are washed out (HDR10: saturation -56%, luma -13%; HLG:
  saturation -41% against the same picture in SDR): it receives PQ/HLG with no tone mapping. A
  prototype in the test (16-bit read, tone mapped) brings the saturation back (0.94 against 0.97).
- The engine's libplacebo is built without Dolby Vision (`--auto-features disabled`: `dovi` off,
  `PL_HAVE_DOVI` undefined), so mpv ignores the RPU: profile 5 cannot show its real colours;
  8.1/8.4 play their HDR10/HLG base; 7 its base layer. Not changed (an engine rebuild).
- The real Cue (this build) under Wine and Xvfb: the welcome, a Theora .ogg playing as a film, Vorbis,
  Opus and FLAC-in-Ogg songs with their covers, and the home listing the Theora file with its frame
  ("watched") and the songs with their covers; `cue.log` writes the new "song open" lines.
- Portable benchmarks (`tools/benchmark_redesign.cpp`, new; `tools/benchmark_frames.cpp`), the
  same pixels before and after: the home at 4K 3.05 -> 2.34 ms a frame (pointer on the one to
  continue 2.92 -> 1.85; the list 2.36 -> 1.51), the welcome 4.17/2.93/3.05 -> 3.44/2.30/2.46,
  what's new at rest 3.56 -> 2.70, the home fading in from a film 8.36 -> 7.14.
- Borders at half scales: `raster_test` draws a panel and a switch at 1, 1.5, 2, 2.5, 3, 3.5 and
  4.5 px per art px at odd and even places and checks the same thickness on every side (it fails
  on the previous drawing: 2/1 px at 1.5).
- Only Windows shows: the real HDR display, a song's opening times on the author's PC (the new
  `cue.log` lines), the borders in the real window.

## The redesign: home, welcome, what's new, panels (2026-11-13)

Reviewed by the author part by part, on his PC, from test builds of the real Cue (the welcome,
what's new, the one panel, the home's **continue**, the subtitle's dimming and style, the
switches' light, **show pictures** and its switch).

- `tools/test.sh` (112 programs) passed, with the new `home_continue_test`, `news_view_test` and
  `subtitle_dim_test`; `onboarding_test` and `i18n_layout_test` over 8 languages, 4 sizes and
  10 windows. The touched views' tests and `tools/fuzz_views.cpp` also passed under
  AddressSanitizer, LeakSanitizer and UBSan.
- Cross-compiled with MinGW-w64 (`make OFFICIAL=1`, `make OFFICIAL=1 setup`, `make msix`);
  `verify_release.py` checked the installer against the build and the engine's sources.
- Not run here: Wine was not available for this round, so `tools/wine-tests.sh` did not run.
  What only Windows draws was seen on the author's PC, not measured here: the window's real glow
  in the welcome, the style's sample in the real font (GDI), the subtitle dimmed inside the
  engine, and covers read already reduced by WIC (`IWICBitmapSourceTransform`; any other format
  falls back to the full read). `TESTES-NO-PC\2-TESTES-NATIVOS.bat` runs
  `recent_thumbs_win_test`, whose JPEGs go through the same reader, on a real PC.

## A film opened with Cue closed (2026-11-12, night)

- The author's screen recording: the window appeared in the home's 16:9 (the home drawn in it for
  a few frames), went black, vanished for a frame and came back in the film's shape.
- The same in the real Cue under Wine, recorded at 30 frames a second (`x11grab`), 1.4 before
  and after the change: a 2.39:1 MKV and a 9:16 MP4 now open in their shape from the first frame
  with nothing of the home drawn; an MPEG-TS (no header read) opens in 16:9 without the home,
  then takes its shape as before. The log says `window in the film's shape from its header`.
- The header reader against ffprobe's display aspect on real files (ffmpeg: plain, 2.39:1,
  anamorphic in MKV and MP4, a phone video turned in its matrix, 9:16, WebM, a 4K HEVC MP4):
  the same shape for every one. `media_header_test` (built files, every truncation and 40,000
  corruptions, also under AddressSanitizer and UBSan).
- `tools/test.sh` (109 programs) and `tools/wine-tests.sh` (111 Windows builds) passed.

## The home's Recent list, with less text (2026-11-12, evening)

Option A of three shown to the author (pictures from Cue's own drawing code with his recents).

- `recents_view_test`: titles as the home shows them, including what must stay (*Song (Live at
  Wembley)*, *Film (2019)*, *Song (Official Video)*, *Abbaesque - Thing* by ABBA, a title that
  would come out empty); also under AddressSanitizer and UBSan.
- The real Cue under Wine with six recents like the author's (songs tagged with their artist, a
  film and an episode left halfway, one entry from yesterday): *Music Baby / Jane Remover*,
  *Crimson / Hunnygloss*, *DNA / Number Teddie · ontem*, *RuPaul's Drag Race / S18E15* with
  *faltam 1 min*, the film's title in the middle of its entry beside *faltam 2 min*; no song
  lengths, no *hoje*.
- `tools/test.sh` (108 programs) and `tools/wine-tests.sh` (110 Windows builds) passed.

## The phone remote's side buttons and the timeline previews (2026-11-12, later)

Same environment. What changed is in CHANGELOG.md (1.4: the first three entries).

- **The phone's « in a queue**, with the real Cue under Wine and the real page in Chromium (a
  phone paired through state.json): from a fresh page on the third song, « went to the second
  song and the next « paused it (`queue-play` of the playing song); after the change « asks the
  PC for its previous file (`prev`): third, second, first, then nothing; » the same way; a film
  in up-next mode back to the song before it, at once. `tests/remote_side_buttons_browser.cjs`
  fails on the page before the change (its first «) and passes after; `remote_test` covers the
  command and the state's `prev`.
- **The wheel**: music has time and volume; in volume mode « and » send the volume in steps of 5
  (0 to 125), for films and songs, checked in the real Cue (100 → 95 → 90 → 95 → 100 → 105 on a
  film, the film's time running on). `remote_ring_browser.cjs` updated: up-next mode is a film's.
- **Timeline previews**, with the real helper (`--thumb-host`) and the shipped engine under Wine,
  76 requests per file (16 jumps, a 30 s sweep right, a 30 s sweep left), the build before
  against the build after: the same time and the same pixels (hashed) for every answer, on
  H.264 and HEVC 10-bit MKV, MP4 with B-frames (an edit list's shift found), MKV, WebM VP9 and
  MPEG-TS (no index: answers from the engine's own only). Decodes in the two sweeps, before →
  after: 4K HEVC MKV and MP4 60 → 0 (the jumps had decoded those keyframes), 1080p HEVC 60 → 0,
  1080p H.264 MKV 60 → 12, VP9 60 → 2, TS 60 → 36. Decoder threads, the same 76 requests:
  slice threading for HEVC 1080p 66 → 54 ms and 4K 170 → 140 ms a request (median, 2 threads);
  H.264 kept frame threading (55 against 68 ms).
- In the real Cue, the same pointer path over the timeline (a slow sweep, a fast sweep back,
  small moves) with `--log`: pictures shown 90 → 135 (H.264 1080p), 86 → 157 (HEVC 1080p), 48 →
  129 (HEVC 4K); the helper busy with the timeline 4.5 → 2.3 s, 4.8 → 1.3 s, 7.2 → 2.2 s.
- `tests/keyframe_index_test.cpp`: Matroska and MP4 files built byte by byte, every truncation
  and 40,000 random corruptions (no read past the file, under AddressSanitizer and UBSan too),
  the span rule, calibration, trust and dropping.
- `tools/test.sh`: 108 C++ test programs and the JavaScript checks passed. `tools/wine-tests.sh`:
  the 110 Windows builds passed. The 12 phone-page browser tests passed.

## Second round (2026-11-12)

Same environment (Linux x86_64, two cores, GCC 13.3, MinGW-w64 GCC 13.2, Wine 9.0, Chromium,
Node 22). What changed is in PERFORMANCE-REVIEW.md, "second round".

- `tools/test.sh`: all 107 C++ test programs, the translation and phone-page checks and the
  JavaScript regressions passed. New: `colour_pace_test`, `poll_pace_test`, `home_partial_test`
  (36 random sessions, 32,400 frames, each drawn the way the app does and compared pixel for
  pixel with a whole new frame), `helper_rest_test`, `remote_idle_test`, `system_events_test`,
  `audio_add_test`;
  extended: `media_geometry_test` (`demux_shape`), `episode_advance_test` (`stopped_short`);
  `smooth_playback_test` now finds the poll's pace in `set_poll`.
- `tools/wine-tests.sh`: the 109 Windows builds of the portable tests passed under Wine.
- AddressSanitizer, LeakSanitizer and UBSan on the new and touched tests (the canvas clip,
  the home's partial frames, the raster, the paces): clean.
- The phone remote's browser tests and `tests/remote_e2e_harness.cjs` against the real server
  (with its new loopback bell) under Wine: passed.
- The real Cue with the shipped engine under Wine, the build of the previous delivery against
  this one: a film's picture opening at its first frame (5 and 7 runs), a file cut short keeping
  its place, the colours' pace on long and short GOPs, the window's wake-ups paused, on a still
  home and behind another window (a counting build, not shipped), the preview helper ending
  after a minute idle and starting again with the next film, the remote's server idle and its
  stop.
- `benchmark_frames`: the home's final pixels hash the same as before in every case.
- **Load audio track**, in the real Cue and in `tracks_engine_test` against the shipped engine:
  a sound longer than the film kept it on its last frame to 00:20 of 00:12 before, now the film
  ends at 00:12; the same file chosen twice is selected instead of listed twice; a file that is
  not audio is refused and said so; a sound added to one film is gone with the next file.

Not reproducible here, checked by reading and by source tests: cloud-only songs on the phone and
in "up next", full screen after a display change, the place written on sleep. Steps for the
author's PC are in the delivery's report.

## First round

Date: 2026-11-11. Environment: Linux x86_64 (two cores), GCC 13.3, MinGW-w64 GCC 13.2,
Wine 9.0, Chromium (Playwright), Node 22, Python 3.12. Input: the 1.3.2 package on the
author's PC (GitHub `main` plus the Store edition). The engine DLL and its source ZIP are
unchanged (SHA-256 below).

## Portable tests

- `tools/test.sh`: all 98 C++ test programs, the translation and phone-page checks and the
  JavaScript regressions passed. New: `remote_auth_test` (SHA-256/HMAC against FIPS and
  RFC 4231 vectors and 200 Python digests, the protocol's values, header parsing, replay window),
  `remote_auth_page_test.cjs` (the page's own SHA-256/HMAC against Node's crypto for every
  length 0–300 and keys of 0–129 bytes), the store's sealing, migration and stable saves in
  `remote_test`, allowed update hosts in `update_test`, and the exact fast blending in
  `raster_test` (8.4 million source/destination pairs).
- `tools/sanitize.sh` with AddressSanitizer, UndefinedBehaviorSanitizer **and LeakSanitizer**
  (`detect_leaks=1`; the 1.3.2 environment could not run LeakSanitizer): every test clean.
- `tools/wine-tests.sh` (new): the same 98 programs built for Windows and run under Wine, so the
  `_WIN32` paths run too (BCrypt, DPAPI, `ReplaceFileW`, wide paths). It found one real bug,
  fixed in 1.4: MinGW's `std::filesystem::copy_file` refuses to overwrite on Windows, so the
  settings backup was never refreshed and every unchanged save reported a failure.
- `tools/fuzz.sh` (new): random input under AddressSanitizer and UBSan. The player and the home
  with NaN, infinite, negative and huge times and volumes, broken UTF-8, 0–8 tracks, the mini
  player, full screen and every interface size, at sizes from 1×1 to 1920×1080 (about 1,100
  rounds of 400 events); the news, remote-pairing and welcome screens and "up next" at any
  size and scale (160 rounds); and everything the network can send the remote, mutated: HTTP
  requests, WebSocket frames, JSON commands, the signed header, file names for series and
  listings (1.5 million iterations). It found one real bug, fixed in 1.4: "up next" froze Cue
  in a window under 200 px tall at 100% (a dotted box drawn with a 0 px step). Nothing else.
- Chapters (`chapters_test`): breaks on the timeline, the preview's name, Page Up / Page Down
  (the 3 s rule, presses in a row, other jumps in between, the first and last chapters, files
  with one chapter or none), the "…" row only with chapters, the list (marked, following
  playback, click, Esc, a new file without chapters, 120 chapters scrolling to the one playing)
  and both menus whole (more than 300 cases) across every language, interface size, film and song.
  The shortcut sheet holds the new entry in all 480 of its windows (`shortcuts_test`).
  The preview names the chapter under the pointer even when its picture comes from a keyframe
  in another chapter; the Settings choice is saved and offered for films and songs; an open
  menu keeps Page Up / Page Down; the list closes when the window becomes the mini player.
- Settings' "i" (`settings_morph_test`): in 14 windows and scales, both ways, the corner the
  menu hangs from does not move by one pixel, the box only grows or only shrinks, and the
  glide's last frame is the menu itself; no glide with reduced motion.
- Settings' short list (`settings_examples_test`): for a film and a song, Settings shows one
  **interface** row naming the size and background in use; it opens the interface's page, which
  stays open across sizes. The page itself, in 180 windows, scales, languages, films and songs,
  with the examples on and off: the same width and place as Settings (and back), the picture where
  Settings had it and the size's first, two groups each with its icon and none named like the
  page. `ui_scale_test`, `free_controls_test`, `about_test` keep the page's
  choices working.
  `tools/fuzz_play.cpp` adds random chapter lists (empty, unnamed, zero-length, 140 long,
  changing mid-play) and Page Up / Page Down.
- `clang-tidy` (clang-analyzer and bugprone checks) over every source file, Windows code
  included (MinGW headers): no real defect; its leak reports are messages handed to
  `PostMessageW`, freed by their receiver or deleted when posting fails.

The 1.3.2 package as received failed one test (`bar_place_test`): it read `platform.cpp` for a
sequence the Store edition had moved inside `#if`. The test now checks the same order in the
new shape.

## Browser and end-to-end

- The eleven optional Chromium tests (`tests/remote_*_browser.cjs`, `remote_memory.cjs`) ran and
  passed (they could not run for 1.3.2), updated to the challenge and to signed requests, which
  their fake servers verify with Node's crypto.
- `tests/remote_audio_browser.cjs`: music's centre shows the 7x7 note, the icon and its mode
  dots are centred together in the button (8 px apart), and play/pause is as tall as «.
- `tests/remote_auth_browser.cjs` (new): file and folder names with apostrophes, brackets,
  accents, CJK and emoji are signed exactly as the browser sends them; replayed, moved and
  forged requests are refused.
- `tests/remote_e2e_harness.cjs` (new): the real Windows server (`tools/remote_harness.cpp`,
  under Wine) with the real page: pairing by the link's code, the challenge answered without
  the credential, signed folder and thumbnail requests accepted by the C++ server, replay /
  moved / unsigned / Bearer refused, a wrong proof closed with 4001, the derived session key
  accepted and ended with its connection, and a real 1.3.2 page reloading into 1.4 while
  staying paired.

## Engine (shipped DLL, under Wine)

`tests/tracks_engine_test.cpp` (new): `audio-add <file> auto <title>` adds an external track
without switching, lists its file and title (what Cue matches to choose it), plays it when
chosen, and reports a missing file as a failed command; chapter lists from MKV and MP4 carry
their titles and times, and chapter jumps and steps work.

## Builds

- Windows x64 application, GitHub and Store editions, and the installer stub cross-compiled
  without warnings.
- `tools/verify_release.py`: installer SHA-256, LZMA, CRC32, 107 payload files, staged bytes,
  DLL manifest and the 16 source archives of `Cue-1.4-Engine-Source.zip` (the 1.3.2 ZIP,
  byte for byte) verified.
- Store layout (`make msix`): the manifest is 1.3.2's with version 1.4.0.0 (identity, publisher,
  display name, architecture, file types, firewall rule, capabilities unchanged); the same 264
  files; `Cue.exe` carries `cue-edition:microsoft-store` and no GitHub update address.

Engine DLL SHA-256: `7bf665b49e5f60473c7338e3aebb6975a5b854129b47bb8f393ca78656277418`.
Engine source ZIP SHA-256: `6ce73f8f9b7209e390f45c16f6ec0fbdfab383eddb7a7726656f1f24b4fcb13e`.

## Pending on a real Windows PC and phone

MakePri/MakeAppx packing and the signed test install of the Store package, the native test
executables (DPAPI and CNG on real Windows), resource use while playing (CPU, memory, GPU),
the GPU path, a real phone on real Wi-Fi, and a look at the new "load audio track" rows and the chapters with a real film.
Wine and Chromium in a container are not Windows and a phone.
