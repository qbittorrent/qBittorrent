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

        setup : function (table, progressIndex, context_menu) {
            this.table = $(table);
            this.rows = new Hash();
            this.cur = new Array();
            this.priority_hidden = false;
            this.progressIndex = progressIndex;
            this.context_menu = context_menu;
            this.table.sortedColumn = getLocalStorageItem('sorted_column', 'name');
            this.table.reverseSort = getLocalStorageItem('reverse_sort', 'false');;
        },

        setSortedColumn : function (column) {
            if (column != this.table.sortedColumn) {
                this.table.sortedColumn = column;
                this.table.reverseSort = 'false';
            } else {
                // Toggle sort order
                this.table.reverseSort = this.table.reverseSort == 'true' ? 'false' : 'true';
            }
            localStorage.setItem('sorted_column', column);
            localStorage.setItem('reverse_sort', this.table.reverseSort);
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

        hidePriority : function () {
            if (this.priority_hidden)
                return;
            $('prioHeader').addClass('invisible');
            var trs = this.table.getElements('tr');
            trs.each(function (tr, i) {
                var tds = tr.getElements('td');
                tds[2].addClass('invisible');
            }.bind(this));
            this.priority_hidden = true;
        },

        showPriority : function () {
            if (!this.priority_hidden)
                return;
            $('prioHeader').removeClass('invisible');
            var trs = this.table.getElements('tr');
            trs.each(function (tr, i) {
                var tds = tr.getElements('td');
                tds[2].removeClass('invisible');
            }.bind(this));
            this.priority_hidden = false;
        },

        insertRow : function (id, row, data, status, pos) {
            if (this.rows.has(id)) {
                return;
            }
            var tr = new Element('tr');
            tr.addClass("menu-target");
            this.rows.set(id, tr);
            for (var i = 0; i < row.length; i++) {
                var td = new Element('td');
                if (i == this.progressIndex) {
                    td.adopt(new ProgressBar(row[i].toFloat(), {
                            'id' : 'pb_' + id,
                            'width' : 80
                        }));
                    if (typeof data[i] != 'undefined')
                        td.set('data-raw', data[i])
                } else {
                    if (i == 0) {
                        td.adopt(new Element('img', {
                                'src' : row[i],
                                'class' : 'statusIcon'
                            }));
                    } else {
                        if (i == 2) {
                            // Priority
                            if (this.priority_hidden)
                                td.addClass('invisible');
                        }
                        td.set('html', row[i]);
                        if (typeof data[i] != 'undefined')
                            td.set('data-raw', data[i])
                    }
                }
                td.injectInside(tr);
            };

            tr.addEvent('mouseover', function (e) {
                tr.addClass('over');
            }.bind(this));
            tr.addEvent('mouseout', function (e) {
                tr.removeClass('over');
            }.bind(this));
            tr.addEvent('contextmenu', function (e) {
                if (!this.cur.contains(id)) {
                    // Remove selected style from previous ones
                    for (i = 0; i < this.cur.length; i++) {
                        if (this.rows.has(this.cur[i])) {
                            var temptr = this.rows.get(this.cur[i]);
                            temptr.removeClass('selected');
                        }
                    }
                    this.cur.empty();
                    this.cur[this.cur.length] = id;
                    temptr = this.rows.get(id);
                    temptr.addClass("selected");
                }
                return true;
            }.bind(this));
            tr.addEvent('click', function (e) {
                e.stop();
                if (e.control) {
                    // CTRL key was pressed
                    if (this.cur.contains(id)) {
                        // remove it
                        this.cur.erase(id);
                        // Remove selected style
                        if (this.rows.has(id)) {
                            temptr = this.rows.get(id);
                            temptr.removeClass('selected');
                        }
                    } else {
                        this.cur[this.cur.length] = id;
                        // Add selected style
                        if (this.rows.has(id)) {
                            temptr = this.rows.get(id);
                            temptr.addClass('selected');
                        }
                    }
                } else {
                    if (e.shift && this.cur.length == 1) {
                        // Shift key was pressed
                        var first_id = this.cur[0];
                        var first_tr = this.rows.get(first_id);
                        var last_id = id;
                        var last_tr = this.rows.get(last_id);
                        var all_trs = this.table.getChildren('tr');
                        var index_first_tr = all_trs.indexOf(first_tr);
                        var index_last_tr = all_trs.indexOf(last_tr);
                        var trs_to_select = all_trs.filter(function (item, index) {
                                if (index_first_tr < index_last_tr)
                                    return (index > index_first_tr) && (index <= index_last_tr);
                                else
                                    return (index < index_first_tr) && (index >= index_last_tr);
                            });
                        trs_to_select.each(function (item, index) {
                            // Add to selection
                            this.cur[this.cur.length] = this.getRowId(item);
                            // Select it visually
                            item.addClass('selected');
                        }.bind(this));
                    } else {
                        // Simple selection
                        // Remove selected style from previous ones
                        for (i = 0; i < this.cur.length; i++) {
                            if (this.rows.has(this.cur[i])) {
                                var temptr = this.rows.get(this.cur[i]);
                                temptr.removeClass('selected');
                            }
                        }
                        this.cur.empty();
                        // Add selected style to new one
                        if (this.rows.has(id)) {
                            temptr = this.rows.get(id);
                            temptr.addClass('selected');
                        }
                        this.cur[0] = id;
                        // TODO: Warn Properties panel
                    }
                }
                return false;
            }.bind(this));

            // Insert
            var trs = this.table.getChildren('tr');
            if (pos >= trs.length) {
                tr.inject(this.table);
            } else {
                tr.inject(trs[pos], 'before');
            }
            //tr.injectInside(this.table);
            // Update context menu
            this.context_menu.addTarget(tr);
        },

        selectAll : function () {
            this.cur.empty();
            this.rows.each(function (tr, id) {
                this.cur[this.cur.length] = id;
                if (!tr.hasClass('selected')) {
                    tr.addClass('selected');
                }
            }, this);
        },

        updateRow : function (id, row, data, status, newpos) {
            if (!this.rows.has(id)) {
                return false;
            }
            
            var tr = this.rows.get(id);
            var tds = tr.getElements('td');
            for (var i = 0; i < row.length; i++) {
                if (i == 1)
                    continue; // Do not refresh name
                if (i == this.progressIndex) {
                    $('pb_' + id).setValue(row[i]);
                } else {
                    if (i == 0) {
                        tds[i].getChildren('img')[0].set('src', row[i]);
                    } else {
                        tds[i].set('html', row[i]);
                    }
                }
                if (typeof data[i] != 'undefined')
                    tds[i].set('data-raw', data[i])
            };
            
            // Prevent freezing of the backlight.
            tr.removeClass('over');
            
            // Move to 'newpos'
            var trs = this.table.getChildren('tr');
            if (newpos >= trs.length) {
                tr.inject(this.table);
            } else {
                tr.inject(trs[newpos], 'before');
            }
            
            return true;
        },

        removeRow : function (id) {
            if (this.cur.contains(id)) {
                this.cur.erase(id);
            }
            if (this.rows.has(id)) {
                var tr = this.rows.get(id);
                tr.dispose();
                this.altRow();
                this.rows.erase(id);
                return true;
            }
            return false;
        },

        selectedIds : function () {
            return this.cur;
        },

        getRowId : function (tr) {
            return this.rows.keyOf(tr);
        },

        getRowIds : function () {
            return this.rows.getKeys();
        }

    });
//dynamicTable.implement(new Options);
//dynamicTable.implement(new Events);

/*************************************************************/
