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

'use strict';

if (window.qBittorrent === undefined) {
    window.qBittorrent = {};
}

window.qBittorrent.DynamicTable = (function() {
    const exports = function() {
        return {
            TorrentsTable: TorrentsTable,
            TorrentPeersTable: TorrentPeersTable,
            SearchResultsTable: SearchResultsTable,
            SearchPluginsTable: SearchPluginsTable,
            TorrentTrackersTable: TorrentTrackersTable,
            TorrentFilesTable: TorrentFilesTable
        };
    };

    let DynamicTableHeaderContextMenuClass = null;
    let ProgressColumnWidth = -1;

    const DynamicTable = new Class({

        initialize: function() {},

        setup: function(dynamicTableDivId, dynamicTableFixedHeaderDivId, contextMenu) {
            this.dynamicTableDivId = dynamicTableDivId;
            this.dynamicTableFixedHeaderDivId = dynamicTableFixedHeaderDivId;
            this.fixedTableHeader = $(dynamicTableFixedHeaderDivId).getElements('tr')[0];
            this.hiddenTableHeader = $(dynamicTableDivId).getElements('tr')[0];
            this.tableBody = $(dynamicTableDivId).getElements('tbody')[0];
            this.rows = new Hash();
            this.selectedRows = [];
            this.columns = [];
            this.contextMenu = contextMenu;
            this.sortedColumn = LocalPreferences.get('sorted_column_' + this.dynamicTableDivId, 0);
            this.reverseSort = LocalPreferences.get('reverse_sort_' + this.dynamicTableDivId, '0');
            this.initColumns();
            this.loadColumnsOrder();
            this.updateTableHeaders();
            this.setupCommonEvents();
            this.setupHeaderEvents();
            this.setupHeaderMenu();
            this.setSortedColumnIcon(this.sortedColumn, null, (this.reverseSort === '1'));
        },

        setupCommonEvents: function() {
            const scrollFn = function() {
                $(this.dynamicTableFixedHeaderDivId).getElements('table')[0].style.left = -$(this.dynamicTableDivId).scrollLeft + 'px';
            }.bind(this);

            $(this.dynamicTableDivId).addEvent('scroll', scrollFn);

            // if the table exists within a panel
            if ($(this.dynamicTableDivId).getParent('.panel')) {
                const resizeFn = function() {
                    const panel = $(this.dynamicTableDivId).getParent('.panel');
                    let h = panel.getBoundingClientRect().height - $(this.dynamicTableFixedHeaderDivId).getBoundingClientRect().height;
                    $(this.dynamicTableDivId).style.height = h + 'px';

                    // Workaround due to inaccurate calculation of elements heights by browser

                    let n = 2;

                    while (panel.clientWidth != panel.offsetWidth && n > 0) { // is panel vertical scrollbar visible ?
                        --n;
                        h -= 0.5;
                        $(this.dynamicTableDivId).style.height = h + 'px';
                    }

                    this.lastPanelHeight = panel.getBoundingClientRect().height;
                }.bind(this);

                $(this.dynamicTableDivId).getParent('.panel').addEvent('resize', resizeFn);

                this.lastPanelHeight = 0;

                // Workaround. Resize event is called not always (for example it isn't called when browser window changes it's size)

                const checkResizeFn = function() {
                    const panel = $(this.dynamicTableDivId).getParent('.panel');
                    if (this.lastPanelHeight != panel.getBoundingClientRect().height) {
                        this.lastPanelHeight = panel.getBoundingClientRect().height;
                        panel.fireEvent('resize');
                    }
                }.bind(this);

                setInterval(checkResizeFn, 500);
            }
        },

        setupHeaderEvents: function() {
            this.currentHeaderAction = '';
            this.canResize = false;

            const resetElementBorderStyle = function(el, side) {
                if (side === 'left' || side !== 'right') {
                    el.setStyle('border-left-style', '');
                    el.setStyle('border-left-color', '');
                    el.setStyle('border-left-width', '');
                }
                if (side === 'right' || side !== 'left') {
                    el.setStyle('border-right-style', '');
                    el.setStyle('border-right-color', '');
                    el.setStyle('border-right-width', '');
                }
            };

            const mouseMoveFn = function(e) {
                const brect = e.target.getBoundingClientRect();
                const mouseXRelative = e.event.clientX - brect.left;
                if (this.currentHeaderAction === '') {
                    if (brect.width - mouseXRelative < 5) {
                        this.resizeTh = e.target;
                        this.canResize = true;
                        e.target.getParent("tr").style.cursor = 'col-resize';
                    }
                    else if ((mouseXRelative < 5) && e.target.getPrevious('[class=""]')) {
                        this.resizeTh = e.target.getPrevious('[class=""]');
                        this.canResize = true;
                        e.target.getParent("tr").style.cursor = 'col-resize';
                    }
                    else {
                        this.canResize = false;
                        e.target.getParent("tr").style.cursor = '';
                    }
                }
                if (this.currentHeaderAction === 'drag') {
                    const previousVisibleSibling = e.target.getPrevious('[class=""]');
                    let borderChangeElement = previousVisibleSibling;
                    let changeBorderSide = 'right';

                    if (mouseXRelative > brect.width / 2) {
                        borderChangeElement = e.target;
                        this.dropSide = 'right';
                    }
                    else {
                        this.dropSide = 'left';
                    }

                    e.target.getParent("tr").style.cursor = 'move';

                    if (!previousVisibleSibling) { // right most column
                        borderChangeElement = e.target;

                        if (mouseXRelative <= brect.width / 2)
                            changeBorderSide = 'left';
                    }

                    borderChangeElement.setStyle('border-' + changeBorderSide + '-style', 'solid');
                    borderChangeElement.setStyle('border-' + changeBorderSide + '-color', '#e60');
                    borderChangeElement.setStyle('border-' + changeBorderSide + '-width', 'initial');

                    resetElementBorderStyle(borderChangeElement, changeBorderSide === 'right' ? 'left' : 'right');

                    borderChangeElement.getSiblings('[class=""]').each(function(el) {
                        resetElementBorderStyle(el);
                    });
                }
                this.lastHoverTh = e.target;
                this.lastClientX = e.event.clientX;
            }.bind(this);

            const mouseOutFn = function(e) {
                resetElementBorderStyle(e.target);
            }.bind(this);

            const onBeforeStart = function(el) {
                this.clickedTh = el;
                this.currentHeaderAction = 'start';
                this.dragMovement = false;
                this.dragStartX = this.lastClientX;
            }.bind(this);

            const onStart = function(el, event) {
                if (this.canResize) {
                    this.currentHeaderAction = 'resize';
                    this.startWidth = this.resizeTh.getStyle('width').toFloat();
                }
                else {
                    this.currentHeaderAction = 'drag';
                    el.setStyle('background-color', '#C1D5E7');
                }
            }.bind(this);

            const onDrag = function(el, event) {
                if (this.currentHeaderAction === 'resize') {
                    let width = this.startWidth + (event.page.x - this.dragStartX);
                    if (width < 16)
                        width = 16;
                    this.columns[this.resizeTh.columnName].width = width;
                    this.updateColumn(this.resizeTh.columnName);
                }
            }.bind(this);

            const onComplete = function(el, event) {
                resetElementBorderStyle(this.lastHoverTh);
                el.setStyle('background-color', '');
                if (this.currentHeaderAction === 'resize')
                    LocalPreferences.set('column_' + this.resizeTh.columnName + '_width_' + this.dynamicTableDivId, this.columns[this.resizeTh.columnName].width);
                if ((this.currentHeaderAction === 'drag') && (el !== this.lastHoverTh)) {
                    this.saveColumnsOrder();
                    const val = LocalPreferences.get('columns_order_' + this.dynamicTableDivId).split(',');
                    val.erase(el.columnName);
                    let pos = val.indexOf(this.lastHoverTh.columnName);
                    if (this.dropSide === 'right') ++pos;
                    val.splice(pos, 0, el.columnName);
                    LocalPreferences.set('columns_order_' + this.dynamicTableDivId, val.join(','));
                    this.loadColumnsOrder();
                    this.updateTableHeaders();
                    while (this.tableBody.firstChild)
                        this.tableBody.removeChild(this.tableBody.firstChild);
                    this.updateTable(true);
                }
                if (this.currentHeaderAction === 'drag') {
                    resetElementBorderStyle(el);
                    el.getSiblings('[class=""]').each(function(el) {
                        resetElementBorderStyle(el);
                    });
                }
                this.currentHeaderAction = '';
            }.bind(this);

            const onCancel = function(el) {
                this.currentHeaderAction = '';
                this.setSortedColumn(el.columnName);
            }.bind(this);

            const ths = this.fixedTableHeader.getElements('th');

            for (let i = 0; i < ths.length; ++i) {
                const th = ths[i];
                th.addEvent('mousemove', mouseMoveFn);
                th.addEvent('mouseout', mouseOutFn);
                th.makeResizable({
                    modifiers: {
                        x: '',
                        y: ''
                    },
                    onBeforeStart: onBeforeStart,
                    onStart: onStart,
                    onDrag: onDrag,
                    onComplete: onComplete,
                    onCancel: onCancel
                });
            }
        },

        setupDynamicTableHeaderContextMenuClass: function() {
            if (!DynamicTableHeaderContextMenuClass) {
                DynamicTableHeaderContextMenuClass = new Class({
                    Extends: window.qBittorrent.ContextMenu.ContextMenu,
                    updateMenuItems: function() {
                        for (let i = 0; i < this.dynamicTable.columns.length; ++i) {
                            if (this.dynamicTable.columns[i].caption === '')
                                continue;
                            if (this.dynamicTable.columns[i].visible !== '0')
                                this.setItemChecked(this.dynamicTable.columns[i].name, true);
                            else
                                this.setItemChecked(this.dynamicTable.columns[i].name, false);
                        }
                    }
                });
            }
        },

        showColumn: function(columnName, show) {
            this.columns[columnName].visible = show ? '1' : '0';
            LocalPreferences.set('column_' + columnName + '_visible_' + this.dynamicTableDivId, show ? '1' : '0');
            this.updateColumn(columnName);
        },

        setupHeaderMenu: function() {
            this.setupDynamicTableHeaderContextMenuClass();

            const menuId = this.dynamicTableDivId + '_headerMenu';

            const ul = new Element('ul', {
                id: menuId,
                class: 'contextMenu scrollableMenu'
            });

            const createLi = function(columnName, text) {
                const html = '<a href="#' + columnName + '" ><img src="images/qbt-theme/checked.svg"/>' + window.qBittorrent.Misc.escapeHtml(text) + '</a>';
                return new Element('li', {
                    html: html
                });
            };

            const actions = {};

            const onMenuItemClicked = function(element, ref, action) {
                this.showColumn(action, this.columns[action].visible === '0');
            }.bind(this);

            for (let i = 0; i < this.columns.length; ++i) {
                const text = this.columns[i].caption;
                if (text === '')
                    continue;
                ul.appendChild(createLi(this.columns[i].name, text));
                actions[this.columns[i].name] = onMenuItemClicked;
            }

            ul.inject(document.body);

            this.headerContextMenu = new DynamicTableHeaderContextMenuClass({
                targets: '#' + this.dynamicTableFixedHeaderDivId + ' tr',
                actions: actions,
                menu: menuId,
                offsets: {
                    x: -15,
                    y: 2
                }
            });

            this.headerContextMenu.dynamicTable = this;
        },

        initColumns: function() {},

        newColumn: function(name, style, caption, defaultWidth, defaultVisible) {
            const column = {};
            column['name'] = name;
            column['title'] = name;
            column['visible'] = LocalPreferences.get('column_' + name + '_visible_' + this.dynamicTableDivId, defaultVisible ? '1' : '0');
            column['force_hide'] = false;
            column['caption'] = caption;
            column['style'] = style;
            column['width'] = LocalPreferences.get('column_' + name + '_width_' + this.dynamicTableDivId, defaultWidth);
            column['dataProperties'] = [name];
            column['getRowValue'] = function(row, pos) {
                if (pos === undefined)
                    pos = 0;
                return row['full_data'][this.dataProperties[pos]];
            };
            column['compareRows'] = function(row1, row2) {
                if (this.getRowValue(row1) < this.getRowValue(row2))
                    return -1;
                else if (this.getRowValue(row1) > this.getRowValue(row2))
                    return 1;
                else return 0;
            };
            column['updateTd'] = function(td, row) {
                const value = this.getRowValue(row)
                td.set('text', value);
                td.set('title', value);
            };
            column['onResize'] = null;
            this.columns.push(column);
            this.columns[name] = column;

            this.hiddenTableHeader.appendChild(new Element('th'));
            this.fixedTableHeader.appendChild(new Element('th'));
        },

        loadColumnsOrder: function() {
            const columnsOrder = [];
            const val = LocalPreferences.get('columns_order_' + this.dynamicTableDivId);
            if (val === null || val === undefined) return;
            val.split(',').forEach(function(v) {
                if ((v in this.columns) && (!columnsOrder.contains(v)))
                    columnsOrder.push(v);
            }.bind(this));

            for (let i = 0; i < this.columns.length; ++i)
                if (!columnsOrder.contains(this.columns[i].name))
                    columnsOrder.push(this.columns[i].name);

            for (let i = 0; i < this.columns.length; ++i)
                this.columns[i] = this.columns[columnsOrder[i]];
        },

        saveColumnsOrder: function() {
            let val = '';
            for (let i = 0; i < this.columns.length; ++i) {
                if (i > 0)
                    val += ',';
                val += this.columns[i].name;
            }
            LocalPreferences.set('columns_order_' + this.dynamicTableDivId, val);
        },

        updateTableHeaders: function() {
            this.updateHeader(this.hiddenTableHeader);
            this.updateHeader(this.fixedTableHeader);
        },

        updateHeader: function(header) {
            const ths = header.getElements('th');

            for (let i = 0; i < ths.length; ++i) {
                const th = ths[i];
                th._this = this;
                th.setAttribute('title', this.columns[i].caption);
                th.set('text', this.columns[i].caption);
                th.setAttribute('style', 'width: ' + this.columns[i].width + 'px;' + this.columns[i].style);
                th.columnName = this.columns[i].name;
                th.addClass('column_' + th.columnName);
                if ((this.columns[i].visible == '0') || this.columns[i].force_hide)
                    th.addClass('invisible');
                else
                    th.removeClass('invisible');
            }
        },

        getColumnPos: function(columnName) {
            for (let i = 0; i < this.columns.length; ++i)
                if (this.columns[i].name == columnName)
                    return i;
            return -1;
        },

        updateColumn: function(columnName) {
            const pos = this.getColumnPos(columnName);
            const visible = ((this.columns[pos].visible != '0') && !this.columns[pos].force_hide);
            const ths = this.hiddenTableHeader.getElements('th');
            const fths = this.fixedTableHeader.getElements('th');
            const trs = this.tableBody.getElements('tr');
            const style = 'width: ' + this.columns[pos].width + 'px;' + this.columns[pos].style;

            ths[pos].setAttribute('style', style);
            fths[pos].setAttribute('style', style);

            if (visible) {
                ths[pos].removeClass('invisible');
                fths[pos].removeClass('invisible');
                for (let i = 0; i < trs.length; ++i)
                    trs[i].getElements('td')[pos].removeClass('invisible');
            }
            else {
                ths[pos].addClass('invisible');
                fths[pos].addClass('invisible');
                for (let j = 0; j < trs.length; ++j)
                    trs[j].getElements('td')[pos].addClass('invisible');
            }
            if (this.columns[pos].onResize !== null) {
                this.columns[pos].onResize(columnName);
            }
        },

        getSortedColumn: function() {
            return LocalPreferences.get('sorted_column_' + this.dynamicTableDivId);
        },

        setSortedColumn: function(column) {
            if (column != this.sortedColumn) {
                const oldColumn = this.sortedColumn;
                this.sortedColumn = column;
                this.reverseSort = '0';
                this.setSortedColumnIcon(column, oldColumn, false);
            }
            else {
                // Toggle sort order
                this.reverseSort = this.reverseSort === '0' ? '1' : '0';
                this.setSortedColumnIcon(column, null, (this.reverseSort === '1'));
            }
            LocalPreferences.set('sorted_column_' + this.dynamicTableDivId, column);
            LocalPreferences.set('reverse_sort_' + this.dynamicTableDivId, this.reverseSort);
            this.updateTable(false);
        },

        setSortedColumnIcon: function(newColumn, oldColumn, isReverse) {
            const getCol = function(headerDivId, colName) {
                const colElem = $$("#" + headerDivId + " .column_" + colName);
                if (colElem.length == 1)
                    return colElem[0];
                return null;
            };

            const colElem = getCol(this.dynamicTableFixedHeaderDivId, newColumn);
            if (colElem !== null) {
                colElem.addClass('sorted');
                if (isReverse)
                    colElem.addClass('reverse');
                else
                    colElem.removeClass('reverse');
            }
            const oldColElem = getCol(this.dynamicTableFixedHeaderDivId, oldColumn);
            if (oldColElem !== null) {
                oldColElem.removeClass('sorted');
                oldColElem.removeClass('reverse');
            }
        },

        getSelectedRowId: function() {
            if (this.selectedRows.length > 0)
                return this.selectedRows[0];
            return '';
        },

        isRowSelected: function(rowId) {
            return this.selectedRows.contains(rowId);
        },

        altRow: function() {
            if (!MUI.ieLegacySupport)
                return;

            const trs = this.tableBody.getElements('tr');
            trs.each(function(el, i) {
                if (i % 2) {
                    el.addClass('alt');
                }
                else {
                    el.removeClass('alt');
                }
            }.bind(this));
        },

        selectAll: function() {
            this.deselectAll();

            const trs = this.tableBody.getElements('tr');
            for (let i = 0; i < trs.length; ++i) {
                const tr = trs[i];
                this.selectedRows.push(tr.rowId);
                if (!tr.hasClass('selected'))
                    tr.addClass('selected');
            }
        },

        deselectAll: function() {
            this.selectedRows.empty();
        },

        selectRow: function(rowId) {
            this.selectedRows.push(rowId);
            this.setRowClass();
            this.onSelectedRowChanged();
        },

        deselectRow: function(rowId) {
            this.selectedRows.erase(rowId);
            this.setRowClass();
            this.onSelectedRowChanged();
        },

        selectRows: function(rowId1, rowId2) {
            this.deselectAll();
            if (rowId1 === rowId2) {
                this.selectRow(rowId1);
                return;
            }

            let select = false;
            const that = this;
            this.tableBody.getElements('tr').each(function(tr) {
                if ((tr.rowId == rowId1) || (tr.rowId == rowId2)) {
                    select = !select;
                    that.selectedRows.push(tr.rowId);
                }
                else if (select) {
                    that.selectedRows.push(tr.rowId);
                }
            });
            this.setRowClass();
            this.onSelectedRowChanged();
        },

        reselectRows: function(rowIds) {
            this.deselectAll();
            this.selectedRows = rowIds.slice();
            this.tableBody.getElements('tr').each(function(tr) {
                if (rowIds.indexOf(tr.rowId) > -1)
                    tr.addClass('selected');
            });
        },

        setRowClass: function() {
            const that = this;
            this.tableBody.getElements('tr').each(function(tr) {
                if (that.isRowSelected(tr.rowId))
                    tr.addClass('selected');
                else
                    tr.removeClass('selected');
            });
        },

        onSelectedRowChanged: function() {},

        updateRowData: function(data) {
            const rowId = data['rowId'];
            let row;

            if (!this.rows.has(rowId)) {
                row = {};
                this.rows.set(rowId, row);
                row['full_data'] = {};
                row['rowId'] = rowId;
            }
            else
                row = this.rows.get(rowId);

            row['data'] = data;

            for (const x in data)
                row['full_data'][x] = data[x];
        },

        getFilteredAndSortedRows: function() {
            const filteredRows = [];

            const rows = this.rows.getValues();

            for (let i = 0; i < rows.length; ++i) {
                filteredRows.push(rows[i]);
                filteredRows[rows[i].rowId] = rows[i];
            }

            filteredRows.sort(function(row1, row2) {
                const column = this.columns[this.sortedColumn];
                const res = column.compareRows(row1, row2);
                if (this.reverseSort === '0')
                    return res;
                else
                    return -res;
            }.bind(this));
            return filteredRows;
        },

        getTrByRowId: function(rowId) {
            const trs = this.tableBody.getElements('tr');
            for (let i = 0; i < trs.length; ++i)
                if (trs[i].rowId == rowId)
                    return trs[i];
            return null;
        },

        updateTable: function(fullUpdate) {
            if (fullUpdate === undefined)
                fullUpdate = false;

            const rows = this.getFilteredAndSortedRows();

            for (let i = 0; i < this.selectedRows.length; ++i)
                if (!(this.selectedRows[i] in rows)) {
                    this.selectedRows.splice(i, 1);
                    --i;
                }

            const trs = this.tableBody.getElements('tr');

            for (let rowPos = 0; rowPos < rows.length; ++rowPos) {
                const rowId = rows[rowPos]['rowId'];
                let tr_found = false;
                for (let j = rowPos; j < trs.length; ++j)
                    if (trs[j]['rowId'] == rowId) {
                        tr_found = true;
                        if (rowPos == j)
                            break;
                        trs[j].inject(trs[rowPos], 'before');
                        const tmpTr = trs[j];
                        trs.splice(j, 1);
                        trs.splice(rowPos, 0, tmpTr);
                        break;
                    }
                if (tr_found) // row already exists in the table
                    this.updateRow(trs[rowPos], fullUpdate);
                else { // else create a new row in the table
                    const tr = new Element('tr');

                    tr['rowId'] = rows[rowPos]['rowId'];

                    tr._this = this;
                    tr.addEvent('contextmenu', function(e) {
                        if (!this._this.isRowSelected(this.rowId)) {
                            this._this.deselectAll();
                            this._this.selectRow(this.rowId);
                        }
                        return true;
                    });
                    tr.addEvent('click', function(e) {
                        e.stop();
                        if (e.control || e.meta) {
                            // CTRL/CMD ⌘ key was pressed
                            if (this._this.isRowSelected(this.rowId))
                                this._this.deselectRow(this.rowId);
                            else
                                this._this.selectRow(this.rowId);
                        }
                        else if (e.shift && (this._this.selectedRows.length == 1)) {
                            // Shift key was pressed
                            this._this.selectRows(this._this.getSelectedRowId(), this.rowId);
                        }
                        else {
                            // Simple selection
                            this._this.deselectAll();
                            this._this.selectRow(this.rowId);
                        }
                        return false;
                    });
                    tr.addEvent('touchstart', function(e) {
                        if (!this._this.isRowSelected(this.rowId)) {
                            this._this.deselectAll();
                            this._this.selectRow(this.rowId);
                        }
                        return false;
                    });

                    this.setupTr(tr);

                    for (let k = 0; k < this.columns.length; ++k) {
                        const td = new Element('td');
                        if ((this.columns[k].visible == '0') || this.columns[k].force_hide)
                            td.addClass('invisible');
                        td.injectInside(tr);
                    }

                    // Insert
                    if (rowPos >= trs.length) {
                        tr.inject(this.tableBody);
                        trs.push(tr);
                    }
                    else {
                        tr.inject(trs[rowPos], 'before');
                        trs.splice(rowPos, 0, tr);
                    }

                    // Update context menu
                    if (this.contextMenu)
                        this.contextMenu.addTarget(tr);

                    this.updateRow(tr, true);
                }
            }

            let rowPos = rows.length;

            while ((rowPos < trs.length) && (trs.length > 0)) {
                trs[trs.length - 1].dispose();
                trs.pop();
            }
        },

        setupTr: function(tr) {},

        updateRow: function(tr, fullUpdate) {
            const row = this.rows.get(tr.rowId);
            const data = row[fullUpdate ? 'full_data' : 'data'];

            const tds = tr.getElements('td');
            for (let i = 0; i < this.columns.length; ++i) {
                if (data.hasOwnProperty(this.columns[i].dataProperties[0]))
                    this.columns[i].updateTd(tds[i], row);
            }
            row['data'] = {};
        },

        removeRow: function(rowId) {
            this.selectedRows.erase(rowId);
            const tr = this.getTrByRowId(rowId);
            if (tr !== null) {
                tr.dispose();
                this.rows.erase(rowId);
                return true;
            }
            return false;
        },

        clear: function() {
            this.deselectAll();
            this.rows.empty();
            const trs = this.tableBody.getElements('tr');
            while (trs.length > 0) {
                trs[trs.length - 1].dispose();
                trs.pop();
            }
        },

        selectedRowsIds: function() {
            return this.selectedRows.slice();
        },

        getRowIds: function() {
            return this.rows.getKeys();
        },
    });

    const TorrentsTable = new Class({
        Extends: DynamicTable,

        initColumns: function() {
            this.newColumn('priority', '', '#', 30, true);
            this.newColumn('state_icon', 'cursor: default', '', 22, true);
            this.newColumn('name', '', 'QBT_TR(Name)QBT_TR[CONTEXT=TransferListModel]', 200, true);
            this.newColumn('size', '', 'QBT_TR(Size)QBT_TR[CONTEXT=TransferListModel]', 100, true);
            this.newColumn('total_size', '', 'QBT_TR(Total Size)QBT_TR[CONTEXT=TransferListModel]', 100, false);
            this.newColumn('progress', '', 'QBT_TR(Done)QBT_TR[CONTEXT=TransferListModel]', 85, true);
            this.newColumn('status', '', 'QBT_TR(Status)QBT_TR[CONTEXT=TransferListModel]', 100, true);
            this.newColumn('num_seeds', '', 'QBT_TR(Seeds)QBT_TR[CONTEXT=TransferListModel]', 100, true);
            this.newColumn('num_leechs', '', 'QBT_TR(Peers)QBT_TR[CONTEXT=TransferListModel]', 100, true);
            this.newColumn('dlspeed', '', 'QBT_TR(Down Speed)QBT_TR[CONTEXT=TransferListModel]', 100, true);
            this.newColumn('upspeed', '', 'QBT_TR(Up Speed)QBT_TR[CONTEXT=TransferListModel]', 100, true);
            this.newColumn('eta', '', 'QBT_TR(ETA)QBT_TR[CONTEXT=TransferListModel]', 100, true);
            this.newColumn('ratio', '', 'QBT_TR(Ratio)QBT_TR[CONTEXT=TransferListModel]', 100, true);
            this.newColumn('category', '', 'QBT_TR(Category)QBT_TR[CONTEXT=TransferListModel]', 100, true);
            this.newColumn('tags', '', 'QBT_TR(Tags)QBT_TR[CONTEXT=TransferListModel]', 100, true);
            this.newColumn('added_on', '', 'QBT_TR(Added On)QBT_TR[CONTEXT=TransferListModel]', 100, true);
            this.newColumn('completion_on', '', 'QBT_TR(Completed On)QBT_TR[CONTEXT=TransferListModel]', 100, false);
            this.newColumn('tracker', '', 'QBT_TR(Tracker)QBT_TR[CONTEXT=TransferListModel]', 100, false);
            this.newColumn('dl_limit', '', 'QBT_TR(Down Limit)QBT_TR[CONTEXT=TransferListModel]', 100, false);
            this.newColumn('up_limit', '', 'QBT_TR(Up Limit)QBT_TR[CONTEXT=TransferListModel]', 100, false);
            this.newColumn('downloaded', '', 'QBT_TR(Downloaded)QBT_TR[CONTEXT=TransferListModel]', 100, false);
            this.newColumn('uploaded', '', 'QBT_TR(Uploaded)QBT_TR[CONTEXT=TransferListModel]', 100, false);
            this.newColumn('downloaded_session', '', 'QBT_TR(Session Download)QBT_TR[CONTEXT=TransferListModel]', 100, false);
            this.newColumn('uploaded_session', '', 'QBT_TR(Session Upload)QBT_TR[CONTEXT=TransferListModel]', 100, false);
            this.newColumn('amount_left', '', 'QBT_TR(Remaining)QBT_TR[CONTEXT=TransferListModel]', 100, false);
            this.newColumn('time_active', '', 'QBT_TR(Time Active)QBT_TR[CONTEXT=TransferListModel]', 100, false);
            this.newColumn('save_path', '', 'QBT_TR(Save path)QBT_TR[CONTEXT=TransferListModel]', 100, false);
            this.newColumn('completed', '', 'QBT_TR(Completed)QBT_TR[CONTEXT=TransferListModel]', 100, false);
            this.newColumn('max_ratio', '', 'QBT_TR(Ratio Limit)QBT_TR[CONTEXT=TransferListModel]', 100, false);
            this.newColumn('seen_complete', '', 'QBT_TR(Last Seen Complete)QBT_TR[CONTEXT=TransferListModel]', 100, false);
            this.newColumn('last_activity', '', 'QBT_TR(Last Activity)QBT_TR[CONTEXT=TransferListModel]', 100, false);
            this.newColumn('availability', '', 'QBT_TR(Availability)QBT_TR[CONTEXT=TransferListModel]', 100, false);

            this.columns['state_icon'].onclick = '';
            this.columns['state_icon'].dataProperties[0] = 'state';

            this.columns['num_seeds'].dataProperties.push('num_complete');
            this.columns['num_leechs'].dataProperties.push('num_incomplete');

            this.initColumnsFunctions();
        },

        initColumnsFunctions: function() {

            // state_icon
            this.columns['state_icon'].updateTd = function(td, row) {
                let state = this.getRowValue(row);
                // normalize states
                switch (state) {
                    case "forcedDL":
                    case "metaDL":
                        state = "downloading";
                        break;
                    case "allocating":
                        state = "stalledDL";
                        break;
                    case "forcedUP":
                        state = "uploading";
                        break;
                    case "pausedDL":
                        state = "paused";
                        break;
                    case "pausedUP":
                        state = "completed";
                        break;
                    case "queuedDL":
                    case "queuedUP":
                    case "queuedMoving":
                        state = "queued";
                        break;
                    case "checkingDL":
                    case "checkingUP":
                    case "queuedForChecking":
                    case "checkingResumeData":
                    case "moving":
                        state = "checking";
                        break;
                    case "unknown":
                    case "missingFiles":
                        state = "error";
                        break;
                    default:
                        break; // do nothing
                }

                const img_path = 'images/skin/' + state + '.svg';

                if (td.getChildren('img').length > 0) {
                    const img = td.getChildren('img')[0];
                    if (img.src.indexOf(img_path) < 0) {
                        img.set('src', img_path);
                        img.set('title', state);
                    }
                }
                else {
                    td.adopt(new Element('img', {
                        'src': img_path,
                        'class': 'stateIcon',
                        'title': state
                    }));
                }
            };

            // status
            this.columns['status'].updateTd = function(td, row) {
                const state = this.getRowValue(row);
                if (!state) return;

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
                    case "forcedDL":
                        status = "QBT_TR([F] Downloading)QBT_TR[CONTEXT=TransferListDelegate]";
                        break;
                    case "allocating":
                        status = "QBT_TR(Allocating)QBT_TR[CONTEXT=TransferListDelegate]";
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
                    case "pausedDL":
                        status = "QBT_TR(Paused)QBT_TR[CONTEXT=TransferListDelegate]";
                        break;
                    case "pausedUP":
                        status = "QBT_TR(Completed)QBT_TR[CONTEXT=TransferListDelegate]";
                        break;
                    case "moving":
                        status = "QBT_TR(Moving)QBT_TR[CONTEXT=TransferListDelegate]";
                        break;
                    case "queuedMoving":
                        status = "QBT_TR([Q] Moving)QBT_TR[CONTEXT=TransferListDelegate]";
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

                td.set('text', status);
                td.set('title', status);
            };

            // priority
            this.columns['priority'].updateTd = function(td, row) {
                const queuePos = this.getRowValue(row);
                const formattedQueuePos = (queuePos < 1) ? '*' : queuePos;
                td.set('text', formattedQueuePos);
                td.set('title', formattedQueuePos);
            };

            this.columns['priority'].compareRows = function(row1, row2) {
                let row1_val = this.getRowValue(row1);
                let row2_val = this.getRowValue(row2);
                if (row1_val < 1)
                    row1_val = 1000000;
                if (row2_val < 1)
                    row2_val = 1000000;
                if (row1_val < row2_val)
                    return -1;
                else if (row1_val > row2_val)
                    return 1;
                else return 0;
            };

            // name, category, tags
            this.columns['name'].compareRows = function(row1, row2) {
                const row1Val = this.getRowValue(row1);
                const row2Val = this.getRowValue(row2);
                return row1Val.localeCompare(row2Val, undefined, {numeric: true, sensitivity: 'base'});
            };
            this.columns['category'].compareRows = this.columns['name'].compareRows;
            this.columns['tags'].compareRows = this.columns['name'].compareRows;

            // size, total_size
            this.columns['size'].updateTd = function(td, row) {
                const size = window.qBittorrent.Misc.friendlyUnit(this.getRowValue(row), false);
                td.set('text', size);
                td.set('title', size);
            };
            this.columns['total_size'].updateTd = this.columns['size'].updateTd;

            // progress
            this.columns['progress'].updateTd = function(td, row) {
                const progress = this.getRowValue(row);
                let progressFormated = (progress * 100).round(1);
                if (progressFormated == 100.0 && progress != 1.0)
                    progressFormated = 99.9;

                if (td.getChildren('div').length > 0) {
                    const div = td.getChildren('div')[0];
                    if (td.resized) {
                        td.resized = false;
                        div.setWidth(ProgressColumnWidth - 5);
                    }
                    if (div.getValue() != progressFormated)
                        div.setValue(progressFormated);
                }
                else {
                    if (ProgressColumnWidth < 0)
                        ProgressColumnWidth = td.offsetWidth;
                    td.adopt(new window.qBittorrent.ProgressBar.ProgressBar(progressFormated.toFloat(), {
                        'width': ProgressColumnWidth - 5
                    }));
                    td.resized = false;
                }
            };

            this.columns['progress'].onResize = function(columnName) {
                const pos = this.getColumnPos(columnName);
                const trs = this.tableBody.getElements('tr');
                ProgressColumnWidth = -1;
                for (let i = 0; i < trs.length; ++i) {
                    const td = trs[i].getElements('td')[pos];
                    if (ProgressColumnWidth < 0)
                        ProgressColumnWidth = td.offsetWidth;
                    td.resized = true;
                    this.columns[columnName].updateTd(td, this.rows.get(trs[i].rowId));
                }
            }.bind(this);

            // num_seeds
            this.columns['num_seeds'].updateTd = function(td, row) {
                const num_seeds = this.getRowValue(row, 0);
                const num_complete = this.getRowValue(row, 1);
                let value = num_seeds;
                if (num_complete != -1)
                    value += ' (' + num_complete + ')';
                td.set('text', value);
                td.set('title', value);
            };
            this.columns['num_seeds'].compareRows = function(row1, row2) {
                const num_seeds1 = this.getRowValue(row1, 0);
                const num_complete1 = this.getRowValue(row1, 1);

                const num_seeds2 = this.getRowValue(row2, 0);
                const num_complete2 = this.getRowValue(row2, 1);

                if (num_complete1 < num_complete2)
                    return -1;
                else if (num_complete1 > num_complete2)
                    return 1;
                else if (num_seeds1 < num_seeds2)
                    return -1;
                else if (num_seeds1 > num_seeds2)
                    return 1;
                else return 0;
            };

            // num_leechs
            this.columns['num_leechs'].updateTd = this.columns['num_seeds'].updateTd;
            this.columns['num_leechs'].compareRows = this.columns['num_seeds'].compareRows;

            // dlspeed
            this.columns['dlspeed'].updateTd = function(td, row) {
                const speed = window.qBittorrent.Misc.friendlyUnit(this.getRowValue(row), true);
                td.set('text', speed);
                td.set('title', speed);
            };

            // upspeed
            this.columns['upspeed'].updateTd = this.columns['dlspeed'].updateTd;

            // eta
            this.columns['eta'].updateTd = function(td, row) {
                const eta = window.qBittorrent.Misc.friendlyDuration(this.getRowValue(row));
                td.set('text', eta);
                td.set('title', eta);
            };

            // ratio
            this.columns['ratio'].updateTd = function(td, row) {
                const ratio = this.getRowValue(row);
                const string = (ratio === -1) ? '∞' : window.qBittorrent.Misc.toFixedPointString(ratio, 2);
                td.set('text', string);
                td.set('title', string);
            };

            // added on
            this.columns['added_on'].updateTd = function(td, row) {
                const date = new Date(this.getRowValue(row) * 1000).toLocaleString();
                td.set('text', date);
                td.set('title', date);
            };

            // completion_on
            this.columns['completion_on'].updateTd = function(td, row) {
                const val = this.getRowValue(row);
                if ((val === 0xffffffff) || (val < 0)) {
                    td.set('text', '');
                    td.set('title', '');
                }
                else {
                    const date = new Date(this.getRowValue(row) * 1000).toLocaleString();
                    td.set('text', date);
                    td.set('title', date);
                }
            };

            //  dl_limit, up_limit
            this.columns['dl_limit'].updateTd = function(td, row) {
                const speed = this.getRowValue(row);
                if (speed === 0) {
                    td.set('text', '∞');
                    td.set('title', '∞');
                }
                else {
                    const formattedSpeed = window.qBittorrent.Misc.friendlyUnit(speed, true);
                    td.set('text', formattedSpeed);
                    td.set('title', formattedSpeed);
                }
            };

            this.columns['up_limit'].updateTd = this.columns['dl_limit'].updateTd;

            // downloaded, uploaded, downloaded_session, uploaded_session, amount_left
            this.columns['downloaded'].updateTd = this.columns['size'].updateTd;
            this.columns['uploaded'].updateTd = this.columns['size'].updateTd;
            this.columns['downloaded_session'].updateTd = this.columns['size'].updateTd;
            this.columns['uploaded_session'].updateTd = this.columns['size'].updateTd;
            this.columns['amount_left'].updateTd = this.columns['size'].updateTd;

            // time active
            this.columns['time_active'].updateTd = function(td, row) {
                const time = window.qBittorrent.Misc.friendlyDuration(this.getRowValue(row));
                td.set('text', time);
                td.set('title', time);
            };

            // completed
            this.columns['completed'].updateTd = this.columns['size'].updateTd;

            // max_ratio
            this.columns['max_ratio'].updateTd = this.columns['ratio'].updateTd;

            // seen_complete
            this.columns['seen_complete'].updateTd = this.columns['completion_on'].updateTd;

            // last_activity
            this.columns['last_activity'].updateTd = function(td, row) {
                const val = this.getRowValue(row);
                if (val < 1) {
                    td.set('text', '∞');
                    td.set('title', '∞');
                }
                else {
                    const formattedVal = 'QBT_TR(%1 ago)QBT_TR[CONTEXT=TransferListDelegate]'.replace('%1', window.qBittorrent.Misc.friendlyDuration((new Date()) / 1000 - val));
                    td.set('text', formattedVal);
                    td.set('title', formattedVal);
                }
            };

            // availability
            this.columns['availability'].updateTd = function(td, row) {
                const value = window.qBittorrent.Misc.toFixedPointString(this.getRowValue(row), 3);
                td.set('text', value);
                td.set('title', value);
            };
        },

        applyFilter: function(row, filterName, categoryHash, tagHash, filterTerms) {
            const state = row['full_data'].state;
            const name = row['full_data'].name.toLowerCase();
            let inactive = false;
            let r;

            switch (filterName) {
                case 'downloading':
                    if ((state != 'downloading') && (state.indexOf('DL') === -1))
                        return false;
                    break;
                case 'seeding':
                    if (state != 'uploading' && state != 'forcedUP' && state != 'stalledUP' && state != 'queuedUP' && state != 'checkingUP')
                        return false;
                    break;
                case 'completed':
                    if ((state != 'uploading') && (state.indexOf('UP') === -1))
                        return false;
                    break;
                case 'paused':
                    if (state.indexOf('paused') === -1)
                        return false;
                    break;
                case 'resumed':
                    if (state.indexOf('paused') > -1)
                        return false;
                    break;
                case 'stalled':
                    if ((state != 'stalledUP') && (state != 'stalledDL'))
                        return false;
                    break;
                case 'stalled_uploading':
                    if (state != 'stalledUP')
                        return false;
                    break;
                case 'stalled_downloading':
                    if (state != 'stalledDL')
                        return false;
                    break;
                case 'inactive':
                    inactive = true;
                    // fallthrough
                case 'active':
                    if (state == 'stalledDL')
                        r = (row['full_data'].upspeed > 0);
                    else
                        r = state == 'metaDL' || state == 'downloading' || state == 'forcedDL' || state == 'uploading' || state == 'forcedUP';
                    if (r == inactive)
                        return false;
                    break;
                case 'errored':
                    if (state != 'error' && state != "unknown" && state != "missingFiles")
                        return false;
                    break;
            }

            const categoryHashInt = parseInt(categoryHash);
            if (!isNaN(categoryHashInt)) {
                switch (categoryHashInt) {
                    case CATEGORIES_ALL:
                        break;  // do nothing
                    case CATEGORIES_UNCATEGORIZED:
                        if (row['full_data'].category.length !== 0)
                            return false;
                        break;  // do nothing
                    default:
                        if (categoryHashInt !== genHash(row['full_data'].category))
                            return false;
                }
            }

            const tagHashInt = parseInt(tagHash);
            const isNumber = !isNaN(tagHashInt);
            if (isNumber) {
                switch (tagHashInt) {
                    case TAGS_ALL:
                        break;  // do nothing

                    case TAGS_UNTAGGED:
                        if (row['full_data'].tags.length !== 0)
                            return false;
                        break;  // do nothing

                    default:
                        let rowTags = row['full_data'].tags.split(', ');
                        rowTags = rowTags.map(function(tag) {
                            return genHash(tag);
                        });
                        if (!rowTags.contains(tagHashInt))
                            return false;
                }
            }

            if ((filterTerms !== undefined) && (filterTerms !== null)
                && (filterTerms.length > 0) && !window.qBittorrent.Misc.containsAllTerms(name, filterTerms))
                return false;

            return true;
        },

        getFilteredTorrentsNumber: function(filterName, categoryHash, tagHash) {
            let cnt = 0;
            const rows = this.rows.getValues();

            for (let i = 0; i < rows.length; ++i)
                if (this.applyFilter(rows[i], filterName, categoryHash, tagHash, null)) ++cnt;
            return cnt;
        },

        getFilteredTorrentsHashes: function(filterName, categoryHash, tagHash) {
            const rowsHashes = [];
            const rows = this.rows.getValues();

            for (let i = 0; i < rows.length; ++i)
                if (this.applyFilter(rows[i], filterName, categoryHash, tagHash, null))
                    rowsHashes.push(rows[i]['rowId']);

            return rowsHashes;
        },

        getFilteredAndSortedRows: function() {
            const filteredRows = [];

            const rows = this.rows.getValues();
            const filterText = $('torrentsFilterInput').value.trim().toLowerCase();
            const filterTerms = (filterText.length > 0) ? filterText.split(" ") : null;

            for (let i = 0; i < rows.length; ++i) {
                if (this.applyFilter(rows[i], selected_filter, selected_category, selectedTag, filterTerms)) {
                    filteredRows.push(rows[i]);
                    filteredRows[rows[i].rowId] = rows[i];
                }
            }

            filteredRows.sort(function(row1, row2) {
                const column = this.columns[this.sortedColumn];
                const res = column.compareRows(row1, row2);
                if (this.reverseSort === '0')
                    return res;
                else
                    return -res;
            }.bind(this));
            return filteredRows;
        },

        setupTr: function(tr) {
            tr.addEvent('dblclick', function(e) {
                e.stop();
                this._this.deselectAll();
                this._this.selectRow(this.rowId);
                const row = this._this.rows.get(this.rowId);
                const state = row['full_data'].state;
                if (state.indexOf('paused') > -1)
                    startFN();
                else
                    pauseFN();
                return true;
            });
            tr.addClass("torrentsTableContextMenuTarget");
        },

        getCurrentTorrentHash: function() {
            return this.getSelectedRowId();
        },

        onSelectedRowChanged: function() {
            updatePropertiesPanel();
        }
    });

    const TorrentPeersTable = new Class({
        Extends: DynamicTable,

        initColumns: function() {
            this.newColumn('country', '', 'QBT_TR(Country/Region)QBT_TR[CONTEXT=PeerListWidget]', 22, true);
            this.newColumn('ip', '', 'QBT_TR(IP)QBT_TR[CONTEXT=PeerListWidget]', 80, true);
            this.newColumn('port', '', 'QBT_TR(Port)QBT_TR[CONTEXT=PeerListWidget]', 35, true);
            this.newColumn('connection', '', 'QBT_TR(Connection)QBT_TR[CONTEXT=PeerListWidget]', 50, true);
            this.newColumn('flags', '', 'QBT_TR(Flags)QBT_TR[CONTEXT=PeerListWidget]', 50, true);
            this.newColumn('client', '', 'QBT_TR(Client)QBT_TR[CONTEXT=PeerListWidget]', 140, true);
            this.newColumn('progress', '', 'QBT_TR(Progress)QBT_TR[CONTEXT=PeerListWidget]', 50, true);
            this.newColumn('dl_speed', '', 'QBT_TR(Down Speed)QBT_TR[CONTEXT=PeerListWidget]', 50, true);
            this.newColumn('up_speed', '', 'QBT_TR(Up Speed)QBT_TR[CONTEXT=PeerListWidget]', 50, true);
            this.newColumn('downloaded', '', 'QBT_TR(Downloaded)QBT_TR[CONTEXT=PeerListWidget]', 50, true);
            this.newColumn('uploaded', '', 'QBT_TR(Uploaded)QBT_TR[CONTEXT=PeerListWidget]', 50, true);
            this.newColumn('relevance', '', 'QBT_TR(Relevance)QBT_TR[CONTEXT=PeerListWidget]', 30, true);
            this.newColumn('files', '', 'QBT_TR(Files)QBT_TR[CONTEXT=PeerListWidget]', 100, true);

            this.columns['country'].dataProperties.push('country_code');
            this.columns['flags'].dataProperties.push('flags_desc');
            this.initColumnsFunctions();
        },

        initColumnsFunctions: function() {

            // country
            this.columns['country'].updateTd = function(td, row) {
                const country = this.getRowValue(row, 0);
                const country_code = this.getRowValue(row, 1);

                if (!country_code) {
                    if (td.getChildren('img').length > 0)
                        td.getChildren('img')[0].dispose();
                    return;
                }

                const img_path = 'images/flags/' + country_code + '.svg';

                if (td.getChildren('img').length > 0) {
                    const img = td.getChildren('img')[0];
                    img.set('src', img_path);
                    img.set('class', 'flags');
                    img.set('alt', country);
                    img.set('title', country);
                }
                else
                    td.adopt(new Element('img', {
                        'src': img_path,
                        'class': 'flags',
                        'alt': country,
                        'title': country
                    }));
            };

            // ip
            this.columns['ip'].compareRows = function(row1, row2) {
                const ip1 = this.getRowValue(row1);
                const ip2 = this.getRowValue(row2);

                const a = ip1.split(".");
                const b = ip2.split(".");

                for (let i = 0; i < 4; ++i) {
                    if (a[i] != b[i])
                        return a[i] - b[i];
                }

                return 0;
            };

            // flags
            this.columns['flags'].updateTd = function(td, row) {
                td.set('text', this.getRowValue(row, 0));
                td.set('title', this.getRowValue(row, 1));
            };

            // progress
            this.columns['progress'].updateTd = function(td, row) {
                const progress = this.getRowValue(row);
                let progressFormated = (progress * 100).round(1);
                if (progressFormated == 100.0 && progress != 1.0)
                    progressFormated = 99.9;
                progressFormated += "%";
                td.set('text', progressFormated);
                td.set('title', progressFormated);
            };

            // dl_speed, up_speed
            this.columns['dl_speed'].updateTd = function(td, row) {
                const speed = this.getRowValue(row);
                if (speed === 0) {
                    td.set('text', '');
                    td.set('title', '');
                }
                else {
                    const formattedSpeed = window.qBittorrent.Misc.friendlyUnit(speed, true);
                    td.set('text', formattedSpeed);
                    td.set('title', formattedSpeed);
                }
            };
            this.columns['up_speed'].updateTd = this.columns['dl_speed'].updateTd;

            // downloaded, uploaded
            this.columns['downloaded'].updateTd = function(td, row) {
                const downloaded = window.qBittorrent.Misc.friendlyUnit(this.getRowValue(row), false);
                td.set('text', downloaded);
                td.set('title', downloaded);
            };
            this.columns['uploaded'].updateTd = this.columns['downloaded'].updateTd;

            // relevance
            this.columns['relevance'].updateTd = this.columns['progress'].updateTd;

            // files
            this.columns['files'].updateTd = function(td, row) {
                const value = this.getRowValue(row, 0);
                td.set('text', value.replace(/\n/g, ';'));
                td.set('title', value);
            };

        }
    });

    const SearchResultsTable = new Class({
        Extends: DynamicTable,

        initColumns: function() {
            this.newColumn('fileName', '', 'QBT_TR(Name)QBT_TR[CONTEXT=SearchResultsTable]', 500, true);
            this.newColumn('fileSize', '', 'QBT_TR(Size)QBT_TR[CONTEXT=SearchResultsTable]', 100, true);
            this.newColumn('nbSeeders', '', 'QBT_TR(Seeders)QBT_TR[CONTEXT=SearchResultsTable]', 100, true);
            this.newColumn('nbLeechers', '', 'QBT_TR(Leechers)QBT_TR[CONTEXT=SearchResultsTable]', 100, true);
            this.newColumn('siteUrl', '', 'QBT_TR(Search engine)QBT_TR[CONTEXT=SearchResultsTable]', 250, true);

            this.initColumnsFunctions();
        },

        initColumnsFunctions: function() {
            const displaySize = function(td, row) {
                const size = window.qBittorrent.Misc.friendlyUnit(this.getRowValue(row), false);
                td.set('text', size);
                td.set('title', size);
            }
            const displayNum = function(td, row) {
                const value = this.getRowValue(row);
                const formattedValue = (value === "-1") ? "Unknown" : value;
                td.set('text', formattedValue);
                td.set('title', formattedValue);
            }

            this.columns['fileSize'].updateTd = displaySize;
            this.columns['nbSeeders'].updateTd = displayNum;
            this.columns['nbLeechers'].updateTd = displayNum;
        },

        getFilteredAndSortedRows: function() {
            const getSizeFilters = function() {
                let minSize = (window.qBittorrent.Search.searchSizeFilter.min > 0.00) ? (window.qBittorrent.Search.searchSizeFilter.min * Math.pow(1024, window.qBittorrent.Search.searchSizeFilter.minUnit)) : 0.00;
                let maxSize = (window.qBittorrent.Search.searchSizeFilter.max > 0.00) ? (window.qBittorrent.Search.searchSizeFilter.max * Math.pow(1024, window.qBittorrent.Search.searchSizeFilter.maxUnit)) : 0.00;

                if ((minSize > maxSize) && (maxSize > 0.00)) {
                    const tmp = minSize;
                    minSize = maxSize;
                    maxSize = tmp;
                }

                return {
                    min: minSize,
                    max: maxSize
                }
            };

            const getSeedsFilters = function() {
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
                }
            };

            let filteredRows = [];
            const rows = this.rows.getValues();
            const searchTerms = window.qBittorrent.Search.searchText.pattern.toLowerCase().split(" ");
            const filterTerms = window.qBittorrent.Search.searchText.filterPattern.toLowerCase().split(" ");
            const sizeFilters = getSizeFilters();
            const seedsFilters = getSeedsFilters();
            const searchInTorrentName = $('searchInTorrentName').get('value') === "names";

            if (searchInTorrentName || (filterTerms.length > 0) || (window.qBittorrent.Search.searchSizeFilter.min > 0.00) || (window.qBittorrent.Search.searchSizeFilter.max > 0.00)) {
                for (let i = 0; i < rows.length; ++i) {
                    const row = rows[i];

                if (searchInTorrentName && !window.qBittorrent.Misc.containsAllTerms(row.full_data.fileName, searchTerms)) continue;
                if ((filterTerms.length > 0) && !window.qBittorrent.Misc.containsAllTerms(row.full_data.fileName, filterTerms)) continue;
                    if ((sizeFilters.min > 0.00) && (row.full_data.fileSize < sizeFilters.min)) continue;
                    if ((sizeFilters.max > 0.00) && (row.full_data.fileSize > sizeFilters.max)) continue;
                    if ((seedsFilters.min > 0) && (row.full_data.nbSeeders < seedsFilters.min)) continue;
                    if ((seedsFilters.max > 0) && (row.full_data.nbSeeders > seedsFilters.max)) continue;

                    filteredRows.push(row);
                }
            }
            else {
                filteredRows = rows;
            }

            filteredRows.sort(function(row1, row2) {
                const column = this.columns[this.sortedColumn];
                const res = column.compareRows(row1, row2);
                if (this.reverseSort === '0')
                    return res;
                else
                    return -res;
            }.bind(this));

            return filteredRows;
        },

        setupTr: function(tr) {
            tr.addClass("searchTableRow");
        }
    });

    const SearchPluginsTable = new Class({
        Extends: DynamicTable,

        initColumns: function() {
            this.newColumn('fullName', '', 'QBT_TR(Name)QBT_TR[CONTEXT=SearchPluginsTable]', 175, true);
            this.newColumn('version', '', 'QBT_TR(Version)QBT_TR[CONTEXT=SearchPluginsTable]', 100, true);
            this.newColumn('url', '', 'QBT_TR(Url)QBT_TR[CONTEXT=SearchPluginsTable]', 175, true);
            this.newColumn('enabled', '', 'QBT_TR(Enabled)QBT_TR[CONTEXT=SearchPluginsTable]', 100, true);

            this.initColumnsFunctions();
        },

        initColumnsFunctions: function() {
            this.columns['enabled'].updateTd = function(td, row) {
                const value = this.getRowValue(row);
                if (value) {
                    td.set('text', 'QBT_TR(Yes)QBT_TR[CONTEXT=SearchPluginsTable]');
                    td.set('title', 'QBT_TR(Yes)QBT_TR[CONTEXT=SearchPluginsTable]');
                    td.getParent("tr").addClass("green");
                    td.getParent("tr").removeClass("red");
                }
                else {
                    td.set('text', 'QBT_TR(No)QBT_TR[CONTEXT=SearchPluginsTable]');
                    td.set('title', 'QBT_TR(No)QBT_TR[CONTEXT=SearchPluginsTable]');
                    td.getParent("tr").addClass("red");
                    td.getParent("tr").removeClass("green");
                }
            };
        },

        setupTr: function(tr) {
            tr.addClass("searchPluginsTableRow");
        }
    });

    const TorrentTrackersTable = new Class({
        Extends: DynamicTable,

        initColumns: function() {
            this.newColumn('tier', '', 'QBT_TR(Tier)QBT_TR[CONTEXT=TrackerListWidget]', 35, true);
            this.newColumn('url', '', 'QBT_TR(URL)QBT_TR[CONTEXT=TrackerListWidget]', 250, true);
            this.newColumn('status', '', 'QBT_TR(Status)QBT_TR[CONTEXT=TrackerListWidget]', 125, true);
            this.newColumn('peers', '', 'QBT_TR(Peers)QBT_TR[CONTEXT=TrackerListWidget]', 75, true);
            this.newColumn('seeds', '', 'QBT_TR(Seeds)QBT_TR[CONTEXT=TrackerListWidget]', 75, true);
            this.newColumn('leeches', '', 'QBT_TR(Leeches)QBT_TR[CONTEXT=TrackerListWidget]', 75, true);
            this.newColumn('downloaded', '', 'QBT_TR(Downloaded)QBT_TR[CONTEXT=TrackerListWidget]', 100, true);
            this.newColumn('message', '', 'QBT_TR(Message)QBT_TR[CONTEXT=TrackerListWidget]', 250, true);
        },
    });

    const TorrentFilesTable = new Class({
        Extends: DynamicTable,

        filterTerms: [],
        prevFilterTerms: [],
        prevRowsString: null,
        prevFilteredRows: [],
        prevSortedColumn: null,
        prevReverseSort: null,
        fileTree: new window.qBittorrent.FileTree.FileTree(),

        populateTable: function(root) {
            this.fileTree.setRoot(root);
            root.children.each(function(node) {
                this._addNodeToTable(node, 0);
            }.bind(this));
        },

        _addNodeToTable: function(node, depth) {
            node.depth = depth;

            if (node.isFolder) {
                const data = {
                    rowId: node.rowId,
                    size: node.size,
                    checked: node.checked,
                    remaining: node.remaining,
                    progress: node.progress,
                    priority: window.qBittorrent.PropFiles.normalizePriority(node.priority),
                    availability: node.availability,
                    fileId: -1,
                    name: node.name
                };

                node.data = data;
                node.full_data = data;
                this.updateRowData(data);
            }
            else {
                node.data.rowId = node.rowId;
                node.full_data = node.data;
                this.updateRowData(node.data);
            }

            node.children.each(function(child) {
                this._addNodeToTable(child, depth + 1);
            }.bind(this));
        },

        getRoot: function() {
            return this.fileTree.getRoot();
        },

        getNode: function(rowId) {
            return this.fileTree.getNode(rowId);
        },

        getRow: function(node) {
            const rowId = this.fileTree.getRowId(node);
            return this.rows.get(rowId);
        },

        initColumns: function() {
            this.newColumn('checked', '', '', 50, true);
            this.newColumn('name', '', 'QBT_TR(Name)QBT_TR[CONTEXT=TrackerListWidget]', 300, true);
            this.newColumn('size', '', 'QBT_TR(Size)QBT_TR[CONTEXT=TrackerListWidget]', 75, true);
            this.newColumn('progress', '', 'QBT_TR(Progress)QBT_TR[CONTEXT=TrackerListWidget]', 100, true);
            this.newColumn('priority', '', 'QBT_TR(Download Priority)QBT_TR[CONTEXT=TrackerListWidget]', 150, true);
            this.newColumn('remaining', '', 'QBT_TR(Remaining)QBT_TR[CONTEXT=TrackerListWidget]', 75, true);
            this.newColumn('availability', '', 'QBT_TR(Availability)QBT_TR[CONTEXT=TrackerListWidget]', 75, true);

            this.initColumnsFunctions();
        },

        initColumnsFunctions: function() {
            const that = this;
            const displaySize = function(td, row) {
                const size = window.qBittorrent.Misc.friendlyUnit(this.getRowValue(row), false);
                td.set('text', size);
                td.set('title', size);
            };
            const displayPercentage = function(td, row) {
                const value = window.qBittorrent.Misc.friendlyPercentage(this.getRowValue(row));
                td.set('text', value);
                td.set('title', value);
            };

            // checked
            this.columns['checked'].updateTd = function(td, row) {
                const id = row.rowId;
                const value = this.getRowValue(row);

                if (window.qBittorrent.PropFiles.isDownloadCheckboxExists(id)) {
                    window.qBittorrent.PropFiles.updateDownloadCheckbox(id, value);
                }
                else {
                    const treeImg = new Element('img', {
                        src: 'images/L.gif',
                        styles: {
                            'margin-bottom': -2
                        }
                    });
                    td.adopt(treeImg, window.qBittorrent.PropFiles.createDownloadCheckbox(id, row.full_data.fileId, value));
                }
            };

            // name
            this.columns['name'].updateTd = function(td, row) {
                const id = row.rowId;
                const fileNameId = 'filesTablefileName' + id;
                const node = that.getNode(id);

                if (node.isFolder) {
                    const value = this.getRowValue(row);
                    const collapseIconId = 'filesTableCollapseIcon' + id;
                    const dirImgId = 'filesTableDirImg' + id;
                    if ($(dirImgId)) {
                        // just update file name
                        $(fileNameId).set('text', value);
                    }
                    else {
                        const collapseIcon = new Element('img', {
                            src: 'images/qbt-theme/go-down.svg',
                            styles: {
                                'margin-left': (node.depth * 20)
                            },
                            class: "filesTableCollapseIcon",
                            id: collapseIconId,
                            "data-id": id,
                            onclick: "qBittorrent.PropFiles.collapseIconClicked(this)"
                        });
                        const span = new Element('span', {
                            text: value,
                            id: fileNameId
                        });
                        const dirImg = new Element('img', {
                            src: 'images/qbt-theme/inode-directory.svg',
                            styles: {
                                'width': 15,
                                'padding-right': 5,
                                'margin-bottom': -3
                            },
                            id: dirImgId
                        });
                        const html = collapseIcon.outerHTML + dirImg.outerHTML + span.outerHTML;
                        td.set('html', html);
                    }
                }
                else {
                    const value = this.getRowValue(row);
                    const span = new Element('span', {
                        text: value,
                        id: fileNameId,
                        styles: {
                            'margin-left': ((node.depth + 1) * 20)
                        }
                    });
                    td.set('html', span.outerHTML);
                }
            };

            // size
            this.columns['size'].updateTd = displaySize;

            // progress
            this.columns['progress'].updateTd = function(td, row) {
                const id = row.rowId;
                const value = this.getRowValue(row);

                const progressBar = $('pbf_' + id);
                if (progressBar === null) {
                    td.adopt(new window.qBittorrent.ProgressBar.ProgressBar(value.toFloat(), {
                        id: 'pbf_' + id,
                        width: 80
                    }));
                }
                else {
                    progressBar.setValue(value.toFloat());
                }
            };

            // priority
            this.columns['priority'].updateTd = function(td, row) {
                const id = row.rowId;
                const value = this.getRowValue(row);

                if (window.qBittorrent.PropFiles.isPriorityComboExists(id))
                    window.qBittorrent.PropFiles.updatePriorityCombo(id, value);
                else
                    td.adopt(window.qBittorrent.PropFiles.createPriorityCombo(id, row.full_data.fileId, value));
            };

            // remaining, availability
            this.columns['remaining'].updateTd = displaySize;
            this.columns['availability'].updateTd = displayPercentage;
        },

        altRow: function() {
            let addClass = false;
            const trs = this.tableBody.getElements('tr');
            trs.each(function(tr) {
                if (tr.hasClass("invisible"))
                    return;

                if (addClass){
                    tr.addClass("alt");
                    tr.removeClass("nonAlt");
                }
                else {
                    tr.removeClass("alt");
                    tr.addClass("nonAlt");
                }
                addClass = !addClass;
            }.bind(this));
        },

        _sortNodesByColumn: function(nodes, column) {
            nodes.sort(function(row1, row2) {
                // list folders before files when sorting by name
                if (column.name === "name") {
                    const node1 = this.getNode(row1.data.rowId);
                    const node2 = this.getNode(row2.data.rowId);
                    if (node1.isFolder && !node2.isFolder)
                        return -1;
                    if (node2.isFolder && !node1.isFolder)
                        return 1;
                }

                const res = column.compareRows(row1, row2);
                return (this.reverseSort === '0') ? res : -res;
            }.bind(this));

            nodes.each(function(node) {
                if (node.children.length > 0)
                    this._sortNodesByColumn(node.children, column);
            }.bind(this));
        },

        _filterNodes: function(node, filterTerms, filteredRows) {
            if (node.isFolder) {
                const childAdded = node.children.reduce(function (acc, child) {
                    // we must execute the function before ORing w/ acc or we'll stop checking child nodes after the first successful match
                    return (this._filterNodes(child, filterTerms, filteredRows) || acc);
                }.bind(this), false);

                if (childAdded) {
                    const row = this.getRow(node);
                    filteredRows.push(row);
                    return true;
                }
            }

            if (window.qBittorrent.Misc.containsAllTerms(node.name, filterTerms)) {
                const row = this.getRow(node);
                filteredRows.push(row);
                return true;
            }

            return false;
        },

        setFilter: function(text) {
            const filterTerms = text.trim().toLowerCase().split(' ');
            if ((filterTerms.length === 1) && (filterTerms[0] === ''))
                this.filterTerms = [];
            else
                this.filterTerms = filterTerms;
        },

        getFilteredAndSortedRows: function() {
            if (this.getRoot() === null)
                return [];

            const generateRowsSignature = function(rows) {
                const rowsData = rows.map(function(row) {
                    return row.full_data;
                });
                return JSON.stringify(rowsData);
            };

            const getFilteredRows = function() {
                if (this.filterTerms.length === 0) {
                    const nodeArray = this.fileTree.toArray();
                    const filteredRows = nodeArray.map(function(node) {
                        return this.getRow(node);
                    }.bind(this));
                    return filteredRows;
                }

                const filteredRows = [];
                this.getRoot().children.each(function(child) {
                    this._filterNodes(child, this.filterTerms, filteredRows);
                }.bind(this));
                filteredRows.reverse();
                return filteredRows;
            }.bind(this);

            const hasRowsChanged = function(rowsString, prevRowsStringString) {
                const rowsChanged = (rowsString !== prevRowsStringString);
                const isFilterTermsChanged = this.filterTerms.reduce(function(acc, term, index) {
                    return (acc || (term !== this.prevFilterTerms[index]));
                }.bind(this), false);
                const isFilterChanged = ((this.filterTerms.length !== this.prevFilterTerms.length)
                    || ((this.filterTerms.length > 0) && isFilterTermsChanged));
                const isSortedColumnChanged = (this.prevSortedColumn !== this.sortedColumn);
                const isReverseSortChanged = (this.prevReverseSort !== this.reverseSort);

                return (rowsChanged || isFilterChanged || isSortedColumnChanged || isReverseSortChanged);
            }.bind(this);

            const rowsString = generateRowsSignature(this.rows);
            if (!hasRowsChanged(rowsString, this.prevRowsString)) {
                return this.prevFilteredRows;
            }

            // sort, then filter
            const column = this.columns[this.sortedColumn];
            this._sortNodesByColumn(this.getRoot().children, column);
            const filteredRows = getFilteredRows();

            this.prevFilterTerms = this.filterTerms;
            this.prevRowsString = rowsString;
            this.prevFilteredRows = filteredRows;
            this.prevSortedColumn = this.sortedColumn;
            this.prevReverseSort = this.reverseSort;
            return filteredRows;
        },

        setIgnored: function(rowId, ignore) {
            const row = this.rows.get(rowId);
            if (ignore)
                row.full_data.remaining = 0;
            else
                row.full_data.remaining = (row.full_data.size * (1.0 - (row.full_data.progress / 100)));
        }
    });

    return exports();
})();

/*************************************************************/
