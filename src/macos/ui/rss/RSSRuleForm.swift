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

/// Edits one download rule, with the articles it matches beside it.
struct RSSRuleForm: View {
    @Environment(RSSModel.self) private var model
    @Binding var draft: RSSRuleDraft
    let isDirty: Bool
    let save: () -> Void
    let revert: () -> Void

    var body: some View {
        HSplitView {
            form
                .frame(minWidth: 340, maxHeight: .infinity)
            RSSRulePreview(draft: draft)
                .frame(minWidth: 180, idealWidth: 260, maxHeight: .infinity)
        }
    }

    private var form: some View {
        VStack(spacing: 0) {
            Form {
                Section {
                    TextField("Name", text: $draft.name)
                    Toggle("Enabled", isOn: $draft.enabled)
                    LabeledContent("Last match") {
                        if let date = draft.lastMatch {
                            Text(date, format: .relative(presentation: .named))
                        } else {
                            Text("Never")
                        }
                    }
                }
                Section {
                    TextField("Must contain", text: $draft.mustContain)
                    TextField("Must not contain", text: $draft.mustNotContain)
                    Toggle("Use regular expressions", isOn: $draft.useRegex)
                    TextField("Episode filter", text: $draft.episodeFilter, prompt: Text(verbatim: "1x2;8-15;5;30-;"))
                    Toggle("Use smart episode filter", isOn: $draft.useSmartFilter)
                } header: {
                    Text("Matching titles")
                } footer: {
                    Text(draft.useRegex
                         ? "Expressions are Perl-compatible regular expressions."
                         : "Wildcards: ? matches one character, * any number. Words separated by spaces must all appear; | separates alternatives.")
                }
                Section {
                    LabeledContent("Ignore matches for") {
                        HStack(spacing: 6) {
                            TextField("Ignore matches for", value: $draft.ignoreDays, format: .number.grouping(.never), prompt: Text("0"))
                                .labelsHidden()
                                .multilineTextAlignment(.trailing)
                                .frame(width: 60)
                            Text("days").foregroundStyle(.secondary)
                        }
                    }
                } footer: {
                    Text("After a match, the rule skips new matches for this many days. 0 never skips.")
                }
                Section("Use for these feeds") {
                    let feeds = model.root?.allFeeds ?? []
                    if feeds.isEmpty {
                        Text("No subscriptions").foregroundStyle(.secondary)
                    }
                    ForEach(feeds) { feed in
                        Toggle(isOn: feedBinding(feed.url)) {
                            Text(feed.name).lineLimit(1).help(feed.url)
                        }
                    }
                }
                Section("Torrents") {
                    CategoryPicker(category: $draft.category)
                    LabeledContent("Save to") {
                        HStack {
                            TextField("Save to", text: $draft.savePath, prompt: Text("Default folder"))
                                .labelsHidden()
                            Button("Choose…", action: chooseFolder)
                        }
                    }
                    Picker("Add stopped", selection: $draft.addStopped) {
                        Text("As in Settings").tag(Int32(-1))
                        Text("No").tag(Int32(0))
                        Text("Yes").tag(Int32(1))
                    }
                }
            }
            .formStyle(.grouped)
            Divider()
            HStack {
                Spacer()
                Button("Revert", action: revert)
                    .disabled(!isDirty)
                Button("Save", action: save)
                    .keyboardShortcut("s", modifiers: .command)
                    .disabled(!isDirty)
            }
            .barStyle()
        }
    }

    private func feedBinding(_ url: String) -> Binding<Bool> {
        Binding {
            draft.feedURLs.contains(url)
        } set: { isOn in
            draft.feedURLs.removeAll { $0 == url }
            if isOn { draft.feedURLs.append(url) }
        }
    }

    private func chooseFolder() {
        let panel = NSOpenPanel()
        panel.canChooseFiles = false
        panel.canChooseDirectories = true
        panel.canCreateDirectories = true
        if panel.runModal() == .OK, let url = panel.url {
            draft.savePath = url.path
        }
    }
}

/// The articles of the rule's feeds that the rule, as edited, matches.
private struct RSSRulePreview: View {
    @Environment(RSSModel.self) private var model
    let draft: RSSRuleDraft

    var body: some View {
        let _ = model.refreshCount
        let articles = QBTRSSRules.shared.articles(matching: draft.bridged).map(RSSArticleItem.init)
        let feeds = Dictionary(grouping: articles, by: \.feedTitle)
        List {
            ForEach(feeds.keys.sorted(), id: \.self) { feed in
                Section(feed) {
                    ForEach(feeds[feed] ?? []) { article in
                        Text(article.title)
                            .lineLimit(2)
                            .help(article.title)
                    }
                }
            }
        }
        .safeAreaInset(edge: .top, spacing: 0) {
            Text("Matching articles: \(articles.count)")
                .frame(maxWidth: .infinity, alignment: .leading)
                .barStyle()
                .background(.bar)
        }
        .overlay {
            if articles.isEmpty {
                Text(draft.feedURLs.isEmpty ? "Choose feeds to see matching articles." : "No articles match.")
                    .multilineTextAlignment(.center)
                    .foregroundStyle(.secondary)
                    .padding()
            }
        }
    }
}
