#!/bin/bash
# fsmap.sh <label> <seconds> [ACT] -- cue args : every disk call made by Cue's UI thread, with the caller (Cue-sym.exe)
export WINEPREFIX=/home/claude/wp DISPLAY=:9
lab=$1 secs=$2 act=$3; shift 3; [ "$1" == "--" ] && shift
pgrep Xvfb >/dev/null || (Xvfb :9 -screen 0 1920x1080x24 >/dev/null 2>&1 &)
wineserver -k 2>/dev/null; sleep 0.5
cp ${EXE:-/tmp/Cue-sym.exe} /home/claude/wp/drive_c/Cue/Cue.exe
F="GetFileAttributesW GetFileAttributesExW CreateFileW FindFirstFileExW FindFirstFileW FindNextFileW MoveFileExW CopyFileW DeleteFileW CreateDirectoryW GetDiskFreeSpaceExW GetFileSizeEx ReadFile GetFinalPathNameByHandleW SetFileAttributesW ReplaceFileW"
inc="user32.DispatchMessageW;shell32.SHCreateDirectoryExW;msvcrt._wfopen;msvcrt._wopen;msvcrt._wstat64;msvcrt._wstat;ucrtbase._wfopen"
for f in $F; do inc="$inc;kernel32.$f;kernelbase.$f"; done
WINEDEBUG=-all /usr/lib/wine/wine64 reg add 'HKCU\Software\Wine\Debug' /v RelayInclude /t REG_SZ /d "$inc" /f >/dev/null 2>&1
(WINEDEBUG=+relay,-snoop /usr/lib/wine/wine64 'C:\Cue\Cue.exe' "$@" > /tmp/fs-$lab.txt 2>&1 &)
[ -n "$act" ] && (bash -c "$act" > /tmp/fsact-$lab.txt 2>&1 &)
sleep $secs
wineserver -k 2>/dev/null; sleep 0.5
WINEDEBUG=-all /usr/lib/wine/wine64 reg delete 'HKCU\Software\Wine\Debug' /v RelayInclude /f >/dev/null 2>&1
SYMS=${SYMS:-/tmp/syms.txt} python3 "$(dirname "$0")/fsreport.py" /tmp/fs-$lab.txt
