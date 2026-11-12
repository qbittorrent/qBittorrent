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
import UserNotifications

/// Posts notifications through the system, and opens what they are about
/// when clicked.
@MainActor
final class Notifier: NSObject, UNUserNotificationCenterDelegate {
    private nonisolated static let torrentIDKey = "torrentID"

    var onOpenTorrent: ((String) -> Void)?

    private var center: UNUserNotificationCenter { .current() }

    /// The delegate must be set before launching ends, to hear about a click
    /// that launched the app.
    func start(requestingAuthorization: Bool) {
        center.delegate = self
        if requestingAuthorization { requestAuthorization() }
    }

    func requestAuthorization() {
        center.requestAuthorization(options: [.alert, .sound]) { _, _ in }
    }

    /// Whether the user turned notifications off for the app in System Settings.
    func isDenied() async -> Bool {
        await center.notificationSettings().authorizationStatus == .denied
    }

    /// An empty `torrentID` means that there is no torrent to show.
    func post(title: String, body: String, torrentID: String) {
        let content = UNMutableNotificationContent()
        content.title = title
        content.body = body
        content.sound = .default
        if !torrentID.isEmpty {
            content.userInfo = [Self.torrentIDKey: torrentID]
        }
        let request = UNNotificationRequest(identifier: UUID().uuidString, content: content, trigger: nil)
        center.add(request, withCompletionHandler: nil)
    }

    nonisolated func userNotificationCenter(_ center: UNUserNotificationCenter, willPresent notification: UNNotification) async -> UNNotificationPresentationOptions {
        [.banner, .list, .sound]
    }

    nonisolated func userNotificationCenter(_ center: UNUserNotificationCenter, didReceive response: UNNotificationResponse) async {
        let torrentID = response.notification.request.content.userInfo[Self.torrentIDKey] as? String
        await MainActor.run {
            if let torrentID {
                onOpenTorrent?(torrentID)
            } else {
                AppDelegate.shared.showMainWindow()
            }
        }
    }
}
