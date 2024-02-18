#!/bin/bash
#
if [ -z "$DBUS_SESSION_BUS_ADDRESS" ]; then
  echo "Starting a new D-Bus session."
  eval `dbus-launch --sh-syntax`
fi

echo "Starting gentoo-pipewire-launcher."
gentoo-pipewire-launcher restart & disown
sleep 0.2

/usr/local/bin/dwl -s "/usr/local/bin/somebar & swww init" > /dev/null &
