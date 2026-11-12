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
import UniformTypeIdentifiers

/// A download rule as the editor changes it, before it is saved.
struct RSSRuleDraft: Hashable {
    var name: String
    var enabled: Bool
    var priority: Int32
    var mustContain: String
    var mustNotContain: String
    var useRegex: Bool
    var useSmartFilter: Bool
    var episodeFilter: String
    var feedURLs: [String]
    var ignoreDays: Int32
    let lastMatch: Date?
    var category: String
    var savePath: String
    var addStopped: Int32

    init(_ rule: QBTRSSRule) {
        name = rule.name
        enabled = rule.enabled
        priority = rule.priority
        mustContain = rule.mustContain
        mustNotContain = rule.mustNotContain
        useRegex = rule.useRegex
        useSmartFilter = rule.useSmartFilter
        episodeFilter = rule.episodeFilter
        feedURLs = rule.feedURLs
        ignoreDays = rule.ignoreDays
        lastMatch = rule.lastMatch
        category = rule.category
        savePath = rule.savePath
        addStopped = rule.addStopped
    }

    var bridged: QBTRSSRule {
        let rule = QBTRSSRule(name: name)
        rule.enabled = enabled
        rule.priority = priority
        rule.mustContain = mustContain
        rule.mustNotContain = mustNotContain
        rule.useRegex = useRegex
        rule.useSmartFilter = useSmartFilter
        rule.episodeFilter = episodeFilter
        rule.feedURLs = feedURLs
        rule.ignoreDays = ignoreDays
        rule.category = category
        rule.savePath = savePath
        rule.addStopped = addStopped
        return rule
    }
}

/// The automated download rules: their list, the selected rule's editor and
/// the articles it matches, in a sheet over the RSS page. Edits are saved
/// with the Save button, or when another rule is selected, as in the Qt
/// editor.
struct RSSRulesView: View {
    @Environment(RSSModel.self) private var model
    @Environment(\.dismiss) private var dismiss
    @State private var selectedName: String?
    /// The selected rule, as last saved, and as edited.
    @State private var saved: RSSRuleDraft?
    @State private var draft: RSSRuleDraft?
    @State private var removal: String?
    @State private var errorMessage: String?
    private let rulesAPI = QBTRSSRules.shared

    private var rules: [RSSRuleDraft] {
        let _ = model.refreshCount
        return rulesAPI.rules.map(RSSRuleDraft.init)
    }

    var body: some View {
        VStack(spacing: 0) {
            editor
            Divider()
            HStack {
                Spacer()
                Button("Done") { dismiss() }
                    .keyboardShortcut(.cancelAction)
            }
            .padding(Metrics.margin)
        }
        .frame(minWidth: 820, idealWidth: 1000, minHeight: 520, idealHeight: 680)
    }

    private var editor: some View {
        HSplitView {
            VStack(spacing: 0) {
                ruleList
                Divider()
                ruleButtons
            }
            .frame(minWidth: 200, idealWidth: 230, maxWidth: 320)
            Group {
                if draft != nil {
                    RSSRuleForm(draft: Binding($draft)!, isDirty: draft != saved, save: { save() }, revert: { draft = saved })
                } else {
                    ContentUnavailableView("No Rule Selected", systemImage: "wand.and.stars")
                }
            }
            .frame(minWidth: 520, maxWidth: .infinity, maxHeight: .infinity)
        }
        .onChange(of: selectedName) { oldName, newName in
            if oldName != nil, draft != saved { save(reselecting: false) }
            select(newName)
        }
        .onDisappear {
            if draft != saved { save(reselecting: false) }
        }
        .confirmationDialog(String(localized: "Delete the rule “\(removal ?? "")”?"), isPresented: isRemoving) {
            Button("Delete", role: .destructive) {
                if let removal { rulesAPI.removeRule(removal) }
                selectedName = nil
            }
            Button("Cancel", role: .cancel) {}
        }
        .alert("RSS Download Rules", isPresented: isShowingError) {
            Button("OK") { errorMessage = nil }
        } message: {
            Text(errorMessage ?? "")
        }
    }

    private var ruleList: some View {
        List(selection: $selectedName) {
            ForEach(rules, id: \.name) { rule in
                Toggle(isOn: enabledBinding(rule)) {
                    Text(rule.name).lineLimit(1)
                }
                .tag(rule.name)
            }
        }
        .overlay {
            if rules.isEmpty {
                ContentUnavailableView {
                    Label("No Rules", systemImage: "wand.and.stars")
                } description: {
                    Text("Add a rule with the + button.")
                }
            }
        }
    }

