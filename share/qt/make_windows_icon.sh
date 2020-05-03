#!/bin/bash
# create multiresolution windows icon
ICON_SRC=../../src/qt/res/icons/icon.png
ICON_DST=../../src/qt/res/icons/bcexchange.ico
convert ${ICON_SRC} -resize 16x16 icon-16.png
convert ${ICON_SRC} -resize 32x32 icon-32.png
convert ${ICON_SRC} -resize 48x48 icon-48.png
convert icon-48.png icon-32.png icon-16.png ${ICON_DST}

