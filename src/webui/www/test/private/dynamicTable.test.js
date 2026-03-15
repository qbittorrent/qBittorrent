/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Muhammad Hassan Raza <m.hassanraza100@gmail.com>
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

import { expect, test } from "vitest";

window.qBittorrent ??= {};
window.qBittorrent.LocalPreferences = {
    LocalPreferences: class {
        get(_key, defaultValue) {
            return defaultValue;
        }

        set() {}
    }
};
window.qBittorrent.ClientData = {
    get: (key) => {
        if (key === "use_virtual_list")
            return false;
        return null;
    }
};

await import("../../private/scripts/misc.js");
await import("../../private/scripts/progressbar.js");
await import("../../private/scripts/dynamicTable.js");

const createTorrentsTable = () => {
    const table = new window.qBittorrent.DynamicTable.TorrentsTable();
    table.dynamicTableDivId = "testTransferList";
    table.columns = [];
    table.colgroup = document.createElement("colgroup");
    table.fixedTableHeader = document.createElement("tr");
    table.rows = new Map();
    table.selectedRows = [];
    table.useVirtualList = false;
    table.tableBody = document.createElement("tbody");
    table.contextMenu = null;
    table.sortedColumn = "name";
    table.reverseSort = "0";
    table.initColumns();
    return table;
};

test("Test progress column recolors when only state changes", () => {
    const table = createTorrentsTable();
    table.updateRowData({
        rowId: "torrent-1",
        name: "Ubuntu ISO",
        progress: 0.5,
        state: "downloading"
    });

    const row = table.getRow("torrent-1");
    const tr = table.createRowElement(row);
    table.updateRow(tr, true);

    const progressCell = tr.children[table.getColumnPos("progress")];
    const progressBar = progressCell.firstElementChild;

    expect(progressBar.getValue()).toBe(50);
    expect(progressBar.getBarColor()).toBe("var(--color-progress-downloading)");

    table.updateRowData({
        rowId: "torrent-1",
        state: "stalledDL"
    });
    table.updateRow(tr, false);

    expect(progressCell.firstElementChild).toBe(progressBar);
    expect(progressBar.getValue()).toBe(50);
    expect(progressBar.getBarColor()).toBe("var(--color-progress-stalled)");
});
