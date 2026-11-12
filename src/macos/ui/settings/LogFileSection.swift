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

/// The Qt UI's log file options, from its Behavior page.
struct LogFileSection: View {
    @State private var revision = 0

    var body: some View {
        let _ = revision
        Section {
            Toggle("Save the log to a file", isOn: setting($revision, get: { QBTBehaviorSettings.fileLoggerEnabled }, set: { QBTBehaviorSettings.fileLoggerEnabled = $0 }))
            if QBTBehaviorSettings.fileLoggerEnabled {
                FolderField(title: "Folder", path: setting($revision, get: { QBTBehaviorSettings.fileLoggerPath }, set: { QBTBehaviorSettings.fileLoggerPath = $0 }))
                Toggle("Start a new file when the log file gets large", isOn: setting($revision, get: { QBTBehaviorSettings.fileLoggerBackup }, set: { QBTBehaviorSettings.fileLoggerBackup = $0 }))
                AdvancedNumberField(title: "Maximum file size", value: setting($revision, get: { QBTBehaviorSettings.fileLoggerMaxSize }, set: { QBTBehaviorSettings.fileLoggerMaxSize = $0 }), range: 1...1_024_000, unit: "KiB")
                    .disabled(!QBTBehaviorSettings.fileLoggerBackup)
                Toggle("Delete old log files", isOn: setting($revision, get: { QBTBehaviorSettings.fileLoggerDeleteOld }, set: { QBTBehaviorSettings.fileLoggerDeleteOld = $0 }))
                LabeledContent("Older than") {
                    HStack(spacing: 6) {
                        TextField("Older than", value: setting($revision, get: { Int(QBTBehaviorSettings.fileLoggerAge) }, set: { QBTBehaviorSettings.fileLoggerAge = Int32(clamping: min(max($0, 1), 365)) }), format: .number.grouping(.never))
                            .labelsHidden()
                            .multilineTextAlignment(.trailing)
                            .frame(width: 60)
                        Picker("Unit", selection: setting($revision, get: { QBTBehaviorSettings.fileLoggerAgeType }, set: { QBTBehaviorSettings.fileLoggerAgeType = $0 })) {
                            Text("days").tag(Int32(0))
                            Text("months").tag(Int32(1))
                            Text("years").tag(Int32(2))
                        }
                        .labelsHidden()
                        .fixedSize()
                    }
                }
                .disabled(!QBTBehaviorSettings.fileLoggerDeleteOld)
            }
            Toggle("Log performance warnings", isOn: setting($revision, get: { QBTBehaviorSettings.performanceWarningEnabled }, set: { QBTBehaviorSettings.performanceWarningEnabled = $0 }))
        } header: {
            Text("Log")
        } footer: {
            Text("Old files are the ones set aside when a new file is started.")
        }
    }
}
