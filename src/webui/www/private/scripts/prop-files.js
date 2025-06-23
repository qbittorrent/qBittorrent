/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2009  Christophe Dumez <chris@qbittorrent.org>
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

window.qBittorrent ??= {};
window.qBittorrent.PropFiles ??= (() => {
    const exports = () => {
        return {
            updateData: updateData,
            clear: clear
        };
    };

    let current_hash = "";
    const BY_SHOWN_ORDER = -2;

    const onFilePriorityChanged = (fileIds, priority) => {
        // ignore folders
        fileIds = fileIds.map(Number).filter(id => !window.qBittorrent.TorrentContent.isFolder(id));

        clearTimeout(loadTorrentFilesDataTimer);
        loadTorrentFilesDataTimer = -1;

        fetch("api/v2/torrents/filePrio", {
                method: "POST",
                body: new URLSearchParams({
                    hash: current_hash,
                    id: fileIds.join("|"),
                    priority: priority
                })
            })
            .then((response) => {
                if (!response.ok)
                    return;

                loadTorrentFilesDataTimer = loadTorrentFilesData.delay(1000);
            });
    };

    let loadTorrentFilesDataTimer = -1;
    const loadTorrentFilesData = () => {
        if (document.hidden)
            return;
        if (document.getElementById("propFiles").classList.contains("invisible")
            || document.getElementById("propertiesPanel_collapseToggle").classList.contains("panel-expand")) {
            // Tab changed, don't do anything
            return;
        }
        const new_hash = torrentsTable.getCurrentTorrentID();
        if (new_hash === "") {
            torrentFilesTable.clear();
            clearTimeout(loadTorrentFilesDataTimer);
            return;
        }
        let loadedNewTorrent = false;
        if (new_hash !== current_hash) {
            torrentFilesTable.clear();
            current_hash = new_hash;
            loadedNewTorrent = true;
        }

        const url = new URL("api/v2/torrents/files", window.location);
        url.search = new URLSearchParams({
            hash: current_hash
        });
        fetch(url, {
                method: "GET",
                cache: "no-store"
            })
            .then(async (response) => {
                if (!response.ok)
                    return;

                const files = await response.json();

                window.qBittorrent.TorrentContent.clearFilterInputTimer();

                if (files.length === 0) {
                    torrentFilesTable.clear();
                }
                else {
                    window.qBittorrent.TorrentContent.updateData(files);
                    if (loadedNewTorrent)
                        torrentFilesTable.collapseAllNodes();
                }
            })
            .finally(() => {
                clearTimeout(loadTorrentFilesDataTimer);
                loadTorrentFilesDataTimer = loadTorrentFilesData.delay(5000);
            });
    };

    const updateData = () => {
        clearTimeout(loadTorrentFilesDataTimer);
        loadTorrentFilesDataTimer = -1;
        loadTorrentFilesData();
    };

<<<<<<< HEAD
    const singleFileRename = (hash, node) => {
=======
    const handleNewTorrentFiles = (files) => {
        is_seed = (files.length > 0) ? files[0].is_seed : true;

        const rows = files.map((file, index) => {
            const ignore = (file.priority === FilePriority.Ignored);
            const row = {
                fileId: index,
                checked: (ignore ? TriState.Unchecked : TriState.Checked),
                fileName: file.name,
                name: window.qBittorrent.Filesystem.fileName(file.name),
                size: file.size,
                progress: window.qBittorrent.Misc.toFixedPointString((file.progress * 100), 1),
                priority: normalizePriority(file.priority),
                remaining: (ignore ? 0 : (file.size * (1 - file.progress))),
                availability: file.availability
            };
            return row;
        });

        addRowsToTable(rows);
        updateGlobalCheckbox();
    };

    const addRowsToTable = (rows) => {
        const selectedFiles = torrentFilesTable.selectedRowsIds();
        let rowId = 0;

        const rootNode = new window.qBittorrent.FileTree.FolderNode();

        rows.forEach((row) => {
            const pathItems = row.fileName.split(window.qBittorrent.Filesystem.PathSeparator);

            pathItems.pop(); // remove last item (i.e. file name)
            let parent = rootNode;
            pathItems.forEach((folderName) => {
                if (folderName === ".unwanted")
                    return;

                let folderNode = null;
                if (parent.children !== null) {
                    for (let i = 0; i < parent.children.length; ++i) {
                        const childFolder = parent.children[i];
                        if (childFolder.name === folderName) {
                            folderNode = childFolder;
                            break;
                        }
                    }
                }

                if (folderNode === null) {
                    folderNode = new window.qBittorrent.FileTree.FolderNode();
                    folderNode.path = (parent.path === "")
                        ? folderName
                        : [parent.path, folderName].join(window.qBittorrent.Filesystem.PathSeparator);
                    folderNode.name = folderName;
                    folderNode.rowId = rowId;
                    folderNode.root = parent;
                    parent.addChild(folderNode);

                    ++rowId;
                }

                parent = folderNode;
            });

            const isChecked = row.checked ? TriState.Checked : TriState.Unchecked;
            const remaining = (row.priority === FilePriority.Ignored) ? 0 : row.remaining;
            const childNode = new window.qBittorrent.FileTree.FileNode();
            childNode.name = row.name;
            childNode.path = row.fileName;
            childNode.rowId = rowId;
            childNode.size = row.size;
            childNode.checked = isChecked;
            childNode.remaining = remaining;
            childNode.progress = row.progress;
            childNode.priority = row.priority;
            childNode.availability = row.availability;
            childNode.root = parent;
            childNode.data = row;
            parent.addChild(childNode);

            ++rowId;
        });

        torrentFilesTable.populateTable(rootNode);
        torrentFilesTable.updateTable(false);

        if (selectedFiles.length > 0)
            torrentFilesTable.reselectRows(selectedFiles);
    };

    const filesPriorityMenuClicked = (priority) => {
        const selectedRows = torrentFilesTable.selectedRowsIds();
        if (selectedRows.length === 0)
            return;

        const fileIds = [];
        const fillInChildren = (rowId, fileId) => {
            const children = getAllChildren(rowId, fileId);
            children.fileIds.forEach((childFileId, index) => {
                if (fileId === -1) {
                    fillInChildren(children.rowIds[index], childFileId);
                    return;
                }
                if (!fileIds.includes(childFileId))
                    fileIds.push(childFileId);
            });
        };

        selectedRows.forEach((rowId) => {
            const fileId = torrentFilesTable.getRowFileId(rowId);
            if (fileId !== -1)
                fileIds.push(fileId);
            fillInChildren(rowId, fileId);
        });

        // Will be used both in general case and ByShownOrder case
        const priorityList = {};
        Object.values(FilePriority).forEach((p) => {
            priorityList[p] = [];
        });
        const groupsNum = 3;
        const priorityGroupSize = Math.max(Math.floor(fileIds.length / groupsNum), 1);

        // Will be altered only if we select ByShownOrder, use whatever is passed otherwise
        let priorityGroup = priority;
        let fileId = 0;
        for (let i = 0; i < fileIds.length; ++i) {
            fileId = fileIds[i];
            if (priority === BY_SHOWN_ORDER) {
                switch (Math.floor(i / priorityGroupSize)) {
                    case 0:
                        priorityGroup = FilePriority.Maximum;
                        break;
                    case 1:
                        priorityGroup = FilePriority.High;
                        break;
                    default:
                    case 2:
                        priorityGroup = FilePriority.Normal;
                        break;
                }
            }
            priorityList[priorityGroup].push(fileId);
        }

        Object.entries(priorityList).forEach(([groupPriority, groupedFiles]) => {
            if (groupedFiles.length === 0)
                return;
            setFilePriority(
                groupedFiles,
                groupPriority
            );
        });
    };

    const singleFileRename = (hash) => {
        const rowId = torrentFilesTable.selectedRowsIds()[0];
        if (rowId === undefined)
            return;
        const row = torrentFilesTable.rows.get(rowId);
        if (!row)
            return;

        const node = torrentFilesTable.getNode(rowId);
>>>>>>> 35b94986b (refact: Improve JS priority change logic)
        const path = node.path;

        new MochaUI.Window({
            id: "renamePage",
            icon: "images/qbittorrent-tray.svg",
            title: "QBT_TR(Renaming)QBT_TR[CONTEXT=TorrentContentTreeView]",
            loadMethod: "iframe",
            contentURL: `rename_file.html?v=${CACHEID}&hash=${hash}&isFolder=${node.isFolder}&path=${encodeURIComponent(path)}`,
            scrollbars: false,
            resizable: true,
            maximizable: false,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: window.qBittorrent.Dialog.limitWidthToViewport(400),
            height: 100,
            onCloseComplete: () => {
                updateData();
            }
        });
    };

    const multiFileRename = (hash, selectedRows) => {
        new MochaUI.Window({
            id: "multiRenamePage",
            icon: "images/qbittorrent-tray.svg",
            title: "QBT_TR(Renaming)QBT_TR[CONTEXT=TorrentContentTreeView]",
            data: { hash: hash, selectedRows: selectedRows },
            loadMethod: "xhr",
            contentURL: "rename_files.html?v=${CACHEID}",
            scrollbars: false,
            resizable: true,
            maximizable: false,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: 800,
            height: 420,
            resizeLimit: { x: [800], y: [420] },
            onCloseComplete: () => {
                updateData();
            }
        });
    };

    const onFileRenameHandler = (selectedRows, selectedNodes) => {
        if (selectedNodes.length === 1)
            singleFileRename(current_hash, selectedNodes[0]);
        else if (selectedNodes.length > 1)
            multiFileRename(current_hash, selectedRows);
    };

    const torrentFilesTable = window.qBittorrent.TorrentContent.init("torrentFilesTableDiv", window.qBittorrent.DynamicTable.TorrentFilesTable, onFilePriorityChanged, onFileRenameHandler);

    const clear = () => {
        torrentFilesTable.clear();
    };

    return exports();
})();
Object.freeze(window.qBittorrent.PropFiles);
