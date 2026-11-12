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

import AppKit
import QBTBridge

/// The sheets that per-torrent actions open over the main window.
enum TorrentSheet: Identifiable, Hashable {
    case rename(TorrentItem)
    /// Speed and share limits.
    case options(Set<TorrentItem.ID>)
    case addTags(Set<TorrentItem.ID>)
    case newTag
    case addTrackers(Set<TorrentItem.ID>)

    var id: Self { self }
}

/// Per-torrent actions, as the Qt UI's transfer list context menu offers.
extension SessionModel {
    func options(_ ids: Set<TorrentItem.ID>) -> [QBTTorrentOptionsState] {
        ids.compactMap { QBTTorrentOptions.options(forTorrent: $0) }
    }

    /// Asks before a recheck, unless the user turned that off.
    func confirmRecheck() -> Bool {
        guard QBTAdvancedSettings.confirmTorrentRecheck else { return true }
        let alert = NSAlert()
        alert.messageText = String(localized: "Recheck the selected torrents?")
        alert.addButton(withTitle: String(localized: "Recheck"))
        alert.addButton(withTitle: String(localized: "Cancel"))
        return alert.runModal() == .alertFirstButtonReturn
    }

    func forceStart(_ ids: Set<TorrentItem.ID>) {
        QBTTorrentOptions.forceStartTorrents(Array(ids))
    }

    // MARK: Moving and naming

    /// Asks for a folder and moves the torrents' files there.
    func chooseLocation(for ids: Set<TorrentItem.ID>) {
        let panel = NSOpenPanel()
        panel.message = String(localized: "Choose the folder to move the files to")
        panel.prompt = String(localized: "Move")
        panel.canChooseFiles = false
        panel.canChooseDirectories = true
        panel.canCreateDirectories = true
        if let first = torrents(ids).first {
            panel.directoryURL = URL(fileURLWithPath: first.savePath)
        }
        if panel.runModal() == .OK, let url = panel.url {
            QBTTorrentOptions.moveTorrents(Array(ids), toFolder: url.path)
        }
    }

    func rename(_ id: TorrentItem.ID, to name: String) {
        QBTTorrentOptions.renameTorrent(id, to: name)
        refresh()
    }

    // MARK: Toggles

    func setSequentialDownload(_ enabled: Bool, for ids: Set<TorrentItem.ID>) {
        QBTTorrentOptions.setSequentialDownload(enabled, forTorrents: Array(ids))
    }

    func setFirstLastPiecePriority(_ enabled: Bool, for ids: Set<TorrentItem.ID>) {
        QBTTorrentOptions.setFirstLastPiecePriority(enabled, forTorrents: Array(ids))
    }

    func setSuperSeeding(_ enabled: Bool, for ids: Set<TorrentItem.ID>) {
        QBTTorrentOptions.setSuperSeeding(enabled, forTorrents: Array(ids))
    }

    /// Asks first when turning it on, since the files may move.
    func setAutoManagement(_ enabled: Bool, for ids: Set<TorrentItem.ID>) {
        if enabled {
            let alert = NSAlert()
            alert.messageText = String(localized: "Turn on automatic torrent management?")
            alert.informativeText = String(localized: "The files of the selected torrents may move to their category's folder.")
            alert.addButton(withTitle: String(localized: "Turn On"))
            alert.addButton(withTitle: String(localized: "Cancel"))
            guard alert.runModal() == .alertFirstButtonReturn else { return }
        }
        QBTTorrentOptions.setAutoManagement(enabled, forTorrents: Array(ids))
        refresh()
    }

    // MARK: Tags

    func addTag(_ tag: String, to ids: Set<TorrentItem.ID>) {
        QBTTorrentOptions.addTag(tag, toTorrents: Array(ids))
        refresh()
    }

    func removeTag(_ tag: String, from ids: Set<TorrentItem.ID>) {
        QBTTorrentOptions.removeTag(tag, fromTorrents: Array(ids))
        refresh()
    }

    func removeAllTags(from ids: Set<TorrentItem.ID>) {
        if QBTAdvancedSettings.confirmRemoveAllTags {
            let alert = NSAlert()
            alert.messageText = String(localized: "Remove all tags from the selected torrents?")
            alert.addButton(withTitle: String(localized: "Remove"))
            alert.addButton(withTitle: String(localized: "Cancel"))
            guard alert.runModal() == .alertFirstButtonReturn else { return }
        }
        QBTTorrentOptions.removeAllTags(fromTorrents: Array(ids))
        refresh()
    }

    func deleteTag(_ tag: String) {
        if sidebarSelection == .tag(tag) {
            sidebarSelection = .allTags
        }
        QBTTorrentOptions.deleteTag(tag)
        refresh()
    }

    // MARK: Queue

    enum QueueMove {
        case up, down, top, bottom
    }

    var isQueueingEnabled: Bool { QBTTorrentOptions.isQueueingEnabled }

    func moveInQueue(_ ids: Set<TorrentItem.ID>, _ move: QueueMove) {
        let list = Array(ids)
        switch move {
        case .up: QBTTorrentOptions.increaseQueuePosition(ofTorrents: list)
        case .down: QBTTorrentOptions.decreaseQueuePosition(ofTorrents: list)
        case .top: QBTTorrentOptions.moveTorrents(toQueueTop: list)
        case .bottom: QBTTorrentOptions.moveTorrents(toQueueBottom: list)
        }
    }

    // MARK: Copy and export

    func copy(_ lines: [String]) {
        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(lines.joined(separator: "\n"), forType: .string)
    }

    /// Asks for a folder and saves the torrents' .torrent files there.
    func exportTorrents(_ ids: Set<TorrentItem.ID>) {
        let panel = NSOpenPanel()
        panel.message = String(localized: "Choose the folder to save the .torrent files to")
        panel.prompt = String(localized: "Export")
        panel.canChooseFiles = false
        panel.canChooseDirectories = true
        panel.canCreateDirectories = true
        guard panel.runModal() == .OK, let url = panel.url else { return }

        let errors = QBTTorrentOptions.exportTorrents(Array(ids), toFolder: url.path)
        guard !errors.isEmpty else { return }
        let alert = NSAlert()
        alert.alertStyle = .warning
        alert.messageText = String(localized: "Some .torrent files could not be exported.")
        alert.informativeText = errors.joined(separator: "\n")
        alert.runModal()
    }
}
