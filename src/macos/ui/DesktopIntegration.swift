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
import Observation
import QBTBridge

/// Hands the session's state to macOS: transfer speeds on the Dock tile and
/// in the menu bar, notifications, and a question before quitting. The
/// General settings pane changes it.
@MainActor
@Observable
final class DesktopIntegration {
    static let shared = DesktopIntegration()

    var isSpeedInDockShown = QBTDesktopSettings.speedInDockEnabled {
        didSet {
            QBTDesktopSettings.speedInDockEnabled = isSpeedInDockShown
            updateDockTile()
        }
    }

    /// Off unless the user turns it on.
    var isMenuBarItemShown = UserDefaults.standard.bool(forKey: "MenuBarItem/Shown") {
        didSet {
            UserDefaults.standard.set(isMenuBarItemShown, forKey: "MenuBarItem/Shown")
            updateMenuBarItem()
        }
    }

    var areNotificationsEnabled = QBTDesktopSettings.notificationsEnabled {
        didSet {
            QBTDesktopSettings.notificationsEnabled = areNotificationsEnabled
            if areNotificationsEnabled { notifier.requestAuthorization() }
        }
    }

    var areTorrentAddedNotificationsEnabled = QBTDesktopSettings.torrentAddedNotificationsEnabled {
        didSet { QBTDesktopSettings.torrentAddedNotificationsEnabled = areTorrentAddedNotificationsEnabled }
    }

    var confirmsQuit = QBTDesktopSettings.confirmOnExit {
        didSet { QBTDesktopSettings.confirmOnExit = confirmsQuit }
    }

    @ObservationIgnored let notifier = Notifier()
    @ObservationIgnored private var model: SessionModel?
    @ObservationIgnored private let dockTileView = DockTileView()
    @ObservationIgnored private var menuBarItem: MenuBarItem?

    /// Called once the app has finished launching.
    func start(model: SessionModel) {
        self.model = model
        notifier.onOpenTorrent = { [weak self] id in
            self?.showTorrent(id)
        }
        notifier.start(requestingAuthorization: areNotificationsEnabled)
        QBTDesktopEvents.shared.onEvent = { [weak self] event in
            self?.notify(event)
        }
        updateMenuBarItem()
        observeSpeeds()
    }

    // MARK: Speeds

    private func observeSpeeds() {
        guard let model else { return }
        withObservationTracking {
            _ = model.downloadSpeed
            _ = model.uploadSpeed
        } onChange: { [weak self] in
            // Called before the change; look once it is made.
            Task { @MainActor in
                self?.updateDockTile()
                self?.menuBarItem?.update()
                self?.observeSpeeds()
            }
        }
        updateDockTile()
        menuBarItem?.update()
    }

    private func updateDockTile() {
        let dockTile = NSApp.dockTile
        guard isSpeedInDockShown, let model else {
            if dockTile.contentView != nil {
                dockTile.contentView = nil
                dockTile.display()
            }
            return
        }
        if dockTile.contentView !== dockTileView {
            dockTile.contentView = dockTileView
        }
        if dockTileView.setSpeeds(download: model.downloadSpeed, upload: model.uploadSpeed) {
            dockTile.display()
        }
    }

    private func updateMenuBarItem() {
        guard let model else { return }
        if isMenuBarItemShown, menuBarItem == nil {
            menuBarItem = MenuBarItem(model: model)
        } else if !isMenuBarItemShown, let item = menuBarItem {
            item.remove()
            menuBarItem = nil
        }
    }

    // MARK: Notifications

    private func notify(_ event: QBTDesktopEvent) {
        guard areNotificationsEnabled else { return }
        switch event.kind {
        case .downloadFinished:
            notifier.post(title: String(localized: "Download completed"),
                          body: String(localized: "“\(event.name)” has finished downloading."),
                          torrentID: event.torrentID)
        case .torrentAdded:
            guard areTorrentAddedNotificationsEnabled else { return }
            notifier.post(title: String(localized: "Torrent added"),
                          body: String(localized: "“\(event.name)” was added."),
                          torrentID: event.torrentID)
        case .addFailed:
            notifier.post(title: String(localized: "Couldn’t add torrent"),
                          body: String(localized: "“\(event.name)”: \(event.message)"),
                          torrentID: "")
        case .ioError:
            notifier.post(title: String(localized: "I/O error"),
                          body: String(localized: "An I/O error occurred for “\(event.name)”: \(event.message)"),
                          torrentID: event.torrentID)
        @unknown default:
            return
        }
    }

    /// Shows the torrent selected in the main window, whatever the filters.
    func showTorrent(_ id: String) {
        AppDelegate.shared.showMainWindow()
        NSApp.activate()
        guard let model, model.torrents.contains(where: { $0.id == id }) else { return }
        model.sidebarSelection = .status(.all)
        model.searchText = ""
        model.selection = [id]
    }

    // MARK: Quitting

    /// Asks before quitting while torrents transfer data, as the Qt UI does.
    func shouldQuit() -> Bool {
        guard confirmsQuit, !isSystemQuitting,
              let model, model.torrents.contains(where: { $0.downloadSpeed > 0 || $0.uploadSpeed > 0 })
        else { return true }

        AppDelegate.shared.showMainWindow()
        NSApp.activate()
        let alert = NSAlert()
        alert.messageText = String(localized: "Quit qBittorrent?")
        alert.informativeText = String(localized: "Some files are currently transferring.")
        alert.addButton(withTitle: String(localized: "Cancel"))
        alert.addButton(withTitle: String(localized: "Quit"))
        alert.showsSuppressionButton = true
        alert.suppressionButton?.title = String(localized: "Don’t ask again")
        guard alert.runModal() == .alertSecondButtonReturn else { return false }
        if alert.suppressionButton?.state == .on {
            confirmsQuit = false
        }
        return true
    }

    /// Logging out, restarting or shutting down, which a dialog must not hold up.
    private var isSystemQuitting: Bool {
        let event = NSAppleEventManager.shared().currentAppleEvent
        return event?.attributeDescriptor(forKeyword: AEKeyword(kAEQuitReason)) != nil
    }
}
