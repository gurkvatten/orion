#!/bin/bash

if [ $# == 0 ]; then
  echo "Using flatpak-debug build location"
  LOCATION=../build/flatpak-debug/oriongtk.flatpak
else
  echo "Using $1 as file to install from"
  LOCATION="$1"
fi

echo

flatpak -y --noninteractive --user uninstall --delete-data com.kagi.Orion
echo
flatpak -y --noninteractive --user install $LOCATION
echo
flatpak --user run com.kagi.Orion