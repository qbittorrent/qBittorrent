/* -----------------------------------------------------------------

    ATTACH MOCHA LINK EVENTS
    Notes: Here is where you define your windows and the events that open them.
    If you are not using links to run Mocha methods you can remove this function.

    If you need to add link events to links within windows you are creating, do
    it in the onContentLoaded function of the new window.

   ----------------------------------------------------------------- */
/* Define localStorage object for older browsers */
if (typeof localStorage == 'undefined') {
    window['localStorage'] = {
        getItem: function(name) {
            return Cookie.read(name);
        },
        setItem: function(name, value) {
            Cookie.write(name, value, {
                duration: 365 * 10
            });
        }
    };
}

function getLocalStorageItem(name, defaultVal) {
    var val = localStorage.getItem(name);
    if (val === null || val === undefined)
        val = defaultVal;
    return val;
}

var deleteFN = function() {};
var startFN = function() {};
var pauseFN = function() {};

initializeWindows = function() {

    saveWindowSize = function(windowId) {
        var size = $(windowId).getSize();
        localStorage.setItem('window_' + windowId + '_width', size.x);
        localStorage.setItem('window_' + windowId + '_height', size.y);
    };

    loadWindowWidth = function(windowId, defaultValue) {
        return getLocalStorageItem('window_' + windowId + '_width', defaultValue);
    };

    loadWindowHeight = function(windowId, defaultValue) {
        return getLocalStorageItem('window_' + windowId + '_height', defaultValue);
    };

    function addClickEvent(el, fn) {
        ['Link', 'Button'].each(function(item) {
            if ($(el + item)) {
                $(el + item).addEvent('click', fn);
            }
        });
    }

    addClickEvent('download', function(e) {
        new Event(e).stop();
        showDownloadPage();
    });

    showDownloadPage = function(urls) {
        var id = 'downloadPage';
        var contentUrl = 'download.html';
        if (urls && urls.length)
            contentUrl += '?urls=' + urls.join("|");

        new MochaUI.Window({
            id: id,
            title: "QBT_TR(Download from URLs)QBT_TR[CONTEXT=downloadFromURL]",
            loadMethod: 'iframe',
            contentURL: contentUrl,
            addClass: 'windowFrame', // fixes iframe scrolling on iOS Safari
            scrollbars: true,
            maximizable: false,
            closable: true,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: loadWindowWidth(id, 500),
            height: loadWindowHeight(id, 420),
            onResize: function() {
                saveWindowSize(id);
            }
        });
        updateMainData();
    };

    addClickEvent('preferences', function(e) {
        new Event(e).stop();
        var id = 'preferencesPage';
        new MochaUI.Window({
            id: id,
            title: "QBT_TR(Options)QBT_TR[CONTEXT=OptionsDialog]",
            loadMethod: 'xhr',
            toolbar: true,
            contentURL: 'preferences_content.html',
            require: {
                css: ['css/Tabs.css']
            },
            toolbarURL: 'preferences.html',
            maximizable: false,
            closable: true,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: loadWindowWidth(id, 700),
            height: loadWindowHeight(id, 500),
            onResize: function() {
                saveWindowSize(id);
            }
        });
    });

    addClickEvent('upload', function(e) {
        new Event(e).stop();
        var id = 'uploadPage';
        new MochaUI.Window({
            id: id,
            title: "QBT_TR(Upload local torrent)QBT_TR[CONTEXT=HttpServer]",
            loadMethod: 'iframe',
            contentURL: 'upload.html',
            addClass: 'windowFrame', // fixes iframe scrolling on iOS Safari
            scrollbars: true,
            maximizable: false,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: loadWindowWidth(id, 500),
            height: loadWindowHeight(id, 260),
            onResize: function() {
                saveWindowSize(id);
            }
        });
        updateMainData();
    });

    globalUploadLimitFN = function() {
        new MochaUI.Window({
            id: 'uploadLimitPage',
            title: "QBT_TR(Global Upload Speed Limit)QBT_TR[CONTEXT=MainWindow]",
            loadMethod: 'iframe',
            contentURL: 'uploadlimit.html?hashes=global',
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
        var hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            new MochaUI.Window({
                id: 'uploadLimitPage',
                title: "QBT_TR(Torrent Upload Speed Limiting)QBT_TR[CONTEXT=TransferListWidget]",
                loadMethod: 'iframe',
                contentURL: 'uploadlimit.html?hashes=' + hashes.join("|"),
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
        var hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            var shareRatio = null;
            var torrentsHaveSameShareRatio = true;

            // check if all selected torrents have same share ratio
            for (var i = 0; i < hashes.length; ++i) {
                var hash = hashes[i];
                var row = torrentsTable.rows[hash].full_data;
                var origValues = row.ratio_limit + "|" + row.seeding_time_limit + "|" + row.max_ratio + "|" + row.max_seeding_time;

                // initialize value
                if (shareRatio === null)
                    shareRatio = origValues;

                if (origValues !== shareRatio) {
                    torrentsHaveSameShareRatio = false;
                    break;
                }
            }

            // if all torrents have same share ratio, display that share ratio. else use the default
            var orig = torrentsHaveSameShareRatio ? shareRatio : "";
            new MochaUI.Window({
                id: 'shareRatioPage',
                title: "QBT_TR(Torrent Upload/Download Ratio Limiting)QBT_TR[CONTEXT=UpDownRatioDialog]",
                loadMethod: 'iframe',
                contentURL: 'shareratio.html?hashes=' + hashes.join("|") + '&orig=' + orig,
                scrollbars: false,
                maximizable: false,
                paddingVertical: 0,
                paddingHorizontal: 0,
                width: 424,
                height: 175
            });
        }
    };

    toggleSequentialDownloadFN = function() {
        var hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            new Request({
                url: 'api/v2/torrents/toggleSequentialDownload',
                method: 'post',
                data: {
                    hashes: hashes.join("|")
                }
            }).send();
            updateMainData();
        }
    };

    toggleFirstLastPiecePrioFN = function() {
        var hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            new Request({
                url: 'api/v2/torrents/toggleFirstLastPiecePrio',
                method: 'post',
                data: {
                    hashes: hashes.join("|")
                }
            }).send();
            updateMainData();
        }
    };

    setSuperSeedingFN = function(val) {
        var hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            new Request({
                url: 'api/v2/torrents/setSuperSeeding',
                method: 'post',
                data: {
                    value: val,
                    hashes: hashes.join("|")
                }
            }).send();
            updateMainData();
        }
    };

    setForceStartFN = function() {
        var hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            new Request({
                url: 'api/v2/torrents/setForceStart',
                method: 'post',
                data: {
                    value: 'true',
                    hashes: hashes.join("|")
                }
            }).send();
            updateMainData();
        }
    };

    globalDownloadLimitFN = function() {
        new MochaUI.Window({
            id: 'downloadLimitPage',
            title: "QBT_TR(Global Download Speed Limit)QBT_TR[CONTEXT=MainWindow]",
            loadMethod: 'iframe',
            contentURL: 'downloadlimit.html?hashes=global',
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
        var id = 'statisticspage';
        new MochaUI.Window({
            id: id,
            title: 'QBT_TR(Statistics)QBT_TR[CONTEXT=StatsDialog]',
            loadMethod: 'xhr',
            contentURL: 'statistics.html',
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
        var hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            new MochaUI.Window({
                id: 'downloadLimitPage',
                title: "QBT_TR(Torrent Download Speed Limiting)QBT_TR[CONTEXT=TransferListWidget]",
                loadMethod: 'iframe',
                contentURL: 'downloadlimit.html?hashes=' + hashes.join("|"),
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

    deleteFN = function() {
        var hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            new MochaUI.Window({
                id: 'confirmDeletionPage',
                title: "QBT_TR(Deletion confirmation)QBT_TR[CONTEXT=confirmDeletionDlg]",
                loadMethod: 'iframe',
                contentURL: 'confirmdeletion.html?hashes=' + hashes.join("|"),
                scrollbars: false,
                resizable: false,
                maximizable: false,
                padding: 10,
                width: 424,
                height: 140
            });
            updateMainData();
        }
    };

    addClickEvent('delete', function(e) {
        new Event(e).stop();
        deleteFN();
    });

    pauseFN = function() {
        var hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            new Request({
                url: 'api/v2/torrents/pause',
                method: 'post',
                data: {
                    hashes: hashes.join("|")
                }
            }).send();
            updateMainData();
        }
    };

    startFN = function() {
        var hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            new Request({
                url: 'api/v2/torrents/resume',
                method: 'post',
                data: {
                    hashes: hashes.join("|")
                }
            }).send();
            updateMainData();
        }
    };

    autoTorrentManagementFN = function() {
        var hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            var enable = false;
            hashes.each(function(hash, index) {
                var row = torrentsTable.rows[hash];
                if (!row.full_data.auto_tmm)
                    enable = true;
            });
            new Request({
                url: 'api/v2/torrents/setAutoManagement',
                method: 'post',
                data: {
                    hashes: hashes.join("|"),
                    enable: enable
                }
            }).send();
            updateMainData();
        }
    };

    recheckFN = function() {
        var hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            new Request({
                url: 'api/v2/torrents/recheck',
                method: 'post',
                data: {
                    hashes: hashes.join("|"),
                }
            }).send();
            updateMainData();
        }
    };

    reannounceFN = function() {
        var hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            new Request({
                url: 'api/v2/torrents/reannounce',
                method: 'post',
                data: {
                    hashes: hashes.join("|"),
                }
            }).send();
            updateMainData();
        }
    };

    setLocationFN = function() {
        var hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            var hash = hashes[0];
            var row = torrentsTable.rows[hash];
            var path = encodeURIComponent(row.full_data.save_path);
            new MochaUI.Window({
                id: 'setLocationPage',
                title: "QBT_TR(Set location)QBT_TR[CONTEXT=TransferListWidget]",
                loadMethod: 'iframe',
                contentURL: 'setlocation.html?hashes=' + hashes.join('|') + '&path=' + path,
                scrollbars: false,
                resizable: false,
                maximizable: false,
                paddingVertical: 0,
                paddingHorizontal: 0,
                width: 400,
                height: 130
            });
        }
    };

    renameFN = function() {
        var hashes = torrentsTable.selectedRowsIds();
        if (hashes.length == 1) {
            var hash = hashes[0];
            var row = torrentsTable.rows[hash];
            if (row) {
                new MochaUI.Window({
                    id: 'renamePage',
                    title: "QBT_TR(Rename)QBT_TR[CONTEXT=TransferListWidget]",
                    loadMethod: 'iframe',
                    contentURL: 'rename.html?hash=' + hash + '&name=' + encodeURIComponent(row.full_data.name),
                    scrollbars: false,
                    resizable: false,
                    maximizable: false,
                    paddingVertical: 0,
                    paddingHorizontal: 0,
                    width: 250,
                    height: 100
                });
            }
        }
    };

    torrentNewCategoryFN = function() {
        var action = "set";
        var hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            new MochaUI.Window({
                id: 'newCategoryPage',
                title: "QBT_TR(New Category)QBT_TR[CONTEXT=TransferListWidget]",
                loadMethod: 'iframe',
                contentURL: 'newcategory.html?action=' + action + '&hashes=' + hashes.join('|'),
                scrollbars: false,
                resizable: false,
                maximizable: false,
                paddingVertical: 0,
                paddingHorizontal: 0,
                width: 250,
                height: 150
            });
        }
    };

    torrentSetCategoryFN = function(categoryHash) {
        var categoryName = '';
        if (categoryHash != 0)
            categoryName = category_list[categoryHash].name;
        var hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            new Request({
                url: 'api/v2/torrents/setCategory',
                method: 'post',
                data: {
                    hashes: hashes.join("|"),
                    category: categoryName
                }
            }).send();
        }
    };

    createCategoryFN = function() {
        var action = "create";
        new MochaUI.Window({
            id: 'newCategoryPage',
            title: "QBT_TR(New Category)QBT_TR[CONTEXT=CategoryFilterWidget]",
            loadMethod: 'iframe',
            contentURL: 'newcategory.html?action=' + action,
            scrollbars: false,
            resizable: false,
            maximizable: false,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: 250,
            height: 150
        });
        updateMainData();
    };

    editCategoryFN = function(categoryHash) {
        var action = "edit";
        var categoryName = category_list[categoryHash].name;
        var savePath = category_list[categoryHash].savePath;
        new MochaUI.Window({
            id: 'editCategoryPage',
            title: "QBT_TR(Edit Category)QBT_TR[CONTEXT=TransferListWidget]",
            loadMethod: 'iframe',
            contentURL: 'newcategory.html?action=' + action + '&categoryName=' + categoryName + '&savePath=' + savePath,
            scrollbars: false,
            resizable: false,
            maximizable: false,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: 250,
            height: 150
        });
        updateMainData();
    };

    removeCategoryFN = function(categoryHash) {
        var categoryName = category_list[categoryHash].name;
        new Request({
            url: 'api/v2/torrents/removeCategories',
            method: 'post',
            data: {
                categories: categoryName
            }
        }).send();
        setCategoryFilter(CATEGORIES_ALL);
    };

    deleteUnusedCategoriesFN = function() {
        var categories = [];
        for (var hash in category_list) {
            if (torrentsTable.getFilteredTorrentsNumber('all', hash) === 0)
                categories.push(category_list[hash].name);
        }
        new Request({
            url: 'api/v2/torrents/removeCategories',
            method: 'post',
            data: {
                categories: categories.join('\n')
            }
        }).send();
        setCategoryFilter(CATEGORIES_ALL);
    };

    startTorrentsByCategoryFN = function(categoryHash) {
        var hashes = torrentsTable.getFilteredTorrentsHashes('all', categoryHash);
        if (hashes.length) {
            new Request({
                url: 'api/v2/torrents/resume',
                method: 'post',
                data: {
                    hashes: hashes.join("|")
                }
            }).send();
            updateMainData();
        }
    };

    pauseTorrentsByCategoryFN = function(categoryHash) {
        var hashes = torrentsTable.getFilteredTorrentsHashes('all', categoryHash);
        if (hashes.length) {
            new Request({
                url: 'api/v2/torrents/pause',
                method: 'post',
                data: {
                    hashes: hashes.join("|")
                }
            }).send();
            updateMainData();
        }
    };

    deleteTorrentsByCategoryFN = function(categoryHash) {
        var hashes = torrentsTable.getFilteredTorrentsHashes('all', categoryHash);
        if (hashes.length) {
            new MochaUI.Window({
                id: 'confirmDeletionPage',
                title: "QBT_TR(Deletion confirmation)QBT_TR[CONTEXT=confirmDeletionDlg]",
                loadMethod: 'iframe',
                contentURL: 'confirmdeletion.html?hashes=' + hashes.join("|"),
                scrollbars: false,
                resizable: false,
                maximizable: false,
                padding: 10,
                width: 424,
                height: 140
            });
            updateMainData();
        }
    };

    copyNameFN = function() {
        var selectedRows = torrentsTable.selectedRowsIds();
        var names = [];
        if (selectedRows.length) {
            var rows = torrentsTable.getFilteredAndSortedRows();
            for (var i = 0; i < selectedRows.length; ++i) {
                var hash = selectedRows[i];
                names.push(rows[hash].full_data.name);
            }
        }
        return names.join("\n");
    };

    copyMagnetLinkFN = function() {
        var selectedRows = torrentsTable.selectedRowsIds();
        var magnets = [];
        if (selectedRows.length) {
            var rows = torrentsTable.getFilteredAndSortedRows();
            for (var i = 0; i < selectedRows.length; ++i) {
                var hash = selectedRows[i];
                magnets.push(rows[hash].full_data.magnet_uri);
            }
        }
        return magnets.join("\n");
    };

    copyHashFN = function() {
        return torrentsTable.selectedRowsIds().join("\n");
    };

    ['pause', 'resume'].each(function(item) {
        addClickEvent(item + 'All', function(e) {
            new Event(e).stop();
            new Request({
                url: 'api/v2/torrents/' + item,
                method: 'post',
                data: {
                    hashes: "all"
                }
            }).send();
            updateMainData();
        });
    });

    ['pause', 'resume', 'recheck'].each(function(item) {
        addClickEvent(item, function(e) {
            new Event(e).stop();
            var hashes = torrentsTable.selectedRowsIds();
            if (hashes.length) {
                hashes.each(function(hash, index) {
                    new Request({
                        url: 'api/v2/torrents/' + item,
                        method: 'post',
                        data: {
                            hashes: hash
                        }
                    }).send();
                });
                updateMainData();
            }
        });
    });

    ['decreasePrio', 'increasePrio', 'topPrio', 'bottomPrio'].each(function(item) {
        addClickEvent(item, function(e) {
            new Event(e).stop();
            setPriorityFN(item);
        });
    });

    setPriorityFN = function(cmd) {
        var hashes = torrentsTable.selectedRowsIds();
        if (hashes.length) {
            new Request({
                url: 'api/v2/torrents/' + cmd,
                method: 'post',
                data: {
                    hashes: hashes.join("|")
                }
            }).send();
            updateMainData();
        }
    };

    addClickEvent('about', function(e) {
        new Event(e).stop();
        var id = 'aboutpage';
        new MochaUI.Window({
            id: id,
            title: 'QBT_TR(About)QBT_TR[CONTEXT=AboutDlg]',
            loadMethod: 'xhr',
            contentURL: 'about.html',
            padding: 10,
            width: loadWindowWidth(id, 550),
            height: loadWindowHeight(id, 290),
            onResize: function() {
                saveWindowSize(id);
            }
        });
    });

    addClickEvent('logout', function(e) {
        new Event(e).stop();
        new Request({
            url: 'api/v2/auth/logout',
            method: 'post',
            onSuccess: function() {
                window.location.reload();
            }
        }).send();
    });

    addClickEvent('shutdown', function(e) {
        new Event(e).stop();
        if (confirm('QBT_TR(Are you sure you want to quit qBittorrent?)QBT_TR[CONTEXT=MainWindow]')) {
            new Request({
                url: 'api/v2/app/shutdown',
                onSuccess: function() {
                    document.write('<!doctype html><html lang="${LANG}"><head> <meta charset="utf-8"> <title>QBT_TR(qBittorrent has been shutdown)QBT_TR[CONTEXT=HttpServer]</title></head><body> <h1 style="text-align: center;">QBT_TR(qBittorrent has been shutdown)QBT_TR[CONTEXT=HttpServer]</h1></body></html>');
                    stop();
                }
            }).send();
        }
    });

    // Deactivate menu header links
    $$('a.returnFalse').each(function(el) {
        el.addEvent('click', function(e) {
            new Event(e).stop();
        });
    });
};
