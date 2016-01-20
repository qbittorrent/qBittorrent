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
    Desc        : Programable sortable table
    Licence     : Open Source MIT Licence

 **************************************************************/

var DynamicTable = new Class({

        initialize : function () {},

        setup : function (tableId, tableHeaderId, context_menu) {
            this.tableId = tableId;
            this.tableHeaderId = tableHeaderId;
            this.table = $(tableId);
            this.rows = new Hash();
            this.cur = new Array();
            this.columns = new Array();
            this.context_menu = context_menu;
            this.sortedColumn = getLocalStorageItem('sorted_column_' + this.tableId, 0);
            this.reverseSort = getLocalStorageItem('reverse_sort_' + this.tableId, '0');
            this.initColumns();
            this.loadColumnsOrder();
            this.updateHeader();
        },

        initColumns : function () {},

        newColumn : function (name, style, caption) {
            var column = {};
            column['name'] = name;
            column['visible'] = getLocalStorageItem('column_' + name + '_visible_' + this.tableId, '1');
            column['force_hide'] = false;
            column['caption'] = caption;
            column['style'] = style;
            column['onclick'] = 'this._this.setSortedColumn(\'' + name + '\');';
            column['dataProperties'] = [name];
            column['getRowValue'] = function (row, pos) {
                if (pos == undefined)
                    pos = 0;
                return row['full_data'][this.dataProperties[pos]];
            };
            column['compareRows'] = function (row1, row2) {
                if (this.getRowValue(row1) < this.getRowValue(row2))
                    return -1;
                else if (this.getRowValue(row1) > this.getRowValue(row2))
                    return 1;
                else return 0;
            };
            column['updateTd'] = function (td, row) {
                td.innerHTML = this.getRowValue(row);
            };
            this.columns.push(column);
            this.columns[name] = column;

            $(this.tableHeaderId).appendChild(new Element('th'));
        },

        loadColumnsOrder : function () {
            columnsOrder = ['state_icon']; // status icon column is always the first
            val = localStorage.getItem('columns_order_' + this.tableId);
            if (val === null || val === undefined) return;
            val.split(',').forEach(function(v) {
                if ((v in this.columns) && (!columnsOrder.contains(v)))
                    columnsOrder.push(v);
            }.bind(this));

            for (i = 0; i < this.columns.length; i++)
                if (!columnsOrder.contains(this.columns[i].name))
                    columnsOrder.push(this.columns[i].name);

            for (i = 0; i < this.columns.length; i++)
                this.columns[i] = this.columns[columnsOrder[i]];
        },

        saveColumnsOrder : function () {
            val = '';
            for (i = 0; i < this.columns.length; i++) {
                if (i > 0)
                    val += ',';
                val += this.columns[i].name;
            }
            localStorage.setItem('columns_order_' + this.tableId, val);
        },

        updateHeader : function () {
            var ths = $(this.tableHeaderId).getElements('th');

            for (var i = 0; i < ths.length; i++) {
                th = ths[i];
                th._this = this;
                th.setAttribute('onclick', this.columns[i].onclick);
                th.innerHTML = this.columns[i].caption;
                th.setAttribute('style', this.columns[i].style);
                if ((this.columns[i].visible == '0') || this.columns[i].force_hide)
                    th.addClass('invisible');
                else
                    th.removeClass('invisible');
            }
        },

        getColumnPos : function (columnName) {
            for (var i = 0; i < this.columns.length; i++)
                if (this.columns[i].name == columnName)
                    return i;
            return -1;
        },

        updateColumn : function (columnName) {
            var pos = this.getColumnPos(columnName);
            var visible = ((this.columns[pos].visible != '0') && !this.columns[pos].force_hide);
            var ths = $(this.tableHeaderId).getElements('th');
            if (visible)
                ths[pos].removeClass('invisible');
            else
                ths[pos].addClass('invisible');
            var trs = this.table.getElements('tr');
            for (var i = 0; i < trs.length; i++)
                if (visible)
                    trs[i].getElements('td')[pos].removeClass('invisible');
                else
                    trs[i].getElements('td')[pos].addClass('invisible');
        },

        setSortedColumn : function (column) {
            if (column != this.sortedColumn) {
                this.sortedColumn = column;
                this.reverseSort = '0';
            }
            else {
                // Toggle sort order
                this.reverseSort = this.reverseSort == '0' ? '1' : '0';
            }
            localStorage.setItem('sorted_column_' + this.tableId, column);
            localStorage.setItem('reverse_sort_' + this.tableId, this.reverseSort);
            this.updateTable(false);
        },

        getSelectedRowId : function () {
            if (this.cur.length > 0)
                return this.cur[0];
            return '';
        },

        altRow : function () {
            if (!MUI.ieLegacySupport)
                return;

            var trs = this.table.getElements('tr');
            trs.each(function (el, i) {
                if (i % 2) {
                    el.addClass('alt');
                } else {
                    el.removeClass('alt');
                }
            }.bind(this));
        },

        selectAll : function () {
            this.cur.empty();

            var trs = this.table.getElements('tr');
            for (var i = 0; i < trs.length; i++) {
                var tr = trs[i];
                this.cur.push(tr.rowId);
                if (!tr.hasClass('selected'))
                    tr.addClass('selected');
            }
        },

        deselectAll : function () {
            this.cur.empty();
        },

        selectRow : function (rowId) {
            this.cur.empty();
            this.cur.push(rowId);
            var trs = this.table.getElements('tr');
            for (var i = 0; i < trs.length; i++) {
                var tr = trs[i];
                if (tr.rowId == rowId) {
                    if (!tr.hasClass('selected'))
                        tr.addClass('selected');
                }
                else
                if (tr.hasClass('selected'))
                    tr.removeClass('selected');
            }
            this.onSelectedRowChanged();
        },

        onSelectedRowChanged : function () {},

        updateRowData : function (data) {
            var rowId = data['rowId'];
            var row;

            if (!this.rows.has(rowId)) {
                row = {};
                this.rows.set(rowId, row);
                row['full_data'] = {};
                row['rowId'] = rowId;
            }
            else
                row = this.rows.get(rowId);

            row['data'] = data;

            for(var x in data)
                row['full_data'][x] = data[x];
        },

        getFilteredAndSortedRows : function () {
            var filteredRows = new Array();

            var rows = this.rows.getValues();

            for (i = 0; i < rows.length; i++)
            {
                filteredRows.push(rows[i]);
                filteredRows[rows[i].rowId] = rows[i];
            }

            filteredRows.sort(function (row1, row2) {
                var column = this.columns[this.sortedColumn];
                res = column.compareRows(row1, row2);
                if (this.reverseSort == '0')
                    return res;
                else
                    return -res;
            }.bind(this));
            return filteredRows;
        },

        getTrByRowId : function (rowId) {
            trs = this.table.getElements('tr');
            for (var i = 0; i < trs.length; i++)
                if (trs[i].rowId == rowId)
                    return trs[i];
            return null;
        },

        updateTable : function (fullUpdate) {
            if (fullUpdate == undefined)
                fullUpdate = false;

            var rows = this.getFilteredAndSortedRows();

            for (var i = 0; i < this.cur.length; i++)
                if (!(this.cur[i] in rows)) {
                    this.cur.splice(i, 1);
                    i--;
                }

            var trs = this.table.getElements('tr');

            for (var rowPos = 0; rowPos < rows.length; rowPos++) {
                var rowId = rows[rowPos]['rowId'];
                tr_found = false;
                for (j = rowPos; j < trs.length; j++)
                    if (trs[j]['rowId'] == rowId) {
                        trs[rowPos].removeClass('over');
                        tr_found = true;
                        if (rowPos == j)
                            break;
                        trs[j].inject(trs[rowPos], 'before');
                        var tmpTr = trs[j];
                        trs.splice(j, 1);
                        trs.splice(rowPos, 0, tmpTr);
                        break;
                    }
                if (tr_found) // row already exists in the table
                    this.updateRow(trs[rowPos], fullUpdate);
                else { // else create a new row in the table
                    var tr = new Element('tr');

                    tr.addClass("menu-target");
                    tr['rowId'] = rows[rowPos]['rowId'];

                    tr._this = this;
                    tr.addEvent('contextmenu', function (e) {
                        if (!this._this.cur.contains(this.rowId))
                            this._this.selectRow(this.rowId);
                        return true;
                    });
                    tr.addEvent('click', function (e) {
                        e.stop();
                        if (e.control) {
                            // CTRL key was pressed
                            if (this._this.cur.contains(this.rowId)) {
                                // remove it
                                this._this.cur.erase(this.rowId);
                                // Remove selected style
                                this.removeClass('selected');
                            }
                            else {
                                this._this.cur.push(this.rowId);
                                // Add selected style
                                this.addClass('selected');
                            }
                        }
                        else {
                            if (e.shift && this._this.cur.length == 1) {
                                // Shift key was pressed
                                var first_row_id = this._this.cur[0];
                                var last_row_id = this.rowId;
                                this._this.cur.empty();
                                var trs = this._this.table.getElements('tr');
                                var select = false;
                                for (var i = 0; i < trs.length; i++) {
                                    var tr = trs[i];

                                    if ((tr.rowId == first_row_id) || (tr.rowId == last_row_id)) {
                                        this._this.cur.push(tr.rowId);
                                        tr.addClass('selected');
                                        select = !select;
                                    }
                                    else {
                                        if (select) {
                                            this._this.cur.push(tr.rowId);
                                            tr.addClass('selected');
                                        }
                                        else
                                            tr.removeClass('selected')
                                    }
                                }
                            } else {
                                // Simple selection
                                this._this.selectRow(this.rowId);
                            }
                        }
                        return false;
                    });

                    this.setupTrEvents(tr);

                    for (var j = 0 ; j < this.columns.length; j++) {
                        var td = new Element('td');
                        if ((this.columns[j].visible == '0') || this.columns[j].force_hide)
                            td.addClass('invisible');
                        td.injectInside(tr);
                    }

                    // Insert
                    if (rowPos >= trs.length) {
                        tr.inject(this.table);
                        trs.push(tr);
                    }
                    else {
                        tr.inject(trs[rowPos], 'before');
                        trs.splice(rowPos, 0, tr);
                    }

                    // Update context menu
                    if (this.context_menu)
                        this.context_menu.addTarget(tr);

                    this.updateRow(tr, true);
                }
            }

            rowPos = rows.length;

            while ((rowPos < trs.length) && (trs.length > 0)) {
                trs[trs.length - 1].dispose();
                trs.pop();
            }
        },

        setupTrEvents : function (tr) {},

        updateRow : function (tr, fullUpdate) {
            var row = this.rows.get(tr.rowId);
            data = row[fullUpdate ? 'full_data' : 'data'];

            tds = tr.getElements('td');
            for (var i = 0; i < this.columns.length; i++) {
                if (data.hasOwnProperty(this.columns[i].dataProperties[0]))
                    this.columns[i].updateTd(tds[i], row);
            }
            row['data'] = {};
        },

        removeRow : function (rowId) {
            this.cur.erase(rowId);
            var tr = this.getTrByRowId(rowId);
            if (tr != null) {
                tr.dispose();
                this.rows.erase(rowId);
                return true;
            }
            return false;
        },

        clear : function () {
            this.cur.empty();
            this.rows.empty();
            var trs = this.table.getElements('tr');
            while (trs.length > 0) {
                trs[trs.length - 1].dispose();
                trs.pop();
            }
        },

        selectedRowsIds : function () {
            return this.cur.slice();
        },

        getRowIds : function () {
            return this.rows.getKeys();
        },
    });

