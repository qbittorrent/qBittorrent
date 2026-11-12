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
import UniformTypeIdentifiers

/// Called by the main-loop dispatcher in place of Qt's own wait for events.
/// AppKit owns the app's life cycle rather than SwiftUI's `App`, because
/// `App.main()` never returns, and qBittorrent must get to shut down.
@_cdecl("qbt_macos_run_ui")
public func runUI() {
    MainActor.assumeIsolated {
        let app = NSApplication.shared
        app.setActivationPolicy(.regular)
        app.delegate = AppDelegate.shared
        app.mainMenu = MainMenu.make()
        app.run()
    }
}

@MainActor
final class AppDelegate: NSObject, NSApplicationDelegate {
    static let shared = AppDelegate()

    private(set) lazy var model = SessionModel()
    /// Made when their pages are first shown.
    private(set) lazy var rssModel = RSSModel()
    private(set) lazy var searchModel = SearchModel { sources in AppDelegate.shared.model.add(sources) }
    private var mainWindow: NSWindow?
    private var settingsWindow: NSWindow?

    func applicationDidFinishLaunching(_ notification: Notification) {
        showMainWindow()
        DesktopIntegration.shared.start(model: model)
        NSApp.activate()
        showLegalNoticeIfNeeded()
    }

    /// Shown once, on first launch, as the Qt UI does before its window.
    private func showLegalNoticeIfNeeded() {
        guard !QBTLegalNotice.isAccepted, let window = mainWindow else { return }
        let alert = NSAlert()
        alert.messageText = QBTLegalNotice.title
        alert.informativeText = QBTLegalNotice.message
        alert.beginSheetModal(for: window) { _ in
            QBTLegalNotice.accept()
        }
    }

    /// Hands quitting to Qt, which shuts down and then stops this run loop.
    func applicationShouldTerminate(_ sender: NSApplication) -> NSApplication.TerminateReply {
        guard DesktopIntegration.shared.shouldQuit() else { return .terminateCancel }
        mainWindow?.orderOut(nil)
        settingsWindow?.orderOut(nil)
        qbt_macos_request_exit()
        return .terminateCancel
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        false
    }

    func applicationShouldHandleReopen(_ sender: NSApplication, hasVisibleWindows: Bool) -> Bool {
        if !hasVisibleWindows { showMainWindow() }
        return true
    }

    /// `.torrent` files opened in Finder, and magnet links opened anywhere.
    func application(_ application: NSApplication, open urls: [URL]) {
        model.add(urls.map { $0.isFileURL ? $0.path : $0.absoluteString })
    }

    @objc func showMainWindow() {
        if mainWindow == nil {
            let controller = NSHostingController(rootView: MainView().environment(model))
            // Not .title: bridged from the pages' navigation titles, the
            // window was titled "Untitled".
            controller.sceneBridgingOptions = [.toolbars]
            let window = NSWindow(contentViewController: controller)
            window.title = "qBittorrent"
            window.styleMask = [.titled, .closable, .miniaturizable, .resizable, .fullSizeContentView]
            window.toolbarStyle = .unified
            window.isReleasedWhenClosed = false
            window.setContentSize(NSSize(width: 1360, height: 760))
            window.setFrameAutosaveName("MainWindow")
            mainWindow = window
        }
        mainWindow?.makeKeyAndOrderFront(nil)
    }

    @objc func showSettings() {
        if settingsWindow == nil {
            let controller = NSHostingController(rootView: SettingsView())
            controller.sceneBridgingOptions = [.toolbars, .title]
            let window = NSWindow(contentViewController: controller)
            window.title = String(localized: "Settings")
            window.styleMask = [.titled, .closable, .miniaturizable, .resizable, .fullSizeContentView]
            window.toolbarStyle = .unified
            window.isReleasedWhenClosed = false
            window.setContentSize(NSSize(width: 780, height: 580))
            window.center()
            window.setFrameAutosaveName("SettingsWindow")
            settingsWindow = window
        }
        settingsWindow?.makeKeyAndOrderFront(nil)
    }

    @objc func showTransfers() { show(.transfers) }
    @objc func showRSSReader() { show(.rss) }
    @objc func showSearch() { show(.search) }

    @objc func showRSSRules() {
        show(.rss)
        model.isEditingRSSRules = true
    }

    @objc func customizeSidebar() {
        show(.transfers)
        model.isCustomizingSidebar = true
    }

    private func show(_ page: MainPage) {
        showMainWindow()
        model.page = page
    }

    @objc func addTorrentFiles() {
        showMainWindow()
        AppActions.addTorrentFiles(model)
    }

    @objc func addTorrentLinks() {
        showMainWindow()
        model.isAddingLinks = true
    }

    @objc func newCategory() {
        showMainWindow()
        model.editedCategory = .create
    }

    // MARK: Torrent menu, acting on the table's selection

