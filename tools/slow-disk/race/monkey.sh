#!/bin/bash
# monkey.sh <seconds> <seed> : the monkey over a fresh Cue (reopened when it closes); stops at a hang
cd "$(dirname "$0")"; export WINEPREFIX=/home/claude/wp WINEDEBUG=-all DISPLAY=:9
L=/home/claude/wp/drive_c/users/root/AppData/Roaming/tinypeep/Cue/cue.log
WIPE=1 ./fresh.sh 'C:\m\c1.mp4' >/dev/null; rm -f $L
python3 monkey.py $1 $2 > /tmp/monkey-$2.txt 2>&1
echo "seed $2: $(tail -2 /tmp/monkey-$2.txt | tr '\n' ' ') closes: $(grep -c 'Cue again' /tmp/monkey-$2.txt)"
grep -iE 'crash|out of memory|terminate' $L | head -5
[ -z "$KEEP" ] && wineserver -k
true
