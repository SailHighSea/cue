#!/bin/bash
# key.sh key... : sends keys to Cue's window (each 0.8 s apart); "sleep:N" waits N s
export DISPLAY=:9
w=$(xdotool search --name '^Cue' 2>/dev/null | head -1); [ -z "$w" ] && w=$(xdotool search --classname cue.exe | head -1)
for k in "$@"; do
  case $k in sleep:*) sleep ${k#sleep:};; click:*) IFS=, read x y <<<"${k#click:}"; xdotool mousemove $x $y click 1; sleep 0.8;; rclick:*) IFS=, read x y <<<"${k#rclick:}"; xdotool mousemove $x $y click 3; sleep 0.8;; *) xdotool windowfocus --sync $w 2>/dev/null; xdotool key --window $w $k; sleep 0.8;; esac
done
echo "window $w"
