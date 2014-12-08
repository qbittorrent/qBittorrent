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
    if ($('prop_general').hasClass('invisible')) {
        // Tab changed, don't do anything
        return;
    }
    var current_hash = myTable.getCurrentTorrentHash();
    if (current_hash == "") {
        clearData();
        loadTorrentDataTimer = loadTorrentData.delay(5000);
        return;
    }
    // Display hash
    $('torrent_hash').set('html', current_hash);
    var url = 'json/propertiesGeneral/' + current_hash;
    var request = new Request.JSON({
        url: url,
        noCache: true,
        method: 'get',
        onFailure: function() {
            $('error_div').set('html', '_(qBittorrent client is not reachable)');
            loadTorrentDataTimer = loadTorrentData.delay(10000);
        },
        onSuccess: function(data) {
            $('error_div').set('html', '');
            if (data) {
                var temp;
                // Update Torrent data
                $('save_path').set('html', data.save_path);
                temp = data.creation_date;
                var timestamp = "_(Unknown)";
                if (temp != -1)
                    timestamp = new Date(data.creation_date * 1000).toISOString();
                $('creation_date').set('html', timestamp);
                $('piece_size').set('html', friendlyUnit(data.piece_size));
                $('comment').set('html', data.comment);
                $('total_uploaded').set('html', friendlyUnit(data.total_uploaded) +
                    " (" + friendlyUnit(data.total_uploaded_session) +
                    " _(this session)" + ")");
                $('total_downloaded').set('html', friendlyUnit(data.total_downloaded) +
                    " (" + friendlyUnit(data.total_downloaded_session) +
                    " _(this session)" + ")");
                $('total_wasted').set('html', data.total_wasted);
                temp = data.up_limit;
                $('up_limit').set('html', temp == -1 ? "∞" : temp);
                temp = data.dl_limit;
                $('dl_limit').set('html', temp == -1 ? "∞" : temp);
                temp = friendlyDuration(status.active_time);
                if (status.is_seed)
                    temp += " (" + "_(Seeded for %1)".replace("%1", status.seeding_time) + ")";
                $('time_elapsed').set('html', temp);
                temp = data.nb_connections + " (" + "_(%1 max)".replace("%1", status.nb_connections_limit) + ")";
                $('nb_connections').set('html', temp);
                $('share_ratio').set('html', data.share_ratio.toFixed(2));
            }
            else {
                clearData();
            }
            loadTorrentDataTimer = loadTorrentData.delay(5000);
        }
    }).send();
}

var updateTorrentData = function() {
    clearTimeout(loadTorrentDataTimer);
    loadTorrentData();
}