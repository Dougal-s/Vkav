#!/bin/bash

cd "$(dirname "$0")"

mkdir "vkav/usr"
mkdir "vkav/usr/bin"
cp "../build/vkav" "vkav/usr/bin"

mkdir "vkav/etc"
mkdir "vkav/etc/vkav"
cp "../src/config" "vkav/etc/vkav"
cp -r "../src/modules" "vkav/etc/vkav"

mkdir "vkav/usr/share"
mkdir "vkav/usr/share/icons"
mkdir "vkav/usr/share/icons/hicolor"

mkdir "vkav/usr/share/icons/hicolor/48x48"
mkdir "vkav/usr/share/icons/hicolor/48x48/apps"
cp "../vkav.png" "vkav/usr/share/icons/hicolor/48x48/apps"

mkdir "vkav/usr/share/icons/hicolor/scalable"
mkdir "vkav/usr/share/icons/hicolor/scalable/apps"
cp "../vkav.svg" "vkav/usr/share/icons/hicolor/scalable/apps"

mkdir "vkav/usr/share/applications"
cp "../vkav.desktop" "vkav/usr/share/applications"

dpkg-deb --build vkav
mv ./vkav.deb ./vkav_0.5.0_$(uname -m).deb
