#!/bin/bash
# bt.sh: backtrace of Cue's UI thread (first thread), symbols from /tmp/syms.txt
export WINEDEBUG=-all WINEPREFIX=/home/claude/wp DISPLAY=:9
wpid=$(/usr/lib/wine/wine64 tasklist 2>/dev/null | grep -i "^cue.exe" | awk '{print $2}' | head -1)
printf 'attach 0x%x\nbt all\ndetach\nquit\n' $wpid > /tmp/dbg.cmd
timeout 60 /usr/lib/wine/wine64 winedbg --file /tmp/dbg.cmd 2>&1 | awk '/Backtracing for thread/{n++; p= ($0 ~ /Cue.exe/) && n2<1; if(p) n2++} p' | grep -E "^\s*=?>?[0-9]+ 0x" | while read -r l; do
  a=$(echo "$l" | grep -o "0x0000014[0-9a-f]*" | head -1)
  if [ -n "$a" ]; then python3 - "$a" <<'PY'
import sys,bisect
a=int(sys.argv[1],16); rows=[]
for l in open('/tmp/syms.txt'):
    p=l.split(' ',2)
    try: rows.append((int(p[0],16),p[2].strip()))
    except: pass
rows.sort(); i=bisect.bisect_right([r[0] for r in rows],a)-1
print('   ', rows[i][1][:120], '+',hex(a-rows[i][0]))
PY
  else echo "$l" | cut -c1-160; fi
done
