#!/bin/bash

if [ $(id -u qbittorrent) != $CHUID ]; then
    usermod -o -u $CHUID qbittorrent 
fi

if [ $(id -g qbittorrent) != $CHGID ]; then
    groupmod -o -g $CHGID qbittorrent
fi

# switch to target user
su qbittorrent

if [ ! -f /config/qBittorrent/qBittorrent.conf ]; then
    mkdir -p /config/qBittorrent/
    cat << EOF > /config/qBittorrent/qBittorrent.conf
[AutoRun]
enabled=false
program=

[LegalNotice]
Accepted=true

[Preferences]
Connection\UPnP=false
Connection\PortRangeMin=6881
Downloads\SavePath=/downloads/
Downloads\ScanDirsV2=@Variant(\0\0\0\x1c\0\0\0\0)
Downloads\TempPath=/downloads/incomplete/
WebUI\Address=*
WebUI\ServerDomains=*
EOF
fi

qbittorrent-nox --webui-port=$WEBUI_PORT
