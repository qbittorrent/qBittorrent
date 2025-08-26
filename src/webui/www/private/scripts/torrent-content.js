/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2025 Thomas Piccirello <thomas@piccirello.com>
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
window.qBittorrent.TorrentContent ??= (() => {
    const exports = () => {
        return {
            init: init,
            normalizePriority: normalizePriority,
            isFolder: isFolder,
            createDownloadCheckbox: createDownloadCheckbox,
            updateDownloadCheckbox: updateDownloadCheckbox,
            createPriorityCombo: createPriorityCombo,
            updatePriorityCombo: updatePriorityCombo,
            updateData: updateData,
            clearFilterInputTimer: clearFilterInputTimer
        };
    };

    let torrentFilesTable;
    const FilePriority = window.qBittorrent.FileTree.FilePriority;
    const TriState = window.qBittorrent.FileTree.TriState;
    let torrentFilesFilterInputTimer = -1;
    let onFilePriorityChanged = null;

    const normalizePriority = (priority) => {
        priority = Number(priority);

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

    const triStateFromPriority = (priority) => {
        switch (normalizePriority(priority)) {
            case FilePriority.Ignored:
                return TriState.Unchecked;
            case FilePriority.Normal:
            case FilePriority.High:
            case FilePriority.Maximum:
                return TriState.Checked;
            case FilePriority.Mixed:
                return TriState.Partial;
        }
    };

    const isFolder = (fileId) => {
        return fileId === -1;
    };

    const getAllChildren = (id, fileId) => {
        const node = torrentFilesTable.getNode(id);
        const rowIds = [node.rowId];
        const fileIds = [node.fileId];

        const getChildFiles = (node) => {
            rowIds.push(node.rowId);
            fileIds.push(node.fileId);

            if (node.isFolder) {
                for (const child of node.children)
                    getChildFiles(child);
            }
        };

        for (const child of node.children)
            getChildFiles(child);

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
        const fileId = Number(checkbox.getAttribute("data-file-id"));

        const rows = getAllChildren(id, fileId);

        setFilePriority(rows.rowIds, rows.fileIds, priority);
        updateParentFolder(id);
    };

    const fileComboboxChanged = (e) => {
        const combobox = e.target;
        const priority = combobox.value;
        const id = combobox.getAttribute("data-id");
        const fileId = Number(combobox.getAttribute("data-file-id"));

        const rows = getAllChildren(id, fileId);

        setFilePriority(rows.rowIds, rows.fileIds, priority);
        updateParentFolder(id);
    };

    const createDownloadCheckbox = (id, fileId, checked) => {
        const checkbox = document.createElement("input");
        checkbox.type = "checkbox";
        checkbox.setAttribute("data-id", id);
        checkbox.setAttribute("data-file-id", fileId);
        checkbox.addEventListener("click", fileCheckboxClicked);

        updateCheckbox(checkbox, checked);
        return checkbox;
    };

    const updateDownloadCheckbox = (checkbox, id, fileId, checked) => {
        checkbox.setAttribute("data-id", id);
        checkbox.setAttribute("data-file-id", fileId);
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

    const createPriorityCombo = (id, fileId, selectedPriority) => {
        const createOption = (priority, isSelected, text) => {
            const option = document.createElement("option");
            option.value = priority.toString();
            option.selected = isSelected;
            option.textContent = text;
            return option;
        };

        const select = document.createElement("select");
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

    const updatePriorityCombo = (combobox, id, fileId, selectedPriority) => {
        combobox.setAttribute("data-id", id);
        combobox.setAttribute("data-file-id", fileId);
        if (normalizePriority(combobox.value) !== selectedPriority)
            selectComboboxPriority(combobox, normalizePriority(selectedPriority));
    };

    const selectComboboxPriority = (combobox, priority) => {
        const options = combobox.options;
        for (const option of options) {
            if (normalizePriority(option.value) === priority)
                option.selected = true;
            else
                option.selected = false;
        }

        combobox.value = priority;
    };

    const getComboboxPriority = (id) => {
        const node = torrentFilesTable.getNode(id.toString());
        return normalizePriority(node.priority, 10);
    };

    const switchGlobalCheckboxState = (e) => {
        e.stopPropagation();

        const rowIds = [];
        const fileIds = [];
        const checkbox = document.getElementById("tristate_cb");
        const priority = (checkbox.state === TriState.Checked) ? FilePriority.Ignored : FilePriority.Normal;

        if (checkbox.state === TriState.Checked) {
            setCheckboxUnchecked(checkbox);
            for (const row of torrentFilesTable.rows) {
                const rowId = row.rowId;
                const node = torrentFilesTable.getNode(rowId);
                const fileId = node.fileId;
                const isChecked = (node.checked === TriState.Checked);
                if (isChecked) {
                    rowIds.push(rowId);
                    fileIds.push(fileId);
                }
            }
        }
        else {
            setCheckboxChecked(checkbox);
            for (const row of torrentFilesTable.rows) {
                const rowId = row.rowId;
                const node = torrentFilesTable.getNode(rowId);
                const fileId = node.fileId;
                const isUnchecked = (node.checked === TriState.Unchecked);
                if (isUnchecked) {
                    rowIds.push(rowId);
                    fileIds.push(fileId);
                }
            }
        }

        if (rowIds.length > 0) {
            setFilePriority(rowIds, fileIds, priority);
            for (const id of rowIds)
                updateParentFolder(id);
        }
    };

    const updateGlobalCheckbox = () => {
        const checkbox = document.getElementById("tristate_cb");
        if (torrentFilesTable.isAllCheckboxesChecked())
            setCheckboxChecked(checkbox);
        else if (torrentFilesTable.isAllCheckboxesUnchecked())
            setCheckboxUnchecked(checkbox);
        else
            setCheckboxPartial(checkbox);
    };

    const setCheckboxChecked = (checkbox) => {
        checkbox.state = TriState.Checked;
        checkbox.indeterminate = false;
        checkbox.checked = true;
    };

    const setCheckboxUnchecked = (checkbox) => {
        checkbox.state = TriState.Unchecked;
        checkbox.indeterminate = false;
        checkbox.checked = false;
    };

    const setCheckboxPartial = (checkbox) => {
        checkbox.state = TriState.Partial;
        checkbox.indeterminate = true;
    };

    const setFilePriority = (ids, fileIds, priority) => {
        priority = normalizePriority(priority);

        if (onFilePriorityChanged)
            onFilePriorityChanged(fileIds, priority);

        const nodes = ids.map((id) => {
            const node = torrentFilesTable.getNode(id.toString());
            node.priority = priority;
            node.checked = triStateFromPriority(priority);
            return node;
        });

        // must update all nodes above before recalculating
        for (const node of nodes)
            node.calculateRemaining();
    };

    const updateData = (files) => {
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

        for (const row of rows) {
            const pathItems = row.fileName.split(window.qBittorrent.Filesystem.PathSeparator);

            pathItems.pop(); // remove last item (i.e. file name)
            let parent = rootNode;
            for (const folderName of pathItems) {
                if (folderName === ".unwanted")
                    continue;

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
            }

            const isChecked = row.checked ? TriState.Checked : TriState.Unchecked;
            const childNode = new window.qBittorrent.FileTree.FileNode();
            childNode.name = row.name;
            childNode.path = row.fileName;
            childNode.rowId = rowId;
            childNode.fileId = row.fileId;
            childNode.size = row.size;
            childNode.checked = isChecked;
            childNode.progress = row.progress;
            childNode.priority = row.priority;
            childNode.availability = row.availability;
            childNode.root = parent;
            parent.addChild(childNode);

            ++rowId;
        }

        torrentFilesTable.populateTable(rootNode);
        torrentFilesTable.updateTable();

        if (selectedFiles.length > 0)
            torrentFilesTable.reselectRows(selectedFiles);
    };

    const filesPriorityMenuClicked = (priority) => {
        const selectedRows = torrentFilesTable.selectedRowsIds();
        if (selectedRows.length === 0)
            return;

        const rowIds = [];
        const fileIds = [];
        for (const rowId of selectedRows) {
            rowIds.push(rowId);
            fileIds.push(Number(torrentFilesTable.getRowFileId(rowId)));
        }

        const uniqueRowIds = new Set();
        const uniqueFileIds = new Set();
        for (const [i, rowId] of rowIds.entries()) {
            const rows = getAllChildren(rowId, fileIds[i]);
            for (const rowId of rows.rowIds)
                uniqueRowIds.add(rowId);
            for (const fileId of rows.fileIds)
                uniqueFileIds.add(fileId);
        }

        setFilePriority([...uniqueRowIds.keys()], [...uniqueFileIds.keys()], priority);
        for (const id of rowIds)
            updateParentFolder(id);
    };

    const updateParentFolder = (id) => {
        const updateComplete = () => {
            // we've finished recursing
            updateGlobalCheckbox();
            torrentFilesTable.calculateRemaining();
            torrentFilesTable.updateTable(true);
        };

        const node = torrentFilesTable.getNode(id);
        const parent = node.parent;
        if (parent === torrentFilesTable.getRoot()) {
            updateComplete();
            return;
        }

        const siblings = parent.children;

        let checkedCount = 0;
        let uncheckedCount = 0;
        let indeterminateCount = 0;
        let desiredComboboxPriority = null;
        for (const sibling of siblings) {
            switch (sibling.checked) {
                case TriState.Checked:
                    ++checkedCount;
                    break;
                case TriState.Unchecked:
                    ++uncheckedCount;
                    break;
                case TriState.Partial:
                    ++indeterminateCount;
                    break;
            }

            if (desiredComboboxPriority === null)
                desiredComboboxPriority = getComboboxPriority(sibling.rowId);
            else if (desiredComboboxPriority !== getComboboxPriority(sibling.rowId))
                desiredComboboxPriority = FilePriority.Mixed;
        }

        const currentCheckboxState = parent.checked;
        let desiredCheckboxState = TriState.Unchecked;
        if ((indeterminateCount > 0) || ((checkedCount > 0) && (uncheckedCount > 0)))
            desiredCheckboxState = TriState.Partial;
        else if (checkedCount > 0)
            desiredCheckboxState = TriState.Checked;

        const currentComboboxPriority = getComboboxPriority(parent.rowId);
        if ((currentCheckboxState !== desiredCheckboxState) || (currentComboboxPriority !== desiredComboboxPriority)) {
            const node = torrentFilesTable.getNode(parent.rowId.toString());
            node.priority = desiredComboboxPriority;
            node.checked = desiredCheckboxState;

            updateParentFolder(parent.rowId);
        }
        else {
            updateComplete();
        }
    };

    const init = (tableId, tableClass, onFilePriorityChangedHandler = undefined, onFileRenameHandler = undefined) => {
        if (onFilePriorityChangedHandler !== undefined)
            onFilePriorityChanged = onFilePriorityChangedHandler;

        torrentFilesTable = new tableClass();

        const torrentFilesContextMenu = new window.qBittorrent.ContextMenu.ContextMenu({
            targets: `#${tableId} tbody tr`,
            menu: "torrentFilesMenu",
            actions: {
                Rename: (element, ref) => {
                    if (onFileRenameHandler !== undefined) {
                        const nodes = torrentFilesTable.selectedRowsIds().map(row => torrentFilesTable.getNode(row));
                        onFileRenameHandler(torrentFilesTable.selectedRows, nodes);
                    }
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
        });

        torrentFilesTable.setup(tableId, "torrentFilesTableFixedHeaderDiv", torrentFilesContextMenu, true);
        // inject checkbox into table header
        const tableHeaders = document.querySelectorAll("#torrentFilesTableFixedHeaderDiv .dynamicTableHeader th");
        if (tableHeaders.length > 0) {
            const checkbox = document.createElement("input");
            checkbox.type = "checkbox";
            checkbox.id = "tristate_cb";
            checkbox.addEventListener("click", switchGlobalCheckboxState);

            const checkboxTH = tableHeaders[0];
            checkboxTH.appendChild(checkbox);
        }

        // default sort by name column
        if (torrentFilesTable.getSortedColumn() === null)
            torrentFilesTable.setSortedColumn("name");

        // listen for changes to torrentFilesFilterInput
        document.getElementById("torrentFilesFilterInput").addEventListener("input", (event) => {
            clearTimeout(torrentFilesFilterInputTimer);
            torrentFilesFilterInputTimer = -1;

            const value = document.getElementById("torrentFilesFilterInput").value;
            torrentFilesTable.setFilter(value);

            torrentFilesFilterInputTimer = setTimeout(() => {
                torrentFilesFilterInputTimer = -1;

                torrentFilesTable.updateTable();

                if (value.trim() === "")
                    torrentFilesTable.collapseAllNodes();
                else
                    torrentFilesTable.expandAllNodes();
            }, window.qBittorrent.Misc.FILTER_INPUT_DELAY);
        });

        return torrentFilesTable;
    };

    const clearFilterInputTimer = () => {
        clearTimeout(torrentFilesFilterInputTimer);
        torrentFilesFilterInputTimer = -1;
    };

    return exports();
})();
Object.freeze(window.qBittorrent.TorrentContent);
