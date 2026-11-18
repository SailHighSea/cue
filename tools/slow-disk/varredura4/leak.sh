#!/bin/bash
# leak.sh <rounds> <label>: Cue in wp2 (:10), files opened one after another (videos, songs, a folder, home and back),
# handles / GDI / USER / private bytes / RSS sampled every 8 opens
export WINEPREFIX=/home/claude/wp2 WINEDEBUG=-all DISPLAY=:10
W=/usr/lib/wine/wine64
S=/home/claude/wp2/drive_c/users/root/AppData/Roaming/tinypeep/Cue
wineserver -k 2>/dev/null; sleep 1; rm -f $S/state.json*; mkdir -p $S
echo '{"schema":5,"prefs":{"ambient":true,"onboarded":true,"news_seen":"1.4","news_closed":"1.4","volume":60}}' > $S/state.json
($W 'C:\Cue\Cue.exe' 'C:\m\c1.mp4' > /dev/null 2>&1 &)
sleep 8
F=('C:\m\c2.mp4' 'C:\m\song1.mp3' 'C:\m\c3.mp4' 'C:\m\song2.mp3' 'C:\m\tall.mp4' 'C:\m\song3.mp3' 'C:\m\film1.mp4' 'C:\m\song4.mp3' 'C:\f' 'C:\m\c4.mp4' 'key:0x1B' 'C:\m\song1.mp3' 'key:0xB0' 'key:0xB0' 'C:\m\c1.mp4')
for r in $(seq 1 $1); do
  for i in "${!F[@]}"; do
    f=${F[$i]}; [[ $f == key:* ]] && $W 'C:\sendopen.exe' "0:$f" >/dev/null 2>&1 || $W 'C:\sendopen.exe' "0:$f" >/dev/null 2>&1
    sleep 2.5
  done
  pid=$(pgrep -f 'C:\\Cue\\Cue.exe' | head -1)
  rss=$(awk '/VmRSS/{print $2}' /proc/$pid/status 2>/dev/null)
  echo "$2 round $r $(date +%T) $($W 'C:\prc.exe' 2>/dev/null | tr -d '\r') rss ${rss} KB fds $(ls /proc/$pid/fd 2>/dev/null | wc -l)"
done
wineserver -k
