#!/bin/bash
# Opening paths that must not change: an Explorer selection (3 files 0.1 s apart: the first plays, the
# others follow in the list), a folder handed over, a selection on a slow disk.
cd "$(dirname "$0")"; ok=1
SLOW=1 ./fresh.sh 'C:\m\c1.mp4' >/dev/null
./send.sh '0:C:\m\c2.mp4' '100:C:\m\c3.mp4' '100:C:\m\c4.mp4' >/dev/null; sleep 5; a=$(./color.sh)
./send.sh '0:key:0xB0' >/dev/null; sleep 4; b=$(./color.sh)
./send.sh '0:key:0xB0' >/dev/null; sleep 4; c=$(./color.sh)
echo "selection: $a | next: $b | next: $c"
[[ "$a" == *"c2.mp4 · Cue -> green" && "$b" == *"c3.mp4 · Cue -> blue" && "$c" == *"c4.mp4 · Cue -> yellow" ]] || ok=0
mkdir -p /home/claude/wp/drive_c/f && cp /home/claude/wp/drive_c/m/c4.mp4 /home/claude/wp/drive_c/f/a.mp4 && cp /home/claude/wp/drive_c/m/c3.mp4 /home/claude/wp/drive_c/f/b.mp4
./send.sh '1000:C:\f' >/dev/null; sleep 5; d=$(./color.sh); ./send.sh '0:key:0xB0' >/dev/null; sleep 4; e=$(./color.sh)
echo "folder: $d | next: $e"
[[ "$d" == *"a.mp4 · Cue -> yellow" && "$e" == *"b.mp4 · Cue -> blue" ]] || ok=0
cp /home/claude/wp/drive_c/m/c2.mp4 /home/claude/slowhd/c2.mp4
echo 1500 > /tmp/slowhd.lag
./send.sh '1000:E:\c1.mp4' '100:E:\c2.mp4' >/dev/null; sleep 3; rm -f /tmp/slowhd.lag; sleep 12; f=$(./color.sh)
./send.sh '0:key:0xB0' >/dev/null; sleep 5; g=$(./color.sh)
echo "slow selection: $f | next: $g"
[[ "$f" == *"c1.mp4 · Cue -> red" && "$g" == *"c2.mp4 · Cue -> green" ]] || ok=0
[ $ok = 1 ] && echo PASS || echo FAIL
