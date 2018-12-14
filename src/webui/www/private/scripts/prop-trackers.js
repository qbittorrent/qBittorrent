'use strict';

var current_hash = "";

var loadTrackersDataTimer;
var loadTrackersData = function() {
    if ($('prop_trackers').hasClass('invisible')
        || $('propertiesPanel_collapseToggle').hasClass('panel-expand')) {
        // Tab changed, don't do anything
        return;
    }
    var new_hash = torrentsTable.getCurrentTorrentHash();
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
    var url = new URI('api/v2/torrents/trackers?hash=' + current_hash);
    new Request.JSON({
        url: url,
        noCache: true,
        method: 'get',
        onComplete: function() {
            clearTimeout(loadTrackersDataTimer);
            loadTrackersDataTimer = loadTrackersData.delay(10000);
        },
        onSuccess: function(trackers) {
            var selectedTrackers = torrentTrackersTable.selectedRowsIds();
            torrentTrackersTable.clear();

            if (trackers) {
                trackers.each(function(tracker) {
                    var url = escapeHtml(tracker.url);
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

                    var row = {
                        rowId: url,
                        tier: tracker.tier,
                        url: url,
                        status: status,
                        peers: tracker.num_peers,
                        seeds: (tracker.num_seeds >= 0) ? tracker.num_seeds : "QBT_TR(N/A)QBT_TR[CONTEXT=TrackerListWidget]",
                        leeches: (tracker.num_leeches >= 0) ? tracker.num_leeches : "QBT_TR(N/A)QBT_TR[CONTEXT=TrackerListWidget]",
                        downloaded: (tracker.num_downloaded >= 0) ? tracker.num_downloaded : "QBT_TR(N/A)QBT_TR[CONTEXT=TrackerListWidget]",
                        message: escapeHtml(tracker.msg)
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

var updateTrackersData = function() {
    clearTimeout(loadTrackersDataTimer);
    loadTrackersData();
};

var torrentTrackersContextMenu = new ContextMenu({
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
        var selectedTrackers = torrentTrackersTable.selectedRowsIds();
        var containsStaticTracker = selectedTrackers.some(function(tracker) {
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

    var selectedTrackers = torrentTrackersTable.selectedRowsIds();
    new Request({
        url: 'api/v2/torrents/removeTrackers',
        method: 'post',
        data: {
            hash: current_hash,
            urls: selectedTrackers.join("|")
        },
        onSuccess: function() {
            updateTrackersData();
        }
    }).send();
};

new ClipboardJS('#CopyTrackerUrl', {
    text: function(trigger) {
        return torrentTrackersTable.selectedRowsIds().join("\n");
    }
});

torrentTrackersTable.setup('torrentTrackersTableDiv', 'torrentTrackersTableFixedHeaderDiv', torrentTrackersContextMenu);
