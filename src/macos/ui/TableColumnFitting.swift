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
import SwiftUI

// SwiftUI's Table has no API for sizing a column to its content, so this file
// reaches the NSTableView that backs a Table and sizes its columns through
// AppKit. It relies on how SwiftUI builds a Table today (an NSTableView in an
// NSScrollView, with each cell's content in an NSHostingView). When that
// changes, the table is not found or its cells are not measured, and the
// commands do nothing.

extension View {
    /// Lets the user size the columns of the Table this modifies to their content,
    /// as Qt's "Resize to contents" does: "Size Column to Fit" and "Size All Columns
    /// to Fit" in the header's context menu, and double-clicking a column divider.
    /// A Table with column customization keeps the new widths with the others.
    func columnsSizeToFit() -> some View {
        background(TableAnchor())
    }
}

/// Sits behind a Table, with the same frame, to tell which NSTableView backs it.
private struct TableAnchor: NSViewRepresentable {
    func makeNSView(context: Context) -> TableAnchorView {
        TableAnchorView()
    }

    func updateNSView(_ view: TableAnchorView, context: Context) {}
}

private final class TableAnchorView: NSView {
    private weak var cachedTable: NSTableView?

    override func hitTest(_ point: NSPoint) -> NSView? {
        nil
    }

    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        if window != nil {
            ColumnFitter.shared.add(self)
        }
    }

    /// The table whose scroll view covers most of this view's frame.
    var table: NSTableView? {
        guard let window, let contentView = window.contentView else { return nil }
        let frame = convert(bounds, to: nil)
        if let table = cachedTable, table.window === window, overlap(of: table, with: frame) > 0.9 {
            return table
        }
        var best: (table: NSTableView, overlap: CGFloat)?
        forEachTable(in: contentView) { table in
            let overlap = overlap(of: table, with: frame)
            if overlap > (best?.overlap ?? 0.9) {
                best = (table, overlap)
            }
        }
        cachedTable = best?.table
        return cachedTable
    }

    /// The share of the larger of the two areas that both cover.
    private func overlap(of table: NSTableView, with frame: NSRect) -> CGFloat {
        guard let scrollView = table.enclosingScrollView, !table.isHiddenOrHasHiddenAncestor else { return 0 }
        let tableFrame = scrollView.convert(scrollView.bounds, to: nil)
        let common = tableFrame.intersection(frame)
        let largest = max(tableFrame.width * tableFrame.height, frame.width * frame.height)
        guard !common.isNull, largest > 0 else { return 0 }
        return common.width * common.height / largest
    }

    private func forEachTable(in view: NSView, _ body: (NSTableView) -> Void) {
        if let table = view as? NSTableView {
            body(table)
            return
        }
        for subview in view.subviews {
            forEachTable(in: subview, body)
        }
    }
}

/// Watches the mouse for the header clicks of the tables that opted in.
@MainActor
private final class ColumnFitter {
    static let shared = ColumnFitter()

    /// How many cells beyond the visible rows one command measures, from the top:
    /// Qt's default of 1000 rows for one column, fewer rows per column for many,
    /// since measuring a cell that is not shown takes about half a millisecond.
    private static let cellLimit = 1000
    /// How close to a divider a double-click sizes the column on its left.
    private static let dividerSlop: CGFloat = 4

    private var anchors: [WeakAnchor] = []
    private var monitor: Any?

    func add(_ anchor: TableAnchorView) {
        anchors.removeAll { $0.view == nil || $0.view === anchor }
        anchors.append(WeakAnchor(view: anchor))
        guard monitor == nil else { return }
        monitor = NSEvent.addLocalMonitorForEvents(matching: [.leftMouseDown, .rightMouseDown]) { event in
            let handled = MainActor.assumeIsolated { ColumnFitter.shared.handle(event) }
            return handled ? nil : event
        }
    }

    private func handle(_ event: NSEvent) -> Bool {
        let isMenuClick = event.type == .rightMouseDown
            || (event.type == .leftMouseDown && event.modifierFlags.contains(.control))
        let isDoubleClick = event.type == .leftMouseDown && event.clickCount == 2
        guard isMenuClick || isDoubleClick, let window = event.window else { return false }

        for anchor in anchors {
            guard let view = anchor.view, view.window === window, let table = view.table,
                  let header = table.headerView else { continue }
            let point = header.convert(event.locationInWindow, from: nil)
            guard header.visibleRect.contains(point) else { continue }
            if isMenuClick {
                showMenu(for: event, in: header, table: table, column: header.column(at: point))
                return true
            }
            guard let column = dividerColumn(at: point, in: header, table: table) else { return false }
            sizeToFit(table, columns: [column])
            return true
        }
        return false
    }

