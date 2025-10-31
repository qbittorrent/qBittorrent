/*
 * MIT License
 * Copyright (c) 2008 Ishan Arora <ishan@qbittorrent.org> & Christophe Dumez <chris@qbittorrent.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**************************************************************

    Script      : Dynamic Table
    Version     : 0.5
    Authors     : Ishan Arora & Christophe Dumez
    Desc        : Programmable sortable table
    Licence     : Open Source MIT Licence

 **************************************************************/

"use strict";

window.qBittorrent ??= {};
window.qBittorrent.DynamicTable ??= (() => {
    const exports = () => {
        return {
            TorrentsTable: TorrentsTable,
            TorrentPeersTable: TorrentPeersTable,
            SearchResultsTable: SearchResultsTable,
            SearchPluginsTable: SearchPluginsTable,
            TorrentTrackersTable: TorrentTrackersTable,
            TorrentFilesTable: TorrentFilesTable,
            BulkRenameTorrentFilesTable: BulkRenameTorrentFilesTable,
            AddTorrentFilesTable: AddTorrentFilesTable,
            LogMessageTable: LogMessageTable,
            LogPeerTable: LogPeerTable,
            RssFeedTable: RssFeedTable,
            RssArticleTable: RssArticleTable,
            RssDownloaderRulesTable: RssDownloaderRulesTable,
            RssDownloaderFeedSelectionTable: RssDownloaderFeedSelectionTable,
            RssDownloaderArticlesTable: RssDownloaderArticlesTable,
            TorrentWebseedsTable: TorrentWebseedsTable,
            TorrentCreationTasksTable: TorrentCreationTasksTable,
        };
    };

    const compareNumbers = (val1, val2) => {
        if (val1 < val2)
            return -1;
        if (val1 > val2)
            return 1;
        return 0;
    };

    const localPreferences = new window.qBittorrent.LocalPreferences.LocalPreferences();

    class DynamicTable {
        #DynamicTableHeaderContextMenuClass = null;

        setup(dynamicTableDivId, dynamicTableFixedHeaderDivId, contextMenu, useVirtualList = false) {
            this.dynamicTableDivId = dynamicTableDivId;
            this.dynamicTableFixedHeaderDivId = dynamicTableFixedHeaderDivId;
            this.dynamicTableDiv = document.getElementById(dynamicTableDivId);
            this.useVirtualList = useVirtualList && (localPreferences.get("use_virtual_list", "false") === "true");
            this.fixedTableHeader = document.querySelector(`#${dynamicTableFixedHeaderDivId} thead tr`);
            this.hiddenTableHeader = this.dynamicTableDiv.querySelector("thead tr");
            this.table = this.dynamicTableDiv.querySelector("table");
            this.tableBody = this.dynamicTableDiv.querySelector("tbody");
            this.rowHeight = 26;
            this.rows = new Map();
            this.cachedElements = [];
            this.selectedRows = [];
            this.columns = [];
            this.contextMenu = contextMenu;
            this.sortedColumn = localPreferences.get(`sorted_column_${this.dynamicTableDivId}`, 0);
            this.reverseSort = localPreferences.get(`reverse_sort_${this.dynamicTableDivId}`, "0");
            this.initColumns();
            this.loadColumnsOrder();
            this.updateTableHeaders();
            this.setupCommonEvents();
            this.setupHeaderEvents();
            this.setupHeaderMenu();
            this.setupAltRow();
            this.setupVirtualList();
        }

        setupVirtualList() {
            if (!this.useVirtualList)
                return;
            this.table.style.position = "relative";

            this.renderedOffset = this.dynamicTableDiv.scrollTop;
            this.renderedHeight = this.dynamicTableDiv.offsetHeight;
            const resizeCallback = window.qBittorrent.Misc.createDebounceHandler(100, () => {
                const height = this.dynamicTableDiv.offsetHeight;
                const needRerender = this.renderedHeight < height;
                this.renderedHeight = height;
                if (needRerender)
                    this.rerender();
            });
            new ResizeObserver(resizeCallback).observe(this.dynamicTableDiv);
        }

        setupCommonEvents() {
            const tableFixedHeaderDiv = document.getElementById(this.dynamicTableFixedHeaderDivId);

            const tableElement = tableFixedHeaderDiv.querySelector("table");
            this.dynamicTableDiv.addEventListener("scroll", (e) => {
                tableElement.style.left = `${-this.dynamicTableDiv.scrollLeft}px`;
                // rerender on scroll
                if (this.useVirtualList) {
                    this.renderedOffset = this.dynamicTableDiv.scrollTop;
                    this.rerender();
                }
            });

            this.dynamicTableDiv.addEventListener("click", (e) => {
                const tr = e.target.closest("tr");
                if (!tr) {
                    // clicking on the table body deselects all rows
                    this.deselectAll();
                    this.setRowClass();
                    return;
                }

                if (e.ctrlKey || e.metaKey) {
                    // CTRL/CMD ⌘ key was pressed
                    if (this.isRowSelected(tr.rowId))
                        this.deselectRow(tr.rowId);
                    else
                        this.selectRow(tr.rowId);
                }
                else if (e.shiftKey && (this.selectedRows.length === 1)) {
                    // Shift key was pressed
                    this.selectRows(this.getSelectedRowId(), tr.rowId);
                }
                else {
                    // Simple selection
                    this.deselectAll();
                    this.selectRow(tr.rowId);
                }
            });

            this.dynamicTableDiv.addEventListener("contextmenu", (e) => {
                const tr = e.target.closest("tr");
                if (!tr)
                    return;

                if (!this.isRowSelected(tr.rowId)) {
                    this.deselectAll();
                    this.selectRow(tr.rowId);
                }
            }, true);

            this.dynamicTableDiv.addEventListener("touchstart", (e) => {
                const tr = e.target.closest("tr");
                if (!tr)
                    return;

                if (!this.isRowSelected(tr.rowId)) {
                    this.deselectAll();
                    this.selectRow(tr.rowId);
                }
            }, { passive: true });

            this.dynamicTableDiv.addEventListener("keydown", (e) => {
                const tr = e.target.closest("tr");
                if (!tr)
                    return;

                switch (e.key) {
                    case "ArrowUp":
                        e.preventDefault();
                        this.selectPreviousRow();
                        this.dynamicTableDiv.querySelector(".selected").scrollIntoView({ block: "nearest" });
                        break;

                    case "ArrowDown":
                        e.preventDefault();
                        this.selectNextRow();
                        this.dynamicTableDiv.querySelector(".selected").scrollIntoView({ block: "nearest" });
                        break;
                }
            });
        }

        setupHeaderEvents() {
            this.currentHeaderAction = "";
            this.canResize = false;

            const resetElementBorderStyle = (el, side) => {
                if ((side === "left") || (side !== "right"))
                    el.style.borderLeft = "";
                if ((side === "right") || (side !== "left"))
                    el.style.borderRight = "";
            };

            const mouseMoveFn = function(e) {
                const brect = e.target.getBoundingClientRect();
                const mouseXRelative = e.clientX - brect.left;
                if (this.currentHeaderAction === "") {
                    if ((brect.width - mouseXRelative) < 5) {
                        this.resizeTh = e.target;
                        this.canResize = true;
                        e.target.closest("tr").style.cursor = "col-resize";
                    }
                    else if ((mouseXRelative < 5) && e.target.getPrevious('[class=""]')) {
                        this.resizeTh = e.target.getPrevious('[class=""]');
                        this.canResize = true;
                        e.target.closest("tr").style.cursor = "col-resize";
                    }
                    else {
                        this.canResize = false;
                        e.target.closest("tr").style.cursor = "";
                    }
                }
                if (this.currentHeaderAction === "drag") {
                    const previousVisibleSibling = e.target.getPrevious('[class=""]');
                    let borderChangeElement = previousVisibleSibling;
                    let changeBorderSide = "right";

                    if (mouseXRelative > (brect.width / 2)) {
                        borderChangeElement = e.target;
                        this.dropSide = "right";
                    }
                    else {
                        this.dropSide = "left";
                    }

                    e.target.closest("tr").style.cursor = "move";

                    if (!previousVisibleSibling) { // right most column
                        borderChangeElement = e.target;

                        if (mouseXRelative <= (brect.width / 2))
                            changeBorderSide = "left";
                    }

                    const borderStyle = "solid #e60";
                    if (changeBorderSide === "left") {
                        borderChangeElement.style.borderLeft = borderStyle;
                        borderChangeElement.style.borderLeftWidth = "initial";
                    }
                    else {
                        borderChangeElement.style.borderRight = borderStyle;
                        borderChangeElement.style.borderRightWidth = "initial";
                    }

                    resetElementBorderStyle(borderChangeElement, ((changeBorderSide === "right") ? "left" : "right"));

                    borderChangeElement.getSiblings('[class=""]').each((el) => {
                        resetElementBorderStyle(el);
                    });
                }
                this.lastHoverTh = e.target;
                this.lastClientX = e.clientX;
            }.bind(this);

            const mouseOutFn = (e) => {
                resetElementBorderStyle(e.target);
            };

            const onBeforeStart = function(el) {
                this.clickedTh = el;
                this.currentHeaderAction = "start";
                this.dragMovement = false;
                this.dragStartX = this.lastClientX;
            }.bind(this);

            const onStart = function(el, event) {
                if (this.canResize) {
                    this.currentHeaderAction = "resize";
                    this.startWidth = Number.parseInt(this.resizeTh.style.width, 10);
                }
                else {
                    this.currentHeaderAction = "drag";
                    el.style.backgroundColor = "#C1D5E7";
                }
            }.bind(this);

            const onDrag = function(el, event) {
                if (this.currentHeaderAction === "resize") {
                    let width = this.startWidth + (event.event.pageX - this.dragStartX);
                    if (width < 16)
                        width = 16;

                    this.#setColumnWidth(this.resizeTh.columnName, width);
                }
            }.bind(this);

            const onComplete = function(el, event) {
                resetElementBorderStyle(this.lastHoverTh);
                el.style.backgroundColor = "";
                if (this.currentHeaderAction === "resize")
                    this.saveColumnWidth(this.resizeTh.columnName);
                if ((this.currentHeaderAction === "drag") && (el !== this.lastHoverTh)) {
                    this.saveColumnsOrder();
                    const val = localPreferences.get(`columns_order_${this.dynamicTableDivId}`).split(",");
                    val.erase(el.columnName);
                    let pos = val.indexOf(this.lastHoverTh.columnName);
                    if (this.dropSide === "right")
                        ++pos;
                    val.splice(pos, 0, el.columnName);
                    localPreferences.set(`columns_order_${this.dynamicTableDivId}`, val.join(","));
                    this.loadColumnsOrder();
                    this.updateTableHeaders();
                    this.tableBody.replaceChildren();
                    this.updateTable(true);
                    this.reselectRows(this.selectedRowsIds());
                }
                if (this.currentHeaderAction === "drag") {
                    resetElementBorderStyle(el);
                    el.getSiblings('[class=""]').each((el) => {
                        resetElementBorderStyle(el);
                    });
                }
                this.currentHeaderAction = "";
            }.bind(this);

            const onCancel = function(el) {
                this.currentHeaderAction = "";

                // ignore click/touch events performed when on the column's resize area
                if (!this.canResize)
                    this.setSortedColumn(el.columnName);
            }.bind(this);

            const onTouch = function(e) {
                const column = e.target.columnName;
                this.currentHeaderAction = "";
                this.setSortedColumn(column);
            }.bind(this);

            const onDoubleClick = function(e) {
                e.preventDefault();
                this.currentHeaderAction = "";

                // only resize when hovering on the column's resize area
                if (this.canResize) {
                    this.currentHeaderAction = "resize";
                    this.autoResizeColumn(e.target.columnName);
                    onComplete(e.target);
                }
            }.bind(this);

            for (const th of this.getRowCells(this.fixedTableHeader)) {
                th.addEventListener("mousemove", mouseMoveFn);
                th.addEventListener("mouseout", mouseOutFn);
                th.addEventListener("touchend", onTouch, { passive: true });
                th.addEventListener("dblclick", onDoubleClick);
                th.makeResizable({
                    modifiers: {
                        x: "",
                        y: ""
                    },
                    onBeforeStart: onBeforeStart,
                    onStart: onStart,
                    onDrag: onDrag,
                    onComplete: onComplete,
                    onCancel: onCancel
                });
            }
        }

        setupDynamicTableHeaderContextMenuClass() {
            this.#DynamicTableHeaderContextMenuClass ??= class extends window.qBittorrent.ContextMenu.ContextMenu {
                updateMenuItems() {
                    for (let i = 0; i < this.dynamicTable.columns.length; ++i) {
                        if (this.dynamicTable.columns[i].caption === "")
                            continue;
                        if (this.dynamicTable.columns[i].visible !== "0")
                            this.setItemChecked(this.dynamicTable.columns[i].name, true);
                        else
                            this.setItemChecked(this.dynamicTable.columns[i].name, false);
                    }
                }
            };
        }

        showColumn(columnName, show) {
            this.columns[columnName].visible = show ? "1" : "0";
            localPreferences.set(`column_${columnName}_visible_${this.dynamicTableDivId}`, show ? "1" : "0");
            this.updateColumn(columnName);
            this.columns[columnName].onVisibilityChange?.(columnName);
        }

        #calculateColumnBodyWidth(column) {
            const columnIndex = this.getColumnPos(column.name);
            const bodyColumn = document.getElementById(this.dynamicTableDivId).querySelectorAll("tr>th")[columnIndex];
            const canvas = document.createElement("canvas");
            const context = canvas.getContext("2d");
            context.font = window.getComputedStyle(bodyColumn, null).getPropertyValue("font");

            const longestTd = { value: "", width: 0 };
            for (const tr of this.getTrs()) {
                const tds = this.getRowCells(tr);
                const td = tds[columnIndex];

                const buffer = column.calculateBuffer(tr.rowId);
                const valueWidth = context.measureText(td.textContent).width;
                if ((valueWidth + buffer) > (longestTd.width)) {
                    longestTd.value = td.textContent;
                    longestTd.width = valueWidth + buffer;
                }
            }

            // slight buffer to prevent clipping
            return longestTd.width + 10;
        }

        #setColumnWidth(columnName, width) {
            const column = this.columns[columnName];
            column.width = width;

            const pos = this.getColumnPos(column.name);
            const style = `width: ${column.width}px; ${column.style}`;
            this.getRowCells(this.hiddenTableHeader)[pos].style.cssText = style;
            this.getRowCells(this.fixedTableHeader)[pos].style.cssText = style;
            // rerender on column resize
            if (this.useVirtualList)
                this.rerender();

            column.onResize?.(column.name);
        }

        autoResizeColumn(columnName) {
            const column = this.columns[columnName];

            let width = column.staticWidth ?? 0;
            if (column.staticWidth === null) {
                // check required min body width
                const bodyTextWidth = this.#calculateColumnBodyWidth(column);

                // check required min header width
                const columnIndex = this.getColumnPos(column.name);
                const headColumn = document.getElementById(this.dynamicTableFixedHeaderDivId).querySelectorAll("tr>th")[columnIndex];
                const canvas = document.createElement("canvas");
                const context = canvas.getContext("2d");
                context.font = window.getComputedStyle(headColumn, null).getPropertyValue("font");
                const columnTitle = column.caption;
                const sortedIconWidth = 20;
                const headTextWidth = context.measureText(columnTitle).width + sortedIconWidth;

                width = Math.max(headTextWidth, bodyTextWidth);
            }

            this.#setColumnWidth(column.name, width);
            this.saveColumnWidth(column.name);
        }

        saveColumnWidth(columnName) {
            localPreferences.set(`column_${columnName}_width_${this.dynamicTableDivId}`, this.columns[columnName].width);
        }

        setupHeaderMenu() {
            this.setupDynamicTableHeaderContextMenuClass();

            const menuId = `${this.dynamicTableDivId}_headerMenu`;

            // reuse menu if already exists
            let ul = document.getElementById(menuId);
            if (ul === null) {
                ul = document.createElement("ul");
                ul.id = menuId;
                ul.className = "contextMenu scrollableMenu";
            }

            const createLi = (columnName, text) => {
                const anchor = document.createElement("a");
                anchor.href = `#${columnName}`;
                anchor.textContent = text;

                const img = document.createElement("img");
                img.src = "images/checked-completed.svg";
                anchor.prepend(img);

                const listItem = document.createElement("li");
                listItem.appendChild(anchor);

                return listItem;
            };

            const actions = {
                autoResizeAction: function(element, ref, action) {
                    this.autoResizeColumn(element.columnName);
                }.bind(this),

                autoResizeAllAction: function(element, ref, action) {
                    for (const { name } of this.columns)
                        this.autoResizeColumn(name);
                }.bind(this),
            };

            const onMenuItemClicked = function(element, ref, action) {
                this.showColumn(action, this.columns[action].visible === "0");
            }.bind(this);

            // recreate child elements when reusing (enables the context menu to work correctly)
            ul.replaceChildren();

            for (let i = 0; i < this.columns.length; ++i) {
                const text = this.columns[i].caption;
                if (text === "")
                    continue;
                ul.appendChild(createLi(this.columns[i].name, text));
                actions[this.columns[i].name] = onMenuItemClicked;
            }

            const createResizeElement = (text, href) => {
                const anchor = document.createElement("a");
                anchor.href = href;
                anchor.textContent = text;

                const spacer = document.createElement("span");
                spacer.style = "display: inline-block; width: calc(.5em + 16px);";
                anchor.prepend(spacer);

                const li = document.createElement("li");
                li.appendChild(anchor);
                return li;
            };

            const autoResizeAllElement = createResizeElement("QBT_TR(Resize All)QBT_TR[CONTEXT=ListWidget]", "#autoResizeAllAction");
            const autoResizeElement = createResizeElement("QBT_TR(Resize)QBT_TR[CONTEXT=ListWidget]", "#autoResizeAction");

            ul.firstElementChild.classList.add("separator");
            ul.insertBefore(autoResizeAllElement, ul.firstElementChild);
            ul.insertBefore(autoResizeElement, ul.firstElementChild);
            document.body.append(ul);

            this.headerContextMenu = new this.#DynamicTableHeaderContextMenuClass({
                targets: `#${this.dynamicTableFixedHeaderDivId} tr th`,
                actions: actions,
                menu: menuId,
                offsets: {
                    x: 0,
                    y: 2
                }
            });

            this.headerContextMenu.dynamicTable = this;
        }

        initColumns() {}

        newColumn(name, style, caption, defaultWidth, defaultVisible) {
            const column = {};
            column["name"] = name;
            column["title"] = name;
            column["visible"] = localPreferences.get(`column_${name}_visible_${this.dynamicTableDivId}`, (defaultVisible ? "1" : "0"));
            column["force_hide"] = false;
            column["caption"] = caption;
            column["style"] = style;
            column["width"] = localPreferences.get(`column_${name}_width_${this.dynamicTableDivId}`, defaultWidth);
            column["dataProperties"] = [name];
            column["getRowValue"] = function(row, pos) {
                if (pos === undefined)
                    pos = 0;
                return row["full_data"][this.dataProperties[pos]];
            };
            column["compareRows"] = function(row1, row2) {
                const value1 = this.getRowValue(row1);
                const value2 = this.getRowValue(row2);
                if ((typeof(value1) === "number") && (typeof(value2) === "number"))
                    return compareNumbers(value1, value2);
                return window.qBittorrent.Misc.naturalSortCollator.compare(value1, value2);
            };
            column["updateTd"] = function(td, row) {
                const value = this.getRowValue(row);
                td.textContent = value;
                td.title = value;
            };
            column["isVisible"] = function() {
                return (this.visible === "1") && !this.force_hide;
            };
            column["onResize"] = null;
            column["onVisibilityChange"] = null;
            column["staticWidth"] = null;
            column["calculateBuffer"] = () => 0;
            this.columns.push(column);
            this.columns[name] = column;

            this.hiddenTableHeader.append(document.createElement("th"));
            this.fixedTableHeader.append(document.createElement("th"));
        }

        loadColumnsOrder() {
            const columnsOrder = [];
            const val = localPreferences.get(`columns_order_${this.dynamicTableDivId}`);
            if ((val === null) || (val === undefined))
                return;
            for (const v of val.split(",")) {
                if ((v in this.columns) && (!columnsOrder.contains(v)))
                    columnsOrder.push(v);
            }

            for (let i = 0; i < this.columns.length; ++i) {
                if (!columnsOrder.contains(this.columns[i].name))
                    columnsOrder.push(this.columns[i].name);
            }

            for (let i = 0; i < this.columns.length; ++i)
                this.columns[i] = this.columns[columnsOrder[i]];
        }

        saveColumnsOrder() {
            let val = "";
            for (let i = 0; i < this.columns.length; ++i) {
                if (i > 0)
                    val += ",";
                val += this.columns[i].name;
            }
            localPreferences.set(`columns_order_${this.dynamicTableDivId}`, val);
        }

        updateTableHeaders() {
            this.updateHeader(this.hiddenTableHeader);
            this.updateHeader(this.fixedTableHeader);
            this.setSortedColumnIcon(this.sortedColumn, null, (this.reverseSort === "1"));
        }

        updateHeader(header) {
            const ths = this.getRowCells(header);
            for (const [i, th] of ths.entries()) {
                if (th.columnName !== this.columns[i].name) {
                    th.title = this.columns[i].caption;
                    th.textContent = this.columns[i].caption;
                    th.style.cssText = `width: ${this.columns[i].width}px; ${this.columns[i].style}`;
                    th.columnName = this.columns[i].name;
                    th.className = `column_${th.columnName}`;
                    th.classList.toggle("invisible", ((this.columns[i].visible === "0") || this.columns[i].force_hide));
                }
            }
        }

        getColumnPos(columnName) {
            for (let i = 0; i < this.columns.length; ++i) {
                if (this.columns[i].name === columnName)
                    return i;
            }
            return -1;
        }

        updateColumn(columnName, updateCellData = false) {
            const column = this.columns[columnName];
            const pos = this.getColumnPos(columnName);
            const ths = this.getRowCells(this.hiddenTableHeader);
            const fths = this.getRowCells(this.fixedTableHeader);
            const action = column.isVisible() ? "remove" : "add";
            ths[pos].classList[action]("invisible");
            fths[pos].classList[action]("invisible");

            for (const tr of this.getTrs()) {
                const td = this.getRowCells(tr)[pos];
                td.classList[action]("invisible");
                if (updateCellData)
                    column.updateTd(td, this.rows.get(tr.rowId));
            }
        }

        getSortedColumn() {
            return localPreferences.get(`sorted_column_${this.dynamicTableDivId}`);
        }

        /**
         * @param {string} column name to sort by
         * @param {string|null} reverse defaults to implementation-specific behavior when not specified. Should only be passed when restoring previous state.
         */
        setSortedColumn(column, reverse = null) {
            if (column !== this.sortedColumn) {
                const oldColumn = this.sortedColumn;
                this.sortedColumn = column;
                this.reverseSort = reverse ?? "0";
                this.setSortedColumnIcon(column, oldColumn, false);
            }
            else {
                // Toggle sort order
                this.reverseSort = reverse ?? (this.reverseSort === "0" ? "1" : "0");
                this.setSortedColumnIcon(column, null, (this.reverseSort === "1"));
            }
            localPreferences.set(`sorted_column_${this.dynamicTableDivId}`, column);
            localPreferences.set(`reverse_sort_${this.dynamicTableDivId}`, this.reverseSort);
            this.updateTable(false);
        }

        setSortedColumnIcon(newColumn, oldColumn, isReverse) {
            const getCol = (headerDivId, colName) => {
                const colElem = document.querySelectorAll(`#${headerDivId} .column_${colName}`);
                if (colElem.length === 1)
                    return colElem[0];
                return null;
            };

            const colElem = getCol(this.dynamicTableFixedHeaderDivId, newColumn);
            if (colElem !== null) {
                colElem.classList.add("sorted");
                colElem.classList.toggle("reverse", isReverse);
            }
            const oldColElem = getCol(this.dynamicTableFixedHeaderDivId, oldColumn);
            if (oldColElem !== null)
                oldColElem.classList.remove("sorted", "reverse");
        }

        getSelectedRowId() {
            if (this.selectedRows.length > 0)
                return this.selectedRows[0];
            return "";
        }

        isRowSelected(rowId) {
            return this.selectedRows.contains(rowId);
        }

        setupAltRow() {
            const useAltRowColors = (localPreferences.get("use_alt_row_colors", "true") === "true");
            if (useAltRowColors)
                document.getElementById(this.dynamicTableDivId).classList.add("altRowColors");
        }

        selectAll() {
            this.deselectAll();
            for (const row of this.getFilteredAndSortedRows())
                this.selectedRows.push(row.rowId);
            this.setRowClass();
        }

        deselectAll() {
            this.selectedRows.empty();
        }

        selectRow(rowId) {
            this.selectedRows.push(rowId);
            this.setRowClass();
            this.onSelectedRowChanged();
        }

        deselectRow(rowId) {
            this.selectedRows.erase(rowId);
            this.setRowClass();
            this.onSelectedRowChanged();
        }

        selectRows(rowId1, rowId2) {
            this.deselectAll();
            if (rowId1 === rowId2) {
                this.selectRow(rowId1);
                return;
            }

            let select = false;
            for (const tr of this.getTrs()) {
                if ((tr.rowId === rowId1) || (tr.rowId === rowId2)) {
                    select = !select;
                    this.selectedRows.push(tr.rowId);
                }
                else if (select) {
                    this.selectedRows.push(tr.rowId);
                }
            }
            this.setRowClass();
            this.onSelectedRowChanged();
        }

        reselectRows(rowIds) {
            this.deselectAll();
            this.selectedRows = rowIds.slice();
            this.setRowClass();
        }

        setRowClass() {
            for (const tr of this.getTrs())
                tr.classList.toggle("selected", this.isRowSelected(tr.rowId));
        }

        onSelectedRowChanged() {}

        updateRowData(data) {
            // ensure rowId is a string
            const rowId = `${data["rowId"]}`;
            let row;

            if (!this.rows.has(rowId)) {
                row = {
                    full_data: {},
                    rowId: rowId
                };
                this.rows.set(rowId, row);
            }
            else {
                row = this.rows.get(rowId);
            }

            row["data"] = data;
            for (const x in data) {
                if (!Object.hasOwn(data, x))
                    continue;
                row["full_data"][x] = data[x];
            }
        }

        getTrs() {
            return this.tableBody.querySelectorAll("tr");
        }

        getRowCells(tr) {
            return tr.querySelectorAll("td, th");
        }

        getRow(rowId) {
            return this.rows.get(rowId);
        }

        getFilteredAndSortedRows() {
            const filteredRows = [];

            for (const row of this.getRowValues()) {
                filteredRows.push(row);
                filteredRows[row.rowId] = row;
            }

            const column = this.columns[this.sortedColumn];
            const isReverseSort = (this.reverseSort === "0");
            filteredRows.sort((row1, row2) => {
                const result = column.compareRows(row1, row2);
                return isReverseSort ? result : -result;
            });
            return filteredRows;
        }

        getTrByRowId(rowId) {
            return Array.prototype.find.call(this.getTrs(), (tr => tr.rowId === rowId));
        }

        updateTable(fullUpdate = false) {
            const rows = this.getFilteredAndSortedRows();

            for (let i = 0; i < this.selectedRows.length; ++i) {
                if (!(this.selectedRows[i] in rows)) {
                    this.selectedRows.splice(i, 1);
                    --i;
                }
            }

            if (this.useVirtualList) {
                // rerender on table update
                this.rerender(rows);
            }
            else {
                const trs = [...this.getTrs()];
                const trMap = new Map(trs.map(tr => [tr.rowId, tr]));

                for (const row of rows) {
                    const rowId = row.rowId;
                    const existingTr = trMap.get(rowId);
                    if (existingTr !== undefined) {
                        this.updateRow(existingTr, fullUpdate);
                    }
                    else {
                        const tr = this.createRowElement(row);

                        // TODO look into using DocumentFragment or appending all trs at once for add'l performance gains
                        // add to end of table - we'll move into the proper order later
                        this.tableBody.appendChild(tr);
                        trMap.set(rowId, tr);

                        this.updateRow(tr, true);
                    }
                }

                // reorder table rows
                let prevTr = null;
                for (const [rowPos, { rowId }] of rows.entries()) {
                    const tr = trMap.get(rowId);
                    trMap.delete(rowId);

                    const isInCorrectLocation = rowId === trs[rowPos]?.rowId;
                    if (!isInCorrectLocation) {
                        // move row into correct location
                        if (prevTr === null) {
                            // insert as first row in table
                            if (trs.length === 0)
                                this.tableBody.append(tr);
                            else
                                trs[0].before(tr);
                        }
                        else {
                            prevTr.after(tr);
                        }
                    }
                    prevTr = tr;
                }

                for (const tr of trMap.values())
                    tr.remove();
            }
        }

        rerender(rows = this.getFilteredAndSortedRows()) {
            // set the scrollable height
            const tableHeight = rows.length * this.rowHeight;
            if (tableHeight !== this.previousTableHeight) {
                this.previousTableHeight = tableHeight;
                this.table.style.height = `${tableHeight}px`;
            }

            if (this.renderedHeight === 0)
                return;
            // show extra rows at top/bottom to reduce flickering
            const extraRowCount = 20;
            // how many rows can be shown in the visible area
            const visibleRowCount = Math.ceil(this.renderedHeight / this.rowHeight) + (extraRowCount * 2);
            // start position of visible rows, offsetted by renderedOffset
            let startRow = Math.max((Math.trunc(this.renderedOffset / this.rowHeight) - extraRowCount), 0);
            // ensure startRow is even
            if ((startRow % 2) === 1)
                startRow = Math.max(0, startRow - 1);
            const endRow = Math.min((startRow + visibleRowCount), rows.length);

            const elements = [];
            for (let i = startRow; i < endRow; ++i) {
                const row = rows[i];
                if (row === undefined)
                    continue;
                const offset = i * this.rowHeight;
                const position = i - startRow;
                // reuse existing elements
                let element = this.cachedElements[position];
                if (element !== undefined)
                    this.updateRowElement(element, row.rowId, offset);
                else
                    element = this.cachedElements[position] = this.createRowElement(row, offset);
                elements.push(element);
            }
            this.tableBody.replaceChildren(...elements);

            // update row classes
            this.setRowClass();

            // update visible rows
            for (const row of this.tableBody.children)
                this.updateRow(row, true);
        }

        createRowElement(row, top = -1) {
            const tr = document.createElement("tr");
            // set tabindex so element receives keydown events
            // more info: https://developer.mozilla.org/en-US/docs/Web/API/Element/keydown_event
            tr.tabIndex = -1;

            for (let k = 0; k < this.columns.length; ++k) {
                const td = document.createElement("td");
                if ((this.columns[k].visible === "0") || this.columns[k].force_hide)
                    td.classList.add("invisible");
                tr.append(td);
            }

            this.updateRowElement(tr, row.rowId, top);

            // update context menu
            this.contextMenu?.addTarget(tr);
            return tr;
        }

        updateRowElement(tr, rowId, top) {
            tr.dataset.rowId = rowId;
            tr.rowId = rowId;

            tr.className = "";

            if (this.useVirtualList) {
                tr.style.position = "absolute";
                tr.style.top = `${top}px`;
                tr.style.height = `${this.rowHeight}px`;
            }
        }

        getRowData(row, fullUpdate) {
            return row[fullUpdate ? "full_data" : "data"];
        }

        updateRow(tr, fullUpdate) {
            const row = this.rows.get(tr.rowId);
            const data = this.getRowData(row, fullUpdate);

            const tds = this.getRowCells(tr);
            for (let i = 0; i < this.columns.length; ++i) {
                // required due to position: absolute breaks table layout
                if (this.useVirtualList) {
                    tds[i].style.width = `${this.columns[i].width}px`;
                    tds[i].style.maxWidth = `${this.columns[i].width}px`;
                }
                if (this.columns[i].dataProperties.some(prop => Object.hasOwn(data, prop)))
                    this.columns[i].updateTd(tds[i], row);
            }
            row.data = {};
        }

        removeRow(rowId) {
            this.selectedRows.erase(rowId);
            this.rows.delete(rowId);
            if (this.useVirtualList) {
                this.rerender();
            }
            else {
                const tr = this.getTrByRowId(rowId);
                tr?.remove();
            }
        }

        clear() {
            this.deselectAll();
            this.rows.clear();
            if (this.useVirtualList) {
                this.rerender();
            }
            else {
                for (const tr of this.getTrs())
                    tr.remove();
            }
        }

        selectedRowsIds() {
            return this.selectedRows.slice();
        }

        getRowIds() {
            return this.rows.keys();
        }

        getRowValues() {
            return this.rows.values();
        }

        getRowItems() {
            return this.rows.entries();
        }

        getRowSize() {
            return this.rows.size;
        }

        selectNextRow() {
            const visibleRows = Array.prototype.filter.call(this.getTrs(), (tr => !tr.classList.contains("invisible") && (tr.style.display !== "none")));
            const selectedRowId = this.getSelectedRowId();

            let selectedIndex = -1;
            for (const [i, row] of visibleRows.entries()) {
                if (row.getAttribute("data-row-id") === selectedRowId) {
                    selectedIndex = i;
                    break;
                }
            }

            const isLastRowSelected = (selectedIndex >= (visibleRows.length - 1));
            if (!isLastRowSelected) {
                this.deselectAll();

                const newRow = visibleRows[selectedIndex + 1];
                this.selectRow(newRow.getAttribute("data-row-id"));
            }
        }

        selectPreviousRow() {
            const visibleRows = Array.prototype.filter.call(this.getTrs(), (tr => !tr.classList.contains("invisible") && (tr.style.display !== "none")));
            const selectedRowId = this.getSelectedRowId();

            let selectedIndex = -1;
            for (const [i, row] of visibleRows.entries()) {
                if (row.getAttribute("data-row-id") === selectedRowId) {
                    selectedIndex = i;
                    break;
                }
            }

            const isFirstRowSelected = selectedIndex <= 0;
            if (!isFirstRowSelected) {
                this.deselectAll();

                const newRow = visibleRows[selectedIndex - 1];
                this.selectRow(newRow.getAttribute("data-row-id"));
            }
        }
    }

    class TorrentsTable extends DynamicTable {
        setupVirtualList() {
            super.setupVirtualList();
            this.rowHeight = 22;
        }

        initColumns() {
            this.newColumn("priority", "", "#", 30, true);
            this.newColumn("state_icon", "", "QBT_TR(Status Icon)QBT_TR[CONTEXT=TransferListModel]", 30, false);
            this.newColumn("name", "", "QBT_TR(Name)QBT_TR[CONTEXT=TransferListModel]", 200, true);
            this.newColumn("size", "", "QBT_TR(Size)QBT_TR[CONTEXT=TransferListModel]", 100, true);
            this.newColumn("total_size", "", "QBT_TR(Total Size)QBT_TR[CONTEXT=TransferListModel]", 100, false);
            this.newColumn("progress", "", "QBT_TR(Progress)QBT_TR[CONTEXT=TransferListModel]", 85, true);
            this.newColumn("status", "", "QBT_TR(Status)QBT_TR[CONTEXT=TransferListModel]", 100, true);
            this.newColumn("num_seeds", "", "QBT_TR(Seeds)QBT_TR[CONTEXT=TransferListModel]", 100, true);
            this.newColumn("num_leechs", "", "QBT_TR(Peers)QBT_TR[CONTEXT=TransferListModel]", 100, true);
            this.newColumn("dlspeed", "", "QBT_TR(Down Speed)QBT_TR[CONTEXT=TransferListModel]", 100, true);
            this.newColumn("upspeed", "", "QBT_TR(Up Speed)QBT_TR[CONTEXT=TransferListModel]", 100, true);
            this.newColumn("eta", "", "QBT_TR(ETA)QBT_TR[CONTEXT=TransferListModel]", 100, true);
            this.newColumn("ratio", "", "QBT_TR(Ratio)QBT_TR[CONTEXT=TransferListModel]", 100, true);
            this.newColumn("popularity", "", "QBT_TR(Popularity)QBT_TR[CONTEXT=TransferListModel]", 100, true);
            this.newColumn("category", "", "QBT_TR(Category)QBT_TR[CONTEXT=TransferListModel]", 100, true);
            this.newColumn("tags", "", "QBT_TR(Tags)QBT_TR[CONTEXT=TransferListModel]", 100, true);
            this.newColumn("added_on", "", "QBT_TR(Added On)QBT_TR[CONTEXT=TransferListModel]", 100, true);
            this.newColumn("completion_on", "", "QBT_TR(Completed On)QBT_TR[CONTEXT=TransferListModel]", 100, false);
            this.newColumn("tracker", "", "QBT_TR(Tracker)QBT_TR[CONTEXT=TransferListModel]", 100, false);
            this.newColumn("dl_limit", "", "QBT_TR(Down Limit)QBT_TR[CONTEXT=TransferListModel]", 100, false);
            this.newColumn("up_limit", "", "QBT_TR(Up Limit)QBT_TR[CONTEXT=TransferListModel]", 100, false);
            this.newColumn("downloaded", "", "QBT_TR(Downloaded)QBT_TR[CONTEXT=TransferListModel]", 100, false);
            this.newColumn("uploaded", "", "QBT_TR(Uploaded)QBT_TR[CONTEXT=TransferListModel]", 100, false);
            this.newColumn("downloaded_session", "", "QBT_TR(Session Download)QBT_TR[CONTEXT=TransferListModel]", 100, false);
            this.newColumn("uploaded_session", "", "QBT_TR(Session Upload)QBT_TR[CONTEXT=TransferListModel]", 100, false);
            this.newColumn("amount_left", "", "QBT_TR(Remaining)QBT_TR[CONTEXT=TransferListModel]", 100, false);
            this.newColumn("time_active", "", "QBT_TR(Time Active)QBT_TR[CONTEXT=TransferListModel]", 100, false);
            this.newColumn("save_path", "", "QBT_TR(Save path)QBT_TR[CONTEXT=TransferListModel]", 100, false);
            this.newColumn("completed", "", "QBT_TR(Completed)QBT_TR[CONTEXT=TransferListModel]", 100, false);
            this.newColumn("max_ratio", "", "QBT_TR(Ratio Limit)QBT_TR[CONTEXT=TransferListModel]", 100, false);
            this.newColumn("seen_complete", "", "QBT_TR(Last Seen Complete)QBT_TR[CONTEXT=TransferListModel]", 100, false);
            this.newColumn("last_activity", "", "QBT_TR(Last Activity)QBT_TR[CONTEXT=TransferListModel]", 100, false);
            this.newColumn("availability", "", "QBT_TR(Availability)QBT_TR[CONTEXT=TransferListModel]", 100, false);
            this.newColumn("download_path", "", "QBT_TR(Incomplete Save Path)QBT_TR[CONTEXT=TransferListModel]", 100, false);
            this.newColumn("infohash_v1", "", "QBT_TR(Info Hash v1)QBT_TR[CONTEXT=TransferListModel]", 100, false);
            this.newColumn("infohash_v2", "", "QBT_TR(Info Hash v2)QBT_TR[CONTEXT=TransferListModel]", 100, false);
            this.newColumn("reannounce", "", "QBT_TR(Reannounce In)QBT_TR[CONTEXT=TransferListModel]", 100, false);
            this.newColumn("private", "", "QBT_TR(Private)QBT_TR[CONTEXT=TransferListModel]", 100, false);

            this.columns["state_icon"].dataProperties[0] = "state";
            this.columns["name"].dataProperties.push("state");
            this.columns["num_seeds"].dataProperties.push("num_complete");
            this.columns["num_leechs"].dataProperties.push("num_incomplete");
            this.columns["time_active"].dataProperties.push("seeding_time");

            this.initColumnsFunctions();
        }

        initColumnsFunctions() {
            const getStateIconClasses = (state) => {
                let stateClass = "stateUnknown";
                // normalize states
                switch (state) {
                    case "forcedDL":
                    case "metaDL":
                    case "forcedMetaDL":
                    case "downloading":
                        stateClass = "stateDownloading";
                        break;
                    case "forcedUP":
                    case "uploading":
                        stateClass = "stateUploading";
                        break;
                    case "stalledUP":
                        stateClass = "stateStalledUP";
                        break;
                    case "stalledDL":
                        stateClass = "stateStalledDL";
                        break;
                    case "stoppedDL":
                        stateClass = "stateStoppedDL";
                        break;
                    case "stoppedUP":
                        stateClass = "stateStoppedUP";
                        break;
                    case "queuedDL":
                    case "queuedUP":
                        stateClass = "stateQueued";
                        break;
                    case "checkingDL":
                    case "checkingUP":
                    case "queuedForChecking":
                    case "checkingResumeData":
                        stateClass = "stateChecking";
                        break;
                    case "moving":
                        stateClass = "stateMoving";
                        break;
                    case "error":
                    case "unknown":
                    case "missingFiles":
                        stateClass = "stateError";
                        break;
                    default:
                        break; // do nothing
                }

                return `stateIcon ${stateClass}`;
            };

            // state_icon
            this.columns["state_icon"].updateTd = function(td, row) {
                const state = this.getRowValue(row);
                let div = td.firstElementChild;
                if (div === null) {
                    div = document.createElement("div");
                    td.append(div);
                }

                div.className = `${getStateIconClasses(state)} stateIconColumn`;
            };

            this.columns["state_icon"].onVisibilityChange = (columnName) => {
                // show state icon in name column only when standalone
                // state icon column is hidden
                this.updateColumn("name", true);
            };

            // name
            this.columns["name"].updateTd = function(td, row) {
                const name = this.getRowValue(row, 0);
                const state = this.getRowValue(row, 1);
                let span = td.firstElementChild;
                if (span === null) {
                    span = document.createElement("span");
                    td.append(span);
                }

                span.className = this.isStateIconShown() ? `${getStateIconClasses(state)}` : "";
                span.textContent = name;
                td.title = name;
            };

            this.columns["name"].isStateIconShown = () => !this.columns["state_icon"].isVisible();

            // status
            this.columns["status"].updateTd = function(td, row) {
                const state = this.getRowValue(row);
                if (!state)
                    return;

                let status;
                switch (state) {
                    case "downloading":
                        status = "QBT_TR(Downloading)QBT_TR[CONTEXT=TransferListDelegate]";
                        break;
                    case "stalledDL":
                        status = "QBT_TR(Stalled)QBT_TR[CONTEXT=TransferListDelegate]";
                        break;
                    case "metaDL":
                        status = "QBT_TR(Downloading metadata)QBT_TR[CONTEXT=TransferListDelegate]";
                        break;
                    case "forcedMetaDL":
                        status = "QBT_TR([F] Downloading metadata)QBT_TR[CONTEXT=TransferListDelegate]";
                        break;
                    case "forcedDL":
                        status = "QBT_TR([F] Downloading)QBT_TR[CONTEXT=TransferListDelegate]";
                        break;
                    case "uploading":
                    case "stalledUP":
                        status = "QBT_TR(Seeding)QBT_TR[CONTEXT=TransferListDelegate]";
                        break;
                    case "forcedUP":
                        status = "QBT_TR([F] Seeding)QBT_TR[CONTEXT=TransferListDelegate]";
                        break;
                    case "queuedDL":
                    case "queuedUP":
                        status = "QBT_TR(Queued)QBT_TR[CONTEXT=TransferListDelegate]";
                        break;
                    case "checkingDL":
                    case "checkingUP":
                        status = "QBT_TR(Checking)QBT_TR[CONTEXT=TransferListDelegate]";
                        break;
                    case "queuedForChecking":
                        status = "QBT_TR(Queued for checking)QBT_TR[CONTEXT=TransferListDelegate]";
                        break;
                    case "checkingResumeData":
                        status = "QBT_TR(Checking resume data)QBT_TR[CONTEXT=TransferListDelegate]";
                        break;
                    case "stoppedDL":
                        status = "QBT_TR(Stopped)QBT_TR[CONTEXT=TransferListDelegate]";
                        break;
                    case "stoppedUP":
                        status = "QBT_TR(Completed)QBT_TR[CONTEXT=TransferListDelegate]";
                        break;
                    case "moving":
                        status = "QBT_TR(Moving)QBT_TR[CONTEXT=TransferListDelegate]";
                        break;
                    case "missingFiles":
                        status = "QBT_TR(Missing Files)QBT_TR[CONTEXT=TransferListDelegate]";
                        break;
                    case "error":
                        status = "QBT_TR(Errored)QBT_TR[CONTEXT=TransferListDelegate]";
                        break;
                    default:
                        status = "QBT_TR(Unknown)QBT_TR[CONTEXT=HttpServer]";
                }

                td.textContent = status;
                td.title = status;
            };

            this.columns["status"].compareRows = (row1, row2) => {
                return compareNumbers(row1.full_data._statusOrder, row2.full_data._statusOrder);
            };

            // priority
            this.columns["priority"].updateTd = function(td, row) {
                const queuePos = this.getRowValue(row);
                const formattedQueuePos = (queuePos < 1) ? "*" : queuePos;
                td.textContent = formattedQueuePos;
                td.title = formattedQueuePos;
            };

            this.columns["priority"].compareRows = function(row1, row2) {
                let row1_val = this.getRowValue(row1);
                let row2_val = this.getRowValue(row2);
                if (row1_val < 1)
                    row1_val = 1000000;
                if (row2_val < 1)
                    row2_val = 1000000;
                return compareNumbers(row1_val, row2_val);
            };

            // name, category, tags
            this.columns["name"].compareRows = function(row1, row2) {
                const row1Val = this.getRowValue(row1);
                const row2Val = this.getRowValue(row2);
                return row1Val.localeCompare(row2Val, undefined, { numeric: true, sensitivity: "base" });
            };
            this.columns["category"].compareRows = this.columns["name"].compareRows;
            this.columns["tags"].compareRows = this.columns["name"].compareRows;

            // size, total_size
            this.columns["size"].updateTd = function(td, row) {
                const size = window.qBittorrent.Misc.friendlyUnit(this.getRowValue(row), false);
                td.textContent = size;
                td.title = size;
            };
            this.columns["total_size"].updateTd = this.columns["size"].updateTd;

            // progress
            this.columns["progress"].updateTd = function(td, row) {
                const progress = this.getRowValue(row);
                const progressFormatted = window.qBittorrent.Misc.toFixedPointString((progress * 100), 1);

                const div = td.firstElementChild;
                if (div !== null) {
                    if (div.getValue() !== progressFormatted)
                        div.setValue(progressFormatted);
                }
                else {
                    td.append(new window.qBittorrent.ProgressBar.ProgressBar(progressFormatted));
                }
            };
            this.columns["progress"].staticWidth = 100;

            // num_seeds
            this.columns["num_seeds"].updateTd = function(td, row) {
                const num_seeds = this.getRowValue(row, 0);
                const num_complete = this.getRowValue(row, 1);
                let value = num_seeds;
                if (num_complete !== -1)
                    value += ` (${num_complete})`;
                td.textContent = value;
                td.title = value;
            };
            this.columns["num_seeds"].compareRows = function(row1, row2) {
                const num_seeds1 = this.getRowValue(row1, 0);
                const num_complete1 = this.getRowValue(row1, 1);

                const num_seeds2 = this.getRowValue(row2, 0);
                const num_complete2 = this.getRowValue(row2, 1);

                const result = compareNumbers(num_complete1, num_complete2);
                if (result !== 0)
                    return result;
                return compareNumbers(num_seeds1, num_seeds2);
            };

            // num_leechs
            this.columns["num_leechs"].updateTd = this.columns["num_seeds"].updateTd;
            this.columns["num_leechs"].compareRows = this.columns["num_seeds"].compareRows;

            // dlspeed
            this.columns["dlspeed"].updateTd = function(td, row) {
                const speed = window.qBittorrent.Misc.friendlyUnit(this.getRowValue(row), true);
                td.textContent = speed;
                td.title = speed;
            };

            // upspeed
            this.columns["upspeed"].updateTd = this.columns["dlspeed"].updateTd;

            // eta
            this.columns["eta"].updateTd = function(td, row) {
                const eta = window.qBittorrent.Misc.friendlyDuration(this.getRowValue(row), window.qBittorrent.Misc.MAX_ETA);
                td.textContent = eta;
                td.title = eta;
            };

            // ratio
            this.columns["ratio"].updateTd = function(td, row) {
                const ratio = this.getRowValue(row);
                const string = (ratio === -1) ? "∞" : window.qBittorrent.Misc.toFixedPointString(ratio, 2);
                td.textContent = string;
                td.title = string;
            };

            // popularity
            this.columns["popularity"].updateTd = function(td, row) {
                const value = this.getRowValue(row);
                const popularity = (value === -1) ? "∞" : window.qBittorrent.Misc.toFixedPointString(value, 2);
                td.textContent = popularity;
                td.title = popularity;
            };

            // added on
            this.columns["added_on"].updateTd = function(td, row) {
                const date = new Date(this.getRowValue(row) * 1000).toLocaleString();
                td.textContent = date;
                td.title = date;
            };

            // completion_on
            this.columns["completion_on"].updateTd = function(td, row) {
                const val = this.getRowValue(row);
                if ((val === 0xffffffff) || (val < 0)) {
                    td.textContent = "";
                    td.title = "";
                }
                else {
                    const date = new Date(this.getRowValue(row) * 1000).toLocaleString();
                    td.textContent = date;
                    td.title = date;
                }
            };

            // tracker
            this.columns["tracker"].updateTd = function(td, row) {
                const value = this.getRowValue(row);
                const tracker = displayFullURLTrackerColumn ? value : window.qBittorrent.Misc.getHost(value);
                td.textContent = tracker;
                td.title = value;
            };

            //  dl_limit, up_limit
            this.columns["dl_limit"].updateTd = function(td, row) {
                const speed = this.getRowValue(row);
                if (speed === 0) {
                    td.textContent = "∞";
                    td.title = "∞";
                }
                else {
                    const formattedSpeed = window.qBittorrent.Misc.friendlyUnit(speed, true);
                    td.textContent = formattedSpeed;
                    td.title = formattedSpeed;
                }
            };

            this.columns["up_limit"].updateTd = this.columns["dl_limit"].updateTd;

            // downloaded, uploaded, downloaded_session, uploaded_session, amount_left
            this.columns["downloaded"].updateTd = this.columns["size"].updateTd;
            this.columns["uploaded"].updateTd = this.columns["size"].updateTd;
            this.columns["downloaded_session"].updateTd = this.columns["size"].updateTd;
            this.columns["uploaded_session"].updateTd = this.columns["size"].updateTd;
            this.columns["amount_left"].updateTd = this.columns["size"].updateTd;

            // time active
            this.columns["time_active"].updateTd = function(td, row) {
                const activeTime = this.getRowValue(row, 0);
                const seedingTime = this.getRowValue(row, 1);
                const time = (seedingTime > 0)
                    ? ("QBT_TR(%1 (seeded for %2))QBT_TR[CONTEXT=TransferListDelegate]"
                        .replace("%1", window.qBittorrent.Misc.friendlyDuration(activeTime))
                        .replace("%2", window.qBittorrent.Misc.friendlyDuration(seedingTime)))
                    : window.qBittorrent.Misc.friendlyDuration(activeTime);
                td.textContent = time;
                td.title = time;
            };

            // completed
            this.columns["completed"].updateTd = this.columns["size"].updateTd;

            // max_ratio
            this.columns["max_ratio"].updateTd = this.columns["ratio"].updateTd;

            // seen_complete
            this.columns["seen_complete"].updateTd = this.columns["completion_on"].updateTd;

            // last_activity
            this.columns["last_activity"].updateTd = function(td, row) {
                const val = this.getRowValue(row);
                if (val < 1) {
                    td.textContent = "∞";
                    td.title = "∞";
                }
                else {
                    const formattedVal = "QBT_TR(%1 ago)QBT_TR[CONTEXT=TransferListDelegate]".replace("%1", window.qBittorrent.Misc.friendlyDuration((Date.now() / 1000) - val));
                    td.textContent = formattedVal;
                    td.title = formattedVal;
                }
            };

            // availability
            this.columns["availability"].updateTd = function(td, row) {
                const value = window.qBittorrent.Misc.toFixedPointString(this.getRowValue(row), 3);
                td.textContent = value;
                td.title = value;
            };

            // infohash_v1
            this.columns["infohash_v1"].updateTd = function(td, row) {
                const sourceInfohashV1 = this.getRowValue(row);
                const infohashV1 = (sourceInfohashV1 !== "") ? sourceInfohashV1 : "QBT_TR(N/A)QBT_TR[CONTEXT=TransferListDelegate]";
                td.textContent = infohashV1;
                td.title = infohashV1;
            };

            // infohash_v2
            this.columns["infohash_v2"].updateTd = function(td, row) {
                const sourceInfohashV2 = this.getRowValue(row);
                const infohashV2 = (sourceInfohashV2 !== "") ? sourceInfohashV2 : "QBT_TR(N/A)QBT_TR[CONTEXT=TransferListDelegate]";
                td.textContent = infohashV2;
                td.title = infohashV2;
            };

            // reannounce
            this.columns["reannounce"].updateTd = function(td, row) {
                const time = window.qBittorrent.Misc.friendlyDuration(this.getRowValue(row));
                td.textContent = time;
                td.title = time;
            };

            // private
            this.columns["private"].updateTd = function(td, row) {
                const hasMetadata = row["full_data"].has_metadata;
                const isPrivate = this.getRowValue(row);
                const string = hasMetadata
                    ? (isPrivate
                        ? "QBT_TR(Yes)QBT_TR[CONTEXT=PropertiesWidget]"
                        : "QBT_TR(No)QBT_TR[CONTEXT=PropertiesWidget]")
                    : "QBT_TR(N/A)QBT_TR[CONTEXT=PropertiesWidget]";
                td.textContent = string;
                td.title = string;
            };
        }

        applyFilter(row, filterName, category, tag, trackerHost, filterTerms) {
            const state = row["full_data"].state;
            let inactive = false;

            switch (filterName) {
                case "downloading":
                    if ((state !== "downloading") && !state.includes("DL"))
                        return false;
                    break;
                case "seeding":
                    if ((state !== "uploading") && (state !== "forcedUP") && (state !== "stalledUP") && (state !== "queuedUP") && (state !== "checkingUP"))
                        return false;
                    break;
                case "completed":
                    if ((state !== "uploading") && !state.includes("UP"))
                        return false;
                    break;
                case "stopped":
                    if (!state.includes("stopped"))
                        return false;
                    break;
                case "running":
                    if (state.includes("stopped"))
                        return false;
                    break;
                case "stalled":
                    if ((state !== "stalledUP") && (state !== "stalledDL"))
                        return false;
                    break;
                case "stalled_uploading":
                    if (state !== "stalledUP")
                        return false;
                    break;
                case "stalled_downloading":
                    if (state !== "stalledDL")
                        return false;
                    break;
                case "inactive":
                    inactive = true;
                    // fallthrough
                case "active": {
                    let r;
                    if (state === "stalledDL")
                        r = (row["full_data"].upspeed > 0);
                    else
                        r = (state === "metaDL") || (state === "forcedMetaDL") || (state === "downloading") || (state === "forcedDL") || (state === "uploading") || (state === "forcedUP");
                    if (r === inactive)
                        return false;
                    break;
                }
                case "checking":
                    if ((state !== "checkingUP") && (state !== "checkingDL") && (state !== "checkingResumeData"))
                        return false;
                    break;
                case "moving":
                    if (state !== "moving")
                        return false;
                    break;
                case "errored":
                    if ((state !== "error") && (state !== "unknown") && (state !== "missingFiles"))
                        return false;
                    break;
            }

            switch (category) {
                case CATEGORIES_ALL:
                    break; // do nothing

                case CATEGORIES_UNCATEGORIZED:
                    if (row["full_data"].category.length > 0)
                        return false;
                    break; // do nothing

                default:
                    if (!useSubcategories) {
                        if (category !== row["full_data"].category)
                            return false;
                    }
                    else {
                        const selectedCategory = window.qBittorrent.Client.categoryMap.get(category);
                        if (selectedCategory !== undefined) {
                            const selectedCategoryName = `${category}/`;
                            const torrentCategoryName = `${row["full_data"].category}/`;
                            if (!torrentCategoryName.startsWith(selectedCategoryName))
                                return false;
                        }
                    }
                    break;
            }

            switch (tag) {
                case TAGS_ALL:
                    break; // do nothing

                case TAGS_UNTAGGED:
                    if (row["full_data"].tags.length > 0)
                        return false;
                    break; // do nothing

                default: {
                    const tags = row["full_data"].tags.split(", ");
                    if (!tags.contains(tag))
                        return false;
                    break;
                }
            }

            switch (trackerHost) {
                case TRACKERS_ALL:
                    break; // do nothing

                case TRACKERS_ANNOUNCE_ERROR:
                    if (!row["full_data"]["has_other_announce_error"])
                        return false;
                    break;

                case TRACKERS_ERROR:
                    if (!row["full_data"]["has_tracker_error"])
                        return false;
                    break;

                case TRACKERS_TRACKERLESS:
                    if (row["full_data"].trackers_count > 0)
                        return false;
                    break;

                case TRACKERS_WARNING:
                    if (!row["full_data"]["has_tracker_warning"])
                        return false;
                    break;

                default: {
                    const trackerTorrentMap = trackerMap.get(trackerHost);
                    if (trackerTorrentMap !== undefined) {
                        let found = false;
                        for (const torrents of trackerTorrentMap.values()) {
                            if (torrents.has(row["full_data"].rowId)) {
                                found = true;
                                break;
                            }
                        }
                        if (!found)
                            return false;
                    }
                    break;
                }
            }

            if ((filterTerms !== undefined) && (filterTerms !== null)) {
                const filterBy = document.getElementById("torrentsFilterSelect").value;
                const textToSearch = row["full_data"][filterBy].toLowerCase();
                if (filterTerms instanceof RegExp) {
                    if (!filterTerms.test(textToSearch))
                        return false;
                }
                else {
                    if ((filterTerms.length > 0) && !window.qBittorrent.Misc.containsAllTerms(textToSearch, filterTerms))
                        return false;
                }
            }

            return true;
        }

        getFilteredTorrentsNumber(filterName, category, tag, tracker) {
            let cnt = 0;

            for (const row of this.rows.values()) {
                if (this.applyFilter(row, filterName, category, tag, tracker, null))
                    ++cnt;
            }
            return cnt;
        }

        getFilteredTorrentsHashes(filterName, category, tag, tracker) {
            const rowsHashes = [];
            const useRegex = document.getElementById("torrentsFilterRegexBox").checked;
            const filterText = document.getElementById("torrentsFilterInput").value.trim().toLowerCase();
            let filterTerms;
            try {
                filterTerms = (filterText.length > 0)
                    ? (useRegex ? new RegExp(filterText) : filterText.split(" "))
                    : null;
            }
            catch (e) { // SyntaxError: Invalid regex pattern
                return filteredRows;
            }

            for (const row of this.rows.values()) {
                if (this.applyFilter(row, filterName, category, tag, tracker, filterTerms))
                    rowsHashes.push(row["rowId"]);
            }

            return rowsHashes;
        }

        getFilteredAndSortedRows() {
            const filteredRows = [];

            const useRegex = document.getElementById("torrentsFilterRegexBox").checked;
            const filterText = document.getElementById("torrentsFilterInput").value.trim().toLowerCase();
            let filterTerms;
            try {
                filterTerms = (filterText.length > 0)
                    ? (useRegex ? new RegExp(filterText) : filterText.split(" "))
                    : null;
            }
            catch (e) { // SyntaxError: Invalid regex pattern
                return filteredRows;
            }

            for (const row of this.rows.values()) {
                if (this.applyFilter(row, selectedStatus, selectedCategory, selectedTag, selectedTracker, filterTerms)) {
                    filteredRows.push(row);
                    filteredRows[row.rowId] = row;
                }
            }

            const column = this.columns[this.sortedColumn];
            const isReverseSort = (this.reverseSort === "0");
            filteredRows.sort((row1, row2) => {
                const result = column.compareRows(row1, row2);
                return isReverseSort ? result : -result;
            });
            return filteredRows;
        }

        setupCommonEvents() {
            super.setupCommonEvents();
            this.dynamicTableDiv.addEventListener("dblclick", (e) => {
                const tr = e.target.closest("tr");
                if (!tr)
                    return;

                this.deselectAll();
                this.selectRow(tr.rowId);
                const row = this.getRow(tr.rowId);
                const state = row["full_data"].state;

                const prefKey =
                    (state !== "uploading")
                    && (state !== "stoppedUP")
                    && (state !== "forcedUP")
                    && (state !== "stalledUP")
                    && (state !== "queuedUP")
                    && (state !== "checkingUP")
                    ? "dblclick_download"
                    : "dblclick_complete";

                if (localPreferences.get(prefKey, "1") !== "1")
                    return true;

                if (state.includes("stopped"))
                    startFN();
                else
                    stopFN();
            });
        }

        getCurrentTorrentID() {
            return this.getSelectedRowId();
        }

        onSelectedRowChanged() {
            updatePropertiesPanel();
        }

        isStopped(hash) {
            const row = this.getRow(hash);
            return (row === undefined) ? true : row.full_data.state.includes("stopped");
        }
    }

    class TorrentPeersTable extends DynamicTable {
        initColumns() {
            this.newColumn("country", "", "QBT_TR(Country/Region)QBT_TR[CONTEXT=PeerListWidget]", 22, true);
            this.newColumn("ip", "", "QBT_TR(IP/Address)QBT_TR[CONTEXT=PeerListWidget]", 80, true);
            this.newColumn("port", "", "QBT_TR(Port)QBT_TR[CONTEXT=PeerListWidget]", 35, true);
            this.newColumn("connection", "", "QBT_TR(Connection)QBT_TR[CONTEXT=PeerListWidget]", 50, true);
            this.newColumn("flags", "", "QBT_TR(Flags)QBT_TR[CONTEXT=PeerListWidget]", 50, true);
            this.newColumn("client", "", "QBT_TR(Client)QBT_TR[CONTEXT=PeerListWidget]", 140, true);
            this.newColumn("peer_id_client", "", "QBT_TR(Peer ID Client)QBT_TR[CONTEXT=PeerListWidget]", 60, false);
            this.newColumn("progress", "", "QBT_TR(Progress)QBT_TR[CONTEXT=PeerListWidget]", 50, true);
            this.newColumn("dl_speed", "", "QBT_TR(Down Speed)QBT_TR[CONTEXT=PeerListWidget]", 50, true);
            this.newColumn("up_speed", "", "QBT_TR(Up Speed)QBT_TR[CONTEXT=PeerListWidget]", 50, true);
            this.newColumn("downloaded", "", "QBT_TR(Downloaded)QBT_TR[CONTEXT=PeerListWidget]", 50, true);
            this.newColumn("uploaded", "", "QBT_TR(Uploaded)QBT_TR[CONTEXT=PeerListWidget]", 50, true);
            this.newColumn("relevance", "", "QBT_TR(Relevance)QBT_TR[CONTEXT=PeerListWidget]", 30, true);
            this.newColumn("files", "", "QBT_TR(Files)QBT_TR[CONTEXT=PeerListWidget]", 100, true);

            this.columns["country"].dataProperties.push("country_code");
            this.columns["flags"].dataProperties.push("flags_desc");
            this.initColumnsFunctions();
        }

        initColumnsFunctions() {
            // country
            this.columns["country"].updateTd = function(td, row) {
                const country = this.getRowValue(row, 0);
                const country_code = this.getRowValue(row, 1);

                let span = td.firstElementChild;
                if (span === null) {
                    span = document.createElement("span");
                    span.classList.add("flags");
                    td.append(span);
                }

                span.style.backgroundImage = `url('images/flags/${country_code || "xx"}.svg')`;
                span.textContent = country;
                td.title = country;
            };

            // ip
            this.columns["ip"].compareRows = function(row1, row2) {
                const ip1 = this.getRowValue(row1);
                const ip2 = this.getRowValue(row2);

                return window.qBittorrent.Misc.naturalSortCollator.compare(ip1, ip2);
            };

            // flags
            this.columns["flags"].updateTd = function(td, row) {
                td.textContent = this.getRowValue(row, 0);
                td.title = this.getRowValue(row, 1);
            };

            // progress
            this.columns["progress"].updateTd = function(td, row) {
                const progress = this.getRowValue(row);
                const progressFormatted = `${window.qBittorrent.Misc.toFixedPointString((progress * 100), 1)}%`;
                td.textContent = progressFormatted;
                td.title = progressFormatted;
            };

            // dl_speed, up_speed
            this.columns["dl_speed"].updateTd = function(td, row) {
                const speed = this.getRowValue(row);
                if (speed === 0) {
                    td.textContent = "";
                    td.title = "";
                }
                else {
                    const formattedSpeed = window.qBittorrent.Misc.friendlyUnit(speed, true);
                    td.textContent = formattedSpeed;
                    td.title = formattedSpeed;
                }
            };
            this.columns["up_speed"].updateTd = this.columns["dl_speed"].updateTd;

            // downloaded, uploaded
            this.columns["downloaded"].updateTd = function(td, row) {
                const downloaded = window.qBittorrent.Misc.friendlyUnit(this.getRowValue(row), false);
                td.textContent = downloaded;
                td.title = downloaded;
            };
            this.columns["uploaded"].updateTd = this.columns["downloaded"].updateTd;

            // relevance
            this.columns["relevance"].updateTd = this.columns["progress"].updateTd;
            this.columns["relevance"].staticWidth = 100;

            // files
            this.columns["files"].updateTd = function(td, row) {
                const value = this.getRowValue(row, 0);
                td.textContent = value.replace(/\n/g, ";");
                td.title = value;
            };

        }
    }

    class SearchResultsTable extends DynamicTable {
        initColumns() {
            this.newColumn("fileName", "", "QBT_TR(Name)QBT_TR[CONTEXT=SearchResultsTable]", 500, true);
            this.newColumn("fileSize", "", "QBT_TR(Size)QBT_TR[CONTEXT=SearchResultsTable]", 100, true);
            this.newColumn("nbSeeders", "", "QBT_TR(Seeders)QBT_TR[CONTEXT=SearchResultsTable]", 100, true);
            this.newColumn("nbLeechers", "", "QBT_TR(Leechers)QBT_TR[CONTEXT=SearchResultsTable]", 100, true);
            this.newColumn("engineName", "", "QBT_TR(Engine)QBT_TR[CONTEXT=SearchResultsTable]", 100, true);
            this.newColumn("siteUrl", "", "QBT_TR(Engine URL)QBT_TR[CONTEXT=SearchResultsTable]", 250, true);
            this.newColumn("pubDate", "", "QBT_TR(Published On)QBT_TR[CONTEXT=SearchResultsTable]", 200, true);

            this.initColumnsFunctions();
        }

        initColumnsFunctions() {
            const displaySize = function(td, row) {
                const size = window.qBittorrent.Misc.friendlyUnit(this.getRowValue(row), false);
                td.textContent = size;
                td.title = size;
            };
            const displayNum = function(td, row) {
                const value = this.getRowValue(row);
                const formattedValue = (value === "-1") ? "Unknown" : value;
                td.textContent = formattedValue;
                td.title = formattedValue;
            };
            const displayDate = function(td, row) {
                const value = this.getRowValue(row) * 1000;
                const formattedValue = (Number.isNaN(value) || (value <= 0)) ? "" : (new Date(value).toLocaleString());
                td.textContent = formattedValue;
                td.title = formattedValue;
            };

            this.columns["fileSize"].updateTd = displaySize;
            this.columns["nbSeeders"].updateTd = displayNum;
            this.columns["nbLeechers"].updateTd = displayNum;
            this.columns["pubDate"].updateTd = displayDate;
        }

        getFilteredAndSortedRows() {
            const getSizeFilters = () => {
                let minSize = (window.qBittorrent.Search.searchSizeFilter.min > 0) ? (window.qBittorrent.Search.searchSizeFilter.min * Math.pow(1024, window.qBittorrent.Search.searchSizeFilter.minUnit)) : 0;
                let maxSize = (window.qBittorrent.Search.searchSizeFilter.max > 0) ? (window.qBittorrent.Search.searchSizeFilter.max * Math.pow(1024, window.qBittorrent.Search.searchSizeFilter.maxUnit)) : 0;

                if ((minSize > maxSize) && (maxSize > 0)) {
                    const tmp = minSize;
                    minSize = maxSize;
                    maxSize = tmp;
                }

                return {
                    min: minSize,
                    max: maxSize
                };
            };

            const getSeedsFilters = () => {
                let minSeeds = (window.qBittorrent.Search.searchSeedsFilter.min > 0) ? window.qBittorrent.Search.searchSeedsFilter.min : 0;
                let maxSeeds = (window.qBittorrent.Search.searchSeedsFilter.max > 0) ? window.qBittorrent.Search.searchSeedsFilter.max : 0;

                if ((minSeeds > maxSeeds) && (maxSeeds > 0)) {
                    const tmp = minSeeds;
                    minSeeds = maxSeeds;
                    maxSeeds = tmp;
                }

                return {
                    min: minSeeds,
                    max: maxSeeds
                };
            };

            let filteredRows = [];
            const searchTerms = window.qBittorrent.Search.searchText.pattern.toLowerCase().split(" ");
            const filterTerms = window.qBittorrent.Search.searchText.filterPattern.toLowerCase().split(" ");
            const sizeFilters = getSizeFilters();
            const seedsFilters = getSeedsFilters();
            const searchInTorrentName = document.getElementById("searchInTorrentName").value === "names";

            if (searchInTorrentName || (filterTerms.length > 0) || (window.qBittorrent.Search.searchSizeFilter.min > 0) || (window.qBittorrent.Search.searchSizeFilter.max > 0)) {
                for (const row of this.getRowValues()) {

                    if (searchInTorrentName && !window.qBittorrent.Misc.containsAllTerms(row.full_data.fileName, searchTerms))
                        continue;
                    if ((filterTerms.length > 0) && !window.qBittorrent.Misc.containsAllTerms(row.full_data.fileName, filterTerms))
                        continue;
                    if ((sizeFilters.min > 0) && (row.full_data.fileSize < sizeFilters.min))
                        continue;
                    if ((sizeFilters.max > 0) && (row.full_data.fileSize > sizeFilters.max))
                        continue;
                    if ((seedsFilters.min > 0) && (row.full_data.nbSeeders < seedsFilters.min))
                        continue;
                    if ((seedsFilters.max > 0) && (row.full_data.nbSeeders > seedsFilters.max))
                        continue;

                    filteredRows.push(row);
                }
            }
            else {
                filteredRows = [...this.getRowValues()];
            }

            const column = this.columns[this.sortedColumn];
            const isReverseSort = (this.reverseSort === "0");
            filteredRows.sort((row1, row2) => {
                const result = column.compareRows(row1, row2);
                return isReverseSort ? result : -result;
            });

            return filteredRows;
        }
    }

    class SearchPluginsTable extends DynamicTable {
        initColumns() {
            this.newColumn("fullName", "", "QBT_TR(Name)QBT_TR[CONTEXT=SearchPluginsTable]", 175, true);
            this.newColumn("version", "", "QBT_TR(Version)QBT_TR[CONTEXT=SearchPluginsTable]", 100, true);
            this.newColumn("url", "", "QBT_TR(Url)QBT_TR[CONTEXT=SearchPluginsTable]", 175, true);
            this.newColumn("enabled", "", "QBT_TR(Enabled)QBT_TR[CONTEXT=SearchPluginsTable]", 100, true);

            this.initColumnsFunctions();
        }

        initColumnsFunctions() {
            this.columns["enabled"].updateTd = function(td, row) {
                const value = this.getRowValue(row);
                if (value) {
                    td.textContent = "QBT_TR(Yes)QBT_TR[CONTEXT=SearchPluginsTable]";
                    td.title = "QBT_TR(Yes)QBT_TR[CONTEXT=SearchPluginsTable]";
                    td.closest("tr").classList.add("green");
                    td.closest("tr").classList.remove("red");
                }
                else {
                    td.textContent = "QBT_TR(No)QBT_TR[CONTEXT=SearchPluginsTable]";
                    td.title = "QBT_TR(No)QBT_TR[CONTEXT=SearchPluginsTable]";
                    td.closest("tr").classList.add("red");
                    td.closest("tr").classList.remove("green");
                }
            };
        }
    }

    class TorrentTrackersTable extends DynamicTable {
        collapseState = new Map(); // { rowId: String, isCollapsed: bool }

        isTrackerCollapsed(id) {
            return this.collapseState.get(id) ?? true;
        }

        toggleTrackerCollapsed(id) {
            this.collapseState.set(id, !this.isTrackerCollapsed(id));
            this.#updateTrackerRowState(id, this.isTrackerCollapsed(id));
        }

        #updateEndpointVisibility(endpoint, shouldHide) {
            const span = document.getElementById(`trackersTableTrackerUrl${endpoint}`);
            // span won't exist if row has been filtered out
            if (span === null)
                return;
            const tr = span.parentElement.parentElement;
            tr.classList.toggle("invisible", shouldHide);
        }

        #updateTrackerCollapseIcon(tracker, isCollapsed) {
            const span = document.getElementById(`trackersTableTrackerUrl${tracker}`);
            // span won't exist if row has been filtered out
            if (span === null)
                return;
            const td = span.parentElement;

            // rotate the collapse icon
            const collapseIcon = td.firstElementChild;
            collapseIcon.classList.toggle("rotate", isCollapsed);
        }

        #updateTrackerRowState(id, shouldCollapse) {
            // collapsed rows will be filtered out when using virtual list
            if (this.useVirtualList)
                return;

            this.#updateTrackerCollapseIcon(id, shouldCollapse);

            for (const row of this.getRowValues()) {
                const parentId = row.full_data._tracker;
                if (parentId === id)
                    this.#updateEndpointVisibility(row.rowId, shouldCollapse);
            }
        }

        clearCollapseState() {
            this.collapseState.clear();
        }

        initColumns() {
            this.newColumn("url", "", "QBT_TR(URL/Announce Endpoint)QBT_TR[CONTEXT=TrackerListWidget]", 250, true);
            this.newColumn("tier", "", "QBT_TR(Tier)QBT_TR[CONTEXT=TrackerListWidget]", 35, true);
            this.newColumn("btVersion", "", "QBT_TR(BT Protocol)QBT_TR[CONTEXT=TrackerListWidget]", 35, true);
            this.newColumn("status", "", "QBT_TR(Status)QBT_TR[CONTEXT=TrackerListWidget]", 125, true);
            this.newColumn("peers", "", "QBT_TR(Peers)QBT_TR[CONTEXT=TrackerListWidget]", 75, true);
            this.newColumn("seeds", "", "QBT_TR(Seeds)QBT_TR[CONTEXT=TrackerListWidget]", 75, true);
            this.newColumn("leeches", "", "QBT_TR(Leeches)QBT_TR[CONTEXT=TrackerListWidget]", 75, true);
            this.newColumn("downloaded", "", "QBT_TR(Times Downloaded)QBT_TR[CONTEXT=TrackerListWidget]", 100, true);
            this.newColumn("message", "", "QBT_TR(Message)QBT_TR[CONTEXT=TrackerListWidget]", 250, true);
            this.newColumn("nextAnnounce", "", "QBT_TR(Next Announce)QBT_TR[CONTEXT=TrackerListWidget]", 150, true);
            this.newColumn("minAnnounce", "", "QBT_TR(Min Announce)QBT_TR[CONTEXT=TrackerListWidget]", 150, true);

            this.initColumnsFunctions();
        }

        initColumnsFunctions() {
            const naturalSort = function(row1, row2) {
                if (!row1.full_data._sortable || !row2.full_data._sortable)
                    return 0;

                const value1 = this.getRowValue(row1);
                const value2 = this.getRowValue(row2);
                return window.qBittorrent.Misc.naturalSortCollator.compare(value1, value2);
            };

            this.columns["url"].updateTd = (td, row) => {
                const id = row.rowId;
                const data = row.full_data;

                let collapseIcon = td.firstElementChild;
                if (collapseIcon === null) {
                    collapseIcon = document.createElement("img");
                    collapseIcon.src = "images/go-down.svg";
                    collapseIcon.className = "filesTableCollapseIcon";
                    collapseIcon.addEventListener("click", (e) => {
                        const id = collapseIcon.dataset.id;
                        this.toggleTrackerCollapsed(id);
                        if (this.useVirtualList)
                            this.rerender();
                    });
                    td.append(collapseIcon);
                }
                if (data._isTracker) {
                    collapseIcon.style.display = "inline";
                    collapseIcon.style.visibility = data._hasEndpoints ? "visible" : "hidden";
                    collapseIcon.dataset.id = id;
                    collapseIcon.classList.toggle("rotate", this.isTrackerCollapsed(id));
                }
                else {
                    collapseIcon.style.display = "none";
                }

                let span = td.children[1];
                if (span === undefined) {
                    span = document.createElement("span");
                    td.append(span);
                }
                span.id = `trackersTableTrackerUrl${id}`;
                span.textContent = data.url;
                span.style.marginLeft = data._isTracker ? "0" : "20px";
            };

            this.columns["url"].compareRows = naturalSort;
            this.columns["status"].compareRows = naturalSort;
            this.columns["message"].compareRows = naturalSort;

            const sortNumbers = function(row1, row2) {
                if (!row1.full_data._sortable || !row2.full_data._sortable)
                    return 0;

                const value1 = this.getRowValue(row1);
                const value2 = this.getRowValue(row2);
                if (value1 === "")
                    return -1;
                if (value2 === "")
                    return 1;
                return compareNumbers(value1, value2);
            };

            this.columns["tier"].compareRows = sortNumbers;

            const sortMixed = function(row1, row2) {
                if (!row1.full_data._sortable || !row2.full_data._sortable)
                    return 0;

                const value1 = this.getRowValue(row1);
                const value2 = this.getRowValue(row2);
                if (value1 === "QBT_TR(N/A)QBT_TR[CONTEXT=TrackerListWidget]")
                    return -1;
                if (value2 === "QBT_TR(N/A)QBT_TR[CONTEXT=TrackerListWidget]")
                    return 1;
                return compareNumbers(value1, value2);
            };

            this.columns["peers"].compareRows = sortMixed;
            this.columns["seeds"].compareRows = sortMixed;
            this.columns["leeches"].compareRows = sortMixed;
            this.columns["downloaded"].compareRows = sortMixed;

            this.columns["status"].updateTd = function(td, row) {
                let statusClass = "trackerUnknown";
                const status = this.getRowValue(row);
                switch (status) {
                    case "QBT_TR(Disabled)QBT_TR[CONTEXT=TrackerListWidget]":
                        statusClass = "trackerDisabled";
                        break;
                    case "QBT_TR(Not contacted yet)QBT_TR[CONTEXT=TrackerListWidget]":
                        statusClass = "trackerNotContacted";
                        break;
                    case "QBT_TR(Working)QBT_TR[CONTEXT=TrackerListWidget]":
                        statusClass = "trackerWorking";
                        break;
                    case "QBT_TR(Updating...)QBT_TR[CONTEXT=TrackerListWidget]":
                        statusClass = "trackerUpdating";
                        break;
                    case "QBT_TR(Not working)QBT_TR[CONTEXT=TrackerListWidget]":
                    case "QBT_TR(Tracker error)QBT_TR[CONTEXT=TrackerListWidget]":
                    case "QBT_TR(Unreachable)QBT_TR[CONTEXT=TrackerListWidget]":
                        statusClass = "trackerNotWorking";
                        break;
                }

                for (const c of [...td.classList]) {
                    if (c.startsWith("tracker"))
                        td.classList.remove(c);
                }
                td.classList.add(statusClass);
                td.textContent = status;
                td.title = status;
            };

            const friendlyDuration = function(td, row) {
                const value = this.getRowValue(row) ?? 0;
                const seconds = Math.max(value - (Date.now() / 1000), 0);
                const duration = window.qBittorrent.Misc.friendlyDuration(seconds, window.qBittorrent.Misc.MAX_ETA);
                td.textContent = duration;
                td.title = duration;
            };

            this.columns["nextAnnounce"].updateTd = friendlyDuration;
            this.columns["minAnnounce"].updateTd = friendlyDuration;
        }

        getFilteredAndSortedRows() {
            const trackers = [];
            const trakcerEndpoints = new Map();

            for (const row of this.getRowValues()) {
                const tracker = row.full_data._tracker;
                if (tracker) {
                    if (this.useVirtualList && this.isTrackerCollapsed(tracker))
                        continue;
                    const endpoints = trakcerEndpoints.get(tracker);
                    if (endpoints === undefined)
                        trakcerEndpoints.set(tracker, [row]);
                    else
                        endpoints.push(row);
                }
                else {
                    trackers.push(row);
                }
            }

            const column = this.columns[this.sortedColumn];
            const isReverseSort = this.reverseSort === "0";
            const sortRows = (row1, row2) => {
                const result = column.compareRows(row1, row2);
                return isReverseSort ? result : -result;
            };

            const result = [];
            for (const tracker of trackers.sort(sortRows)) {
                result.push(tracker);
                const endpoints = trakcerEndpoints.get(tracker.rowId) || [];
                result.push(...endpoints.sort(sortRows));
            }

            return result;
        }

        updateTable(fullUpdate = false) {
            super.updateTable(fullUpdate);
            if (!this.useVirtualList) {
                for (const row of this.getRowValues()) {
                    if (row.full_data._isTracker)
                        continue;
                    this.#updateEndpointVisibility(row.rowId, this.isTrackerCollapsed(row.full_data._tracker));
                }
            }
        }

        setupCommonEvents() {
            super.setupCommonEvents();
            this.dynamicTableDiv.addEventListener("dblclick", (e) => {
                const tr = e.target.closest("tr");
                if (!tr || (tr.rowId.startsWith("** [") || tr.rowId.startsWith("endpoint|")))
                    return;

                window.qBittorrent.PropTrackers.editTracker(tr);
            });
        }
    }

    class TorrentFilesTable extends DynamicTable {
        filterTerms = [];
        prevFilterTerms = [];
        prevRowsString = null;
        prevFilteredRows = [];
        prevSortedColumn = null;
        prevReverseSort = null;
        fileTree = new window.qBittorrent.FileTree.FileTree();
        supportCollapsing = true;
        collapseState = new Map();
        fileNameColumn = "name";

        isCollapsed(id) {
            if (!this.supportCollapsing)
                return false;
            return this.collapseState.get(id)?.collapsed ?? false;
        }

        expandNode(id) {
            if (!this.supportCollapsing)
                return;

            const state = this.collapseState.get(id);
            if (state !== undefined)
                state.collapsed = false;
            this.#updateNodeState(id, false);
        }

        collapseNode(id) {
            if (!this.supportCollapsing)
                return;

            const state = this.collapseState.get(id);
            if (state !== undefined)
                state.collapsed = true;
            this.#updateNodeState(id, true);
        }

        expandAllNodes() {
            if (!this.supportCollapsing)
                return;

            for (const [key, _] of this.collapseState)
                this.expandNode(key);
        }

        collapseAllNodes() {
            if (!this.supportCollapsing)
                return;

            for (const [key, state] of this.collapseState) {
                // collapse all nodes except root
                if (state.depth >= 1)
                    this.collapseNode(key);
            }
        }

        #updateNodeVisibility(node, shouldHide) {
            const span = document.getElementById(`filesTablefileName${node.rowId}`);
            // span won't exist if row has been filtered out
            if (span === null)
                return;
            const tr = span.parentElement.parentElement;
            tr.classList.toggle("invisible", shouldHide);
        }

        #updateNodeCollapseIcon(node, isCollapsed) {
            const span = document.getElementById(`filesTablefileName${node.rowId}`);
            // span won't exist if row has been filtered out
            if (span === null)
                return;
            const td = span.parentElement;

            // rotate the collapse icon
            const collapseIcon = td.firstElementChild;
            collapseIcon.classList.toggle("rotate", isCollapsed);
        }

        #updateNodeState(id, shouldCollapse) {
            // collapsed rows will be filtered out when using virtual list
            if (this.useVirtualList)
                return;
            const node = this.getNode(id);
            if (!node.isFolder)
                return;

            this.#updateNodeCollapseIcon(node, shouldCollapse);

            this.#updateNodeChildVisibility(node, shouldCollapse);
        }

        #updateNodeChildVisibility(root, shouldHide) {
            const stack = [...root.children];
            while (stack.length > 0) {
                const node = stack.pop();

                this.#updateNodeVisibility(node, (shouldHide ? shouldHide : this.isCollapsed(node.root.rowId)));

                stack.push(...node.children);
            }
        }

        clear() {
            super.clear();
            this.collapseState.clear();
        }

        setupVirtualList() {
            super.setupVirtualList();
            this.rowHeight = 29.5;
        }

        expandFolder(id) {
            if (!this.supportCollapsing)
                return;

            const node = this.getNode(id);
            if (node.isFolder)
                this.expandNode(node.rowId);
        }

        collapseFolder(id) {
            if (!this.supportCollapsing)
                return;

            const node = this.getNode(id);
            if (node.isFolder)
                this.collapseNode(node.rowId);
        }

        isAllCheckboxesChecked() {
            return this.fileTree.toArray().every((node) => node.checked === window.qBittorrent.FileTree.TriState.Checked);
        }

        isAllCheckboxesUnchecked() {
            return this.fileTree.toArray().every((node) => node.checked !== window.qBittorrent.FileTree.TriState.Checked);
        }

        populateTable(root) {
            this.fileTree.setRoot(root);
            for (const node of root.children)
                this.#addNodeToTable(node, 0, root);
        }

        #addNodeToTable(node, depth, parent) {
            node.depth = depth;
            node.parent = parent;

            if (node.isFolder && this.supportCollapsing && !this.collapseState.has(node.rowId))
                this.collapseState.set(node.rowId, { depth: depth, collapsed: false });

            this.updateRowData({
                rowId: node.rowId,
            });

            for (const child of node.children)
                this.#addNodeToTable(child, depth + 1, node);
        }

        getFileTreeArray() {
            return this.fileTree.toArray();
        }

        getRoot() {
            return this.fileTree.getRoot();
        }

        getNode(rowId) {
            return this.fileTree.getNode(rowId);
        }

        getRow(node) {
            const rowId = this.fileTree.getRowId(node).toString();
            return this.rows.get(rowId);
        }

        getRowFileId(rowId) {
            const node = this.getNode(rowId);
            return node.fileId;
        }

        getRowData(row, fullUpdate) {
            return this.getNode(row.rowId);
        }

        calculateRemaining() {
            this.fileTree.getRoot().calculateRemaining();
        }

        initColumns() {
            this.newColumn("checked", "", "", 50, true);
            this.newColumn("name", "", "QBT_TR(Name)QBT_TR[CONTEXT=TrackerListWidget]", 300, true);
            this.newColumn("size", "", "QBT_TR(Total Size)QBT_TR[CONTEXT=TrackerListWidget]", 75, true);
            this.newColumn("progress", "", "QBT_TR(Progress)QBT_TR[CONTEXT=TrackerListWidget]", 100, true);
            this.newColumn("priority", "", "QBT_TR(Download Priority)QBT_TR[CONTEXT=TrackerListWidget]", 150, true);
            this.newColumn("remaining", "", "QBT_TR(Remaining)QBT_TR[CONTEXT=TrackerListWidget]", 75, true);
            this.newColumn("availability", "", "QBT_TR(Availability)QBT_TR[CONTEXT=TrackerListWidget]", 75, true);

            this.initColumnsFunctions();
        }

        initColumnsFunctions() {
            const that = this;
            const displaySize = function(td, row) {
                const size = window.qBittorrent.Misc.friendlyUnit(this.getRowValue(row), false);
                td.textContent = size;
                td.title = size;
            };
            const displayPercentage = function(td, row) {
                const value = window.qBittorrent.Misc.friendlyPercentage(this.getRowValue(row));
                td.textContent = value;
                td.title = value;
            };

            // checked
            this.columns["checked"].updateTd = function(td, row) {
                const id = row.rowId;
                const value = this.getRowValue(row);
                const fileId = that.getRowFileId(id);

                if (td.firstElementChild === null) {
                    const treeImg = document.createElement("img");
                    treeImg.src = "images/L.svg";
                    treeImg.style.marginBottom = "-2px";
                    td.append(treeImg);
                }

                const downloadCheckbox = td.children[1];
                if (downloadCheckbox === undefined)
                    td.append(window.qBittorrent.TorrentContent.createDownloadCheckbox(id, fileId, value));
                else
                    window.qBittorrent.TorrentContent.updateDownloadCheckbox(downloadCheckbox, id, fileId, value);

            };
            this.columns["checked"].staticWidth = 50;

            // name
            this.columns["name"].updateTd = function(td, row) {
                const id = row.rowId;
                const fileNameId = `filesTablefileName${id}`;
                const node = that.getNode(id);
                const value = this.getRowValue(row);

                let collapseIcon = td.firstElementChild;
                if (collapseIcon === null) {
                    collapseIcon = document.createElement("img");
                    collapseIcon.src = "images/go-down.svg";
                    collapseIcon.className = "filesTableCollapseIcon";
                    collapseIcon.addEventListener("click", (e) => {
                        const id = collapseIcon.dataset.id;
                        const node = that.getNode(id);
                        if (node !== null) {
                            if (that.isCollapsed(node.rowId))
                                that.expandNode(node.rowId);
                            else
                                that.collapseNode(node.rowId);
                            if (that.useVirtualList)
                                that.rerender();
                        }
                    });
                    td.append(collapseIcon);
                }
                if (node.isFolder) {
                    collapseIcon.style.marginLeft = `${node.depth * 20}px`;
                    collapseIcon.style.display = "inline";
                    collapseIcon.dataset.id = id;
                    collapseIcon.classList.toggle("rotate", that.isCollapsed(node.rowId));
                }
                else {
                    collapseIcon.style.display = "none";
                }

                let dirImg = td.children[1];
                if (dirImg === undefined) {
                    dirImg = document.createElement("img");
                    dirImg.src = "images/directory.svg";
                    dirImg.style.width = "20px";
                    dirImg.style.paddingRight = "5px";
                    dirImg.style.marginBottom = "-3px";
                    td.append(dirImg);
                }
                dirImg.style.display = node.isFolder ? "inline" : "none";

                let span = td.children[2];
                if (span === undefined) {
                    span = document.createElement("span");
                    td.append(span);
                }
                span.id = fileNameId;
                span.textContent = value;
                span.style.marginLeft = node.isFolder ? "0" : `${(node.depth + 1) * 20}px`;
            };
            this.columns["name"].calculateBuffer = (rowId) => {
                const node = that.getNode(rowId);
                // folders add 20px for folder icon and 15px for collapse icon
                const folderBuffer = node.isFolder ? 35 : 0;
                return (node.depth * 20) + folderBuffer;
            };

            // size
            this.columns["size"].updateTd = displaySize;

            // progress
            if (this.columns["progress"] !== undefined) {
                this.columns["progress"].updateTd = function(td, row) {
                    const value = Number(this.getRowValue(row));

                    const progressBar = td.firstElementChild;
                    if (progressBar === null)
                        td.append(new window.qBittorrent.ProgressBar.ProgressBar(value));
                    else
                        progressBar.setValue(value);
                };
                this.columns["progress"].staticWidth = 100;
            }

            // priority
            this.columns["priority"].updateTd = function(td, row) {
                const id = row.rowId;
                const value = this.getRowValue(row);
                const fileId = that.getRowFileId(id);

                const priorityCombo = td.firstElementChild;
                if (priorityCombo === null)
                    td.append(window.qBittorrent.TorrentContent.createPriorityCombo(id, fileId, value));
                else
                    window.qBittorrent.TorrentContent.updatePriorityCombo(priorityCombo, id, fileId, value);
            };
            this.columns["priority"].staticWidth = 140;

            // remaining, availability
            if (this.columns["remaining"] !== undefined)
                this.columns["remaining"].updateTd = displaySize;
            if (this.columns["availability"] !== undefined)
                this.columns["availability"].updateTd = displayPercentage;

            for (const column of this.columns) {
                column["getRowValue"] = function(row, pos = 0) {
                    const node = that.getNode(row.rowId);
                    return node[this.dataProperties[pos]];
                };
            }
        }

        #sortNodesByColumn(root, column) {
            const isColumnName = (column.name === this.fileNameColumn);
            const isReverseSort = (this.reverseSort === "0");

            const stack = [root];
            while (stack.length > 0) {
                const node = stack.pop();

                node.children.sort((node1, node2) => {
                    // list folders before files when sorting by name
                    if (isColumnName) {
                        if (node1.isFolder && !node2.isFolder)
                            return -1;
                        if (!node1.isFolder && node2.isFolder)
                            return 1;
                    }

                    const result = column.compareRows(node1, node2);
                    return isReverseSort ? result : -result;
                });

                stack.push(...node.children);
            }
        }

        #filterNodes(root, filterTerms) {
            const ret = [];
            const stack = [root];
            const visited = [];

            while (stack.length > 0) {
                const node = stack.at(-1);

                if (node.isFolder && (!this.useVirtualList || !this.isCollapsed(node.rowId))) {
                    const lastVisited = visited.at(-1);

                    if ((visited.length <= 0) || (lastVisited !== node)) {
                        visited.push(node);
                        stack.push(...node.children);
                        continue;
                    }

                    // has children added or itself matches
                    if (lastVisited.has_children_added || window.qBittorrent.Misc.containsAllTerms(node.name, filterTerms)) {
                        ret.push(this.getRow(node));
                        delete node.has_children_added;

                        // propagate up
                        const parent = node.root;
                        if (parent !== undefined)
                            parent.has_children_added = true;
                    }

                    visited.pop();
                }
                else {
                    if (window.qBittorrent.Misc.containsAllTerms(node[this.fileNameColumn], filterTerms)) {
                        ret.push(this.getRow(node));

                        const parent = node.root;
                        if (parent !== undefined)
                            parent.has_children_added = true;
                    }
                }

                stack.pop();
            }

            ret.reverse();
            return ret;
        }

        setFilter(text) {
            const filterTerms = text.trim().toLowerCase().split(" ");
            if ((filterTerms.length === 1) && (filterTerms[0] === ""))
                this.filterTerms = [];
            else
                this.filterTerms = filterTerms;
        }

        generateRowsSignature() {
            const rowsData = [];
            for (const { rowId } of this.getRowValues())
                rowsData.push({ ...this.getNode(rowId).serialize(), collapsed: this.isCollapsed(rowId) });
            return JSON.stringify(rowsData);
        }

        getFilteredAndSortedRows() {
            const root = this.getRoot();
            if (root === null)
                return [];

            const hasRowsChanged = function(rowsString, prevRowsStringString) {
                const rowsChanged = (rowsString !== prevRowsStringString);
                const isFilterTermsChanged = this.filterTerms.reduce((acc, term, index) => {
                    return (acc || (term !== this.prevFilterTerms[index]));
                }, false);
                const isFilterChanged = ((this.filterTerms.length !== this.prevFilterTerms.length)
                    || ((this.filterTerms.length > 0) && isFilterTermsChanged));
                const isSortedColumnChanged = (this.prevSortedColumn !== this.sortedColumn);
                const isReverseSortChanged = (this.prevReverseSort !== this.reverseSort);

                return (rowsChanged || isFilterChanged || isSortedColumnChanged || isReverseSortChanged);
            }.bind(this);

            const rowsString = this.generateRowsSignature();
            if (!hasRowsChanged(rowsString, this.prevRowsString))
                return this.prevFilteredRows;

            // sort, then filter
            this.#sortNodesByColumn(root, this.columns[this.sortedColumn]);
            const rows = (() => {
                if (this.filterTerms.length === 0) {
                    const nodeArray = this.fileTree.toArray();
                    const filteredRows = nodeArray.map(node => this.getRow(node));
                    return filteredRows;
                }

                return this.#filterNodes(root.children[0], this.filterTerms);
            })();

            this.prevFilterTerms = this.filterTerms;
            this.prevRowsString = rowsString;
            this.prevFilteredRows = rows;
            this.prevSortedColumn = this.sortedColumn;
            this.prevReverseSort = this.reverseSort;
            return rows;
        }

        setupCommonEvents() {
            super.setupCommonEvents();
            this.dynamicTableDiv.addEventListener("keydown", (e) => {
                const tr = e.target.closest("tr");
                if (!tr)
                    return;

                switch (e.key) {
                    case "ArrowLeft":
                        e.preventDefault();
                        this.collapseFolder(this.getSelectedRowId());
                        break;
                    case "ArrowRight":
                        e.preventDefault();
                        this.expandFolder(this.getSelectedRowId());
                        break;
                }
            });
        }
    }

    class BulkRenameTorrentFilesTable extends TorrentFilesTable {
        prevCheckboxNum = null;
        supportCollapsing = false;
        fileNameColumn = "original";

        setupVirtualList() {
            super.setupVirtualList();
            this.rowHeight = 29;
        }

        getSelectedRows() {
            const nodes = this.fileTree.toArray();
            return nodes.filter(x => x.checked === 0);
        }

        initColumns() {
            // Blocks saving header width (because window width isn't saved)
            localPreferences.remove(`column_checked_width_${this.dynamicTableDivId}`);
            localPreferences.remove(`column_original_width_${this.dynamicTableDivId}`);
            localPreferences.remove(`column_renamed_width_${this.dynamicTableDivId}`);
            this.newColumn("checked", "", "", 50, true);
            this.newColumn("original", "", "QBT_TR(Original)QBT_TR[CONTEXT=TrackerListWidget]", 270, true);
            this.newColumn("renamed", "", "QBT_TR(Renamed)QBT_TR[CONTEXT=TrackerListWidget]", 220, true);

            this.initColumnsFunctions();
        }

        /**
         * Toggles the global checkbox and all checkboxes underneath
         */
        toggleGlobalCheckbox() {
            const checkbox = document.getElementById("rootMultiRename_cb");
            const isChecked = checkbox.checked || checkbox.indeterminate;

            for (const cb of document.querySelectorAll("input.RenamingCB")) {
                cb.indeterminate = false;
                if (isChecked) {
                    cb.checked = true;
                    cb.state = "checked";
                }
                else {
                    cb.checked = false;
                    cb.state = "unchecked";
                }
            }

            const nodes = this.fileTree.toArray();
            for (const node of nodes)
                node.checked = isChecked ? 0 : 1;

            this.updateGlobalCheckbox();
        }

        toggleNodeTreeCheckbox(rowId, checkState) {
            const node = this.getNode(rowId);
            node.checked = checkState;
            const checkbox = document.getElementById(`cbRename${rowId}`);
            checkbox.checked = node.checked === 0;
            checkbox.state = checkbox.checked ? "checked" : "unchecked";

            for (let i = 0; i < node.children.length; ++i)
                this.toggleNodeTreeCheckbox(node.children[i].rowId, checkState);
        }

        updateGlobalCheckbox() {
            const checkbox = document.getElementById("rootMultiRename_cb");
            const nodes = this.fileTree.toArray();
            const isAllChecked = nodes.every((node) => node.checked === 0);
            const isAllUnchecked = (() => nodes.every((node) => node.checked !== 0));
            if (isAllChecked) {
                checkbox.state = "checked";
                checkbox.indeterminate = false;
                checkbox.checked = true;
            }
            else if (isAllUnchecked()) {
                checkbox.state = "unchecked";
                checkbox.indeterminate = false;
                checkbox.checked = false;
            }
            else {
                checkbox.state = "partial";
                checkbox.indeterminate = true;
                checkbox.checked = false;
            }
        }

        initColumnsFunctions() {
            const that = this;

            // checked
            this.columns["checked"].updateTd = (td, row) => {
                const id = row.rowId;
                const node = that.getNode(id);

                if (td.firstElementChild === null) {
                    const treeImg = document.createElement("img");
                    treeImg.src = "images/L.svg";
                    treeImg.style.marginBottom = "-2px";
                    td.append(treeImg);
                }

                let checkbox = td.children[1];
                if (checkbox === undefined) {
                    checkbox = document.createElement("input");
                    checkbox.type = "checkbox";
                    checkbox.className = "RenamingCB";
                    checkbox.addEventListener("click", (e) => {
                        e.stopPropagation();
                        const targetId = e.target.dataset.id;
                        const ids = [];
                        // when holding shift, set all files between the previously selected one and the clicked one
                        if (e.shiftKey && (that.prevCheckboxNum !== null) && (targetId !== that.prevCheckboxNum)) {
                            const targetState = that.tableBody.querySelector(`.RenamingCB[data-id="${that.prevCheckboxNum}"]`).checked;
                            const checkboxes = that.tableBody.getElementsByClassName("RenamingCB");
                            let started = false;
                            for (const cb of checkboxes) {
                                const currId = cb.dataset.id;
                                if ((currId === targetId) || (currId === that.prevCheckboxNum)) {
                                    if (started) {
                                        ids.push(currId);
                                        cb.checked = targetState;
                                        break;
                                    }
                                    started = true;
                                }
                                if (started) {
                                    ids.push(currId);
                                    cb.checked = targetState;
                                }
                            }
                        }
                        else {
                            ids.push(targetId);
                        }
                        for (const id of ids) {
                            const node = that.getNode(id);
                            node.checked = e.target.checked ? 0 : 1;
                        }
                        that.updateGlobalCheckbox();
                        that.onRowSelectionChange(that.getNode(targetId));
                        that.prevCheckboxNum = targetId;
                    });
                    checkbox.indeterminate = false;
                    td.append(checkbox);
                }
                checkbox.id = `cbRename${id}`;
                checkbox.dataset.id = id;
                checkbox.checked = (node.checked === 0);
                checkbox.state = checkbox.checked ? "checked" : "unchecked";
            };
            this.columns["checked"].staticWidth = 50;

            // original
            this.columns["original"].updateTd = function(td, row) {
                const id = row.rowId;
                const node = that.getNode(id);
                const value = this.getRowValue(row);

                let dirImg = td.children[0];
                if (dirImg === undefined) {
                    dirImg = document.createElement("img");
                    dirImg.src = "images/directory.svg";
                    dirImg.style.width = "20px";
                    dirImg.style.paddingRight = "5px";
                    dirImg.style.marginBottom = "-3px";
                    td.append(dirImg);
                }
                if (node.isFolder) {
                    dirImg.style.display = "inline";
                    dirImg.style.marginLeft = `${node.depth * 20}px`;
                }
                else {
                    dirImg.style.display = "none";
                }

                let span = td.children[1];
                if (span === undefined) {
                    span = document.createElement("span");
                    td.append(span);
                }
                span.textContent = value;
                span.style.marginLeft = node.isFolder ? "0" : `${(node.depth + 1) * 20}px`;
            };

            // renamed
            this.columns["renamed"].updateTd = (td, row) => {
                const id = row.rowId;
                const fileNameRenamedId = `filesTablefileRenamed${id}`;
                const node = that.getNode(id);

                let span = td.firstElementChild;
                if (span === null) {
                    span = document.createElement("span");
                    td.append(span);
                }
                span.id = fileNameRenamedId;
                span.textContent = node.renamed;
            };

            for (const column of this.columns) {
                column["getRowValue"] = function(row, pos = 0) {
                    const node = that.getNode(row.rowId);
                    return node[this.dataProperties[pos]];
                };
            }
        }

        onRowSelectionChange(row) {}

        selectRow() {}

        reselectRows(rowIds) {
            this.deselectAll();
            for (const tr of this.getTrs()) {
                if (rowIds.includes(tr.rowId)) {
                    const node = this.getNode(tr.rowId);
                    node.checked = 0;

                    const checkbox = tr.querySelector(".RenamingCB");
                    checkbox.state = "checked";
                    checkbox.indeterminate = false;
                    checkbox.checked = true;
                }
            }
            this.updateGlobalCheckbox();
        }

        generateRowsSignature() {
            const rowsData = [];
            for (const { full_data } of this.getRowValues())
                rowsData.push(full_data);
            return JSON.stringify(rowsData);
        }

        setupCommonEvents() {
            const headerDiv = document.getElementById("bulkRenameFilesTableFixedHeaderDiv");
            this.dynamicTableDiv.addEventListener("scroll", (e) => {
                headerDiv.scrollLeft = this.dynamicTableDiv.scrollLeft;
                // rerender on scroll
                if (this.useVirtualList) {
                    this.renderedOffset = this.dynamicTableDiv.scrollTop;
                    this.rerender();
                }
            });
        }
    }

    class AddTorrentFilesTable extends TorrentFilesTable {
        initColumns() {
            this.newColumn("checked", "", "", 50, true);
            this.newColumn("name", "", "QBT_TR(Name)QBT_TR[CONTEXT=TrackerListWidget]", 190, true);
            this.newColumn("size", "", "QBT_TR(Total Size)QBT_TR[CONTEXT=TrackerListWidget]", 75, true);
            this.newColumn("priority", "", "QBT_TR(Download Priority)QBT_TR[CONTEXT=TrackerListWidget]", 140, true);

            this.initColumnsFunctions();
        }
    }

    class RssFeedTable extends DynamicTable {
        initColumns() {
            this.newColumn("state_icon", "", "", 30, true);
            this.newColumn("name", "", "QBT_TR(RSS feeds)QBT_TR[CONTEXT=FeedListWidget]", -1, true);

            this.columns["state_icon"].dataProperties[0] = "";

            // map name row to "[name] ([unread])"
            this.columns["name"].dataProperties.push("unread");
            this.columns["name"].updateTd = function(td, row) {
                const name = this.getRowValue(row, 0);
                const unreadCount = this.getRowValue(row, 1);
                const value = `${name} (${unreadCount})`;
                td.textContent = value;
                td.title = value;
            };
        }
        setupHeaderMenu() {}
        setupHeaderEvents() {}
        getFilteredAndSortedRows() {
            return [...this.getRowValues()];
        }
        selectRow(rowId) {
            this.selectedRows.push(rowId);
            this.setRowClass();
            this.onSelectedRowChanged();

            let path = "";
            for (const row of this.getRowValues()) {
                if (row.rowId === rowId) {
                    path = row.full_data.dataPath;
                    break;
                }
            }
            window.qBittorrent.Rss.showRssFeed(path);
        }
        setupCommonEvents() {
            super.setupCommonEvents();
            this.dynamicTableDiv.addEventListener("dblclick", (e) => {
                const tr = e.target.closest("tr");
                if (!tr || (tr.rowId === "0"))
                    return;

                window.qBittorrent.Rss.moveItem(this.getRow(tr.rowId).full_data.dataPath);
            });
        }
        updateRow(tr, fullUpdate) {
            const row = this.rows.get(tr.rowId);
            const tds = this.getRowCells(tr);

            tds[0].style.overflow = "visible";
            const indentation = row.full_data.indentation;
            tds[0].style.paddingLeft = `${indentation * 32 + 4}px`;
            tds[1].style.paddingLeft = `${indentation * 32 + 4}px`;

            return super.updateRow(tr, fullUpdate);
        }
        updateIcons() {
            // state_icon
            for (const row of this.getRowValues()) {
                let img_path;
                switch (row.full_data.status) {
                    case "default":
                        img_path = "images/application-rss.svg";
                        break;
                    case "hasError":
                        img_path = "images/task-reject.svg";
                        break;
                    case "isLoading":
                        img_path = "images/spinner.svg";
                        break;
                    case "unread":
                        img_path = "images/mail-inbox.svg";
                        break;
                    case "isFolder":
                        img_path = "images/folder-documents.svg";
                        break;
                }
                let td;
                for (let i = 0; i < this.tableBody.rows.length; ++i) {
                    if (this.tableBody.rows[i].rowId === row.rowId) {
                        td = this.tableBody.rows[i].children[0];
                        break;
                    }
                }
                if (td.getChildren("img").length > 0) {
                    const img = td.getChildren("img")[0];
                    if (!img.src.includes(img_path)) {
                        img.src = img_path;
                        img.title = status;
                    }
                }
                else {
                    const img = document.createElement("img");
                    img.src = img_path;
                    img.className = "stateIcon";
                    img.width = "22";
                    img.height = "22";
                    td.append(img);
                }
            }
        }
        newColumn(name, style, caption, defaultWidth, defaultVisible) {
            const column = {};
            column["name"] = name;
            column["title"] = name;
            column["visible"] = defaultVisible;
            column["force_hide"] = false;
            column["caption"] = caption;
            column["style"] = style;
            if (defaultWidth !== -1)
                column["width"] = defaultWidth;

            column["dataProperties"] = [name];
            column["getRowValue"] = function(row, pos) {
                if (pos === undefined)
                    pos = 0;
                return row["full_data"][this.dataProperties[pos]];
            };
            column["compareRows"] = function(row1, row2) {
                const value1 = this.getRowValue(row1);
                const value2 = this.getRowValue(row2);
                if ((typeof(value1) === "number") && (typeof(value2) === "number"))
                    return compareNumbers(value1, value2);
                return window.qBittorrent.Misc.naturalSortCollator.compare(value1, value2);
            };
            column["updateTd"] = function(td, row) {
                const value = this.getRowValue(row);
                td.textContent = value;
                td.title = value;
            };
            column["onResize"] = null;
            this.columns.push(column);
            this.columns[name] = column;

            this.hiddenTableHeader.append(document.createElement("th"));
            this.fixedTableHeader.append(document.createElement("th"));
        }
    }

    class RssArticleTable extends DynamicTable {
        initColumns() {
            this.newColumn("name", "", "QBT_TR(Torrents: (double-click to download))QBT_TR[CONTEXT=RSSWidget]", -1, true);
        }
        setupHeaderMenu() {}
        setupHeaderEvents() {}
        getFilteredAndSortedRows() {
            return [...this.getRowValues()];
        }
        selectRow(rowId) {
            this.selectedRows.push(rowId);
            this.setRowClass();
            this.onSelectedRowChanged();

            let articleId = "";
            let feedUid = "";
            for (const row of this.getRowValues()) {
                if (row.rowId === rowId) {
                    articleId = row.full_data.dataId;
                    feedUid = row.full_data.feedUid;
                    this.tableBody.rows[row.rowId].classList.remove("unreadArticle");
                    break;
                }
            }
            window.qBittorrent.Rss.showDetails(feedUid, articleId);
        }

        setupCommonEvents() {
            super.setupCommonEvents();
            this.dynamicTableDiv.addEventListener("dblclick", (e) => {
                const tr = e.target.closest("tr");
                if (!tr)
                    return;

                const { name, torrentURL } = this.getRow(tr.rowId).full_data;
                qBittorrent.Client.createAddTorrentWindow(name, torrentURL);
            });
        }
        updateRow(tr, fullUpdate) {
            const row = this.rows.get(tr.rowId);
            tr.classList.toggle("unreadArticle", !row.full_data.isRead);

            return super.updateRow(tr, fullUpdate);
        }
        newColumn(name, style, caption, defaultWidth, defaultVisible) {
            const column = {};
            column["name"] = name;
            column["title"] = name;
            column["visible"] = defaultVisible;
            column["force_hide"] = false;
            column["caption"] = caption;
            column["style"] = style;
            if (defaultWidth !== -1)
                column["width"] = defaultWidth;

            column["dataProperties"] = [name];
            column["getRowValue"] = function(row, pos) {
                if (pos === undefined)
                    pos = 0;
                return row["full_data"][this.dataProperties[pos]];
            };
            column["compareRows"] = function(row1, row2) {
                const value1 = this.getRowValue(row1);
                const value2 = this.getRowValue(row2);
                if ((typeof(value1) === "number") && (typeof(value2) === "number"))
                    return compareNumbers(value1, value2);
                return window.qBittorrent.Misc.naturalSortCollator.compare(value1, value2);
            };
            column["updateTd"] = function(td, row) {
                const value = this.getRowValue(row);
                td.textContent = value;
                td.title = value;
            };
            column["onResize"] = null;
            this.columns.push(column);
            this.columns[name] = column;

            this.hiddenTableHeader.append(document.createElement("th"));
            this.fixedTableHeader.append(document.createElement("th"));
        }
    }

    class RssDownloaderRulesTable extends DynamicTable {
        initColumns() {
            this.newColumn("checked", "", "", 30, true);
            this.newColumn("name", "", "", -1, true);

            this.columns["checked"].updateTd = (td, row) => {
                if (document.getElementById(`cbRssDlRule${row.rowId}`) === null) {
                    const checkbox = document.createElement("input");
                    checkbox.type = "checkbox";
                    checkbox.id = `cbRssDlRule${row.rowId}`;
                    checkbox.checked = row.full_data.checked;

                    checkbox.addEventListener("click", function(e) {
                        window.qBittorrent.RssDownloader.rssDownloaderRulesTable.updateRowData({
                            rowId: row.rowId,
                            checked: this.checked
                        });
                        window.qBittorrent.RssDownloader.modifyRuleState(row.full_data.name, "enabled", this.checked);
                        e.stopPropagation();
                    });

                    td.append(checkbox);
                }
                else {
                    document.getElementById(`cbRssDlRule${row.rowId}`).checked = row.full_data.checked;
                }
            };
            this.columns["checked"].staticWidth = 50;
        }
        setupHeaderMenu() {}
        setupHeaderEvents() {}
        getFilteredAndSortedRows() {
            return [...this.getRowValues()];
        }

        setupCommonEvents() {
            super.setupCommonEvents();
            this.dynamicTableDiv.addEventListener("dblclick", (e) => {
                const tr = e.target.closest("tr");
                if (!tr)
                    return;

                window.qBittorrent.RssDownloader.renameRule(this.getRow(tr.rowId).full_data.name);
            });
        }
        newColumn(name, style, caption, defaultWidth, defaultVisible) {
            const column = {};
            column["name"] = name;
            column["title"] = name;
            column["visible"] = defaultVisible;
            column["force_hide"] = false;
            column["caption"] = caption;
            column["style"] = style;
            if (defaultWidth !== -1)
                column["width"] = defaultWidth;

            column["dataProperties"] = [name];
            column["getRowValue"] = function(row, pos) {
                if (pos === undefined)
                    pos = 0;
                return row["full_data"][this.dataProperties[pos]];
            };
            column["compareRows"] = function(row1, row2) {
                const value1 = this.getRowValue(row1);
                const value2 = this.getRowValue(row2);
                if ((typeof(value1) === "number") && (typeof(value2) === "number"))
                    return compareNumbers(value1, value2);
                return window.qBittorrent.Misc.naturalSortCollator.compare(value1, value2);
            };
            column["updateTd"] = function(td, row) {
                const value = this.getRowValue(row);
                td.textContent = value;
                td.title = value;
            };
            column["onResize"] = null;
            this.columns.push(column);
            this.columns[name] = column;

            this.hiddenTableHeader.append(document.createElement("th"));
            this.fixedTableHeader.append(document.createElement("th"));
        }
        selectRow(rowId) {
            this.selectedRows.push(rowId);
            this.setRowClass();
            this.onSelectedRowChanged();

            let name = "";
            for (const row of this.getRowValues()) {
                if (row.rowId === rowId) {
                    name = row.full_data.name;
                    break;
                }
            }
            window.qBittorrent.RssDownloader.showRule(name);
        }
    }

    class RssDownloaderFeedSelectionTable extends DynamicTable {
        initColumns() {
            this.newColumn("checked", "", "", 30, true);
            this.newColumn("name", "", "", -1, true);

            this.columns["checked"].updateTd = (td, row) => {
                if (document.getElementById(`cbRssDlFeed${row.rowId}`) === null) {
                    const checkbox = document.createElement("input");
                    checkbox.type = "checkbox";
                    checkbox.id = `cbRssDlFeed${row.rowId}`;
                    checkbox.checked = row.full_data.checked;

                    checkbox.addEventListener("click", function(e) {
                        window.qBittorrent.RssDownloader.rssDownloaderFeedSelectionTable.updateRowData({
                            rowId: row.rowId,
                            checked: this.checked
                        });
                        e.stopPropagation();
                    });

                    td.append(checkbox);
                }
                else {
                    document.getElementById(`cbRssDlFeed${row.rowId}`).checked = row.full_data.checked;
                }
            };
            this.columns["checked"].staticWidth = 50;
        }
        setupHeaderMenu() {}
        setupHeaderEvents() {}
        getFilteredAndSortedRows() {
            return [...this.getRowValues()];
        }
        newColumn(name, style, caption, defaultWidth, defaultVisible) {
            const column = {};
            column["name"] = name;
            column["title"] = name;
            column["visible"] = defaultVisible;
            column["force_hide"] = false;
            column["caption"] = caption;
            column["style"] = style;
            if (defaultWidth !== -1)
                column["width"] = defaultWidth;

            column["dataProperties"] = [name];
            column["getRowValue"] = function(row, pos) {
                if (pos === undefined)
                    pos = 0;
                return row["full_data"][this.dataProperties[pos]];
            };
            column["compareRows"] = function(row1, row2) {
                const value1 = this.getRowValue(row1);
                const value2 = this.getRowValue(row2);
                if ((typeof(value1) === "number") && (typeof(value2) === "number"))
                    return compareNumbers(value1, value2);
                return window.qBittorrent.Misc.naturalSortCollator.compare(value1, value2);
            };
            column["updateTd"] = function(td, row) {
                const value = this.getRowValue(row);
                td.textContent = value;
                td.title = value;
            };
            column["onResize"] = null;
            this.columns.push(column);
            this.columns[name] = column;

            this.hiddenTableHeader.append(document.createElement("th"));
            this.fixedTableHeader.append(document.createElement("th"));
        }
        selectRow() {}
    }

    class RssDownloaderArticlesTable extends DynamicTable {
        initColumns() {
            this.newColumn("name", "", "", -1, true);
        }
        setupHeaderMenu() {}
        setupHeaderEvents() {}
        getFilteredAndSortedRows() {
            return [...this.getRowValues()];
        }
        newColumn(name, style, caption, defaultWidth, defaultVisible) {
            const column = {};
            column["name"] = name;
            column["title"] = name;
            column["visible"] = defaultVisible;
            column["force_hide"] = false;
            column["caption"] = caption;
            column["style"] = style;
            if (defaultWidth !== -1)
                column["width"] = defaultWidth;

            column["dataProperties"] = [name];
            column["getRowValue"] = function(row, pos) {
                if (pos === undefined)
                    pos = 0;
                return row["full_data"][this.dataProperties[pos]];
            };
            column["compareRows"] = function(row1, row2) {
                const value1 = this.getRowValue(row1);
                const value2 = this.getRowValue(row2);
                if ((typeof(value1) === "number") && (typeof(value2) === "number"))
                    return compareNumbers(value1, value2);
                return window.qBittorrent.Misc.naturalSortCollator.compare(value1, value2);
            };
            column["updateTd"] = function(td, row) {
                const value = this.getRowValue(row);
                td.textContent = value;
                td.title = value;
            };
            column["onResize"] = null;
            this.columns.push(column);
            this.columns[name] = column;

            this.hiddenTableHeader.append(document.createElement("th"));
            this.fixedTableHeader.append(document.createElement("th"));
        }
        selectRow() {}
        updateRow(tr, fullUpdate) {
            const row = this.rows.get(tr.rowId);
            tr.classList.toggle("articleTableFeed", row.full_data.isFeed);
            tr.classList.toggle("articleTableArticle", !row.full_data.isFeed);

            return super.updateRow(tr, fullUpdate);
        }
    }

    class LogMessageTable extends DynamicTable {
        filterText = "";

        initColumns() {
            this.newColumn("rowId", "", "QBT_TR(ID)QBT_TR[CONTEXT=ExecutionLogWidget]", 50, true);
            this.newColumn("message", "", "QBT_TR(Message)QBT_TR[CONTEXT=ExecutionLogWidget]", 350, true);
            this.newColumn("timestamp", "", "QBT_TR(Timestamp)QBT_TR[CONTEXT=ExecutionLogWidget]", 150, true);
            this.newColumn("type", "", "QBT_TR(Log Type)QBT_TR[CONTEXT=ExecutionLogWidget]", 100, true);
            this.initColumnsFunctions();
        }

        initColumnsFunctions() {
            this.columns["timestamp"].updateTd = function(td, row) {
                const date = new Date(this.getRowValue(row) * 1000).toLocaleString();
                td.textContent = date;
                td.title = date;
            };

            this.columns["type"].updateTd = function(td, row) {
                // Type of the message: Log::NORMAL: 1, Log::INFO: 2, Log::WARNING: 4, Log::CRITICAL: 8
                let logLevel, addClass;
                switch (Number(this.getRowValue(row))) {
                    case 1:
                        logLevel = "QBT_TR(Normal)QBT_TR[CONTEXT=ExecutionLogWidget]";
                        addClass = "logNormal";
                        break;
                    case 2:
                        logLevel = "QBT_TR(Info)QBT_TR[CONTEXT=ExecutionLogWidget]";
                        addClass = "logInfo";
                        break;
                    case 4:
                        logLevel = "QBT_TR(Warning)QBT_TR[CONTEXT=ExecutionLogWidget]";
                        addClass = "logWarning";
                        break;
                    case 8:
                        logLevel = "QBT_TR(Critical)QBT_TR[CONTEXT=ExecutionLogWidget]";
                        addClass = "logCritical";
                        break;
                    default:
                        logLevel = "QBT_TR(Unknown)QBT_TR[CONTEXT=ExecutionLogWidget]";
                        addClass = "logUnknown";
                        break;
                }
                td.textContent = logLevel;
                td.title = logLevel;
                td.closest("tr").classList.add(`logTableRow${addClass}`);
            };
        }

        getFilteredAndSortedRows() {
            let filteredRows = [];
            this.filterText = window.qBittorrent.Log.getFilterText();
            const filterTerms = (this.filterText.length > 0) ? this.filterText.toLowerCase().split(" ") : [];
            const logLevels = window.qBittorrent.Log.getSelectedLevels();
            if ((filterTerms.length > 0) || (logLevels.length < 4)) {
                for (const row of this.getRowValues()) {
                    if (!logLevels.includes(row.full_data.type.toString()))
                        continue;

                    if ((filterTerms.length > 0) && !window.qBittorrent.Misc.containsAllTerms(row.full_data.message, filterTerms))
                        continue;

                    filteredRows.push(row);
                }
            }
            else {
                filteredRows = [...this.getRowValues()];
            }

            const column = this.columns[this.sortedColumn];
            const isReverseSort = (this.reverseSort === "0");
            filteredRows.sort((row1, row2) => {
                const result = column.compareRows(row1, row2);
                return isReverseSort ? result : -result;
            });

            this.filteredLength = filteredRows.length;

            return filteredRows;
        }
    }

    class LogPeerTable extends LogMessageTable {
        initColumns() {
            this.newColumn("rowId", "", "QBT_TR(ID)QBT_TR[CONTEXT=ExecutionLogWidget]", 50, true);
            this.newColumn("ip", "", "QBT_TR(IP)QBT_TR[CONTEXT=ExecutionLogWidget]", 150, true);
            this.newColumn("timestamp", "", "QBT_TR(Timestamp)QBT_TR[CONTEXT=ExecutionLogWidget]", 150, true);
            this.newColumn("blocked", "", "QBT_TR(Status)QBT_TR[CONTEXT=ExecutionLogWidget]", 150, true);
            this.newColumn("reason", "", "QBT_TR(Reason)QBT_TR[CONTEXT=ExecutionLogWidget]", 150, true);

            this.columns["timestamp"].updateTd = function(td, row) {
                const date = new Date(this.getRowValue(row) * 1000).toLocaleString();
                td.textContent = date;
                td.title = date;
            };

            this.columns["blocked"].updateTd = function(td, row) {
                let status, addClass;
                if (this.getRowValue(row)) {
                    status = "QBT_TR(Blocked)QBT_TR[CONTEXT=ExecutionLogWidget]";
                    addClass = "peerBlocked";
                }
                else {
                    status = "QBT_TR(Banned)QBT_TR[CONTEXT=ExecutionLogWidget]";
                    addClass = "peerBanned";
                }
                td.textContent = status;
                td.title = status;
                td.closest("tr").classList.add(`logTableRow${addClass}`);
            };
        }

        getFilteredAndSortedRows() {
            let filteredRows = [];
            this.filterText = window.qBittorrent.Log.getFilterText();
            const filterTerms = (this.filterText.length > 0) ? this.filterText.toLowerCase().split(" ") : [];
            if (filterTerms.length > 0) {
                for (const row of this.getRowValues()) {
                    if ((filterTerms.length > 0) && !window.qBittorrent.Misc.containsAllTerms(row.full_data.ip, filterTerms))
                        continue;

                    filteredRows.push(row);
                }
            }
            else {
                filteredRows = [...this.getRowValues()];
            }

            const column = this.columns[this.sortedColumn];
            const isReverseSort = (this.reverseSort === "0");
            filteredRows.sort((row1, row2) => {
                const result = column.compareRows(row1, row2);
                return isReverseSort ? result : -result;
            });

            return filteredRows;
        }
    }

    class TorrentWebseedsTable extends DynamicTable {
        initColumns() {
            this.newColumn("url", "", "QBT_TR(URL)QBT_TR[CONTEXT=HttpServer]", 500, true);
        }
    }

    class TorrentCreationTasksTable extends DynamicTable {
        initColumns() {
            this.newColumn("state_icon", "", "QBT_TR(Status Icon)QBT_TR[CONTEXT=TorrentCreator]", 30, false);
            this.newColumn("source_path", "", "QBT_TR(Source Path)QBT_TR[CONTEXT=TorrentCreator]", 200, true);
            this.newColumn("progress", "", "QBT_TR(Progress)QBT_TR[CONTEXT=TorrentCreator]", 85, true);
            this.newColumn("status", "", "QBT_TR(Status)QBT_TR[CONTEXT=TorrentCreator]", 100, true);
            this.newColumn("torrent_format", "", "QBT_TR(Format)QBT_TR[CONTEXT=TorrentCreator]", 100, true);
            this.newColumn("piece_size", "", "QBT_TR(Piece Size)QBT_TR[CONTEXT=TorrentCreator]", 100, true);
            this.newColumn("private", "", "QBT_TR(Private)QBT_TR[CONTEXT=TorrentCreator]", 30, true);
            this.newColumn("added_on", "", "QBT_TR(Added On)QBT_TR[CONTEXT=TorrentCreator]", 100, true);
            this.newColumn("start_on", "", "QBT_TR(Started On)QBT_TR[CONTEXT=TorrentCreator]", 100, true);
            this.newColumn("completion_on", "", "QBT_TR(Completed On)QBT_TR[CONTEXT=TorrentCreator]", 100, true);
            this.newColumn("trackers", "", "QBT_TR(Trackers)QBT_TR[CONTEXT=TorrentCreator]", 100, true);
            this.newColumn("web_seeds", "", "QBT_TR(Web Seeds)QBT_TR[CONTEXT=TorrentCreator]", 100, false);
            this.newColumn("comment", "", "QBT_TR(Comment)QBT_TR[CONTEXT=TorrentCreator]", 100, true);
            this.newColumn("source", "", "QBT_TR(Source)QBT_TR[CONTEXT=TorrentCreator]", 100, false);
            this.newColumn("error_message", "", "QBT_TR(Error Message)QBT_TR[CONTEXT=TorrentCreator]", 100, true);

            this.columns["state_icon"].dataProperties[0] = "status";
            this.columns["source_path"].dataProperties.push("status");

            this.initColumnsFunctions();
        }

        initColumnsFunctions() {
            const getStateIconClasses = (state) => {
                let stateClass = "stateUnknown";
                // normalize states
                switch (state) {
                    case "Running":
                        stateClass = "stateRunning";
                        break;
                    case "Queued":
                        stateClass = "stateQueued";
                        break;
                    case "Finished":
                        stateClass = "stateStoppedUP";
                        break;
                    case "Failed":
                        stateClass = "stateError";
                        break;
                }

                return `stateIcon ${stateClass}`;
            };

            // state_icon
            this.columns["state_icon"].updateTd = function(td, row) {
                const state = this.getRowValue(row);
                let div = td.firstElementChild;
                if (div === null) {
                    div = document.createElement("div");
                    td.append(div);
                }

                div.className = `${getStateIconClasses(state)} stateIconColumn`;
            };

            this.columns["state_icon"].onVisibilityChange = (columnName) => {
                // show state icon in name column only when standalone
                // state icon column is hidden
                this.updateColumn("name", true);
            };

            // source_path
            this.columns["source_path"].updateTd = function(td, row) {
                const name = this.getRowValue(row, 0);
                const state = this.getRowValue(row, 1);
                let span = td.firstElementChild;
                if (span === null) {
                    span = document.createElement("span");
                    td.append(span);
                }

                span.className = this.isStateIconShown() ? getStateIconClasses(state) : "";
                span.textContent = name;
                td.title = name;
            };

            this.columns["source_path"].isStateIconShown = () => !this.columns["state_icon"].isVisible();

            // status
            this.columns["status"].updateTd = function(td, row) {
                const state = this.getRowValue(row);
                if (!state)
                    return;

                let status = "QBT_TR(Unknown)QBT_TR[CONTEXT=HttpServer]";
                switch (state) {
                    case "Queued":
                        status = "QBT_TR(Queued)QBT_TR[CONTEXT=TorrentCreator]";
                        break;
                    case "Running":
                        status = "QBT_TR(Running)QBT_TR[CONTEXT=TorrentCreator]";
                        break;
                    case "Finished":
                        status = "QBT_TR(Finished)QBT_TR[CONTEXT=TorrentCreator]";
                        break;
                    case "Failed":
                        status = "QBT_TR(Failed)QBT_TR[CONTEXT=TorrentCreator]";
                        break;
                }

                td.textContent = status;
                td.title = status;
            };

            // torrent_format
            this.columns["torrent_format"].updateTd = function(td, row) {
                const torrentFormat = this.getRowValue(row);
                if (!torrentFormat)
                    return;

                let format = "QBT_TR(Unknown)QBT_TR[CONTEXT=HttpServer]";
                switch (torrentFormat) {
                    case "v1":
                        format = "V1";
                        break;
                    case "v2":
                        format = "V2";
                        break;
                    case "hybrid":
                        format = "QBT_TR(Hybrid)QBT_TR[CONTEXT=TorrentCreator]";
                        break;
                }

                td.textContent = format;
                td.title = format;
            };

            // progress
            this.columns["progress"].updateTd = function(td, row) {
                const progress = this.getRowValue(row);

                const div = td.firstElementChild;
                if (div !== null) {
                    if (div.getValue() !== progress)
                        div.setValue(progress);
                }
                else {
                    td.append(new window.qBittorrent.ProgressBar.ProgressBar(progress));
                }
            };
            this.columns["progress"].staticWidth = 100;

            // piece_size
            this.columns["piece_size"].updateTd = function(td, row) {
                const pieceSize = this.getRowValue(row);
                const size = (pieceSize === 0) ? "QBT_TR(N/A)QBT_TR[CONTEXT=TorrentCreator]" : window.qBittorrent.Misc.friendlyUnit(pieceSize, false);
                td.textContent = size;
                td.title = size;
            };

            // private
            this.columns["private"].updateTd = function(td, row) {
                const isPrivate = this.getRowValue(row);
                const string = isPrivate
                    ? "QBT_TR(Yes)QBT_TR[CONTEXT=PropertiesWidget]"
                    : "QBT_TR(No)QBT_TR[CONTEXT=PropertiesWidget]";
                td.textContent = string;
                td.title = string;
            };

            const displayDate = function(td, row) {
                const val = this.getRowValue(row);
                if (!val) {
                    td.textContent = "";
                    td.title = "";
                }
                else {
                    const date = new Date(val).toLocaleString();
                    td.textContent = date;
                    td.title = date;
                }
            };

            // added_on, start_on, completion_on
            this.columns["added_on"].updateTd = displayDate;
            this.columns["start_on"].updateTd = displayDate;
            this.columns["completion_on"].updateTd = displayDate;
        }

        setupCommonEvents() {
            super.setupCommonEvents();
            this.dynamicTableDiv.addEventListener("dblclick", (e) => {
                const tr = e.target.closest("tr");
                if (!tr)
                    return;

                this.deselectAll();
                this.selectRow(tr.rowId);

                window.qBittorrent.TorrentCreator.exportTorrents();
            });
        }
    }

    return exports();
})();
Object.freeze(window.qBittorrent.DynamicTable);

/*************************************************************/
