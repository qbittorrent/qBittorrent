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

/// Chooses the sections and rows the sidebar shows, as Finder's sidebar
/// settings do. Each section's All row is always shown.
struct SidebarCustomizationSheet: View {
    @Environment(SessionModel.self) private var model
    @Environment(\.dismiss) private var dismiss

    private var sidebar: SidebarSettings { model.sidebar }

    var body: some View {
        @Bindable var sidebar = sidebar
        VStack(spacing: 0) {
            Form {
                section(.status) {
                    ForEach(StatusFilter.allCases) { filter in
                        Toggle(filter.title, isOn: shownBinding(filter))
                            .disabled(filter == .all)
                    }
                    Toggle("Hide filters without torrents", isOn: $sidebar.hidesEmptyStatusFilters)
                }
                section(.categories) {
                    allRow
                    Toggle(SidebarItem.uncategorized.title, isOn: shownBinding(.uncategorized))
                }
                section(.tags) {
                    allRow
                    Toggle(SidebarItem.untagged.title, isOn: shownBinding(.untagged))
                }
                if sidebar.showsTrackerStatusSection {
                    section(.trackerStatus) {
                        trackerStatusToggles
                    }
                }
                section(.trackers) {
                    if !sidebar.showsTrackerStatusSection {
                        trackerStatusToggles
                    }
                    Toggle(SidebarItem.trackerless.title, isOn: shownBinding(.trackerless))
                    Toggle("Show tracker status in a section of its own", isOn: $sidebar.showsTrackerStatusSection)
                }
            }
            .formStyle(.grouped)
            .toggleStyle(.checkbox)

            Divider()
            HStack {
                Button("Restore Defaults") { sidebar.restoreDefaults() }
                Spacer()
                Button("Done") { dismiss() }
                    .keyboardShortcut(.defaultAction)
            }
            .padding(Metrics.margin)
        }
        .frame(width: 420, height: 560)
    }

    /// A section of the sheet: whether the sidebar shows it, then its rows,
    /// which can only be chosen while it is shown.
    private func section(_ section: SidebarSection, @ViewBuilder rows: () -> some View) -> some View {
        Section {
            VStack(alignment: .leading, spacing: 6) {
                rows()
            }
            .padding(.leading, 20)
            .disabled(!sidebar.isShown(section))
        } header: {
            Toggle(isOn: shownBinding(section)) {
                Text(section.title).font(.headline)
            }
        }
    }

    private var allRow: some View {
        Toggle("All", isOn: .constant(true))
            .disabled(true)
    }

    @ViewBuilder
    private var trackerStatusToggles: some View {
        Toggle(SidebarItem.trackerError.title, isOn: shownBinding(.trackerError))
        Toggle(SidebarItem.unreachable.title, isOn: shownBinding(.unreachable))
    }

    private func shownBinding(_ section: SidebarSection) -> Binding<Bool> {
        Binding {
            !sidebar.hiddenSections.contains(section)
        } set: { isShown in
            if isShown {
                sidebar.hiddenSections.remove(section)
            } else {
                sidebar.hiddenSections.insert(section)
            }
        }
    }

    private func shownBinding(_ item: SidebarItem) -> Binding<Bool> {
        Binding {
            !sidebar.hiddenItems.contains(item)
        } set: { isShown in
            if isShown {
                sidebar.hiddenItems.remove(item)
            } else {
                sidebar.hiddenItems.insert(item)
            }
        }
    }

    private func shownBinding(_ filter: StatusFilter) -> Binding<Bool> {
        Binding {
            sidebar.isChosen(filter)
        } set: { isShown in
            if isShown {
                sidebar.shownStatusFilters.insert(filter)
            } else {
                sidebar.shownStatusFilters.remove(filter)
            }
        }
    }
}
