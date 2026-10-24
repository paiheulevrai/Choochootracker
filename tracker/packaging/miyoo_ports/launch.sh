#!/bin/sh
mydir=$(cd "$(dirname "$0")" && pwd)
cd "$mydir" || exit 1
exec > "$mydir/log.txt" 2>&1
export LD_LIBRARY_PATH="$mydir/libs:$LD_LIBRARY_PATH"
export SDL_VIDEODRIVER=mmiyoo
export SDL_AUDIODRIVER=mmiyoo
: "${miyoodir:=/mnt/SDCARD/miyoo}"
export miyoodir
if [ -f /mnt/SDCARD/.tmp_update/script/stop_audioserver.sh ]; then
  sh /mnt/SDCARD/.tmp_update/script/stop_audioserver.sh
  sleep 1
elif pgrep audioserver >/dev/null 2>&1; then
  killall -q -9 audioserver audioserver.mod
  sleep 1
fi
unset LD_PRELOAD
chmod +x "$mydir/choochootracker"
./choochootracker
