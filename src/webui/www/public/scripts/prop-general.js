var clearData = function() {
    $('time_elapsed').set('html', '');
    $('eta').set('html', '');
    $('nb_connections').set('html', '');
    $('total_downloaded').set('html', '');
    $('total_uploaded').set('html', '');
    $('dl_speed').set('html', '');
    $('up_speed').set('html', '');
    $('dl_limit').set('html', '');
    $('up_limit').set('html', '');
    $('total_wasted').set('html', '');
    $('seeds').set('html', '');
    $('peers').set('html', '');
    $('share_ratio').set('html', '');
    $('reannounce').set('html', '');
    $('last_seen').set('html', '');
    $('total_size').set('html', '');
    $('pieces').set('html', '');
    $('created_by').set('html', '');
    $('addition_date').set('html', '');
    $('completion_date').set('html', '');
    $('creation_date').set('html', '');
    $('torrent_hash').set('html', '');
    $('save_path').set('html', '');
    $('comment').set('html', '');
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
                if (data.seeding_time > 0)
                    temp = "QBT_TR(%1 (%2 this session))QBT_TR"
                            .replace("%1", friendlyDuration(data.time_elapsed))
                            .replace("%2", friendlyDuration(data.seeding_time))
                else
                    temp = friendlyDuration(data.time_elapsed)
                $('time_elapsed').set('html', temp);

                $('eta').set('html', friendlyDuration(data.eta));

                temp = "QBT_TR(%1 (%2 max))QBT_TR"
                        .replace("%1", data.nb_connections)
                        .replace("%2", data.nb_connections_limit < 0 ? "∞" : data.nb_connections_limit)
                $('nb_connections').set('html', temp);

                temp = "QBT_TR(%1 (%2 this session))QBT_TR"
                        .replace("%1", friendlyUnit(data.total_downloaded))
                        .replace("%2", friendlyUnit(data.total_downloaded_session))
                $('total_downloaded').set('html', temp);

                temp = "QBT_TR(%1 (%2 this session))QBT_TR"
                        .replace("%1", friendlyUnit(data.total_uploaded))
                        .replace("%2", friendlyUnit(data.total_uploaded_session))
                $('total_uploaded').set('html', temp);

                temp = "QBT_TR(%1 (%2 avg.))QBT_TR"
                        .replace("%1", friendlyUnit(data.dl_speed, true))
                        .replace("%2", friendlyUnit(data.dl_speed_avg, true));
                $('dl_speed').set('html', temp);

                temp = "QBT_TR(%1 (%2 avg.))QBT_TR"
                        .replace("%1", friendlyUnit(data.up_speed, true))
                        .replace("%2", friendlyUnit(data.up_speed_avg, true));
                $('up_speed').set('html', temp);

                temp = (data.dl_limit == -1 ? "∞" : friendlyUnit(data.dl_limit, true));
                $('dl_limit').set('html', temp);

                temp = (data.up_limit == -1 ? "∞" : friendlyUnit(data.up_limit, true));
                $('up_limit').set('html', temp);

                $('total_wasted').set('html', friendlyUnit(data.total_wasted));

                temp = "QBT_TR(%1 (%2 total))QBT_TR"
                        .replace("%1", data.seeds)
                        .replace("%2", data.seeds_total);
                $('seeds').set('html', temp);

                temp = "QBT_TR(%1 (%2 total))QBT_TR"
                        .replace("%1", data.peers)
                        .replace("%2", data.peers_total);
                $('peers').set('html', temp);

                $('share_ratio').set('html', data.share_ratio.toFixed(2));

                $('reannounce').set('html', friendlyDuration(data.reannounce));

                if (data.last_seen != -1)
                    temp = new Date(data.last_seen * 1000).toLocaleString();
                else
                    temp = "QBT_TR(Never)QBT_TR";
                $('last_seen').set('html', temp);

                $('total_size').set('html', friendlyUnit(data.total_size));

                if (data.pieces_num != -1)
                    temp = "QBT_TR(%1 x %2 (have %3))QBT_TR"
                            .replace("%1", data.pieces_num)
                            .replace("%2", friendlyUnit(data.piece_size))
                            .replace("%3", data.pieces_have);
                else
                    temp = "QBT_TR(Unknown)QBT_TR";
                $('pieces').set('html', temp);

                $('created_by').set('html', data.created_by);
                if (data.addition_date != -1)
                    temp = new Date(data.addition_date * 1000).toLocaleString();
                else
                    temp = "QBT_TR(Unknown)QBT_TR";

                $('addition_date').set('html', temp);
                if (data.completion_date != -1)
                    temp = new Date(data.completion_date * 1000).toLocaleString();
                else
                    temp = "";

                $('completion_date').set('html', temp);

                if (data.creation_date != -1)
                    temp = new Date(data.creation_date * 1000).toLocaleString();
                else
                    temp = "QBT_TR(Unknown)QBT_TR";
                $('creation_date').set('html', temp);

                $('save_path').set('html', data.save_path);

                $('comment').set('html', parseHtmlLinks(data.comment));
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
