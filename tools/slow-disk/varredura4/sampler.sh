#!/bin/bash
# sampler.sh <out>: every 40 s, the wp2 main Cue's memory split (Windows side / native side)
while true; do
  for p in $(pgrep -f 'Cue.exe'); do
    tr '\0' '\n' < /proc/$p/environ 2>/dev/null | grep -q 'WINEPREFIX=/home/claude/wp2' || continue
    tr '\0' ' ' < /proc/$p/cmdline | grep -q thumb-host && continue
    echo "$(date +%T) $(python3 /tmp/memsplit.py $p 2>/dev/null)" >> $1
  done
  sleep 40
done
