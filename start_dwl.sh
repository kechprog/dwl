#!/bin/bash

gentoo-pipewire-launcher restart & disown
/usr/local/bin/dwl -s "/usr/local/bin/somebar & swww init" > /dev/null &
