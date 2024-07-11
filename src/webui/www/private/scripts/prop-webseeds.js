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

"use strict";

window.qBittorrent ??= {};
window.qBittorrent.PropWebseeds ??= (() => {
    const exports = () => {
        return {
            updateData: updateData
        };
    };

    const torrentWebseedsTable = new window.qBittorrent.DynamicTable.TorrentWebseedsTable();

    let current_hash = "";

    let loadWebSeedsDataTimer = -1;
    const loadWebSeedsData = function() {
        if ($("propWebSeeds").hasClass("invisible")
            || $("propertiesPanel_collapseToggle").hasClass("panel-expand")) {
            // Tab changed, don't do anything
            return;
        }
        const new_hash = torrentsTable.getCurrentTorrentID();
        if (new_hash === "") {
            torrentWebseedsTable.clear();
            clearTimeout(loadWebSeedsDataTimer);
            loadWebSeedsDataTimer = loadWebSeedsData.delay(10000);
            return;
        }
        if (new_hash !== current_hash) {
            torrentWebseedsTable.clear();
            current_hash = new_hash;
        }
        new Request.JSON({
            url: new URI("api/v2/torrents/webseeds").setData("hash", current_hash),
            method: "get",
            noCache: true,
            onComplete: function() {
                clearTimeout(loadWebSeedsDataTimer);
                loadWebSeedsDataTimer = loadWebSeedsData.delay(10000);
            },
            onSuccess: function(webseeds) {
                const selectedWebseeds = torrentWebseedsTable.selectedRowsIds();
                torrentWebseedsTable.clear();

                if (webseeds) {
                    // Update WebSeeds data
                    webseeds.each((webseed) => {
                        torrentWebseedsTable.updateRowData({
                            rowId: webseed.url,
                            url: webseed.url,
                        });
                    });
                }

                torrentWebseedsTable.updateTable(false);

                if (selectedWebseeds.length > 0)
                    torrentWebseedsTable.reselectRows(selectedWebseeds);
            }
        }).send();
    };

    const updateData = function() {
        clearTimeout(loadWebSeedsDataTimer);
        loadWebSeedsDataTimer = -1;
        loadWebSeedsData();
    };

    torrentWebseedsTable.setup("torrentWebseedsTableDiv", "torrentWebseedsTableFixedHeaderDiv");

    return exports();
})();
Object.freeze(window.qBittorrent.PropWebseeds);
