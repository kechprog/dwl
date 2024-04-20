#!/bin/bash

export QT_QPA_PLATFORM=wayland
export XDG_CURRENT_DESKTOP=sway
export XDG_SESSION_DESKTOP=sway


if [ -z "$DBUS_SESSION_BUS_ADDRESS" ]; then
  echo "Starting a new D-Bus session."
  eval `dbus-launch --sh-syntax`
fi

dbus-run-session /usr/local/bin/dwl -s "/usr/local/bin/somebar & swww-daemon"
