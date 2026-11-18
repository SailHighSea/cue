#!/bin/bash
# livedrag.sh <out.mp4> <song>: the real Cue (wp2, :10) while its edge is dragged (xdotool steps), recorded at 30 fps
export WINEPREFIX=/home/claude/wp2 WINEDEBUG=-all DISPLAY=:10
S=/home/claude/wp2/drive_c/users/root/AppData/Roaming/tinypeep/Cue
wineserver -k 2>/dev/null; sleep 0.5; rm -f $S/state.json*
echo '{"schema":5,"prefs":{"ambient":true,"onboarded":true,"news_seen":"1.4","news_closed":"1.4","volume":60}}' > $S/state.json
/usr/lib/wine/wine64 reg add 'HKCU\Control Panel\Desktop' /v LogPixels /t REG_DWORD /d 96 /f >/dev/null 2>&1
(/usr/lib/wine/wine64 'C:\Cue\Cue.exe' "$2" > /dev/null 2>&1 &)
sleep 9
w=$(xdotool search --name 'Cue' | while read i; do g=$(xdotool getwindowgeometry --shell $i | grep WIDTH | cut -d= -f2); [ "$g" -gt 100 ] && echo $i; done | head -1)
xdotool windowmove $w 0 0 windowsize $w 300 300; sleep 2
ffmpeg -loglevel error -y -f x11grab -framerate 30 -video_size 1600x1000 -i :10.0+0,0 -t 40 -c:v libx264 -preset ultrafast -crf 18 $1 &
F=$!; sleep 1
for W in $(seq 300 10 1600); do xdotool windowsize $w $W 300; sleep 0.03; done; sleep 1
for H in $(seq 300 10 1000); do xdotool windowsize $w 1600 $H; sleep 0.03; done; sleep 1
for W in $(seq 1600 -10 260); do xdotool windowsize $w $W 1000; sleep 0.03; done; sleep 1
for H in $(seq 1000 -10 220); do xdotool windowsize $w 260 $H; sleep 0.03; done; sleep 1
wait $F; wineserver -k
