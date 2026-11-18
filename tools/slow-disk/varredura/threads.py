# threads.py <pid> <secs>: per-thread wakeups/s and CPU
import sys, os, time
pid=int(sys.argv[1]); secs=float(sys.argv[2]); hz=os.sysconf('SC_CLK_TCK')
def snap():
    r={}
    for t in os.listdir(f'/proc/{pid}/task'):
        try:
            s=open(f'/proc/{pid}/task/{t}/stat').read().rsplit(')',1)[1].split()
            name=open(f'/proc/{pid}/task/{t}/comm').read().strip()
            sw=0
            for l in open(f'/proc/{pid}/task/{t}/status'):
                if 'ctxt_switches' in l: sw+=int(l.split()[1])
            r[t]=(name,int(s[11])+int(s[12]),sw)
        except Exception: pass
    return r
a=snap(); time.sleep(secs); b=snap()
rows=[]
for t,(n,c,s) in b.items():
    if t in a: rows.append(((s-a[t][2])/secs,(c-a[t][1])/hz/secs*100,t,n))
for w,c,t,n in sorted(rows,reverse=True)[:10]: print(f"  {t:>8} {n:<16} {w:7.1f}/s cpu {c:.2f}%")
