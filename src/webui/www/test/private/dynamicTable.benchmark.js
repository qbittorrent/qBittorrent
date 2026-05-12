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

import { Window } from "happy-dom";

const testWindow = new Window();
globalThis.window = testWindow;
globalThis.document = testWindow.document;
globalThis.HTMLElement = testWindow.HTMLElement;
globalThis.Event = testWindow.Event;

const {
    createTableFixture,
    DynamicTable,
    resetDynamicTableTestState,
} = await import("./dynamicTable.test-utils.js");

const createSearchResultsTable = (rowCount) => {
    const table = createTableFixture(DynamicTable.SearchResultsTable);
    document.body.insertAdjacentHTML("beforeend", "<select id=\"searchInTorrentName\"><option value=\"everywhere\">Everywhere</option><option value=\"names\">Names</option></select>");

    for (const i of Array.from({ length: rowCount }, (value, index) => index)) {
        table.updateRowData({
            rowId: `${i}`,
            fileName: `linux-${rowCount - i}.iso`,
            fileSize: i * 1024,
            nbSeeders: i % 200,
            nbLeechers: 0,
            engineName: "Engine",
            siteUrl: "",
            pubDate: 0,
        });
    }

    table.updateTable();
    return table;
};

const measure = (label, iterations, callback) => {
    const startedAt = performance.now();
    for (const i of Array.from({ length: iterations }, (value, index) => index))
        callback(i);
    const duration = performance.now() - startedAt;
    console.log(`${label}: ${duration.toFixed(2)} ms total, ${(duration / iterations).toFixed(4)} ms/op`);
};

resetDynamicTableTestState();
const table = createSearchResultsTable(10000);

measure("cached filtered rows lookup", 1000, () => {
    table.getFilteredAndSortedRows();
});

measure("select all visible rows", 10, () => {
    table.selectAll();
});

measure("seed filter recompute", 50, (i) => {
    window.qBittorrent.Search.searchSeedsFilter.min = (i % 2) === 0 ? 50 : 0;
    table.getFilteredAndSortedRows();
});
