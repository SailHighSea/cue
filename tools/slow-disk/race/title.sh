#!/bin/bash
export DISPLAY=:9
for w in $(xdotool search --name 'Cue$' 2>/dev/null); do n=$(xdotool getwindowname $w); [ "$n" != "Cue" ] && echo "$n"; done
