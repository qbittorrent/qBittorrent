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
            normalizePriority: normalizePriority,
            isDownloadCheckboxExists: isDownloadCheckboxExists,
            createDownloadCheckbox: createDownloadCheckbox,
            updateDownloadCheckbox: updateDownloadCheckbox,
            isPriorityComboExists: isPriorityComboExists,
            createPriorityCombo: createPriorityCombo,
            updatePriorityCombo: updatePriorityCombo,
            updateData: updateData,
            collapseIconClicked: collapseIconClicked,
            expandFolder: expandFolder,
            collapseFolder: collapseFolder,
            clear: clear
        };
    };

    const torrentFilesTable = new window.qBittorrent.DynamicTable.TorrentFilesTable();
    const FilePriority = window.qBittorrent.FileTree.FilePriority;
    const TriState = window.qBittorrent.FileTree.TriState;
    let is_seed = true;
    let current_hash = "";

    const normalizePriority = (priority) => {
        switch (priority) {
            case FilePriority.Ignored:
            case FilePriority.Normal:
            case FilePriority.High:
            case FilePriority.Maximum:
            case FilePriority.Mixed:
                return priority;
            default:
                return FilePriority.Normal;
        }
    };

    const getAllChildren = (id, fileId) => {
        const node = torrentFilesTable.getNode(id);
        if (!node.isFolder) {
            return {
                rowIds: [id],
                fileIds: [fileId]
            };
        }

        const rowIds = [];
        const fileIds = [];

        const getChildFiles = (node) => {
            if (node.isFolder) {
                node.children.each((child) => {
                    getChildFiles(child);
                });
            }
            else {
                rowIds.push(node.data.rowId);
                fileIds.push(node.data.fileId);
            }
        };

        node.children.each((child) => {
            getChildFiles(child);
        });

        return {
            rowIds: rowIds,
            fileIds: fileIds
        };
    };

    const fileCheckboxClicked = (e) => {
        e.stopPropagation();

        const checkbox = e.target;
        const priority = checkbox.checked ? FilePriority.Normal : FilePriority.Ignored;
        const id = checkbox.getAttribute("data-id");
        const fileId = checkbox.getAttribute("data-file-id");

        const rows = getAllChildren(id, fileId);

        setFilePriority(rows.rowIds, rows.fileIds, priority);
        updateGlobalCheckbox();
    };

    const fileComboboxChanged = (e) => {
        const combobox = e.target;
        const priority = combobox.value;
        const id = combobox.getAttribute("data-id");
        const fileId = combobox.getAttribute("data-file-id");

        const rows = getAllChildren(id, fileId);

        setFilePriority(rows.rowIds, rows.fileIds, priority);
        updateGlobalCheckbox();
    };

    const isDownloadCheckboxExists = (id) => {
        return $(`cbPrio${id}`) !== null;
    };

    const createDownloadCheckbox = (id, fileId, checked) => {
        const checkbox = document.createElement("input");
        checkbox.type = "checkbox";
        checkbox.id = `cbPrio${id}`;
        checkbox.setAttribute("data-id", id);
        checkbox.setAttribute("data-file-id", fileId);
        checkbox.className = "DownloadedCB";
        checkbox.addEventListener("click", fileCheckboxClicked);

        updateCheckbox(checkbox, checked);
        return checkbox;
    };

    const updateDownloadCheckbox = (id, checked) => {
        const checkbox = $(`cbPrio${id}`);
        updateCheckbox(checkbox, checked);
    };

    const updateCheckbox = (checkbox, checked) => {
        switch (checked) {
            case TriState.Checked:
                setCheckboxChecked(checkbox);
                break;
            case TriState.Unchecked:
                setCheckboxUnchecked(checkbox);
                break;
            case TriState.Partial:
                setCheckboxPartial(checkbox);
                break;
        }
    };

    const isPriorityComboExists = (id) => {
        return $(`comboPrio${id}`) !== null;
    };

    const createPriorityCombo = (id, fileId, selectedPriority) => {
        const createOption = (priority, isSelected, text) => {
            const option = document.createElement("option");
            option.value = priority.toString();
            option.selected = isSelected;
            option.textContent = text;
            return option;
        };

        const select = document.createElement("select");
        select.id = `comboPrio${id}`;
        select.setAttribute("data-id", id);
        select.setAttribute("data-file-id", fileId);
        select.classList.add("combo_priority");
        select.addEventListener("change", fileComboboxChanged);

        select.appendChild(createOption(FilePriority.Ignored, (FilePriority.Ignored === selectedPriority), "QBT_TR(Do not download)QBT_TR[CONTEXT=PropListDelegate]"));
        select.appendChild(createOption(FilePriority.Normal, (FilePriority.Normal === selectedPriority), "QBT_TR(Normal)QBT_TR[CONTEXT=PropListDelegate]"));
        select.appendChild(createOption(FilePriority.High, (FilePriority.High === selectedPriority), "QBT_TR(High)QBT_TR[CONTEXT=PropListDelegate]"));
        select.appendChild(createOption(FilePriority.Maximum, (FilePriority.Maximum === selectedPriority), "QBT_TR(Maximum)QBT_TR[CONTEXT=PropListDelegate]"));

        // "Mixed" priority is for display only; it shouldn't be selectable
        const mixedPriorityOption = createOption(FilePriority.Mixed, (FilePriority.Mixed === selectedPriority), "QBT_TR(Mixed)QBT_TR[CONTEXT=PropListDelegate]");
        mixedPriorityOption.disabled = true;
        select.appendChild(mixedPriorityOption);

        return select;
    };

    const updatePriorityCombo = (id, selectedPriority) => {
        const combobox = $(`comboPrio${id}`);
        if (Number(combobox.value) !== selectedPriority)
            selectComboboxPriority(combobox, selectedPriority);
    };

    const selectComboboxPriority = (combobox, priority) => {
        const options = combobox.options;
        for (let i = 0; i < options.length; ++i) {
            const option = options[i];
            if (Number(option.value) === priority)
                option.selected = true;
            else
                option.selected = false;
        }

        combobox.value = priority;
    };

    const switchCheckboxState = (e) => {
        e.stopPropagation();

        const rowIds = [];
        const fileIds = [];
        let priority = FilePriority.Ignored;
        const checkbox = $("tristate_cb");

        if (checkbox.state === "checked") {
            setCheckboxUnchecked(checkbox);
            // set file priority for all checked to Ignored
            torrentFilesTable.getFilteredAndSortedRows().forEach((row) => {
                const rowId = row.rowId;
                const fileId = row.full_data.fileId;
                const isChecked = (row.full_data.checked === TriState.Checked);
                const isFolder = (fileId === -1);
                if (!isFolder && isChecked) {
                    rowIds.push(rowId);
                    fileIds.push(fileId);
                }
            });
        }
        else {
            setCheckboxChecked(checkbox);
            priority = FilePriority.Normal;
            // set file priority for all unchecked to Normal
            torrentFilesTable.getFilteredAndSortedRows().forEach((row) => {
                const rowId = row.rowId;
                const fileId = row.full_data.fileId;
                const isUnchecked = (row.full_data.checked === TriState.Unchecked);
                const isFolder = (fileId === -1);
                if (!isFolder && isUnchecked) {
                    rowIds.push(rowId);
                    fileIds.push(fileId);
                }
            });
        }

        if (rowIds.length > 0)
            setFilePriority(rowIds, fileIds, priority);
    };

    const updateGlobalCheckbox = () => {
        const checkbox = $("tristate_cb");
        if (isAllCheckboxesChecked())
            setCheckboxChecked(checkbox);
        else if (isAllCheckboxesUnchecked())
            setCheckboxUnchecked(checkbox);
        else
            setCheckboxPartial(checkbox);
    };

    const setCheckboxChecked = (checkbox) => {
        checkbox.state = "checked";
        checkbox.indeterminate = false;
        checkbox.checked = true;
    };

    const setCheckboxUnchecked = (checkbox) => {
        checkbox.state = "unchecked";
        checkbox.indeterminate = false;
        checkbox.checked = false;
    };

    const setCheckboxPartial = (checkbox) => {
        checkbox.state = "partial";
        checkbox.indeterminate = true;
    };

    const isAllCheckboxesChecked = () => Array.prototype.every.call(document.querySelectorAll("input.DownloadedCB"), (checkbox => checkbox.checked));

    const isAllCheckboxesUnchecked = () => Array.prototype.every.call(document.querySelectorAll("input.DownloadedCB"), (checkbox => !checkbox.checked));

    const setFilePriority = (ids, fileIds, priority) => {
        if (current_hash === "")
            return;

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

        const ignore = (priority === FilePriority.Ignored);
        ids.forEach((id) => {
            torrentFilesTable.setIgnored(id, ignore);

            const combobox = $(`comboPrio${id}`);
            if (combobox !== null)
                selectComboboxPriority(combobox, priority);
        });

        torrentFilesTable.updateTable(false);
    };

    let loadTorrentFilesDataTimer = -1;
    const loadTorrentFilesData = () => {
        if ($("propFiles").classList.contains("invisible")
            || $("propertiesPanel_collapseToggle").classList.contains("panel-expand")) {
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

                clearTimeout(torrentFilesFilterInputTimer);
                torrentFilesFilterInputTimer = -1;

                if (files.length === 0) {
                    torrentFilesTable.clear();
                }
                else {
                    handleNewTorrentFiles(files);
                    if (loadedNewTorrent)
                        collapseAllNodes();
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

    const collapseIconClicked = (event) => {
        const id = event.getAttribute("data-id");
        const node = torrentFilesTable.getNode(id);
        const isCollapsed = (event.parentElement.getAttribute("data-collapsed") === "true");

        if (isCollapsed)
            expandNode(node);
        else
            collapseNode(node);
    };

    const expandFolder = (id) => {
        const node = torrentFilesTable.getNode(id);
        if (node.isFolder)
            expandNode(node);
    };

    const collapseFolder = (id) => {
        const node = torrentFilesTable.getNode(id);
        if (node.isFolder)
            collapseNode(node);
    };

    const filesPriorityMenuClicked = (priority) => {
        const selectedRows = torrentFilesTable.selectedRowsIds();
        if (selectedRows.length === 0)
            return;

        const rowIds = [];
        const fileIds = [];
        selectedRows.forEach((rowId) => {
            const elem = $(`comboPrio${rowId}`);
            rowIds.push(rowId);
            fileIds.push(elem.getAttribute("data-file-id"));
        });

        const uniqueRowIds = {};
        const uniqueFileIds = {};
        for (let i = 0; i < rowIds.length; ++i) {
            const rows = getAllChildren(rowIds[i], fileIds[i]);
            rows.rowIds.forEach((rowId) => {
                uniqueRowIds[rowId] = true;
            });
            rows.fileIds.forEach((fileId) => {
                uniqueFileIds[fileId] = true;
            });
        }

        setFilePriority(Object.keys(uniqueRowIds), Object.keys(uniqueFileIds), priority);
    };

    const singleFileRename = (hash) => {
        const rowId = torrentFilesTable.selectedRowsIds()[0];
        if (rowId === undefined)
            return;
        const row = torrentFilesTable.rows.get(rowId);
        if (!row)
            return;

        const node = torrentFilesTable.getNode(rowId);
        const path = node.path;

        new MochaUI.Window({
            id: "renamePage",
            icon: "images/qbittorrent-tray.svg",
            title: "QBT_TR(Renaming)QBT_TR[CONTEXT=TorrentContentTreeView]",
            loadMethod: "iframe",
            contentURL: `rename_file.html?hash=${hash}&isFolder=${node.isFolder}&path=${encodeURIComponent(path)}`,
            scrollbars: false,
            resizable: true,
            maximizable: false,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: 400,
            height: 100
        });
    };

    const multiFileRename = (hash) => {
        new MochaUI.Window({
            id: "multiRenamePage",
            icon: "images/qbittorrent-tray.svg",
            title: "QBT_TR(Renaming)QBT_TR[CONTEXT=TorrentContentTreeView]",
            data: { hash: hash, selectedRows: torrentFilesTable.selectedRows },
            loadMethod: "xhr",
            contentURL: "rename_files.html",
            scrollbars: false,
            resizable: true,
            maximizable: false,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: 800,
            height: 420,
            resizeLimit: { x: [800], y: [420] }
        });
    };

    const torrentFilesContextMenu = new window.qBittorrent.ContextMenu.ContextMenu({
        targets: "#torrentFilesTableDiv tr",
        menu: "torrentFilesMenu",
        actions: {
            Rename: (element, ref) => {
                const hash = torrentsTable.getCurrentTorrentID();
                if (!hash)
                    return;

                if (torrentFilesTable.selectedRowsIds().length > 1)
                    multiFileRename(hash);
                else
                    singleFileRename(hash);
            },

            FilePrioIgnore: (element, ref) => {
                filesPriorityMenuClicked(FilePriority.Ignored);
            },
            FilePrioNormal: (element, ref) => {
                filesPriorityMenuClicked(FilePriority.Normal);
            },
            FilePrioHigh: (element, ref) => {
                filesPriorityMenuClicked(FilePriority.High);
            },
            FilePrioMaximum: (element, ref) => {
                filesPriorityMenuClicked(FilePriority.Maximum);
            }
        },
        offsets: {
            x: 0,
            y: 2
        },
        onShow: function() {
            if (is_seed)
                this.hideItem("FilePrio");
            else
                this.showItem("FilePrio");
        }
    });

    torrentFilesTable.setup("torrentFilesTableDiv", "torrentFilesTableFixedHeaderDiv", torrentFilesContextMenu);
    // inject checkbox into table header
    const tableHeaders = document.querySelectorAll("#torrentFilesTableFixedHeaderDiv .dynamicTableHeader th");
    if (tableHeaders.length > 0) {
        const checkbox = document.createElement("input");
        checkbox.type = "checkbox";
        checkbox.id = "tristate_cb";
        checkbox.addEventListener("click", switchCheckboxState);

        const checkboxTH = tableHeaders[0];
        checkboxTH.append(checkbox);
    }

    // default sort by name column
    if (torrentFilesTable.getSortedColumn() === null)
        torrentFilesTable.setSortedColumn("name");

    // listen for changes to torrentFilesFilterInput
    let torrentFilesFilterInputTimer = -1;
    $("torrentFilesFilterInput").addEventListener("input", () => {
        clearTimeout(torrentFilesFilterInputTimer);

        const value = $("torrentFilesFilterInput").value;
        torrentFilesTable.setFilter(value);

        torrentFilesFilterInputTimer = setTimeout(() => {
            torrentFilesFilterInputTimer = -1;

            if (current_hash === "")
                return;

            torrentFilesTable.updateTable();

            if (value.trim() === "")
                collapseAllNodes();
            else
                expandAllNodes();
        }, window.qBittorrent.Misc.FILTER_INPUT_DELAY);
    });

    /**
     * Show/hide a node's row
     */
    const _hideNode = (node, shouldHide) => {
        const span = $(`filesTablefileName${node.rowId}`);
        // span won't exist if row has been filtered out
        if (span === null)
            return;
        const rowElem = span.parentElement.parentElement;
        rowElem.classList.toggle("invisible", shouldHide);
    };

    /**
     * Update a node's collapsed state and icon
     */
    const _updateNodeState = (node, isCollapsed) => {
        const span = $(`filesTablefileName${node.rowId}`);
        // span won't exist if row has been filtered out
        if (span === null)
            return;
        const td = span.parentElement;

        // store collapsed state
        td.setAttribute("data-collapsed", isCollapsed);

        // rotate the collapse icon
        const collapseIcon = td.getElementsByClassName("filesTableCollapseIcon")[0];
        collapseIcon.classList.toggle("rotate", isCollapsed);
    };

    const _isCollapsed = (node) => {
        const span = $(`filesTablefileName${node.rowId}`);
        if (span === null)
            return true;

        const td = span.parentElement;
        return td.getAttribute("data-collapsed") === "true";
    };

    const expandNode = (node) => {
        _collapseNode(node, false, false, false);
    };

    const collapseNode = (node) => {
        _collapseNode(node, true, false, false);
    };

    const expandAllNodes = () => {
        const root = torrentFilesTable.getRoot();
        root.children.each((node) => {
            node.children.each((child) => {
                _collapseNode(child, false, true, false);
            });
        });
    };

    const collapseAllNodes = () => {
        const root = torrentFilesTable.getRoot();
        root.children.each((node) => {
            node.children.each((child) => {
                _collapseNode(child, true, true, false);
            });
        });
    };

    /**
     * Collapses a folder node with the option to recursively collapse all children
     * @param {FolderNode} node the node to collapse/expand
     * @param {boolean} shouldCollapse true if the node should be collapsed, false if it should be expanded
     * @param {boolean} applyToChildren true if the node's children should also be collapsed, recursively
     * @param {boolean} isChildNode true if the current node is a child of the original node we collapsed/expanded
     */
    const _collapseNode = (node, shouldCollapse, applyToChildren, isChildNode) => {
        if (!node.isFolder)
            return;

        const shouldExpand = !shouldCollapse;
        const isNodeCollapsed = _isCollapsed(node);
        const nodeInCorrectState = ((shouldCollapse && isNodeCollapsed) || (shouldExpand && !isNodeCollapsed));
        const canSkipNode = (isChildNode && (!applyToChildren || nodeInCorrectState));
        if (!isChildNode || applyToChildren || !canSkipNode)
            _updateNodeState(node, shouldCollapse);

        node.children.each((child) => {
            _hideNode(child, shouldCollapse);

            if (!child.isFolder)
                return;

            // don't expand children that have been independently collapsed, unless applyToChildren is true
            const shouldExpandChildren = (shouldExpand && applyToChildren);
            const isChildCollapsed = _isCollapsed(child);
            if (!shouldExpandChildren && isChildCollapsed)
                return;

            _collapseNode(child, shouldCollapse, applyToChildren, true);
        });
    };

    const clear = () => {
        torrentFilesTable.clear();
    };

    return exports();
})();
Object.freeze(window.qBittorrent.PropFiles);
