#!/bin/bash
# hung.sh : is Cue's window answering? (SendMessageTimeout 3 s from another process: "ok" or "HUNG")
export WINEPREFIX=/home/claude/wp WINEDEBUG=-all DISPLAY=:9
/usr/lib/wine/wine64 'C:\sendopen.exe' 2>/dev/null | grep -q 'no Cue' && { echo "no window"; exit; }
r=$(/usr/lib/wine/wine64 'C:\ping.exe' 2>/dev/null | tr -d '\r'); echo "$r"
