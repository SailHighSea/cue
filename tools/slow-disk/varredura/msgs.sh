#!/bin/bash
# msgs.sh <label> <warm s> <measure s> [ACT_BEFORE] -- cue args: UI-thread messages per second (relay of DispatchMessageW + window proc)
export WINEPREFIX=/home/claude/wp DISPLAY=:9
lab=$1 warm=$2 meas=$3 pre=$4; shift 4
wineserver -k 2>/dev/null; sleep 0.5
WINEDEBUG=-all /usr/lib/wine/wine64 reg add 'HKCU\Software\Wine\Debug' /v RelayInclude /t REG_SZ /d 'user32.DispatchMessageW;user32.SetTimer' /f >/dev/null 2>&1
(WINEDEBUG=+relay /usr/lib/wine/wine64 'C:\Cue\Cue.exe' "$@" > /tmp/relay-$lab.txt 2>&1 &)
sleep 3; [ -n "$pre" ] && bash -c "$pre"
sleep $warm; L1=$(wc -l < /tmp/relay-$lab.txt); sleep $meas; L2=$(wc -l < /tmp/relay-$lab.txt)
sed -n "${L1},${L2}p" /tmp/relay-$lab.txt > /tmp/relay-$lab.win
wineserver -k
n=$(grep -c "Call user32.DispatchMessageW" /tmp/relay-$lab.win)
echo "== $lab: $(echo "scale=1; $n/$meas" | bc) msgs/s; by kind:" 
grep -o "Call window proc [0-9a-fA-F]* (hwnd=[0-9a-fA-F]*,msg=[A-Z_0-9a-fx]*,wp=[0-9a-fA-F]*" /tmp/relay-$lab.win | sed 's/.*msg=//' | awk -F',wp=' '{print $1, ($1=="WM_TIMER"? "id " $2+0 : "")}' | sort | uniq -c | sort -rn | head -6 | awk -v m=$meas '{printf "   %5.1f/s %s %s %s\n", $1/m, $2, $3, $4}'  # (19: hex in capitals in Wine 9; 8001 = WM_APP+1, the engine's events)
