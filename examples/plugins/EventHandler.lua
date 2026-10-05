NAME = "Event Handler Example"
VERSION = "0.1"

function onTorrentAdded(torrent)
    qBittorrent.log(string.format("%s: Torrent '%s' is added.", NAME, torrent.name))

    qBittorrent.debug(string.format("Torrent: %s", torrent.name))
    qBittorrent.debug(string.format("  ID: %s", torrent.id))
    qBittorrent.debug(string.format("  Info hash v1: %s", torrent.infoHashV1))
    qBittorrent.debug(string.format("  Info hash v2: %s", torrent.infoHashV2))
    qBittorrent.debug(string.format("  Creator: %s", torrent.creator))
    qBittorrent.debug(string.format("  Creation time: %s", qBittorrent.formatDateTime(torrent.creationTime)))
    qBittorrent.debug(string.format("  Comment: %s", torrent.comment))
    qBittorrent.debug(string.format("  Category: %s", torrent.category))
    qBittorrent.debug(string.format("  Tags: %s", table.concat(torrent.tags, ", ")))
    qBittorrent.debug(string.format("  Save path: %s", torrent.savePath))
    qBittorrent.debug(string.format("  Download path: %s", torrent.downloadPath))
    qBittorrent.debug(string.format("  Content path: %s", torrent.contentPath))
    qBittorrent.debug(string.format("  Root path: %s", torrent.rootPath))
    qBittorrent.debug(string.format("  Current tracker: %s", torrent.currentTracker))
    qBittorrent.debug("  Trackers:")
    for _, trackerStatus in ipairs(torrent.trackerStatuses) do
        qBittorrent.debug(string.format("    %s", trackerStatus.url))
    end
    qBittorrent.debug("  URL seeds:")
    for _, urlSeed in ipairs(torrent.urlSeeds) do
        qBittorrent.debug(string.format("    %s", urlSeed))
    end
    qBittorrent.debug(string.format("  Has metadata: %s", tostring(torrent.hasMetadata)))
end

function onTorrentMetadataReady(torrent)
    qBittorrent.log(string.format("%s: Torrent '%s' has metadata ready.", NAME, torrent.name))

    qBittorrent.debug(string.format("Torrent: %s", torrent.name))
    qBittorrent.debug(string.format("  ID: %s", torrent.id))
    qBittorrent.debug(string.format("  Info hash v1: %s", torrent.infoHashV1))
    qBittorrent.debug(string.format("  Info hash v2: %s", torrent.infoHashV2))
    qBittorrent.debug(string.format("  Private: %s", tostring(torrent.private)))
    qBittorrent.debug(string.format("  Total size: %s", qBittorrent.friendlySizeUnit(torrent.totalSize)))
    qBittorrent.debug(string.format("  Piece size: %s", qBittorrent.friendlySizeUnit(torrent.pieceSize)))
    qBittorrent.debug(string.format("  Pieces count: %d", torrent.piecesCount))
    qBittorrent.debug(string.format("  Files count: %d", torrent.filesCount))
    qBittorrent.debug("  Files:")
    for i = 0, (torrent.filesCount - 1) do
        qBittorrent.debug(string.format("    %s (%s)", torrent:filePath(i)
                , qBittorrent.friendlySizeUnit(torrent:fileSize(i))))
    end
end

function onTorrentFinished(torrent)
    qBittorrent.log(string.format("%s: Torrent '%s' is finished.", NAME, torrent.name))
end

function onTorrentStarted(torrent)
    qBittorrent.log(string.format("%s: Torrent '%s' is started.", NAME, torrent.name) )
end

function onTorrentStopped(torrent)
    qBittorrent.log(string.format("%s: Torrent '%s' is stopped.", NAME, torrent.name))
end

function onTorrentSavePathChanged(torrent)
    qBittorrent.log(string.format("%s: Torrent '%s' has changed save path.", NAME, torrent.name))
end

function onTorrentSavingModeChanged(torrent)
    qBittorrent.log(string.format("%s: Torrent '%s' has changed saving mode.", NAME, torrent.name))
end

function onTorrentCategoryChanged(torrent, oldCategory)
    qBittorrent.log(string.format("%s: Torrent '%s' has changed category.", NAME, torrent.name))
    qBittorrent.debug(string.format("Torrent: %s. Old category: %s. New category: %s.", torrent.name, oldCategory, torrent.category))