    @objc func startSelected() { model.start(model.selection) }
    @objc func stopSelected() { model.stop(model.selection) }
    @objc func recheckSelected() { model.recheck(model.selection) }
    @objc func reannounceSelected() { model.reannounce(model.selection) }
    @objc func revealSelected() { model.revealInFinder(model.selection) }
    @objc func copyMagnetLinks() { model.copyMagnetLinks(model.selection) }
    @objc func removeSelected() { model.requestRemoval(model.torrents(model.selection)) }
    @objc func toggleDetails() { model.isDetailsShown.toggle() }
    @objc func toggleAlternativeSpeedLimits() { model.toggleAlternativeSpeedLimits() }
}

extension AppDelegate: NSMenuItemValidation {
    private static let selectionActions: Set<Selector> = [
        #selector(startSelected), #selector(stopSelected), #selector(recheckSelected),
        #selector(reannounceSelected), #selector(revealSelected), #selector(copyMagnetLinks),
        #selector(removeSelected)
    ]

    private static let pageActions: [Selector?: MainPage] = [
        #selector(showTransfers): .transfers, #selector(showRSSReader): .rss, #selector(showSearch): .search
    ]

    func validateMenuItem(_ menuItem: NSMenuItem) -> Bool {
        if let action = menuItem.action, Self.selectionActions.contains(action) {
            return !model.selection.isEmpty
        }
        if menuItem.action == #selector(toggleDetails) {
            menuItem.title = model.isDetailsShown ? String(localized: "Hide Details") : String(localized: "Show Details")
            return model.page == .transfers
        }
        if let page = Self.pageActions[menuItem.action] {
            menuItem.state = model.page == page ? .on : .off
        }
        if menuItem.action == #selector(toggleAlternativeSpeedLimits) {
            menuItem.state = model.isAlternativeSpeedEnabled ? .on : .off
        }
        return true
    }
}

@MainActor
enum AppActions {
    static func addTorrentFiles(_ model: SessionModel) {
        let panel = NSOpenPanel()
        panel.allowsMultipleSelection = true
        panel.allowedContentTypes = [UTType(filenameExtension: "torrent") ?? .data]
        panel.message = String(localized: "Choose torrent files to add")
        if panel.runModal() == .OK {
            model.add(panel.urls.map(\.path), category: model.selectedCategory)
        }
    }
}

@MainActor
private enum MainMenu {
    static func make() -> NSMenu {
        let menu = NSMenu()
        menu.addItem(submenu(appMenu()))
        menu.addItem(submenu(fileMenu()))
        menu.addItem(submenu(editMenu()))
        menu.addItem(submenu(viewMenu()))
        menu.addItem(submenu(torrentMenu()))
        let window = windowMenu()
        menu.addItem(submenu(window))
        NSApp.windowsMenu = window
        return menu
    }

    private static func submenu(_ menu: NSMenu) -> NSMenuItem {
        let item = NSMenuItem(title: menu.title, action: nil, keyEquivalent: "")
        item.submenu = menu
        return item
    }

    private static func item(_ title: String, _ action: Selector?, _ key: String = "", _ modifiers: NSEvent.ModifierFlags = .command, target: AnyObject? = nil) -> NSMenuItem {
        let item = NSMenuItem(title: title, action: action, keyEquivalent: key)
        item.keyEquivalentModifierMask = modifiers
        item.target = target
        return item
    }

