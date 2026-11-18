#!/bin/bash
# go.sh <label> <warmup s> <measure s> [cue args...]   (ACT=script run after warmup, in background, while measuring)
export WINEDEBUG=-all WINEPREFIX=/home/claude/wp DISPLAY=:9
lab=$1 warm=$2 meas=$3; shift 3
pgrep Xvfb >/dev/null || (Xvfb :9 -screen 0 1920x1080x24 >/dev/null 2>&1 &)
wineserver -k 2>/dev/null; sleep 0.5
wineserver -p 30 ; (/usr/lib/wine/wine64 "C:\\idle.exe" >/dev/null 2>&1 &) ; sleep 3
export SLOWHD_PREFIX="${SLOWHD_PREFIX:-/home/claude/slowhd/|/home/claude/wp/dosdevices/e:/}"  # (or your own)
[ -n "$SLOW" ] && export LD_PRELOAD=/home/claude/sim/slowhd.so
rm -f /tmp/slowhd.state
/usr/lib/wine/wine64 'C:\Cue\Cue.exe' "$@" > /tmp/cue.out 2>&1 &
unset LD_PRELOAD
if [ -n "$NOPROBE" ]; then (sleep $((warm+meas+2)); echo "no probe") > /tmp/probe-$lab.txt & else /usr/lib/wine/wine64 "Z:\\home\\claude\\sim\\ui_probe.exe" $((warm+meas+2)) > /tmp/probe-$lab.txt 2>&1 & fi
PR=$!
sleep $warm
pid=$(pgrep -f 'C:\\Cue\\Cue.exe' | head -1); [ -z "$pid" ] && pid=$(pgrep -f 'Cue.exe' | grep -v $PR | head -1)
[ -n "$ACT" ] && (bash -c "$ACT" > /tmp/act-$lab.txt 2>&1 &)
if [ -n "$pid" ] && [ -d /proc/$pid ]; then python3 /home/claude/sim/stat.py $pid $meas > /tmp/stat-$lab.txt 2>&1; [ -n "$THREADS" ] && python3 /home/claude/sim/threads.py $pid 5 > /tmp/thr-$lab.txt; else echo "no Cue process" > /tmp/stat-$lab.txt; fi
wait $PR
alive=$(pgrep -f 'C:\\Cue\\Cue.exe' >/dev/null && echo yes || echo no)
echo "== $lab: $(cat /tmp/stat-$lab.txt) | $(tail -1 /tmp/probe-$lab.txt) | stalls: $(grep -c stall /tmp/probe-$lab.txt)"
grep stall /tmp/probe-$lab.txt | head -5; [ -n "$THREADS" ] && cat /tmp/thr-$lab.txt
wineserver -k 2>/dev/null; true
