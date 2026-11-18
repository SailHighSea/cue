#!/bin/bash
# kinds.sh <relay .win> <seconds>: messages per second by kind (8001 = WM_APP+1, the engine's events)
grep -o "Call window proc [0-9a-fA-F]* (hwnd=[0-9a-fA-F]*,msg=[A-Z_0-9a-fx]*,wp=[0-9a-fA-F]*" "$1" | sed 's/.*msg=//' | awk -F',wp=' '{print $1, ($1=="WM_TIMER"? "id " $2+0 : "")}' | sort | uniq -c | sort -rn | head -8 | awk -v m=$2 '{printf "   %5.1f/s %s %s %s\n", $1/m, $2, $3, $4}'
