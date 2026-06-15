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

const preferences = new Map();
const clientData = new Map();
let animationFrameCallbacks = [];

class LocalPreferences {
    get(key, defaultValue = null) {
        return preferences.has(key) ? preferences.get(key) : defaultValue;
    }

    set(key, value) {
        preferences.set(key, value);
    }
}

class TestContextMenu {
    constructor(options) {
        this.options = options;
    }

    addTarget() {}
    setItemChecked() {}
}

const qBittorrent = window.qBittorrent ??= {};
qBittorrent.LocalPreferences = { LocalPreferences: LocalPreferences };
qBittorrent.ClientData = {
    get: (key) => clientData.get(key)
};
qBittorrent.ContextMenu = { ContextMenu: TestContextMenu };
qBittorrent.Misc = {
    containsAllTerms: (text, terms) => terms.every(term => text.toLowerCase().includes(term)),
    createDebounceHandler: (delay, fn) => fn,
    formatDate: (date) => date.toISOString(),
    friendlyUnit: (value) => `${value}`,
    naturalSortCollator: new Intl.Collator(undefined, { numeric: true, sensitivity: "base" }),
};

window.parent = window;
window.requestAnimationFrame = (callback) => {
    animationFrameCallbacks.push(callback);
    return animationFrameCallbacks.length;
};
globalThis.requestAnimationFrame = window.requestAnimationFrame;
globalThis.ResizeObserver = class {
    observe() {}
};
HTMLElement.prototype.makeResizable = function() {};

await import("../../private/scripts/dynamicTable.js");

const DynamicTable = window.qBittorrent.DynamicTable;
const rowIds = (rows) => rows.map(row => row.rowId);

const resetDynamicTableTestState = () => {
    preferences.clear();
    clientData.clear();
    clientData.set("display_density", "default");
    clientData.set("use_virtual_list", false);
    animationFrameCallbacks = [];
    document.body.replaceChildren();

    qBittorrent.Search = {
        searchSeedsFilter: {
            min: 0,
            max: 0,
        },
        searchSizeFilter: {
            min: 0,
            minUnit: 0,
            max: 0,
            maxUnit: 0,
        },
        searchText: {
            pattern: "",
            filterPattern: "",
        },
    };
};

const createTableFixture = (TableClass) => {
    document.body.insertAdjacentHTML("beforeend", `
        <div id="dynamicTableFixedHeader"><table><thead><tr></tr></thead></table></div>
        <div id="dynamicTable"><table><tbody></tbody></table></div>
    `);

    const table = new TableClass();
    table.setup("dynamicTable", "dynamicTableFixedHeader");
    return table;
};

const addSearchModeSelect = () => {
    document.body.insertAdjacentHTML("beforeend", "<select id=\"searchInTorrentName\"><option value=\"everywhere\">Everywhere</option><option value=\"names\">Names</option></select>");
};

const addSearchResult = (table, rowId, fileName, nbSeeders) => {
    table.updateRowData({ rowId: rowId, fileName: fileName, fileSize: 100, nbSeeders: nbSeeders, nbLeechers: 0, engineName: "Engine", siteUrl: "", pubDate: 0 });
};

const flushAnimationFrames = () => {
    const callbacks = animationFrameCallbacks;
    animationFrameCallbacks = [];
    for (const callback of callbacks)
        callback();
};

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
    addSearchModeSelect();
    addSearchResult(table, "1", "ubuntu.iso", 2);
    addSearchResult(table, "2", "debian.iso", 8);

    const allRows = table.getFilteredAndSortedRows();
    expect(rowIds(allRows)).toStrictEqual(["2", "1"]);
    expect(table.getFilteredAndSortedRows()).toBe(allRows);

    addSearchResult(table, "3", "arch.iso", 5);
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
    addSearchModeSelect();
    addSearchResult(table, "1", "ubuntu.iso", 2);
    addSearchResult(table, "2", "debian.iso", 8);

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
    clientData.set("use_virtual_list", true);
    const table = createTableFixture(DynamicTable.SearchPluginsTable);
    const rerender = vi.spyOn(table, "rerender").mockImplementation(() => {});

    table.dynamicTableDiv.dispatchEvent(new Event("scroll"));
    table.dynamicTableDiv.dispatchEvent(new Event("scroll"));

    expect(rerender).not.toHaveBeenCalled();
    flushAnimationFrames();
    expect(rerender).toHaveBeenCalledTimes(1);
});
