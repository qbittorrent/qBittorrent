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

if (window.qBittorrent === undefined)
    window.qBittorrent = {};

window.qBittorrent.PropFiles = (function() {
    const exports = function() {
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
            collapseFolder: collapseFolder
        };
    };

    const torrentFilesTable = new window.qBittorrent.DynamicTable.TorrentFilesTable();
    const FilePriority = window.qBittorrent.FileTree.FilePriority;
    const TriState = window.qBittorrent.FileTree.TriState;
    let is_seed = true;
    let current_hash = "";

    const normalizePriority = function(priority) {
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

    const getAllChildren = function(id, fileId) {
        const node = torrentFilesTable.getNode(id);
        if (!node.isFolder) {
            return {
                rowIds: [id],
                fileIds: [fileId]
            };
        }

        const rowIds = [];
        const fileIds = [];

        const getChildFiles = function(node) {
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

    const fileCheckboxClicked = function(e) {
        e.stopPropagation();

        const checkbox = e.target;
        const priority = checkbox.checked ? FilePriority.Normal : FilePriority.Ignored;
        const id = checkbox.getAttribute("data-id");
        const fileId = checkbox.getAttribute("data-file-id");

        const rows = getAllChildren(id, fileId);

        setFilePriority(rows.rowIds, rows.fileIds, priority);
        updateGlobalCheckbox();
    };

    const fileComboboxChanged = function(e) {
        const combobox = e.target;
        const priority = combobox.value;
        const id = combobox.getAttribute("data-id");
        const fileId = combobox.getAttribute("data-file-id");

        const rows = getAllChildren(id, fileId);

        setFilePriority(rows.rowIds, rows.fileIds, priority);
        updateGlobalCheckbox();
    };

    const isDownloadCheckboxExists = function(id) {
        return ($("cbPrio" + id) !== null);
    };

    const createDownloadCheckbox = function(id, fileId, checked) {
        const checkbox = new Element("input");
        checkbox.type = "checkbox";
        checkbox.id = "cbPrio" + id;
        checkbox.setAttribute("data-id", id);
        checkbox.setAttribute("data-file-id", fileId);
        checkbox.className = "DownloadedCB";
        checkbox.addEvent("click", fileCheckboxClicked);

        updateCheckbox(checkbox, checked);
        return checkbox;
    };

    const updateDownloadCheckbox = function(id, checked) {
        const checkbox = $("cbPrio" + id);
        updateCheckbox(checkbox, checked);
    };

    const updateCheckbox = function(checkbox, checked) {
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

    const isPriorityComboExists = function(id) {
        return ($("comboPrio" + id) !== null);
    };

    const createPriorityOptionElement = function(priority, selected, html) {
        const elem = new Element("option");
        elem.value = priority.toString();
        elem.innerHTML = html;
        if (selected)
            elem.selected = true;
        return elem;
    };

    const createPriorityCombo = function(id, fileId, selectedPriority) {
        const select = new Element("select");
        select.id = "comboPrio" + id;
        select.setAttribute("data-id", id);
        select.setAttribute("data-file-id", fileId);
        select.addClass("combo_priority");
        select.addEvent("change", fileComboboxChanged);

        createPriorityOptionElement(FilePriority.Ignored, (FilePriority.Ignored === selectedPriority), "QBT_TR(Do not download)QBT_TR[CONTEXT=PropListDelegate]").injectInside(select);
        createPriorityOptionElement(FilePriority.Normal, (FilePriority.Normal === selectedPriority), "QBT_TR(Normal)QBT_TR[CONTEXT=PropListDelegate]").injectInside(select);
        createPriorityOptionElement(FilePriority.High, (FilePriority.High === selectedPriority), "QBT_TR(High)QBT_TR[CONTEXT=PropListDelegate]").injectInside(select);
        createPriorityOptionElement(FilePriority.Maximum, (FilePriority.Maximum === selectedPriority), "QBT_TR(Maximum)QBT_TR[CONTEXT=PropListDelegate]").injectInside(select);

        // "Mixed" priority is for display only; it shouldn't be selectable
        const mixedPriorityOption = createPriorityOptionElement(FilePriority.Mixed, (FilePriority.Mixed === selectedPriority), "QBT_TR(Mixed)QBT_TR[CONTEXT=PropListDelegate]");
        mixedPriorityOption.disabled = true;
        mixedPriorityOption.injectInside(select);

        return select;
    };

    const updatePriorityCombo = function(id, selectedPriority) {
        const combobox = $("comboPrio" + id);
        if (parseInt(combobox.value, 10) !== selectedPriority)
            selectComboboxPriority(combobox, selectedPriority);
    };

    const selectComboboxPriority = function(combobox, priority) {
        const options = combobox.options;
        for (let i = 0; i < options.length; ++i) {
            const option = options[i];
            if (parseInt(option.value, 10) === priority)
                option.selected = true;
            else
                option.selected = false;
        }

        combobox.value = priority;
    };

    const switchCheckboxState = function(e) {
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

    const updateGlobalCheckbox = function() {
        const checkbox = $("tristate_cb");
        if (isAllCheckboxesChecked())
            setCheckboxChecked(checkbox);
        else if (isAllCheckboxesUnchecked())
            setCheckboxUnchecked(checkbox);
        else
            setCheckboxPartial(checkbox);
    };

    const setCheckboxChecked = function(checkbox) {
        checkbox.state = "checked";
        checkbox.indeterminate = false;
        checkbox.checked = true;
    };

    const setCheckboxUnchecked = function(checkbox) {
        checkbox.state = "unchecked";
        checkbox.indeterminate = false;
        checkbox.checked = false;
    };

    const setCheckboxPartial = function(checkbox) {
        checkbox.state = "partial";
        checkbox.indeterminate = true;
    };

    const isAllCheckboxesChecked = function() {
        const checkboxes = $$("input.DownloadedCB");
        for (let i = 0; i < checkboxes.length; ++i) {
            if (!checkboxes[i].checked)
                return false;
        }
        return true;
    };

    const isAllCheckboxesUnchecked = function() {
        const checkboxes = $$("input.DownloadedCB");
        for (let i = 0; i < checkboxes.length; ++i) {
            if (checkboxes[i].checked)
                return false;
        }
        return true;
    };

    const setFilePriority = function(ids, fileIds, priority) {
        if (current_hash === "")
            return;

        clearTimeout(loadTorrentFilesDataTimer);
        new Request({
            url: "api/v2/torrents/filePrio",
            method: "post",
            data: {
                "hash": current_hash,
                "id": fileIds.join("|"),
                "priority": priority
            },
            onComplete: function() {
                loadTorrentFilesDataTimer = loadTorrentFilesData.delay(1000);
            }
        }).send();

        const ignore = (priority === FilePriority.Ignored);
        ids.forEach((_id) => {
            torrentFilesTable.setIgnored(_id, ignore);

            const combobox = $("comboPrio" + _id);
            if (combobox !== null)
                selectComboboxPriority(combobox, priority);
        });

        torrentFilesTable.updateTable(false);
    };

    let loadTorrentFilesDataTimer;
    const loadTorrentFilesData = function() {
        if ($("prop_files").hasClass("invisible")
            || $("propertiesPanel_collapseToggle").hasClass("panel-expand")) {
            // Tab changed, don't do anything
            return;
        }
        const new_hash = torrentsTable.getCurrentTorrentID();
        if (new_hash === "") {
            torrentFilesTable.clear();
            clearTimeout(loadTorrentFilesDataTimer);
            loadTorrentFilesDataTimer = loadTorrentFilesData.delay(5000);
            return;
        }
        let loadedNewTorrent = false;
        if (new_hash !== current_hash) {
            torrentFilesTable.clear();
            current_hash = new_hash;
            loadedNewTorrent = true;
        }
        const url = new URI("api/v2/torrents/files?hash=" + current_hash);
        new Request.JSON({
            url: url,
            method: "get",
            noCache: true,
            onComplete: function() {
                clearTimeout(loadTorrentFilesDataTimer);
                loadTorrentFilesDataTimer = loadTorrentFilesData.delay(5000);
            },
            onSuccess: function(files) {
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
            }
        }).send();
    };

    const updateData = function() {
        clearTimeout(loadTorrentFilesDataTimer);
        loadTorrentFilesData();
    };

    const handleNewTorrentFiles = function(files) {
        is_seed = (files.length > 0) ? files[0].is_seed : true;

        const rows = files.map((file, index) => {
            let progress = (file.progress * 100).round(1);
            if ((progress === 100) && (file.progress < 1))
                progress = 99.9;

            const ignore = (file.priority === FilePriority.Ignored);
            const checked = (ignore ? TriState.Unchecked : TriState.Checked);
            const remaining = (ignore ? 0 : (file.size * (1.0 - file.progress)));
            const row = {
                fileId: index,
                checked: checked,
                fileName: file.name,
                name: window.qBittorrent.Filesystem.fileName(file.name),
                size: file.size,
                progress: progress,
                priority: normalizePriority(file.priority),
                remaining: remaining,
                availability: file.availability
            };

            return row;
        });

        addRowsToTable(rows);
        updateGlobalCheckbox();
    };

    const addRowsToTable = function(rows) {
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
        torrentFilesTable.altRow();

        if (selectedFiles.length > 0)
            torrentFilesTable.reselectRows(selectedFiles);
    };

    const collapseIconClicked = function(event) {
        const id = event.getAttribute("data-id");
        const node = torrentFilesTable.getNode(id);
        const isCollapsed = (event.parentElement.getAttribute("data-collapsed") === "true");

        if (isCollapsed)
            expandNode(node);
        else
            collapseNode(node);
    };

    const expandFolder = function(id) {
        const node = torrentFilesTable.getNode(id);
        if (node.isFolder)
            expandNode(node);
    };

    const collapseFolder = function(id) {
        const node = torrentFilesTable.getNode(id);
        if (node.isFolder)
            collapseNode(node);
    };

    const filesPriorityMenuClicked = function(priority) {
        const selectedRows = torrentFilesTable.selectedRowsIds();
        if (selectedRows.length === 0)
            return;

        const rowIds = [];
        const fileIds = [];
        selectedRows.forEach((rowId) => {
            const elem = $("comboPrio" + rowId);
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

    const singleFileRename = function(hash) {
        const rowId = torrentFilesTable.selectedRowsIds()[0];
        if (rowId === undefined)
            return;
        const row = torrentFilesTable.rows[rowId];
        if (!row)
            return;

        const node = torrentFilesTable.getNode(rowId);
        const path = node.path;

        new MochaUI.Window({
            id: "renamePage",
            title: "QBT_TR(Renaming)QBT_TR[CONTEXT=TorrentContentTreeView]",
            loadMethod: "iframe",
            contentURL: "rename_file.html?hash=" + hash + "&isFolder=" + node.isFolder
                + "&path=" + encodeURIComponent(path),
            scrollbars: false,
            resizable: true,
            maximizable: false,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: 400,
            height: 100
        });
    };

    const multiFileRename = function(hash) {
        new MochaUI.Window({
            id: "multiRenamePage",
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
            resizeLimit: { "x": [800], "y": [420] }
        });
    };

    const torrentFilesContextMenu = new window.qBittorrent.ContextMenu.ContextMenu({
        targets: "#torrentFilesTableDiv tr",
        menu: "torrentFilesMenu",
        actions: {
            Rename: function(element, ref) {
                const hash = torrentsTable.getCurrentTorrentID();
                if (!hash)
                    return;

                if (torrentFilesTable.selectedRowsIds().length > 1)
                    multiFileRename(hash);
                else
                    singleFileRename(hash);
            },

            FilePrioIgnore: function(element, ref) {
                filesPriorityMenuClicked(FilePriority.Ignored);
            },
            FilePrioNormal: function(element, ref) {
                filesPriorityMenuClicked(FilePriority.Normal);
            },
            FilePrioHigh: function(element, ref) {
                filesPriorityMenuClicked(FilePriority.High);
            },
            FilePrioMaximum: function(element, ref) {
                filesPriorityMenuClicked(FilePriority.Maximum);
            }
        },
        offsets: {
            x: -15,
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
    const tableHeaders = $$("#torrentFilesTableFixedHeaderDiv .dynamicTableHeader th");
    if (tableHeaders.length > 0) {
        const checkbox = new Element("input");
        checkbox.type = "checkbox";
        checkbox.id = "tristate_cb";
        checkbox.addEvent("click", switchCheckboxState);

        const checkboxTH = tableHeaders[0];
        checkbox.injectInside(checkboxTH);
    }

    // default sort by name column
    if (torrentFilesTable.getSortedColumn() === null)
        torrentFilesTable.setSortedColumn("name");

    // listen for changes to torrentFilesFilterInput
    let torrentFilesFilterInputTimer = -1;
    $("torrentFilesFilterInput").addEvent("input", () => {
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
    const _hideNode = function(node, shouldHide) {
        const span = $("filesTablefileName" + node.rowId);
        // span won't exist if row has been filtered out
        if (span === null)
            return;
        const rowElem = span.parentElement.parentElement;
        if (shouldHide)
            rowElem.addClass("invisible");
        else
            rowElem.removeClass("invisible");
    };

    /**
     * Update a node's collapsed state and icon
     */
    const _updateNodeState = function(node, isCollapsed) {
        const span = $("filesTablefileName" + node.rowId);
        // span won't exist if row has been filtered out
        if (span === null)
            return;
        const td = span.parentElement;

        // store collapsed state
        td.setAttribute("data-collapsed", isCollapsed);

        // rotate the collapse icon
        const collapseIcon = td.getElementsByClassName("filesTableCollapseIcon")[0];
        if (isCollapsed)
            collapseIcon.addClass("rotate");
        else
            collapseIcon.removeClass("rotate");
    };

    const _isCollapsed = function(node) {
        const span = $("filesTablefileName" + node.rowId);
        if (span === null)
            return true;

        const td = span.parentElement;
        return td.getAttribute("data-collapsed") === "true";
    };

    const expandNode = function(node) {
        _collapseNode(node, false, false, false);
        torrentFilesTable.altRow();
    };

    const collapseNode = function(node) {
        _collapseNode(node, true, false, false);
        torrentFilesTable.altRow();
    };

    const expandAllNodes = function() {
        const root = torrentFilesTable.getRoot();
        root.children.each((node) => {
            node.children.each((child) => {
                _collapseNode(child, false, true, false);
            });
        });
        torrentFilesTable.altRow();
    };

    const collapseAllNodes = function() {
        const root = torrentFilesTable.getRoot();
        root.children.each((node) => {
            node.children.each((child) => {
                _collapseNode(child, true, true, false);
            });
        });
        torrentFilesTable.altRow();
    };

    /**
     * Collapses a folder node with the option to recursively collapse all children
     * @param {FolderNode} node the node to collapse/expand
     * @param {boolean} shouldCollapse true if the node should be collapsed, false if it should be expanded
     * @param {boolean} applyToChildren true if the node's children should also be collapsed, recursively
     * @param {boolean} isChildNode true if the current node is a child of the original node we collapsed/expanded
     */
    const _collapseNode = function(node, shouldCollapse, applyToChildren, isChildNode) {
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

    return exports();
})();

Object.freeze(window.qBittorrent.PropFiles);
