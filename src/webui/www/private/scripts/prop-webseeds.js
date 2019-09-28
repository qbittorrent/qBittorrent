/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Diego de las Heras <ngosang@hotmail.es>
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

'use strict';

if (window.qBittorrent === undefined) {
    window.qBittorrent = {};
}

window.qBittorrent.PropWebseeds = (function() {
    const exports = function() {
        return {
            updateData: updateData
        };
    };

    const webseedsDynTable = new Class({

        initialize: function() {},

        setup: function(table) {
            this.table = $(table);
            this.rows = new Hash();
        },

        removeRow: function(url) {
            if (this.rows.has(url)) {
                const tr = this.rows.get(url);
                tr.dispose();
                this.rows.erase(url);
                return true;
            }
            return false;
        },

        removeAllRows: function() {
            this.rows.each(function(tr, url) {
                this.removeRow(url);
            }.bind(this));
        },

        updateRow: function(tr, row) {
            const tds = tr.getElements('td');
            for (let i = 0; i < row.length; ++i) {
                tds[i].set('html', row[i]);
            }
            return true;
        },

        insertRow: function(row) {
            const url = row[0];
            if (this.rows.has(url)) {
                const tableRow = this.rows.get(url);
                this.updateRow(tableRow, row);
                return;
            }
            //this.removeRow(id);
            const tr = new Element('tr');
            this.rows.set(url, tr);
            for (let i = 0; i < row.length; ++i) {
                const td = new Element('td');
                td.set('html', row[i]);
                td.injectInside(tr);
            }
            tr.injectInside(this.table);
        },
    });

    let current_hash = "";

    let loadWebSeedsDataTimer;
    const loadWebSeedsData = function() {
        if ($('prop_webseeds').hasClass('invisible')
            || $('propertiesPanel_collapseToggle').hasClass('panel-expand')) {
            // Tab changed, don't do anything
            return;
        }
        const new_hash = torrentsTable.getCurrentTorrentHash();
        if (new_hash === "") {
            wsTable.removeAllRows();
            clearTimeout(loadWebSeedsDataTimer);
            loadWebSeedsDataTimer = loadWebSeedsData.delay(10000);
            return;
        }
        if (new_hash != current_hash) {
            wsTable.removeAllRows();
            current_hash = new_hash;
        }
        const url = new URI('api/v2/torrents/webseeds?hash=' + current_hash);
        new Request.JSON({
            url: url,
            noCache: true,
            method: 'get',
            onFailure: function() {
                $('error_div').set('html', 'QBT_TR(qBittorrent client is not reachable)QBT_TR[CONTEXT=HttpServer]');
                clearTimeout(loadWebSeedsDataTimer);
                loadWebSeedsDataTimer = loadWebSeedsData.delay(20000);
            },
            onSuccess: function(webseeds) {
                $('error_div').set('html', '');
                if (webseeds) {
                    // Update WebSeeds data
                    webseeds.each(function(webseed) {
                        const row = [];
                        row.length = 1;
                        row[0] = webseed.url;
                        wsTable.insertRow(row);
                    });
                }
                else {
                    wsTable.removeAllRows();
                }
                clearTimeout(loadWebSeedsDataTimer);
                loadWebSeedsDataTimer = loadWebSeedsData.delay(10000);
            }
        }).send();
    };

    const updateData = function() {
        clearTimeout(loadWebSeedsDataTimer);
        loadWebSeedsData();
    };

    const wsTable = new webseedsDynTable();
    wsTable.setup($('webseedsTable'));

    return exports();
})();
