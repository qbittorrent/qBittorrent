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

/// The Dock tile: the app icon with the download and upload speeds on
/// badges, as the Qt UI draws it.
final class DockTileView: NSView {
    private static let badgeSize = NSSize(width: 128, height: 30)
    private static let badgeSpacing: CGFloat = 2
    private static let downloadColor = NSColor(red: 0.216, green: 0.482, blue: 0.173, alpha: 0.9)
    private static let uploadColor = NSColor(red: 0.094, green: 0.243, blue: 0.835, alpha: 0.9)

    private var downloadSpeed: Int64 = 0
    private var uploadSpeed: Int64 = 0

    private let attributes: [NSAttributedString.Key: Any] = {
        let shadow = NSShadow()
        shadow.shadowOffset = NSSize(width: 2, height: -2)
        shadow.shadowBlurRadius = 4
        return [
            .foregroundColor: NSColor.white,
            .shadow: shadow,
            .font: NSFont.boldSystemFont(ofSize: 26)
        ]
    }()

    /// Returns whether anything changed, so that the tile needs drawing.
    func setSpeeds(download: Int64, upload: Int64) -> Bool {
        guard download != downloadSpeed || upload != uploadSpeed else { return false }
        downloadSpeed = download
        uploadSpeed = upload
        return true
    }

    override func draw(_ dirtyRect: NSRect) {
        NSApp.applicationIconImage.draw(in: bounds)
        var bottom: CGFloat = 0
        if downloadSpeed > 0 {
            drawBadge(color: Self.downloadColor, arrow: "↓", speed: downloadSpeed, at: bottom)
            bottom += Self.badgeSize.height + Self.badgeSpacing
        }
        if uploadSpeed > 0 {
            drawBadge(color: Self.uploadColor, arrow: "↑", speed: uploadSpeed, at: bottom)
        }
    }

    private func drawBadge(color: NSColor, arrow: String, speed: Int64, at y: CGFloat) {
        let rect = NSRect(origin: NSPoint(x: 0, y: y), size: Self.badgeSize)
        let radius = rect.height / 2
        color.setFill()
        NSBezierPath(roundedRect: rect, xRadius: radius, yRadius: radius).fill()

        let arrowInset = rect.height * 0.2
        let text = Format.bytes(speed) as NSString
        let textSize = text.size(withAttributes: attributes)
        let textOrigin = NSPoint(x: rect.midX - textSize.width / 2 + arrowInset, y: rect.midY - textSize.height / 2 + 1)
        text.draw(at: textOrigin, withAttributes: attributes)
        (arrow as NSString).draw(at: NSPoint(x: arrowInset, y: textOrigin.y), withAttributes: attributes)
    }
}
