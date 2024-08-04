/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2008  Christophe Dumez <chris@qbittorrent.org>
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

/* -----------------------------------------------------------------

    ATTACH MOCHA LINK EVENTS
    Notes: Here is where you define your windows and the events that open them.
    If you are not using links to run Mocha methods you can remove this function.

    If you need to add link events to links within windows you are creating, do
    it in the onContentLoaded function of the new window.

   ----------------------------------------------------------------- */
"use strict";

const LocalPreferences = new window.qBittorrent.LocalPreferences.LocalPreferencesClass();

let saveWindowSize = function() {};
let loadWindowWidth = function() {};
let loadWindowHeight = function() {};
let showDownloadPage = function() {};
let globalUploadLimitFN = function() {};
let uploadLimitFN = function() {};
let shareRatioFN = function() {};
let toggleSequentialDownloadFN = function() {};
let toggleFirstLastPiecePrioFN = function() {};
let setSuperSeedingFN = function() {};
let setForceStartFN = function() {};
let globalDownloadLimitFN = function() {};
let StatisticsLinkFN = function() {};
let downloadLimitFN = function() {};
let deleteFN = function() {};
let stopFN = function() {};
let startFN = function() {};
let autoTorrentManagementFN = function() {};
let recheckFN = function() {};
let reannounceFN = function() {};
let setLocationFN = function() {};
let renameFN = function() {};
let renameFilesFN = function() {};
let torrentNewCategoryFN = function() {};
let torrentSetCategoryFN = function() {};
let createCategoryFN = function() {};
let createSubcategoryFN = function() {};
let editCategoryFN = function() {};
let removeCategoryFN = function() {};
let deleteUnusedCategoriesFN = function() {};
let startTorrentsByCategoryFN = function() {};
let stopTorrentsByCategoryFN = function() {};
let deleteTorrentsByCategoryFN = function() {};
let torrentAddTagsFN = function() {};
let torrentSetTagsFN = function() {};
let torrentRemoveAllTagsFN = function() {};
let createTagFN = function() {};
let removeTagFN = function() {};
let deleteUnusedTagsFN = function() {};
let startTorrentsByTagFN = function() {};
let stopTorrentsByTagFN = function() {};
let deleteTorrentsByTagFN = function() {};
let startTorrentsByTrackerFN = function() {};
let stopTorrentsByTrackerFN = function() {};
let deleteTorrentsByTrackerFN = function() {};
let copyNameFN = function() {};
let copyInfohashFN = function(policy) {};
let copyMagnetLinkFN = function() {};
let copyIdFN = function() {};
let copyCommentFN = function() {};
let setQueuePositionFN = function() {};
let exportTorrentFN = function() {};

