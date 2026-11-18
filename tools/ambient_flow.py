# (Cue, 17/11) Does the halo flow? From an ambient_analyze.py .npz: the light's speed over time (OKLab per second,
# top band and left band), how much of the time it stands still, and how even its speed is.
import sys, numpy as np
def oklab(c):
    c = c/255.0; l = np.where(c <= 0.04045, c/12.92, ((c+0.055)/1.055)**2.4)
    M1 = np.array([[0.4122214708,0.5363325363,0.0514459929],[0.2119034982,0.6806995451,0.1073969566],[0.0883024619,0.2817188376,0.6299787005]])
    M2 = np.array([[0.2104542553,0.7936177850,-0.0040720468],[1.9779984951,-2.4285922050,0.4505937099],[0.0259040371,0.7827717662,-0.8086757660]])
    return np.cbrt(l @ M1.T) @ M2.T
d = np.load(sys.argv[1]); t = d['t']; t0, t1 = float(sys.argv[2]), float(sys.argv[3])
m = (t >= t0) & (t < t1)
for name in ('H', 'S'):
    X = oklab(d[name][m].astype(float)); tt = t[m]
    # speed over 0.25 s windows (the light is painted at ~20-30 Hz: shorter windows only see the steps)
    w = 15
    sp = np.linalg.norm(X[w:] - X[:-w], axis=1) / (w/60)
    still = float((sp < 0.004).mean())   # under ~1 step of 8 bits in a quarter second
    print(f"{name}: still {still*100:.0f}% of the time | speed mean {sp.mean():.3f}/s, p90 {np.percentile(sp,90):.3f}/s, max {sp.max():.3f}/s | evenness (std/mean) {sp.std()/max(1e-9,sp.mean()):.2f}")
