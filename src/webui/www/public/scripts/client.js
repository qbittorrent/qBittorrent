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
var updateMainData = function(){};
var alternativeSpeedLimits = false;
var queueing_enabled = true;
var syncMainDataTimerPeriod = 1500;

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

    setFilter = function (f) {
        // Visually Select the right filter
        $("all_filter").removeClass("selectedFilter");
        $("downloading_filter").removeClass("selectedFilter");
        $("seeding_filter").removeClass("selectedFilter");
        $("completed_filter").removeClass("selectedFilter");
        $("paused_filter").removeClass("selectedFilter");
        $("resumed_filter").removeClass("selectedFilter");
        $("active_filter").removeClass("selectedFilter");
        $("inactive_filter").removeClass("selectedFilter");
        $(f + "_filter").addClass("selectedFilter");
        selected_filter = f;
        localStorage.setItem('selected_filter', f);
        // Reload torrents
        if (typeof myTable.table != 'undefined')
            updateMainData();
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

    // Show Top Toolbar is enabled by default
    if (localStorage.getItem('show_top_toolbar') == null)
        var showTopToolbar = true;
    else
        var showTopToolbar = localStorage.getItem('show_top_toolbar') == "true";
    if (!showTopToolbar) {
        $('showTopToolbarLink').firstChild.style.opacity = '0';
        $('mochaToolbar').addClass('invisible');
    }

    var speedInTitle = localStorage.getItem('speed_in_browser_title_bar') == "true";
    if (!speedInTitle)
        $('speedInBrowserTitleBarLink').firstChild.style.opacity = '0';

    // After Show Top Toolbar
    MochaUI.Desktop.setDesktopSize();

    var syncMainDataLastResponseId = 0;
    var serverState = {};

    var syncMainDataTimer;
    var syncMainData = function () {
        var url = new URI('sync/maindata');
        url.setData('rid', syncMainDataLastResponseId);
        var request = new Request.JSON({
            url : url,
            noCache : true,
            method : 'get',
            onFailure : function () {
                $('error_div').set('html', 'QBT_TR(qBittorrent client is not reachable)QBT_TR');
                clearTimeout(syncMainDataTimer);
                syncMainDataTimer = syncMainData.delay(2000);
            },
            onSuccess : function (response) {
                $('error_div').set('html', '');
                if (response) {
                    var full_update = (response['full_update'] == true);
                    if (full_update)
                        myTable.rows.erase();
                    if (response['rid'])
                        syncMainDataLastResponseId = response['rid'];
                    if (response['torrents'])
                        for (var key in response['torrents']) {
                            response['torrents'][key]['hash'] = key;
                            myTable.updateRowData(response['torrents'][key]);
                        }
                    myTable.updateTable(full_update);
                    if (response['torrents_removed'])
                        response['torrents_removed'].each(function (hash) {
                            myTable.removeRow(hash);
                        });
                    myTable.altRow();
                    if (response['server_state']) {
                        var tmp = response['server_state'];
                        for(var key in tmp)
                            serverState[key] = tmp[key];
                        processServerState();
                    }
                }
                clearTimeout(syncMainDataTimer);
                syncMainDataTimer = syncMainData.delay(syncMainDataTimerPeriod);
            }
        }).send();
    };

    updateMainData = function() {
        myTable.updateTable();
        clearTimeout(syncMainDataTimer);
        syncMainDataTimer = syncMainData.delay(100);
    }

    var processServerState = function () {
        var transfer_info = "";
        if (serverState.dl_rate_limit > 0)
            transfer_info += "[" + friendlyUnit(serverState.dl_rate_limit, true) + "] ";
        transfer_info += friendlyUnit(serverState.dl_info_speed, true);
        transfer_info += " (" + friendlyUnit(serverState.dl_info_data, false) + ")"
        $("DlInfos").set('html', transfer_info);
        transfer_info = "";
        if (serverState.up_rate_limit > 0)
            transfer_info += "[" + friendlyUnit(serverState.up_rate_limit, true) + "] ";
        transfer_info += friendlyUnit(serverState.up_info_speed, true)
        transfer_info += " (" + friendlyUnit(serverState.up_info_data, false) + ")"
        $("UpInfos").set('html', transfer_info);
        if (speedInTitle) {
            document.title = "QBT_TR([D:%1 U:%2])QBT_TR".replace("%1", friendlyUnit(serverState.dl_info_speed, true)).replace("%2", friendlyUnit(serverState.up_info_speed, true));
            document.title += " qBittorrent ${VERSION} QBT_TR(Web UI)QBT_TR";
        }else
            document.title = "qBittorrent ${VERSION} QBT_TR(Web UI)QBT_TR";
        $('DHTNodes').set('html', 'QBT_TR(DHT: %1 nodes)QBT_TR'.replace("%1", serverState.dht_nodes));
        if (serverState.connection_status == "connected")
            $('connectionStatus').src = 'images/skin/connected.png';
        else if (serverState.connection_status == "firewalled")
            $('connectionStatus').src = 'images/skin/firewalled.png';
        else
            $('connectionStatus').src = 'images/skin/disconnected.png';

        if (queueing_enabled != serverState.queueing) {
            queueing_enabled = serverState.queueing;
            myTable.columns['priority'].force_hide = !queueing_enabled;
            myTable.updateColumn('priority');
            if (queueing_enabled) {
                $('queueingLinks').removeClass('invisible');
                $('queueingButtons').removeClass('invisible');
                $('queueingMenuItems').removeClass('invisible');
            }
            else {
                $('queueingLinks').addClass('invisible');
                $('queueingButtons').addClass('invisible');
                $('queueingMenuItems').addClass('invisible');
            }
        }

        if (alternativeSpeedLimits != serverState.use_alt_speed_limits) {
            alternativeSpeedLimits = serverState.use_alt_speed_limits;
            updateAltSpeedIcon(alternativeSpeedLimits);
        }

        syncMainDataTimerPeriod = serverState.refresh_interval;
        if (syncMainDataTimerPeriod < 500)
            syncMainDataTimerPeriod = 500;
    };

    var updateAltSpeedIcon = function(enabled) {
        if (enabled)
            $('alternativeSpeedLimits').src = "images/slow.png";
        else
            $('alternativeSpeedLimits').src = "images/slow_off.png"
    }

    $('alternativeSpeedLimits').addEvent('click', function() {
        // Change icon immediately to give some feedback
        updateAltSpeedIcon(!alternativeSpeedLimits);

        new Request({url: 'command/toggleAlternativeSpeedLimits',
                method: 'post',
                onComplete: function() {
                    alternativeSpeedLimits = !alternativeSpeedLimits;
                    updateMainData();
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
        updateMainData();
    };

    $('showTopToolbarLink').addEvent('click', function(e) {
        showTopToolbar = !showTopToolbar;
        localStorage.setItem('show_top_toolbar', showTopToolbar.toString());
        if (showTopToolbar) {
            $('showTopToolbarLink').firstChild.style.opacity = '1';
            $('mochaToolbar').removeClass('invisible');
        }
        else {
            $('showTopToolbarLink').firstChild.style.opacity = '0';
            $('mochaToolbar').addClass('invisible');
        }
        MochaUI.Desktop.setDesktopSize();
    });

    $('speedInBrowserTitleBarLink').addEvent('click', function(e) {
        speedInTitle = !speedInTitle;
        localStorage.setItem('speed_in_browser_title_bar', speedInTitle.toString());
        if (speedInTitle)
            $('speedInBrowserTitleBarLink').firstChild.style.opacity = '1';
        else
            $('speedInBrowserTitleBarLink').firstChild.style.opacity = '0';
        processServerState();
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
            updateMainData();
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
            js : ['scripts/prop-general.js', 'scripts/prop-trackers.js', 'scripts/prop-webseeds.js', 'scripts/prop-files.js'],
        },
        tabsURL : 'properties.html',
        tabsOnload : function() {
            MochaUI.initializeTabs('propertiesTabs');

            updatePropertiesPanel = function() {
                if (!$('prop_general').hasClass('invisible'))
                    updateTorrentData();
                else if (!$('prop_trackers').hasClass('invisible'))
                    updateTrackersData();
                else if (!$('prop_webseeds').hasClass('invisible'))
                    updateWebSeedsData();
                else if (!$('prop_files').hasClass('invisible'))
                    updateTorrentFilesData();
            }

            $('PropGeneralLink').addEvent('click', function(e){
                $('prop_general').removeClass("invisible");
                $('prop_trackers').addClass("invisible");
                $('prop_webseeds').addClass("invisible");
                $('prop_files').addClass("invisible");
                updatePropertiesPanel();
            });

            $('PropTrackersLink').addEvent('click', function(e){
                $('prop_trackers').removeClass("invisible");
                $('prop_general').addClass("invisible");
                $('prop_webseeds').addClass("invisible");
                $('prop_files').addClass("invisible");
                updatePropertiesPanel();
            });

            $('PropWebSeedsLink').addEvent('click', function(e){
                $('prop_webseeds').removeClass("invisible");
                $('prop_general').addClass("invisible");
                $('prop_trackers').addClass("invisible");
                $('prop_files').addClass("invisible");
                updatePropertiesPanel();
            });

            $('PropFilesLink').addEvent('click', function(e){
                $('prop_files').removeClass("invisible");
                $('prop_general').addClass("invisible");
                $('prop_trackers').addClass("invisible");
                $('prop_webseeds').addClass("invisible");
                updatePropertiesPanel();
            });

            $('propertiesPanel_collapseToggle').addEvent('click', function(e){
                updatePropertiesPanel();
            });
        },
        column : 'mainColumn',
        height : prop_h
    });
});

function closeWindows() {
    MochaUI.closeAll();
};

var keyboardEvents = new Keyboard({
    defaultEventType: 'keydown',
    events: {
        'ctrl+a': function(event) {
            myTable.selectAll();
            event.preventDefault();
        },
        'delete': function(event) {
            deleteFN();
            event.preventDefault();
        }
    }
});

keyboardEvents.activate();
