#!/bin/bash
# helper.sh <part> <file>: the sound helper alone on the slow disk
export WINEDEBUG=-all WINEPREFIX=/home/claude/wp DISPLAY=:9
export SLOWHD_PREFIX="/home/claude/slowhd|/home/claude/wp/dosdevices/e:" LD_PRELOAD=/home/claude/sim/slowhd.so
/usr/lib/wine/wine64 'C:\Cue\Cue.exe' --sound-print 'C:\print.txt' "$2" $1 ${3:-0}