    /// Adds, deletes, imports and exports rules, under the list.
    private var ruleButtons: some View {
        HStack(spacing: 4) {
            Button(action: addRule) {
                Image(systemName: "plus")
                    .frame(width: 20, height: 20)
            }
            .help("Add a download rule")
            .keyboardShortcut("n", modifiers: .command)
            .disabled(!model.isAvailable)
            Button {
                removal = selectedName
            } label: {
                Image(systemName: "minus")
                    .frame(width: 20, height: 20)
            }
            .help("Delete the selected rule")
            .disabled(selectedName == nil)
            Spacer()
            Menu {
                Button("Import Rules…", action: importRules)
                Button("Export Rules…", action: exportRules)
                    .disabled(rules.isEmpty)
            } label: {
                Image(systemName: "ellipsis.circle")
            }
            .menuStyle(.borderlessButton)
            .menuIndicator(.hidden)
            .fixedSize()
            .help("Import or export rules")
            .disabled(!model.isAvailable)
        }
        .buttonStyle(.borderless)
        .barStyle()
    }

    // MARK: Actions

    private func select(_ name: String?) {
        let rule = rules.first { $0.name == name }
        saved = rule
        draft = rule
    }

    /// With `reselecting`, a renamed rule stays selected under its new name.
    private func save(reselecting: Bool = true) {
        guard let draft, let saved else { return }
        let name = draft.name.trimmingCharacters(in: .whitespaces)
        if name.isEmpty {
            errorMessage = String(localized: "A rule needs a name.")
            return
        }
        if name != saved.name, rulesAPI.hasRule(name) {
            errorMessage = String(localized: "A rule named “\(name)” already exists.")
            return
        }
        var stored = draft
        stored.name = name
        guard rulesAPI.setRule(stored.bridged, originalName: saved.name) else {
            errorMessage = String(localized: "The rule could not be saved.")
            return
        }
        self.saved = stored
        self.draft = stored
        if reselecting, selectedName != name {
            // Saved already; selecting the new name must not save again.
            selectedName = name
        }
    }

    private func addRule() {
        let base = String(localized: "New Rule")
        var name = base
        var number = 2
        while rulesAPI.hasRule(name) {
            name = "\(base) \(number)"
            number += 1
        }
        let rule = QBTRSSRule(name: name)
        rule.enabled = true
        rulesAPI.setRule(rule, originalName: nil)
        model.refresh()
        selectedName = name
    }

    private func enabledBinding(_ rule: RSSRuleDraft) -> Binding<Bool> {
        Binding {
            rule.name == saved?.name ? (draft?.enabled ?? rule.enabled) : rule.enabled
        } set: { enabled in
            var changed = rule
            changed.enabled = enabled
            rulesAPI.setRule(changed.bridged, originalName: rule.name)
            if rule.name == saved?.name {
                saved?.enabled = enabled
                draft?.enabled = enabled
            }
            model.refresh()
        }
    }

    private static let legacyType = UTType(filenameExtension: "rssrules") ?? .data

    private func importRules() {
        let panel = NSOpenPanel()
        panel.allowedContentTypes = [.json, Self.legacyType]
        panel.message = String(localized: "Choose a rules file to import")
        guard panel.runModal() == .OK, let url = panel.url else { return }
        do {
            let data = try Data(contentsOf: url)
            let format: QBTRSSRulesFormat = url.pathExtension.lowercased() == "rssrules" ? .legacy : .JSON
            try rulesAPI.importRules(data, format: format)
            model.refresh()
        } catch {
            errorMessage = String(localized: "The rules could not be imported: \(error.localizedDescription)")
        }
    }

    private func exportRules() {
        if draft != saved { save() }
        let panel = NSSavePanel()
        panel.allowedContentTypes = [.json, Self.legacyType]
        panel.nameFieldStringValue = "rules.json"
        guard panel.runModal() == .OK, let url = panel.url else { return }
        let format: QBTRSSRulesFormat = url.pathExtension.lowercased() == "rssrules" ? .legacy : .JSON
        do {
            try rulesAPI.exportRules(format: format).write(to: url, options: .atomic)
        } catch {
            errorMessage = String(localized: "The rules could not be exported: \(error.localizedDescription)")
        }
    }

    private var isRemoving: Binding<Bool> {
        Binding(get: { removal != nil }, set: { if !$0 { removal = nil } })
    }

    private var isShowingError: Binding<Bool> {
        Binding(get: { errorMessage != nil }, set: { if !$0 { errorMessage = nil } })
    }
}
