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

'use strict';

if (window.qBittorrent === undefined) {
    window.qBittorrent = {};
}

window.qBittorrent.PropTrackers = (function() {
    const exports = function() {
        return {
            updateData: updateData
        };
    };

    let current_hash = "";

    const torrentTrackersTable = new window.qBittorrent.DynamicTable.TorrentTrackersTable();
    let loadTrackersDataTimer;

    const loadTrackersData = function() {
        if ($('prop_trackers').hasClass('invisible')
            || $('propertiesPanel_collapseToggle').hasClass('panel-expand')) {
            // Tab changed, don't do anything
            return;
        }
        const new_hash = torrentsTable.getCurrentTorrentHash();
        if (new_hash === "") {
            torrentTrackersTable.clear();
            clearTimeout(loadTrackersDataTimer);
            loadTrackersDataTimer = loadTrackersData.delay(10000);
            return;
        }
        if (new_hash != current_hash) {
            torrentTrackersTable.clear();
            current_hash = new_hash;
        }
        const url = new URI('api/v2/torrents/trackers?hash=' + current_hash);
        new Request.JSON({
            url: url,
            noCache: true,
            method: 'get',
            onComplete: function() {
                clearTimeout(loadTrackersDataTimer);
                loadTrackersDataTimer = loadTrackersData.delay(10000);
            },
            onSuccess: function(trackers) {
                const selectedTrackers = torrentTrackersTable.selectedRowsIds();
                torrentTrackersTable.clear();

                if (trackers) {
                    trackers.each(function(tracker) {
                        let status;
                        switch (tracker.status) {
                            case 0:
                                status = "QBT_TR(Disabled)QBT_TR[CONTEXT=TrackerListWidget]";
                                break;
                            case 1:
                                status = "QBT_TR(Not contacted yet)QBT_TR[CONTEXT=TrackerListWidget]";
                                break;
                            case 2:
                                status = "QBT_TR(Working)QBT_TR[CONTEXT=TrackerListWidget]";
                                break;
                            case 3:
                                status = "QBT_TR(Updating...)QBT_TR[CONTEXT=TrackerListWidget]";
                                break;
                            case 4:
                                status = "QBT_TR(Not working)QBT_TR[CONTEXT=TrackerListWidget]";
                                break;
                        }

                        const row = {
                            rowId: tracker.url,
                            tier: tracker.tier,
                            url: tracker.url,
                            status: status,
                            peers: tracker.num_peers,
                            seeds: (tracker.num_seeds >= 0) ? tracker.num_seeds : "QBT_TR(N/A)QBT_TR[CONTEXT=TrackerListWidget]",
                            leeches: (tracker.num_leeches >= 0) ? tracker.num_leeches : "QBT_TR(N/A)QBT_TR[CONTEXT=TrackerListWidget]",
                            downloaded: (tracker.num_downloaded >= 0) ? tracker.num_downloaded : "QBT_TR(N/A)QBT_TR[CONTEXT=TrackerListWidget]",
                            message: tracker.msg
                        };

                        torrentTrackersTable.updateRowData(row);
                    });

                    torrentTrackersTable.updateTable(false);
                    torrentTrackersTable.altRow();

                    if (selectedTrackers.length > 0)
                        torrentTrackersTable.reselectRows(selectedTrackers);
                }
            }
        }).send();
    };

    const updateData = function() {
        clearTimeout(loadTrackersDataTimer);
        loadTrackersData();
    };

    const torrentTrackersContextMenu = new window.qBittorrent.ContextMenu.ContextMenu({
        targets: '#torrentTrackersTableDiv',
        menu: 'torrentTrackersMenu',
        actions: {
            AddTracker: function(element, ref) {
                addTrackerFN();
            },
            EditTracker: function(element, ref) {
                // only allow editing of one row
                element.firstChild.click();
                editTrackerFN(element);
            },
            RemoveTracker: function(element, ref) {
                removeTrackerFN(element);
            }
        },
        offsets: {
            x: -15,
            y: 2
        },
        onShow: function() {
            const selectedTrackers = torrentTrackersTable.selectedRowsIds();
            const containsStaticTracker = selectedTrackers.some(function(tracker) {
                return (tracker.indexOf("** [") === 0);
            });

            if (containsStaticTracker || (selectedTrackers.length === 0)) {
                this.hideItem('EditTracker');
                this.hideItem('RemoveTracker');
                this.hideItem('CopyTrackerUrl');
            }
            else {
                this.showItem('EditTracker');
                this.showItem('RemoveTracker');
                this.showItem('CopyTrackerUrl');
            }
        }
    });

    const addTrackerFN = function() {
        if (current_hash.length === 0) return;
        new MochaUI.Window({
            id: 'trackersPage',
            title: "QBT_TR(Trackers addition dialog)QBT_TR[CONTEXT=TrackersAdditionDialog]",
            loadMethod: 'iframe',
            contentURL: 'addtrackers.html?hash=' + current_hash,
            scrollbars: true,
            resizable: false,
            maximizable: false,
            closable: true,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: 500,
            height: 250,
            onCloseComplete: function() {
                updateData();
            }
        });
    };

    const editTrackerFN = function(element) {
        if (current_hash.length === 0) return;

        const trackerUrl = encodeURIComponent(element.childNodes[1].innerText);
        new MochaUI.Window({
            id: 'trackersPage',
            title: "QBT_TR(Tracker editing)QBT_TR[CONTEXT=TrackerListWidget]",
            loadMethod: 'iframe',
            contentURL: 'edittracker.html?hash=' + current_hash + '&url=' + trackerUrl,
            scrollbars: true,
            resizable: false,
            maximizable: false,
            closable: true,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: 500,
            height: 150,
            onCloseComplete: function() {
                updateData();
            }
        });
    };

    const removeTrackerFN = function(element) {
        if (current_hash.length === 0) return;

        const selectedTrackers = torrentTrackersTable.selectedRowsIds();
        new Request({
            url: 'api/v2/torrents/removeTrackers',
            method: 'post',
            data: {
                hash: current_hash,
                urls: selectedTrackers.join("|")
            },
            onSuccess: function() {
                updateData();
            }
        }).send();
    };

    new ClipboardJS('#CopyTrackerUrl', {
        text: function(trigger) {
            return torrentTrackersTable.selectedRowsIds().join("\n");
        }
    });

    torrentTrackersTable.setup('torrentTrackersTableDiv', 'torrentTrackersTableFixedHeaderDiv', torrentTrackersContextMenu);

    return exports();
})();
