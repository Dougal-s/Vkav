#!/bin/bash

cd "$(dirname "$0")"

mkdir "vkav_0.3-1/usr"
mkdir "vkav_0.3-1/usr/bin"
cp "../build/vkav" "vkav_0.3-1/usr/bin"

mkdir "vkav_0.3-1/etc"
mkdir "vkav_0.3-1/etc/vkav"
cp "../src/config" "vkav_0.3-1/etc/vkav"
cp -r "../src/shaders" "vkav_0.3-1/etc/vkav"

mkdir "vkav_0.3-1/usr/share"
mkdir "vkav_0.3-1/usr/share/icons"
mkdir "vkav_0.3-1/usr/share/icons/hicolor"

mkdir "vkav_0.3-1/usr/share/icons/hicolor/48x48"
mkdir "vkav_0.3-1/usr/share/icons/hicolor/48x48/apps"
cp "../vkav.png" "vkav_0.3-1/usr/share/icons/hicolor/48x48/apps"

mkdir "vkav_0.3-1/usr/share/icons/hicolor/scalable"
mkdir "vkav_0.3-1/usr/share/icons/hicolor/scalable/apps"
cp "../vkav.svg" "vkav_0.3-1/usr/share/icons/hicolor/scalable/apps"

mkdir "vkav_0.3-1/usr/share/applications"
cp "../vkav.desktop" "vkav_0.3-1/usr/share/applications"

dpkg-deb --build vkav_0.3-1
