#!/bin/bash

export QT_QPA_PLATFORM=wayland
export XDG_CURRENT_DESKTOP=sway
export XDG_SESSION_DESKTOP=sway


if [ -z "$DBUS_SESSION_BUS_ADDRESS" ]; then
  echo "Starting a new D-Bus session."
  eval `dbus-launch --sh-syntax`
fi

echo "Starting gentoo-pipewire-launcher."
gentoo-pipewire-launcher restart & disown
sleep 0.2

dbus-run-session /usr/local/bin/dwl -s "/usr/local/bin/somebar & swww init"

sleep 1

exec systemctl --user import-environment DISPLAY WAYLAND_DISPLAY
exec hash dbus-update-activation-environment 2>/dev/null && \
     dbus-update-activation-environment --systemd DISPLAY WAYLAND_DISPLAY
