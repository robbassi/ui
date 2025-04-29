#!/bin/sh

gcc -I opt/SDL2/include/SDL2 src/main.c opt/SDL2/lib/{libSDL2,libSDL2_ttf,libSDL2_image}.a -lm -lX11 -lXext -lXss -lXrandr -lXi -lXcursor -lXfixes -ludev -lGL
