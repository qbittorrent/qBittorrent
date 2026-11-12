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

/// The sidebar's tag filters, as the Qt UI's tag filter list.
struct TagsSidebarSection: View {
    @Environment(SessionModel.self) private var model
    @AppStorage("Sidebar/TagsExpanded") private var isExpanded = true

    var body: some View {
        Section("Tags", isExpanded: $isExpanded) {
            row(String(localized: "All"), systemImage: "tag", count: model.torrents.count)
                .tag(SidebarSelection.allTags)
            if model.sidebar.isShown(.untagged) {
                row(String(localized: "Untagged"), systemImage: "tag.slash", count: model.torrents.count(where: { $0.tags.isEmpty }))
                    .tag(SidebarSelection.untagged)
            }
            ForEach(model.tags, id: \.self) { tag in
                row(tag, systemImage: "tag.fill", count: model.torrents.count(where: { $0.tags.contains(tag) }))
                    .tag(SidebarSelection.tag(tag))
                    .contextMenu {
                        Button("New Tag…") { model.torrentSheet = .newTag }
                        Divider()
                        Button("Remove Tag", role: .destructive) { model.deleteTag(tag) }
                    }
                    .dropDestination(for: String.self) { ids, _ in
                        model.addTag(tag, to: Set(ids))
                        return true
                    }
            }
        }
    }

    private func row(_ title: String, systemImage: String, count: Int) -> some View {
        Label(title, systemImage: systemImage)
            .badge(count)
    }
}