    private static func appMenu() -> NSMenu {
        let menu = NSMenu(title: "qBittorrent")
        menu.addItem(item(String(localized: "About qBittorrent"), #selector(NSApplication.orderFrontStandardAboutPanel(_:))))
        menu.addItem(.separator())
        menu.addItem(item(String(localized: "Settings…"), #selector(AppDelegate.showSettings), ",", target: AppDelegate.shared))
        menu.addItem(.separator())
        let services = NSMenu(title: String(localized: "Services"))
        menu.addItem(submenu(services))
        NSApp.servicesMenu = services
        menu.addItem(.separator())
        menu.addItem(item(String(localized: "Hide qBittorrent"), #selector(NSApplication.hide(_:)), "h"))
        menu.addItem(item(String(localized: "Hide Others"), #selector(NSApplication.hideOtherApplications(_:)), "h", [.command, .option]))
        menu.addItem(item(String(localized: "Show All"), #selector(NSApplication.unhideAllApplications(_:))))
        menu.addItem(.separator())
        menu.addItem(item(String(localized: "Quit qBittorrent"), #selector(NSApplication.terminate(_:)), "q"))
        return menu
    }

    private static func fileMenu() -> NSMenu {
        let delegate = AppDelegate.shared
        let menu = NSMenu(title: String(localized: "File"))
        menu.addItem(item(String(localized: "Add Torrent File…"), #selector(AppDelegate.addTorrentFiles), "o", target: delegate))
        menu.addItem(item(String(localized: "Add Torrent Link…"), #selector(AppDelegate.addTorrentLinks), "o", [.command, .shift], target: delegate))
        menu.addItem(.separator())
        menu.addItem(item(String(localized: "New Category…"), #selector(AppDelegate.newCategory), "n", [.command, .shift], target: delegate))
        menu.addItem(.separator())
        menu.addItem(item(String(localized: "Close Window"), #selector(NSWindow.performClose(_:)), "w"))
        return menu
    }

    private static func editMenu() -> NSMenu {
        let menu = NSMenu(title: String(localized: "Edit"))
        menu.addItem(item(String(localized: "Undo"), Selector(("undo:")), "z"))
        menu.addItem(item(String(localized: "Redo"), Selector(("redo:")), "z", [.command, .shift]))
        menu.addItem(.separator())
        menu.addItem(item(String(localized: "Cut"), #selector(NSText.cut(_:)), "x"))
        menu.addItem(item(String(localized: "Copy"), #selector(NSText.copy(_:)), "c"))
        menu.addItem(item(String(localized: "Paste"), #selector(NSText.paste(_:)), "v"))
        menu.addItem(item(String(localized: "Select All"), #selector(NSText.selectAll(_:)), "a"))
        menu.addItem(.separator())
        menu.addItem(item(String(localized: "Find"), #selector(NSResponder.performTextFinderAction(_:)), "f"))
        return menu
    }

    private static func viewMenu() -> NSMenu {
        let delegate = AppDelegate.shared
        let menu = NSMenu(title: String(localized: "View"))
        menu.addItem(item(String(localized: "Show Sidebar"), #selector(NSSplitViewController.toggleSidebar(_:)), "s", [.command, .control]))
        menu.addItem(item(String(localized: "Customize Sidebar…"), #selector(AppDelegate.customizeSidebar), target: delegate))
        menu.addItem(item(String(localized: "Customize Toolbar…"), #selector(NSWindow.runToolbarCustomizationPalette(_:))))
        menu.addItem(item(String(localized: "Show Details"), #selector(AppDelegate.toggleDetails), "i", [.command, .option], target: delegate))
        menu.addItem(.separator())
        menu.addItem(item(String(localized: "Transfers"), #selector(AppDelegate.showTransfers), "1", target: delegate))
        menu.addItem(item(String(localized: "RSS"), #selector(AppDelegate.showRSSReader), "2", target: delegate))
        menu.addItem(item(String(localized: "Search"), #selector(AppDelegate.showSearch), "3", target: delegate))
        menu.addItem(item(String(localized: "RSS Download Rules…"), #selector(AppDelegate.showRSSRules), "r", [.command, .control, .option], target: delegate))
        menu.addItem(.separator())
        menu.addItem(item(String(localized: "Alternative Speed Limits"), #selector(AppDelegate.toggleAlternativeSpeedLimits), "l", [.command, .shift], target: delegate))
        return menu
    }

    private static func torrentMenu() -> NSMenu {
        let delegate = AppDelegate.shared
        let menu = NSMenu(title: String(localized: "Torrent"))
        menu.addItem(item(String(localized: "Start"), #selector(AppDelegate.startSelected), "r", target: delegate))
        menu.addItem(item(String(localized: "Stop"), #selector(AppDelegate.stopSelected), ".", target: delegate))
        menu.addItem(.separator())
        menu.addItem(item(String(localized: "Force Recheck"), #selector(AppDelegate.recheckSelected), target: delegate))
        menu.addItem(item(String(localized: "Force Reannounce"), #selector(AppDelegate.reannounceSelected), "r", [.command, .shift], target: delegate))
        menu.addItem(.separator())
        menu.addItem(item(String(localized: "Show in Finder"), #selector(AppDelegate.revealSelected), "r", [.command, .option], target: delegate))
        menu.addItem(item(String(localized: "Copy Magnet Link"), #selector(AppDelegate.copyMagnetLinks), "c", [.command, .option], target: delegate))
        menu.addItem(.separator())
        menu.addItem(item(String(localized: "Remove…"), #selector(AppDelegate.removeSelected), "\u{8}", target: delegate))
        return menu
    }

    private static func windowMenu() -> NSMenu {
        let menu = NSMenu(title: String(localized: "Window"))
        menu.addItem(item(String(localized: "Minimize"), #selector(NSWindow.performMiniaturize(_:)), "m"))
        menu.addItem(item(String(localized: "Zoom"), #selector(NSWindow.performZoom(_:))))
        menu.addItem(.separator())
        menu.addItem(item(String(localized: "qBittorrent"), #selector(AppDelegate.showMainWindow), "0", target: AppDelegate.shared))
        menu.addItem(.separator())
        menu.addItem(item(String(localized: "Bring All to Front"), #selector(NSApplication.arrangeInFront(_:))))
        return menu
    }
}
