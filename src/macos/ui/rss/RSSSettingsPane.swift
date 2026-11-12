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

/// The RSS pane of the settings window.
struct RSSSettingsPane: View {
    @State private var revision = 0
    @State private var smartFilters = ""
    private let rss = QBTRSS.shared
    private let rules = QBTRSSRules.shared

    var body: some View {
        let _ = revision
        Form {
            Section("Reader") {
                Toggle("Fetch RSS feeds", isOn: setting($revision, get: { rss.processingEnabled }, set: { rss.processingEnabled = $0 }))
                LabeledContent("Update feeds every") {
                    HStack(spacing: 6) {
                        TextField("Update feeds every", value: setting($revision, get: { Int(rss.refreshInterval) }, set: { rss.refreshInterval = Int32(clamping: $0) }), format: .number.grouping(.never))
                            .labelsHidden()
                            .multilineTextAlignment(.trailing)
                            .frame(width: 90)
                        Text("minutes").foregroundStyle(.secondary)
                    }
                }
                LabeledContent("Wait between fetches from the same host") {
                    HStack(spacing: 6) {
                        TextField("Wait between fetches from the same host", value: setting($revision, get: { Int(rss.fetchDelay) }, set: { rss.fetchDelay = Int32(clamping: $0) }), format: .number.grouping(.never))
                            .labelsHidden()
                            .multilineTextAlignment(.trailing)
                            .frame(width: 90)
                        Text("seconds").foregroundStyle(.secondary)
                    }
                }
                LabeledContent("Articles kept per feed") {
                    TextField("Articles kept per feed", value: setting($revision, get: { Int(rss.maxArticlesPerFeed) }, set: { rss.maxArticlesPerFeed = Int32(clamping: $0) }), format: .number.grouping(.never))
                        .labelsHidden()
                        .multilineTextAlignment(.trailing)
                        .frame(width: 90)
                }
            }
            Section {
                Toggle("Download torrents with rules", isOn: setting($revision, get: { rules.autoDownloadEnabled }, set: { rules.autoDownloadEnabled = $0 }))
                Toggle("Download REPACK and PROPER episodes", isOn: setting($revision, get: { rules.downloadRepacks }, set: { rules.downloadRepacks = $0 }))
                LabeledContent("Download rules") {
                    Button("Edit Rules…") { AppDelegate.shared.showRSSRules() }
                }
            } header: {
                Text("Automatic downloads")
            } footer: {
                Text("Rules pick torrents from new articles by their titles.")
            }
            Section {
                TextEditor(text: $smartFilters)
                    .font(.body.monospaced())
                    .frame(minHeight: 90)
                    .onChange(of: smartFilters) {
                        rules.smartEpisodeFilters = smartFilters.components(separatedBy: .newlines)
                    }
            } header: {
                Text("Smart episode filters")
            } footer: {
                Text("Regular expressions, one per line, that find the season and episode in a title. Rules with the smart episode filter use them to download each episode only once.")
            }
            if !rss.isAvailable {
                Text("RSS starts once the torrents are loaded.")
                    .foregroundStyle(.secondary)
            }
        }
        .onAppear {
            smartFilters = rules.smartEpisodeFilters.joined(separator: "\n")
        }
    }
}
