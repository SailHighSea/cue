#!/bin/bash
cd /home/claude/w/repo
x86_64-w64-mingw32-g++-posix build/obj/*/*.o build/obj/res.o -o /tmp/Cue-sym.exe -flto=2 -Wl,--gc-sections -municode -mwindows -static -static-libgcc -static-libstdc++ -ldwmapi -lole32 -lshell32 -luuid -lshcore -lcomdlg32 -lwinmm -lwindowscodecs -lwinhttp -lbcrypt -lws2_32 -liphlpapi -lcrypt32
x86_64-w64-mingw32-nm -C --defined-only /tmp/Cue-sym.exe | awk '$2 ~ /[tT]/' | sort > /tmp/syms.txt