var TorrentsTable = new Class({
        Extends: DynamicTable,

        initColumns : function () {
            this.newColumn('priority', 'width: 30px', '#');
            this.newColumn('state_icon', 'width: 16px; cursor: default', '');
            this.newColumn('name', 'min-width: 200px', 'QBT_TR(Name)QBT_TR');
            this.newColumn('size', 'width: 100px', 'QBT_TR(Size)QBT_TR');
            this.newColumn('progress', 'width: 80px', 'QBT_TR(Done)QBT_TR');
            this.newColumn('num_seeds', 'width: 100px', 'QBT_TR(Seeds)QBT_TR');
            this.newColumn('num_leechs', 'width: 100px', 'QBT_TR(Peers)QBT_TR');
            this.newColumn('dlspeed', 'width: 100px', 'QBT_TR(Down Speed)QBT_TR');
            this.newColumn('upspeed', 'width: 100px', 'QBT_TR(Up Speed)QBT_TR');
            this.newColumn('eta', 'width: 100px', 'QBT_TR(ETA)QBT_TR');
            this.newColumn('ratio', 'width: 100px', 'QBT_TR(Ratio)QBT_TR');
            this.newColumn('label', 'width: 100px', 'QBT_TR(Label)QBT_TR');

            this.columns['state_icon'].onclick = '';
            this.columns['state_icon'].dataProperties[0] = 'state';

            this.columns['num_seeds'].dataProperties.push('num_complete');

            this.columns['num_leechs'].dataProperties.push('num_incomplete');

            this.initColumnsFunctions();
        },

        initColumnsFunctions : function () {

            // state_icon

            this.columns['state_icon'].updateTd = function (td, row) {
                var state = this.getRowValue(row);

                if (state == "forcedDL" || state == "metaDL")
                    state = "downloading";
                else if (state == "allocating")
                    state = "stalledDL";
                else if (state == "forcedUP")
                    state = "uploading";
                else if (state == "pausedDL")
                    state = "paused";
                else if (state == "pausedUP")
                    state = "completed";
                else if (state == "queuedDL" || state == "queuedUP")
                    state = "queued";
                else if (state == "checkingDL" || state == "checkingUP" ||
                        state == "queuedForChecking" || state == "checkingResumeData")
                    state = "checking";
                else if (state == "unknown" || state == "error" || state == "missingFiles")
                    state = "error";

                var img_path = 'images/skin/' + state + '.png';

                if (td.getChildren('img').length) {
                    var img = td.getChildren('img')[0];
                    if (img.src.indexOf(img_path) < 0)
                        img.set('src', img_path);
                }
                else
                    td.adopt(new Element('img', {
                        'src' : img_path,
                        'class' : 'statusIcon'
                    }));
            };

            // priority

            this.columns['priority'].updateTd = function (td, row) {
                var priority = this.getRowValue(row);
                td.set('html', priority < 1 ? '*' : priority);
            };

            this.columns['priority'].compareRows = function (row1, row2) {
                var row1_val = this.getRowValue(row1);
                var row2_val = this.getRowValue(row2);
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

            // name

            this.columns['name'].updateTd = function (td, row) {
                td.set('html', escapeHtml(this.getRowValue(row)));
            };

            // size

            this.columns['size'].updateTd = function (td, row) {
                var size = this.getRowValue(row);
                td.set('html', friendlyUnit(size, false));
            };

            // progress

            this.columns['progress'].updateTd = function (td, row) {
                var progress = this.getRowValue(row);
                var progressFormated = (progress * 100).round(1);
                if (progressFormated == 100.0 && progress != 1.0)
                    progressFormated = 99.9;

                if (td.getChildren('div').length) {
                    var div = td.getChildren('div')[0];
                    if (div.getValue() != progressFormated)
                        div.setValue(progressFormated);
                }
                else
                    td.adopt(new ProgressBar(progressFormated.toFloat(), {
                        'width' : 80
                    }));
            };

            // num_seeds

            this.columns['num_seeds'].updateTd = function (td, row) {
                var num_seeds = this.getRowValue(row, 0);
                var num_complete = this.getRowValue(row, 1);
                var html = num_seeds;
                if (num_complete != -1)
                    html += ' (' + num_complete + ')';
                td.set('html', html);
            };
            this.columns['num_seeds'].compareRows = function (row1, row2) {
                var num_seeds1 = this.getRowValue(row1, 0);
                var num_complete1 = this.getRowValue(row1, 1);

                var num_seeds2 = this.getRowValue(row2, 0);
                var num_complete2 = this.getRowValue(row2, 1);

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

            this.columns['dlspeed'].updateTd = function (td, row) {
                var speed = this.getRowValue(row);
                td.set('html', friendlyUnit(speed, true));
            };

            // upspeed

            this.columns['upspeed'].updateTd = this.columns['dlspeed'].updateTd;

            // eta

            this.columns['eta'].updateTd = function (td, row) {
                var eta = this.getRowValue(row);
                td.set('html', friendlyDuration(eta, true));
            };

            // ratio

            this.columns['ratio'].updateTd = function (td, row) {
                var ratio = this.getRowValue(row);
                var html = null;
                if (ratio == -1)
                    html = '∞';
                else
                    html = (Math.floor(100 * ratio) / 100).toFixed(2); //Don't round up
                td.set('html', html);
            };
        },

        applyFilter : function (row, filterName, labelName) {
            var state = row['full_data'].state;
            var inactive = false;
            var r;

            switch(filterName) {
                case 'downloading':
                    if (state != 'downloading' && !~state.indexOf('DL'))
                        return false;
                    break;
                case 'seeding':
                    if (state != 'uploading' && state != 'forcedUP' && state != 'stalledUP' && state != 'queuedUP' && state != 'checkingUP')
                        return false;
                    break;
                case 'completed':
                    if (state != 'uploading' && !~state.indexOf('UP'))
                        return false;
                    break;
                case 'paused':
                    if (!~state.indexOf('paused'))
                        return false;
                    break;
                case 'resumed':
                    if (~state.indexOf('paused'))
                        return false;
                    break;
                case 'inactive':
                    inactive = true;
                case 'active':
                    if (state == 'stalledDL')
                        r = (row['full_data'].upspeed > 0)
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

            if (labelName == LABELS_ALL)
                return true;

            if (labelName == LABELS_UNLABELLED && row['full_data'].label.length === 0)
                return true;

            if (labelName != genHash( row['full_data'].label) )
                return false;

            return true;
        },

        getFilteredTorrentsNumber : function (filterName) {
            var cnt = 0;
            var rows = this.rows.getValues();

            for (i = 0; i < rows.length; i++)
                if (this.applyFilter(rows[i], filterName, LABELS_ALL)) cnt++;

            return cnt;
        },

        getFilteredAndSortedRows : function () {
            var filteredRows = new Array();

            var rows = this.rows.getValues();

            for (i = 0; i < rows.length; i++)
                if (this.applyFilter(rows[i], selected_filter, selected_label)) {
                    filteredRows.push(rows[i]);
                    filteredRows[rows[i].rowId] = rows[i];
                }

            filteredRows.sort(function (row1, row2) {
                var column = this.columns[this.sortedColumn];
                res = column.compareRows(row1, row2);
                if (this.reverseSort == '0')
                    return res;
                else
                    return -res;
            }.bind(this));
            return filteredRows;
        },

        setupTrEvents : function (tr) {
            tr.addEvent('dblclick', function (e) {
                e.stop();
                this._this.selectRow(this.rowId);
                var row = this._this.rows.get(this.rowId);
                var state = row['full_data'].state;
                if (~state.indexOf('paused'))
                    startFN();
                else
                    pauseFN();
                return true;
            });
        },

        getCurrentTorrentHash : function () {
            return this.getSelectedRowId();
        },

        onSelectedRowChanged : function () {
            updatePropertiesPanel();
        }
    });

var TorrentPeersTable = new Class({
        Extends: DynamicTable,

        initColumns : function () {
            this.newColumn('country', 'width: 4px', '');
            this.newColumn('ip', 'width: 80px', 'QBT_TR(IP)QBT_TR');
            this.newColumn('port', 'width: 35px', 'QBT_TR(Port)QBT_TR');
            this.newColumn('client', 'width: 110px', 'QBT_TR(Client)QBT_TR');
            this.newColumn('progress', 'width: 30px', 'QBT_TR(Progress)QBT_TR');
            this.newColumn('dl_speed', 'width: 30px', 'QBT_TR(Down Speed)QBT_TR');
            this.newColumn('up_speed', 'width: 30px', 'QBT_TR(Up Speed)QBT_TR');
            this.newColumn('downloaded', 'width: 30px', 'QBT_TR(Downloaded)QBT_TR[CONTEXT=PeerListWidget]');
            this.newColumn('uploaded', 'width: 30px', 'QBT_TR(Uploaded)QBT_TR[CONTEXT=PeerListWidget]');
            this.newColumn('connection', 'width: 30px', 'QBT_TR(Connection)QBT_TR');
            this.newColumn('flags', 'width: 30px', 'QBT_TR(Flags)QBT_TR');
            this.newColumn('relevance', 'min-width: 30px', 'QBT_TR(Relevance)QBT_TR');

            this.columns['country'].dataProperties.push('country_code');
            this.columns['flags'].dataProperties.push('flags_desc');
            this.initColumnsFunctions();
        },

        initColumnsFunctions : function () {

            // country

            this.columns['country'].updateTd = function (td, row) {
                var country = this.getRowValue(row, 0);
                var country_code = this.getRowValue(row, 1);

                if (!country_code) {
                    if (td.getChildren('img').length)
                        td.getChildren('img')[0].dispose();
                    return;
                }

                var img_path = 'images/flags/' + country_code + '.png';

                if (td.getChildren('img').length) {
                    var img = td.getChildren('img')[0];
                    img.set('src', img_path);
                    img.set('alt', country);
                    img.set('title', country);
                }
                else
                    td.adopt(new Element('img', {
                        'src' : img_path,
                        'alt' : country,
                        'title' : country
                    }));
            };

            // ip

            this.columns['ip'].compareRows = function (row1, row2) {
                var ip1 = this.getRowValue(row1);
                var ip2 = this.getRowValue(row2);

                var a = ip1.split(".");
                var b = ip2.split(".");

                for (var i = 0; i < 4; i++){
                    if (a[i] != b[i])
                        return a[i] - b[i];
                }

                return 0;
            };

            // progress, relevance

            this.columns['progress'].updateTd = function (td, row) {
                var progress = this.getRowValue(row);
                var progressFormated = (progress * 100).round(1);
                if (progressFormated == 100.0 && progress != 1.0)
                    progressFormated = 99.9;
                progressFormated += "%";
                td.set('html', progressFormated);
            };

            this.columns['relevance'].updateTd = this.columns['progress'].updateTd;

            // dl_speed, up_speed

            this.columns['dl_speed'].updateTd = function (td, row) {
                var speed = this.getRowValue(row);
                if (speed == 0)
                    td.set('html', '');
                else
                    td.set('html', friendlyUnit(speed, true));
            };

            this.columns['up_speed'].updateTd = this.columns['dl_speed'].updateTd;

            // downloaded, uploaded

            this.columns['downloaded'].updateTd = function (td, row) {
                var downloaded = this.getRowValue(row);
                td.set('html', friendlyUnit(downloaded, false));
            };

            this.columns['uploaded'].updateTd = this.columns['downloaded'].updateTd;

            // flags

            this.columns['flags'].updateTd = function (td, row) {
                td.innerHTML = this.getRowValue(row, 0);
                td.title = this.getRowValue(row, 1);
            };

        }
    });

/*************************************************************/
