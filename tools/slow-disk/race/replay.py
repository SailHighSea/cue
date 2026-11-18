#!/usr/bin/env python3
# replay.py <monkey log> [from s] [to s] : does again what the monkey did, at the same moments
import os, re, subprocess, sys, time
log, t_from, t_to = sys.argv[1], float(sys.argv[2]) if len(sys.argv) > 2 else 0, float(sys.argv[3]) if len(sys.argv) > 3 else 1e9
env = dict(os.environ, DISPLAY=':9', WINEPREFIX='/home/claude/wp', WINEDEBUG='-all')
def sh(*a): return subprocess.run(a, env=env, capture_output=True, text=True).stdout.strip()
def win():
    for w in sh('xdotool', 'search', '--name', 'Cue').split():
        g = sh('xdotool', 'getwindowgeometry', '--shell', w)
        d = dict(l.split('=') for l in g.splitlines() if '=' in l)
        if int(d.get('WIDTH', 0)) > 100: return w, int(d['X']), int(d['Y'])
acts = []
for l in open(log):
    m = re.match(r'\s*([\d.]+) (\w+) ?(.*)', l)
    if m and t_from <= float(m.group(1)) <= t_to: acts.append((float(m.group(1)), m.group(2), m.group(3)))
t0 = time.time() - (acts[0][0] if acts else 0)
for at, kind, rest in acts:
    while time.time() - t0 < at: time.sleep(0.002)
    W = win()
    if not W: continue
    w, x, y = W
    if kind == 'keys':
        sh('xdotool', 'windowfocus', w); sh('xdotool', 'key', '--delay', '0', *rest.split())
    elif kind == 'click':
        m = re.match(r'b(\d)x(\d) at (\d+),(\d+)', rest)
        sh('xdotool', 'mousemove', str(x + int(m.group(3))), str(y + int(m.group(4))), 'click', '--repeat', m.group(2), '--delay', '30', m.group(1))
    elif kind == 'wheel':
        m = re.match(r'at (\d+),(\d+)', rest)
        sh('xdotool', 'mousemove', str(x + int(m.group(1))), str(y + int(m.group(2))), 'click', '5')
    elif kind == 'open':
        sh('/usr/lib/wine/wine64', r'C:\sendopen.exe', *rest.split())
    print(f'{time.time()-t0:7.2f} {kind} {rest}', flush=True)
