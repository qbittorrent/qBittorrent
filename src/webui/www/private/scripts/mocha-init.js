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

window.qBittorrent ??= {};
window.qBittorrent.Dialog ??= (() => {
    const exports = () => {
        return {
            baseModalOptions: baseModalOptions
        };
    };

    const deepFreeze = (obj) => {
        // https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/freeze#examples
        // accounts for circular refs
        const frozen = new WeakSet();
        const deepFreezeSafe = (obj) => {
            if (frozen.has(obj))
                return;

            frozen.add(obj);

            const keys = Reflect.ownKeys(obj);
            for (const key of keys) {
                const value = obj[key];
                if ((value && (typeof value === "object")) || (typeof value === "function"))
                    deepFreezeSafe(value);
            }
            Object.freeze(obj);
        };

        deepFreezeSafe(obj);
    };

    const baseModalOptions = Object.assign(Object.create(null), {
        addClass: "modalDialog",
        collapsible: false,
        cornerRadius: 5,
        draggable: true,
        footerHeight: 20,
        icon: "images/qbittorrent-tray.svg",
        loadMethod: "xhr",
        maximizable: false,
        method: "post",
        minimizable: false,
        padding: {
            top: 15,
            right: 10,
            bottom: 15,
            left: 5
        },
        resizable: true,
        width: 480,
        onCloseComplete: () => {
            // make sure overlay is properly hidden upon modal closing
            document.getElementById("modalOverlay").style.display = "none";
        }
    });

    deepFreeze(baseModalOptions);

    return exports();
})();
Object.freeze(window.qBittorrent.Dialog);

const LocalPreferences = new window.qBittorrent.LocalPreferences.LocalPreferences();

let saveWindowSize = () => {};
let loadWindowWidth = () => {};
let loadWindowHeight = () => {};
let showDownloadPage = () => {};
let globalUploadLimitFN = () => {};
let uploadLimitFN = () => {};
let shareRatioFN = () => {};
let toggleSequentialDownloadFN = () => {};
let toggleFirstLastPiecePrioFN = () => {};
let setSuperSeedingFN = () => {};
let setForceStartFN = () => {};
let globalDownloadLimitFN = () => {};
let StatisticsLinkFN = () => {};
let downloadLimitFN = () => {};
let deleteSelectedTorrentsFN = () => {};
let stopFN = () => {};
let startFN = () => {};
let autoTorrentManagementFN = () => {};
let recheckFN = () => {};
let reannounceFN = () => {};
let setLocationFN = () => {};
let renameFN = () => {};
let renameFilesFN = () => {};
let startVisibleTorrentsFN = () => {};
let stopVisibleTorrentsFN = () => {};
let deleteVisibleTorrentsFN = () => {};
let torrentNewCategoryFN = () => {};
let torrentSetCategoryFN = () => {};
let createCategoryFN = () => {};
let createSubcategoryFN = () => {};
let editCategoryFN = () => {};
let removeCategoryFN = () => {};
let deleteUnusedCategoriesFN = () => {};
let torrentAddTagsFN = () => {};
let torrentSetTagsFN = () => {};
let torrentRemoveAllTagsFN = () => {};
let createTagFN = () => {};
let removeTagFN = () => {};
let deleteUnusedTagsFN = () => {};
let deleteTrackerFN = () => {};
let copyNameFN = () => {};
let copyInfohashFN = (policy) => {};
let copyMagnetLinkFN = () => {};
let copyIdFN = () => {};
let copyCommentFN = () => {};
let setQueuePositionFN = () => {};
let exportTorrentFN = () => {};

