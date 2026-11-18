#!/bin/bash
# open-burst.sh <gap s> file... : 2nd instances opening files one after another
export WINEPREFIX=/home/claude/wp WINEDEBUG=-all DISPLAY=:9
gap=$1; shift
for f in "$@"; do (/usr/lib/wine/wine64 'C:\Cue\Cue.exe' "$f" >/dev/null 2>&1 &); sleep $gap; done
