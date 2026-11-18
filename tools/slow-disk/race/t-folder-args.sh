#!/bin/bash
# Cue started with a folder (Explorer's "open with" on a folder): its media in a list of Cue's own, in
# name order; the title is the file playing, "next" goes on, and a file still opens as before.
cd "$(dirname "$0")"; ok=1
L=/home/claude/wp/drive_c/users/root/AppData/Roaming/tinypeep/Cue/cue.log
WAIT=9 ./fresh.sh 'C:\f' 2>/dev/null; a=$(./color.sh); ./send.sh '0:key:0xB0' >/dev/null; sleep 4; b=$(./color.sh)
echo "folder: $a | next: $b"
[[ "$a" == *"a.mp4 · Cue -> yellow" && "$b" == *"b.mp4 · Cue -> blue" ]] || ok=0
WAIT=8 ./fresh.sh 'C:\m\c3.mp4' 2>/dev/null; c=$(./color.sh); echo "file: $c"; [[ "$c" == *"c3.mp4 · Cue -> blue" ]] || ok=0
[ $ok = 1 ] && echo PASS || echo FAIL
