#!/bin/bash
# Records the Cue's ambient halo in Wine (Xvfb :99): plays a video and grabs the screen at 60 fps.
#   ambient_measure.sh <cue dir> <video> <out.mkv> [seconds] [strength]
set -u
CUE=$1 VID=$2 OUT=$3 SECS=${4:-26} STR=${5:-0.15}
export DISPLAY=:99 WINEPREFIX=/home/claude/wpfx WINEDEBUG=-all
APPD=$(echo $WINEPREFIX/drive_c/users/*/AppData/Roaming/tinypeep/Cue)
WINEPREFIX=/home/claude/wpfx wineserver -k 2>/dev/null; sleep 1
# a fresh start each time: no resume, the chosen strength, welcome already seen
python3 - "$APPD/state.json" "$STR" <<'PY'
import json,sys,re
p,s=sys.argv[1],float(sys.argv[2])
t=open(p).read()
t=re.sub(r'"ambient_strength": [0-9.]+','"ambient_strength": %.2f'%s,t)
t=t.replace('"resume": true','"resume": false')
open(p,'w').write(t)
PY
rm -f "$APPD/cue.log"
WV="Z:$(echo $VID | sed 's#/#\\#g')"
wine "$CUE/Cue.exe" --log "$WV" >/dev/null 2>&1 &
# grab from the moment the window exists
for i in $(seq 1 100); do xdotool search --name "Cue" >/dev/null 2>&1 && break; sleep 0.1; done
ffmpeg -loglevel error -y -f x11grab -framerate 60 -video_size 1920x1080 -i :99 -t $SECS -c:v libx264rgb -preset ultrafast -crf 0 "$OUT"
cp "$APPD/cue.log" "${OUT%.mkv}.cue.log" 2>/dev/null
WINEPREFIX=/home/claude/wpfx wineserver -k 2>/dev/null || true
