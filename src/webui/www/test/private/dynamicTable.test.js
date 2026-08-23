/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Uğur Gümüşhan <ugur.gumushan@icloud.com>
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

import "../../private/scripts/localPreferences.js";
import "../../private/scripts/client-data.js";
import "../../private/scripts/misc.js";
import "../../private/scripts/dynamicTable.js";

const DIV_ID = "searchPluginsTableDiv";
const HEADER_DIV_ID = "searchPluginsTableFixedHeaderDiv";
const ORDER_KEY = `columns_order_${DIV_ID}`;

// column names of SearchPluginsTable in their default order
const DEFAULT_COLUMN_NAMES = ["fullName", "version", "url", "enabled"];

const setupDom = () => {
    document.body.innerHTML = `
        <div id="${HEADER_DIV_ID}" class="dynamicTableFixedHeaderDiv">
            <table class="dynamicTable unselectable">
                <thead>
                    <tr class="dynamicTableHeader"></tr>
                </thead>
            </table>
        </div>
        <div id="${DIV_ID}" class="dynamicTableDiv">
            <table class="dynamicTable unselectable">
                <tbody></tbody>
            </table>
        </div>`;
};

// stub browser APIs that dynamicTable.js relies on but the test environment does not provide
const stubBrowserApis = () => {
    window.ResizeObserver ??= class ResizeObserver {
        observe() {}

        disconnect() {}
    };
    window.Element.prototype.makeResizable ??= function() {};
    window.qBittorrent.ContextMenu ??= { ContextMenu: class ContextMenu {} };
};

const createTable = (savedOrder) => {
    if (savedOrder === null)
        localStorage.removeItem(ORDER_KEY);
    else
        localStorage.setItem(ORDER_KEY, savedOrder);

    setupDom();
    const table = new window.qBittorrent.DynamicTable.SearchPluginsTable();
    // setup() calls initColumns(), loadColumnsOrder() and updateTableHeaders().
    // A corrupted saved order used to make this throw in updateHeader() (#24808).
    table.setup(DIV_ID, HEADER_DIV_ID, null);
    return table;
};

const getColumnNames = (table) => table.columns.map(column => column.name);

test("Test loadColumnsOrder() without a saved order", () => {
    stubBrowserApis();
    const table = createTable(null);

    expect(getColumnNames(table)).toStrictEqual(DEFAULT_COLUMN_NAMES);
});

test("Test loadColumnsOrder() with a valid saved order", () => {
    stubBrowserApis();
    const table = createTable("enabled,fullName,url,version");

    expect(getColumnNames(table)).toStrictEqual(["enabled", "fullName", "url", "version"]);
    // the named properties must keep pointing at the reordered entries
    expect(table.columns["enabled"]).toBe(table.columns[0]);
    expect(table.columns["version"]).toBe(table.columns[3]);
});

test("Test loadColumnsOrder() with a stale saved order", () => {
    stubBrowserApis();
    // "removed" no longer exists and "fullName" is duplicated
    const table = createTable("removed,fullName,fullName,version");

    expect(getColumnNames(table)).toStrictEqual(["fullName", "version", "url", "enabled"]);
});

test("Test loadColumnsOrder() with a corrupted saved order", () => {
    stubBrowserApis();
    // Array indices and prototype properties pass an `in` check on the columns
    // array but are not column names. They must be ignored instead of producing
    // undefined entries that crash updateHeader().
    const table = createTable("0,length,map,fullName,,version");

    expect(getColumnNames(table)).toStrictEqual(DEFAULT_COLUMN_NAMES);
    for (const column of table.columns)
        expect(column).toBeDefined();

    // saving must normalize the corrupted value
    table.saveColumnsOrder();
    expect(localStorage.getItem(ORDER_KEY)).toBe("fullName,version,url,enabled");
});
