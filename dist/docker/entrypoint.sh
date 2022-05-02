#!/bin/sh

if [ ! -f /config/qBittorrent/qBittorrent.conf ]; then
    mkdir -p /config/qBittorrent/
    cat << EOF > /config/qBittorrent/qBittorrent.conf
[BitTorrent]
Session\DefaultSavePath=/downloads
Session\TempPath=/downloads/temp

[LegalNotice]
Accepted=false
EOF

    if [ "$LEGAL" = "accept" ] ; then
        sed -i '/^\[LegalNotice\]$/{$!{N;s|\(\[LegalNotice\]\nAccepted=\).*|\1true|}}' /config/qBittorrent/qBittorrent.conf
    else
        sed -i '/^\[LegalNotice\]$/{$!{N;s|\(\[LegalNotice\]\nAccepted=\).*|\1false|}}' /config/qBittorrent/qBittorrent.conf
    fi
fi

HOME="/config" XDG_CONFIG_HOME="/config" XDG_DATA_HOME="/config" qbittorrent-nox --webui-port=$WEBUI_PORT
