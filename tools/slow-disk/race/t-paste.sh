#!/bin/bash
# Ctrl+V of a path copied as text, on a disk taking 3 s per look-up: the window keeps answering, the
# file opens when the disk answers; text that is not a file does nothing.
cd "$(dirname "$0")"; export WINEPREFIX=/home/claude/wp WINEDEBUG=-all DISPLAY=:9; ok=1
SLOW=1 ./fresh.sh 'C:\m\c2.mp4' >/dev/null
w=$(xdotool search --name 'Cue$' | while read w; do [ "$(xdotool getwindowname $w)" != Cue ] && echo $w; done | head -1)
/usr/lib/wine/wine64 'C:\setclip.exe' 'E:\c1.mp4'; echo 3000 > /tmp/slowhd.lag
xdotool windowfocus $w; xdotool key ctrl+v; sleep 0.3
t0=$(date +%s.%N); p=$(./hung.sh); t1=$(date +%s.%N); echo "window while the disk looks: $p ($(echo "($t1-$t0)*1000/1" | bc) ms)"
[ "$p" == ok ] || ok=0
sleep 14; rm -f /tmp/slowhd.lag; sleep 6; a=$(./color.sh); echo "then: $a"; [[ "$a" == *"c1.mp4 · Cue -> red" ]] || ok=0
/usr/lib/wine/wine64 'C:\setclip.exe' 'hello, not a file'
w=$(xdotool search --name 'Cue$' | while read w; do [ "$(xdotool getwindowname $w)" != Cue ] && echo $w; done | head -1)
xdotool windowfocus $w; xdotool key ctrl+v; sleep 3; b=$(./color.sh); echo "text: $b"; [[ "$b" == *"c1.mp4 · Cue -> red" ]] || ok=0
[ $ok = 1 ] && echo PASS || echo FAIL
