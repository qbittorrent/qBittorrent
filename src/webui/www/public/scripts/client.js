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

selected_filter = getLocalStorageItem('selected_filter', 'all');
selected_label = null;

var loadSelectedLabel = function () {
    if (getLocalStorageItem('any_label', '1') == '0')
        selected_label = getLocalStorageItem('selected_label', '');
    else
        selected_label = null;
}
loadSelectedLabel();

var saveSelectedLabel = function () {
    if (selected_label == null)
        localStorage.setItem('any_label', '1');
    else {
        localStorage.setItem('any_label', '0');
        localStorage.setItem('selected_label', selected_label);
    }
}


window.addEvent('load', function () {

    var saveColumnSizes = function () {
        var filters_width = $('Filters').getSize().x;
        var properties_height_rel = $('propertiesPanel').getSize().y / Window.getSize().y;
        localStorage.setItem('filters_width', filters_width);
        localStorage.setItem('properties_height_rel', properties_height_rel);
    }

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
        selected_filter = f;
        localStorage.setItem('selected_filter', f);
        // Reload torrents
        if (typeof myTable.table != 'undefined')
            updateTransferList();
    }

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
            setFilter(selected_filter);
        },
        column : 'filtersColumn',
        height : 300
    });
    initializeWindows();

    var speedInTitle = localStorage.getItem('speed_in_browser_title_bar') == "true";
    if (!speedInTitle)
        $('speedInBrowserTitleBarLink').firstChild.style.opacity = '0';

    var loadTorrentsInfoTimer;
    var loadTorrentsInfo = function () {
        var url = new URI('json/torrents');
        var request = new Request.JSON({
            url : url,
            noCache : true,
            method : 'get',
            onFailure : function () {
                $('error_div').set('html', 'QBT_TR(qBittorrent client is not reachable)QBT_TR');
                clearTimeout(loadTorrentsInfoTimer);
                loadTorrentsInfoTimer = loadTorrentsInfo.delay(2000);
            },
            onSuccess : function (response) {
                $('error_div').set('html', '');
                if (response) {
                    var queueing_enabled = false;
                    var torrents_hashes = new Array();
                    for (var i = 0; i < response.length; i++) {
                        torrents_hashes.push(response[i].hash);
                        if (response[i].priority > -1)
                            queueing_enabled = true;
                        myTable.updateRowData(response[i])
                    }

                    var keys = myTable.rows.getKeys();
                    for (var i = 0; i < keys.length; i++) {
                        if (!torrents_hashes.contains(keys[i]))
                            myTable.rows.erase(keys[i]);
                    }

                    myTable.columns['priority'].force_hide = !queueing_enabled;
                    myTable.updateColumn('priority');
                    if (queueing_enabled) {
                        $('queueingButtons').removeClass('invisible');
                        $('queueingMenuItems').removeClass('invisible');
                    }
                    else {
                        $('queueingButtons').addClass('invisible');
                        $('queueingMenuItems').addClass('invisible');
                    }

                    myTable.updateTable(true);
                    myTable.altRow();
                }
                clearTimeout(loadTorrentsInfoTimer);
                loadTorrentsInfoTimer = loadTorrentsInfo.delay(1500);
            }
        }).send();
    };

    updateTransferList = function() {
        myTable.updateTable();
        clearTimeout(loadTorrentsInfoTimer);
        loadTorrentsInfoTimer = loadTorrentsInfo.delay(30);
    }

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
    }

    // Start fetching data now
    loadTransferInfo();

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
