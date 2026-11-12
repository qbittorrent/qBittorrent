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

/// Opens one "Add Torrent" window for each torrent the user adds through
/// this front-end, as the Qt UI's GUIAddTorrentManager does. The windows are
/// not modal, so several torrents can be looked at side by side.
@MainActor
final class AddTorrentWindows {
    static let shared = AddTorrentWindows()

    /// Open windows, by torrent ID.
    private var controllers: [String: AddTorrentWindowController] = [:]

    /// `stopped` true forces the torrents to be added stopped.
    func present(_ sources: [String], category: String, stopped: Bool, model: SessionModel) {
        for source in sources {
            QBTAddTorrentRequest.loadSource(source) { request, error in
                MainActor.assumeIsolated {
                    if let request {
                        self.show(request, category: category, stopped: stopped, model: model)
                    } else {
                        self.showError(String(localized: "Could not add “\(source)”."), details: error ?? "")
                    }
                }
            }
        }
    }

    private func show(_ request: QBTAddTorrentRequest, category: String, stopped: Bool, model: SessionModel) {
        if let existing = controllers[request.torrentID] {
            existing.window?.makeKeyAndOrderFront(nil)
            return
        }

        if request.isDuplicate {
            let details: String = switch request.mergeIntoExistingTorrent() {
            case .merged: String(localized: "Its trackers and web seeds were added to the torrent in the list.")
            case .private: String(localized: "Trackers cannot be merged because it is a private torrent.")
            default: String(localized: "Merging of trackers is turned off.")
            }
            showError(String(localized: "“\(request.name)” is already in the list."), details: details)
            return
        }

        let form = AddTorrentForm(request: request, category: category, stopped: stopped)
        let id = request.torrentID
        let controller = AddTorrentWindowController(form: form, model: model) { [weak self] in
            self?.controllers[id] = nil
        }
        controllers[id] = controller
        request.onMetadataReceived = { [weak form] in
            MainActor.assumeIsolated { form?.metadataArrived() }
        }
        request.downloadMetadata()
        controller.showWindow(nil)
        NSApp.activate()
    }

    private func showError(_ message: String, details: String) {
        let alert = NSAlert()
        alert.messageText = message
        alert.informativeText = details
        alert.runModal()
    }
}

/// One "Add Torrent" window. Closing it cancels the torrent.
@MainActor
final class AddTorrentWindowController: NSWindowController, NSWindowDelegate {
    private let form: AddTorrentForm
    private let onClose: () -> Void
    private var isFinished = false

    init(form: AddTorrentForm, model: SessionModel, onClose: @escaping () -> Void) {
        self.form = form
        self.onClose = onClose
        let window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 960, height: 640),
                              styleMask: [.titled, .closable, .miniaturizable, .resizable],
                              backing: .buffered, defer: true)
        window.isReleasedWhenClosed = false
        super.init(window: window)

        window.delegate = self
        window.title = form.request.name.isEmpty ? String(localized: "Magnet Link") : form.request.name
        window.contentViewController = NSHostingController(rootView: AddTorrentDialog(form: form) { [weak self] add in
            self?.finish(add: add)
        }
        .environment(model))
        window.setContentSize(NSSize(width: 960, height: 640))
        window.center()
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) is not supported")
    }

    private func finish(add: Bool) {
        guard !isFinished else { return }
        if add {
            QBTAddTorrentDefaults.dialogEnabled = !form.doNotShowAgain
            if !form.request.add(with: form.options) {
                let alert = NSAlert()
                alert.messageText = String(localized: "Could not add “\(form.request.name)”.")
                alert.informativeText = String(localized: "See the log for details.")
                alert.runModal()
            }
        } else {
            form.request.cancel()
        }
        isFinished = true
        close()
    }

    func windowWillClose(_ notification: Notification) {
        if !isFinished {
            isFinished = true
            form.request.cancel()
        }
        form.request.onMetadataReceived = nil
        onClose()
    }
}
