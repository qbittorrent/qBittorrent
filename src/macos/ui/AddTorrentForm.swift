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
import Observation
import QBTBridge

/// The state of one "Add Torrent" dialog: the torrent being added and the
/// options the user chose, which start from the session's defaults.
@MainActor
@Observable
final class AddTorrentForm {
    let request: QBTAddTorrentRequest
    private(set) var content: TorrentContent?

    var category: String {
        didSet { categoryChanged() }
    }
    var tags: String = ""
    /// Automatic torrent management: the category decides the folders.
    var useAutoTMM: Bool {
        didSet { categoryChanged() }
    }
    var savePath: String
    var useDownloadPath: Bool
    var downloadPath: String
    var startTorrent: Bool
    var stopCondition: QBTStopCondition
    var sequential = false
    var firstLastPiecePriority = false
    var contentLayout: QBTContentLayout {
        didSet { layOutFiles() }
    }
    var skipHashCheck = false
    var addToQueueTop: Bool
    var keepTorrentFile = false
    var doNotShowAgain = false

    /// The manual folders, kept while the category decides them.
    private var manualSavePath: String
    private var manualDownloadPath: String
    private var manualUseDownloadPath: Bool

    /// `stopped` true forces the torrent to be added stopped; otherwise the
    /// session's default applies.
    init(request: QBTAddTorrentRequest, category: String, stopped: Bool) {
        self.request = request
        self.category = category
        useAutoTMM = QBTAddTorrentDefaults.autoTMMEnabled
        let savePath = QBTAddTorrentDefaults.savePath
        let downloadPath = QBTAddTorrentDefaults.downloadPath
        let useDownloadPath = QBTAddTorrentDefaults.downloadPathEnabled
        manualSavePath = savePath
        manualDownloadPath = downloadPath
        manualUseDownloadPath = useDownloadPath
        self.savePath = savePath
        self.downloadPath = downloadPath
        self.useDownloadPath = useDownloadPath
        startTorrent = !(stopped || QBTAddTorrentDefaults.addStopped)
        stopCondition = QBTAddTorrentDefaults.stopCondition
        contentLayout = QBTAddTorrentDefaults.contentLayout
        addToQueueTop = QBTAddTorrentDefaults.addToQueueTop
        categoryChanged()
        if request.hasMetadata {
            metadataArrived()
        }
    }

    var hasMetadata: Bool { content != nil }

    /// Same as AddNewTorrentDialog::updateMetadata(): a torrent that has its
    /// metadata cannot stop when it receives it, so it is added stopped.
    func metadataArrived() {
        guard content == nil, request.hasMetadata else { return }
        let priorities = request.initialFilePriorities.map { QBTDownloadPriority(rawValue: $0.intValue) ?? .normal }
        content = TorrentContent(sizes: request.fileSizes.map(\.int64Value), priorities: priorities,
                                 paths: request.filePaths(with: contentLayout))
        if stopCondition == .metadataReceived {
            startTorrent = false
            stopCondition = .none
        }
    }

    private func layOutFiles() {
        content?.layOut(request.filePaths(with: contentLayout))
    }

    /// Same as AddNewTorrentDialog::TMMChanged() and categoryChanged().
    private func categoryChanged() {
        if useAutoTMM {
            savePath = QBTAddTorrentDefaults.savePath(forCategory: category)
            downloadPath = QBTAddTorrentDefaults.downloadPath(forCategory: category)
            useDownloadPath = !downloadPath.isEmpty
        } else {
            savePath = manualSavePath
            downloadPath = manualDownloadPath
            useDownloadPath = manualUseDownloadPath
        }
    }

    /// Remembers what the user picked by hand, for when automatic management
    /// is turned off again.
    func rememberManualPaths() {
        guard !useAutoTMM else { return }
        manualSavePath = savePath
        manualDownloadPath = downloadPath
        manualUseDownloadPath = useDownloadPath
    }

    var tagList: [String] {
        var seen = Set<String>()
        return tags.split(separator: ",")
            .map { $0.trimmingCharacters(in: .whitespaces) }
            .filter { !$0.isEmpty && seen.insert($0).inserted }
    }

    var invalidTags: [String] {
        tagList.filter { !QBTAddTorrentDefaults.isValidTagName($0) }
    }

    func addTag(_ tag: String) {
        guard !tagList.contains(tag) else { return }
        tags = (tagList + [tag]).joined(separator: ", ")
    }

    var freeDiskSpace: Int64 {
        QBTAddTorrentDefaults.freeDiskSpace(atPath: savePath)
    }

    var canAdd: Bool {
        invalidTags.isEmpty && (useAutoTMM || !savePath.isEmpty)
    }

    var options: QBTAddTorrentOptions {
        let options = QBTAddTorrentOptions()
        options.category = category
        options.tags = tagList
        options.useAutoTMM = useAutoTMM
        options.savePath = savePath
        options.useDownloadPath = useDownloadPath
        options.downloadPath = downloadPath
        options.addStopped = !startTorrent
        options.stopCondition = startTorrent ? stopCondition : .none
        options.sequential = sequential
        options.firstLastPiecePriority = firstLastPiecePriority
        options.contentLayout = contentLayout
        options.skipHashCheck = skipHashCheck
        options.addToQueueTop = addToQueueTop
        options.filePriorities = content?.priorities.map { NSNumber(value: $0.rawValue) } ?? []
        options.keepTorrentFile = keepTorrentFile
        return options
    }
}
