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

/// The torrent table with the details panel under it, resized by dragging
/// the line between them, as the Qt UI's splitter. The panel's height is
/// remembered.
struct DetailsSplit: View {
    private static let minPanelHeight = 120.0
    private static let minTableHeight = 120.0
    /// The area around the line that can be dragged.
    private static let handleHeight = 8.0

    @Environment(SessionModel.self) private var model
    @AppStorage("Details/Height") private var storedHeight = 260.0
    /// The height while the line is dragged; stored when the drag ends.
    @State private var draggedHeight: Double?
    @State private var dragStartHeight: Double?

    var body: some View {
        GeometryReader { geometry in
            let maxHeight = max(Self.minPanelHeight, geometry.size.height - Self.minTableHeight)
            VStack(spacing: 0) {
                TorrentTableView()
                    .frame(maxHeight: .infinity)
                if model.isDetailsShown {
                    Divider()
                        .overlay { handle(maxHeight: maxHeight) }
                        .zIndex(1)
                    TorrentDetails()
                        .frame(height: clamped(draggedHeight ?? storedHeight, maxHeight: maxHeight))
                }
            }
        }
    }

    private func handle(maxHeight: Double) -> some View {
        Color.clear
            .frame(height: Self.handleHeight)
            .contentShape(Rectangle())
            .onHover { isInside in
                if isInside {
                    NSCursor.resizeUpDown.push()
                } else {
                    NSCursor.pop()
                }
            }
            .gesture(
                DragGesture(minimumDistance: 1, coordinateSpace: .global)
                    .onChanged { drag in
                        let start = dragStartHeight ?? clamped(storedHeight, maxHeight: maxHeight)
                        dragStartHeight = start
                        draggedHeight = clamped(start - drag.translation.height, maxHeight: maxHeight)
                    }
                    .onEnded { _ in
                        if let draggedHeight { storedHeight = draggedHeight }
                        draggedHeight = nil
                        dragStartHeight = nil
                    }
            )
    }

    private func clamped(_ height: Double, maxHeight: Double) -> Double {
        min(max(height, Self.minPanelHeight), maxHeight)
    }
}
