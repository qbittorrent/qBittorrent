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
import SwiftUI

/// A search field in the toolbar that shows as a magnifying glass until it
/// is clicked, and folds back once it is left empty, as the toolbar search
/// of Mail does when the window is narrow.
struct FilterToolbarField: View {
    @Binding var text: String
    let prompt: String
    let help: LocalizedStringKey
    @State private var isExpanded = false

    var body: some View {
        if isExpanded || !text.isEmpty {
            SearchField(text: $text, prompt: prompt) {
                if text.isEmpty { isExpanded = false }
            }
            .frame(width: 200)
        } else {
            Button {
                isExpanded = true
            } label: {
                Label(prompt, systemImage: "magnifyingglass")
            }
            .help(help)
        }
    }
}

/// AppKit's search field, which takes the focus when it appears and reports
/// when the user leaves it.
private struct SearchField: NSViewRepresentable {
    @Binding var text: String
    let prompt: String
    let onEndEditing: () -> Void

    func makeNSView(context: Context) -> NSSearchField {
        let field = NSSearchField()
        field.placeholderString = prompt
        field.sendsSearchStringImmediately = true
        field.delegate = context.coordinator
        field.stringValue = text
        DispatchQueue.main.async {
            field.window?.makeFirstResponder(field)
        }
        return field
    }

    func updateNSView(_ field: NSSearchField, context: Context) {
        context.coordinator.parent = self
        if field.stringValue != text {
            field.stringValue = text
        }
    }

    func makeCoordinator() -> Coordinator {
        Coordinator(parent: self)
    }

    @MainActor
    final class Coordinator: NSObject, NSSearchFieldDelegate {
        var parent: SearchField

        init(parent: SearchField) {
            self.parent = parent
        }

        func controlTextDidChange(_ notification: Notification) {
            guard let field = notification.object as? NSSearchField else { return }
            parent.text = field.stringValue
        }

        func controlTextDidEndEditing(_ notification: Notification) {
            parent.onEndEditing()
        }
    }
}
