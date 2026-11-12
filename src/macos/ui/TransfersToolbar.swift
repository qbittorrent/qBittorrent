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

/// The transfers toolbar, which the user can customize. By default:
/// adding, the selection's actions, the details and the filter, each a
/// group of its own with the filter last, as the search of Mail and
/// Finder. The Qt UI's other actions can be added from the customization
/// sheet. From macOS 26, which draws neighboring items on one shared
/// background, fixed spacers keep the groups apart.
struct TransfersToolbar: ViewModifier {
    @Environment(SessionModel.self) private var model

    func body(content: Content) -> some View {
        if #available(macOS 26.0, *) {
            content.toolbar(id: "Transfers") {
                addItem
                ToolbarSpacer(.fixed)
                selectionItems
                extraItems
                ToolbarSpacer(.fixed)
                detailsItem
                ToolbarSpacer(.fixed)
                filterItem
            }
        } else {
            content.toolbar(id: "Transfers") {
                addItem
                selectionItems
                extraItems
                detailsItem
                filterItem
            }
        }
    }

    private var noSelection: Bool { model.selection.isEmpty }

    private var addItem: some CustomizableToolbarContent {
        ToolbarItem(id: "add") {
            Menu {
                Button("Add Torrent File…") { AppActions.addTorrentFiles(model) }
                Button("Add Torrent Link…") { model.isAddingLinks = true }
            } label: {
                Label("Add", systemImage: "plus")
            } primaryAction: {
                AppActions.addTorrentFiles(model)
            }
            .help("Add torrents")
        }
    }

    @ToolbarContentBuilder
    private var selectionItems: some CustomizableToolbarContent {
        ToolbarItem(id: "start") {
            button("Start", "play.fill", help: "Start the selected torrents") { model.start(model.selection) }
        }
        ToolbarItem(id: "stop") {
            button("Stop", "stop.fill", help: "Stop the selected torrents") { model.stop(model.selection) }
        }
        ToolbarItem(id: "remove") {
            button("Remove", "trash", help: "Remove the selected torrents") {
                model.requestRemoval(model.torrents(model.selection))
            }
        }
    }

    /// The Qt UI's other actions, hidden until the user adds them.
    @ToolbarContentBuilder
    private var extraItems: some CustomizableToolbarContent {
        ToolbarItem(id: "recheck") {
            button("Force Recheck", "arrow.triangle.2.circlepath", help: "Check the selected torrents' downloaded data") {
                model.recheck(model.selection)
            }
        }
        .defaultCustomization(.hidden)
        ToolbarItem(id: "reannounce") {
            button("Force Reannounce", "megaphone", help: "Announce the selected torrents to their trackers now") {
                model.reannounce(model.selection)
            }
        }
        .defaultCustomization(.hidden)
        ToolbarItem(id: "moveToTop") {
            queueButton("Move to Top", "arrow.up.to.line", .top)
        }
        .defaultCustomization(.hidden)
        ToolbarItem(id: "moveUp") {
            queueButton("Move Up", "arrow.up", .up)
        }
        .defaultCustomization(.hidden)
        ToolbarItem(id: "moveDown") {
            queueButton("Move Down", "arrow.down", .down)
        }
        .defaultCustomization(.hidden)
        ToolbarItem(id: "moveToBottom") {
            queueButton("Move to Bottom", "arrow.down.to.line", .bottom)
        }
        .defaultCustomization(.hidden)
        ToolbarItem(id: "reveal") {
            button("Show in Finder", "folder", help: "Show the selected torrents' files in Finder") {
                model.revealInFinder(model.selection)
            }
        }
        .defaultCustomization(.hidden)
        ToolbarItem(id: "copyMagnet") {
            button("Copy Magnet Link", "link", help: "Copy the selected torrents' magnet links") {
                model.copyMagnetLinks(model.selection)
            }
        }
        .defaultCustomization(.hidden)
        ToolbarItem(id: "alternativeSpeed") {
            Button {
                model.toggleAlternativeSpeedLimits()
            } label: {
                Label("Alternative Speed Limits", systemImage: model.isAlternativeSpeedEnabled ? "tortoise.fill" : "tortoise")
            }
            .help("Switch the alternative speed limits on or off")
        }
        .defaultCustomization(.hidden)
        ToolbarItem(id: "settings") {
            Button {
                AppDelegate.shared.showSettings()
            } label: {
                Label("Settings", systemImage: "gearshape")
            }
            .help("Open the settings")
        }
        .defaultCustomization(.hidden)
    }

    private var detailsItem: some CustomizableToolbarContent {
        ToolbarItem(id: "details") {
            Button {
                model.isDetailsShown.toggle()
            } label: {
                Label("Details", systemImage: "rectangle.bottomthird.inset.filled")
            }
            .help("Show or hide the torrent details")
        }
    }

    /// The filter can be moved but not removed: it is the only way to
    /// filter the torrents by name.
    private var filterItem: some CustomizableToolbarContent {
        @Bindable var model = model
        return ToolbarItem(id: "filter") {
            FilterToolbarField(
                text: $model.searchText,
                prompt: String(localized: "Filter torrent names"),
                help: "Filter the torrents by name"
            )
        }
        .customizationBehavior(.reorderable)
    }

    /// A button acting on the selected torrents.
    private func button(_ title: LocalizedStringKey, _ systemImage: String, help: LocalizedStringKey, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Label(title, systemImage: systemImage)
        }
        .help(help)
        .disabled(noSelection)
    }

    private func queueButton(_ title: LocalizedStringKey, _ systemImage: String, _ move: SessionModel.QueueMove) -> some View {
        Button {
            model.moveInQueue(model.selection, move)
        } label: {
            Label(title, systemImage: systemImage)
        }
        .help("Move the selected torrents in the download queue")
        .disabled(noSelection || !model.isQueueingEnabled)
    }
}
