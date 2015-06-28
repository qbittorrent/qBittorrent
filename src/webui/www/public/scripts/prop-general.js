var clearData = function() {
    $('torrent_hash').set('html', '');
    $('save_path').set('html', '');
    $('creation_date').set('html', '');
    $('piece_size').set('html', '');
    $('comment').set('html', '');
    $('total_uploaded').set('html', '');
    $('total_downloaded').set('html', '');
    $('total_wasted').set('html', '');
    $('up_limit').set('html', '');
    $('dl_limit').set('html', '');
    $('time_elapsed').set('html', '');
    $('nb_connections').set('html', '');
    $('share_ratio').set('html', '');
}

var loadTorrentDataTimer;
var loadTorrentData = function() {
    if ($('prop_general').hasClass('invisible') ||
        $('propertiesPanel_collapseToggle').hasClass('panel-expand')) {
        // Tab changed, don't do anything
        return;
    }
    var current_hash = myTable.getCurrentTorrentHash();
    if (current_hash == "") {
        clearData();
        clearTimeout(loadTorrentDataTimer);
        loadTorrentDataTimer = loadTorrentData.delay(5000);
        return;
    }
    // Display hash
    $('torrent_hash').set('html', current_hash);
    var url = 'query/propertiesGeneral/' + current_hash;
    var request = new Request.JSON({
        url: url,
        noCache: true,
        method: 'get',
        onFailure: function() {
            $('error_div').set('html', 'QBT_TR(qBittorrent client is not reachable)QBT_TR');
            clearTimeout(loadTorrentDataTimer);
            loadTorrentDataTimer = loadTorrentData.delay(10000);
        },
        onSuccess: function(data) {
            $('error_div').set('html', '');
            if (data) {
                var temp;
                // Update Torrent data
                $('save_path').set('html', data.save_path);
                temp = data.creation_date;
                var timestamp = "QBT_TR(Unknown)QBT_TR";
                if (temp != -1)
                    timestamp = new Date(data.creation_date * 1000).toISOString();
                $('creation_date').set('html', timestamp);
                $('piece_size').set('html', friendlyUnit(data.piece_size));
                $('comment').set('html', data.comment);
                $('total_uploaded').set('html', friendlyUnit(data.total_uploaded) +
                    " (" + friendlyUnit(data.total_uploaded_session) +
                    " QBT_TR(this session)QBT_TR" + ")");
                $('total_downloaded').set('html', friendlyUnit(data.total_downloaded) +
                    " (" + friendlyUnit(data.total_downloaded_session) +
                    " QBT_TR(this session)QBT_TR" + ")");
                $('total_wasted').set('html', friendlyUnit(data.total_wasted));
                temp = data.up_limit;
                $('up_limit').set('html', temp == -1 ? "∞" : temp);
                temp = data.dl_limit;
                $('dl_limit').set('html', temp == -1 ? "∞" : temp);
                temp = friendlyDuration(data.time_elapsed) +
                    " (" + "QBT_TR(Seeded for %1)QBT_TR".replace("%1", friendlyDuration(data.seeding_time)) + ")";
                $('time_elapsed').set('html', temp);
                temp = data.nb_connections + " (" + "QBT_TR(%1 max)QBT_TR".replace("%1", data.nb_connections_limit) + ")";
                $('nb_connections').set('html', temp);
                $('share_ratio').set('html', data.share_ratio.toFixed(2));
            }
            else {
                clearData();
            }
            clearTimeout(loadTorrentDataTimer);
            loadTorrentDataTimer = loadTorrentData.delay(5000);
        }
    }).send();
}

var updateTorrentData = function() {
    clearTimeout(loadTorrentDataTimer);
    loadTorrentData();
}