end

function onTorrentTagAdded(torrent, tag)
    qBittorrent.log(string.format("%s: Torrent '%s' has tag added.", NAME, torrent.name))
    qBittorrent.debug(string.format("Torrent: %s. Added tag: %s.", torrent.name, tag))
end

function onTorrentTagRemoved(torrent, tag)
    qBittorrent.log(string.format("%s: Torrent '%s' has tag removed.", NAME, torrent.name))
    qBittorrent.debug(string.format("Torrent: %s. Removed tag: %s.", torrent.name, tag))
end

function onTorrentContentFileRenamed(torrent, fileIndex, oldPath)
    qBittorrent.log(string.format("%s: Torrent '%s' has file renamed.", NAME, torrent.name))
    qBittorrent.debug(string.format("Torrent: %s. Renamed file: '%s'. Old NAME: '%s'.", torrent.name, torrent:filePath(fileIndex), oldPath))
end

function onTorrentContentFolderRenamed(torrent, newPath, oldPath, renamedFiles)
    qBittorrent.log(string.format("%s: Torrent '%s' has folder renamed.", NAME, torrent.name))
    qBittorrent.debug(string.format("Torrent: %s. Renamed folder: '%s'. Old NAME: '%s'.", torrent.name, newPath, oldPath))
end

function onTorrentContentFolderRenamingFailed(torrent, newPath, oldPath, renamedFiles, failedFileIndexes)
    qBittorrent.log(string.format("%s: Torrent '%s' has folder renaming failed.", NAME, torrent.name))
    qBittorrent.debug(string.format("Torrent: %s. Folder: '%s'. New NAME: '%s'.", torrent.name, oldPath, newPath))
end

function onTorrentIOError(torrent, message)
    qBittorrent.log(string.format("%s: Torrent '%s' has IO error.", NAME, torrent.name))
    qBittorrent.debug(string.format("Torrent: %s. IO error message: %s.", torrent.name, message))
end

function onTorrentAboutToBeRemoved(torrent)
    qBittorrent.log(string.format("%s: Torrent '%s' is about to be removed.", NAME, torrent.name))
end

function onTorrentAnnounceSuccess(torrent, tracker)
    qBittorrent.log(string.format("%s: Torrent '%s' has announced successfully.", NAME, torrent.name))
    qBittorrent.debug(string.format("Torrent: %s. Announced tracker: %s.", torrent.name, tracker))
end

function onTorrentAnnounceWarning(torrent, tracker)
    qBittorrent.log(string.format("%s: Torrent '%s' has announced with warning.", NAME, torrent.name))
    qBittorrent.debug(string.format("Torrent: %s. Announced tracker: %s.", torrent.name, tracker))
end

function onTorrentAnnounceError(torrent, tracker)
    qBittorrent.log(string.format("%s: Torrent '%s' has failed announce.", NAME, torrent.name))
    qBittorrent.debug(string.format("Torrent: %s. Failed tracker: %s.", torrent.name, tracker))
end

function onTorrentTrackersAdded(torrent, addedTrackers)
    qBittorrent.log(string.format("%s: Torrent '%s' has added trackers.", NAME, torrent.name))

    qBittorrent.debug(string.format("Torrent: %s. Added trackers:", torrent.name))
    for _, tracker in ipairs(addedTrackers) do
        qBittorrent.debug(string.format("\t%s", tracker.url))
    end
end

function onTorrentTrackersRemoved(torrent, removedTrackers)
    qBittorrent.log(string.format("%s: Torrent '%s' has removed trackers.", NAME, torrent.name))
    qBittorrent.debug(string.format("Torrent: %s. Removed trackers:", torrent.name))
    for _, tracker in ipairs(removedTrackers) do
        qBittorrent.debug(string.format("\t%s", tracker))
    end
end

function onTorrentTrackerStatusesUpdated(torrent, updatedTrackers)
    qBittorrent.log(string.format("%s: Torrent '%s' has updated tracker statuses.", NAME, torrent.name))
    qBittorrent.debug(string.format("Torrent: %s. Updated trackers:", torrent.name))
    for trackerURL, trackerStatus in pairs(updatedTrackers) do
        qBittorrent.debug(string.format("\t%s", trackerURL))
    end
end
