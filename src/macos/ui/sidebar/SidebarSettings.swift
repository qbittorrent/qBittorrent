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

import Foundation
import QBTBridge

/// The sections of the sidebar, in the order shown.
enum SidebarSection: String, CaseIterable, Identifiable {
    case status, categories, tags, trackerStatus, trackers

    var id: Self { self }

    var title: String {
        switch self {
        case .status: String(localized: "Status")
        case .categories: String(localized: "Categories")
        case .tags: String(localized: "Tags")
        case .trackerStatus: String(localized: "Tracker Status")
        case .trackers: String(localized: "Trackers")
        }
    }
}

/// The fixed rows of the sidebar that the user can hide, besides the status
/// filters.
enum SidebarItem: String, CaseIterable, Identifiable {
    case uncategorized, untagged, trackerError, unreachable, trackerless

    var id: Self { self }

    var title: String {
        switch self {
        case .uncategorized: String(localized: "Uncategorized")
        case .untagged: String(localized: "Untagged")
        case .trackerError: String(localized: "Tracker Error")
        case .unreachable: String(localized: "Unreachable")
        case .trackerless: String(localized: "Trackerless")
        }
    }
}

/// What the sidebar shows. The rows and sections are stored in user
/// defaults with the sidebar's other settings; the two options the Qt UI
/// also has are stored in its preferences.
@MainActor
@Observable
final class SidebarSettings {
    private static let statusFiltersKey = "Sidebar/StatusFilters"
    private static let hiddenSectionsKey = "Sidebar/HiddenSections"
    private static let hiddenItemsKey = "Sidebar/HiddenItems"

    /// The status filters shown besides "All", which is always shown.
    var shownStatusFilters: Set<StatusFilter> {
        didSet { Self.store(shownStatusFilters, Self.statusFiltersKey) }
    }
    var hiddenSections: Set<SidebarSection> {
        didSet { Self.store(hiddenSections, Self.hiddenSectionsKey) }
    }
    var hiddenItems: Set<SidebarItem> {
        didSet { Self.store(hiddenItems, Self.hiddenItemsKey) }
    }
    var hidesEmptyStatusFilters = QBTBehaviorSettings.hideZeroStatusFilters {
        didSet { QBTBehaviorSettings.hideZeroStatusFilters = hidesEmptyStatusFilters }
    }
    var showsTrackerStatusSection = QBTBehaviorSettings.useSeparateTrackerStatusFilter {
        didSet { QBTBehaviorSettings.useSeparateTrackerStatusFilter = showsTrackerStatusSection }
    }

    init() {
        let defaults = UserDefaults.standard
        shownStatusFilters = defaults.string(forKey: Self.statusFiltersKey).map(Self.load) ?? StatusFilter.defaultFilters
        hiddenSections = Self.load(defaults.string(forKey: Self.hiddenSectionsKey) ?? "")
        hiddenItems = Self.load(defaults.string(forKey: Self.hiddenItemsKey) ?? "")
    }

    func isShown(_ section: SidebarSection) -> Bool {
        if section == .trackerStatus, !showsTrackerStatusSection { return false }
        return !hiddenSections.contains(section)
    }

    func isShown(_ item: SidebarItem) -> Bool {
        !hiddenItems.contains(item) && isShown(section(of: item))
    }

    /// Whether the user chose to show the filter; "All" always is.
    func isChosen(_ filter: StatusFilter) -> Bool {
        filter == .all || shownStatusFilters.contains(filter)
    }

    /// The tracker status rows move to a section of their own when the user
    /// chose so, as in the Qt UI.
    func section(of item: SidebarItem) -> SidebarSection {
        switch item {
        case .uncategorized: .categories
        case .untagged: .tags
        case .trackerError, .unreachable: showsTrackerStatusSection ? .trackerStatus : .trackers
        case .trackerless: .trackers
        }
    }

    func restoreDefaults() {
        shownStatusFilters = StatusFilter.defaultFilters
        hiddenSections = []
        hiddenItems = []
        hidesEmptyStatusFilters = false
        showsTrackerStatusSection = false
    }

    private static func load<Value: RawRepresentable<String>>(_ list: String) -> Set<Value> {
        Set(list.split(separator: ",").compactMap { Value(rawValue: String($0)) })
    }

    private static func store<Value: RawRepresentable<String>>(_ values: Set<Value>, _ key: String) {
        UserDefaults.standard.set(values.map(\.rawValue).sorted().joined(separator: ","), forKey: key)
    }
}

extension SessionModel {
    /// Whether the sidebar shows the row, so that a hidden row does not stay
    /// selected.
    func isShownInSidebar(_ selection: SidebarSelection) -> Bool {
        switch selection {
        case .status(let filter):
            sidebar.isShown(.status) && sidebar.isChosen(filter)
                && (filter == .all || !sidebar.hidesEmptyStatusFilters || count(filter) > 0)
        case .allCategories, .category:
            sidebar.isShown(.categories)
        case .uncategorized:
            sidebar.isShown(.uncategorized)
        case .allTags, .tag:
            sidebar.isShown(.tags)
        case .untagged:
            sidebar.isShown(.untagged)
        case .tracker(.trackerError):
            sidebar.isShown(.trackerError)
        case .tracker(.unreachable):
            sidebar.isShown(.unreachable)
        case .tracker(.trackerless):
            sidebar.isShown(.trackerless)
        case .tracker(.host):
            sidebar.isShown(.trackers)
        }
    }
}
