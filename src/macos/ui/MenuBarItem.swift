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

/// An item in the menu bar with the transfer speeds, and a menu for what
/// one does without opening the window.
@MainActor
final class MenuBarItem: NSObject, NSMenuDelegate {
    private let item = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)
    private let model: SessionModel

    init(model: SessionModel) {
        self.model = model
        super.init()
        item.button?.font = .monospacedDigitSystemFont(ofSize: NSFont.smallSystemFontSize, weight: .regular)
        let menu = NSMenu()
        menu.delegate = self
        item.menu = menu
        update()
    }

    func remove() {
        NSStatusBar.system.removeStatusItem(item)
    }

    func update() {
        item.button?.title = "↓ \(speed(model.downloadSpeed))  ↑ \(speed(model.uploadSpeed))"
    }

    private func speed(_ value: Int64) -> String {
        String(localized: "\(Format.bytes(value))/s")
    }

    func menuNeedsUpdate(_ menu: NSMenu) {
        menu.removeAllItems()
        let alternative = menuItem(String(localized: "Alternative Speed Limits"), #selector(toggleAlternativeSpeedLimits))
        alternative.state = model.isAlternativeSpeedEnabled ? .on : .off
        menu.addItem(alternative)
        menu.addItem(.separator())
        menu.addItem(menuItem(String(localized: "Start All Torrents"), #selector(startAll)))
        menu.addItem(menuItem(String(localized: "Stop All Torrents"), #selector(stopAll)))
        menu.addItem(.separator())
        menu.addItem(menuItem(String(localized: "Show qBittorrent"), #selector(showWindow)))
        menu.addItem(.separator())
        let quit = NSMenuItem(title: String(localized: "Quit qBittorrent"), action: #selector(NSApplication.terminate(_:)), keyEquivalent: "")
        menu.addItem(quit)
    }

    private func menuItem(_ title: String, _ action: Selector) -> NSMenuItem {
        let item = NSMenuItem(title: title, action: action, keyEquivalent: "")
        item.target = self
        return item
    }

    @objc private func toggleAlternativeSpeedLimits() {
        model.toggleAlternativeSpeedLimits()
    }

    @objc private func startAll() {
        model.start(Set(model.torrents.map(\.id)))
    }

    @objc private func stopAll() {
        model.stop(Set(model.torrents.map(\.id)))
    }

    @objc private func showWindow() {
        AppDelegate.shared.showMainWindow()
        NSApp.activate()
    }
}
