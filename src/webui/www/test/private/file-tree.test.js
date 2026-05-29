/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026 Andy Ye <35905412+TurboTheTurtle@users.noreply.github.com>
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

"use strict";

import { expect, test } from "vitest";

window.qBittorrent ??= {};
window.qBittorrent.LocalPreferences = {
    LocalPreferences: class {
        get(key, defaultValue) {
            return defaultValue;
        }
    }
};
window.qBittorrent.ClientData = {
    get: () => false
};
window.qBittorrent.Misc = {
    filterInPlace: (array, predicate) => {
        let writeIndex = 0;
        for (const value of array) {
            if (predicate(value)) {
                array[writeIndex] = value;
                ++writeIndex;
            }
        }
        array.length = writeIndex;
    },
    formatDate: () => "",
    friendlyPercentage: () => "",
    friendlyUnit: () => "",
    toFixedPointString: () => ""
};
window.qBittorrent.Filesystem = {
    PathSeparator: "/"
};
window.qBittorrent.TorrentContent = {
    createDownloadCheckbox: () => document.createElement("input"),
    createPriorityCombo: () => document.createElement("select"),
    updateDownloadCheckbox: () => {},
    updatePriorityCombo: () => {}
};

await import("../../private/scripts/file-tree.js");
await import("../../private/scripts/dynamicTable.js");

const createRoot = (rowIds) => {
    const root = new window.qBittorrent.FileTree.FolderNode();

    for (const rowId of rowIds) {
        const file = new window.qBittorrent.FileTree.FileNode();
        file.name = `file${rowId}.txt`;
        file.path = file.name;
        file.rowId = rowId;
        file.fileId = rowId;
        file.root = root;
        root.addChild(file);
    }

    return root;
};

const createFolder = ({ name, path, rowId, parent }) => {
    const folder = new window.qBittorrent.FileTree.FolderNode();
    folder.name = name;
    folder.path = path;
    folder.rowId = rowId;
    folder.root = parent;
    parent.addChild(folder);
    return folder;
};

const createFile = ({ name, path, rowId, fileId, parent }) => {
    const file = new window.qBittorrent.FileTree.FileNode();
    file.name = name;
    file.path = path;
    file.rowId = rowId;
    file.fileId = fileId;
    file.root = parent;
    parent.addChild(file);
    return file;
};

const createRootedFolderTree = () => {
    const root = new window.qBittorrent.FileTree.FolderNode();
    const rootFolder = createFolder({ name: "Root", path: "Root", rowId: 0, parent: root });
    const folderA = createFolder({ name: "a", path: "Root/a", rowId: 1, parent: rootFolder });
    const nested = createFolder({ name: "nested", path: "Root/a/nested", rowId: 2, parent: folderA });
    createFile({ name: "alpha.txt", path: "Root/a/nested/alpha.txt", rowId: 3, fileId: 0, parent: nested });
    return root;
};

const createUnrootedFolderTree = () => {
    const root = new window.qBittorrent.FileTree.FolderNode();
    const folderA = createFolder({ name: "a", path: "a", rowId: 0, parent: root });
    const nested = createFolder({ name: "nested", path: "a/nested", rowId: 1, parent: folderA });
    createFile({ name: "alpha.txt", path: "a/nested/alpha.txt", rowId: 2, fileId: 0, parent: nested });
    return root;
};

test("FileTree.setRoot() clears stale node mappings", () => {
    const fileTree = new window.qBittorrent.FileTree.FileTree();

    fileTree.setRoot(createRoot([0, 1]));
    expect(fileTree.getNode(1)).not.toBeNull();

    fileTree.setRoot(createRoot([0]));
    expect(fileTree.getNode(0)).not.toBeNull();
    expect(fileTree.getNode(1)).toBeNull();
});

test("TorrentFilesTable.populateTable() replaces stale row data", () => {
    const table = new window.qBittorrent.DynamicTable.TorrentFilesTable();
    table.rows = new Map();

    table.populateTable(createRoot([0, 1]));
    expect([...table.getRowIds()]).toStrictEqual(["0", "1"]);

    table.populateTable(createRoot([0]));
    expect([...table.getRowIds()]).toStrictEqual(["0"]);
});

test("TorrentFilesTable collapse state uses normalized folder paths", () => {
    const table = new window.qBittorrent.DynamicTable.TorrentFilesTable();
    table.rows = new Map();

    table.populateTable(createRootedFolderTree());
    table.collapseNode(1);

    const collapseState = table.getFolderCollapseState("Root");
    expect(collapseState.has("")).toBe(false);
    expect(collapseState.get("a")).toBe(true);
    expect(collapseState.get("a/nested")).toBe(false);

    table.clearCollapseState();
    table.populateTable(createUnrootedFolderTree());
    table.restoreFolderCollapseState(collapseState);

    expect(table.isCollapsed(0)).toBe(true);
    expect(table.isCollapsed(1)).toBe(false);
});

test("TorrentFilesTable does not transfer root folder collapse state to first child", () => {
    const table = new window.qBittorrent.DynamicTable.TorrentFilesTable();
    table.rows = new Map();

    table.populateTable(createRootedFolderTree());
    table.collapseNode(0);

    const collapseState = table.getFolderCollapseState("Root");
    expect(collapseState.get("a")).toBe(false);

    table.clearCollapseState();
    table.populateTable(createUnrootedFolderTree());
    table.restoreFolderCollapseState(collapseState);

    expect(table.isCollapsed(0)).toBe(false);
});
