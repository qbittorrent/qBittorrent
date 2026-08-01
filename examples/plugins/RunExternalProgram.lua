NAME = "Run External Program Example"
VERSION = "0.1"

function onTorrentAdded(torrent)
    local testFile = string.format("%s/%s.txt", torrent.savePath, torrent.name)
    qBittorrent.exec(string.format("touch \"%s\"", testFile))
end

function onTorrentFinished(torrent)
    qBittorrent.exec(string.format("/usr/bin/nemo \"%s\"", torrent.savePath))
end

function onTorrentAboutToBeRemoved(torrent)
    local testFile = string.format("%s/%s.txt", torrent.savePath, torrent.name)
    qBittorrent.exec(string.format("rm \"%s\"", testFile))
end
