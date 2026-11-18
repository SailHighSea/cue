# An external hard disk, simulated (Linux + Wine)

`slowhd.c` is an `LD_PRELOAD` library: every file under `SLOWHD_PREFIX` (paths separated by `|`)
behaves like a USB hard disk. One queue for all processes, a seek (`SLOWHD_SEEK_MS`, 14) per
non-sequential read and per look-up, a transfer rate (`SLOWHD_MBPS`, 25) and a spin-up
(`SLOWHD_SPINUP_MS`, 2500) after `SLOWHD_SLEEP_S` (30) seconds without a read: the drive slept.
Wine's server opens the files, so a descriptor is known by its path at its first read.

`ui_probe.exe` pings Cue's window from another process every 10 ms and prints each time its window
thread took 50 ms or more to answer (then closes Cue). `run.sh <s> <Cue args>` runs Cue from
`C:\Cue` with `E:` on the slow disk; `helper.sh head|tail <file>` runs the episodes' sound helper
alone; `disk.py` prints how many operations and how much disk time there were.

    gcc -O2 -shared -fPIC -o slowhd.so slowhd.c -ldl
    x86_64-w64-mingw32-g++-posix -O2 -static -o ui_probe.exe ui_probe.cpp
    ln -s /path/to/slow/folder $WINEPREFIX/dosdevices/e:
    wine64 reg add 'HKLM\Software\Wine\Drives' /v e: /d floppy /f   # E: as a removable drive
