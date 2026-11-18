#!/bin/bash
# Cue closed, a selection of two: the first opens Cue, the second follows 0.4 s later: c1 plays, c2 next.
cd "$(dirname "$0")"
WAIT=0.4 ./fresh.sh 'C:\m\c1.mp4' >/dev/null; ./open-burst.sh 0.1 'C:\m\c2.mp4'; sleep 9; a=$(./color.sh)
./send.sh '0:key:0xB0' >/dev/null; sleep 4; b=$(./color.sh)
echo "cold selection: $a | next: $b"
[[ "$a" == *"c1.mp4 · Cue -> red" && "$b" == *"c2.mp4 · Cue -> green" ]] && echo PASS || echo FAIL
