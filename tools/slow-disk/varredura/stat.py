# stat.py <pid> <secs>: CPU% and context switches (wakeups) per second of a process, all threads
import sys, os, time
pid=int(sys.argv[1]); secs=float(sys.argv[2]); hz=os.sysconf('SC_CLK_TCK')
def snap():
    cpu=0; sw=0
    for t in os.listdir(f'/proc/{pid}/task'):
        try:
            s=open(f'/proc/{pid}/task/{t}/stat').read().rsplit(')',1)[1].split()
            cpu+=int(s[11])+int(s[12])
            for l in open(f'/proc/{pid}/task/{t}/status'):
                if 'ctxt_switches' in l: sw+=int(l.split()[1])
        except Exception: pass
    rss=0
    for l in open(f'/proc/{pid}/status'):
        if l.startswith('VmRSS'): rss=int(l.split()[1])
    io=dict(l.split(': ') for l in open(f'/proc/{pid}/io'))
    return cpu,sw,rss,int(io['rchar']),int(io['syscr'])
a=snap(); t=time.time(); time.sleep(secs); b=snap(); d=time.time()-t
print(f"cpu {100*(b[0]-a[0])/hz/d:.2f}%  wakeups {(b[1]-a[1])/d:.0f}/s  rss {b[2]/1024:.0f} MB  read {(b[3]-a[3])/d/1024:.0f} KB/s  readcalls {(b[4]-a[4])/d:.1f}/s  threads {len(os.listdir(f'/proc/{pid}/task'))}")
