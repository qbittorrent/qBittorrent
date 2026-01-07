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

window.qBittorrent ??= {};
window.qBittorrent.PropTrackers ??= (() => {
    const exports = () => {
        return {
            editTracker: editTrackerFN,
            updateData: updateData,
            clear: clear
        };
    };

    let current_hash = "";

    const torrentTrackersTable = new window.qBittorrent.DynamicTable.TorrentTrackersTable();
    let loadTrackersDataTimer = -1;

    const trackerStatusText = (tracker) => {
        if (tracker.updating)
            return "QBT_TR(Updating...)QBT_TR[CONTEXT=TrackerListWidget]";
        switch (tracker.status) {
            case 0:
                return "QBT_TR(Disabled)QBT_TR[CONTEXT=TrackerListWidget]";
            case 1:
                return "QBT_TR(Not contacted yet)QBT_TR[CONTEXT=TrackerListWidget]";
            case 2:
                return "QBT_TR(Working)QBT_TR[CONTEXT=TrackerListWidget]";
            case 4:
                return "QBT_TR(Not working)QBT_TR[CONTEXT=TrackerListWidget]";
            case 5:
                return "QBT_TR(Tracker error)QBT_TR[CONTEXT=TrackerListWidget]";
            case 6:
                return "QBT_TR(Unreachable)QBT_TR[CONTEXT=TrackerListWidget]";
        }
    };

    const loadTrackersData = () => {
        if (document.hidden)
            return;
        if (document.getElementById("propTrackers").classList.contains("invisible")
            || document.getElementById("propertiesPanel_collapseToggle").classList.contains("panel-expand")) {
            // Tab changed, don't do anything
            return;
        }
        const new_hash = torrentsTable.getCurrentTorrentID();
        if (new_hash === "") {
            torrentTrackersTable.clear();
            torrentTrackersTable.clearCollapseState();
            clearTimeout(loadTrackersDataTimer);
            return;
        }
        if (new_hash !== current_hash) {
            torrentTrackersTable.clear();
            torrentTrackersTable.clearCollapseState();
            current_hash = new_hash;
        }

        const url = new URL("api/v2/torrents/trackers", window.location);
        url.search = new URLSearchParams({
            hash: current_hash
        });
        fetch(url, {
                method: "GET",
                cache: "no-store"
            })
            .then(async (response) => {
                if (!response.ok)
                    return;

                const selectedTrackers = torrentTrackersTable.selectedRowsIds();

                const trackers = await response.json();
                if (trackers) {
                    torrentTrackersTable.clear();

                    const notApplicable = "QBT_TR(N/A)QBT_TR[CONTEXT=TrackerListWidget]";
                    trackers.each((tracker) => {
                        const row = {
                            rowId: tracker.url,
                            tier: (tracker.tier >= 0) ? tracker.tier : "",
                            btVersion: "",
                            url: tracker.url,
                            status: trackerStatusText(tracker),
                            peers: (tracker.num_peers >= 0) ? tracker.num_peers : notApplicable,
                            seeds: (tracker.num_seeds >= 0) ? tracker.num_seeds : notApplicable,
                            leeches: (tracker.num_leeches >= 0) ? tracker.num_leeches : notApplicable,
                            downloaded: (tracker.num_downloaded >= 0) ? tracker.num_downloaded : notApplicable,
                            message: tracker.msg,
                            nextAnnounce: tracker.next_announce,
                            minAnnounce: tracker.min_announce,
                            _isTracker: true,
                            _hasEndpoints: tracker.endpoints && (tracker.endpoints.length > 0),
                            _sortable: !tracker.url.startsWith("** [")
                        };

                        torrentTrackersTable.updateRowData(row);

                        if (tracker.endpoints !== undefined) {
                            for (const endpoint of tracker.endpoints) {
                                const row = {
                                    rowId: `endpoint|${tracker.url}|${endpoint.name}|${endpoint.bt_version}`,
                                    tier: "",
                                    btVersion: `v${endpoint.bt_version}`,
                                    url: endpoint.name,
                                    status: trackerStatusText(endpoint),
                                    peers: (endpoint.num_peers >= 0) ? endpoint.num_peers : notApplicable,
                                    seeds: (endpoint.num_seeds >= 0) ? endpoint.num_seeds : notApplicable,
                                    leeches: (endpoint.num_leeches >= 0) ? endpoint.num_leeches : notApplicable,
                                    downloaded: (endpoint.num_downloaded >= 0) ? endpoint.num_downloaded : notApplicable,
                                    message: endpoint.msg,
                                    nextAnnounce: endpoint.next_announce,
                                    minAnnounce: endpoint.min_announce,
                                    _isTracker: false,
                                    _tracker: tracker.url,
                                    _sortable: true,
                                };
                                torrentTrackersTable.updateRowData(row);
                            }
                        }
                    });

                    torrentTrackersTable.updateTable(false);

                    if (selectedTrackers.length > 0)
                        torrentTrackersTable.reselectRows(selectedTrackers);
                }
            })
            .finally(() => {
                clearTimeout(loadTrackersDataTimer);
                loadTrackersDataTimer = loadTrackersData.delay(window.qBittorrent.Client.getSyncMainDataInterval());
            });
    };

    const updateData = () => {
        clearTimeout(loadTrackersDataTimer);
        loadTrackersDataTimer = -1;
        loadTrackersData();
    };

    const torrentTrackersContextMenu = new window.qBittorrent.ContextMenu.ContextMenu({
        targets: "#torrentTrackersTableDiv",
        menu: "torrentTrackersMenu",
        actions: {
            AddTracker: (element, ref) => {
                addTrackerFN();
            },
            EditTracker: (element, ref) => {
                editTrackerFN(element);
            },
            RemoveTracker: (element, ref) => {
                removeTrackerFN(element);
            },
            ReannounceTrackers: (element, ref) => {
                reannounceTrackersFN(element, torrentTrackersTable.selectedRowsIds());
            },
            ReannounceAllTrackers: (element, ref) => {
                reannounceTrackersFN(element, []);
            }
        },
        offsets: {
            x: 0,
            y: 2
        },
        onShow: function() {
            const selectedTrackers = torrentTrackersTable.selectedRowsIds();
            const containsStaticTracker = selectedTrackers.some((tracker) => {
                return tracker.startsWith("** [") || tracker.startsWith("endpoint|");
            });

            if (containsStaticTracker || (selectedTrackers.length === 0)) {
                this.hideItem("EditTracker");
                this.hideItem("RemoveTracker");
                this.hideItem("CopyTrackerUrl");
                this.hideItem("ReannounceTrackers");
            }
            else {
                if (selectedTrackers.length === 1)
                    this.showItem("EditTracker");
                else
                    this.hideItem("EditTracker");

                this.showItem("RemoveTracker");
                this.showItem("CopyTrackerUrl");

                const torrentHash = torrentsTable.getCurrentTorrentID();
                if (torrentsTable.isStopped(torrentHash)) {
                    this.hideItem("ReannounceTrackers");
                    this.hideItem("ReannounceAllTrackers");
                }
                else {
                    this.showItem("ReannounceTrackers");
                    this.showItem("ReannounceAllTrackers");
                }
            }
        }
    });

    const addTrackerFN = () => {
        if (current_hash.length === 0)
            return;

        const selectedTorrents = torrentsTable.selectedRowsIds();
        if (selectedTorrents.length !== 0)
            current_hash = selectedTorrents.map(encodeURIComponent).join("|");

        new MochaUI.Window({
            id: "trackersPage",
            icon: "images/qbittorrent-tray.svg",
            title: "QBT_TR(Add trackers)QBT_TR[CONTEXT=TrackersAdditionDialog]",
            loadMethod: "iframe",
            contentURL: `addtrackers.html?v=${CACHEID}&hash=${current_hash}`,
            scrollbars: true,
            resizable: false,
            maximizable: false,
            closable: true,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: window.qBittorrent.Dialog.limitWidthToViewport(500),
            height: 260,
            onCloseComplete: () => {
                updateData();
            }
        });
    };

    const editTrackerFN = (element) => {
        if (current_hash.length === 0)
            return;

        const tracker = torrentTrackersTable.getRow(torrentTrackersTable.getSelectedRowId());
        const contentURL = new URL("edittracker.html", window.location);
        contentURL.search = new URLSearchParams({
            v: "${CACHEID}",
            hash: current_hash,
            url: tracker.full_data.url,
            tier: tracker.full_data.tier
        });

        new MochaUI.Window({
            id: "trackersPage",
            icon: "images/qbittorrent-tray.svg",
            title: "QBT_TR(Tracker editing)QBT_TR[CONTEXT=TrackerListWidget]",
            loadMethod: "iframe",
            contentURL: contentURL.toString(),
            scrollbars: true,
            resizable: false,
            maximizable: false,
            closable: true,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: window.qBittorrent.Dialog.limitWidthToViewport(500),
            height: 200,
            onCloseComplete: () => {
                updateData();
            }
        });
    };

    const removeTrackerFN = (element) => {
        if (current_hash.length === 0)
            return;

        const selectedTorrents = torrentsTable.selectedRowsIds();
        if (selectedTorrents.length !== 0)
            current_hash = selectedTorrents.map(encodeURIComponent).join("|");

        fetch("api/v2/torrents/removeTrackers", {
                method: "POST",
                body: new URLSearchParams({
                    hash: current_hash,
                    urls: torrentTrackersTable.selectedRowsIds().map(encodeURIComponent).join("|")
                })
            })
            .then((response) => {
                if (!response.ok)
                    return;

                updateData();
            });
    };

    const reannounceTrackersFN = (element, trackers) => {
        if (current_hash.length === 0)
            return;

        const body = new URLSearchParams({
            hashes: current_hash
        });
        if (trackers.length > 0)
            body.set("urls", trackers.map(encodeURIComponent).join("|"));

        fetch("api/v2/torrents/reannounce", {
                method: "POST",
                body: body
            })
            .then((response) => {
                if (!response.ok)
                    return;

                updateData();
            });
    };

    const clear = () => {
        torrentTrackersTable.clear();
    };

    document.getElementById("CopyTrackerUrl").addEventListener("click", async (event) => {
        const text = torrentTrackersTable.selectedRowsIds().join("\n");
        await clipboardCopy(text);
    });

    torrentTrackersTable.setup("torrentTrackersTableDiv", "torrentTrackersTableFixedHeaderDiv", torrentTrackersContextMenu, true);

    return exports();
})();
Object.freeze(window.qBittorrent.PropTrackers);
