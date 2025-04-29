#!/bin/sh

nix-shell -p gcc  xorg.libICE xorg.libXi xorg.libXScrnSaver xorg.libXcursor xorg.libXinerama xorg.libXext xorg.libXrandr xorg.libXxf86vm pipewire.dev udev libGL
