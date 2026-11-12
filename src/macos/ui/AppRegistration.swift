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
import ServiceManagement
import UniformTypeIdentifiers

/// Opening the app at login, through the system's login items.
@MainActor
enum LoginItem {
    static var status: SMAppService.Status {
        SMAppService.mainApp.status
    }

    static func setEnabled(_ enabled: Bool) throws {
        if enabled {
            try SMAppService.mainApp.register()
        } else {
            try SMAppService.mainApp.unregister()
        }
    }

    /// Where the user approves a login item that needs it.
    static func openSettings() {
        SMAppService.openSystemSettingsLoginItems()
    }
}

/// The app that opens `.torrent` files and magnet links. The bundle's
/// Info.plist declares both.
@MainActor
enum DefaultApp {
    private static let torrentType = UTType(filenameExtension: "torrent", conformingTo: .data) ?? .data
    private static let magnetScheme = "magnet"

    static var torrentFileHandler: URL? {
        NSWorkspace.shared.urlForApplication(toOpen: torrentType)
    }

    static var magnetLinkHandler: URL? {
        URL(string: "\(magnetScheme):").flatMap { NSWorkspace.shared.urlForApplication(toOpen: $0) }
    }

    static func isThisApp(_ url: URL?) -> Bool {
        url?.resolvingSymlinksInPath().standardizedFileURL == Bundle.main.bundleURL.resolvingSymlinksInPath().standardizedFileURL
    }

    /// The system may ask the user to confirm.
    static func makeThisAppDefault() async throws {
        let app = Bundle.main.bundleURL
        try await NSWorkspace.shared.setDefaultApplication(at: app, toOpen: torrentType)
        try await NSWorkspace.shared.setDefaultApplication(at: app, toOpenURLsWithScheme: magnetScheme)
    }
}
