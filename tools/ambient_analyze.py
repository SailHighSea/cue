# (Cue, 17/11) the halo measured in Wine: tools/ambient_measure.sh records, this reads.
#   python3 tools/ambient_analyze.py rec.mkv out.npz
# Reads a recording of the halo (ambient_measure.sh) and reports, for each cut of the test video:
# how long the light takes (from the cut in the picture) to start/half/settle, the per-frame step
# (the biggest jump between two frames), and the light's brightness on the way (a dip = a sRGB mix).
import sys, json, subprocess, numpy as np
path = sys.argv[1]
W, H = 1920, 1080
def frames(path):
    p = subprocess.Popen(['ffmpeg','-loglevel','error','-i',path,'-f','rawvideo','-pix_fmt','rgb24','-'], stdout=subprocess.PIPE)
    n = W*H*3
    while True:
        b = p.stdout.read(n)
        if len(b) < n: break
        yield np.frombuffer(b, np.uint8).reshape(H, W, 3)
def lin(c):
    c = c/255.0
    return np.where(c <= 0.04045, c/12.92, ((c+0.055)/1.055)**2.4)
rect = None; vid = []; halo = []; side = []
for f in frames(path):
    if rect is None:
        m = f.max(axis=2) > 20
        ys, xs = np.where(m[:, :])
        # the window: the big bright block (rows/cols mostly lit)
        rows = np.where(m.sum(axis=1) > W*0.4)[0]; cols = np.where(m.sum(axis=0) > H*0.4)[0]
        if len(rows) < 10 or len(cols) < 10: vid.append(None); halo.append(None); side.append(None); continue
        rect = (cols[0], rows[0], cols[-1], rows[-1])
        print('window', rect, file=sys.stderr)
    x0, y0, x1, y1 = rect
    cx, cy = (x0+x1)//2, (y0+y1)//2
    vid.append(f[cy-20:cy+20, cx-20:cx+20].reshape(-1,3).mean(0))
    band = f[y0-14:y0-8, x0+(x1-x0)//4: x1-(x1-x0)//4].reshape(-1,3).astype(float)
    halo.append(band.mean(0))
    side.append(f[cy-60:cy+60, x0-14:x0-8].reshape(-1,3).astype(float).mean(0))
fps = 60.0
ok = [i for i,v in enumerate(vid) if v is not None]
V = np.array([vid[i] for i in ok]); Hh = np.array([halo[i] for i in ok]); t = np.array(ok)/fps
# cuts in the picture
dv = np.abs(np.diff(V, axis=0)).max(1)
cuts = [i+1 for i in np.where(dv > 40)[0]]
cuts = [c for j,c in enumerate(cuts) if j == 0 or c - cuts[j-1] > 30]
out = []
for j, c in enumerate(cuts):
    end = cuts[j+1] if j+1 < len(cuts) else len(t)
    a = Hh[max(0,c-3)]; b = Hh[end-3]
    d = np.linalg.norm(b - a)
    if d < 3: continue
    mv = np.where(np.abs(Hh[c:end] - a).max(1) >= 2)[0]
    if not len(mv): continue
    s0 = c + max(0, mv[0] - 1)
    lag = round((s0 - c)/fps, 3)
    seg = Hh[s0:end]
    prog = np.clip(((seg - a) @ (b - a)) / (d*d), -1, 2)
    def when(p):
        k = np.where(prog >= p)[0]
        return round(k[0]/fps, 3) if len(k) else None
    steps = np.abs(np.diff(Hh[s0:end], axis=0)).max(1)
    moving = steps[steps > 0]
    L = (lin(seg) @ np.array([0.2126, 0.7152, 0.0722]))
    La, Lb = L[0], L[-1]
    i10, i90 = when(0.1), when(0.9)
    mid = L[int((i10 or 0)*fps):int((i90 or 0)*fps)+1] if i10 is not None and i90 is not None else L
    dip = float(mid.min() / max(1e-6, min(La, Lb))) if len(mid) else None
    # frames where it moved, per second of the change (how many distinct updates)
    upd = int((steps[int((i10 or 0)*fps):int((i90 or 1)*fps)+1] > 0).sum()) if i10 is not None and i90 is not None else None
    out.append(dict(cut=round(t[c],2), lag=lag, frm=[int(x) for x in a], to=[int(x) for x in b], first4=[int(x) for x in steps[:8:2]], half=when(0.5), settle90=i90, t10_90=round(i90-i10,3) if i90 and i10 is not None else None,
                    max_step=int(steps.max()), updates_10_90=upd, dip=round(dip,3) if dip else None))
for o in out: print(json.dumps(o))
np.savez(sys.argv[2] if len(sys.argv) > 2 else '/tmp/meas.npz', t=t, V=V, H=Hh, S=np.array([side[i] for i in ok]), rect=np.array(rect))
