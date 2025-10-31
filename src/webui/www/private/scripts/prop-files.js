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

    const singleFileRename = (hash, node) => {
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
