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

import SwiftUI

/// The app's visual rules, after the macOS Human Interface Guidelines: only
/// the system's text styles, and the same few spacings everywhere.
///
/// - Body: content, list rows, values; monospaced for hashes and addresses.
/// - Callout: bars, and the second line of a list row.
/// - Headline: a heading inside a pane or sheet.
/// - Empty states: `ContentUnavailableView`.
enum Metrics {
    /// Between a sheet's or dialog's edge and its content, as the HIG's
    /// window margins.
    static let margin = 20.0
    /// Inside dense panes, such as the details panel.
    static let paneInset = 12.0
    /// The bars at the edges of pages, lists and tables.
    static let barHeight = 28.0
    static let barInset = 8.0
}

extension View {
    /// A bar at the edge of a page, list or table: the same height, inset,
    /// text and control size everywhere.
    func barStyle() -> some View {
        font(.callout)
            .controlSize(.small)
            .padding(.horizontal, Metrics.barInset)
            // A fixed height: with only a minimum, a bar in a safe area
            // inset takes all the height it is offered.
            .frame(height: Metrics.barHeight)
    }
}
