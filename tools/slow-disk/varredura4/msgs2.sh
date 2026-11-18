#!/bin/bash
# msgs2.sh <label> <warm s> <measure s> <pre-action> [cue args]: UI-thread messages per second by kind (wp2, :10)
export WINEPREFIX=/home/claude/wp2 DISPLAY=:10
lab=$1 warm=$2 meas=$3 pre=$4; shift 4
S=/home/claude/wp2/drive_c/users/root/AppData/Roaming/tinypeep/Cue
wineserver -k 2>/dev/null; sleep 0.5; rm -f $S/state.json*; mkdir -p $S
echo '{"schema":5,"prefs":{"ambient":true,"onboarded":true,"news_seen":"1.4","news_closed":"1.4","volume":60}}' > $S/state.json
WINEDEBUG=-all /usr/lib/wine/wine64 reg add 'HKCU\Software\Wine\Debug' /v RelayInclude /t REG_SZ /d 'user32.DispatchMessageW;user32.SetTimer;user32.KillTimer;win32u.NtUserSetTimer' /f >/dev/null 2>&1
(WINEDEBUG=+relay /usr/lib/wine/wine64 'C:\Cue\Cue.exe' "$@" > /tmp/relay-$lab.txt 2>&1 &)
sleep 4; [ -n "$pre" ] && bash -c "$pre"
sleep $warm; L1=$(wc -l < /tmp/relay-$lab.txt); sleep $meas; L2=$(wc -l < /tmp/relay-$lab.txt)
sed -n "${L1},${L2}p" /tmp/relay-$lab.txt > /tmp/relay-$lab.win
pid=$(pgrep -f 'C:\\Cue\\Cue.exe' | head -1)
wineserver -k
n=$(grep -c "Call user32.DispatchMessageW" /tmp/relay-$lab.win)
st=$(grep -c "Call user32.SetTimer" /tmp/relay-$lab.win)
echo "== $lab: $(echo "scale=1; $n/$meas" | bc) msgs/s, SetTimer $(echo "scale=1; $st/$meas" | bc)/s; by kind:"
grep -o "Call window proc [0-9a-fA-F]* (hwnd=[0-9a-fA-F]*,msg=[A-Z_0-9a-fx]*,wp=[0-9a-fA-F]*" /tmp/relay-$lab.win | sed 's/.*msg=//' | awk -F',wp=' '{print $1, ($1=="WM_TIMER"? "id " $2 : "")}' | sort | uniq -c | sort -rn | head -8 | awk -v m=$meas '{printf "   %5.1f/s %s %s %s\n", $1/m, $2, $3, $4}'
