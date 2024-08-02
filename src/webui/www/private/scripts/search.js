/*
 * MIT License
 * Copyright (C) 2024 Thomas Piccirello
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

"use strict";

if (window.qBittorrent === undefined)
    window.qBittorrent = {};

window.qBittorrent.Search = (function() {
    const exports = function() {
        return {
            startStopSearch: startStopSearch,
            manageSearchPlugins: manageSearchPlugins,
            searchPlugins: searchPlugins,
            searchText: searchText,
            searchSeedsFilter: searchSeedsFilter,
            searchSizeFilter: searchSizeFilter,
            init: init,
            getPlugin: getPlugin,
            searchInTorrentName: searchInTorrentName,
            onSearchPatternChanged: onSearchPatternChanged,
            categorySelected: categorySelected,
            pluginSelected: pluginSelected,
            searchSeedsFilterChanged: searchSeedsFilterChanged,
            searchSizeFilterChanged: searchSizeFilterChanged,
            searchSizeFilterPrefixChanged: searchSizeFilterPrefixChanged,
            closeSearchTab: closeSearchTab,
        };
    };

    const searchTabIdPrefix = "Search-";
    let loadSearchPluginsTimer;
    const searchPlugins = [];
    let prevSearchPluginsResponse;
    let selectedCategory = "QBT_TR(All categories)QBT_TR[CONTEXT=SearchEngineWidget]";
    let selectedPlugin = "enabled";
    let prevSelectedPlugin;
    // whether the current search pattern differs from the pattern that the active search was performed with
    let searchPatternChanged = false;

    let searchResultsTable;
    /** @type Map<number, {
     * searchPattern: string,
     * filterPattern: string,
     * seedsFilter: {min: number, max: number},
     * sizeFilter: {min: number, minUnit: number, max: number, maxUnit: number},
     * searchIn: string,
     * rows: [],
     * rowId: number,
     * selectedRowIds: number[],
     * running: boolean,
     * loadResultsTimer: Timer,
     * sort: {column: string, reverse: string},
     * }> **/
    const searchState = new Map();
    const searchText = {
        pattern: "",
        filterPattern: ""
    };
    const searchSeedsFilter = {
        min: 0,
        max: 0
    };
    const searchSizeFilter = {
        min: 0.00,
        minUnit: 2, // B = 0, KiB = 1, MiB = 2, GiB = 3, TiB = 4, PiB = 5, EiB = 6
        max: 0.00,
        maxUnit: 3
    };

    const init = function() {
        // load "Search in" preference from local storage
        $("searchInTorrentName").set("value", (LocalPreferences.get("search_in_filter") === "names") ? "names" : "everywhere");
        const searchResultsTableContextMenu = new window.qBittorrent.ContextMenu.ContextMenu({
            targets: ".searchTableRow",
            menu: "searchResultsTableMenu",
            actions: {
                Download: downloadSearchTorrent,
                OpenDescriptionUrl: openSearchTorrentDescriptionUrl
            },
            offsets: {
                x: -15,
                y: -53
            }
        });
        searchResultsTable = new window.qBittorrent.DynamicTable.SearchResultsTable();
        searchResultsTable.setup("searchResultsTableDiv", "searchResultsTableFixedHeaderDiv", searchResultsTableContextMenu);
        getPlugins();

        // listen for changes to searchInNameFilter
        let searchInNameFilterTimer = -1;
        $("searchInNameFilter").addEvent("input", () => {
            clearTimeout(searchInNameFilterTimer);
            searchInNameFilterTimer = setTimeout(() => {
                searchInNameFilterTimer = -1;

                const value = $("searchInNameFilter").get("value");
                searchText.filterPattern = value;
                searchFilterChanged();
            }, window.qBittorrent.Misc.FILTER_INPUT_DELAY);
        });

        new Keyboard({
            defaultEventType: "keydown",
            events: {
                "Enter": function(e) {
                    // accept enter key as a click
                    new Event(e).stop();

                    const elem = e.event.srcElement;
                    if (elem.className.contains("searchInputField")) {
                        $("startSearchButton").click();
                        return;
                    }

                    switch (elem.id) {
                        case "manageSearchPlugins":
                            manageSearchPlugins();
                            break;
                    }
                }
            }
        }).activate();

        // restore search tabs
        const searchJobs = JSON.parse(LocalPreferences.get("search_jobs", "[]"));
        for (const { id, pattern } of searchJobs)
            createSearchTab(id, pattern);
    };

    const numSearchTabs = function() {
        return $("searchTabs").getElements("li").length;
    };

    const getSearchIdFromTab = function(tab) {
        return Number(tab.id.substring(searchTabIdPrefix.length));
    };

    const createSearchTab = function(searchId, pattern) {
        const newTabId = `${searchTabIdPrefix}${searchId}`;
        const tabElem = new Element("a", {
            text: pattern,
        });
        const closeTabElem = new Element("img", {
            alt: "QBT_TR(Close tab)QBT_TR[CONTEXT=SearchWidget]",
            title: "QBT_TR(Close tab)QBT_TR[CONTEXT=SearchWidget]",
            src: "images/application-exit.svg",
            width: "8",
            height: "8",
            style: "padding-right: 7px; margin-bottom: -1px; margin-left: -5px",
            onclick: "qBittorrent.Search.closeSearchTab(this)",
        });
        closeTabElem.inject(tabElem, "top");
        tabElem.appendChild(getStatusIconElement("QBT_TR(Searching...)QBT_TR[CONTEXT=SearchJobWidget]", "images/queued.svg"));
        $("searchTabs").appendChild(new Element("li", {
            id: newTabId,
            class: "selected",
            html: tabElem.outerHTML,
        }));

        // unhide the results elements
        if (numSearchTabs() >= 1) {
            $("searchResultsNoSearches").style.display = "none";
            $("searchResultsFilters").style.display = "block";
            $("searchResultsTableContainer").style.display = "block";
            $("searchTabsToolbar").style.display = "block";
        }

        // reinitialize tabs
        $("searchTabs").getElements("li").removeEvents("click");
        $("searchTabs").getElements("li").addEvent("click", function(e) {
            $("startSearchButton").set("text", "QBT_TR(Search)QBT_TR[CONTEXT=SearchEngineWidget]");
            setActiveTab(this);
        });

        // select new tab
        setActiveTab($(newTabId));

        searchResultsTable.clear();
        resetFilters();

        searchState.set(searchId, {
            searchPattern: pattern,
            filterPattern: searchText.filterPattern,
            seedsFilter: { min: searchSeedsFilter.min, max: searchSeedsFilter.max },
            sizeFilter: { min: searchSizeFilter.min, minUnit: searchSizeFilter.minUnit, max: searchSizeFilter.max, maxUnit: searchSizeFilter.maxUnit },
            searchIn: getSearchInTorrentName(),
            rows: [],
            rowId: 0,
            selectedRowIds: [],
            running: true,
            loadResultsTimer: null,
            sort: { column: searchResultsTable.sortedColumn, reverse: searchResultsTable.reverseSort },
        });
        updateSearchResultsData(searchId);
    };

    const closeSearchTab = function(el) {
        const tab = el.parentElement.parentElement;
        const searchId = getSearchIdFromTab(tab);
        const isTabSelected = tab.hasClass("selected");
        const newTabToSelect = isTabSelected ? tab.nextSibling || tab.previousSibling : null;

        const currentSearchId = getSelectedSearchId();
        const state = searchState.get(currentSearchId);
        // don't bother sending a stop request if already stopped
        if (state && state.running)
            stopSearch(searchId);

        tab.destroy();

        new Request({
            url: new URI("api/v2/search/delete"),
            method: "post",
            data: {
                id: searchId
            },
        }).send();

        const searchJobs = JSON.parse(LocalPreferences.get("search_jobs", "[]"));
        const jobIndex = searchJobs.findIndex((job) => job.id === searchId);
        if (jobIndex >= 0) {
            searchJobs.splice(jobIndex, 1);
            LocalPreferences.set("search_jobs", JSON.stringify(searchJobs));
        }

        if (numSearchTabs() === 0) {
            resetSearchState();
            resetFilters();

            $("numSearchResultsVisible").set("html", 0);
            $("numSearchResultsTotal").set("html", 0);
            $("searchResultsNoSearches").style.display = "block";
            $("searchResultsFilters").style.display = "none";
            $("searchResultsTableContainer").style.display = "none";
            $("searchTabsToolbar").style.display = "none";
        }
        else if (isTabSelected && newTabToSelect) {
            setActiveTab(newTabToSelect);
            $("startSearchButton").set("text", "QBT_TR(Search)QBT_TR[CONTEXT=SearchEngineWidget]");
        }
    };

    const saveCurrentTabState = function() {
        const currentSearchId = getSelectedSearchId();
        if (!currentSearchId)
            return;

        const state = searchState.get(currentSearchId);
        if (!state)
            return;

        state.filterPattern = searchText.filterPattern;
        state.seedsFilter = {
            min: searchSeedsFilter.min,
            max: searchSeedsFilter.max,
        };
        state.sizeFilter = {
            min: searchSizeFilter.min,
            minUnit: searchSizeFilter.minUnit,
            max: searchSizeFilter.max,
            maxUnit: searchSizeFilter.maxUnit,
        };
        state.searchIn = getSearchInTorrentName();

        state.sort = {
            column: searchResultsTable.sortedColumn,
            reverse: searchResultsTable.reverseSort,
        };

        // we must copy the array to avoid taking a reference to it
        state.selectedRowIds = [...searchResultsTable.selectedRows];
    };

    const setActiveTab = function(tab) {
        const searchId = getSearchIdFromTab(tab);
        if (searchId === getSelectedSearchId())
            return;

        saveCurrentTabState();

        MochaUI.selected(tab, "searchTabs");

        const state = searchState.get(searchId);
        let rowsToSelect = [];

        // restore table rows
        searchResultsTable.clear();
        if (state) {
            for (const row of state.rows)
                searchResultsTable.updateRowData(row);

            rowsToSelect = state.selectedRowIds;

            // restore filters
            searchText.pattern = state.searchPattern;
            searchText.filterPattern = state.filterPattern;
            $("searchInNameFilter").set("value", state.filterPattern);

            searchSeedsFilter.min = state.seedsFilter.min;
            searchSeedsFilter.max = state.seedsFilter.max;
            $("searchMinSeedsFilter").set("value", state.seedsFilter.min);
            $("searchMaxSeedsFilter").set("value", state.seedsFilter.max);

            searchSizeFilter.min = state.sizeFilter.min;
            searchSizeFilter.minUnit = state.sizeFilter.minUnit;
            searchSizeFilter.max = state.sizeFilter.max;
            searchSizeFilter.maxUnit = state.sizeFilter.maxUnit;
            $("searchMinSizeFilter").set("value", state.sizeFilter.min);
            $("searchMinSizePrefix").set("value", state.sizeFilter.minUnit);
            $("searchMaxSizeFilter").set("value", state.sizeFilter.max);
            $("searchMaxSizePrefix").set("value", state.sizeFilter.maxUnit);

            const currentSearchPattern = $("searchPattern").getProperty("value").trim();
            if (state.running && (state.searchPattern === currentSearchPattern)) {
                // allow search to be stopped
                $("startSearchButton").set("text", "QBT_TR(Stop)QBT_TR[CONTEXT=SearchEngineWidget]");
                searchPatternChanged = false;
            }

            searchResultsTable.setSortedColumn(state.sort.column, state.sort.reverse);

            $("searchInTorrentName").set("value", state.searchIn);
        }

        // must restore all filters before calling updateTable
        searchResultsTable.updateTable();
        searchResultsTable.altRow();

        // must reselect rows after calling updateTable
        if (rowsToSelect.length > 0)
            searchResultsTable.reselectRows(rowsToSelect);

        $("numSearchResultsVisible").set("html", searchResultsTable.getFilteredAndSortedRows().length);
        $("numSearchResultsTotal").set("html", searchResultsTable.getRowIds().length);

        setupSearchTableEvents(true);
    };

    const getStatusIconElement = function(text, image) {
        return new Element("img", {
            alt: text,
            title: text,
            src: image,
            class: "statusIcon",
            width: "10",
            height: "10",
            style: "margin-bottom: -2px; margin-left: 7px",
        });
    };

    const updateStatusIconElement = function(searchId, text, image) {
        const searchTab = $(`${searchTabIdPrefix}${searchId}`);
        if (searchTab) {
            const statusIcon = searchTab.getElement(".statusIcon");
            statusIcon.set("alt", text);
            statusIcon.set("title", text);
            statusIcon.set("src", image);
        }
    };

    const startSearch = function(pattern, category, plugins) {
        searchPatternChanged = false;

        const url = new URI("api/v2/search/start");
        new Request.JSON({
            url: url,
            method: "post",
            data: {
                pattern: pattern,
                category: category,
                plugins: plugins
            },
            onSuccess: function(response) {
                $("startSearchButton").set("text", "QBT_TR(Stop)QBT_TR[CONTEXT=SearchEngineWidget]");
                const searchId = response.id;
                createSearchTab(searchId, pattern);

                const searchJobs = JSON.parse(LocalPreferences.get("search_jobs", "[]"));
                searchJobs.push({ id: searchId, pattern: pattern });
                LocalPreferences.set("search_jobs", JSON.stringify(searchJobs));
            }
        }).send();
    };

    const stopSearch = function(searchId) {
        const url = new URI("api/v2/search/stop");
        new Request({
            url: url,
            method: "post",
            data: {
                id: searchId
            },
            onSuccess: function(response) {
                resetSearchState(searchId);
                // not strictly necessary to do this when the tab is being closed, but there's no harm in it
                updateStatusIconElement(searchId, "QBT_TR(Search aborted)QBT_TR[CONTEXT=SearchJobWidget]", "images/task-reject.svg");
            }
        }).send();
    };

    const getSelectedSearchId = function() {
        const selectedTab = $("searchTabs").getElement("li.selected");
        return selectedTab ? getSearchIdFromTab(selectedTab) : null;
    };

    const startStopSearch = function() {
        const currentSearchId = getSelectedSearchId();
        const state = searchState.get(currentSearchId);
        const isSearchRunning = state && state.running;
        if (!isSearchRunning || searchPatternChanged) {
            const pattern = $("searchPattern").getProperty("value").trim();
            const category = $("categorySelect").getProperty("value");
            const plugins = $("pluginsSelect").getProperty("value");

            if (!pattern || !category || !plugins)
                return;

            searchText.pattern = pattern;
            startSearch(pattern, category, plugins);
        }
        else {
            stopSearch(currentSearchId);
        }
    };

    const openSearchTorrentDescriptionUrl = function() {
        searchResultsTable.selectedRowsIds().each((rowId) => {
            window.open(searchResultsTable.rows.get(rowId).full_data.descrLink, "_blank");
        });
    };

    const copySearchTorrentName = function() {
        const names = [];
        searchResultsTable.selectedRowsIds().each((rowId) => {
            names.push(searchResultsTable.rows.get(rowId).full_data.fileName);
        });
        return names.join("\n");
    };

    const copySearchTorrentDownloadLink = function() {
        const urls = [];
        searchResultsTable.selectedRowsIds().each((rowId) => {
            urls.push(searchResultsTable.rows.get(rowId).full_data.fileUrl);
        });
        return urls.join("\n");
    };

    const copySearchTorrentDescriptionUrl = function() {
        const urls = [];
        searchResultsTable.selectedRowsIds().each((rowId) => {
            urls.push(searchResultsTable.rows.get(rowId).full_data.descrLink);
        });
        return urls.join("\n");
    };

    const downloadSearchTorrent = function() {
        const urls = [];
        searchResultsTable.selectedRowsIds().each((rowId) => {
            urls.push(searchResultsTable.rows.get(rowId).full_data.fileUrl);
        });

        // only proceed if at least 1 row was selected
        if (!urls.length)
            return;

        showDownloadPage(urls);
    };

    const manageSearchPlugins = function() {
        const id = "searchPlugins";
        if (!$(id)) {
            new MochaUI.Window({
                id: id,
                title: "QBT_TR(Search plugins)QBT_TR[CONTEXT=PluginSelectDlg]",
                loadMethod: "xhr",
                contentURL: "views/searchplugins.html",
                scrollbars: false,
                maximizable: false,
                paddingVertical: 0,
                paddingHorizontal: 0,
                width: loadWindowWidth(id, 600),
                height: loadWindowHeight(id, 360),
                onResize: function() {
                    saveWindowSize(id);
                },
                onBeforeBuild: function() {
                    loadSearchPlugins();
                },
                onClose: function() {
                    clearTimeout(loadSearchPluginsTimer);
                }
            });
        }
    };

    const loadSearchPlugins = function() {
        getPlugins();
        loadSearchPluginsTimer = loadSearchPlugins.delay(2000);
    };

    const onSearchPatternChanged = function() {
        const currentSearchId = getSelectedSearchId();
        const state = searchState.get(currentSearchId);
        const currentSearchPattern = $("searchPattern").getProperty("value").trim();
        // start a new search if pattern has changed, otherwise allow the search to be stopped
        if (state && (state.searchPattern === currentSearchPattern)) {
            searchPatternChanged = false;
            $("startSearchButton").set("text", "QBT_TR(Stop)QBT_TR[CONTEXT=SearchEngineWidget]");
        }
        else {
            searchPatternChanged = true;
            $("startSearchButton").set("text", "QBT_TR(Search)QBT_TR[CONTEXT=SearchEngineWidget]");
        }
    };

    const categorySelected = function() {
        selectedCategory = $("categorySelect").get("value");
    };

    const pluginSelected = function() {
        selectedPlugin = $("pluginsSelect").get("value");

        if (selectedPlugin !== prevSelectedPlugin) {
            prevSelectedPlugin = selectedPlugin;
            getSearchCategories();
        }
    };

    const reselectCategory = function() {
        for (let i = 0; i < $("categorySelect").options.length; ++i) {
            if ($("categorySelect").options[i].get("value") === selectedCategory)
                $("categorySelect").options[i].selected = true;
        }

        categorySelected();
    };

    const reselectPlugin = function() {
        for (let i = 0; i < $("pluginsSelect").options.length; ++i) {
            if ($("pluginsSelect").options[i].get("value") === selectedPlugin)
                $("pluginsSelect").options[i].selected = true;
        }

        pluginSelected();
    };

    const resetSearchState = function(searchId) {
        $("startSearchButton").set("text", "QBT_TR(Search)QBT_TR[CONTEXT=SearchEngineWidget]");
        const state = searchState.get(searchId);
        if (state) {
            state.running = false;
            clearTimeout(state.loadResultsTimer);
        }
    };

    const getSearchCategories = function() {
        const populateCategorySelect = function(categories) {
            const categoryHtml = [];
            categories.each((category) => {
                const option = new Element("option");
                option.set("value", category.id);
                option.set("html", category.name);
                categoryHtml.push(option.outerHTML);
            });

            // first category is "All Categories"
            if (categoryHtml.length > 1) {
                // add separator
                const option = new Element("option");
                option.set("disabled", true);
                option.set("html", "──────────");
                categoryHtml.splice(1, 0, option.outerHTML);
            }

            $("categorySelect").set("html", categoryHtml.join(""));
        };

        const selectedPlugin = $("pluginsSelect").get("value");

        if ((selectedPlugin === "all") || (selectedPlugin === "enabled")) {
            const uniqueCategories = {};
            for (const plugin of searchPlugins) {
                if ((selectedPlugin === "enabled") && !plugin.enabled)
                    continue;
                for (const category of plugin.supportedCategories) {
                    if (uniqueCategories[category.id] === undefined)
                        uniqueCategories[category.id] = category;
                }
            }
            // we must sort the ids to maintain consistent order.
            const categories = Object.keys(uniqueCategories).sort().map(id => uniqueCategories[id]);
            populateCategorySelect(categories);
        }
        else {
            const plugin = getPlugin(selectedPlugin);
            const plugins = (plugin === null) ? [] : plugin.supportedCategories;
            populateCategorySelect(plugins);
        }

        reselectCategory();
    };

    const getPlugins = function() {
        new Request.JSON({
            url: new URI("api/v2/search/plugins"),
            method: "get",
            noCache: true,
            onSuccess: function(response) {
                if (response !== prevSearchPluginsResponse) {
                    prevSearchPluginsResponse = response;
                    searchPlugins.length = 0;
                    response.forEach((plugin) => {
                        searchPlugins.push(plugin);
                    });

                    const pluginsHtml = [];
                    pluginsHtml.push('<option value="enabled">QBT_TR(Only enabled)QBT_TR[CONTEXT=SearchEngineWidget]</option>');
                    pluginsHtml.push('<option value="all">QBT_TR(All plugins)QBT_TR[CONTEXT=SearchEngineWidget]</option>');

                    const searchPluginsEmpty = (searchPlugins.length === 0);
                    if (!searchPluginsEmpty) {
                        $("searchResultsNoPlugins").style.display = "none";
                        if (numSearchTabs() === 0)
                            $("searchResultsNoSearches").style.display = "block";

                        // sort plugins alphabetically
                        const allPlugins = searchPlugins.sort((left, right) => {
                            const leftName = left.fullName;
                            const rightName = right.fullName;
                            return window.qBittorrent.Misc.naturalSortCollator.compare(leftName, rightName);
                        });

                        allPlugins.each((plugin) => {
                            if (plugin.enabled === true)
                                pluginsHtml.push("<option value='" + window.qBittorrent.Misc.escapeHtml(plugin.name) + "'>" + window.qBittorrent.Misc.escapeHtml(plugin.fullName) + "</option>");
                        });

                        if (pluginsHtml.length > 2)
                            pluginsHtml.splice(2, 0, "<option disabled>──────────</option>");
                    }

                    $("pluginsSelect").set("html", pluginsHtml.join(""));

                    $("searchPattern").setProperty("disabled", searchPluginsEmpty);
                    $("categorySelect").setProperty("disabled", searchPluginsEmpty);
                    $("pluginsSelect").setProperty("disabled", searchPluginsEmpty);
                    $("startSearchButton").setProperty("disabled", searchPluginsEmpty);

                    if (window.qBittorrent.SearchPlugins !== undefined)
                        window.qBittorrent.SearchPlugins.updateTable();

                    reselectPlugin();
                }
            }
        }).send();
    };

    const getPlugin = function(name) {
        for (let i = 0; i < searchPlugins.length; ++i) {
            if (searchPlugins[i].name === name)
                return searchPlugins[i];
        }

        return null;
    };

    const resetFilters = function() {
        searchText.filterPattern = "";
        $("searchInNameFilter").set("value", "");

        searchSeedsFilter.min = 0;
        searchSeedsFilter.max = 0;
        $("searchMinSeedsFilter").set("value", searchSeedsFilter.min);
        $("searchMaxSeedsFilter").set("value", searchSeedsFilter.max);

        searchSizeFilter.min = 0.00;
        searchSizeFilter.minUnit = 2; // B = 0, KiB = 1, MiB = 2, GiB = 3, TiB = 4, PiB = 5, EiB = 6
        searchSizeFilter.max = 0.00;
        searchSizeFilter.maxUnit = 3;
        $("searchMinSizeFilter").set("value", searchSizeFilter.min);
        $("searchMinSizePrefix").set("value", searchSizeFilter.minUnit);
        $("searchMaxSizeFilter").set("value", searchSizeFilter.max);
        $("searchMaxSizePrefix").set("value", searchSizeFilter.maxUnit);
    };

    const getSearchInTorrentName = function() {
        return $("searchInTorrentName").get("value") === "names" ? "names" : "everywhere";
    };

    const searchInTorrentName = function() {
        LocalPreferences.set("search_in_filter", getSearchInTorrentName());
        searchFilterChanged();
    };

    const searchSeedsFilterChanged = function() {
        searchSeedsFilter.min = $("searchMinSeedsFilter").get("value");
        searchSeedsFilter.max = $("searchMaxSeedsFilter").get("value");

        searchFilterChanged();
    };

    const searchSizeFilterChanged = function() {
        searchSizeFilter.min = $("searchMinSizeFilter").get("value");
        searchSizeFilter.minUnit = $("searchMinSizePrefix").get("value");
        searchSizeFilter.max = $("searchMaxSizeFilter").get("value");
        searchSizeFilter.maxUnit = $("searchMaxSizePrefix").get("value");

        searchFilterChanged();
    };

    const searchSizeFilterPrefixChanged = function() {
        if ((Number($("searchMinSizeFilter").get("value")) !== 0) || (Number($("searchMaxSizeFilter").get("value")) !== 0))
            searchSizeFilterChanged();
    };

    const searchFilterChanged = function() {
        searchResultsTable.updateTable();
        $("numSearchResultsVisible").set("html", searchResultsTable.getFilteredAndSortedRows().length);
    };

    const setupSearchTableEvents = function(enable) {
        if (enable) {
            $$(".searchTableRow").each((target) => {
                target.addEventListener("dblclick", downloadSearchTorrent, false);
            });
        }
        else {
            $$(".searchTableRow").each((target) => {
                target.removeEventListener("dblclick", downloadSearchTorrent, false);
            });
        }
    };

    const loadSearchResultsData = function(searchId) {
        const state = searchState.get(searchId);

        const maxResults = 500;
        const url = new URI("api/v2/search/results");
        new Request.JSON({
            url: url,
            method: "get",
            noCache: true,
            data: {
                id: searchId,
                limit: maxResults,
                offset: state.rowId
            },
            onFailure: function(response) {
                if ((response.status === 400) || (response.status === 404)) {
                    // bad params. search id is invalid
                    resetSearchState(searchId);
                    updateStatusIconElement(searchId, "QBT_TR(An error occurred during search...)QBT_TR[CONTEXT=SearchJobWidget]", "images/error.svg");
                }
                else {
                    clearTimeout(state.loadResultsTimer);
                    state.loadResultsTimer = loadSearchResultsData.delay(3000, this, searchId);
                }
            },
            onSuccess: function(response) {
                $("error_div").set("html", "");

                const state = searchState.get(searchId);
                // check if user stopped the search prior to receiving the response
                if (!state.running) {
                    clearTimeout(state.loadResultsTimer);
                    updateStatusIconElement(searchId, "QBT_TR(Search aborted)QBT_TR[CONTEXT=SearchJobWidget]", "images/task-reject.svg");
                    return;
                }

                if (response) {
                    setupSearchTableEvents(false);

                    const state = searchState.get(searchId);
                    const newRows = [];

                    if (response.results) {
                        const results = response.results;
                        for (let i = 0; i < results.length; ++i) {
                            const result = results[i];
                            const row = {
                                rowId: state.rowId,
                                descrLink: result.descrLink,
                                fileName: result.fileName,
                                fileSize: result.fileSize,
                                fileUrl: result.fileUrl,
                                nbLeechers: result.nbLeechers,
                                nbSeeders: result.nbSeeders,
                                siteUrl: result.siteUrl,
                                pubDate: result.pubDate,
                            };

                            newRows.push(row);
                            state.rows.push(row);
                            state.rowId += 1;
                        }
                    }

                    // only update table if this search is currently being displayed
                    if (searchId === getSelectedSearchId()) {
                        for (const row of newRows)
                            searchResultsTable.updateRowData(row);

                        $("numSearchResultsVisible").set("html", searchResultsTable.getFilteredAndSortedRows().length);
                        $("numSearchResultsTotal").set("html", searchResultsTable.getRowIds().length);

                        searchResultsTable.updateTable();
                        searchResultsTable.altRow();
                    }

                    setupSearchTableEvents(true);

                    if ((response.status === "Stopped") && (state.rowId >= response.total)) {
                        resetSearchState(searchId);
                        updateStatusIconElement(searchId, "QBT_TR(Search has finished)QBT_TR[CONTEXT=SearchJobWidget]", "images/task-complete.svg");
                        return;
                    }
                }

                clearTimeout(state.loadResultsTimer);
                state.loadResultsTimer = loadSearchResultsData.delay(2000, this, searchId);
            }
        }).send();
    };

    const updateSearchResultsData = function(searchId) {
        const state = searchState.get(searchId);
        clearTimeout(state.loadResultsTimer);
        state.loadResultsTimer = loadSearchResultsData.delay(500, this, searchId);
    };

    new ClipboardJS(".copySearchDataToClipboard", {
        text: function(trigger) {
            switch (trigger.id) {
                case "copySearchTorrentName":
                    return copySearchTorrentName();
                case "copySearchTorrentDownloadLink":
                    return copySearchTorrentDownloadLink();
                case "copySearchTorrentDescriptionUrl":
                    return copySearchTorrentDescriptionUrl();
                default:
                    return "";
            }
        }
    });

    return exports();
})();

Object.freeze(window.qBittorrent.Search);
