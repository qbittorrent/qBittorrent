/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2018  Thomas Piccirello <thomas.piccirello@gmail.com>
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

window.qBittorrent.PropPeers = (function() {
    const exports = function() {
        return {
            updateData: updateData
        }
    };

    const torrentPeersTable = new window.qBittorrent.DynamicTable.TorrentPeersTable();
    let loadTorrentPeersTimer;
    let syncTorrentPeersLastResponseId = 0;
    let show_flags = true;

    const loadTorrentPeersData = function() {
        if ($('prop_peers').hasClass('invisible')
            || $('propertiesPanel_collapseToggle').hasClass('panel-expand')) {
            syncTorrentPeersLastResponseId = 0;
            torrentPeersTable.clear();
            return;
        }
        const current_hash = torrentsTable.getCurrentTorrentHash();
        if (current_hash === "") {
            syncTorrentPeersLastResponseId = 0;
            torrentPeersTable.clear();
            clearTimeout(loadTorrentPeersTimer);
            loadTorrentPeersTimer = loadTorrentPeersData.delay(getSyncMainDataInterval());
            return;
        }
        const url = new URI('api/v2/sync/torrentPeers');
        url.setData('rid', syncTorrentPeersLastResponseId);
        url.setData('hash', current_hash);
        new Request.JSON({
            url: url,
            noCache: true,
            method: 'get',
            onComplete: function() {
                clearTimeout(loadTorrentPeersTimer);
                loadTorrentPeersTimer = loadTorrentPeersData.delay(getSyncMainDataInterval());
            },
            onSuccess: function(response) {
                $('error_div').set('html', '');
                if (response) {
                    const full_update = (response['full_update'] === true);
                    if (full_update)
                        torrentPeersTable.clear();
                    if (response['rid'])
                        syncTorrentPeersLastResponseId = response['rid'];
                    if (response['peers']) {
                        for (const key in response['peers']) {
                            response['peers'][key]['rowId'] = key;

                            torrentPeersTable.updateRowData(response['peers'][key]);
                        }
                    }
                    if (response['peers_removed']) {
                        response['peers_removed'].each(function(hash) {
                            torrentPeersTable.removeRow(hash);
                        });
                    }
                    torrentPeersTable.updateTable(full_update);
                    torrentPeersTable.altRow();

                    if (response['show_flags']) {
                        if (show_flags != response['show_flags']) {
                            show_flags = response['show_flags'];
                            torrentPeersTable.columns['country'].force_hide = !show_flags;
                            torrentPeersTable.updateColumn('country');
                        }
                    }
                }
                else {
                    torrentPeersTable.clear();
                }
            }
        }).send();
    };

    const updateData = function() {
        clearTimeout(loadTorrentPeersTimer);
        loadTorrentPeersData();
    };

    const torrentPeersContextMenu = new window.qBittorrent.ContextMenu.ContextMenu({
        targets: '#torrentPeersTableDiv',
        menu: 'torrentPeersMenu',
        actions: {
            addPeer: function(element, ref) {
                const hash = torrentsTable.getCurrentTorrentHash();
                if (!hash)
                    return;

                new MochaUI.Window({
                    id: 'addPeersPage',
                    title: "QBT_TR(Add Peers)QBT_TR[CONTEXT=PeersAdditionDialog]",
                    loadMethod: 'iframe',
                    contentURL: 'addpeers.html?hash=' + hash,
                    scrollbars: false,
                    resizable: false,
                    maximizable: false,
                    paddingVertical: 0,
                    paddingHorizontal: 0,
                    width: 350,
                    height: 240
                });
            },
            banPeer: function(element, ref) {
                const selectedPeers = torrentPeersTable.selectedRowsIds();
                if (selectedPeers.length === 0)
                    return;

                if (confirm('QBT_TR(Are you sure you want to permanently ban the selected peers?)QBT_TR[CONTEXT=PeerListWidget]')) {
                    new Request({
                        url: 'api/v2/transfer/banPeers',
                        noCache: true,
                        method: 'post',
                        data: {
                            hash: torrentsTable.getCurrentTorrentHash(),
                            peers: selectedPeers.join('|')
                        }
                    }).send();
                }
            }
        },
        offsets: {
            x: -15,
            y: 2
        },
        onShow: function() {
            const selectedPeers = torrentPeersTable.selectedRowsIds();

            if (selectedPeers.length >= 1) {
                this.showItem('copyPeer');
                this.showItem('banPeer');
            }
            else {
                this.hideItem('copyPeer');
                this.hideItem('banPeer');
            }
        }
    });

    new ClipboardJS('#CopyPeerInfo', {
        text: function(trigger) {
            return torrentPeersTable.selectedRowsIds().join("\n");
        }
    });

    torrentPeersTable.setup('torrentPeersTableDiv', 'torrentPeersTableFixedHeaderDiv', torrentPeersContextMenu);

    return exports();
})();
