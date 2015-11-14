var webseedsDynTable = new Class({

    initialize: function() {},

    setup: function(table) {
        this.table = $(table);
        this.rows = new Hash();
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
        for (var i = 0; i < row.length; i++) {
            tds[i].set('html', row[i]);
        }
        return true;
    },

    insertRow: function(row) {
        var url = row[0];
        if (this.rows.has(url)) {
            var tr = this.rows.get(url);
            this.updateRow(tr, row);
            return;
        }
        //this.removeRow(id);
        var tr = new Element('tr');
        this.rows.set(url, tr);
        for (var i = 0; i < row.length; i++) {
            var td = new Element('td');
            td.set('html', row[i]);
            td.injectInside(tr);
        }
        tr.injectInside(this.table);
    },
});

var current_hash = "";

var loadWebSeedsDataTimer;
var loadWebSeedsData = function() {
    if ($('prop_webseeds').hasClass('invisible') ||
        $('propertiesPanel_collapseToggle').hasClass('panel-expand')) {
        // Tab changed, don't do anything
        return;
    }
    var new_hash = myTable.getCurrentTorrentHash();
    if (new_hash == "") {
        wsTable.removeAllRows();
        clearTimeout(loadWebSeedsDataTimer);
        loadWebSeedsDataTimer = loadWebSeedsData.delay(10000);
        return;
    }
    if (new_hash != current_hash) {
        wsTable.removeAllRows();
        current_hash = new_hash;
    }
    var url = 'query/propertiesWebSeeds/' + current_hash;
    var request = new Request.JSON({
        url: url,
        noCache: true,
        method: 'get',
        onFailure: function() {
            $('error_div').set('html', 'QBT_TR(qBittorrent client is not reachable)QBT_TR');
            clearTimeout(loadWebSeedsDataTimer);
            loadWebSeedsDataTimer = loadWebSeedsData.delay(20000);
        },
        onSuccess: function(webseeds) {
            $('error_div').set('html', '');
            if (webseeds) {
                // Update WebSeeds data
                webseeds.each(function(webseed) {
                    var row = new Array();
                    row.length = 1;
                    row[0] = webseed.url;
                    wsTable.insertRow(row);
                });
            }
            else {
                wsTable.removeAllRows();
            }
            clearTimeout(loadWebSeedsDataTimer);
            loadWebSeedsDataTimer = loadWebSeedsData.delay(10000);
        }
    }).send();
}

var updateWebSeedsData = function() {
    clearTimeout(loadWebSeedsDataTimer);
    loadWebSeedsData();
}

wsTable = new webseedsDynTable();
wsTable.setup($('webseedsTable'));
