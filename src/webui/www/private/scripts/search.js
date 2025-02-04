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

window.qBittorrent ??= {};
window.qBittorrent.Search ??= (() => {
    const exports = () => {
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
    let loadSearchPluginsTimer = -1;
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

    const searchResultsTabsContextMenu = new window.qBittorrent.ContextMenu.ContextMenu({
        targets: ".searchTab",
        menu: "searchResultsTabsMenu",
        actions: {
            closeTab: (tab) => { closeSearchTab(tab); },
            closeAllTabs: () => {
                for (const tab of document.querySelectorAll("#searchTabs .searchTab"))
                    closeSearchTab(tab);
            }
        },
        offsets: {
            x: -15,
            y: -53
        },
        onShow: function() {
            setActiveTab(this.options.element);
        }
    });

    const init = () => {
        // load "Search in" preference from local storage
        $("searchInTorrentName").value = (LocalPreferences.get("search_in_filter") === "names") ? "names" : "everywhere";
        const searchResultsTableContextMenu = new window.qBittorrent.ContextMenu.ContextMenu({
            targets: "#searchResultsTableDiv tr",
            menu: "searchResultsTableMenu",
            actions: {
                Download: downloadSearchTorrent,
                OpenDescriptionUrl: openSearchTorrentDescriptionUrl
            },
            offsets: {
                x: 0,
                y: -60
            }
        });
        searchResultsTable = new window.qBittorrent.DynamicTable.SearchResultsTable();
        searchResultsTable.setup("searchResultsTableDiv", "searchResultsTableFixedHeaderDiv", searchResultsTableContextMenu);
        getPlugins();

        searchResultsTable.dynamicTableDiv.addEventListener("dblclick", (e) => { downloadSearchTorrent(); });

        // listen for changes to searchInNameFilter
        let searchInNameFilterTimer = -1;
        $("searchInNameFilter").addEventListener("input", () => {
            clearTimeout(searchInNameFilterTimer);
            searchInNameFilterTimer = setTimeout(() => {
                searchInNameFilterTimer = -1;

                const value = $("searchInNameFilter").value;
                searchText.filterPattern = value;
                searchFilterChanged();
            }, window.qBittorrent.Misc.FILTER_INPUT_DELAY);
        });

        document.getElementById("SearchPanel").addEventListener("keydown", (event) => {
            switch (event.key) {
                case "Enter": {
                    event.preventDefault();
                    event.stopPropagation();

                    switch (event.target.id) {
                        case "manageSearchPlugins":
                            manageSearchPlugins();
                            break;
                        case "searchPattern":
                            document.getElementById("startSearchButton").click();
                            break;
                    }

                    break;
                }
            }
        });

        // restore search tabs
        const searchJobs = JSON.parse(LocalPreferences.get("search_jobs", "[]"));
        for (const { id, pattern } of searchJobs)
            createSearchTab(id, pattern);
    };

    const numSearchTabs = () => {
        return document.querySelectorAll("#searchTabs li").length;
    };

    const getSearchIdFromTab = (tab) => {
        return Number(tab.id.substring(searchTabIdPrefix.length));
    };

    const createSearchTab = (searchId, pattern) => {
        const newTabId = `${searchTabIdPrefix}${searchId}`;
        const tabElem = document.createElement("a");
        tabElem.textContent = pattern;

        const closeTabElem = document.createElement("img");
        closeTabElem.alt = "QBT_TR(Close tab)QBT_TR[CONTEXT=SearchWidget]";
        closeTabElem.title = "QBT_TR(Close tab)QBT_TR[CONTEXT=SearchWidget]";
        closeTabElem.src = "images/application-exit.svg";
        closeTabElem.width = "10";
        closeTabElem.height = "10";
        closeTabElem.addEventListener("click", function(e) { qBittorrent.Search.closeSearchTab(this); });

        tabElem.prepend(closeTabElem);
        tabElem.appendChild(getStatusIconElement("QBT_TR(Searching...)QBT_TR[CONTEXT=SearchJobWidget]", "images/queued.svg"));

        const listItem = document.createElement("li");
        listItem.id = newTabId;
        listItem.classList.add("selected", "searchTab");
        listItem.addEventListener("click", (e) => {
            setActiveTab(listItem);
            document.getElementById("startSearchButton").lastChild.textContent = "QBT_TR(Search)QBT_TR[CONTEXT=SearchEngineWidget]";
        });
        listItem.appendChild(tabElem);
        $("searchTabs").appendChild(listItem);
        searchResultsTabsContextMenu.addTarget(listItem);

        // unhide the results elements
        if (numSearchTabs() >= 1) {
            $("searchResultsNoSearches").classList.add("invisible");
            $("searchResultsFilters").classList.remove("invisible");
            $("searchResultsTableContainer").classList.remove("invisible");
            $("searchTabsToolbar").classList.remove("invisible");
        }

        // select new tab
        setActiveTab(listItem);

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
            loadResultsTimer: -1,
            sort: { column: searchResultsTable.sortedColumn, reverse: searchResultsTable.reverseSort },
        });
        updateSearchResultsData(searchId);
    };

    const closeSearchTab = (el) => {
        const tab = el.closest("li.searchTab");
        if (!tab)
            return;

        const searchId = getSearchIdFromTab(tab);
        const isTabSelected = tab.classList.contains("selected");
        const newTabToSelect = isTabSelected ? (tab.nextSibling || tab.previousSibling) : null;

        const currentSearchId = getSelectedSearchId();
        const state = searchState.get(currentSearchId);
        // don't bother sending a stop request if already stopped
        if (state && state.running)
            stopSearch(searchId);

        tab.destroy();

        fetch("api/v2/search/delete", {
            method: "POST",
            body: new URLSearchParams({
                id: searchId
            })
        });

        const searchJobs = JSON.parse(LocalPreferences.get("search_jobs", "[]"));
        const jobIndex = searchJobs.findIndex((job) => job.id === searchId);
        if (jobIndex >= 0) {
            searchJobs.splice(jobIndex, 1);
            LocalPreferences.set("search_jobs", JSON.stringify(searchJobs));
        }

        if (numSearchTabs() === 0) {
            resetSearchState();
            resetFilters();

            $("numSearchResultsVisible").textContent = 0;
            $("numSearchResultsTotal").textContent = 0;
            $("searchResultsNoSearches").classList.remove("invisible");
            $("searchResultsFilters").classList.add("invisible");
            $("searchResultsTableContainer").classList.add("invisible");
            $("searchTabsToolbar").classList.add("invisible");
        }
        else if (isTabSelected && newTabToSelect) {
            setActiveTab(newTabToSelect);
            document.getElementById("startSearchButton").lastChild.textContent = "QBT_TR(Search)QBT_TR[CONTEXT=SearchEngineWidget]";
        }
    };

    const saveCurrentTabState = () => {
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

    const setActiveTab = (tab) => {
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
            $("searchInNameFilter").value = state.filterPattern;

            searchSeedsFilter.min = state.seedsFilter.min;
            searchSeedsFilter.max = state.seedsFilter.max;
            $("searchMinSeedsFilter").value = state.seedsFilter.min;
            $("searchMaxSeedsFilter").value = state.seedsFilter.max;

            searchSizeFilter.min = state.sizeFilter.min;
            searchSizeFilter.minUnit = state.sizeFilter.minUnit;
            searchSizeFilter.max = state.sizeFilter.max;
            searchSizeFilter.maxUnit = state.sizeFilter.maxUnit;
            $("searchMinSizeFilter").value = state.sizeFilter.min;
            $("searchMinSizePrefix").value = state.sizeFilter.minUnit;
            $("searchMaxSizeFilter").value = state.sizeFilter.max;
            $("searchMaxSizePrefix").value = state.sizeFilter.maxUnit;

            const currentSearchPattern = $("searchPattern").value.trim();
            if (state.running && (state.searchPattern === currentSearchPattern)) {
                // allow search to be stopped
                document.getElementById("startSearchButton").lastChild.textContent = "QBT_TR(Stop)QBT_TR[CONTEXT=SearchEngineWidget]";
                searchPatternChanged = false;
            }

            searchResultsTable.setSortedColumn(state.sort.column, state.sort.reverse);

            $("searchInTorrentName").value = state.searchIn;
        }

        // must restore all filters before calling updateTable
        searchResultsTable.updateTable();

        // must reselect rows after calling updateTable
        if (rowsToSelect.length > 0)
            searchResultsTable.reselectRows(rowsToSelect);

        $("numSearchResultsVisible").textContent = searchResultsTable.getFilteredAndSortedRows().length;
        $("numSearchResultsTotal").textContent = searchResultsTable.getRowSize();
    };

    const getStatusIconElement = (text, image) => {
        const statusIcon = document.createElement("img");
        statusIcon.alt = text;
        statusIcon.title = text;
        statusIcon.src = image;
        statusIcon.className = "statusIcon";
        statusIcon.width = "12";
        statusIcon.height = "12";
        return statusIcon;
    };

    const updateStatusIconElement = (searchId, text, image) => {
        const searchTab = $(`${searchTabIdPrefix}${searchId}`);
        if (searchTab) {
            const statusIcon = searchTab.querySelector(".statusIcon");
            statusIcon.alt = text;
            statusIcon.title = text;
            statusIcon.src = image;
        }
    };

    const startSearch = (pattern, category, plugins) => {
        searchPatternChanged = false;
        fetch("api/v2/search/start", {
                method: "POST",
                body: new URLSearchParams({
                    pattern: pattern,
                    category: category,
                    plugins: plugins
                })
            })
            .then(async (response) => {
                if (!response.ok)
                    return;

                const responseJSON = await response.json();

                document.getElementById("startSearchButton").lastChild.textContent = "QBT_TR(Stop)QBT_TR[CONTEXT=SearchEngineWidget]";
                const searchId = responseJSON.id;
                createSearchTab(searchId, pattern);

                const searchJobs = JSON.parse(LocalPreferences.get("search_jobs", "[]"));
                searchJobs.push({ id: searchId, pattern: pattern });
                LocalPreferences.set("search_jobs", JSON.stringify(searchJobs));
            });
    };

    const stopSearch = (searchId) => {
        fetch("api/v2/search/stop", {
                method: "POST",
                body: new URLSearchParams({
                    id: searchId
                })
            })
            .then((response) => {
                if (!response.ok)
                    return;

                resetSearchState(searchId);
                // not strictly necessary to do this when the tab is being closed, but there's no harm in it
                updateStatusIconElement(searchId, "QBT_TR(Search aborted)QBT_TR[CONTEXT=SearchJobWidget]", "images/task-reject.svg");
            });
    };

    const getSelectedSearchId = () => {
        const selectedTab = $("searchTabs").querySelector("li.selected");
        return selectedTab ? getSearchIdFromTab(selectedTab) : null;
    };

    const startStopSearch = () => {
        const currentSearchId = getSelectedSearchId();
        const state = searchState.get(currentSearchId);
        const isSearchRunning = state && state.running;
        if (!isSearchRunning || searchPatternChanged) {
            const pattern = $("searchPattern").value.trim();
            const category = $("categorySelect").value;
            const plugins = $("pluginsSelect").value;

            if (!pattern || !category || !plugins)
                return;

            searchText.pattern = pattern;
            startSearch(pattern, category, plugins);
        }
        else {
            stopSearch(currentSearchId);
        }
    };

    const openSearchTorrentDescriptionUrl = () => {
        for (const rowID of searchResultsTable.selectedRowsIds())
            window.open(searchResultsTable.getRow(rowID).full_data.descrLink, "_blank");
    };

    const copySearchTorrentName = () => {
        const names = [];
        searchResultsTable.selectedRowsIds().each((rowId) => {
            names.push(searchResultsTable.getRow(rowId).full_data.fileName);
        });
        return names.join("\n");
    };

    const copySearchTorrentDownloadLink = () => {
        const urls = [];
        searchResultsTable.selectedRowsIds().each((rowId) => {
            urls.push(searchResultsTable.getRow(rowId).full_data.fileUrl);
        });
        return urls.join("\n");
    };

    const copySearchTorrentDescriptionUrl = () => {
        const urls = [];
        searchResultsTable.selectedRowsIds().each((rowId) => {
            urls.push(searchResultsTable.getRow(rowId).full_data.descrLink);
        });
        return urls.join("\n");
    };

    const downloadSearchTorrent = () => {
        const urls = [];
        for (const rowID of searchResultsTable.selectedRowsIds())
            urls.push(searchResultsTable.getRow(rowID).full_data.fileUrl);

        // only proceed if at least 1 row was selected
        if (!urls.length)
            return;

        showDownloadPage(urls);
    };

    const manageSearchPlugins = () => {
        const id = "searchPlugins";
        if (!$(id)) {
            new MochaUI.Window({
                id: id,
                title: "QBT_TR(Search plugins)QBT_TR[CONTEXT=PluginSelectDlg]",
                icon: "images/qbittorrent-tray.svg",
                loadMethod: "xhr",
                contentURL: "views/searchplugins.html",
                scrollbars: false,
                maximizable: false,
                paddingVertical: 0,
                paddingHorizontal: 0,
                width: loadWindowWidth(id, 600),
                height: loadWindowHeight(id, 360),
                onResize: window.qBittorrent.Misc.createDebounceHandler(500, (e) => {
                    saveWindowSize(id);
                }),
                onBeforeBuild: () => {
                    loadSearchPlugins();
                },
                onClose: () => {
                    clearTimeout(loadSearchPluginsTimer);
                    loadSearchPluginsTimer = -1;
                }
            });
        }
    };

    const loadSearchPlugins = () => {
        getPlugins();
        loadSearchPluginsTimer = loadSearchPlugins.delay(2000);
    };

    const onSearchPatternChanged = () => {
        const currentSearchId = getSelectedSearchId();
        const state = searchState.get(currentSearchId);
        const currentSearchPattern = $("searchPattern").value.trim();
        // start a new search if pattern has changed, otherwise allow the search to be stopped
        if (state && (state.searchPattern === currentSearchPattern)) {
            searchPatternChanged = false;
            document.getElementById("startSearchButton").lastChild.textContent = "QBT_TR(Stop)QBT_TR[CONTEXT=SearchEngineWidget]";
        }
        else {
            searchPatternChanged = true;
            document.getElementById("startSearchButton").lastChild.textContent = "QBT_TR(Search)QBT_TR[CONTEXT=SearchEngineWidget]";
        }
    };

    const categorySelected = () => {
        selectedCategory = $("categorySelect").value;
    };

    const pluginSelected = () => {
        selectedPlugin = $("pluginsSelect").value;

        if (selectedPlugin !== prevSelectedPlugin) {
            prevSelectedPlugin = selectedPlugin;
            getSearchCategories();
        }
    };

    const reselectCategory = () => {
        for (let i = 0; i < $("categorySelect").options.length; ++i) {
            if ($("categorySelect").options[i].get("value") === selectedCategory)
                $("categorySelect").options[i].selected = true;
        }

        categorySelected();
    };

    const reselectPlugin = () => {
        for (let i = 0; i < $("pluginsSelect").options.length; ++i) {
            if ($("pluginsSelect").options[i].get("value") === selectedPlugin)
                $("pluginsSelect").options[i].selected = true;
        }

        pluginSelected();
    };

    const resetSearchState = (searchId) => {
        document.getElementById("startSearchButton").lastChild.textContent = "QBT_TR(Search)QBT_TR[CONTEXT=SearchEngineWidget]";
        const state = searchState.get(searchId);
        if (state) {
            state.running = false;
            clearTimeout(state.loadResultsTimer);
            state.loadResultsTimer = -1;
        }
    };

    const getSearchCategories = () => {
        const populateCategorySelect = (categories) => {
            const categoryOptions = [];

            for (const category of categories) {
                const option = document.createElement("option");
                option.value = category.id;
                option.textContent = category.name;
                categoryOptions.push(option);
            };

            // first category is "All Categories"
            if (categoryOptions.length > 1) {
                // add separator
                const option = document.createElement("option");
                option.disabled = true;
                option.textContent = "──────────";
                categoryOptions.splice(1, 0, option);
            }

            $("categorySelect").replaceChildren(...categoryOptions);
        };

        const selectedPlugin = $("pluginsSelect").value;

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

    const getPlugins = () => {
        fetch("api/v2/search/plugins", {
                method: "GET",
                cache: "no-store"
            })
            .then(async (response) => {
                if (!response.ok)
                    return;

                const responseJSON = await response.json();

                const createOption = (text, value, disabled = false) => {
                    const option = document.createElement("option");
                    if (value !== undefined)
                        option.value = value;
                    option.textContent = text;
                    option.disabled = disabled;
                    return option;
                };

                if (prevSearchPluginsResponse !== responseJSON) {
                    prevSearchPluginsResponse = responseJSON;
                    searchPlugins.length = 0;
                    responseJSON.forEach((plugin) => {
                        searchPlugins.push(plugin);
                    });

                    const pluginOptions = [];
                    pluginOptions.push(createOption("QBT_TR(Only enabled)QBT_TR[CONTEXT=SearchEngineWidget]", "enabled"));
                    pluginOptions.push(createOption("QBT_TR(All plugins)QBT_TR[CONTEXT=SearchEngineWidget]", "all"));

                    const searchPluginsEmpty = (searchPlugins.length === 0);
                    if (!searchPluginsEmpty) {
                        $("searchResultsNoPlugins").classList.add("invisible");
                        if (numSearchTabs() === 0)
                            $("searchResultsNoSearches").classList.remove("invisible");

                        // sort plugins alphabetically
                        const allPlugins = searchPlugins.sort((left, right) => {
                            const leftName = left.fullName;
                            const rightName = right.fullName;
                            return window.qBittorrent.Misc.naturalSortCollator.compare(leftName, rightName);
                        });

                        allPlugins.each((plugin) => {
                            if (plugin.enabled === true)
                                pluginOptions.push(createOption(plugin.fullName, plugin.name));
                        });

                        if (pluginOptions.length > 2)
                            pluginOptions.splice(2, 0, createOption("──────────", undefined, true));
                    }

                    $("pluginsSelect").replaceChildren(...pluginOptions);

                    $("searchPattern").disabled = searchPluginsEmpty;
                    $("categorySelect").disabled = searchPluginsEmpty;
                    $("pluginsSelect").disabled = searchPluginsEmpty;
                    document.getElementById("startSearchButton").disabled = searchPluginsEmpty;

                    if (window.qBittorrent.SearchPlugins !== undefined)
                        window.qBittorrent.SearchPlugins.updateTable();

                    reselectPlugin();
                }
            });
    };

    const getPlugin = (name) => {
        for (let i = 0; i < searchPlugins.length; ++i) {
            if (searchPlugins[i].name === name)
                return searchPlugins[i];
        }

        return null;
    };

    const resetFilters = () => {
        searchText.filterPattern = "";
        $("searchInNameFilter").value = "";

        searchSeedsFilter.min = 0;
        searchSeedsFilter.max = 0;
        $("searchMinSeedsFilter").value = searchSeedsFilter.min;
        $("searchMaxSeedsFilter").value = searchSeedsFilter.max;

        searchSizeFilter.min = 0.00;
        searchSizeFilter.minUnit = 2; // B = 0, KiB = 1, MiB = 2, GiB = 3, TiB = 4, PiB = 5, EiB = 6
        searchSizeFilter.max = 0.00;
        searchSizeFilter.maxUnit = 3;
        $("searchMinSizeFilter").value = searchSizeFilter.min;
        $("searchMinSizePrefix").value = searchSizeFilter.minUnit;
        $("searchMaxSizeFilter").value = searchSizeFilter.max;
        $("searchMaxSizePrefix").value = searchSizeFilter.maxUnit;
    };

    const getSearchInTorrentName = () => {
        return ($("searchInTorrentName").value === "names") ? "names" : "everywhere";
    };

    const searchInTorrentName = () => {
        LocalPreferences.set("search_in_filter", getSearchInTorrentName());
        searchFilterChanged();
    };

    const searchSeedsFilterChanged = () => {
        searchSeedsFilter.min = $("searchMinSeedsFilter").value;
        searchSeedsFilter.max = $("searchMaxSeedsFilter").value;

        searchFilterChanged();
    };

    const searchSizeFilterChanged = () => {
        searchSizeFilter.min = $("searchMinSizeFilter").value;
        searchSizeFilter.minUnit = $("searchMinSizePrefix").value;
        searchSizeFilter.max = $("searchMaxSizeFilter").value;
        searchSizeFilter.maxUnit = $("searchMaxSizePrefix").value;

        searchFilterChanged();
    };

    const searchSizeFilterPrefixChanged = () => {
        if ((Number($("searchMinSizeFilter").value) !== 0) || (Number($("searchMaxSizeFilter").value) !== 0))
            searchSizeFilterChanged();
    };

    const searchFilterChanged = () => {
        searchResultsTable.updateTable();
        $("numSearchResultsVisible").textContent = searchResultsTable.getFilteredAndSortedRows().length;
    };

    const loadSearchResultsData = function(searchId) {
        const state = searchState.get(searchId);
        const url = new URL("api/v2/search/results", window.location);
        url.search = new URLSearchParams({
            id: searchId,
            limit: 500,
            offset: state.rowId
        });
        fetch(url, {
                method: "GET",
                cache: "no-store"
            })
            .then(async (response) => {
                if (!response.ok) {
                    if ((response.status === 400) || (response.status === 404)) {
                        // bad params. search id is invalid
                        resetSearchState(searchId);
                        updateStatusIconElement(searchId, "QBT_TR(An error occurred during search...)QBT_TR[CONTEXT=SearchJobWidget]", "images/error.svg");
                    }
                    else {
                        clearTimeout(state.loadResultsTimer);
                        state.loadResultsTimer = loadSearchResultsData.delay(3000, this, searchId);
                    }
                    return;
                }

                $("error_div").textContent = "";

                const state = searchState.get(searchId);
                // check if user stopped the search prior to receiving the response
                if (!state.running) {
                    clearTimeout(state.loadResultsTimer);
                    updateStatusIconElement(searchId, "QBT_TR(Search aborted)QBT_TR[CONTEXT=SearchJobWidget]", "images/task-reject.svg");
                    return;
                }

                const responseJSON = await response.json();
                if (responseJSON) {
                    const state = searchState.get(searchId);
                    const newRows = [];

                    if (responseJSON.results) {
                        const results = responseJSON.results;
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
                                engineName: result.engineName,
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

                        $("numSearchResultsVisible").textContent = searchResultsTable.getFilteredAndSortedRows().length;
                        $("numSearchResultsTotal").textContent = searchResultsTable.getRowSize();

                        searchResultsTable.updateTable();
                    }

                    if ((responseJSON.status === "Stopped") && (state.rowId >= responseJSON.total)) {
                        resetSearchState(searchId);
                        updateStatusIconElement(searchId, "QBT_TR(Search has finished)QBT_TR[CONTEXT=SearchJobWidget]", "images/task-complete.svg");
                        return;
                    }
                }

                clearTimeout(state.loadResultsTimer);
                state.loadResultsTimer = loadSearchResultsData.delay(2000, this, searchId);
            });
    };

    const updateSearchResultsData = function(searchId) {
        const state = searchState.get(searchId);
        clearTimeout(state.loadResultsTimer);
        state.loadResultsTimer = loadSearchResultsData.delay(500, this, searchId);
    };

    new ClipboardJS(".copySearchDataToClipboard", {
        text: (trigger) => {
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