const initializeWindows = () => {
    saveWindowSize = (windowId) => {
        const size = $(windowId).getSize();
        LocalPreferences.set(`window_${windowId}_width`, size.x);
        LocalPreferences.set(`window_${windowId}_height`, size.y);
    };

    loadWindowWidth = (windowId, defaultValue) => {
        return LocalPreferences.get(`window_${windowId}_width`, defaultValue);
    };

    loadWindowHeight = (windowId, defaultValue) => {
        return LocalPreferences.get(`window_${windowId}_height`, defaultValue);
    };

    const addClickEvent = (el, fn) => {
        ["Link", "Button"].each((item) => {
            if ($(el + item))
                $(el + item).addEventListener("click", fn);
        });
    };

    addClickEvent("download", (e) => {
        e.preventDefault();
        e.stopPropagation();
        showDownloadPage();
    });

    showDownloadPage = (urls) => {
        const id = "downloadPage";
        const contentURL = new URL("download.html", window.location);

        if (urls && (urls.length > 0)) {
            contentURL.search = new URLSearchParams({
                urls: urls.map(encodeURIComponent).join("|")
            });
        }

        new MochaUI.Window({
            id: id,
            icon: "images/qbittorrent-tray.svg",
            title: "QBT_TR(Download from URLs)QBT_TR[CONTEXT=downloadFromURL]",
            loadMethod: "iframe",
            contentURL: contentURL.toString(),
            addClass: "windowFrame", // fixes iframe scrolling on iOS Safari
            scrollbars: true,
            maximizable: false,
            closable: true,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: loadWindowWidth(id, 500),
            height: loadWindowHeight(id, 600),
            onResize: window.qBittorrent.Misc.createDebounceHandler(500, (e) => {
                saveWindowSize(id);
            })
        });
        updateMainData();
    };

    addClickEvent("preferences", (e) => {
        e.preventDefault();
        e.stopPropagation();

        const id = "preferencesPage";
        new MochaUI.Window({
            id: id,
            icon: "images/qbittorrent-tray.svg",
            title: "QBT_TR(Options)QBT_TR[CONTEXT=OptionsDialog]",
            loadMethod: "xhr",
            toolbar: true,
            contentURL: "views/preferences.html",
            require: {
                css: ["css/Tabs.css"]
            },
            toolbarURL: "views/preferencesToolbar.html",
            maximizable: false,
            closable: true,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: loadWindowWidth(id, 730),
            height: loadWindowHeight(id, 600),
            onResize: window.qBittorrent.Misc.createDebounceHandler(500, (e) => {
                saveWindowSize(id);
            })
        });
    });

    addClickEvent("manageCookies", (e) => {
        e.preventDefault();
        e.stopPropagation();

        const id = "cookiesPage";
        new MochaUI.Window({
            id: id,
            title: "QBT_TR(Manage Cookies)QBT_TR[CONTEXT=CookiesDialog]",
            loadMethod: "xhr",
            contentURL: "views/cookies.html",
            maximizable: false,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: loadWindowWidth(id, 900),
            height: loadWindowHeight(id, 400),
            onResize: () => {
                saveWindowSize(id);
            }
        });
    });

    addClickEvent("upload", (e) => {
        e.preventDefault();
        e.stopPropagation();

        const id = "uploadPage";
        new MochaUI.Window({
            id: id,
            icon: "images/qbittorrent-tray.svg",
            title: "QBT_TR(Upload local torrent)QBT_TR[CONTEXT=HttpServer]",
            loadMethod: "iframe",
            contentURL: "upload.html",
            addClass: "windowFrame", // fixes iframe scrolling on iOS Safari
            scrollbars: true,
            maximizable: false,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: loadWindowWidth(id, 500),
            height: loadWindowHeight(id, 460),
            onResize: window.qBittorrent.Misc.createDebounceHandler(500, (e) => {
                saveWindowSize(id);
            })
        });
        updateMainData();
    });

    globalUploadLimitFN = () => {
        const contentURL = new URL("uploadlimit.html", window.location);
        contentURL.search = new URLSearchParams({
            hashes: "global"
        });
        new MochaUI.Window({
            id: "uploadLimitPage",
            icon: "images/qbittorrent-tray.svg",
            title: "QBT_TR(Global Upload Speed Limit)QBT_TR[CONTEXT=MainWindow]",
            loadMethod: "iframe",
            contentURL: contentURL.toString(),
            scrollbars: false,
            resizable: false,
            maximizable: false,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: 424,
            height: 100
        });
    };

    uploadLimitFN = () => {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length <= 0)
            return;

        const contentURL = new URL("uploadlimit.html", window.location);
        contentURL.search = new URLSearchParams({
            hashes: hashes.join("|")
        });
        new MochaUI.Window({
            id: "uploadLimitPage",
            icon: "images/qbittorrent-tray.svg",
            title: "QBT_TR(Torrent Upload Speed Limiting)QBT_TR[CONTEXT=TransferListWidget]",
            loadMethod: "iframe",
            contentURL: contentURL.toString(),
            scrollbars: false,
            resizable: false,
            maximizable: false,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: 424,
            height: 100
        });
    };

    shareRatioFN = () => {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length <= 0)
            return;

        let shareRatio = null;
        let torrentsHaveSameShareRatio = true;

        // check if all selected torrents have same share ratio
        for (let i = 0; i < hashes.length; ++i) {
            const hash = hashes[i];
            const row = torrentsTable.getRow(hash).full_data;
            const origValues = `${row.ratio_limit}|${row.seeding_time_limit}|${row.inactive_seeding_time_limit}|${row.max_ratio}`
                + `|${row.max_seeding_time}|${row.max_inactive_seeding_time}`;

            // initialize value
            if (shareRatio === null)
                shareRatio = origValues;

            if (origValues !== shareRatio) {
                torrentsHaveSameShareRatio = false;
                break;
            }
        }

        const contentURL = new URL("shareratio.html", window.location);
        contentURL.search = new URLSearchParams({
            hashes: hashes.join("|"),
            // if all torrents have same share ratio, display that share ratio. else use the default
            orig: torrentsHaveSameShareRatio ? shareRatio : ""
        });
        new MochaUI.Window({
            id: "shareRatioPage",
            icon: "images/qbittorrent-tray.svg",
            title: "QBT_TR(Torrent Upload/Download Ratio Limiting)QBT_TR[CONTEXT=UpDownRatioDialog]",
            loadMethod: "iframe",
            contentURL: contentURL.toString(),
            scrollbars: false,
            maximizable: false,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: 424,
            height: 220
        });
    };

    toggleSequentialDownloadFN = () => {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            fetch("api/v2/torrents/toggleSequentialDownload", {
                method: "POST",
                body: new URLSearchParams({
                    hashes: hashes.join("|")
                })
            });
            updateMainData();
        }
    };

    toggleFirstLastPiecePrioFN = () => {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            fetch("api/v2/torrents/toggleFirstLastPiecePrio", {
                method: "POST",
                body: new URLSearchParams({
                    hashes: hashes.join("|")
                })
            });
            updateMainData();
        }
    };

    setSuperSeedingFN = (val) => {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            fetch("api/v2/torrents/setSuperSeeding", {
                method: "POST",
                body: new URLSearchParams({
                    hashes: hashes.join("|"),
                    value: val
                })
            });
            updateMainData();
        }
    };

    setForceStartFN = () => {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            fetch("api/v2/torrents/setForceStart", {
                method: "POST",
                body: new URLSearchParams({
                    hashes: hashes.join("|"),
                    value: "true"
                })
            });
            updateMainData();
        }
    };

    globalDownloadLimitFN = () => {
        const contentURL = new URL("downloadlimit.html", window.location);
        contentURL.search = new URLSearchParams({
            hashes: "global"
        });
        new MochaUI.Window({
            id: "downloadLimitPage",
            icon: "images/qbittorrent-tray.svg",
            title: "QBT_TR(Global Download Speed Limit)QBT_TR[CONTEXT=MainWindow]",
            loadMethod: "iframe",
            contentURL: contentURL.toString(),
            scrollbars: false,
            resizable: false,
            maximizable: false,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: 424,
            height: 100
        });
    };

    StatisticsLinkFN = () => {
        const id = "statisticspage";
        new MochaUI.Window({
            id: id,
            icon: "images/qbittorrent-tray.svg",
            title: "QBT_TR(Statistics)QBT_TR[CONTEXT=StatsDialog]",
            loadMethod: "xhr",
            contentURL: "views/statistics.html",
            maximizable: false,
            padding: 10,
            width: loadWindowWidth(id, 285),
            height: loadWindowHeight(id, 415),
            onResize: window.qBittorrent.Misc.createDebounceHandler(500, (e) => {
                saveWindowSize(id);
            })
        });
    };

    downloadLimitFN = () => {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length <= 0)
            return;

        const contentURL = new URL("downloadlimit.html", window.location);
        contentURL.search = new URLSearchParams({
            hashes: hashes.join("|")
        });
        new MochaUI.Window({
            id: "downloadLimitPage",
            icon: "images/qbittorrent-tray.svg",
            title: "QBT_TR(Torrent Download Speed Limiting)QBT_TR[CONTEXT=TransferListWidget]",
            loadMethod: "iframe",
            contentURL: contentURL.toString(),
            scrollbars: false,
            resizable: false,
            maximizable: false,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: 424,
            height: 100
        });
    };

    deleteSelectedTorrentsFN = (forceDeleteFiles = false) => {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length > 0) {
            if (window.qBittorrent.Cache.preferences.get().confirm_torrent_deletion) {
                new MochaUI.Modal({
                    ...window.qBittorrent.Dialog.baseModalOptions,
                    id: "confirmDeletionPage",
                    title: "QBT_TR(Remove torrent(s))QBT_TR[CONTEXT=confirmDeletionDlg]",
                    data: {
                        hashes: hashes,
                        forceDeleteFiles: forceDeleteFiles
                    },
                    contentURL: "views/confirmdeletion.html",
                    onContentLoaded: (w) => {
                        MochaUI.resizeWindow(w, { centered: true });
                        MochaUI.centerWindow(w);
                    },
                    onCloseComplete: () => {
                        // make sure overlay is properly hidden upon modal closing
                        document.getElementById("modalOverlay").style.display = "none";
                    }
                });
            }
            else {
                fetch("api/v2/torrents/delete", {
                        method: "POST",
                        body: new URLSearchParams({
                            hashes: hashes.join("|"),
                            deleteFiles: forceDeleteFiles
                        })
                    })
                    .then((response) => {
                        if (!response.ok) {
                            alert("QBT_TR(Unable to delete torrents.)QBT_TR[CONTEXT=HttpServer]");
                            return;
                        }

                        torrentsTable.deselectAll();
                        updateMainData();
                        updatePropertiesPanel();
                    });
            }
        }
    };

    addClickEvent("delete", (e) => {
        e.preventDefault();
        e.stopPropagation();
        deleteSelectedTorrentsFN();
    });

    stopFN = () => {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            fetch("api/v2/torrents/stop", {
                method: "POST",
                body: new URLSearchParams({
                    hashes: hashes.join("|")
                })
            });
            updateMainData();
        }
    };

    startFN = () => {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            fetch("api/v2/torrents/start", {
                method: "POST",
                body: new URLSearchParams({
                    hashes: hashes.join("|")
                })
            });
            updateMainData();
        }
    };

    autoTorrentManagementFN = () => {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length > 0) {
            const enableAutoTMM = hashes.some((hash) => !(torrentsTable.getRow(hash).full_data.auto_tmm));
            if (enableAutoTMM) {
                new MochaUI.Modal({
                    ...window.qBittorrent.Dialog.baseModalOptions,
                    id: "confirmAutoTMMDialog",
                    title: "QBT_TR(Enable automatic torrent management)QBT_TR[CONTEXT=confirmAutoTMMDialog]",
                    data: {
                        hashes: hashes,
                        enable: enableAutoTMM
                    },
                    contentURL: "views/confirmAutoTMM.html"
                });
            }
            else {
                fetch("api/v2/torrents/setAutoManagement", {
                        method: "POST",
                        body: new URLSearchParams({
                            hashes: hashes.join("|"),
                            enable: enableAutoTMM
                        })
                    })
                    .then((response) => {
                        if (!response.ok) {
                            alert("QBT_TR(Unable to set Auto Torrent Management for the selected torrents.)QBT_TR[CONTEXT=HttpServer]");
                            return;
                        }

                        updateMainData();
                    });
            }
        }
    };

    recheckFN = () => {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length > 0) {
            if (window.qBittorrent.Cache.preferences.get().confirm_torrent_recheck) {
                new MochaUI.Modal({
                    ...window.qBittorrent.Dialog.baseModalOptions,
                    id: "confirmRecheckDialog",
                    title: "QBT_TR(Recheck confirmation)QBT_TR[CONTEXT=confirmRecheckDialog]",
                    data: { hashes: hashes },
                    contentURL: "views/confirmRecheck.html"
                });
            }
            else {
                fetch("api/v2/torrents/recheck", {
                        method: "POST",
                        body: new URLSearchParams({
                            hashes: hashes.join("|"),
                        })
                    })
                    .then((response) => {
                        if (!response.ok) {
                            alert("QBT_TR(Unable to recheck torrents.)QBT_TR[CONTEXT=HttpServer]");
                            return;
                        }

                        updateMainData();
                    });
            }
        }
    };

    reannounceFN = () => {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            fetch("api/v2/torrents/reannounce", {
                method: "POST",
                body: new URLSearchParams({
                    hashes: hashes.join("|")
                })
            });
            updateMainData();
        }
    };

    setLocationFN = () => {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length <= 0)
            return;

        const contentURL = new URL("setlocation.html", window.location);
        contentURL.search = new URLSearchParams({
            hashes: hashes.join("|"),
            path: encodeURIComponent(torrentsTable.getRow(hashes[0]).full_data.save_path)
        });
        new MochaUI.Window({
            id: "setLocationPage",
            icon: "images/qbittorrent-tray.svg",
            title: "QBT_TR(Set location)QBT_TR[CONTEXT=TransferListWidget]",
            loadMethod: "iframe",
            contentURL: contentURL.toString(),
            scrollbars: false,
            resizable: true,
            maximizable: false,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: 400,
            height: 130
        });
    };

    renameFN = () => {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length !== 1)
            return;

        const row = torrentsTable.getRow(hashes[0]);
        if (!row)
            return;

        const contentURL = new URL("rename.html", window.location);
        contentURL.search = new URLSearchParams({
            hash: hashes[0],
            name: row.full_data.name
        });
        new MochaUI.Window({
            id: "renamePage",
            icon: "images/qbittorrent-tray.svg",
            title: "QBT_TR(Rename)QBT_TR[CONTEXT=TransferListWidget]",
            loadMethod: "iframe",
            contentURL: contentURL.toString(),
            scrollbars: false,
            resizable: true,
            maximizable: false,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: 400,
            height: 100
        });
    };

    renameFilesFN = () => {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length === 1) {
            const hash = hashes[0];
            const row = torrentsTable.getRow(hash);
            if (row) {
                new MochaUI.Window({
                    id: "multiRenamePage",
                    icon: "images/qbittorrent-tray.svg",
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
                    resizeLimit: { x: [800], y: [420] }
                });
            }
        }
    };

    startVisibleTorrentsFN = () => {
        const hashes = torrentsTable.getFilteredTorrentsHashes(selectedStatus, selectedCategory, selectedTag, selectedTracker);
        if (hashes.length > 0) {
            fetch("api/v2/torrents/start", {
                    method: "POST",
                    body: new URLSearchParams({
                        hashes: hashes.join("|")
                    })
                })
                .then((response) => {
                    if (!response.ok) {
                        alert("QBT_TR(Unable to start torrents.)QBT_TR[CONTEXT=HttpServer]");
                        return;
                    }

                    updateMainData();
                    updatePropertiesPanel();
                });
        }
    };

    stopVisibleTorrentsFN = () => {
        const hashes = torrentsTable.getFilteredTorrentsHashes(selectedStatus, selectedCategory, selectedTag, selectedTracker);
        if (hashes.length > 0) {
            fetch("api/v2/torrents/stop", {
                    method: "POST",
                    body: new URLSearchParams({
                        hashes: hashes.join("|")
                    })
                })
                .then((response) => {
                    if (!response.ok) {
                        alert("QBT_TR(Unable to stop torrents.)QBT_TR[CONTEXT=HttpServer]");
                        return;
                    }

                    updateMainData();
                    updatePropertiesPanel();
                });
        }
    };

    deleteVisibleTorrentsFN = () => {
        const hashes = torrentsTable.getFilteredTorrentsHashes(selectedStatus, selectedCategory, selectedTag, selectedTracker);
        if (hashes.length > 0) {
            if (window.qBittorrent.Cache.preferences.get().confirm_torrent_deletion) {
                new MochaUI.Modal({
                    ...window.qBittorrent.Dialog.baseModalOptions,
                    id: "confirmDeletionPage",
                    title: "QBT_TR(Remove torrent(s))QBT_TR[CONTEXT=confirmDeletionDlg]",
                    data: {
                        hashes: hashes,
                        isDeletingVisibleTorrents: true
                    },
                    contentURL: "views/confirmdeletion.html",
                    onContentLoaded: (w) => {
                        MochaUI.resizeWindow(w, { centered: true });
                        MochaUI.centerWindow(w);
                    }
                });
            }
            else {
                fetch("api/v2/torrents/delete", {
                        method: "POST",
                        body: new URLSearchParams({
                            hashes: hashes.join("|"),
                            deleteFiles: false,
                        })
                    })
                    .then((response) => {
                        if (!response.ok) {
                            alert("QBT_TR(Unable to delete torrents.)QBT_TR[CONTEXT=HttpServer]");
                            return;
                        }

                        torrentsTable.deselectAll();
                        updateMainData();
                        updatePropertiesPanel();
                    });
            }
        }
    };

    torrentNewCategoryFN = () => {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length <= 0)
            return;

        const contentURL = new URL("newcategory.html", window.location);
        contentURL.search = new URLSearchParams({
            action: "set",
            hashes: hashes.join("|")
        });
        new MochaUI.Window({
            id: "newCategoryPage",
            icon: "images/qbittorrent-tray.svg",
            title: "QBT_TR(New Category)QBT_TR[CONTEXT=TransferListWidget]",
            loadMethod: "iframe",
            contentURL: contentURL.toString(),
            scrollbars: false,
            resizable: true,
            maximizable: false,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: 400,
            height: 150
        });
    };

    torrentSetCategoryFN = (category) => {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length <= 0)
            return;

        fetch("api/v2/torrents/setCategory", {
                method: "POST",
                body: new URLSearchParams({
                    hashes: hashes.join("|"),
                    category: category
                })
            })
            .then((response) => {
                if (!response.ok)
                    return;

                updateMainData();
            });
    };

    createCategoryFN = () => {
        const contentURL = new URL("newcategory.html", window.location);
        contentURL.search = new URLSearchParams({
            action: "create"
        });
        new MochaUI.Window({
            id: "newCategoryPage",
            icon: "images/qbittorrent-tray.svg",
            title: "QBT_TR(New Category)QBT_TR[CONTEXT=CategoryFilterWidget]",
            loadMethod: "iframe",
            contentURL: contentURL.toString(),
            scrollbars: false,
            resizable: true,
            maximizable: false,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: 400,
            height: 150
        });
    };

    createSubcategoryFN = (category) => {
        const contentURL = new URL("newcategory.html", window.location);
        contentURL.search = new URLSearchParams({
            action: "createSubcategory",
            categoryName: `${category}/`
        });
        new MochaUI.Window({
            id: "newSubcategoryPage",
            icon: "images/qbittorrent-tray.svg",
            title: "QBT_TR(New Category)QBT_TR[CONTEXT=CategoryFilterWidget]",
            loadMethod: "iframe",
            contentURL: contentURL.toString(),
            scrollbars: false,
            resizable: true,
            maximizable: false,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: 400,
            height: 150
        });
    };

    editCategoryFN = (category) => {
        const contentURL = new URL("newcategory.html", window.location);
        contentURL.search = new URLSearchParams({
            action: "edit",
            categoryName: category,
            savePath: categoryMap.get(category).savePath
        });
        new MochaUI.Window({
            id: "editCategoryPage",
            icon: "images/qbittorrent-tray.svg",
            title: "QBT_TR(Edit Category)QBT_TR[CONTEXT=TransferListWidget]",
            loadMethod: "iframe",
            contentURL: contentURL.toString(),
            scrollbars: false,
            resizable: true,
            maximizable: false,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: 400,
            height: 150
        });
    };

    removeCategoryFN = (category) => {
        fetch("api/v2/torrents/removeCategories", {
                method: "POST",
                body: new URLSearchParams({
                    categories: category
                })
            })
            .then((response) => {
                if (!response.ok)
                    return;

                setCategoryFilter(CATEGORIES_ALL);
                updateMainData();
            });
    };

    deleteUnusedCategoriesFN = () => {
        const categories = [];
        for (const category of categoryMap.keys()) {
            if (torrentsTable.getFilteredTorrentsNumber("all", category, TAGS_ALL, TRACKERS_ALL) === 0)
                categories.push(category);
        }
        fetch("api/v2/torrents/removeCategories", {
                method: "POST",
                body: new URLSearchParams({
                    categories: categories.join("\n")
                })
            })
            .then((response) => {
                if (!response.ok)
                    return;

                setCategoryFilter(CATEGORIES_ALL);
                updateMainData();
            });
    };

    torrentAddTagsFN = () => {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length <= 0)
            return;

        const contentURL = new URL("newtag.html", window.location);
        contentURL.search = new URLSearchParams({
            action: "set",
            hashes: hashes.join("|")
        });
        new MochaUI.Window({
            id: "newTagPage",
            icon: "images/qbittorrent-tray.svg",
            title: "QBT_TR(Add tags)QBT_TR[CONTEXT=TransferListWidget]",
            loadMethod: "iframe",
            contentURL: contentURL.toString(),
            scrollbars: false,
            resizable: true,
            maximizable: false,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: 250,
            height: 100
        });
    };

    torrentSetTagsFN = (tag, isSet) => {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length <= 0)
            return;

        fetch((isSet ? "api/v2/torrents/addTags" : "api/v2/torrents/removeTags"), {
            method: "POST",
            body: new URLSearchParams({
                hashes: hashes.join("|"),
                tags: tag
            })
        });
    };

    torrentRemoveAllTagsFN = () => {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            fetch("api/v2/torrents/removeTags", {
                method: "POST",
                body: new URLSearchParams({
                    hashes: hashes.join("|")
                })
            });
        }
    };

    createTagFN = () => {
        const contentURL = new URL("newtag.html", window.location);
        contentURL.search = new URLSearchParams({
            action: "create"
        });
        new MochaUI.Window({
            id: "newTagPage",
            icon: "images/qbittorrent-tray.svg",
            title: "QBT_TR(New Tag)QBT_TR[CONTEXT=TagFilterWidget]",
            loadMethod: "iframe",
            contentURL: contentURL.toString(),
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

    removeTagFN = (tag) => {
        fetch("api/v2/torrents/deleteTags", {
            method: "POST",
            body: new URLSearchParams({
                tags: tag
            })
        });
        setTagFilter(TAGS_ALL);
    };

    deleteUnusedTagsFN = () => {
        const tags = [];
        for (const tag of tagMap.keys()) {
            if (torrentsTable.getFilteredTorrentsNumber("all", CATEGORIES_ALL, tag, TRACKERS_ALL) === 0)
                tags.push(tag);
        }
        fetch("api/v2/torrents/deleteTags", {
            method: "POST",
            body: new URLSearchParams({
                tags: tags.join(",")
            })
        });
        setTagFilter(TAGS_ALL);
    };

    deleteTrackerFN = (trackerHost) => {
        if ((trackerHost === TRACKERS_ALL) || (trackerHost === TRACKERS_TRACKERLESS))
            return;

        const contentURL = new URL("confirmtrackerdeletion.html", window.location);
        contentURL.search = new URLSearchParams({
            host: trackerHost,
            urls: [...trackerMap.get(trackerHost).keys()].map(encodeURIComponent).join("|")
        });
        new MochaUI.Window({
            id: "confirmDeletionPage",
            title: "QBT_TR(Remove tracker)QBT_TR[CONTEXT=confirmDeletionDlg]",
            loadMethod: "iframe",
            contentURL: contentURL.toString(),
            scrollbars: false,
            resizable: true,
            maximizable: false,
            padding: 10,
            width: 424,
            height: 100,
            onCloseComplete: () => {
                updateMainData();
                setTrackerFilter(TRACKERS_ALL);
            }
        });
    };

    copyNameFN = () => {
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

    copyInfohashFN = (policy) => {
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

    copyMagnetLinkFN = () => {
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

    copyIdFN = () => {
        return torrentsTable.selectedRowsIds().join("\n");
    };

    copyCommentFN = () => {
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

    exportTorrentFN = async () => {
        const hashes = torrentsTable.selectedRowsIds();
        for (const hash of hashes) {
            const row = torrentsTable.getRow(hash);
            if (!row)
                continue;

            const name = row.full_data.name;
            const url = new URL("api/v2/torrents/export", window.location);
            url.search = new URLSearchParams({
                hash: hash
            });

            // download response to file
            await window.qBittorrent.Misc.downloadFile(url, `${name}.torrent`, "QBT_TR(Unable to export torrent file)QBT_TR[CONTEXT=MainWindow]");

            // https://stackoverflow.com/questions/53560991/automatic-file-downloads-limited-to-10-files-on-chrome-browser
            await window.qBittorrent.Misc.sleep(200);
        }
    };

    addClickEvent("stopAll", (e) => {
        e.preventDefault();
        e.stopPropagation();

        if (confirm("QBT_TR(Would you like to stop all torrents?)QBT_TR[CONTEXT=MainWindow]")) {
            fetch("api/v2/torrents/stop", {
                method: "POST",
                body: new URLSearchParams({
                    hashes: "all"
                })
            });
            updateMainData();
        }
    });

    addClickEvent("startAll", (e) => {
        e.preventDefault();
        e.stopPropagation();

        if (confirm("QBT_TR(Would you like to start all torrents?)QBT_TR[CONTEXT=MainWindow]")) {
            fetch("api/v2/torrents/start", {
                method: "POST",
                body: new URLSearchParams({
                    hashes: "all"
                })
            });
            updateMainData();
        }
    });

    ["stop", "start", "recheck"].each((item) => {
        addClickEvent(item, (e) => {
            e.preventDefault();
            e.stopPropagation();

            const hashes = torrentsTable.selectedRowsIds();
            if (hashes.length) {
                hashes.each((hash, index) => {
                    fetch(`api/v2/torrents/${item}`, {
                        method: "POST",
                        body: new URLSearchParams({
                            hashes: hash
                        })
                    });
                });
                updateMainData();
            }
        });
    });

    ["decreasePrio", "increasePrio", "topPrio", "bottomPrio"].each((item) => {
        addClickEvent(item, (e) => {
            e.preventDefault();
            e.stopPropagation();
            setQueuePositionFN(item);
        });
    });

    setQueuePositionFN = (cmd) => {
        const hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            fetch(`api/v2/torrents/${cmd}`, {
                method: "POST",
                body: new URLSearchParams({
                    hashes: hashes.join("|")
                })
            });
            updateMainData();
        }
    };

    addClickEvent("about", (e) => {
        e.preventDefault();
        e.stopPropagation();

        const id = "aboutpage";
        new MochaUI.Window({
            id: id,
            icon: "images/qbittorrent-tray.svg",
            title: "QBT_TR(About qBittorrent)QBT_TR[CONTEXT=AboutDialog]",
            loadMethod: "xhr",
            contentURL: "views/about.html",
            require: {
                css: ["css/Tabs.css"]
            },
            toolbar: true,
            toolbarURL: "views/aboutToolbar.html",
            padding: 10,
            width: loadWindowWidth(id, 570),
            height: loadWindowHeight(id, 360),
            onResize: window.qBittorrent.Misc.createDebounceHandler(500, (e) => {
                saveWindowSize(id);
            })
        });
    });

    addClickEvent("logout", (e) => {
        e.preventDefault();
        e.stopPropagation();

        fetch("api/v2/auth/logout", {
                method: "POST"
            })
            .then((response) => {
                if (!response.ok)
                    return;

                window.location.reload(true);
            });
    });

    addClickEvent("shutdown", (e) => {
        e.preventDefault();
        e.stopPropagation();

        if (confirm("QBT_TR(Are you sure you want to quit qBittorrent?)QBT_TR[CONTEXT=MainWindow]")) {
            fetch("api/v2/app/shutdown", {
                    method: "POST"
                })
                .then((response) => {
                    if (!response.ok)
                        return;

                    const shutdownMessage = "QBT_TR(%1 has been shutdown)QBT_TR[CONTEXT=HttpServer]".replace("%1", window.qBittorrent.Client.mainTitle());
                    document.write(`<!doctype html><html lang="${LANG}"><head> <meta charset="UTF-8"> <meta name="color-scheme" content="light dark"> <title>${shutdownMessage}</title> <style>* {font-family: Arial, Helvetica, sans-serif;}</style></head><body> <h1 style="text-align: center;">${shutdownMessage}</h1></body></html>`);
                    document.close();
                    window.stop();
                    window.qBittorrent.Client.stop();
                });
        }
    });

    // Deactivate menu header links
    for (const el of document.querySelectorAll("a.returnFalse")) {
        el.addEventListener("click", (e) => {
            e.preventDefault();
            e.stopPropagation();
        });
    }
};
