#!/bin/bash
# A file on a slow disk (3 s per operation) and, 1 s later, one on a quick disk: the second (the last
# choice) must play within a few seconds, and still be playing when the slow disk answers.
cd "$(dirname "$0")"
SLOW=1 ./fresh.sh 'C:\m\c2.mp4' >/dev/null
echo 3000 > /tmp/slowhd.lag
./send.sh '0:E:\c1.mp4' '1000:C:\m\c3.mp4' >/dev/null
sleep 6; a=$(./color.sh)
sleep 24; rm -f /tmp/slowhd.lag; sleep 8; b=$(./color.sh)
echo "after 6 s: $a | after the disk answered: $b"
[[ "$a" == *"c3.mp4 · Cue -> blue" && "$b" == *"c3.mp4 · Cue -> blue" ]] && echo PASS || echo FAIL
