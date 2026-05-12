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

export const resetDynamicTableTestState = () => {
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

export const setClientData = (key, value) => {
    clientData.set(key, value);
};

export const createTableFixture = (TableClass, id = "dynamicTable") => {
    const headerId = `${id}FixedHeader`;
    document.body.insertAdjacentHTML("beforeend", `
        <div id="${headerId}"><table><thead><tr></tr></thead></table></div>
        <div id="${id}"><table><tbody></tbody></table></div>
    `);

    const table = new TableClass();
    table.setup(id, headerId);
    return table;
};

export const flushAnimationFrames = () => {
    const callbacks = animationFrameCallbacks;
    animationFrameCallbacks = [];
    for (const callback of callbacks)
        callback();
};

export const DynamicTable = window.qBittorrent.DynamicTable;
