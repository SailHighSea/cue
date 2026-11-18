import sys,re,bisect,collections
rows=[]
import os
for l in open(os.environ.get('SYMS','/tmp/syms.txt')):
    p=l.split(' ',2)
    try: rows.append((int(p[0],16),p[2].strip()))
    except: pass
rows.sort(); addrs=[r[0] for r in rows]
def sym(a):
    if not (0x140000000<=a<0x150000000): return None
    i=bisect.bisect_right(addrs,a)-1
    return rows[i][1][:90]
ui=collections.Counter(); calls=[]
pat=re.compile(r'^([0-9a-f]{4}):Call (\S+)\((.*)\) ret=([0-9a-f]+)')
for l in open(sys.argv[1],errors='replace'):
    m=pat.match(l)
    if not m: continue
    tid,fn,args,ret=m.groups()
    if fn.endswith('DispatchMessageW'): ui[tid]+=1; continue
    calls.append((tid,fn,args,int(ret,16)))
uit=ui.most_common(1)[0][0] if ui else None
print("UI thread:",uit, "threads seen:", len(set(c[0] for c in calls)))
skip=(r'C:\Cue',r'C:\windows',r'\??\C:\windows') + (() if len(sys.argv)>2 else (r'C:\users\root\AppData',))
agg=collections.Counter(); ex={}
for tid,fn,args,ret in calls:
    if tid!=uit: continue
    s=sym(ret)
    if s is None: continue   # called from inside another DLL
    path=re.findall(r'L"([^"]*)"',args)
    p=(path[0] if path else args[:40]).replace('\\\\','\\')
    if any(p.lower().startswith(x.lower()) for x in skip) and 'Pictures' not in p and 'Imagens' not in p: continue
    key=(fn.split('.')[-1],s); agg[key]+=1; ex.setdefault(key,p)
for (fn,s),n in agg.most_common():
    print(f"{n:5d}  {fn:24s} {s}   e.g. {ex[(fn,s)][:80]}")
