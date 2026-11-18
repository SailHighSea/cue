#!/bin/bash
# fresh.sh [cue args] : Cue anew (state kept unless WIPE=1); SLOW=1: E: on the slow disk
export WINEPREFIX=/home/claude/wp WINEDEBUG=-all DISPLAY=:9
wineserver -k 2>/dev/null; sleep 0.5; rm -f /tmp/slowhd.lag /tmp/slowhd.gone /tmp/slowhd.state
[ -n "$WIPE" ] && rm -rf /home/claude/wp/drive_c/users/root/AppData/Roaming/tinypeep/Cue/state.json*
export SLOWHD_PREFIX="/home/claude/slowhd/|/home/claude/wp/dosdevices/e:/"
[ -n "$SLOW" ] && export LD_PRELOAD=/home/claude/sim/slowhd.so
(/usr/lib/wine/wine64 'C:\Cue\Cue.exe' --log "$@" > /tmp/cue.out 2>&1 &)
unset LD_PRELOAD
sleep ${WAIT:-5}
