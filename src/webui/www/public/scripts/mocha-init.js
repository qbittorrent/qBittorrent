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
    }
}

function getLocalStorageItem(name, defaultVal) {
    val = localStorage.getItem(name);
    if (val === null || val === undefined)
        val = defaultVal;
    return val;
}

var deleteFN = function() {};
var startFN = function() {};
var pauseFN = function() {};

initializeWindows = function() {

    function addClickEvent(el, fn) {
        ['Link', 'Button'].each(function(item) {
            if ($(el + item)) {
                $(el + item).addEvent('click', fn);
            }
        });
    }

    addClickEvent('download', function(e) {
        new Event(e).stop();
        new MochaUI.Window({
            id: 'downloadPage',
            title: "QBT_TR(Download from URLs)QBT_TR",
            loadMethod: 'iframe',
            contentURL: 'download.html',
            scrollbars: true,
            resizable: false,
            maximizable: false,
            closable: true,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: 500,
            height: 300
        });
        updateMainData();
    });

    addClickEvent('preferences', function(e) {
        new Event(e).stop();
        new MochaUI.Window({
            id: 'preferencesPage',
            title: "QBT_TR(Options)QBT_TR",
            loadMethod: 'xhr',
            toolbar: true,
            contentURL: 'preferences_content.html',
            require: {
                css: ['css/Tabs.css']
            },
            toolbarURL: 'preferences.html',
            resizable: true,
            maximizable: false,
            closable: true,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: 700,
            height: 300
        });
    });

    addClickEvent('upload', function(e) {
        new Event(e).stop();
        new MochaUI.Window({
            id: 'uploadPage',
            title: "QBT_TR(Download local torrent)QBT_TR",
            loadMethod: 'iframe',
            contentURL: 'upload.html',
            scrollbars: true,
            resizable: false,
            maximizable: false,
            paddingVertical: 0,
            paddingHorizontal: 0,
            width: 600,
            height: 130
        });
        updateMainData();
    });

    globalUploadLimitFN = function() {
        new MochaUI.Window({
            id: 'uploadLimitPage',
            title: "QBT_TR(Global Upload Speed Limit)QBT_TR",
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
    }

    uploadLimitFN = function() {
        var h = myTable.selectedIds();
        if (h.length) {
            var hash = h[0];
            new MochaUI.Window({
                id: 'uploadLimitPage',
                title: "QBT_TR(Torrent Upload Speed Limiting)QBT_TR",
                loadMethod: 'iframe',
                contentURL: 'uploadlimit.html?hashes=' + h.join("|"),
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

    toggleSequentialDownloadFN = function() {
        var h = myTable.selectedIds();
        if (h.length) {
            new Request({
                url: 'command/toggleSequentialDownload',
                method: 'post',
                data: {
                    hashes: h.join("|")
                }
            }).send();
            updateMainData();
        }
    };

    toggleFirstLastPiecePrioFN = function() {
        var h = myTable.selectedIds();
        if (h.length) {
            new Request({
                url: 'command/toggleFirstLastPiecePrio',
                method: 'post',
                data: {
                    hashes: h.join("|")
                }
            }).send();
            updateMainData();
        }
    };

    setSuperSeedingFN = function(val) {
        var h = myTable.selectedIds();
        if (h.length) {
            new Request({
                url: 'command/setSuperSeeding',
                method: 'post',
                data: {
                    value: val,
                    hashes: h.join("|")
                }
            }).send();
            updateMainData();
        }
    };

    setForceStartFN = function(val) {
        var h = myTable.selectedIds();
        if (h.length) {
            new Request({
                url: 'command/setForceStart',
                method: 'post',
                data: {
                    value: val,
                    hashes: h.join("|")
                }
            }).send();
            updateMainData();
        }
    };

    globalDownloadLimitFN = function() {
        new MochaUI.Window({
            id: 'downloadLimitPage',
            title: "QBT_TR(Global Download Speed Limit)QBT_TR",
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
    }

    downloadLimitFN = function() {
        var h = myTable.selectedIds();
        if (h.length) {
            var hash = h[0];
            new MochaUI.Window({
                id: 'downloadLimitPage',
                title: "QBT_TR(Torrent Download Speed Limiting)QBT_TR",
                loadMethod: 'iframe',
                contentURL: 'downloadlimit.html?hashes=' + h.join("|"),
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
        var h = myTable.selectedIds();
        /*if(h.length && confirm('QBT_TR(Are you sure you want to delete the selected torrents from the transfer list?)QBT_TR')) {
            h.each(function(item, index){
                new Request({url: 'command/delete', method: 'post', data: {hash: item}}).send();
            });
        }*/
        if (h.length) {
            new MochaUI.Window({
                id: 'confirmDeletionPage',
                title: "QBT_TR(Deletion confirmation - qBittorrent)QBT_TR",
                loadMethod: 'iframe',
                contentURL: 'confirmdeletion.html?hashes=' + h.join(','),
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
        var h = myTable.selectedIds();
        if (h.length) {
            h.each(function(hash, index) {
                new Request({
                    url: 'command/pause',
                    method: 'post',
                    data: {
                        hash: hash
                    }
                }).send();
            });
            updateMainData();
        }
    };

    startFN = function() {
        var h = myTable.selectedIds();
        if (h.length) {
            h.each(function(hash, index) {
                new Request({
                    url: 'command/resume',
                    method: 'post',
                    data: {
                        hash: hash
                    }
                }).send();
            });
            updateMainData();
        }
    };

    recheckFN = function() {
        var h = myTable.selectedIds();
        if (h.length) {
            h.each(function(hash, index) {
                new Request({
                    url: 'command/recheck',
                    method: 'post',
                    data: {
                        hash: hash
                    }
                }).send();
            });
            updateMainData();
        }
    };

    ['pauseAll', 'resumeAll'].each(function(item) {
        addClickEvent(item, function(e) {
            new Event(e).stop();
            new Request({
                url: 'command/' + item
            }).send();
            updateMainData();
        });
    });

    ['pause', 'resume', 'recheck'].each(function(item) {
        addClickEvent(item, function(e) {
            new Event(e).stop();
            var h = myTable.selectedIds();
            if (h.length) {
                h.each(function(hash, index) {
                    new Request({
                        url: 'command/' + item,
                        method: 'post',
                        data: {
                            hash: hash
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
        var h = myTable.selectedIds();
        if (h.length) {
            new Request({
                url: 'command/' + cmd,
                method: 'post',
                data: {
                    hashes: h.join("|")
                }
            }).send();
            updateMainData();
        }
    }

    addClickEvent('about', function(e) {
        new Event(e).stop();
        new MochaUI.Window({
            id: 'aboutpage',
            title: 'QBT_TR(About)QBT_TR',
            loadMethod: 'xhr',
            contentURL: 'about.html',
            width: 550,
            height: 290,
            padding: 10
        });
    });

    addClickEvent('logout', function(e) {
        new Event(e).stop();
        new Request({
            url: 'logout',
            method: 'get',
            onSuccess: function() {
                window.location.reload();
            }
        }).send();
    });

    addClickEvent('shutdown', function(e) {
        new Event(e).stop();
        if (confirm('QBT_TR(Are you sure you want to quit qBittorrent?)QBT_TR')) {
            new Request({
                url: 'command/shutdown',
                onSuccess: function() {
                    document.write("<?xml version=\"1.0\" encoding=\"UTF-8\"?><!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\"><html xmlns=\"http://www.w3.org/1999/xhtml\"><head><title>QBT_TR(qBittorrent has been shutdown.)QBT_TR</title><style type=\"text/css\">body { text-align: center; }</style></head><body><h1>QBT_TR(qBittorrent has been shutdown.)QBT_TR</h1></body></html>");
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
}
