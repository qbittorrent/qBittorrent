var is_seed = true;
var current_hash = "";

var setCBState = function(state) {
    $("tristate_cb").state = state;
    if (state === "partial") {
        $("tristate_cb").indeterminate = true;
    }
    else if (state === "checked") {
        $("tristate_cb").indeterminate = false;
        $("tristate_cb").checked = true;
    }
    else if (state === "unchecked") {
        $("tristate_cb").indeterminate = false;
        $("tristate_cb").checked = false;
    }
};

var switchCBState = function() {
    // Uncheck
    if (($("tristate_cb").state === "partial") || ($("tristate_cb").state === "checked")) {
        $("tristate_cb").state = "unchecked";
        $("tristate_cb").checked = false;
        // Uncheck all checkboxes
        var indexes = [];
        $$('input.DownloadedCB').each(function(item, index) {
            item.erase("checked");
            indexes.push(index);
        });
        setFilePriority(indexes, 0);
    }
    else if ($("tristate_cb").state === "unchecked") {
        // Check
        $("tristate_cb").state = "checked";
        $("tristate_cb").checked = true;
        // Check all checkboxes
        var indexes = [];
        $$('input.DownloadedCB').each(function(item, index) {
            item.set("checked", "checked");
            indexes.push(index);
        });
        setFilePriority(indexes, FilePriority.Normal);
    }
};

var allCBChecked = function() {
    var CBs = $$('input.DownloadedCB');
    for (var i = 0; i < CBs.length; i += 1) {
        var item = CBs[i];
        if (!$defined(item.get('checked')) || !item.get('checked'))
            return false;
    }
    return true;
};

var allCBUnchecked = function() {
    var CBs = $$('input.DownloadedCB');
    for (var i = 0; i < CBs.length; i += 1) {
        var item = CBs[i];
        if ($defined(item.get('checked')) && item.get('checked'))
            return false;
    }
    return true;
};

var FilePriority = {
    "Ignored": 0,
    "Normal": 1,
    "High": 6,
    "Maximum": 7,
    "Mixed": -1
};

var normalizePriority = function(priority) {
    switch (priority) {
        case FilePriority.Ignored:
        case FilePriority.Normal:
        case FilePriority.High:
        case FilePriority.Maximum:
        case FilePriority.Mixed:
            return priority;
        default:
            return FilePriority.Normal;
    }
};

var setFilePriority = function(id, priority) {
    if (current_hash === "") return;
    var ids = Array.isArray(id) ? id : [id];

    clearTimeout(loadTorrentFilesDataTimer);
    new Request({
        url: 'api/v2/torrents/filePrio',
        method: 'post',
        data: {
            'hash': current_hash,
            'id': ids.join('|'),
            'priority': priority
        },
        onComplete: function() {
            loadTorrentFilesDataTimer = loadTorrentFilesData.delay(1000);
        }
    }).send();
    // Display or add combobox
    if (priority > 0) {
        ids.forEach(function(_id) {
            if ($('comboPrio' + _id).hasClass("invisible")) {
                $('comboPrio' + _id).set("value", priority);
                $('comboPrio' + _id).removeClass("invisible");
            }
        });
    }
    else {
        ids.forEach(function(_id) {
            if (!$('comboPrio' + _id).hasClass("invisible"))
                $('comboPrio' + _id).addClass("invisible");
        });
    }
};

var createDownloadedCB = function(id, downloaded) {
    var CB = new Element('input');
    CB.set('type', 'checkbox');
    if (downloaded)
        CB.set('checked', 'checked');
    CB.set('id', 'cbPrio' + id);
    CB.set('class', 'DownloadedCB');
    CB.addEvent('change', function(e) {
        var checked = 0;
        if ($defined($('cbPrio' + id).get('checked')) && $('cbPrio' + id).get('checked'))
            checked = 1;
        setFilePriority(id, checked);
        if (allCBChecked()) {
            setCBState("checked");
        }
        else if (allCBUnchecked()) {
            setCBState("unchecked");
        }
        else {
            setCBState("partial");
        }
    });
    return CB;
};

var createPriorityCombo = function(id, selected_prio) {
    var select = new Element('select');
    select.set('id', 'comboPrio' + id);
    select.addEvent('change', function(e) {
        var new_prio = $('comboPrio' + id).get('value');
        setFilePriority(id, new_prio);
    });

    function createOptionElement(priority, html) {
        var elem = new Element("option");
        elem.set('value', priority.toString());
        elem.set('html', html);
        if (priority == selected_prio)
            elem.setAttribute('selected', '');
        return elem;
    }

    createOptionElement(FilePriority.Normal, "QBT_TR(Normal)QBT_TR[CONTEXT=PropListDelegate]").injectInside(select);
    createOptionElement(FilePriority.High, "QBT_TR(High)QBT_TR[CONTEXT=PropListDelegate]").injectInside(select);
    createOptionElement(FilePriority.Maximum, "QBT_TR(Maximum)QBT_TR[CONTEXT=PropListDelegate]").injectInside(select);

    if (is_seed || (selected_prio === FilePriority.Ignored) || (selected_prio === FilePriority.Mixed)) {
        select.addClass("invisible");
    }
    else {
        select.removeClass("invisible");
    }
    select.addClass("combo_priority");
    return select;
};

