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
import ServiceManagement
import SwiftUI

/// How the app fits in with macOS: login, Dock, menu bar, notifications,
/// default app, confirmations and the log.
struct GeneralPane: View {
    @Bindable private var desktop = DesktopIntegration.shared
    @State private var loginItemStatus = LoginItem.status
    @State private var torrentFileHandler = DefaultApp.torrentFileHandler
    @State private var magnetLinkHandler = DefaultApp.magnetLinkHandler
    @State private var areNotificationsDenied = false
    @State private var errorMessage: String?
    @State private var revision = 0

    var body: some View {
        Form {
            Section {
                Toggle("Open at login", isOn: Binding {
                    loginItemStatus == .enabled || loginItemStatus == .requiresApproval
                } set: { enabled in
                    do {
                        try LoginItem.setEnabled(enabled)
                    } catch {
                        errorMessage = error.localizedDescription
                    }
                    loginItemStatus = LoginItem.status
                })
                if loginItemStatus == .requiresApproval {
                    LabeledContent {
                        Button("Open Login Items Settings…") { LoginItem.openSettings() }
                    } label: {
                        Text("Allow qBittorrent in Login Items to finish.")
                            .foregroundStyle(.secondary)
                    }
                }
            }
            Section("Dock and menu bar") {
                Toggle("Show transfer speeds on the Dock icon", isOn: $desktop.isSpeedInDockShown)
                Toggle("Show transfer speeds in the menu bar", isOn: $desktop.isMenuBarItemShown)
            }
            Section {
                Toggle("Show notifications", isOn: $desktop.areNotificationsEnabled)
                Toggle("Also when a torrent is added", isOn: $desktop.areTorrentAddedNotificationsEnabled)
                    .disabled(!desktop.areNotificationsEnabled)
                if desktop.areNotificationsEnabled, areNotificationsDenied {
                    LabeledContent {
                        Button("Open Notification Settings…", action: openNotificationSettings)
                    } label: {
                        Text("Notifications are turned off for qBittorrent in System Settings.")
                            .foregroundStyle(.secondary)
                    }
                }
            } header: {
                Text("Notifications")
            } footer: {
                Text("For finished downloads, torrents that could not be added, and disk errors.")
            }
            Section("Default app") {
                LabeledContent("Torrent files", value: appName(torrentFileHandler))
                LabeledContent("Magnet links", value: appName(magnetLinkHandler))
                Button("Make qBittorrent the Default App") {
                    Task {
                        do {
                            try await DefaultApp.makeThisAppDefault()
                        } catch {
                            errorMessage = error.localizedDescription
                        }
                        refresh()
                    }
                }
                .disabled(DefaultApp.isThisApp(torrentFileHandler) && DefaultApp.isThisApp(magnetLinkHandler))
            }
            Section {
                Toggle("Ask before quitting while torrents are transferring", isOn: $desktop.confirmsQuit)
                Toggle("Ask before removing torrents", isOn: setting($revision, get: { QBTBehaviorSettings.confirmTorrentDeletion }, set: { QBTBehaviorSettings.confirmTorrentDeletion = $0 }))
            } footer: {
                Text("Without asking, Remove keeps the torrents' files.")
            }
            LogFileSection()
        }
        .task { refresh() }
        // The user may have changed these in System Settings meanwhile.
        .onReceive(NotificationCenter.default.publisher(for: NSApplication.didBecomeActiveNotification)) { _ in
            refresh()
        }
        .onChange(of: desktop.areNotificationsEnabled) { refresh() }
        .alert("The setting could not be changed.", isPresented: Binding {
            errorMessage != nil
        } set: { shown in
            if !shown { errorMessage = nil }
        }) {
            Button("OK") {}
        } message: {
            Text(errorMessage ?? "")
        }
    }

    private func refresh() {
        loginItemStatus = LoginItem.status
        torrentFileHandler = DefaultApp.torrentFileHandler
        magnetLinkHandler = DefaultApp.magnetLinkHandler
        Task {
            areNotificationsDenied = await desktop.notifier.isDenied()
        }
    }

    private func appName(_ url: URL?) -> String {
        guard let url else { return String(localized: "None") }
        let name = FileManager.default.displayName(atPath: url.path)
        return DefaultApp.isThisApp(url) ? String(localized: "\(name) (this app)") : name
    }

    private func openNotificationSettings() {
        let id = Bundle.main.bundleIdentifier ?? ""
        if let url = URL(string: "x-apple.systempreferences:com.apple.Notifications-Settings.extension?id=\(id)") {
            NSWorkspace.shared.open(url)
        }
    }
}
