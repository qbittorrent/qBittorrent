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

var dynamicTable = new Class({

        initialize : function () {},

        setup : function (table, context_menu) {
            this.table = $(table);
            this.rows = new Hash();
            this.cur = new Array();
            this.columns = new Array();
            this.context_menu = context_menu;
            this.sortedColumn = getLocalStorageItem('sorted_column', 'name');
            this.reverseSort = getLocalStorageItem('reverse_sort', '0');
            this.initColumns();
            this.loadColumnsOrder();
            this.updateHeader();
        },

        initColumns : function () {
            this.newColumn('priority', 'width: 30px; cursor: pointer', '#');
            this.newColumn('state_icon', 'width: 16px', '');
            this.newColumn('name', 'min-width: 200px; cursor: pointer', 'QBT_TR(Name)QBT_TR');
            this.newColumn('size', 'width: 100px; cursor: pointer', 'QBT_TR(Size)QBT_TR');
            this.newColumn('progress', 'width: 80px; cursor: pointer', 'QBT_TR(Done)QBT_TR');
            this.newColumn('num_seeds', 'width: 100px; cursor: pointer', 'QBT_TR(Seeds)QBT_TR');
            this.newColumn('num_leechs', 'width: 100px; cursor: pointer', 'QBT_TR(Peers)QBT_TR');
            this.newColumn('dlspeed', 'width: 100px; cursor: pointer', 'QBT_TR(Down Speed)QBT_TR');
            this.newColumn('upspeed', 'width: 100px; cursor: pointer', 'QBT_TR(Up Speed)QBT_TR');
            this.newColumn('eta', 'width: 100px; cursor: pointer', 'QBT_TR(ETA)QBT_TR');
            this.newColumn('ratio', 'width: 100px; cursor: pointer', 'QBT_TR(Ratio)QBT_TR');

            this.columns['state_icon'].onclick = '';
            this.columns['state_icon'].dataProperties[0] = 'state';

            this.columns['num_seeds'].dataProperties.push('num_complete');

            this.columns['num_leechs'].dataProperties.push('num_incomplete');

            this.initColumnsFunctions();
        },

        newColumn : function (name, style, caption) {
            var column = {};
            column['name'] = name;
            column['visible'] = getLocalStorageItem('column_' + name + '_visible', '1');
            column['force_hide'] = false;
            column['caption'] = caption;
            column['style'] = style;
            column['onclick'] = 'setSortedColumn(\'' + name + '\');';
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

            $('torrentTableHeader').appendChild(new Element('th'));
        },

        loadColumnsOrder : function () {
            columnsOrder = ['state_icon']; // status icon column is always the first
            val = localStorage.getItem('columns_order');
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
            localStorage.setItem('columns_order', val);
        },

        updateHeader : function () {
            ths = $('torrentTableHeader').getElements('th');

            for (var i = 0; i < ths.length; i++) {
                th = ths[i];
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
            var ths = $('torrentTableHeader').getElements('th');
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
            localStorage.setItem('sorted_column', column);
            localStorage.setItem('reverse_sort', this.reverseSort);
        },

        getCurrentTorrentHash : function () {
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
                this.cur.push(tr.hash);
                if (!tr.hasClass('selected'))
                    tr.addClass('selected');
            }
        },

        selectRow : function (hash) {
            this.cur.empty();
            this.cur.push(hash);
            var trs = this.table.getElements('tr');
            for (var i = 0; i < trs.length; i++) {
                var tr = trs[i];
                if (tr.hash == hash) {
                    if (!tr.hasClass('selected'))
                        tr.addClass('selected');
                }
                else
                if (tr.hasClass('selected'))
                    tr.removeClass('selected');
            }
        },

        updateRowData : function (data) {
            var hash = data['hash'];
            var row;

            if (!this.rows.has(hash)) {
                row = {};
                this.rows.set(hash, row);
                row['full_data'] = {};
                row['hash'] = hash;
            }
            else
                row = this.rows.get(hash);

            row['data'] = data;

            for(var x in data)
                row['full_data'][x] = data[x];
        },

        applyFilter : function (row, filterName, labelName) {
            var state = row['full_data'].state;
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
                    if (state != 'pausedDL')
                        return false;
                    break;
                case 'resumed':
                    if (~state.indexOf('paused'))
                        return false;
                    break;
                case 'active':
                    if (state != 'downloading' && state != 'forcedDL' && state != 'uploading' && state != 'forcedUP')
                        return false;
                    break;
                case 'inactive':
                    if (state == 'downloading' || state == 'forcedDL' || state == 'uploading'  || state == 'forcedUP')
                        return false;
                    break;
            }

            if (labelName == null)
                return true;

            if (labelName != row['full_data'].label)
                return false;

            return true;
        },

        getFilteredAndSortedRows : function () {
            var filteredRows = new Array();

            var rows = this.rows.getValues();

            for (i = 0; i < rows.length; i++)
                if (this.applyFilter(rows[i], selected_filter, selected_label)) {
                    filteredRows.push(rows[i]);
                    filteredRows[rows[i].hash] = rows[i];
                }

            filteredRows.sort(function (row1, row2) {
                column = this.columns[this.sortedColumn];
                res = column.compareRows(row1, row2);
                if (this.reverseSort == '0')
                    return res;
                else
                    return -res;
            }.bind(this));
            return filteredRows;
        },

        getTrByHash : function (hash) {
            trs = this.table.getElements('tr');
            for (var i = 0; i < trs.length; i++)
                if (trs[i].hash == hash)
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
                var hash = rows[rowPos]['hash'];
                tr_found = false;
                for (j = rowPos; j < trs.length; j++)
                    if (trs[j]['hash'] == hash) {
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
                    tr['hash'] = rows[rowPos]['hash'];

                    tr.addEvent('contextmenu', function (e) {
                        if (!myTable.cur.contains(this.hash))
                            myTable.selectRow(this.hash);
                        return true;
                    });
                    tr.addEvent('dblclick', function (e) {
                        e.stop();
                        myTable.selectRow(this.hash);
                        var row = myTable.rows.get(this.hash);
                        var state = row['full_data'].state;
                        if (~state.indexOf('paused'))
                            startFN();
                        else
                            pauseFN();
                        return true;
                    });
                    tr.addEvent('click', function (e) {
                        e.stop();
                        if (e.control) {
                            // CTRL key was pressed
                            if (myTable.cur.contains(this.hash)) {
                                // remove it
                                myTable.cur.erase(this.hash);
                                // Remove selected style
                                this.removeClass('selected');
                            }
                            else {
                                myTable.cur.push(this.hash);
                                // Add selected style
                                this.addClass('selected');
                            }
                        }
                        else {
                            if (e.shift && myTable.cur.length == 1) {
                                // Shift key was pressed
                                var first_row_hash = myTable.cur[0];
                                var last_row_hash = this.hash;
                                myTable.cur.empty();
                                var trs = myTable.table.getElements('tr');
                                var select = false;
                                for (var i = 0; i < trs.length; i++) {
                                    var tr = trs[i];

                                    if ((tr.hash == first_row_hash) || (tr.hash == last_row_hash)) {
                                        myTable.cur.push(tr.hash);
                                        tr.addClass('selected');
                                        select = !select;
                                    }
                                    else {
                                        if (select) {
                                            myTable.cur.push(tr.hash);
                                            tr.addClass('selected');
                                        }
                                        else
                                            tr.removeClass('selected')
                                    }
                                }
                            } else {
                                // Simple selection
                                myTable.selectRow(this.hash);
                                updatePropertiesPanel();
                            }
                        }
                        return false;
                    });

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

        updateRow : function (tr, fullUpdate) {
            var row = this.rows.get(tr.hash);
            data = row[fullUpdate ? 'full_data' : 'data'];

            tds = tr.getElements('td');

            for(var prop in data)
                for (var i = 0; i < this.columns.length; i++)
                    for (var j = 0; j < this.columns[i].dataProperties.length; j++)
                        if (this.columns[i].dataProperties[j] == prop)
                            this.columns[i].updateTd(tds[i], row);

            if (this.cur.contains(tr.hash)) {
                if (!tr.hasClass('selected'))
                    tr.addClass('selected');
            }
            else {
                if (tr.hasClass('selected'))
                    tr.removeClass('selected');
            }
        },

        removeRow : function (hash) {
            this.cur.erase(hash);
            var tr = this.getTrByHash(hash);
            if (tr != null) {
                tr.dispose();
                this.rows.erase(hash);
                return true;
            }
            return false;
        },

        selectedIds : function () {
            return this.cur;
        },

        getRowIds : function () {
            return this.rows.getKeys();
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
                else if (state == "unknown")
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
                td.set('html', priority < 0 ? null : priority);
            };
            this.columns['priority'].compareRows = function (row1, row2) {
                var row1_val = this.getRowValue(row1);
                var row2_val = this.getRowValue(row2);
                if (row1_val == -1)
                    row1_val = 1000000;
                if (row2_val == -1)
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
        }

    });
//dynamicTable.implement(new Options);
//dynamicTable.implement(new Events);

/*************************************************************/
