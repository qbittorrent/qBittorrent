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

enum Format {
    static func bytes(_ value: Int64) -> String {
        ByteCountFormatter.string(fromByteCount: value, countStyle: .file)
    }

    /// Empty when idle, so that the many idle rows stay quiet.
    static func speed(_ value: Int64) -> String {
        value > 0 ? String(localized: "\(bytes(value))/s") : ""
    }

    /// `8640000` seconds is the core's "infinity".
    static func eta(_ seconds: Int64) -> String {
        guard seconds >= 0, seconds < 8_640_000 else { return "∞" }
        return Duration.seconds(seconds).formatted(.units(allowed: [.days, .hours, .minutes, .seconds], width: .narrow, maximumUnitCount: 2))
    }

    static func ratio(_ value: Double) -> String {
        value < 0 || value >= 9999 ? "∞" : value.formatted(.number.precision(.fractionLength(2)))
    }

    static func duration(_ seconds: Int64) -> String {
        guard seconds > 0 else { return "–" }
        return Duration.seconds(seconds).formatted(.units(allowed: [.days, .hours, .minutes], width: .abbreviated, maximumUnitCount: 2))
    }

    /// Bytes per second; 0 or less means unlimited.
    static func limit(_ bytesPerSecond: Int32) -> String {
        bytesPerSecond > 0 ? bytes(Int64(bytesPerSecond)) + "/s" : String(localized: "Unlimited")
    }

    static func percent(_ value: Double) -> String {
        value.formatted(.percent.precision(.fractionLength(1)))
    }
}
