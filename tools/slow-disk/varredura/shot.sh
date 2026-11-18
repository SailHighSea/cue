#!/bin/bash
# shot.sh <out.png> : the X screen (Xvfb :9)
DISPLAY=:9 import -window root "$1" 2>/dev/null
