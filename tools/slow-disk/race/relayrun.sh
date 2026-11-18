#!/bin/bash
# relayrun.sh <funcs> -- cue args : Cue under Wine's relay of only these functions (to /tmp/relay.txt)
export WINEPREFIX=/home/claude/wp DISPLAY=:9
f=$1; shift; [ "$1" == "--" ] && shift
wineserver -k 2>/dev/null; sleep 0.5
WINEDEBUG=-all /usr/lib/wine/wine64 reg add 'HKCU\Software\Wine\Debug' /v RelayInclude /t REG_SZ /d "$f" /f >/dev/null 2>&1
(WINEDEBUG=+relay,+tid /usr/lib/wine/wine64 'C:\Cue\Cue.exe' --log "$@" > /tmp/relay.txt 2>&1 &)
sleep ${WAIT:-5}
