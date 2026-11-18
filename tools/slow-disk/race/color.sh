#!/bin/bash
# color.sh : the colour in the middle of Cue's picture (title + rgb)
export DISPLAY=:9
w=$(xdotool search --name 'Cue$' 2>/dev/null | while read w; do n=$(xdotool getwindowname $w); [ "$n" != "Cue" ] && echo $w; done | head -1)
[ -z "$w" ] && { echo "no window"; exit; }
eval $(xdotool getwindowgeometry --shell $w)
import -window root /tmp/c.png 2>/dev/null
python3 - "$X" "$Y" "$WIDTH" "$HEIGHT" "$(xdotool getwindowname $w)" <<'P'
import sys; from PIL import Image
x,y,w,h=map(int,sys.argv[1:5]); im=Image.open('/tmp/c.png').convert('RGB')
r,g,b=im.getpixel((x+w//4,y+h//4))
name = 'red' if r>180 and g<80 and b<80 else 'green' if g>100 and r<80 and b<80 else 'blue' if b>180 and r<80 and g<80 else 'yellow' if r>180 and g>180 and b<80 else f'({r},{g},{b})'
print(sys.argv[5], '->', name)
P