var filesDynTable = new Class({

    initialize: function() {},

    setup: function(table) {
        this.table = $(table);
        this.rows = new Hash();
    },

    removeRow: function(id) {
        if (this.rows.has(id)) {
            var tr = this.rows.get(id);
            tr.dispose();
            this.rows.erase(id);
            return true;
        }
        return false;
    },

    removeAllRows: function() {
        this.rows.each(function(tr, id) {
            this.removeRow(id);
        }.bind(this));
    },

    updateRow: function(tr, row, id) {
        var tds = tr.getElements('td');
        for (var i = 0; i < row.length; ++i) {
            switch (i) {
                case 0:  // checkbox
                    if (row[i] > 0)
                        tds[i].getChildren('input')[0].set('checked', 'checked');
                    else
                        tds[i].getChildren('input')[0].removeProperty('checked');
                    break;
                case 3:  // progress bar
                    $('pbf_' + id).setValue(row[i].toFloat());
                    break;
                case 4:  // download priority
                    var priority = normalizePriority(row[i]);
                    if (!is_seed && (priority > 0)) {
                        tds[i].getChildren('select').set('value', priority);
                        if ($('comboPrio' + id).hasClass("invisible"))
                            $('comboPrio' + id).removeClass("invisible");
                    }
                    else {
                        if (!$('comboPrio' + id).hasClass("invisible"))
                            $('comboPrio' + id).addClass("invisible");
                    }
                    break;
                default:
                    tds[i].set('html', row[i]);
            }
        }
        return true;
    },

    insertRow: function(id, row) {
        if (this.rows.has(id)) {
            var tableRow = this.rows.get(id);
            this.updateRow(tableRow, row, id);
            return;
        }
        //this.removeRow(id);
        var tr = new Element('tr');
        this.rows.set(id, tr);
        for (var i = 0; i < row.length; ++i) {
            var td = new Element('td');
            switch (i) {
                case 0:
                    var tree_img = new Element('img', {
                        src: 'images/L.gif',
                        style: 'margin-bottom: -2px'
                    });
                    td.adopt(tree_img, createDownloadedCB(id, row[i]));
                    break;
                case 1:
                    td.set('html', row[i]);
                    td.set('title', row[i]);
                    break;
                case 3:
                    td.adopt(new ProgressBar(row[i].toFloat(), {
                        'id': 'pbf_' + id,
                        'width': 80
                    }));
                    break;
                case 4:
                    td.adopt(createPriorityCombo(id, normalizePriority(row[i])));
                    break;
                default:
                    td.set('html', row[i]);
                    break;
            }
            td.injectInside(tr);
        }
        tr.injectInside(this.table);
    },
});

var loadTorrentFilesDataTimer;
var loadTorrentFilesData = function() {
    if ($('prop_files').hasClass('invisible')
        || $('propertiesPanel_collapseToggle').hasClass('panel-expand')) {
        // Tab changed, don't do anything
        return;
    }
    var new_hash = torrentsTable.getCurrentTorrentHash();
    if (new_hash === "") {
        fTable.removeAllRows();
        clearTimeout(loadTorrentFilesDataTimer);
        loadTorrentFilesDataTimer = loadTorrentFilesData.delay(5000);
        return;
    }
    if (new_hash != current_hash) {
        fTable.removeAllRows();
        current_hash = new_hash;
    }
    var url = new URI('api/v2/torrents/files?hash=' + current_hash);
    new Request.JSON({
        url: url,
        noCache: true,
        method: 'get',
        onFailure: function() {
            $('error_div').set('html', 'QBT_TR(qBittorrent client is not reachable)QBT_TR[CONTEXT=HttpServer]');
            clearTimeout(loadTorrentFilesDataTimer);
            loadTorrentFilesDataTimer = loadTorrentFilesData.delay(10000);
        },
        onSuccess: function(files) {
            $('error_div').set('html', '');
            if (files) {
                // Update Trackers data
                var i = 0;
                files.each(function(file) {
                    if (i === 0) {
                        is_seed = file.is_seed;
                    }
                    var row = [];
                    row.length = 4;
                    row[0] = file.priority;
                    row[1] = escapeHtml(file.name);
                    row[2] = friendlyUnit(file.size, false);
                    row[3] = (file.progress * 100).round(1);
                    if (row[3] == 100.0 && file.progress < 1.0)
                        row[3] = 99.9;
                    row[4] = file.priority;
                    row[5] = friendlyUnit(file.size * (1.0 - file.progress));
                    row[6] = friendlyPercentage(file.availability);

                    fTable.insertRow(i, row);
                    ++i;
                }.bind(this));
                // Set global CB state
                if (allCBChecked()) {
                    setCBState("checked");
                }
                else if (allCBUnchecked()) {
                    setCBState("unchecked");
                }
                else {
                    setCBState("partial");
                }
            }
            else {
                fTable.removeAllRows();
            }
            clearTimeout(loadTorrentFilesDataTimer);
            loadTorrentFilesDataTimer = loadTorrentFilesData.delay(5000);
        }
    }).send();
};

var updateTorrentFilesData = function() {
    clearTimeout(loadTorrentFilesDataTimer);
    loadTorrentFilesData();
};

fTable = new filesDynTable();
fTable.setup($('filesTable'));