const initializeWindows = function() {
    saveWindowSize = function(windowId) {
        const size = $(windowId).getSize();
        LocalPreferences.set("window_" + windowId + "_width", size.x);
        LocalPreferences.set("window_" + windowId + "_height", size.y);
    };

    loadWindowWidth = function(windowId, defaultValue) {
        return LocalPreferences.get("window_" + windowId + "_width", defaultValue);
    };

    loadWindowHeight = function(windowId, defaultValue) {
        return LocalPreferences.get("window_" + windowId + "_height", defaultValue);
    };

    function addClickEvent(el, fn) {
        ["Link", "Button"].each((item) => {
            if ($(el + item))
                $(el + item).addEvent("click", fn);
        });
    }

    addClickEvent("download", (e) => {
        new Event(e).stop();
        showDownloadPage();
    });

    showDownloadPage = function(urls) {
        const id = "downloadPage";
        const contentUri = new URI("download.html");

        if (urls && (urls.length > 0))
            contentUri.setData("urls", urls.map(encodeURIComponent).join("|"));

        new MochaUI.Window({
            id: id,
            title: "QBT_TR(Download from URLs)QBT_TR[CONTEXT=downloadFromURL]",
            loadMethod: "iframe",
            contentURL: contentUri.toString(),
            addClass: "windowFrame", // fixes iframe scrolling on iOS Safari
            scrollbars: true,
            maximizable: false,
            closable: true,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: loadWindowWidth(id, 500),
            height: loadWindowHeight(id, 600),
            onResize: function() {
                saveWindowSize(id);
            }
        });
        updateMainData();
    };

    addClickEvent("preferences", (e) => {
        new Event(e).stop();
        const id = "preferencesPage";
        new MochaUI.Window({
            id: id,
            title: "QBT_TR(Options)QBT_TR[CONTEXT=OptionsDialog]",
            loadMethod: "xhr",
            toolbar: true,
            contentURL: new URI("views/preferences.html").toString(),
            require: {
                css: ["css/Tabs.css"]
            },
            toolbarURL: "views/preferencesToolbar.html",
            maximizable: false,
            closable: true,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: loadWindowWidth(id, 700),
            height: loadWindowHeight(id, 600),
            onResize: function() {
                saveWindowSize(id);
            }
        });
    });

    addClickEvent("upload", (e) => {
        new Event(e).stop();
        const id = "uploadPage";
        new MochaUI.Window({
            id: id,
            title: "QBT_TR(Upload local torrent)QBT_TR[CONTEXT=HttpServer]",
            loadMethod: "iframe",
            contentURL: new URI("upload.html").toString(),
            addClass: "windowFrame", // fixes iframe scrolling on iOS Safari
            scrollbars: true,
            maximizable: false,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: loadWindowWidth(id, 500),
            height: loadWindowHeight(id, 460),
            onResize: function() {
                saveWindowSize(id);
            }
        });
        updateMainData();
    });

    globalUploadLimitFN = function() {
        new MochaUI.Window({
            id: "uploadLimitPage",
            title: "QBT_TR(Global Upload Speed Limit)QBT_TR[CONTEXT=MainWindow]",
            loadMethod: "iframe",
            contentURL: new URI("uploadlimit.html").setData("hashes", "global").toString(),
            scrollbars: false,
            resizable: false,
            maximizable: false,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: 424,
            height: 80
        });
    };

    uploadLimitFN = function() {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            new MochaUI.Window({
                id: "uploadLimitPage",
                title: "QBT_TR(Torrent Upload Speed Limiting)QBT_TR[CONTEXT=TransferListWidget]",
                loadMethod: "iframe",
                contentURL: new URI("uploadlimit.html").setData("hashes", hashes.join("|")).toString(),
                scrollbars: false,
                resizable: false,
                maximizable: false,
                paddingVertical: 0,
                paddingHorizontal: 0,
                width: 424,
                height: 80
            });
        }
    };

    shareRatioFN = function() {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            let shareRatio = null;
            let torrentsHaveSameShareRatio = true;

            // check if all selected torrents have same share ratio
            for (let i = 0; i < hashes.length; ++i) {
                const hash = hashes[i];
                const row = torrentsTable.rows[hash].full_data;
                const origValues = row.ratio_limit + "|" + row.seeding_time_limit + "|" + row.inactive_seeding_time_limit + "|"
                    + row.max_ratio + "|" + row.max_seeding_time + "|" + row.max_inactive_seeding_time;

                // initialize value
                if (shareRatio === null)
                    shareRatio = origValues;

                if (origValues !== shareRatio) {
                    torrentsHaveSameShareRatio = false;
                    break;
                }
            }

            // if all torrents have same share ratio, display that share ratio. else use the default
            const orig = torrentsHaveSameShareRatio ? shareRatio : "";
            new MochaUI.Window({
                id: "shareRatioPage",
                title: "QBT_TR(Torrent Upload/Download Ratio Limiting)QBT_TR[CONTEXT=UpDownRatioDialog]",
                loadMethod: "iframe",
                contentURL: new URI("shareratio.html").setData("hashes", hashes.join("|")).setData("orig", orig).toString(),
                scrollbars: false,
                maximizable: false,
                paddingVertical: 0,
                paddingHorizontal: 0,
                width: 424,
                height: 200
            });
        }
    };

    toggleSequentialDownloadFN = function() {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            new Request({
                url: "api/v2/torrents/toggleSequentialDownload",
                method: "post",
                data: {
                    hashes: hashes.join("|")
                }
            }).send();
            updateMainData();
        }
    };

    toggleFirstLastPiecePrioFN = function() {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            new Request({
                url: "api/v2/torrents/toggleFirstLastPiecePrio",
                method: "post",
                data: {
                    hashes: hashes.join("|")
                }
            }).send();
            updateMainData();
        }
    };

    setSuperSeedingFN = function(val) {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            new Request({
                url: "api/v2/torrents/setSuperSeeding",
                method: "post",
                data: {
                    value: val,
                    hashes: hashes.join("|")
                }
            }).send();
            updateMainData();
        }
    };

    setForceStartFN = function() {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            new Request({
                url: "api/v2/torrents/setForceStart",
                method: "post",
                data: {
                    value: "true",
                    hashes: hashes.join("|")
                }
            }).send();
            updateMainData();
        }
    };

    globalDownloadLimitFN = function() {
        new MochaUI.Window({
            id: "downloadLimitPage",
            title: "QBT_TR(Global Download Speed Limit)QBT_TR[CONTEXT=MainWindow]",
            loadMethod: "iframe",
            contentURL: new URI("downloadlimit.html").setData("hashes", "global").toString(),
            scrollbars: false,
            resizable: false,
            maximizable: false,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: 424,
            height: 80
        });
    };

    StatisticsLinkFN = function() {
        const id = "statisticspage";
        new MochaUI.Window({
            id: id,
            title: "QBT_TR(Statistics)QBT_TR[CONTEXT=StatsDialog]",
            loadMethod: "xhr",
            contentURL: new URI("views/statistics.html").toString(),
            maximizable: false,
            padding: 10,
            width: loadWindowWidth(id, 275),
            height: loadWindowHeight(id, 370),
            onResize: function() {
                saveWindowSize(id);
            }
        });
    };

    downloadLimitFN = function() {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            new MochaUI.Window({
                id: "downloadLimitPage",
                title: "QBT_TR(Torrent Download Speed Limiting)QBT_TR[CONTEXT=TransferListWidget]",
                loadMethod: "iframe",
                contentURL: new URI("downloadlimit.html").setData("hashes", hashes.join("|")).toString(),
                scrollbars: false,
                resizable: false,
                maximizable: false,
                paddingVertical: 0,
                paddingHorizontal: 0,
                width: 424,
                height: 80
            });
        }
    };

    deleteFN = function(deleteFiles = false) {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            new MochaUI.Window({
                id: "confirmDeletionPage",
                title: "QBT_TR(Remove torrent(s))QBT_TR[CONTEXT=confirmDeletionDlg]",
                loadMethod: "iframe",
                contentURL: new URI("confirmdeletion.html").setData("hashes", hashes.join("|")).setData("deleteFiles", deleteFiles).toString(),
                scrollbars: false,
                resizable: true,
                maximizable: false,
                padding: 10,
                width: 424,
                height: 160
            });
            updateMainData();
        }
    };

    addClickEvent("delete", (e) => {
        new Event(e).stop();
        deleteFN();
    });

    stopFN = function() {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            new Request({
                url: "api/v2/torrents/stop",
                method: "post",
                data: {
                    hashes: hashes.join("|")
                }
            }).send();
            updateMainData();
        }
    };

    startFN = function() {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            new Request({
                url: "api/v2/torrents/start",
                method: "post",
                data: {
                    hashes: hashes.join("|")
                }
            }).send();
            updateMainData();
        }
    };

    autoTorrentManagementFN = function() {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            let enable = false;
            hashes.each((hash, index) => {
                const row = torrentsTable.rows[hash];
                if (!row.full_data.auto_tmm)
                    enable = true;
            });
            new Request({
                url: "api/v2/torrents/setAutoManagement",
                method: "post",
                data: {
                    hashes: hashes.join("|"),
                    enable: enable
                }
            }).send();
            updateMainData();
        }
    };

    recheckFN = function() {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            new Request({
                url: "api/v2/torrents/recheck",
                method: "post",
                data: {
                    hashes: hashes.join("|"),
                }
            }).send();
            updateMainData();
        }
    };

    reannounceFN = function() {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            new Request({
                url: "api/v2/torrents/reannounce",
                method: "post",
                data: {
                    hashes: hashes.join("|"),
                }
            }).send();
            updateMainData();
        }
    };

    setLocationFN = function() {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            const hash = hashes[0];
            const row = torrentsTable.rows[hash];

            new MochaUI.Window({
                id: "setLocationPage",
                title: "QBT_TR(Set location)QBT_TR[CONTEXT=TransferListWidget]",
                loadMethod: "iframe",
                contentURL: new URI("setlocation.html").setData("hashes", hashes.join("|")).setData("path", encodeURIComponent(row.full_data.save_path)).toString(),
                scrollbars: false,
                resizable: true,
                maximizable: false,
                paddingVertical: 0,
                paddingHorizontal: 0,
                width: 400,
                height: 130
            });
        }
    };

    renameFN = function() {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length === 1) {
            const hash = hashes[0];
            const row = torrentsTable.rows[hash];
            if (row) {
                new MochaUI.Window({
                    id: "renamePage",
                    title: "QBT_TR(Rename)QBT_TR[CONTEXT=TransferListWidget]",
                    loadMethod: "iframe",
                    contentURL: new URI("rename.html").setData("hash", hash).setData("name", row.full_data.name).toString(),
                    scrollbars: false,
                    resizable: true,
                    maximizable: false,
                    paddingVertical: 0,
                    paddingHorizontal: 0,
                    width: 400,
                    height: 100
                });
            }
        }
    };

    renameFilesFN = function() {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length === 1) {
            const hash = hashes[0];
            const row = torrentsTable.rows[hash];
            if (row) {
                new MochaUI.Window({
                    id: "multiRenamePage",
                    title: "QBT_TR(Renaming)QBT_TR[CONTEXT=TransferListWidget]",
                    data: { hash: hash, selectedRows: [] },
                    loadMethod: "xhr",
                    contentURL: "rename_files.html",
                    scrollbars: false,
                    resizable: true,
                    maximizable: false,
                    paddingVertical: 0,
                    paddingHorizontal: 0,
                    width: 800,
                    height: 420,
                    resizeLimit: { "x": [800], "y": [420] }
                });
            }
        }
    };

    torrentNewCategoryFN = function() {
        const action = "set";
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            new MochaUI.Window({
                id: "newCategoryPage",
                title: "QBT_TR(New Category)QBT_TR[CONTEXT=TransferListWidget]",
                loadMethod: "iframe",
                contentURL: new URI("newcategory.html").setData("action", action).setData("hashes", hashes.join("|")).toString(),
                scrollbars: false,
                resizable: true,
                maximizable: false,
                paddingVertical: 0,
                paddingHorizontal: 0,
                width: 400,
                height: 150
            });
        }
    };

    torrentSetCategoryFN = function(categoryHash) {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length <= 0)
            return;

        const categoryName = category_list.has(categoryHash)
            ? category_list.get(categoryHash).name
            : "";
        new Request({
            url: "api/v2/torrents/setCategory",
            method: "post",
            data: {
                hashes: hashes.join("|"),
                category: categoryName
            }
        }).send();
    };

    createCategoryFN = function() {
        const action = "create";
        new MochaUI.Window({
            id: "newCategoryPage",
            title: "QBT_TR(New Category)QBT_TR[CONTEXT=CategoryFilterWidget]",
            loadMethod: "iframe",
            contentURL: new URI("newcategory.html").setData("action", action).toString(),
            scrollbars: false,
            resizable: true,
            maximizable: false,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: 400,
            height: 150
        });
        updateMainData();
    };

    createSubcategoryFN = function(categoryHash) {
        const action = "createSubcategory";
        const categoryName = category_list.get(categoryHash).name + "/";
        new MochaUI.Window({
            id: "newSubcategoryPage",
            title: "QBT_TR(New Category)QBT_TR[CONTEXT=CategoryFilterWidget]",
            loadMethod: "iframe",
            contentURL: new URI("newcategory.html").setData("action", action).setData("categoryName", categoryName).toString(),
            scrollbars: false,
            resizable: true,
            maximizable: false,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: 400,
            height: 150
        });
        updateMainData();
    };

    editCategoryFN = function(categoryHash) {
        const action = "edit";
        const category = category_list.get(categoryHash);
        new MochaUI.Window({
            id: "editCategoryPage",
            title: "QBT_TR(Edit Category)QBT_TR[CONTEXT=TransferListWidget]",
            loadMethod: "iframe",
            contentURL: new URI("newcategory.html").setData("action", action).setData("categoryName", category.name).setData("savePath", category.savePath).toString(),
            scrollbars: false,
            resizable: true,
            maximizable: false,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: 400,
            height: 150
        });
        updateMainData();
    };

    removeCategoryFN = function(categoryHash) {
        const categoryName = category_list.get(categoryHash).name;
        new Request({
            url: "api/v2/torrents/removeCategories",
            method: "post",
            data: {
                categories: categoryName
            }
        }).send();
        setCategoryFilter(CATEGORIES_ALL);
    };

    deleteUnusedCategoriesFN = function() {
        const categories = [];
        category_list.forEach((category, hash) => {
            if (torrentsTable.getFilteredTorrentsNumber("all", hash, TAGS_ALL, TRACKERS_ALL) === 0)
                categories.push(category.name);
        });

        new Request({
            url: "api/v2/torrents/removeCategories",
            method: "post",
            data: {
                categories: categories.join("\n")
            }
        }).send();
        setCategoryFilter(CATEGORIES_ALL);
    };

    startTorrentsByCategoryFN = function(categoryHash) {
        const hashes = torrentsTable.getFilteredTorrentsHashes("all", categoryHash, TAGS_ALL, TRACKERS_ALL);
        if (hashes.length) {
            new Request({
                url: "api/v2/torrents/start",
                method: "post",
                data: {
                    hashes: hashes.join("|")
                }
            }).send();
            updateMainData();
        }
    };

    stopTorrentsByCategoryFN = function(categoryHash) {
        const hashes = torrentsTable.getFilteredTorrentsHashes("all", categoryHash, TAGS_ALL, TRACKERS_ALL);
        if (hashes.length) {
            new Request({
                url: "api/v2/torrents/stop",
                method: "post",
                data: {
                    hashes: hashes.join("|")
                }
            }).send();
            updateMainData();
        }
    };

    deleteTorrentsByCategoryFN = function(categoryHash) {
        const hashes = torrentsTable.getFilteredTorrentsHashes("all", categoryHash, TAGS_ALL, TRACKERS_ALL);
        if (hashes.length) {
            new MochaUI.Window({
                id: "confirmDeletionPage",
                title: "QBT_TR(Remove torrent(s))QBT_TR[CONTEXT=confirmDeletionDlg]",
                loadMethod: "iframe",
                contentURL: new URI("confirmdeletion.html").setData("hashes", hashes.join("|")).toString(),
                scrollbars: false,
                resizable: true,
                maximizable: false,
                padding: 10,
                width: 424,
                height: 160
            });
            updateMainData();
        }
    };

    torrentAddTagsFN = function() {
        const action = "set";
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            new MochaUI.Window({
                id: "newTagPage",
                title: "QBT_TR(Add tags)QBT_TR[CONTEXT=TransferListWidget]",
                loadMethod: "iframe",
                contentURL: new URI("newtag.html").setData("action", action).setData("hashes", hashes.join("|")).toString(),
                scrollbars: false,
                resizable: true,
                maximizable: false,
                paddingVertical: 0,
                paddingHorizontal: 0,
                width: 250,
                height: 100
            });
        }
    };

    torrentSetTagsFN = function(tagHash, isSet) {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length <= 0)
            return;

        const tagName = tagList.has(tagHash) ? tagList.get(tagHash).name : "";
        new Request({
            url: (isSet ? "api/v2/torrents/addTags" : "api/v2/torrents/removeTags"),
            method: "post",
            data: {
                hashes: hashes.join("|"),
                tags: tagName,
            }
        }).send();
    };

    torrentRemoveAllTagsFN = function() {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            new Request({
                url: ("api/v2/torrents/removeTags"),
                method: "post",
                data: {
                    hashes: hashes.join("|"),
                }
            }).send();
        }
    };

    createTagFN = function() {
        const action = "create";
        new MochaUI.Window({
            id: "newTagPage",
            title: "QBT_TR(New Tag)QBT_TR[CONTEXT=TagFilterWidget]",
            loadMethod: "iframe",
            contentURL: new URI("newtag.html").setData("action", action).toString(),
            scrollbars: false,
            resizable: true,
            maximizable: false,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: 250,
            height: 100
        });
        updateMainData();
    };

    removeTagFN = function(tagHash) {
        const tagName = tagList.get(tagHash).name;
        new Request({
            url: "api/v2/torrents/deleteTags",
            method: "post",
            data: {
                tags: tagName
            }
        }).send();
        setTagFilter(TAGS_ALL);
    };

    deleteUnusedTagsFN = function() {
        const tags = [];
        tagList.forEach((tag, hash) => {
            if (torrentsTable.getFilteredTorrentsNumber("all", CATEGORIES_ALL, hash, TRACKERS_ALL) === 0)
                tags.push(tag.name);
        });
        new Request({
            url: "api/v2/torrents/deleteTags",
            method: "post",
            data: {
                tags: tags.join(",")
            }
        }).send();
        setTagFilter(TAGS_ALL);
    };

    startTorrentsByTagFN = function(tagHash) {
        const hashes = torrentsTable.getFilteredTorrentsHashes("all", CATEGORIES_ALL, tagHash, TRACKERS_ALL);
        if (hashes.length) {
            new Request({
                url: "api/v2/torrents/start",
                method: "post",
                data: {
                    hashes: hashes.join("|")
                }
            }).send();
            updateMainData();
        }
    };

    stopTorrentsByTagFN = function(tagHash) {
        const hashes = torrentsTable.getFilteredTorrentsHashes("all", CATEGORIES_ALL, tagHash, TRACKERS_ALL);
        if (hashes.length) {
            new Request({
                url: "api/v2/torrents/stop",
                method: "post",
                data: {
                    hashes: hashes.join("|")
                }
            }).send();
            updateMainData();
        }
    };

    deleteTorrentsByTagFN = function(tagHash) {
        const hashes = torrentsTable.getFilteredTorrentsHashes("all", CATEGORIES_ALL, tagHash, TRACKERS_ALL);
        if (hashes.length) {
            new MochaUI.Window({
                id: "confirmDeletionPage",
                title: "QBT_TR(Remove torrent(s))QBT_TR[CONTEXT=confirmDeletionDlg]",
                loadMethod: "iframe",
                contentURL: new URI("confirmdeletion.html").setData("hashes", hashes.join("|")).toString(),
                scrollbars: false,
                resizable: true,
                maximizable: false,
                padding: 10,
                width: 424,
                height: 160
            });
            updateMainData();
        }
    };

    startTorrentsByTrackerFN = function(trackerHash) {
        const trackerHashInt = Number.parseInt(trackerHash, 10);
        let hashes = [];
        switch (trackerHashInt) {
            case TRACKERS_ALL:
                hashes = torrentsTable.getFilteredTorrentsHashes("all", CATEGORIES_ALL, TAGS_ALL, TRACKERS_ALL);
                break;
            case TRACKERS_TRACKERLESS:
                hashes = torrentsTable.getFilteredTorrentsHashes("all", CATEGORIES_ALL, TAGS_ALL, TRACKERS_TRACKERLESS);
                break;
            default: {
                const uniqueTorrents = new Set();
                for (const torrents of trackerList.get(trackerHashInt).trackerTorrentMap.values()) {
                    for (const torrent of torrents)
                        uniqueTorrents.add(torrent);
                }
                hashes = [...uniqueTorrents];
                break;
            }
        }

        if (hashes.length > 0) {
            new Request({
                url: "api/v2/torrents/start",
                method: "post",
                data: {
                    hashes: hashes.join("|")
                }
            }).send();
            updateMainData();
        }
    };

    stopTorrentsByTrackerFN = function(trackerHash) {
        const trackerHashInt = Number.parseInt(trackerHash, 10);
        let hashes = [];
        switch (trackerHashInt) {
            case TRACKERS_ALL:
                hashes = torrentsTable.getFilteredTorrentsHashes("all", CATEGORIES_ALL, TAGS_ALL, TRACKERS_ALL);
                break;
            case TRACKERS_TRACKERLESS:
                hashes = torrentsTable.getFilteredTorrentsHashes("all", CATEGORIES_ALL, TAGS_ALL, TRACKERS_TRACKERLESS);
                break;
            default: {
                const uniqueTorrents = new Set();
                for (const torrents of trackerList.get(trackerHashInt).trackerTorrentMap.values()) {
                    for (const torrent of torrents)
                        uniqueTorrents.add(torrent);
                }
                hashes = [...uniqueTorrents];
                break;
            }
        }

        if (hashes.length) {
            new Request({
                url: "api/v2/torrents/stop",
                method: "post",
                data: {
                    hashes: hashes.join("|")
                }
            }).send();
            updateMainData();
        }
    };

    deleteTorrentsByTrackerFN = function(trackerHash) {
        const trackerHashInt = Number.parseInt(trackerHash, 10);
        let hashes = [];
        switch (trackerHashInt) {
            case TRACKERS_ALL:
                hashes = torrentsTable.getFilteredTorrentsHashes("all", CATEGORIES_ALL, TAGS_ALL, TRACKERS_ALL);
                break;
            case TRACKERS_TRACKERLESS:
                hashes = torrentsTable.getFilteredTorrentsHashes("all", CATEGORIES_ALL, TAGS_ALL, TRACKERS_TRACKERLESS);
                break;
            default: {
                const uniqueTorrents = new Set();
                for (const torrents of trackerList.get(trackerHashInt).trackerTorrentMap.values()) {
                    for (const torrent of torrents)
                        uniqueTorrents.add(torrent);
                }
                hashes = [...uniqueTorrents];
                break;
            }
        }

        if (hashes.length) {
            new MochaUI.Window({
                id: "confirmDeletionPage",
                title: "QBT_TR(Remove torrent(s))QBT_TR[CONTEXT=confirmDeletionDlg]",
                loadMethod: "iframe",
                contentURL: new URI("confirmdeletion.html").setData("hashes", hashes.join("|")).toString(),
                scrollbars: false,
                resizable: true,
                maximizable: false,
                padding: 10,
                width: 424,
                height: 160,
                onCloseComplete: function() {
                    updateMainData();
                    setTrackerFilter(TRACKERS_ALL);
                }
            });
        }
    };

    copyNameFN = function() {
        const selectedRows = torrentsTable.selectedRowsIds();
        const names = [];
        if (selectedRows.length > 0) {
            const rows = torrentsTable.getFilteredAndSortedRows();
            for (let i = 0; i < selectedRows.length; ++i) {
                const hash = selectedRows[i];
                names.push(rows[hash].full_data.name);
            }
        }
        return names.join("\n");
    };

    copyInfohashFN = function(policy) {
        const selectedRows = torrentsTable.selectedRowsIds();
        const infohashes = [];
        if (selectedRows.length > 0) {
            const rows = torrentsTable.getFilteredAndSortedRows();
            switch (policy) {
                case 1:
                    for (const id of selectedRows) {
                        const infohash = rows[id].full_data.infohash_v1;
                        if (infohash !== "")
                            infohashes.push(infohash);
                    }
                    break;
                case 2:
                    for (const id of selectedRows) {
                        const infohash = rows[id].full_data.infohash_v2;
                        if (infohash !== "")
                            infohashes.push(infohash);
                    }
                    break;
            }
        }
        return infohashes.join("\n");
    };

    copyMagnetLinkFN = function() {
        const selectedRows = torrentsTable.selectedRowsIds();
        const magnets = [];
        if (selectedRows.length > 0) {
            const rows = torrentsTable.getFilteredAndSortedRows();
            for (let i = 0; i < selectedRows.length; ++i) {
                const hash = selectedRows[i];
                magnets.push(rows[hash].full_data.magnet_uri);
            }
        }
        return magnets.join("\n");
    };

    copyIdFN = function() {
        return torrentsTable.selectedRowsIds().join("\n");
    };

    copyCommentFN = function() {
        const selectedRows = torrentsTable.selectedRowsIds();
        const comments = [];
        if (selectedRows.length > 0) {
            const rows = torrentsTable.getFilteredAndSortedRows();
            for (let i = 0; i < selectedRows.length; ++i) {
                const hash = selectedRows[i];
                const comment = rows[hash].full_data.comment;
                if (comment && (comment !== ""))
                    comments.push(comment);
            }
        }
        return comments.join("\n---------\n");
    };

    exportTorrentFN = async function() {
        const hashes = torrentsTable.selectedRowsIds();
        for (const hash of hashes) {
            const row = torrentsTable.rows.get(hash);
            if (!row)
                continue;

            const name = row.full_data.name;
            const url = new URI("api/v2/torrents/export");
            url.setData("hash", hash);

            // download response to file
            const element = document.createElement("a");
            element.href = url;
            element.download = (name + ".torrent");
            document.body.appendChild(element);
            element.click();
            document.body.removeChild(element);

            // https://stackoverflow.com/questions/53560991/automatic-file-downloads-limited-to-10-files-on-chrome-browser
            await window.qBittorrent.Misc.sleep(200);
        }
    };

    addClickEvent("stopAll", (e) => {
        new Event(e).stop();

        if (confirm("QBT_TR(Would you like to stop all torrents?)QBT_TR[CONTEXT=MainWindow]")) {
            new Request({
                url: "api/v2/torrents/stop",
                method: "post",
                data: {
                    hashes: "all"
                }
            }).send();
            updateMainData();
        }
    });

    addClickEvent("startAll", (e) => {
        new Event(e).stop();

        if (confirm("QBT_TR(Would you like to start all torrents?)QBT_TR[CONTEXT=MainWindow]")) {
            new Request({
                url: "api/v2/torrents/start",
                method: "post",
                data: {
                    hashes: "all"
                }
            }).send();
            updateMainData();
        }
    });

    ["stop", "start", "recheck"].each((item) => {
        addClickEvent(item, (e) => {
            new Event(e).stop();
            const hashes = torrentsTable.selectedRowsIds();
            if (hashes.length) {
                hashes.each((hash, index) => {
                    new Request({
                        url: "api/v2/torrents/" + item,
                        method: "post",
                        data: {
                            hashes: hash
                        }
                    }).send();
                });
                updateMainData();
            }
        });
    });

    ["decreasePrio", "increasePrio", "topPrio", "bottomPrio"].each((item) => {
        addClickEvent(item, (e) => {
            new Event(e).stop();
            setQueuePositionFN(item);
        });
    });

    setQueuePositionFN = function(cmd) {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            new Request({
                url: "api/v2/torrents/" + cmd,
                method: "post",
                data: {
                    hashes: hashes.join("|")
                }
            }).send();
            updateMainData();
        }
    };

    addClickEvent("about", (e) => {
        new Event(e).stop();
        const id = "aboutpage";
        new MochaUI.Window({
            id: id,
            title: "QBT_TR(About qBittorrent)QBT_TR[CONTEXT=AboutDialog]",
            loadMethod: "xhr",
            contentURL: new URI("views/about.html").toString(),
            require: {
                css: ["css/Tabs.css"]
            },
            toolbar: true,
            toolbarURL: "views/aboutToolbar.html",
            padding: 10,
            width: loadWindowWidth(id, 550),
            height: loadWindowHeight(id, 360),
            onResize: function() {
                saveWindowSize(id);
            }
        });
    });

    addClickEvent("logout", (e) => {
        new Event(e).stop();
        new Request({
            url: "api/v2/auth/logout",
            method: "post",
            onSuccess: function() {
                window.location.reload(true);
            }
        }).send();
    });

    addClickEvent("shutdown", (e) => {
        new Event(e).stop();
        if (confirm("QBT_TR(Are you sure you want to quit qBittorrent?)QBT_TR[CONTEXT=MainWindow]")) {
            new Request({
                url: "api/v2/app/shutdown",
                method: "post",
                onSuccess: function() {
                    const shutdownMessage = "QBT_TR(%1 has been shutdown)QBT_TR[CONTEXT=HttpServer]".replace("%1", window.qBittorrent.Client.mainTitle());
                    document.write(`<!doctype html><html lang="${LANG}"><head> <meta charset="UTF-8"> <meta name="color-scheme" content="light dark"> <title>${shutdownMessage}</title> <style>* {font-family: Arial, Helvetica, sans-serif;}</style></head><body> <h1 style="text-align: center;">${shutdownMessage}</h1></body></html>`);
                    document.close();
                    window.stop();
                    window.qBittorrent.Client.stop();
                }
            }).send();
        }
    });

    // Deactivate menu header links
    $$("a.returnFalse").each((el) => {
        el.addEvent("click", (e) => {
            new Event(e).stop();
        });
    });
};
