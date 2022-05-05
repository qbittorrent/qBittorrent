#!/bin/sh

profilePath="/config"
qbtConfigFile="$profilePath/qBittorrent/config/qBittorrent.conf"

if [ ! -f "$qbtConfigFile" ]; then
    mkdir -p "$(dirname $qbtConfigFile)"
    cat << EOF > "$qbtConfigFile"
[BitTorrent]
Session\DefaultSavePath=/downloads
Session\Port=6881
Session\TempPath=/downloads/temp

[LegalNotice]
Accepted=false
EOF

    if [ "$QBT_EULA" = "accept" ]; then
        sed -i '/^\[LegalNotice\]$/{$!{N;s|\(\[LegalNotice\]\nAccepted=\).*|\1true|}}' "$qbtConfigFile"
    else
        sed -i '/^\[LegalNotice\]$/{$!{N;s|\(\[LegalNotice\]\nAccepted=\).*|\1false|}}' "$qbtConfigFile"
    fi
fi

qbittorrent-nox \
    --profile="$profilePath" \
    --webui-port="$QBT_WEBUI_PORT" \
    "$@"
