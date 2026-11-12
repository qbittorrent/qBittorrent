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

typealias SearchResultSort = KeyPathComparator<SearchResultItem>

/// The results of one search, filtered and sorted, as they come in.
struct SearchResultsView: View {
    @Environment(SearchModel.self) private var model
    @Bindable var tab: SearchTab
    @State private var sortOrder = [SearchResultSort(\SearchResultItem.seeders, order: .reverse)]
    @AppStorage("Search/Columns") private var columns = TableColumnCustomization<SearchResultItem>()

    var body: some View {
        let shown = tab.filteredResults
        VStack(spacing: 0) {
            SearchFilterBar(filter: $tab.filter)
            Divider()
            table(shown.sorted(using: sortOrder))
            Divider()
            statusBar(shownCount: shown.count)
        }
    }

    private func table(_ rows: [SearchResultItem]) -> some View {
        Table(rows, selection: $tab.selection, sortOrder: $sortOrder, columnCustomization: $columns) {
            TableColumn("Name", value: \SearchResultItem.name) { (item: SearchResultItem) in
                Text(item.name).help(item.name)
            }
            .width(min: 160, ideal: 380)
            .customizationID("name")

            TableColumn("Size", value: \SearchResultItem.size) { (item: SearchResultItem) in
                ResultNumber(item.size >= 0 ? Format.bytes(item.size) : "")
            }
            .width(min: 60, ideal: 80)
            .customizationID("size")

            TableColumn("Seeders", value: \SearchResultItem.seeders) { (item: SearchResultItem) in
                ResultNumber(count(item.seeders))
            }
            .width(min: 50, ideal: 70)
            .customizationID("seeders")

            TableColumn("Leechers", value: \SearchResultItem.leechers) { (item: SearchResultItem) in
                ResultNumber(count(item.leechers))
            }
            .width(min: 50, ideal: 70)
            .customizationID("leechers")

            TableColumn("Engine", value: \SearchResultItem.engine) { (item: SearchResultItem) in
                Text(item.engine)
            }
            .width(min: 80, ideal: 120)
            .customizationID("engine")

            TableColumn("Published On", value: \SearchResultItem.publishedOn) { (item: SearchResultItem) in
                Text(item.publishedOn == .distantPast ? "" : item.publishedOn.formatted(date: .abbreviated, time: .omitted))
            }
            .width(min: 80, ideal: 110)
            .customizationID("published")
        }
        .columnsSizeToFit()
        .contextMenu(forSelectionType: SearchResultItem.ID.self) { ids in
            if !ids.isEmpty {
                let results = tab.results.filter { ids.contains($0.id) }.map(\.result)
                Button("Download") { model.download(results) }
                Button("Open Description Page") { model.openDescriptionPages(results) }
                Divider()
                Button("Copy Download Link") { model.copyLinks(results) }
            }
        } primaryAction: { ids in
            model.download(tab.results.filter { ids.contains($0.id) }.map(\.result))
        }
        .overlay {
            if tab.results.isEmpty {
                emptyState
            }
        }
    }

    @ViewBuilder
    private var emptyState: some View {
        switch tab.status {
        case .running:
            ProgressView("Searching…")
        case .failed(let error):
            ContentUnavailableView("Search Failed", systemImage: "exclamationmark.triangle",
                                   description: Text(error.isEmpty ? String(localized: "The search engine stopped with an error.") : error))
        default:
            ContentUnavailableView("No Results", systemImage: "magnifyingglass",
                                   description: Text("No torrents were found for “\(tab.pattern)”."))
        }
    }

    private func statusBar(shownCount: Int) -> some View {
        HStack {
            if tab.isRunning {
                ProgressView().controlSize(.small)
            }
            Text(shownCount == tab.results.count
                 ? String(localized: "\(tab.results.count) results")
                 : String(localized: "\(shownCount) of \(tab.results.count) results"))
                .foregroundStyle(.secondary)
            Spacer()
            Button("Open Description Page") { model.openDescriptionPages(tab.selectedResults()) }
                .disabled(tab.selection.isEmpty)
            Button("Download") { model.download(tab.selectedResults()) }
                .disabled(tab.selection.isEmpty)
                .keyboardShortcut(.defaultAction)
        }
        .barStyle()
    }

    /// Sites that do not count peers report -1.
    private func count(_ value: Int64) -> String {
        value >= 0 ? value.formatted() : ""
    }
}

/// Filters results by name, seeds and size.
private struct SearchFilterBar: View {
    @Binding var filter: SearchResultFilter

    var body: some View {
        HStack(spacing: 12) {
            TextField("Filter results", text: $filter.text, prompt: Text("Filter by name"))
                .textFieldStyle(.roundedBorder)
                .frame(minWidth: 140, maxWidth: 260)
            LabeledContent("Seeds:") {
                range($filter.minimumSeeds, $filter.maximumSeeds, format: .number)
            }
            LabeledContent("Size (MiB):") {
                range($filter.minimumSize, $filter.maximumSize, format: .number)
            }
            Spacer()
            if filter != SearchResultFilter() {
                Button("Clear") { filter = SearchResultFilter() }
            }
        }
        .barStyle()
    }

    /// Two fields; an empty one leaves that end open.
    private func range<Value, Style: ParseableFormatStyle>(_ minimum: Binding<Value?>, _ maximum: Binding<Value?>, format: Style) -> some View
    where Style.FormatInput == Value, Style.FormatOutput == String {
        HStack(spacing: 4) {
            TextField("Minimum", value: minimum, format: format, prompt: Text("Min"))
                .frame(width: 60)
            Text("–")
            TextField("Maximum", value: maximum, format: format, prompt: Text("Max"))
                .frame(width: 60)
        }
        .textFieldStyle(.roundedBorder)
        .multilineTextAlignment(.trailing)
    }
}

private struct ResultNumber: View {
    let text: String

    init(_ text: String) {
        self.text = text
    }

    var body: some View {
        Text(verbatim: text).monospacedDigit()
    }
}
