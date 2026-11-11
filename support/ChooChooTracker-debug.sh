#!/bin/bash

PORTSDIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd) || exit 1
LOGFILE="$PORTSDIR/cct-startup.txt"

if ! : > "$LOGFILE"; then
  printf 'Cannot write diagnostic log: %s\n' "$LOGFILE" >&2
  exit 1
fi
exec >> "$LOGFILE" 2>&1

printf '=== ChooChooTracker startup diagnosis ===\n'
date -u
uname -a
if [ -f /etc/os-release ]; then
  cat /etc/os-release
fi
if [ -f /usr/share/knulli/knulli.version ]; then
  cat /usr/share/knulli/knulli.version
fi
printf '\nPorts directory: %s\n' "$PORTSDIR"
printf 'HOME=%s\nXDG_DATA_HOME=%s\n' "$HOME" "${XDG_DATA_HOME:-}"
printf '\n=== PortMaster locations ===\n'
for control in \
  /opt/system/Tools/PortMaster/control.txt \
  /opt/tools/PortMaster/control.txt \
  "${XDG_DATA_HOME:-$HOME/.local/share}/PortMaster/control.txt" \
  /roms/ports/PortMaster/control.txt \
  /userdata/roms/ports/PortMaster/control.txt; do
  if [ -f "$control" ]; then
    printf 'FOUND: %s\n' "$control"
  else
    printf 'MISSING: %s\n' "$control"
  fi
done

printf '\n=== Installed files ===\n'
ls -ld "$PORTSDIR/choochootracker" "$PORTSDIR/choochootracker/choochootracker.aarch64"
if command -v ldd >/dev/null 2>&1 && [ -f "$PORTSDIR/choochootracker/choochootracker.aarch64" ]; then
  ldd "$PORTSDIR/choochootracker/choochootracker.aarch64"
fi

ORIGINAL="$PORTSDIR/ChooChooTracker.sh"
if [ ! -f "$ORIGINAL" ]; then
  printf '\nERROR: Original launcher missing: %s\n' "$ORIGINAL"
  printf 'Place ChooChooTracker-debug.sh next to ChooChooTracker.sh; do not replace it.\n'
  exit 1
fi

printf '\n=== Original launcher trace ===\n'
export PS4='+ ${BASH_SOURCE}:${LINENO}: '
# Drain the original launcher's asynchronous tee before reporting its exit.
bash -x "$ORIGINAL" 2>&1 | cat
status=${PIPESTATUS[0]}
printf '\n=== Launcher exited with status %s ===\n' "$status"
printf 'An application crash may appear above even if the launcher exits with status 0.\n'
exit "$status"
