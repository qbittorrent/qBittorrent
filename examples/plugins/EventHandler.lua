name = "Event Handler Example"
version = "0.1"

function onTorrentAdded(torrent)
    qBittorrent.log(string.format("%s: Torrent '%s' is added.", name, torrent.name))

    qBittorrent.debug(string.format("Torrent: %s. ID: %s.", torrent.name, torrent.id))
    qBittorrent.debug(string.format("Torrent: %s. Info hash v1: %s.", torrent.name, torrent.infoHashV1))
    qBittorrent.debug(string.format("Torrent: %s. Info hash v2: %s.", torrent.name, torrent.infoHashV2))
    qBittorrent.debug(string.format("Torrent: %s. Files count: %d.", torrent.name, torrent.filesCount))
    qBittorrent.debug(string.format("Torrent: %s. Total size: %d.", torrent.name, torrent.totalSize))
    qBittorrent.debug(string.format("Torrent: %s. Category: %s.", torrent.name, torrent.category))
    qBittorrent.debug(string.format("Torrent: %s. Tags: %s.", torrent.name, table.concat(torrent.tags, ", ")))
    qBittorrent.debug(string.format("Torrent: %s. Save path: %s.", torrent.name, torrent.savePath))
    qBittorrent.debug(string.format("Torrent: %s. Download path: %s.", torrent.name, torrent.downloadPath))
    qBittorrent.debug(string.format("Torrent: %s. Content path: %s", torrent.name, torrent.contentPath))
    qBittorrent.debug(string.format("Torrent: %s. Root path: %s.", torrent.name, torrent.rootPath))
    qBittorrent.debug(string.format("Torrent: %s. Current tracker: %s.", torrent.name, torrent.currentTracker))
end

function onTorrentMetadataReady(torrent)
    qBittorrent.log(string.format("%s: Torrent '%s' has metadata ready.", name, torrent.name))
end

function onTorrentFinished(torrent)
    qBittorrent.log(string.format("%s: Torrent '%s' is finished.", name, torrent.name))
end

function onTorrentStarted(torrent)
    qBittorrent.log(string.format("%s: Torrent '%s' is started.", name, torrent.name) )
end

function onTorrentStopped(torrent)
    qBittorrent.log(string.format("%s: Torrent '%s' is stopped.", name, torrent.name))
end

function onTorrentSavePathChanged(torrent)
    qBittorrent.log(string.format("%s: Torrent '%s' has changed save path.", name, torrent.name))
end

function onTorrentCategoryChanged(torrent, oldCategory)
    qBittorrent.log(string.format("%s: Torrent '%s' has changed category.", name, torrent.name))
    qBittorrent.debug(string.format("Torrent: %s. Old category: %s. New category: %s.", torrent.name, oldCategory, torrent.category))
end

function onTorrentTagAdded(torrent, tag)
    qBittorrent.log(string.format("%s: Torrent '%s' has tag added.", name, torrent.name))
    qBittorrent.debug(string.format("Torrent: %s. Added tag: %s.", torrent.name, tag))
end

function onTorrentTagRemoved(torrent, tag)
    qBittorrent.log(string.format("%s: Torrent '%s' has tag removed.", name, torrent.name))
    qBittorrent.debug(string.format("Torrent: %s. Removed tag: %s.", torrent.name, tag))
end

function onTorrentIOError(torrent, message)
    qBittorrent.log(string.format("%s: Torrent '%s' has IO error.", name, torrent.name))
    qBittorrent.debug(string.format("Torrent: %s. IO error message: %s.", torrent.name, message))
end

function onTorrentAboutToBeRemoved(torrent)
    qBittorrent.log(string.format("%s: Torrent '%s' is about to be removed.", name, torrent.name))
end

function onTorrentAnnounceSuccess(torrent, tracker)
    qBittorrent.log(string.format("%s: Torrent '%s' has announced successfully.", name, torrent.name))
    qBittorrent.debug(string.format("Torrent: %s. Announced tracker: %s.", torrent.name, tracker))
end

function onTorrentAnnounceWarning(torrent, tracker)
    qBittorrent.log(string.format("%s: Torrent '%s' has announced with warning.", name, torrent.name))
    qBittorrent.debug(string.format("Torrent: %s. Announced tracker: %s.", torrent.name, tracker))
end

function onTorrentAnnounceError(torrent, tracker)
    qBittorrent.log(string.format("%s: Torrent '%s' has failed announce.", name, torrent.name))
    qBittorrent.debug(string.format("Torrent: %s. Failed tracker: %s.", torrent.name, tracker))
end

function onTorrentsTrackersAdded(torrent, addedTrackers)
    qBittorrent.log(string.format("%s: Torrent '%s' has added trackers.", name, torrent.name))

    addedTrackerURLs = {}
    for tracker in addedTrackers do
        table.insert(addedTrackerURLs, tracker.url)
    end
    qBittorrent.debug(string.format("Torrent: %s. Added trackers: %s.", torrent.name, table.concat(addedTrackerURLs, ", ")))
end

function trackersRemoved(torrent, removedTrackers)
    qBittorrent.log(string.format("%s: Torrent '%s' has removed trackers.", name, torrent.name))
    qBittorrent.debug(string.format("Torrent: %s. Removed trackers: %s.", torrent.name, table.concat(removedTrackers, ", ")))
end
