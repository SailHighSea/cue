#!/bin/bash
export WINEPREFIX=/home/claude/wp WINEDEBUG=-all DISPLAY=:9
/usr/lib/wine/wine64 'C:\sendopen.exe' "$@"
