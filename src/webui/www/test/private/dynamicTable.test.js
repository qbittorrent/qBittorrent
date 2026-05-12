/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  The qBittorrent project
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

import { beforeEach, expect, test, vi } from "vitest";

import {
    DynamicTable,
    createTableFixture,
    flushAnimationFrames,
    resetDynamicTableTestState,
    setClientData,
}
from "./dynamicTable.test-utils.js";

const rowIds = (rows) => rows.map(row => row.rowId);

beforeEach(() => {
    resetDynamicTableTestState();
});

test("selected rows use a Set while preserving public array snapshots", () => {
    const table = createTableFixture(DynamicTable.SearchPluginsTable);
    table.updateRowData({ rowId: "plugin-a", fullName: "Plugin A", version: "1.0", url: "https://example.com/a", enabled: true });
    table.updateRowData({ rowId: "plugin-b", fullName: "Plugin B", version: "1.0", url: "https://example.com/b", enabled: true });
    table.updateRowData({ rowId: "plugin-c", fullName: "Plugin C", version: "1.0", url: "https://example.com/c", enabled: true });
    table.updateTable();

    table.selectRow("plugin-a");
    table.selectRow("plugin-a");
    expect(table.selectedRowsIds()).toStrictEqual(["plugin-a"]);

    table.selectAll();
    expect(table.selectedRowsIds()).toStrictEqual(["plugin-a", "plugin-b", "plugin-c"]);

    const selectedRows = table.selectedRowsIds();
    selectedRows.push("local-only");
    expect(table.selectedRowsIds()).toStrictEqual(["plugin-a", "plugin-b", "plugin-c"]);

    table.removeRow("plugin-b");
    table.updateTable();
    expect(table.selectedRowsIds()).toStrictEqual(["plugin-a", "plugin-c"]);

    table.reselectRows(["plugin-c", "plugin-a"]);
    expect(table.getSelectedRowId()).toBe("plugin-c");
});

test("filtered rows cache invalidates on row data and filter key changes", () => {
    const table = createTableFixture(DynamicTable.SearchResultsTable);
    document.body.insertAdjacentHTML("beforeend", "<select id=\"searchInTorrentName\"><option value=\"everywhere\">Everywhere</option><option value=\"names\">Names</option></select>");

    table.updateRowData({ rowId: "1", fileName: "ubuntu.iso", fileSize: 100, nbSeeders: 2, nbLeechers: 0, engineName: "Engine", siteUrl: "", pubDate: 0 });
    table.updateRowData({ rowId: "2", fileName: "debian.iso", fileSize: 200, nbSeeders: 8, nbLeechers: 0, engineName: "Engine", siteUrl: "", pubDate: 0 });

    const allRows = table.getFilteredAndSortedRows();
    expect(rowIds(allRows)).toStrictEqual(["2", "1"]);
    expect(table.getFilteredAndSortedRows()).toBe(allRows);

    table.updateRowData({ rowId: "3", fileName: "arch.iso", fileSize: 300, nbSeeders: 5, nbLeechers: 0, engineName: "Engine", siteUrl: "", pubDate: 0 });
    const rowsAfterUpdate = table.getFilteredAndSortedRows();
    expect(rowIds(rowsAfterUpdate)).toStrictEqual(["3", "2", "1"]);
    expect(rowsAfterUpdate).not.toBe(allRows);

    window.qBittorrent.Search.searchSeedsFilter.min = 6;
    expect(rowIds(table.getFilteredAndSortedRows())).toStrictEqual(["2"]);

    window.qBittorrent.Search.searchSeedsFilter.min = 0;
    expect(rowIds(table.getFilteredAndSortedRows())).toStrictEqual(["3", "2", "1"]);
});

test("search result filtering reacts to search-in mode without manual invalidation", () => {
    const table = createTableFixture(DynamicTable.SearchResultsTable);
    document.body.insertAdjacentHTML("beforeend", "<select id=\"searchInTorrentName\"><option value=\"everywhere\">Everywhere</option><option value=\"names\">Names</option></select>");

    table.updateRowData({ rowId: "1", fileName: "ubuntu.iso", fileSize: 100, nbSeeders: 2, nbLeechers: 0, engineName: "Engine", siteUrl: "", pubDate: 0 });
    table.updateRowData({ rowId: "2", fileName: "debian.iso", fileSize: 200, nbSeeders: 8, nbLeechers: 0, engineName: "Engine", siteUrl: "", pubDate: 0 });

    window.qBittorrent.Search.searchText.pattern = "ubuntu";
    document.getElementById("searchInTorrentName").value = "names";
    expect(rowIds(table.getFilteredAndSortedRows())).toStrictEqual(["1"]);

    document.getElementById("searchInTorrentName").value = "everywhere";
    expect(rowIds(table.getFilteredAndSortedRows())).toStrictEqual(["2", "1"]);
});

test("sort state can be restored without an early table update", () => {
    const table = createTableFixture(DynamicTable.SearchPluginsTable);
    const updateTable = vi.spyOn(table, "updateTable");

    table.setSortedColumn("version", "1", false);

    expect(table.sortedColumn).toBe("version");
    expect(table.reverseSort).toBe("1");
    expect(updateTable).not.toHaveBeenCalled();
});

test("virtual scroll rerendering is throttled through requestAnimationFrame", () => {
    setClientData("use_virtual_list", true);
    const table = createTableFixture(DynamicTable.SearchPluginsTable);
    const rerender = vi.spyOn(table, "rerender").mockImplementation(() => {});

    table.dynamicTableDiv.dispatchEvent(new Event("scroll"));
    table.dynamicTableDiv.dispatchEvent(new Event("scroll"));

    expect(rerender).not.toHaveBeenCalled();
    flushAnimationFrames();
    expect(rerender).toHaveBeenCalledTimes(1);
});
