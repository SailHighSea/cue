#!/usr/bin/env python3
# monkey.py <seconds> <seed> : keys and clicks on Cue's window without pause, and files handed over in
# the middle; a log of what was done (stdout). Keys through XTEST (the focused window).
import os, random, subprocess, sys, time
secs, seed = float(sys.argv[1]), int(sys.argv[2])
random.seed(seed)
env = dict(os.environ, DISPLAY=':9', WINEPREFIX='/home/claude/wp', WINEDEBUG='-all')
def sh(*a): return subprocess.run(a, env=env, capture_output=True, text=True).stdout.strip()
def win():
    for w in sh('xdotool', 'search', '--name', 'Cue').split():
        n = sh('xdotool', 'getwindowname', w)
        g = sh('xdotool', 'getwindowgeometry', '--shell', w)
        d = dict(l.split('=') for l in g.splitlines() if '=' in l)
        if int(d.get('WIDTH', 0)) > 100: return w, int(d['X']), int(d['Y']), int(d['WIDTH']), int(d['HEIGHT'])
    return None
KEYS = ['space', 'Left', 'Right', 'Up', 'Down', 'Return', 'Escape', 'Tab', 'n', 'f', 'm', 'ctrl+m', 'Home', 'End',
        'Prior', 'Next', 'c', 's', 'j', 'k', 'l', 'a', 'v', 'shift+Left', 'shift+Right', 'ctrl+plus', 'ctrl+minus', 'ctrl+0',
        '1', '5', 'XF86AudioNext', 'XF86AudioPrev', 'XF86AudioPlay', 'period', 'comma', 'bracketleft', 'bracketright', 'BackSpace']
FILES = [r'C:\m\c1.mp4', r'C:\m\c2.mp4', r'C:\m\c3.mp4', r'C:\m\c4.mp4', r'C:\m\song1.mp3', r'C:\m\song2.mp3', r'C:\m\song3.mp3',
         r'C:\m\tall.mp4', r'C:\f', r'C:\m\film1.mp4']
import threading
hung = []
def watch():
    while time.time() - t0 < secs and not hung:
        if win() and 'HUNG' in sh('/usr/lib/wine/wine64', r'C:\ping.exe'): hung.append(time.time() - t0); break
        time.sleep(2)
t0 = time.time(); n = 0
threading.Thread(target=watch, daemon=True).start()
while time.time() - t0 < secs and not hung:
    r = random.random(); n += 1
    W = win()
    if not W:
        print(f'{time.time()-t0:7.2f} no window: Cue again', flush=True)
        subprocess.Popen(['/usr/lib/wine/wine64', r'C:\Cue\Cue.exe', '--log', r'C:\m\c1.mp4'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(4); continue
    w, x, y, ww, hh = W
    if r < 0.55:
        ks = random.choices(KEYS, k=random.randint(1, 5))
        sh('xdotool', 'windowfocus', w); sh('xdotool', 'key', '--delay', '0', *ks)
        print(f'{time.time()-t0:7.2f} keys {" ".join(ks)}')
    elif r < 0.85:
        px, py = x + random.randint(2, ww - 3), y + random.randint(2, hh - 3)
        b = random.choice(['1', '1', '1', '3']); c = random.choice(['1', '1', '2'])
        sh('xdotool', 'mousemove', str(px), str(py), 'click', '--repeat', c, '--delay', '30', b)
        print(f'{time.time()-t0:7.2f} click b{b}x{c} at {px-x},{py-y}')
    elif r < 0.9:
        px, py = x + random.randint(2, ww - 3), y + random.randint(2, hh - 3)
        sh('xdotool', 'mousemove', str(px), str(py), 'click', '--repeat', str(random.randint(1, 6)), random.choice(['4', '5']))
        print(f'{time.time()-t0:7.2f} wheel at {px-x},{py-y}')
    else:
        fs = random.choices(FILES, k=random.randint(1, 3))
        args = ['0:' + fs[0]] + [f'{random.choice([0,50,300,800])}:{f}' for f in fs[1:]]
        sh('/usr/lib/wine/wine64', r'C:\sendopen.exe', *args)
        print(f'{time.time()-t0:7.2f} open {" ".join(args)}')
    sys.stdout.flush()
    time.sleep(random.choice([0, 0, 0.02, 0.05, 0.1, 0.3]))
print(f'HUNG at {hung[0]-3:.1f} s (the ping waited 3 s)' if hung else 'no hang'); print(f'{n} actions')
