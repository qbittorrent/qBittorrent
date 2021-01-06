#!/bin/sh

# get version numbers
versionSrc="src/base/version.h.in"
versionMajor="$(grep -Po '(?<=QBT_VERSION_MAJOR )\d+' "$versionSrc")"
versionMinor="$(grep -Po '(?<=QBT_VERSION_MINOR )\d+' "$versionSrc")"
versionBugfix="$(grep -Po '(?<=QBT_VERSION_BUGFIX )\d+' "$versionSrc")"
versionBuild="$(grep -Po '(?<=QBT_VERSION_BUILD )\d+' "$versionSrc")"
versionStatus="$(grep -Po '(?<=QBT_VERSION_STATUS ")\w+' "$versionSrc")"

if [ "$versionBuild" != "0" ]; then
    projectVersion="$versionMajor.$versionMinor.$versionBugfix.$versionBuild$versionStatus"
else
    projectVersion="$versionMajor.$versionMinor.$versionBugfix$versionStatus"
fi

# pack archives
git archive --format=tar --prefix="qbittorrent-$projectVersion/" HEAD | gzip -9 > "qbittorrent-$projectVersion.tar.gz"
git archive --format=tar --prefix="qbittorrent-$projectVersion/" HEAD | xz -9 > "qbittorrent-$projectVersion.tar.xz"
