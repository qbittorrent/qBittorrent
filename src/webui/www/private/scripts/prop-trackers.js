var trackersDynTable = new Class({

    initialize: function() {},

    setup: function(table, contextMenu) {
        this.table = $(table);
        this.rows = new Hash();
        this.contextMenu = contextMenu;
    },

    removeRow: function(url) {
        if (this.rows.has(url)) {
            var tr = this.rows.get(url);
            tr.dispose();
            this.rows.erase(url);
            return true;
        }
        return false;
    },

    removeAllRows: function() {
        this.rows.each(function(tr, url) {
            this.removeRow(url);
        }.bind(this));
    },

    updateRow: function(tr, row) {
        var tds = tr.getElements('td');
        for (var i = 0; i < row.length; ++i) {
            tds[i].set('html', row[i]);
        }
        return true;
    },

    insertRow: function(row) {
        var url = row[1];
        if (this.rows.has(url)) {
            var tableRow = this.rows.get(url);
            this.updateRow(tableRow, row);
            return;
        }
        //this.removeRow(id);
        var tr = new Element('tr');
        this.rows.set(url, tr);
        for (var i = 0; i < row.length; ++i) {
            var td = new Element('td');
            td.set('html', row[i]);
            td.injectInside(tr);
        }
        this.contextMenu.addTarget(tr);
        tr.injectInside(this.table);
    }
});

var current_hash = "";
var selectedTracker = "";

var loadTrackersDataTimer;
var loadTrackersData = function() {
    if ($('prop_trackers').hasClass('invisible')
        || $('propertiesPanel_collapseToggle').hasClass('panel-expand')) {
        // Tab changed, don't do anything
        return;
    }
    var new_hash = torrentsTable.getCurrentTorrentHash();
    if (new_hash === "") {
        torrentTrackersTable.removeAllRows();
        clearTimeout(loadTrackersDataTimer);
        loadTrackersDataTimer = loadTrackersData.delay(10000);
        return;
    }
    if (new_hash != current_hash) {
        torrentTrackersTable.removeAllRows();
        current_hash = new_hash;
    }
    var url = new URI('api/v2/torrents/trackers?hash=' + current_hash);
    new Request.JSON({
        url: url,
        noCache: true,
        method: 'get',
        onFailure: function() {
            $('error_div').set('html', 'QBT_TR(qBittorrent client is not reachable)QBT_TR[CONTEXT=HttpServer]');
            clearTimeout(loadTrackersDataTimer);
            loadTrackersDataTimer = loadTrackersData.delay(20000);
        },
        onSuccess: function(trackers) {
            $('error_div').set('html', '');
            torrentTrackersTable.removeAllRows();

            if (trackers) {
                // Update Trackers data
                trackers.each(function(tracker) {
                    var status;
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

                    var row = [
                        tracker.tier,
                        escapeHtml(tracker.url),
                        status,
                        tracker.num_peers,
                        (tracker.num_seeds >= 0) ? tracker.num_seeds : "QBT_TR(N/A)QBT_TR[CONTEXT=TrackerListWidget]",
                        (tracker.num_leeches >= 0) ? tracker.num_leeches : "QBT_TR(N/A)QBT_TR[CONTEXT=TrackerListWidget]",
                        (tracker.num_downloaded >= 0) ? tracker.num_downloaded : "QBT_TR(N/A)QBT_TR[CONTEXT=TrackerListWidget]",
                        escapeHtml(tracker.msg)
                    ];

                    torrentTrackersTable.insertRow(row);
                });
            }
            clearTimeout(loadTrackersDataTimer);
            loadTrackersDataTimer = loadTrackersData.delay(10000);
        }
    }).send();
};

var updateTrackersData = function() {
    clearTimeout(loadTrackersDataTimer);
    loadTrackersData();
};

var torrentTrackersContextMenu = new ContextMenu({
    targets: '.torrentTrackersMenuTarget',
    menu: 'torrentTrackersMenu',
    actions: {
        AddTracker: function(element, ref) {
            addTrackerFN();
        },
        EditTracker: function(element, ref) {
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
        var element = this.options.element;
        selectedTracker = element;
        if (element.childNodes[1].innerText.indexOf("** [") === 0) {
            this.hideItem('EditTracker');
            this.hideItem('RemoveTracker');
            this.hideItem('CopyTrackerUrl');
        }
        else {
            this.showItem('EditTracker');
            this.showItem('RemoveTracker');
            this.showItem('CopyTrackerUrl');
        }
        this.options.element.firstChild.click();
    }
});

var addTrackerFN = function() {
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
            updateTrackersData();
        }
    });
};

var editTrackerFN = function(element) {
    if (current_hash.length === 0) return;

    var trackerUrl = encodeURIComponent(element.childNodes[1].innerText);
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
            updateTrackersData();
        }
    });
};

var removeTrackerFN = function(element) {
    if (current_hash.length === 0) return;

    var trackerUrl = element.childNodes[1].innerText;
    new Request({
        url: 'api/v2/torrents/removeTrackers',
        method: 'post',
        data: {
            hash: current_hash,
            urls: trackerUrl
        },
        onSuccess: function() {
            updateTrackersData();
        }
    }).send();
};

torrentTrackersTable = new trackersDynTable();
torrentTrackersTable.setup($('trackersTable'), torrentTrackersContextMenu);

new ClipboardJS('#CopyTrackerUrl', {
    text: function(trigger) {
        if (selectedTracker) {
            var url = selectedTracker.childNodes[1].innerText;
            selectedTracker = "";
            return url;
        }
    }
});
