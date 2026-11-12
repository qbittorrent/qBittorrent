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

/// Shown in place of the search page's content until Python is ready.
/// The search plugins are Python scripts; qBittorrent does not install
/// Python itself on macOS.
struct PythonMissingView: View {
    @Environment(SearchModel.self) private var model

    var body: some View {
        let python = model.python
        VStack(spacing: 16) {
            Image(systemName: "puzzlepiece.extension")
                .font(.system(size: 44))
                .foregroundStyle(.secondary)
            Text(model.pythonStatus == .outdated ? "Python Is Out of Date" : "Python Is Required")
                .font(.title2.bold())
            Text(explanation(python))
                .multilineTextAlignment(.center)
                .frame(maxWidth: 460)
            VStack(alignment: .leading, spacing: 6) {
                Text("To install it, do one of these:")
                Text("• Download the installer from python.org.")
                Text("• Or, with Homebrew, run this in Terminal:")
                Text(verbatim: "brew install python")
                    .font(.body.monospaced())
                    .textSelection(.enabled)
                    .padding(.leading, 14)
                Text("qBittorrent looks for python3 in the PATH. If you start it from the Finder, that may not include Homebrew’s folder; choose the Python program instead.")
                    .font(.callout)
                    .foregroundStyle(.secondary)
                    .padding(.top, 4)
            }
            .frame(maxWidth: 460, alignment: .leading)
            HStack {
                Link("Download Python", destination: URL(string: "https://www.python.org/downloads/macos/")!)
                Button("Choose Python…") { choosePython() }
                Button("Check Again") { model.checkPython() }
                    .keyboardShortcut(.defaultAction)
            }
            .padding(.top, 4)
        }
        .padding(32)
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }

    private func explanation(_ python: QBTSearchEngine) -> String {
        if model.pythonStatus == .outdated {
            return String(localized: "The search engine needs Python \(python.minimumPythonVersion) or later. The Python at \(python.pythonPath) is version \(python.pythonVersion).")
        }
        return String(localized: "The search engine runs search plugins written in Python, and needs Python \(python.minimumPythonVersion) or later. qBittorrent could not find it.")
    }

    private func choosePython() {
        let panel = NSOpenPanel()
        panel.message = String(localized: "Choose the Python program, such as python3")
        panel.directoryURL = URL(fileURLWithPath: "/opt/homebrew/bin")
        panel.treatsFilePackagesAsDirectories = true
        if panel.runModal() == .OK, let url = panel.url {
            model.python.preferredPythonPath = url.path
            model.checkPython()
        }
    }
}
