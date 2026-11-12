/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  svbmwjwj <255624542+svbmwjwj@users.noreply.github.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

import Foundation
import QBTBridge

/// A torrent as one row of the table: a value copied from the core, so that
/// the table can sort and diff it.
struct TorrentItem: Identifiable, Hashable {
    let id: String
    let name: String
    let state: QBTTorrentState
    let errorMessage: String
    let size: Int64
    let progress: Double
    let downloadSpeed: Int64
    let uploadSpeed: Int64
    let eta: Int64
    let ratio: Double
    let downloaded: Int64
    let uploaded: Int64
    let seeds: Int
    let totalSeeds: Int
    let leechers: Int
    let totalLeechers: Int
    let category: String
    let tags: [String]
    let savePath: String
    let contentPath: String
    let tracker: String
    let addedOn: Date
    let completedOn: Date?
    let isPrivate: Bool
    let isForced: Bool
    let hasTrackerError: Bool
    let hasAnnounceError: Bool
    let trackerMessage: String

    init(_ torrent: QBTTorrent) {
        id = torrent.torrentID
        name = torrent.name
        state = torrent.state
        errorMessage = torrent.errorMessage
        size = torrent.size
        progress = torrent.progress
        downloadSpeed = torrent.downloadSpeed
        uploadSpeed = torrent.uploadSpeed
        eta = torrent.eta
        ratio = torrent.ratio
        downloaded = torrent.downloaded
        uploaded = torrent.uploaded
        seeds = Int(torrent.seeds)
        totalSeeds = Int(torrent.totalSeeds)
        leechers = Int(torrent.leechers)
        totalLeechers = Int(torrent.totalLeechers)
        category = torrent.category
        tags = torrent.tags
        savePath = torrent.savePath
        contentPath = torrent.contentPath
        tracker = torrent.tracker
        addedOn = torrent.addedOn
        completedOn = torrent.completedOn
        isPrivate = torrent.isPrivate
        isForced = torrent.isForced
        hasTrackerError = torrent.hasTrackerError
        hasAnnounceError = torrent.hasAnnounceError
        trackerMessage = torrent.trackerMessage
    }

    var isStopped: Bool { state == .stoppedDownloading || state == .stoppedUploading }
    var isErrored: Bool { state == .error || state == .missingFiles }
    var isActive: Bool { downloadSpeed > 0 || uploadSpeed > 0 }

    /// As `TorrentImpl::isDownloading()`.
    var isDownloading: Bool {
        switch state {
        case .downloading, .downloadingMetadata, .forcedDownloadingMetadata, .stalledDownloading,
             .checkingDownloading, .stoppedDownloading, .queuedDownloading, .forcedDownloading:
            true
        default:
            false
        }
    }

    /// As `TorrentImpl::isUploading()`.
    var isUploading: Bool {
        switch state {
        case .uploading, .stalledUploading, .checkingUploading, .queuedUploading, .forcedUploading: true
        default: false
        }
    }

    /// As `TorrentImpl::isCompleted()`.
    var isCompleted: Bool { isUploading || state == .stoppedUploading }

    var isChecking: Bool {
        state == .checkingUploading || state == .checkingDownloading || state == .checkingResumeData
    }

    /// As `Torrent::belongsToCategory()`: a torrent in `a/b` also belongs to `a`.
    func belongs(to category: String) -> Bool {
        self.category == category || self.category.hasPrefix(category + "/")
    }

    var trackerHost: String {
        URL(string: tracker)?.host() ?? tracker
    }

    var statusText: String {
        switch state {
        case .forcedDownloading: String(localized: "[F] Downloading")
        case .downloading: String(localized: "Downloading")
        case .forcedDownloadingMetadata: String(localized: "[F] Downloading metadata")
        case .downloadingMetadata: String(localized: "Downloading metadata")
        case .stalledDownloading: String(localized: "Stalled")
        case .forcedUploading: String(localized: "[F] Seeding")
        case .uploading, .stalledUploading: String(localized: "Seeding")
        case .queuedDownloading, .queuedUploading: String(localized: "Queued")
        case .checkingDownloading, .checkingUploading: String(localized: "Checking")
        case .checkingResumeData: String(localized: "Checking resume data")
        case .stoppedDownloading: String(localized: "Stopped")
        case .stoppedUploading: String(localized: "Completed")
        case .moving: String(localized: "Moving")
        case .missingFiles: String(localized: "Missing Files")
        case .error: String(localized: "Errored")
        default: String(localized: "Unknown")
        }
    }
}

/// The status filters of the sidebar, as `TorrentFilter` in the Qt UI.
enum StatusFilter: String, CaseIterable, Identifiable {
    case all, downloading, seeding, completed, running, stopped, active, inactive, stalled, checking, moving, errored

    var id: Self { self }

    /// Shown until the user picks others: the ones most used to find a torrent.
    static let defaultFilters: Set<StatusFilter> = [downloading, seeding, completed, stopped, active, errored]

    var title: String {
        switch self {
        case .all: String(localized: "All")
        case .downloading: String(localized: "Downloading")
        case .seeding: String(localized: "Seeding")
        case .completed: String(localized: "Completed")
        case .running: String(localized: "Running")
        case .stopped: String(localized: "Stopped")
        case .active: String(localized: "Active")
        case .inactive: String(localized: "Inactive")
        case .stalled: String(localized: "Stalled")
        case .checking: String(localized: "Checking")
        case .moving: String(localized: "Moving")
        case .errored: String(localized: "Errored")
        }
    }

    var systemImage: String {
        switch self {
        case .all: "tray.full"
        case .downloading: "arrow.down.circle"
        case .seeding: "arrow.up.circle"
        case .completed: "checkmark.circle"
        case .running: "play.circle"
        case .stopped: "stop.circle"
        case .active: "bolt.circle"
        case .inactive: "moon.circle"
        case .stalled: "exclamationmark.arrow.circlepath"
        case .checking: "magnifyingglass.circle"
        case .moving: "folder.circle"
        case .errored: "exclamationmark.triangle"
        }
    }

    func matches(_ torrent: TorrentItem) -> Bool {
        switch self {
        case .all: true
        case .downloading: torrent.isDownloading
        case .seeding: torrent.isUploading
        case .completed: torrent.isCompleted
        case .running: !torrent.isStopped
        case .stopped: torrent.isStopped
        case .active: torrent.isActive
        case .inactive: !torrent.isActive
        case .stalled: torrent.state == .stalledUploading || torrent.state == .stalledDownloading
        case .checking: torrent.isChecking
        case .moving: torrent.state == .moving
        case .errored: torrent.isErrored
        }
    }
}

/// Filters on what the trackers say, as the Qt UI's tracker filter list.
enum TrackerFilter: Hashable {
    case trackerError
    case unreachable
    case trackerless
    case host(String)

    func matches(_ torrent: TorrentItem) -> Bool {
        switch self {
        case .trackerError: torrent.hasTrackerError
        case .unreachable: torrent.hasAnnounceError
        case .trackerless: torrent.tracker.isEmpty
        case .host(let host): torrent.trackerHost == host
        }
    }
}

enum SidebarSelection: Hashable {
    case status(StatusFilter)
    case allCategories
    case uncategorized
    case category(String)
    case tracker(TrackerFilter)
    case allTags
    case untagged
    case tag(String)
}

struct CategoryItem: Identifiable, Hashable {
    var id: String { name }
    let name: String
    let savePath: String
    let configuredSavePath: String

    init(_ category: QBTCategory) {
        name = category.name
        savePath = category.savePath
        configuredSavePath = category.configuredSavePath
    }
}
