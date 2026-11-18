#!/bin/bash
# perf.sh <label> <ambient true|false> <pre-action> <cue args...> : UI-thread messages per second (msgs.sh)
# over a clean state (onboarded, the light on or off), 4 s to settle then 10 s measured
cd "$(dirname "$0")"; lab=$1 amb=$2 pre=$3; shift 3
S=/home/claude/wp/drive_c/users/root/AppData/Roaming/tinypeep/Cue
WINEPREFIX=/home/claude/wp wineserver -k 2>/dev/null; rm -f $S/state.json*
echo "{\"schema\":5,\"prefs\":{\"ambient\":$amb,\"onboarded\":true,\"news_seen\":\"1.4\",\"news_closed\":\"1.4\",\"volume\":60}}" > $S/state.json
bash /home/claude/b/01-REPOSITORIO/tools/slow-disk/varredura/msgs.sh "$lab" 4 10 "$pre" "$@" 2>&1 | grep -v XGetInput