    /// The column whose right edge is at `point`, if it can be resized.
    private func dividerColumn(at point: NSPoint, in header: NSTableHeaderView, table: NSTableView) -> Int? {
        let columns = table.columnIndexes(in: header.visibleRect)
        return columns.first { column in
            abs(header.headerRect(ofColumn: column).maxX - point.x) <= Self.dividerSlop
                && isResizable(table.tableColumns[column])
        }
    }

    /// Shows the menu that SwiftUI gives the header, if any, with the sizing commands added.
    private func showMenu(for event: NSEvent, in header: NSTableHeaderView, table: NSTableView, column: Int) {
        let menu = header.menu(for: event) ?? NSMenu()
        if !menu.items.isEmpty {
            menu.addItem(.separator())
        }
        let sizeColumn = MenuAction(String(localized: "Size Column to Fit")) { [weak table] in
            if let table { self.sizeToFit(table, columns: [column]) }
        }
        sizeColumn.isEnabled = column >= 0 && isResizable(table.tableColumns[column])
        menu.addItem(sizeColumn)
        menu.addItem(MenuAction(String(localized: "Size All Columns to Fit")) { [weak table] in
            if let table { self.sizeToFit(table, columns: Array(table.tableColumns.indices)) }
        })
        NSMenu.popUpContextMenu(menu, with: event, for: header)
    }

    private func isResizable(_ column: NSTableColumn) -> Bool {
        !column.isHidden && column.resizingMask.contains(.userResizingMask) && column.maxWidth > column.minWidth
    }

    private func sizeToFit(_ table: NSTableView, columns: [Int]) {
        let visible = table.rows(in: table.visibleRect)
        let rowLimit = max(100, Self.cellLimit / max(1, columns.count))
        let rows = Set(visible.lowerBound..<visible.upperBound)
            .union(0..<min(table.numberOfRows, rowLimit))
        for column in columns where isResizable(table.tableColumns[column]) {
            let tableColumn = table.tableColumns[column]
            var width = headerWidth(of: tableColumn, in: table)
            for row in rows {
                if let cellWidth = contentWidth(of: table, column: column, row: row) {
                    width = max(width, cellWidth)
                }
            }
            tableColumn.width = min(max(width.rounded(.up), tableColumn.minWidth), tableColumn.maxWidth)
        }
    }

    private func headerWidth(of column: NSTableColumn, in table: NSTableView) -> CGFloat {
        // Not the cell's own size, which leaves room for the sort indicator only
        // on the column that is sorted now.
        let cell = column.headerCell
        var width = cell.attributedStringValue.size().width + 8
        if column.sortDescriptorPrototype != nil {
            let bounds = NSRect(x: 0, y: 0, width: 100, height: table.headerView?.frame.height ?? 24)
            width += cell.sortIndicatorRect(forBounds: bounds).width + 6
        }
        return width
    }

    /// The width the cell's content asks for. The hosting view spans the whole
    /// column, and a cell's content already holds its padding.
    private func contentWidth(of table: NSTableView, column: Int, row: Int) -> CGFloat? {
        guard let cell = table.view(atColumn: column, row: row, makeIfNecessary: true),
              let host = cell.subviews.first as? SizableHostingView else { return nil }
        let options = host.sizingOptions
        host.sizingOptions = options.union(.intrinsicContentSize)
        let ideal = host.intrinsicContentSize.width
        host.sizingOptions = options
        return ideal > 0 ? ideal + 1 : nil
    }
}

private struct WeakAnchor {
    weak var view: TableAnchorView?
}

/// Every NSHostingView, whatever its content, so a cell's ideal width can be read.
@MainActor
private protocol SizableHostingView: NSView {
    var sizingOptions: NSHostingSizingOptions { get set }
}

extension NSHostingView: SizableHostingView {}

private final class MenuAction: NSMenuItem {
    private let handler: () -> Void

    init(_ title: String, handler: @escaping () -> Void) {
        self.handler = handler
        super.init(title: title, action: #selector(run), keyEquivalent: "")
        target = self
    }

    required init(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    @objc private func run() {
        handler()
    }
}
