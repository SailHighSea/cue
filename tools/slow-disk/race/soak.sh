#!/bin/bash
# soak.sh <seed from> <count> <seconds> : monkeys until one hangs (Cue left hung for the debugger)
cd "$(dirname "$0")"
for s in $(seq $1 $(( $1 + $2 - 1 ))); do
  KEEP=1 ./monkey.sh $3 $s >> /tmp/soak.txt 2>&1
  grep -q HUNG /tmp/monkey-$s.txt && { echo "HUNG seed $s" >> /tmp/soak.txt; exit; }
  /usr/bin/wineserver -k 2>/dev/null; WINEPREFIX=/home/claude/wp wineserver -k
done
echo done >> /tmp/soak.txt
