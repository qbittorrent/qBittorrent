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
import SwiftUI

/// Folders whose .torrent files are added as they appear.
struct WatchedFoldersSection: View {
    @State private var folders = QBTDownloadSettings.watchedFolders

    var body: some View {
        Section {
            ForEach(folders, id: \.path) { folder in
                HStack {
                    Text(folder.path)
                        .lineLimit(1)
                        .truncationMode(.middle)
                        .help(folder.path)
                    Spacer()
                    Toggle("Include subfolders", isOn: Binding {
                        folder.recursive
                    } set: {
                        QBTDownloadSettings.setWatchedFolder(folder.path, recursive: $0)
                        reload()
                    })
                    .toggleStyle(.checkbox)
                    Button {
                        QBTDownloadSettings.removeWatchedFolder(folder.path)
                        reload()
                    } label: {
                        Image(systemName: "minus.circle")
                    }
                    .buttonStyle(.borderless)
                    .help("Stop watching this folder")
                }
            }
            Button("Add Folder…", action: add)
        } header: {
            Text("Watched folders")
        } footer: {
            Text(".torrent files saved in these folders are added automatically.")
        }
    }

    private func add() {
        let panel = NSOpenPanel()
        panel.canChooseFiles = false
        panel.canChooseDirectories = true
        panel.canCreateDirectories = true
        guard panel.runModal() == .OK, let url = panel.url else { return }
        QBTDownloadSettings.setWatchedFolder(url.path, recursive: false)
        reload()
    }

    private func reload() {
        folders = QBTDownloadSettings.watchedFolders
    }
}

/// An email for each finished torrent.
struct MailNotificationSection: View {
    @State private var revision = 0

    var body: some View {
        let _ = revision
        Section("Email when a torrent finishes") {
            Toggle("Send an email", isOn: setting($revision, get: { QBTDownloadSettings.mailEnabled }, set: { QBTDownloadSettings.mailEnabled = $0 }))
            if QBTDownloadSettings.mailEnabled {
                TextField("From", text: setting($revision, get: { QBTDownloadSettings.mailSender }, set: { QBTDownloadSettings.mailSender = $0 }))
                TextField("To", text: setting($revision, get: { QBTDownloadSettings.mailRecipient }, set: { QBTDownloadSettings.mailRecipient = $0 }))
                TextField("SMTP server", text: setting($revision, get: { QBTDownloadSettings.mailServer }, set: { QBTDownloadSettings.mailServer = $0 }), prompt: Text("smtp.example.com:587"))
                Picker("Encryption", selection: setting($revision, get: { QBTDownloadSettings.mailEncryption }, set: { QBTDownloadSettings.mailEncryption = $0 })) {
                    Text("None").tag(Int32(0))
                    Text("STARTTLS").tag(Int32(1))
                    Text("SSL/TLS").tag(Int32(2))
                }
                Toggle("Authentication", isOn: setting($revision, get: { QBTDownloadSettings.mailAuthEnabled }, set: { QBTDownloadSettings.mailAuthEnabled = $0 }))
                if QBTDownloadSettings.mailAuthEnabled {
                    TextField("Username", text: setting($revision, get: { QBTDownloadSettings.mailUsername }, set: { QBTDownloadSettings.mailUsername = $0 }))
                    SecureField("Password", text: setting($revision, get: { QBTDownloadSettings.mailPassword }, set: { QBTDownloadSettings.mailPassword = $0 }))
                }
            }
        }
    }
}

/// Programs run when a torrent is added or finishes.
struct ExternalProgramsSection: View {
    @State private var revision = 0

    var body: some View {
        let _ = revision
        Section {
            Toggle("Run a program when a torrent is added", isOn: setting($revision, get: { QBTDownloadSettings.runOnAddedEnabled }, set: { QBTDownloadSettings.runOnAddedEnabled = $0 }))
            if QBTDownloadSettings.runOnAddedEnabled {
                TextField("Command", text: setting($revision, get: { QBTDownloadSettings.runOnAddedProgram }, set: { QBTDownloadSettings.runOnAddedProgram = $0 }))
                    .font(.body.monospaced())
            }
            Toggle("Run a program when a torrent finishes", isOn: setting($revision, get: { QBTDownloadSettings.runOnFinishedEnabled }, set: { QBTDownloadSettings.runOnFinishedEnabled = $0 }))
            if QBTDownloadSettings.runOnFinishedEnabled {
                TextField("Command", text: setting($revision, get: { QBTDownloadSettings.runOnFinishedProgram }, set: { QBTDownloadSettings.runOnFinishedProgram = $0 }))
                    .font(.body.monospaced())
            }
        } header: {
            Text("External programs")
        } footer: {
            Text("""
                In the command: %N name, %L category, %G tags, %F content path, %R root path, %D save path, \
                %C number of files, %Z size in bytes, %T current tracker, %I info hash v1, %J info hash v2, \
                %K torrent ID, %M comment. Put quotation marks around a parameter that may contain spaces, as in "%N".
                """)
        }
    }
}
