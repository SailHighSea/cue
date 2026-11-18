import re,bisect,sys
rows=[]
for l in open('/tmp/syms.txt'):
    p=l.split(' ',2)
    try: rows.append((int(p[0],16),p[2].strip()))
    except: pass
rows.sort(); keys=[r[0] for r in rows]
th=None; ui=[]; dw=False; first=None
for l in open(sys.argv[1],errors='replace'):
    if 'Backtracing' in l:
        th=l.split()[3]
        if first is None and 'Cue.exe' in l: first=th
        continue
    if 'libmpv-2 (+0x102c22)' in l: dw=True
    if th==first:
        m=re.search(r' (0x0*14[0-9a-f]{7}) in cue',l)
        if m:
            a=int(m.group(1),16); i=bisect.bisect_right(keys,a)-1; ui.append(rows[i][1][:60])
print('  UI thread:', ' < '.join(ui[:4]), '| engine window in DestroyWindow:', dw)
