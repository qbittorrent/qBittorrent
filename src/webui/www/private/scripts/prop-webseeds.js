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
            updateData: updateData,
            clear: clear
        };
    };

    const torrentWebseedsTable = new window.qBittorrent.DynamicTable.TorrentWebseedsTable();

    let current_hash = "";

    let loadWebSeedsDataTimer = -1;
    const loadWebSeedsData = () => {
        if ($("propWebSeeds").classList.contains("invisible")
            || $("propertiesPanel_collapseToggle").classList.contains("panel-expand")) {
            // Tab changed, don't do anything
            return;
        }
        const new_hash = torrentsTable.getCurrentTorrentID();
        if (new_hash === "") {
            torrentWebseedsTable.clear();
            clearTimeout(loadWebSeedsDataTimer);
            return;
        }
        if (new_hash !== current_hash) {
            torrentWebseedsTable.clear();
            current_hash = new_hash;
        }

        const url = new URL("api/v2/torrents/webseeds", window.location);
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

                const selectedWebseeds = torrentWebseedsTable.selectedRowsIds();
                torrentWebseedsTable.clear();

                const webseeds = await response.json();
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
            })
            .finally(() => {
                clearTimeout(loadWebSeedsDataTimer);
                loadWebSeedsDataTimer = loadWebSeedsData.delay(10000);
            });
    };

    const updateData = () => {
        clearTimeout(loadWebSeedsDataTimer);
        loadWebSeedsDataTimer = -1;
        loadWebSeedsData();
    };

    const torrentWebseedsContextMenu = new window.qBittorrent.ContextMenu.ContextMenu({
        targets: "#torrentWebseedsTableDiv",
        menu: "torrentWebseedsMenu",
        actions: {
            AddWebSeeds: (element, ref) => {
                addWebseedFN();
            },
            EditWebSeed: (element, ref) => {
                // only allow editing of one row
                element.firstElementChild.click();
                editWebSeedFN(element);
            },
            RemoveWebSeed: (element, ref) => {
                removeWebSeedFN(element);
            }
        },
        offsets: {
            x: 0,
            y: 2
        },
        onShow: function() {
            const selectedWebseeds = torrentWebseedsTable.selectedRowsIds();

            if (selectedWebseeds.length === 0) {
                this.hideItem("EditWebSeed");
                this.hideItem("RemoveWebSeed");
                this.hideItem("CopyWebseedUrl");
            }
            else {
                if (selectedWebseeds.length === 1)
                    this.showItem("EditWebSeed");
                else
                    this.hideItem("EditWebSeed");

                this.showItem("RemoveWebSeed");
                this.showItem("CopyWebseedUrl");
            }
        }
    });

    const addWebseedFN = () => {
        if (current_hash.length === 0)
            return;

        new MochaUI.Window({
            id: "webseedsPage",
            title: "QBT_TR(Add web seeds)QBT_TR[CONTEXT=HttpServer]",
            loadMethod: "iframe",
            contentURL: `addwebseeds.html?hash=${current_hash}`,
            scrollbars: true,
            resizable: false,
            maximizable: false,
            closable: true,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: 500,
            height: 260,
            onCloseComplete: () => {
                updateData();
            }
        });
    };

    const editWebSeedFN = (element) => {
        if (current_hash.length === 0)
            return;

        const selectedWebseeds = torrentWebseedsTable.selectedRowsIds();
        if (selectedWebseeds.length > 1)
            return;

        const webseedUrl = selectedWebseeds[0];

        new MochaUI.Window({
            id: "webseedsPage",
            title: "QBT_TR(Web seed editing)QBT_TR[CONTEXT=PropertiesWidget]",
            loadMethod: "iframe",
            contentURL: `editwebseed.html?hash=${current_hash}&url=${encodeURIComponent(webseedUrl)}`,
            scrollbars: true,
            resizable: false,
            maximizable: false,
            closable: true,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: 500,
            height: 150,
            onCloseComplete: () => {
                updateData();
            }
        });
    };

    const removeWebSeedFN = (element) => {
        if (current_hash.length === 0)
            return;

        fetch("api/v2/torrents/removeWebSeeds", {
                method: "POST",
                body: new URLSearchParams({
                    hash: current_hash,
                    urls: torrentWebseedsTable.selectedRowsIds().map(webseed => encodeURIComponent(webseed)).join("|")
                })
            })
            .then((response) => {
                if (!response.ok)
                    return;

                updateData();
            });
    };

    const clear = () => {
        torrentWebseedsTable.clear();
    };

    new ClipboardJS("#CopyWebseedUrl", {
        text: (trigger) => {
            return torrentWebseedsTable.selectedRowsIds().join("\n");
        }
    });

    torrentWebseedsTable.setup("torrentWebseedsTableDiv", "torrentWebseedsTableFixedHeaderDiv", torrentWebseedsContextMenu);

    return exports();
})();
Object.freeze(window.qBittorrent.PropWebseeds);
