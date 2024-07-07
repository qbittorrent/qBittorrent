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

if (window.qBittorrent === undefined)
    window.qBittorrent = {};

window.qBittorrent.PropGeneral = (function() {
    const exports = function() {
        return {
            updateData: updateData
        };
    };

    const piecesBar = new window.qBittorrent.PiecesBar.PiecesBar([], {
        height: 16
    });
    $("progress").appendChild(piecesBar);

    const clearData = function() {
        $("time_elapsed").set("html", "");
        $("eta").set("html", "");
        $("nb_connections").set("html", "");
        $("total_downloaded").set("html", "");
        $("total_uploaded").set("html", "");
        $("dl_speed").set("html", "");
        $("up_speed").set("html", "");
        $("dl_limit").set("html", "");
        $("up_limit").set("html", "");
        $("total_wasted").set("html", "");
        $("seeds").set("html", "");
        $("peers").set("html", "");
        $("share_ratio").set("html", "");
        $("popularity").set("html", "");
        $("reannounce").set("html", "");
        $("last_seen").set("html", "");
        $("total_size").set("html", "");
        $("pieces").set("html", "");
        $("created_by").set("html", "");
        $("addition_date").set("html", "");
        $("completion_date").set("html", "");
        $("creation_date").set("html", "");
        $("torrent_hash_v1").set("html", "");
        $("torrent_hash_v2").set("html", "");
        $("save_path").set("html", "");
        $("comment").set("html", "");
        $("private").set("html", "");
        piecesBar.clear();
    };

    let loadTorrentDataTimer;
    const loadTorrentData = function() {
        if ($("prop_general").hasClass("invisible")
            || $("propertiesPanel_collapseToggle").hasClass("panel-expand")) {
            // Tab changed, don't do anything
            return;
        }
        const current_id = torrentsTable.getCurrentTorrentID();
        if (current_id === "") {
            clearData();
            clearTimeout(loadTorrentDataTimer);
            loadTorrentDataTimer = loadTorrentData.delay(5000);
            return;
        }
        const url = new URI("api/v2/torrents/properties?hash=" + current_id);
        new Request.JSON({
            url: url,
            method: "get",
            noCache: true,
            onFailure: function() {
                $("error_div").set("html", "QBT_TR(qBittorrent client is not reachable)QBT_TR[CONTEXT=HttpServer]");
                clearTimeout(loadTorrentDataTimer);
                loadTorrentDataTimer = loadTorrentData.delay(10000);
            },
            onSuccess: function(data) {
                $("error_div").set("html", "");
                if (data) {
                    // Update Torrent data

                    const timeElapsed = (data.seeding_time > 0)
                        ? "QBT_TR(%1 (seeded for %2))QBT_TR[CONTEXT=PropertiesWidget]"
                        .replace("%1", window.qBittorrent.Misc.friendlyDuration(data.time_elapsed))
                        .replace("%2", window.qBittorrent.Misc.friendlyDuration(data.seeding_time))
                        : window.qBittorrent.Misc.friendlyDuration(data.time_elapsed);
                    $("time_elapsed").set("html", timeElapsed);

                    $("eta").set("html", window.qBittorrent.Misc.friendlyDuration(data.eta, window.qBittorrent.Misc.MAX_ETA));

                    const nbConnections = "QBT_TR(%1 (%2 max))QBT_TR[CONTEXT=PropertiesWidget]"
                        .replace("%1", data.nb_connections)
                        .replace("%2", ((data.nb_connections_limit < 0) ? "∞" : data.nb_connections_limit));
                    $("nb_connections").set("html", nbConnections);

                    const totalDownloaded = "QBT_TR(%1 (%2 this session))QBT_TR[CONTEXT=PropertiesWidget]"
                        .replace("%1", window.qBittorrent.Misc.friendlyUnit(data.total_downloaded))
                        .replace("%2", window.qBittorrent.Misc.friendlyUnit(data.total_downloaded_session));
                    $("total_downloaded").set("html", totalDownloaded);

                    const totalUploaded = "QBT_TR(%1 (%2 this session))QBT_TR[CONTEXT=PropertiesWidget]"
                        .replace("%1", window.qBittorrent.Misc.friendlyUnit(data.total_uploaded))
                        .replace("%2", window.qBittorrent.Misc.friendlyUnit(data.total_uploaded_session));
                    $("total_uploaded").set("html", totalUploaded);

                    const dlSpeed = "QBT_TR(%1 (%2 avg.))QBT_TR[CONTEXT=PropertiesWidget]"
                        .replace("%1", window.qBittorrent.Misc.friendlyUnit(data.dl_speed, true))
                        .replace("%2", window.qBittorrent.Misc.friendlyUnit(data.dl_speed_avg, true));
                    $("dl_speed").set("html", dlSpeed);

                    const upSpeed = "QBT_TR(%1 (%2 avg.))QBT_TR[CONTEXT=PropertiesWidget]"
                        .replace("%1", window.qBittorrent.Misc.friendlyUnit(data.up_speed, true))
                        .replace("%2", window.qBittorrent.Misc.friendlyUnit(data.up_speed_avg, true));
                    $("up_speed").set("html", upSpeed);

                    const dlLimit = (data.dl_limit === -1)
                        ? "∞"
                        : window.qBittorrent.Misc.friendlyUnit(data.dl_limit, true);
                    $("dl_limit").set("html", dlLimit);

                    const upLimit = (data.up_limit === -1)
                        ? "∞"
                        : window.qBittorrent.Misc.friendlyUnit(data.up_limit, true);
                    $("up_limit").set("html", upLimit);

                    $("total_wasted").set("html", window.qBittorrent.Misc.friendlyUnit(data.total_wasted));

                    const seeds = "QBT_TR(%1 (%2 total))QBT_TR[CONTEXT=PropertiesWidget]"
                        .replace("%1", data.seeds)
                        .replace("%2", data.seeds_total);
                    $("seeds").set("html", seeds);

                    const peers = "QBT_TR(%1 (%2 total))QBT_TR[CONTEXT=PropertiesWidget]"
                        .replace("%1", data.peers)
                        .replace("%2", data.peers_total);
                    $("peers").set("html", peers);

                    $("share_ratio").set("html", data.share_ratio.toFixed(2));

                    $("popularity").set("html", data.popularity.toFixed(2));

                    $("reannounce").set("html", window.qBittorrent.Misc.friendlyDuration(data.reannounce));

                    const lastSeen = (data.last_seen >= 0)
                        ? new Date(data.last_seen * 1000).toLocaleString()
                        : "QBT_TR(Never)QBT_TR[CONTEXT=PropertiesWidget]";
                    $("last_seen").set("html", lastSeen);

                    const totalSize = (data.total_size >= 0) ? window.qBittorrent.Misc.friendlyUnit(data.total_size) : "";
                    $("total_size").set("html", totalSize);

                    const pieces = (data.pieces_num >= 0)
                        ? "QBT_TR(%1 x %2 (have %3))QBT_TR[CONTEXT=PropertiesWidget]"
                        .replace("%1", data.pieces_num)
                        .replace("%2", window.qBittorrent.Misc.friendlyUnit(data.piece_size))
                        .replace("%3", data.pieces_have)
                        : "";
                    $("pieces").set("html", pieces);

                    $("created_by").set("text", data.created_by);

                    const additionDate = (data.addition_date >= 0)
                        ? new Date(data.addition_date * 1000).toLocaleString()
                        : "QBT_TR(Unknown)QBT_TR[CONTEXT=HttpServer]";
                    $("addition_date").set("html", additionDate);

                    const completionDate = (data.completion_date >= 0)
                        ? new Date(data.completion_date * 1000).toLocaleString()
                        : "";
                    $("completion_date").set("html", completionDate);

                    const creationDate = (data.creation_date >= 0)
                        ? new Date(data.creation_date * 1000).toLocaleString()
                        : "";
                    $("creation_date").set("html", creationDate);

                    const torrentHashV1 = (data.infohash_v1 !== "")
                        ? data.infohash_v1
                        : "QBT_TR(N/A)QBT_TR[CONTEXT=PropertiesWidget]";
                    $("torrent_hash_v1").set("html", torrentHashV1);

                    const torrentHashV2 = (data.infohash_v2 !== "")
                        ? data.infohash_v2
                        : "QBT_TR(N/A)QBT_TR[CONTEXT=PropertiesWidget]";
                    $("torrent_hash_v2").set("html", torrentHashV2);

                    $("save_path").set("html", data.save_path);

                    $("comment").set("html", window.qBittorrent.Misc.parseHtmlLinks(window.qBittorrent.Misc.escapeHtml(data.comment)));

                    if (data.has_metadata) {
                        $("private").set("text", (data.private
                            ? "QBT_TR(Yes)QBT_TR[CONTEXT=PropertiesWidget]"
                            : "QBT_TR(No)QBT_TR[CONTEXT=PropertiesWidget]"));
                    }
                    else {
                        $("private").set("text", "QBT_TR(N/A)QBT_TR[CONTEXT=PropertiesWidget]");
                    }
                }
                else {
                    clearData();
                }
                clearTimeout(loadTorrentDataTimer);
                loadTorrentDataTimer = loadTorrentData.delay(5000);
            }
        }).send();

        const piecesUrl = new URI("api/v2/torrents/pieceStates?hash=" + current_id);
        new Request.JSON({
            url: piecesUrl,
            method: "get",
            noCache: true,
            onFailure: function() {
                $("error_div").set("html", "QBT_TR(qBittorrent client is not reachable)QBT_TR[CONTEXT=HttpServer]");
                clearTimeout(loadTorrentDataTimer);
                loadTorrentDataTimer = loadTorrentData.delay(10000);
            },
            onSuccess: function(data) {
                $("error_div").set("html", "");

                if (data)
                    piecesBar.setPieces(data);
                else
                    clearData();

                clearTimeout(loadTorrentDataTimer);
                loadTorrentDataTimer = loadTorrentData.delay(5000);
            }
        }).send();
    };

    const updateData = function() {
        clearTimeout(loadTorrentDataTimer);
        loadTorrentData();
    };

    return exports();
})();

Object.freeze(window.qBittorrent.PropGeneral);
