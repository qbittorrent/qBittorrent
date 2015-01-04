/*
 * MIT License
 * Copyright (c) 2008 Ishan Arora <ishan@qbittorrent.org>,
 * Christophe Dumez <chris@qbittorrent.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

myTable = new dynamicTable();

var updatePropertiesPanel = function(){};
var updateTransferInfo = function(){};
var updateTransferList = function(){};
var alternativeSpeedLimits = false;

labels = [];
labelsCounter = {};
allHashes = [];

var stateToImg = function (state) {
    if (state == "pausedUP" || state == "pausedDL") {
        state = "paused";
    } else {
        if (state == "queuedUP" || state == "queuedDL") {
            state = "queued";
        } else {
            if (state == "checkingUP" || state == "checkingDL") {
                state = "checking";
            }
        }
    }
    return 'images/skin/' + state + '.png';
};

var loadStoredLabelFilter = function() {
    var data = localStorage.getItem('selected_label_filter');
    var rtval = ALL_LABEL_FILTER;

    if( data != null )
    {
        try {
            rtval = JSON.parse(data).value;
        }
        catch (e) {
            console.log("loadStoredLabelFilter: e=" + e);
        }
    }

    return rtval;
};

var ALL_LABEL_FILTER = 1;
var UNLABELED_LABEL_FILTER = 2;

filter = getLocalStorageItem('selected_filter', 'all');
labelFilter = loadStoredLabelFilter();

window.addEvent('load', function () {

    var saveColumnSizes = function () {
        var filters_width = $('Filters').getSize().x;
        var properties_height_rel = $('propertiesPanel').getSize().y / Window.getSize().y;
        localStorage.setItem('filters_width', filters_width);
        localStorage.setItem('properties_height_rel', properties_height_rel);
    };

    window.addEvent('resize', function() {
        // Resizing might takes some time.
        saveColumnSizes.delay(200);
    });

    /*MochaUI.Desktop = new MochaUI.Desktop();
    MochaUI.Desktop.desktop.setStyles({
        'background': '#fff',
        'visibility': 'visible'
    });*/
    MochaUI.Desktop.initialize();

    var filt_w = localStorage.getItem('filters_width');
    if ($defined(filt_w))
        filt_w = filt_w.toInt();
    else
        filt_w = 120;
    new MochaUI.Column({
        id : 'filtersColumn',
        placement : 'left',
        onResize : saveColumnSizes,
        width : filt_w,
        resizeLimit : [100, 300]
    });
    new MochaUI.Column({
        id : 'mainColumn',
        placement : 'main',
        width : null,
        resizeLimit : [100, 300]
    });
    MochaUI.Desktop.setDesktopSize();

    setFilter = function (f) {
        // Visually Select the right filter
        $("all_filter").removeClass("selectedFilter");
        $("downloading_filter").removeClass("selectedFilter");
        $("completed_filter").removeClass("selectedFilter");
        $("paused_filter").removeClass("selectedFilter");
        $("active_filter").removeClass("selectedFilter");
        $("inactive_filter").removeClass("selectedFilter");
        $(f + "_filter").addClass("selectedFilter");
        filter = f;
        localStorage.setItem('selected_filter', f);
        // Reload torrents
        if (typeof myTable.table != 'undefined')
            updateTransferList();
    };

    var updateLabelList = true;

    updateLabelFilterList = function()
    {
        $("labelFilter_" + ALL_LABEL_FILTER).removeClass( "selectedFilter" );
        $("labelFilter_" + UNLABELED_LABEL_FILTER).removeClass( "selectedFilter" );

        labels.each( function(label) {
            var el = $("labelFilter_s_" + label.name );

            if( el && el.removeClass ) {
                el.removeClass( "selectedFilter" );
            }
        });

        var obj;
        if( typeof labelFilter === 'number' ) {
            obj = $("labelFilter_" + labelFilter);
        }
        else {
            obj = $("labelFilter_s_" + labelFilter);
        }

        if( obj ) {
            obj.addClass("selectedFilter");
        }
    };

    setLabelFilter = function (f) {
        labelFilter = f;

        // saving as json to preserve type (avoid number to be stored as string)
        localStorage.setItem('selected_label_filter', JSON.stringify({ value:f }));

        // Reload torrents
        if (typeof myTable.table != 'undefined')
            updateTransferList();

        updateLabelFilterList();
    };

    new MochaUI.Panel({
        id : 'Filters',
        title : 'Panel',
        header : false,
        padding : {
            top : 0,
            right : 0,
            bottom : 0,
            left : 0
        },
        loadMethod : 'xhr',
        contentURL : 'filters.html',
        onContentLoaded : function () {
            setFilter(filter);
        },
        column : 'filtersColumn',
        height : 300
    });
    initializeWindows();

    var speedInTitle = localStorage.getItem('speed_in_browser_title_bar') == "true";
    if (!speedInTitle)
        $('speedInBrowserTitleBarLink').firstChild.style.opacity = '0';

    var updateLabelCounter = function(label, hash) {
        if (label && label.length > 0) {
            if (!labelsCounter[label]) {
                updateLabelList = true;
                labelsCounter[label] = [];
            }

            if (labelsCounter[label].indexOf(hash) === -1) {
                updateLabelList = true;
                labelsCounter[label].push(hash);
            }
        }

        for (var l in labelsCounter) {
            if (l == label) {
                continue;
            }

            var idx = labelsCounter[l].indexOf(hash);
            if (idx >= 0) {
                updateLabelList = true;
                labelsCounter[l].splice(idx, 1);
            }
        }
    };

    // based on qtorrentfilter.cpp
    var isTorrentDownloading = function( state ) {
        return ["downloading","pausedDL","queuedDL","stalledDL","checkingDL"].indexOf( state ) !== -1;
    };

    var isTorrentCompleted = function( state ) {
        return ["uploading","pausedUP","queuedUP","stalledUP","checkingUP"].indexOf( state ) !== -1;
    };

    var isTorrentPaused = function( state ) {
        return ["pausedUP","pausedDL"].indexOf( state ) !== -1;
    };

    var isTorrentActive = function( state ) {
        return ["downloading","uploading"].indexOf( state ) !== -1;
    };

    var isTorrentInactive = function( state ) {
        return !isTorrentActive(state);
    };

    var loadTorrentsInfoTimer;
    var loadTorrentsInfo = function () {
        var queueing_enabled = false;
        var url = new URI('json/torrents');
        url.setData('sort', myTable.table.sortedColumn);
        url.setData('reverse', myTable.table.reverseSort);
        var request = new Request.JSON({
            url : url,
            noCache : true,
            method : 'get',
            onFailure : function () {
                $('error_div').set('html', 'QBT_TR(qBittorrent client is not reachable)QBT_TR');
                clearTimeout(loadTorrentsInfoTimer);
                loadTorrentsInfoTimer = loadTorrentsInfo.delay(2000);
            },
            onSuccess : function (events) {
                $('error_div').set('html', '');
                if (events) {
                    // Add new torrents or update them
                    var torrent_hashes = myTable.getRowIds();
                    var filtered_hashes = [];
                    allHashes = [];

                    var pos = 0;
                    events.each(function (event) {
                        updateLabelCounter(event.label, event.hash);
                        allHashes.push( event.hash );

                        if(labelFilter === UNLABELED_LABEL_FILTER)
                        {
                            if(event.label != "") {
                                myTable.removeRow(event.hash);
                                return;
                            }
                        }
                        else
                        {
                            if(labelFilter !== ALL_LABEL_FILTER && labelFilter !== event.label) {
                                myTable.removeRow(event.hash);
                                return;
                            }
                        }

                        if( filter !== "all" ) {
                            var remove = false;
                            if (filter === "paused" && isTorrentPaused(event.state)) {
                                //ok
                            }
                            else if (filter === "downloading" && isTorrentDownloading(event.state)) {
                                //ok
                            }
                            else if (filter === "completed" && isTorrentCompleted(event.state)) {
                                //ok
                            }
                            else if (filter === "active" && isTorrentActive(event.state)) {
                                //ok
                            }
                            else if (filter === "inactive" && isTorrentInactive(event.state)) {
                                //ok
                            }
                            else {
                                myTable.removeRow(event.hash);
                                return;
                            }
                        }

                        filtered_hashes.push(event.hash);

                        var row = [];
                        var data = [];
                        row.length = 11;
                        row[0] = stateToImg(event.state);
                        row[1] = event.name;
                        row[2] = event.priority > -1 ? event.priority : null;
                        data[2] = event.priority;
                        row[3] = friendlyUnit(event.size, false);
                        data[3] = event.size;
                        row[4] = (event.progress * 100).round(1);
                        if (row[4] == 100.0 && event.progress != 1.0)
                            row[4] = 99.9;
                        data[4] = event.progress;
                        row[5] = event.num_seeds;
                        if (event.num_complete != -1)
                            row[5] += " (" + event.num_complete + ")";
                        data[5] = event.num_seeds;
                        row[6] = event.num_leechs;
                        if (event.num_incomplete != -1)
                            row[6] += " (" + event.num_incomplete + ")";
                        data[6] = event.num_leechs;
                        row[7] = friendlyUnit(event.dlspeed, true);
                        data[7] = event.dlspeed;
                        row[8] = friendlyUnit(event.upspeed, true);
                        data[8] = event.upspeed;
                        row[9] = friendlyDuration(event.eta);
                        data[9] = event.eta;
                        if (event.ratio == -1)
                            row[10] = "∞";
                        else
                            row[10] = (Math.floor(100 * event.ratio) / 100).toFixed(2); //Don't round up
                        data[10] = event.ratio;
                        if (row[2] != null)
                            queueing_enabled = true;
                        row[11] = event.label;
                        data[11] = event.label;

                        attrs = {};
                        attrs['downloaded'] = (event.progress == 1.0);
                        attrs['state'] = event.state;
                        attrs['seq_dl'] = (event.seq_dl == true);
                        attrs['f_l_piece_prio'] = (event.f_l_piece_prio == true);
                        attrs['label'] = event.label;

                        if (!torrent_hashes.contains(event.hash)) {
                            // New unfinished torrent
                            torrent_hashes[torrent_hashes.length] = event.hash;
                            myTable.insertRow(event.hash, row, data, attrs, pos);
                        } else {
                            // Update torrent data
                            myTable.updateRow(event.hash, row, data, attrs, pos);
                        }

                        pos++;
                    });
                    // Remove deleted torrents
                    torrent_hashes.each(function (hash) {
                        if (!filtered_hashes.contains(hash)) {
                            myTable.removeRow(hash);
                        }
                    });
                    if (queueing_enabled) {
                        $('queueingButtons').removeClass('invisible');
                        $('queueingMenuItems').removeClass('invisible');
                        myTable.showPriority();
                    } else {
                        $('queueingButtons').addClass('invisible');
                        $('queueingMenuItems').addClass('invisible');
                        myTable.hidePriority();
                    }

                    var labelList = $( 'labelList' );
                    labelList.empty();
                    labelList.appendChild( new Element('li', { html: '<a href="javascript:newLabelFN();">QBT_TR(New...)QBT_TR</a>' } ) );
                    labelList.appendChild( new Element('li', { html: '<a href="javascript:resetLabelFN();">QBT_TR(Reset)QBT_TR</a>' } ) );

                    var first = true;
                    labels.each(function (label) {
                    	var el = new Element('li',
                    			{ html: '<a href="javascript:updateLabelFN(\'' + label.name + '\');">' + label.name + '</a>' } );
                    	if( first ) {
                    		el.removeClass();
                    		el.addClass('separator');
                        	first = false;
                    	}
                    	labelList.appendChild( el );
                    });
                    
                    myTable.altRow();
                }
                clearTimeout(loadTorrentsInfoTimer);
                loadTorrentsInfoTimer = loadTorrentsInfo.delay(1500);
            }
        }).send();
    };

    updateTransferList = function() {
        clearTimeout(loadTorrentsInfoTimer);
        loadTorrentsInfo();
    };

    var loadTransferInfoTimer;
    var loadTransferInfo = function () {
        var url = 'json/transferInfo';
        var request = new Request.JSON({
            url : url,
            noCache : true,
            method : 'get',
            onFailure : function () {
                $('error_div').set('html', 'QBT_TR(qBittorrent client is not reachable)QBT_TR');
                clearTimeout(loadTransferInfoTimer);
                loadTransferInfoTimer = loadTransferInfo.delay(4000);
            },
            onSuccess : function (info) {
                if (info) {
                    var transfer_info = "";
                    if (info.dl_rate_limit != undefined)
                        transfer_info += "[" + friendlyUnit(info.dl_rate_limit, true) + "] ";
                    transfer_info += friendlyUnit(info.dl_info_speed, true);
                    transfer_info += " (" + friendlyUnit(info.dl_info_data, false) + ")"
                    $("DlInfos").set('html', transfer_info);
                    transfer_info = "";
                    if (info.up_rate_limit != undefined)
                        transfer_info += "[" + friendlyUnit(info.up_rate_limit, true) + "] ";
                    transfer_info += friendlyUnit(info.up_info_speed, true)
                    transfer_info += " (" + friendlyUnit(info.up_info_data, false) + ")"
                    $("UpInfos").set('html', transfer_info);
                    if (speedInTitle)
                        document.title = "QBT_TR(D:%1 U:%2)QBT_TR".replace("%1", friendlyUnit(info.dl_info_speed, true)).replace("%2", friendlyUnit(info.up_info_speed, true));
                    else
                        document.title = "QBT_TR(qBittorrent web User Interface)QBT_TR";
                    $('DHTNodes').set('html', 'QBT_TR(DHT: %1 nodes)QBT_TR'.replace("%1", info.dht_nodes));
                    if (info.connection_status == "connected")
                        $('connectionStatus').src = 'images/skin/connected.png';
                    else if (info.connection_status == "firewalled")
                        $('connectionStatus').src = 'images/skin/firewalled.png';
                    else
                        $('connectionStatus').src = 'images/skin/disconnected.png';
                    clearTimeout(loadTransferInfoTimer);
                    loadTransferInfoTimer = loadTransferInfo.delay(3000);
                }
            }
        }).send();
    };

    updateTransferInfo = function() {
        clearTimeout(loadTransferInfoTimer);
        loadTransferInfo();
    };

    // Start fetching data now
    loadTransferInfo();

    var loadLabelsInfoTimer;
    var loadLabelsInfo = function () {
        var url = 'json/labels';
        var request = new Request.JSON({
            url : url,
            noCache : true,
            method : 'get',
            onFailure : function () {
                $('error_div').set('html', 'QBT_TR(qBittorrent client is not reachable)QBT_TR');
                clearTimeout(loadLabelsInfoTimer);
                loadLabelsInfoTimer = loadLabelsInfo.delay(4000);
            },
            onSuccess : function (info) {
                if (info) {
                    if( !labels ) {
                        updateLabelList = true;
                    }

                    if( labels && !updateLabelList ) {
                        var new_labels = _.pluck(info, 'name');
                        var old_labels = _.pluck(labels, 'name');
                        if( !_.isEqual(new_labels, old_labels) )
                        {
                            updateLabelList = true;
                        }
                    }

                    var labelList = $( 'filterLabelList' );
                    if( !labelList || !updateLabelList ) {
                        clearTimeout(loadLabelsInfoTimer);
                        loadLabelsInfoTimer = loadLabelsInfo.delay(3000);
                        return;
                    }

                    labels = info;
                    labels.sort( function(a,b){ return a.name > b.name; } );

                    labelList.empty();
                    updateLabelList = false;

                    var create_link = function( filter, text, count )
                    {
                        var filter_str = filter;
                        var filter_id = "labelFilter_" + filter;
                        if( typeof filter !== 'number' ) {
                            filter_str = '\'' + filter + '\'';
                            filter_id = "labelFilter_s_" + filter;
                        }

                        var el = new Element(
                            'li',
                            {
                                id: filter_id,
                                html:
                                '<a href="#" onclick="setLabelFilter(' + filter_str + ');return false;">' +
                                '<img src="images/oxygen/folder-documents.png"/>' +
                                'QBT_TR(' + text + ')QBT_TR (' + count + ')' + '</a>'
                            }
                        );
                        return el;
                    };

                    var all_labels = 0;
                    try {
                        all_labels = allHashes.length;
                    } catch(e) {
                        all_labels = 0;
                    }
                    labelList.appendChild( create_link(ALL_LABEL_FILTER, 'QBT_TR(All Labels)QBT_TR', all_labels) );

                    var unlabeled = all_labels;
                    for( var label in labelsCounter ) {
                        unlabeled -= labelsCounter[label].length;
                    }
                    labelList.appendChild( create_link(UNLABELED_LABEL_FILTER, 'QBT_TR(Unlabeled)QBT_TR', unlabeled) );

                    labels.each(function (label) {
                        var count = 0;
                        if( labelsCounter[label.name] && labelsCounter[label.name].length )
                        {
                            count = labelsCounter[label.name].length;
                        }
                        labelList.appendChild( create_link(label.name, label.name, count) );
                    });

                    updateLabelFilterList();
                    clearTimeout(loadLabelsInfoTimer);
                    loadLabelsInfoTimer = loadLabelsInfo.delay(3000);
                }
            }
        }).send();
    };

    updateLabelsInfo = function() {
        clearTimeout(loadLabelsInfoTimer);
        loadLabelsInfo();
    }

    // Start fetching data now
    loadLabelsInfo();
    
    var updateAltSpeedIcon = function(enabled) {
        if (enabled)
            $('alternativeSpeedLimits').src = "images/slow.png";
        else
            $('alternativeSpeedLimits').src = "images/slow_off.png"
    }

    // Determine whether the alternative speed limits are enabled or not
    new Request({url: 'command/alternativeSpeedLimitsEnabled',
            method: 'get',
            onSuccess : function (isEnabled) {
                alternativeSpeedLimits = !!parseInt(isEnabled);
                if (alternativeSpeedLimits)
                    $('alternativeSpeedLimits').src = "images/slow.png"
            }
    }).send();

    $('alternativeSpeedLimits').addEvent('click', function() {
        // Change icon immediately to give some feedback
        updateAltSpeedIcon(!alternativeSpeedLimits);

        new Request({url: 'command/toggleAlternativeSpeedLimits',
                method: 'post',
                onComplete: function() {
                    alternativeSpeedLimits = !alternativeSpeedLimits;
                    updateTransferInfo();
                },
                onFailure: function() {
                    // Restore icon in case of failure
                    updateAltSpeedIcon(alternativeSpeedLimits)
                }
        }).send();
    });

    $('DlInfos').addEvent('click', globalDownloadLimitFN);
    $('UpInfos').addEvent('click', globalUploadLimitFN);

    setSortedColumn = function (column) {
        myTable.setSortedColumn(column);
        updateTransferList();
    };

    $('speedInBrowserTitleBarLink').addEvent('click', function(e) {
        speedInTitle = !speedInTitle;
        localStorage.setItem('speed_in_browser_title_bar', speedInTitle.toString());
        if (speedInTitle)
            $('speedInBrowserTitleBarLink').firstChild.style.opacity = '1';
        else
            $('speedInBrowserTitleBarLink').firstChild.style.opacity = '0';
        updateTransferInfo();
    });

    new MochaUI.Panel({
        id : 'transferList',
        title : 'Panel',
        header : false,
        padding : {
            top : 0,
            right : 0,
            bottom : 0,
            left : 0
        },
        loadMethod : 'xhr',
        contentURL : 'transferlist.html',
        onContentLoaded : function () {
            updateTransferList();
        },
        column : 'mainColumn',
        onResize : saveColumnSizes,
        height : null
    });
    var prop_h = localStorage.getItem('properties_height_rel');
    if ($defined(prop_h))
        prop_h = prop_h.toFloat() * Window.getSize().y;
    else
        prop_h = Window.getSize().y / 2.;
    new MochaUI.Panel({
        id : 'propertiesPanel',
        title : 'Panel',
        header : true,
        padding : {
            top : 0,
            right : 0,
            bottom : 0,
            left : 0
        },
        contentURL : 'properties_content.html',
        require : {
            css : ['css/Tabs.css'],
            js : ['scripts/prop-general.js', 'scripts/prop-trackers.js', 'scripts/prop-files.js'],
        },
        tabsURL : 'properties.html',
        tabsOnload : function() {
            MochaUI.initializeTabs('propertiesTabs');

            updatePropertiesPanel = function() {
                if (!$('prop_general').hasClass('invisible'))
                    updateTorrentData();
                else if (!$('prop_trackers').hasClass('invisible'))
                    updateTrackersData();
                else if (!$('prop_files').hasClass('invisible'))
                    updateTorrentFilesData();
            }

            $('PropGeneralLink').addEvent('click', function(e){
                $('prop_general').removeClass("invisible");
                $('prop_trackers').addClass("invisible");
                $('prop_files').addClass("invisible");
                updatePropertiesPanel();
            });

            $('PropTrackersLink').addEvent('click', function(e){
                $('prop_trackers').removeClass("invisible");
                $('prop_general').addClass("invisible");
                $('prop_files').addClass("invisible");
                updatePropertiesPanel();
            });

            $('PropFilesLink').addEvent('click', function(e){
                $('prop_files').removeClass("invisible");
                $('prop_general').addClass("invisible");
                $('prop_trackers').addClass("invisible");
                updatePropertiesPanel();
            });
        },
        column : 'mainColumn',
        height : prop_h
    });
});

function closeWindows() {
    MochaUI.closeAll();
}

window.addEvent('keydown', function (event) {
    if (event.key == 'a' && event.control) {
        event.stop();
        myTable.selectAll();
    }
});
