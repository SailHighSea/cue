#!/bin/bash
# soak2.sh <seed from> <count> <seconds>: like soak.sh, but a hang is recorded (all threads' stacks) and the next seed goes on
cd /home/claude/src/01-REPOSITORIO/tools/slow-disk/race
export WINEDEBUG=-all WINEPREFIX=/home/claude/wp DISPLAY=:9
for s in $(seq $1 $(( $1 + $2 - 1 ))); do
  KEEP=1 ./monkey.sh $3 $s >> /tmp/soak2.txt 2>&1
  if grep -q HUNG /tmp/monkey-$s.txt; then
    wpid=$(/usr/lib/wine/wine64 tasklist 2>/dev/null | grep -i "^cue.exe" | awk '{print $2}' | head -1)
    printf 'attach 0x%x\nbt all\ndetach\nquit\n' $wpid > /tmp/dbg.cmd
    timeout 90 /usr/lib/wine/wine64 winedbg --file /tmp/dbg.cmd > /tmp/hang-$s.txt 2>&1
    python3 /tmp/hangmap.py /tmp/hang-$s.txt >> /tmp/soak2.txt
    cp /home/claude/wp/drive_c/users/root/AppData/Roaming/tinypeep/Cue/cue.log /tmp/hang-$s.log
  fi
  wineserver -k 2>/dev/null; sleep 1
done
echo done >> /tmp/soak2.txt
