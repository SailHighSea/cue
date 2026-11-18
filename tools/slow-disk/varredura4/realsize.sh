#!/bin/bash
# realsize.sh <dpi> <out dir> <song> : the real Cue in wp2 (:10) at many window sizes, a capture of each
export WINEPREFIX=/home/claude/wp2 WINEDEBUG=-all DISPLAY=:10
dpi=$1 out=$2 song=$3; mkdir -p $out
S=/home/claude/wp2/drive_c/users/root/AppData/Roaming/tinypeep/Cue
wineserver -k 2>/dev/null; sleep 0.5; rm -f $S/state.json*
echo '{"schema":5,"prefs":{"ambient":true,"onboarded":true,"news_seen":"1.4","news_closed":"1.4","volume":60}}' > $S/state.json
/usr/lib/wine/wine64 reg add 'HKCU\Control Panel\Desktop' /v LogPixels /t REG_DWORD /d $dpi /f >/dev/null 2>&1
/usr/lib/wine/wine64 reg add 'HKCU\Software\Wine\Fonts' /v LogPixels /t REG_DWORD /d $dpi /f >/dev/null 2>&1
(/usr/lib/wine/wine64 'C:\Cue\Cue.exe' "$song" > /dev/null 2>&1 &)
sleep 9
w=$(xdotool search --name 'Cue' | while read i; do g=$(xdotool getwindowgeometry --shell $i | grep WIDTH | cut -d= -f2); [ "$g" -gt 100 ] && echo $i; done | head -1)
echo "win $w"
k=$(echo "$dpi/96" | bc -l)
for sz in 220x220 260x260 300x300 460x96 520x110 800x120 1000x220 1000x260 1280x300 320x340 480x400 640x480 300x600 420x900 720x720 960x540 1280x720 1600x900; do
  W=$(printf '%.0f' $(echo "${sz%x*}*$k" | bc -l)); H=$(printf '%.0f' $(echo "${sz#*x}*$k" | bc -l))
  xdotool windowmove $w 0 0 windowsize $w $W $H; sleep 1.5
  xdotool mousemove $((W/2)) $((H/2)); sleep 0.15; xdotool mousemove $((W/2+3)) $((H/2+2)); sleep 1.2
  import -window root -crop ${W}x${H}+0+0 +repage $out/$sz.png
done
wineserver -k
