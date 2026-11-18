#!/bin/bash
# realshot.sh <exe> <dpi> <ui_scale id> <WxH at 100%> <song> <out.png> [rest]: one capture of the real Cue (wp2, :10)
export WINEPREFIX=/home/claude/wp2 WINEDEBUG=-all DISPLAY=:10
exe=$1 dpi=$2 ui=$3 sz=$4 song=$5 out=$6
S=/home/claude/wp2/drive_c/users/root/AppData/Roaming/tinypeep/Cue
wineserver -k 2>/dev/null; sleep 0.5; rm -f $S/state.json*; cp "$exe" /home/claude/wp2/drive_c/Cue/Cue.exe
echo "{\"schema\":5,\"prefs\":{\"ambient\":true,\"onboarded\":true,\"news_seen\":\"1.4\",\"news_closed\":\"1.4\",\"volume\":60,\"ui_scale\":$ui}}" > $S/state.json
/usr/lib/wine/wine64 reg add 'HKCU\Control Panel\Desktop' /v LogPixels /t REG_DWORD /d $dpi /f >/dev/null 2>&1
(/usr/lib/wine/wine64 'C:\Cue\Cue.exe' "$song" > /dev/null 2>&1 &)
sleep 9
w=$(xdotool search --name 'Cue' | while read i; do g=$(xdotool getwindowgeometry --shell $i | grep WIDTH | cut -d= -f2); [ "$g" -gt 100 ] && echo $i; done | head -1)
k=$(echo "$dpi/96" | bc -l); W=$(printf '%.0f' $(echo "${sz%x*}*$k" | bc -l)); H=$(printf '%.0f' $(echo "${sz#*x}*$k" | bc -l))
xdotool windowmove $w 0 0 windowsize $w $W $H; sleep 2
if [ -z "$7" ]; then xdotool mousemove $((W/2)) $((H/2)); sleep 0.15; xdotool mousemove $((W/2+3)) $((H/2+2)); sleep 1.5; else xdotool mousemove 1500 900; sleep 4; fi
import -window root -crop ${W}x${H}+0+0 +repage $out
wineserver -k
