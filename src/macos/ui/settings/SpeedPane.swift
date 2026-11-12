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

import QBTBridge
import SwiftUI

/// Global and alternative speed limits, and when the alternative ones apply:
/// the Qt UI's Speed options, apart from the Dock and menu bar ones, which
/// are in the General pane.
struct SpeedPane: View {
    @State private var revision = 0

    var body: some View {
        let _ = revision
        Form {
            Section {
                LimitField(title: "Download", value: kibibytes(setting($revision, get: { QBTSettings.downloadLimit }, set: { QBTSettings.downloadLimit = $0 })), unit: "KiB/s")
                LimitField(title: "Upload", value: kibibytes(setting($revision, get: { QBTSettings.uploadLimit }, set: { QBTSettings.uploadLimit = $0 })), unit: "KiB/s")
            } header: {
                Text("Global limits")
            } footer: {
                Text("0 means unlimited.")
            }
            Section {
                LimitField(title: "Download", value: kibibytes(setting($revision, get: { QBTSettings.alternativeDownloadLimit }, set: { QBTSettings.alternativeDownloadLimit = $0 })), unit: "KiB/s")
                LimitField(title: "Upload", value: kibibytes(setting($revision, get: { QBTSettings.alternativeUploadLimit }, set: { QBTSettings.alternativeUploadLimit = $0 })), unit: "KiB/s")
            } header: {
                Text("Alternative limits")
            } footer: {
                Text("Switch to these with the tortoise button in the status bar.")
            }
            scheduleSection
            Section {
                Toggle("Apply limits to µTP", isOn: setting($revision, get: { QBTSettings.utpRateLimited }, set: { QBTSettings.utpRateLimited = $0 }))
                Toggle("Apply limits to protocol overhead", isOn: setting($revision, get: { QBTSettings.includeOverheadInLimits }, set: { QBTSettings.includeOverheadInLimits = $0 }))
                Toggle("Apply limits to peers on the local network", isOn: setting($revision, get: { !QBTSettings.ignoreLimitsOnLAN }, set: { QBTSettings.ignoreLimitsOnLAN = !$0 }))
            }
        }
    }

    private var scheduleSection: some View {
        Section {
            Toggle("Use the alternative limits on a schedule", isOn: setting($revision, get: { QBTSpeedSettings.schedulerEnabled }, set: { QBTSpeedSettings.schedulerEnabled = $0 }))
            if QBTSpeedSettings.schedulerEnabled {
                DatePicker("From", selection: time(setting($revision, get: { QBTSpeedSettings.schedulerStartMinute }, set: { QBTSpeedSettings.schedulerStartMinute = $0 })), displayedComponents: .hourAndMinute)
                DatePicker("To", selection: time(setting($revision, get: { QBTSpeedSettings.schedulerEndMinute }, set: { QBTSpeedSettings.schedulerEndMinute = $0 })), displayedComponents: .hourAndMinute)
                Picker("Days", selection: setting($revision, get: { QBTSpeedSettings.schedulerDays }, set: { QBTSpeedSettings.schedulerDays = $0 })) {
                    Text("Every day").tag(Int32(0))
                    Text("Weekdays").tag(Int32(1))
                    Text("Weekends").tag(Int32(2))
                    Divider()
                    // Scheduler::Days lists Monday (3) to Sunday (9); the
                    // calendar's symbols start on Sunday.
                    ForEach(Int32(3)...Int32(9), id: \.self) { day in
                        Text(Calendar.current.standaloneWeekdaySymbols[Int(day - 2) % 7]).tag(day)
                    }
                }
            }
        } header: {
            Text("Schedule")
        } footer: {
            if QBTSpeedSettings.schedulerEnabled, QBTSpeedSettings.schedulerStartMinute == QBTSpeedSettings.schedulerEndMinute {
                Text("The start and end times can't be the same.")
                    .foregroundStyle(.red)
            }
        }
    }

    /// A time of day, for minutes since midnight.
    private func time(_ minutes: Binding<Int32>) -> Binding<Date> {
        Binding {
            let calendar = Calendar.current
            let value = Int(minutes.wrappedValue)
            return calendar.date(bySettingHour: value / 60, minute: value % 60, second: 0, of: .now) ?? .now
        } set: {
            let components = Calendar.current.dateComponents([.hour, .minute], from: $0)
            minutes.wrappedValue = Int32(((components.hour ?? 0) * 60) + (components.minute ?? 0))
        }
    }
}
