#!/bin/bash
# run.sh <seconds> <cue args...>   Cue from C:\Cue with E: as a slow USB HDD; the probe watches its UI thread
export WINEDEBUG=-all WINEPREFIX=/home/claude/wp DISPLAY=:9
secs=$1; shift
pgrep Xvfb >/dev/null || (Xvfb :9 -screen 0 1920x1080x24 >/dev/null 2>&1 &)
mkdir -p /home/claude/wp/drive_c/Cue && cp -r /home/claude/w/01-REPOSITORIO/build/Cue/. /home/claude/wp/drive_c/Cue/
export SLOWHD_PREFIX="/home/claude/slowhd|/home/claude/wp/dosdevices/e:"
export LD_PRELOAD=${NOSLOW:+}${NOSLOW:-/home/claude/sim/slowhd.so}
[ -n "$KEEP_STATE" ] || rm -f /tmp/slowhd.state
/usr/lib/wine/wine64 'C:\Cue\Cue.exe' "$@" > /tmp/cue.out 2>&1 &
/usr/lib/wine/wine64 'Z:\home\claude\sim\ui_probe.exe' $secs > /tmp/probe.txt 2>&1
cat /tmp/probe.txt
