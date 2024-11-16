/*
 * MIT License
 * Copyright (C) 2024  Mike Tzou (Chocobo1)
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

"use strict";

window.qBittorrent ??= {};
window.qBittorrent.Client ??= (() => {
    const exports = () => {
        return {
            setup: setup,
            closeWindow: closeWindow,
            closeFrameWindow: closeFrameWindow,
            getSyncMainDataInterval: getSyncMainDataInterval,
            isStopped: isStopped,
            stop: stop,
            mainTitle: mainTitle,
            showSearchEngine: showSearchEngine,
            showRssReader: showRssReader,
            showLogViewer: showLogViewer,
            isShowSearchEngine: isShowSearchEngine,
            isShowRssReader: isShowRssReader,
            isShowLogViewer: isShowLogViewer
        };
    };

    const setup = () => {
        // fetch various data and store it in memory
        window.qBittorrent.Cache.buildInfo.init();
        window.qBittorrent.Cache.preferences.init();
        window.qBittorrent.Cache.qbtVersion.init();
    };

    const closeWindow = (windowID) => {
        const window = document.getElementById(windowID);
        if (!window)
            return;
        MochaUI.closeWindow(window);
    };

    const closeFrameWindow = (window) => {
        closeWindow(window.frameElement.closest("div.mocha").id);
    };

    const getSyncMainDataInterval = () => {
        return customSyncMainDataInterval ? customSyncMainDataInterval : serverSyncMainDataInterval;
    };

    let stopped = false;
    const isStopped = () => {
        return stopped;
    };

    const stop = () => {
        stopped = true;
    };

    const mainTitle = () => {
        const emDash = "\u2014";
        const qbtVersion = window.qBittorrent.Cache.qbtVersion.get();
        const suffix = window.qBittorrent.Cache.preferences.get()["app_instance_name"] || "";
        const title = `qBittorrent ${qbtVersion} QBT_TR(WebUI)QBT_TR[CONTEXT=OptionsDialog]`
            + ((suffix.length > 0) ? ` ${emDash} ${suffix}` : "");
        return title;
    };

    let showingSearchEngine = false;
    let showingRssReader = false;
    let showingLogViewer = false;

    const showSearchEngine = (bool) => {
        showingSearchEngine = bool;
    };
    const showRssReader = (bool) => {
        showingRssReader = bool;
    };
    const showLogViewer = (bool) => {
        showingLogViewer = bool;
    };
    const isShowSearchEngine = () => {
        return showingSearchEngine;
    };
    const isShowRssReader = () => {
        return showingRssReader;
    };
    const isShowLogViewer = () => {
        return showingLogViewer;
    };

    return exports();
})();
Object.freeze(window.qBittorrent.Client);

window.qBittorrent.Client.setup();

// TODO: move global functions/variables into some namespace/scope

this.torrentsTable = new window.qBittorrent.DynamicTable.TorrentsTable();

let updatePropertiesPanel = () => {};

this.updateMainData = () => {};
let alternativeSpeedLimits = false;
let queueing_enabled = true;
let serverSyncMainDataInterval = 1500;
let customSyncMainDataInterval = null;
let useSubcategories = true;
const useAutoHideZeroStatusFilters = LocalPreferences.get("hide_zero_status_filters", "false") === "true";
const displayFullURLTrackerColumn = LocalPreferences.get("full_url_tracker_column", "false") === "true";

/* Categories filter */
const CATEGORIES_ALL = 1;
const CATEGORIES_UNCATEGORIZED = 2;

const category_list = new Map();

let selectedCategory = Number(LocalPreferences.get("selected_category", CATEGORIES_ALL));
let setCategoryFilter = () => {};

/* Tags filter */
const TAGS_ALL = 1;
const TAGS_UNTAGGED = 2;

const tagList = new Map();

let selectedTag = Number(LocalPreferences.get("selected_tag", TAGS_ALL));
let setTagFilter = () => {};

/* Trackers filter */
const TRACKERS_ALL = 1;
const TRACKERS_TRACKERLESS = 2;

/** @type Map<number, {host: string, trackerTorrentMap: Map<string, string[]>}> **/
const trackerList = new Map();

let selectedTracker = Number(LocalPreferences.get("selected_tracker", TRACKERS_ALL));
let setTrackerFilter = () => {};

/* All filters */
let selectedStatus = LocalPreferences.get("selected_filter", "all");
let setStatusFilter = () => {};
let toggleFilterDisplay = () => {};

window.addEventListener("DOMContentLoaded", () => {
    let isSearchPanelLoaded = false;
    let isLogPanelLoaded = false;
    let isRssPanelLoaded = false;

    const saveColumnSizes = () => {
        const filters_width = $("Filters").getSize().x;
        LocalPreferences.set("filters_width", filters_width);
        const properties_height_rel = $("propertiesPanel").getSize().y / Window.getSize().y;
        LocalPreferences.set("properties_height_rel", properties_height_rel);
    };

    window.addEventListener("resize", window.qBittorrent.Misc.createDebounceHandler(500, (e) => {
        // only save sizes if the columns are visible
        if (!$("mainColumn").hasClass("invisible"))
            saveColumnSizes();
    }));

    /* MochaUI.Desktop = new MochaUI.Desktop();
    MochaUI.Desktop.desktop.style.background = "#fff";
    MochaUI.Desktop.desktop.style.visibility = "visible"; */
    MochaUI.Desktop.initialize();

    const buildTransfersTab = () => {
        new MochaUI.Column({
            id: "filtersColumn",
            placement: "left",
            onResize: window.qBittorrent.Misc.createDebounceHandler(500, (e) => {
                saveColumnSizes();
            }),
            width: Number(LocalPreferences.get("filters_width", 210)),
            resizeLimit: [1, 1000]
        });
        new MochaUI.Column({
            id: "mainColumn",
            placement: "main"
        });
    };

    const buildSearchTab = () => {
        new MochaUI.Column({
            id: "searchTabColumn",
            placement: "main",
            width: null
        });

        // start off hidden
        $("searchTabColumn").addClass("invisible");
    };

    const buildRssTab = () => {
        new MochaUI.Column({
            id: "rssTabColumn",
            placement: "main",
            width: null
        });

        // start off hidden
        $("rssTabColumn").addClass("invisible");
    };

    const buildLogTab = () => {
        new MochaUI.Column({
            id: "logTabColumn",
            placement: "main",
            width: null
        });

        // start off hidden
        $("logTabColumn").addClass("invisible");
    };

    buildTransfersTab();
    buildSearchTab();
    buildRssTab();
    buildLogTab();
    MochaUI.initializeTabs("mainWindowTabsList");

    const handleFilterSelectionChange = (prevSelectedTorrent, currSelectedTorrent) => {
        // clear properties panels when filter changes (e.g. selected torrent is no longer visible)
        if (prevSelectedTorrent !== currSelectedTorrent) {
            window.qBittorrent.PropGeneral.clear();
            window.qBittorrent.PropTrackers.clear();
            window.qBittorrent.PropPeers.clear();
            window.qBittorrent.PropWebseeds.clear();
            window.qBittorrent.PropFiles.clear();
        }
    };

    setStatusFilter = (name) => {
        const currentHash = torrentsTable.getCurrentTorrentID();

        LocalPreferences.set("selected_filter", name);
        selectedStatus = name;
        highlightSelectedStatus();
        updateMainData();

        const newHash = torrentsTable.getCurrentTorrentID();
        handleFilterSelectionChange(currentHash, newHash);
    };

    setCategoryFilter = (hash) => {
        const currentHash = torrentsTable.getCurrentTorrentID();

        LocalPreferences.set("selected_category", hash);
        selectedCategory = Number(hash);
        highlightSelectedCategory();
        updateMainData();

        const newHash = torrentsTable.getCurrentTorrentID();
        handleFilterSelectionChange(currentHash, newHash);
    };

    setTagFilter = (hash) => {
        const currentHash = torrentsTable.getCurrentTorrentID();

        LocalPreferences.set("selected_tag", hash);
        selectedTag = Number(hash);
        highlightSelectedTag();
        updateMainData();

        const newHash = torrentsTable.getCurrentTorrentID();
        handleFilterSelectionChange(currentHash, newHash);
    };

    setTrackerFilter = (hash) => {
        const currentHash = torrentsTable.getCurrentTorrentID();

        LocalPreferences.set("selected_tracker", hash);
        selectedTracker = Number(hash);
        highlightSelectedTracker();
        updateMainData();

        const newHash = torrentsTable.getCurrentTorrentID();
        handleFilterSelectionChange(currentHash, newHash);
    };

    toggleFilterDisplay = (filterListID) => {
        const filterList = document.getElementById(filterListID);
        const filterTitle = filterList.previousElementSibling;
        const toggleIcon = filterTitle.firstElementChild;
        toggleIcon.classList.toggle("rotate");
        LocalPreferences.set(`filter_${filterListID.replace("FilterList", "")}_collapsed`, filterList.classList.toggle("invisible").toString());
    };

    new MochaUI.Panel({
        id: "Filters",
        title: "Panel",
        header: false,
        padding: {
            top: 0,
            right: 0,
            bottom: 0,
            left: 0
        },
        loadMethod: "xhr",
        contentURL: "views/filters.html",
        onContentLoaded: () => {
            highlightSelectedStatus();
        },
        column: "filtersColumn",
        height: 300
    });
    initializeWindows();

    // Show Top Toolbar is enabled by default
    let showTopToolbar = LocalPreferences.get("show_top_toolbar", "true") === "true";
    if (!showTopToolbar) {
        $("showTopToolbarLink").firstChild.style.opacity = "0";
        $("mochaToolbar").addClass("invisible");
    }

    // Show Status Bar is enabled by default
    let showStatusBar = LocalPreferences.get("show_status_bar", "true") === "true";
    if (!showStatusBar) {
        $("showStatusBarLink").firstChild.style.opacity = "0";
        $("desktopFooterWrapper").addClass("invisible");
    }

    // Show Filters Sidebar is enabled by default
    let showFiltersSidebar = LocalPreferences.get("show_filters_sidebar", "true") === "true";
    if (!showFiltersSidebar) {
        $("showFiltersSidebarLink").firstChild.style.opacity = "0";
        $("filtersColumn").addClass("invisible");
        $("filtersColumn_handle").addClass("invisible");
    }

    let speedInTitle = LocalPreferences.get("speed_in_browser_title_bar") === "true";
    if (!speedInTitle)
        $("speedInBrowserTitleBarLink").firstChild.style.opacity = "0";

    // After showing/hiding the toolbar + status bar
    window.qBittorrent.Client.showSearchEngine(LocalPreferences.get("show_search_engine") !== "false");
    window.qBittorrent.Client.showRssReader(LocalPreferences.get("show_rss_reader") !== "false");
    window.qBittorrent.Client.showLogViewer(LocalPreferences.get("show_log_viewer") === "true");

    // After Show Top Toolbar
    MochaUI.Desktop.setDesktopSize();

    let syncMainDataLastResponseId = 0;
    const serverState = {};

    const removeTorrentFromCategoryList = (hash) => {
        if (!hash)
            return false;

        let removed = false;
        category_list.forEach((category) => {
            const deleteResult = category.torrents.delete(hash);
            removed ||= deleteResult;
        });

        return removed;
    };

    const addTorrentToCategoryList = (torrent) => {
        const category = torrent["category"];
        if (typeof category === "undefined")
            return false;

        const hash = torrent["hash"];
        if (category.length === 0) { // Empty category
            removeTorrentFromCategoryList(hash);
            return true;
        }

        const categoryHash = window.qBittorrent.Misc.genHash(category);
        if (!category_list.has(categoryHash)) { // This should not happen
            category_list.set(categoryHash, {
                name: category,
                torrents: new Set()
            });
        }

        const torrents = category_list.get(categoryHash).torrents;
        if (!torrents.has(hash)) {
            removeTorrentFromCategoryList(hash);
            torrents.add(hash);
            return true;
        }
        return false;
    };

    const removeTorrentFromTagList = (hash) => {
        if (!hash)
            return false;

        let removed = false;
        tagList.forEach((tag) => {
            const deleteResult = tag.torrents.delete(hash);
            removed ||= deleteResult;
        });

        return removed;
    };

    const addTorrentToTagList = (torrent) => {
        if (torrent["tags"] === undefined) // Tags haven't changed
            return false;

        const hash = torrent["hash"];
        removeTorrentFromTagList(hash);

        if (torrent["tags"].length === 0) // No tags
            return true;

        const tags = torrent["tags"].split(",");
        let added = false;
        for (let i = 0; i < tags.length; ++i) {
            const tagHash = window.qBittorrent.Misc.genHash(tags[i].trim());
            if (!tagList.has(tagHash)) { // This should not happen
                tagList.set(tagHash, {
                    name: tags,
                    torrents: new Set()
                });
            }

            const torrents = tagList.get(tagHash).torrents;
            if (!torrents.has(hash)) {
                torrents.add(hash);
                added = true;
            }
        }
        return added;
    };

    const updateFilter = (filter, filterTitle) => {
        const filterEl = document.getElementById(`${filter}_filter`);
        const filterTorrentCount = torrentsTable.getFilteredTorrentsNumber(filter, CATEGORIES_ALL, TAGS_ALL, TRACKERS_ALL);
        if (useAutoHideZeroStatusFilters) {
            const hideFilter = (filterTorrentCount === 0) && (filter !== "all");
            if (filterEl.classList.toggle("invisible", hideFilter))
                return;
        }
        filterEl.firstElementChild.lastChild.nodeValue = filterTitle.replace("%1", filterTorrentCount);
    };

    const updateFiltersList = () => {
        updateFilter("all", "QBT_TR(All (%1))QBT_TR[CONTEXT=StatusFilterWidget]");
        updateFilter("downloading", "QBT_TR(Downloading (%1))QBT_TR[CONTEXT=StatusFilterWidget]");
        updateFilter("seeding", "QBT_TR(Seeding (%1))QBT_TR[CONTEXT=StatusFilterWidget]");
        updateFilter("completed", "QBT_TR(Completed (%1))QBT_TR[CONTEXT=StatusFilterWidget]");
        updateFilter("running", "QBT_TR(Running (%1))QBT_TR[CONTEXT=StatusFilterWidget]");
        updateFilter("stopped", "QBT_TR(Stopped (%1))QBT_TR[CONTEXT=StatusFilterWidget]");
        updateFilter("active", "QBT_TR(Active (%1))QBT_TR[CONTEXT=StatusFilterWidget]");
        updateFilter("inactive", "QBT_TR(Inactive (%1))QBT_TR[CONTEXT=StatusFilterWidget]");
        updateFilter("stalled", "QBT_TR(Stalled (%1))QBT_TR[CONTEXT=StatusFilterWidget]");
        updateFilter("stalled_uploading", "QBT_TR(Stalled Uploading (%1))QBT_TR[CONTEXT=StatusFilterWidget]");
        updateFilter("stalled_downloading", "QBT_TR(Stalled Downloading (%1))QBT_TR[CONTEXT=StatusFilterWidget]");
        updateFilter("checking", "QBT_TR(Checking (%1))QBT_TR[CONTEXT=StatusFilterWidget]");
        updateFilter("moving", "QBT_TR(Moving (%1))QBT_TR[CONTEXT=StatusFilterWidget]");
        updateFilter("errored", "QBT_TR(Errored (%1))QBT_TR[CONTEXT=StatusFilterWidget]");
    };

    const highlightSelectedStatus = () => {
        const statusFilter = document.getElementById("statusFilterList");
        const filterID = `${selectedStatus}_filter`;
        for (const status of statusFilter.children)
            status.classList.toggle("selectedFilter", (status.id === filterID));
    };

    const updateCategoryList = () => {
        const categoryList = document.getElementById("categoryFilterList");
        if (!categoryList)
            return;
        categoryList.getChildren().each(c => c.destroy());

        const categoryItemTemplate = document.getElementById("categoryFilterItem");

        const createCategoryLink = (hash, name, count) => {
            const categoryFilterItem = categoryItemTemplate.content.cloneNode(true).firstElementChild;
            categoryFilterItem.id = hash;
            categoryFilterItem.classList.toggle("selectedFilter", hash === selectedCategory);

            const span = categoryFilterItem.firstElementChild;
            span.lastElementChild.textContent = `${name} (${count})`;

            return categoryFilterItem;
        };

        const createCategoryTree = (category) => {
            const stack = [{ parent: categoriesFragment, category: category }];
            while (stack.length > 0) {
                const { parent, category } = stack.pop();
                const displayName = category.nameSegments.at(-1);
                const listItem = createCategoryLink(category.categoryHash, displayName, category.categoryCount);
                listItem.firstElementChild.style.paddingLeft = `${(category.nameSegments.length - 1) * 20 + 6}px`;

                parent.appendChild(listItem);

                if (category.children.length > 0) {
                    listItem.querySelector(".categoryToggle").style.visibility = "visible";
                    const unorderedList = document.createElement("ul");
                    listItem.appendChild(unorderedList);
                    for (const subcategory of category.children.reverse())
                        stack.push({ parent: unorderedList, category: subcategory });
                }
                const categoryLocalPref = `category_${category.categoryHash}_collapsed`;
                const isCollapsed = !category.forceExpand && (LocalPreferences.get(categoryLocalPref, "false") === "true");
                LocalPreferences.set(categoryLocalPref, listItem.classList.toggle("collapsedCategory", isCollapsed).toString());
            }
        };

        let uncategorized = 0;
        for (const { full_data: { category } } of torrentsTable.getRowValues()) {
            if (category.length === 0)
                uncategorized += 1;
        }

        const sortedCategories = [];
        category_list.forEach((category, hash) => sortedCategories.push({
            categoryName: category.name,
            categoryHash: hash,
            categoryCount: category.torrents.size,
            nameSegments: category.name.split("/"),
            ...(useSubcategories && {
                children: [],
                parentID: null,
                forceExpand: LocalPreferences.get(`category_${hash}_collapsed`) === null
            })
        }));
        sortedCategories.sort((left, right) => {
            const leftSegments = left.nameSegments;
            const rightSegments = right.nameSegments;

            for (let i = 0, iMax = Math.min(leftSegments.length, rightSegments.length); i < iMax; ++i) {
                const compareResult = window.qBittorrent.Misc.naturalSortCollator.compare(
                    leftSegments[i], rightSegments[i]);
                if (compareResult !== 0)
                    return compareResult;
            }

            return leftSegments.length - rightSegments.length;
        });

        const categoriesFragment = new DocumentFragment();
        categoriesFragment.appendChild(createCategoryLink(CATEGORIES_ALL, "QBT_TR(All)QBT_TR[CONTEXT=CategoryFilterModel]", torrentsTable.getRowSize()));
        categoriesFragment.appendChild(createCategoryLink(CATEGORIES_UNCATEGORIZED, "QBT_TR(Uncategorized)QBT_TR[CONTEXT=CategoryFilterModel]", uncategorized));

        if (useSubcategories) {
            categoryList.classList.add("subcategories");
            for (let i = 0; i < sortedCategories.length; ++i) {
                const category = sortedCategories[i];
                for (let j = (i + 1);
                    ((j < sortedCategories.length) && sortedCategories[j].categoryName.startsWith(`${category.categoryName}/`)); ++j) {
                    const subcategory = sortedCategories[j];
                    category.categoryCount += subcategory.categoryCount;
                    category.forceExpand ||= subcategory.forceExpand;

                    const isDirectSubcategory = (subcategory.nameSegments.length - category.nameSegments.length) === 1;
                    if (isDirectSubcategory) {
                        subcategory.parentID = category.categoryHash;
                        category.children.push(subcategory);
                    }
                }
            }
            for (const category of sortedCategories) {
                if (category.parentID === null)
                    createCategoryTree(category);
            }
        }
        else {
            categoryList.classList.remove("subcategories");
            for (const { categoryHash, categoryName, categoryCount } of sortedCategories)
                categoriesFragment.appendChild(createCategoryLink(categoryHash, categoryName, categoryCount));
        }

        categoryList.appendChild(categoriesFragment);
        window.qBittorrent.Filters.categoriesFilterContextMenu.searchAndAddTargets();
    };

    const highlightSelectedCategory = () => {
        const categoryList = document.getElementById("categoryFilterList");
        if (!categoryList)
            return;

        for (const category of categoryList.getElementsByTagName("li"))
            category.classList.toggle("selectedFilter", (Number(category.id) === selectedCategory));
    };

    const updateTagList = () => {
        const tagFilterList = $("tagFilterList");
        if (tagFilterList === null)
            return;

        tagFilterList.getChildren().each(c => c.destroy());

        const tagItemTemplate = document.getElementById("tagFilterItem");

        const createLink = (hash, text, count) => {
            const tagFilterItem = tagItemTemplate.content.cloneNode(true).firstElementChild;
            tagFilterItem.id = hash;
            tagFilterItem.classList.toggle("selectedFilter", hash === selectedTag);

            const span = tagFilterItem.firstElementChild;
            span.lastChild.textContent = `${text} (${count})`;

            return tagFilterItem;
        };

        let untagged = 0;
        for (const { full_data: { tags } } of torrentsTable.getRowValues()) {
            if (tags.length === 0)
                untagged += 1;
        }

        tagFilterList.appendChild(createLink(TAGS_ALL, "QBT_TR(All)QBT_TR[CONTEXT=TagFilterModel]", torrentsTable.getRowSize()));
        tagFilterList.appendChild(createLink(TAGS_UNTAGGED, "QBT_TR(Untagged)QBT_TR[CONTEXT=TagFilterModel]", untagged));

        const sortedTags = [];
        tagList.forEach((tag, hash) => sortedTags.push({
            tagName: tag.name,
            tagHash: hash,
            tagSize: tag.torrents.size
        }));
        sortedTags.sort((left, right) => window.qBittorrent.Misc.naturalSortCollator.compare(left.tagName, right.tagName));

        for (const { tagName, tagHash, tagSize } of sortedTags)
            tagFilterList.appendChild(createLink(tagHash, tagName, tagSize));

        window.qBittorrent.Filters.tagsFilterContextMenu.searchAndAddTargets();
    };

    const highlightSelectedTag = () => {
        const tagFilterList = document.getElementById("tagFilterList");
        if (!tagFilterList)
            return;

        for (const tag of tagFilterList.children)
            tag.classList.toggle("selectedFilter", (Number(tag.id) === selectedTag));
    };

    const updateTrackerList = () => {
        const trackerFilterList = $("trackerFilterList");
        if (trackerFilterList === null)
            return;

        trackerFilterList.getChildren().each(c => c.destroy());

        const trackerItemTemplate = document.getElementById("trackerFilterItem");

        const createLink = (hash, text, count) => {
            const trackerFilterItem = trackerItemTemplate.content.cloneNode(true).firstElementChild;
            trackerFilterItem.id = hash;
            trackerFilterItem.classList.toggle("selectedFilter", hash === selectedTracker);

            const span = trackerFilterItem.firstElementChild;
            span.lastChild.textContent = text.replace("%1", count);

            return trackerFilterItem;
        };

        let trackerlessTorrentsCount = 0;
        for (const { full_data: { trackers_count: trackersCount } } of torrentsTable.getRowValues()) {
            if (trackersCount === 0)
                trackerlessTorrentsCount += 1;
        }

        trackerFilterList.appendChild(createLink(TRACKERS_ALL, "QBT_TR(All (%1))QBT_TR[CONTEXT=TrackerFiltersList]", torrentsTable.getRowSize()));
        trackerFilterList.appendChild(createLink(TRACKERS_TRACKERLESS, "QBT_TR(Trackerless (%1))QBT_TR[CONTEXT=TrackerFiltersList]", trackerlessTorrentsCount));

        // Sort trackers by hostname
        const sortedList = [];
        trackerList.forEach(({ host, trackerTorrentMap }, hash) => {
            const uniqueTorrents = new Set();
            for (const torrents of trackerTorrentMap.values()) {
                for (const torrent of torrents)
                    uniqueTorrents.add(torrent);
            }

            sortedList.push({
                trackerHost: host,
                trackerHash: hash,
                trackerCount: uniqueTorrents.size,
            });
        });
        sortedList.sort((left, right) => window.qBittorrent.Misc.naturalSortCollator.compare(left.trackerHost, right.trackerHost));
        for (const { trackerHost, trackerHash, trackerCount } of sortedList)
            trackerFilterList.appendChild(createLink(trackerHash, (trackerHost + " (%1)"), trackerCount));

        window.qBittorrent.Filters.trackersFilterContextMenu.searchAndAddTargets();
    };

    const highlightSelectedTracker = () => {
        const trackerFilterList = document.getElementById("trackerFilterList");
        if (!trackerFilterList)
            return;

        for (const tracker of trackerFilterList.children)
            tracker.classList.toggle("selectedFilter", (Number(tracker.id) === selectedTracker));
    };

    const statusSortOrder = Object.freeze({
        "unknown": -1,
        "forcedDL": 0,
        "downloading": 1,
        "forcedMetaDL": 2,
        "metaDL": 3,
        "stalledDL": 4,
        "forcedUP": 5,
        "uploading": 6,
        "stalledUP": 7,
        "checkingResumeData": 8,
        "queuedDL": 9,
        "queuedUP": 10,
        "checkingUP": 11,
        "checkingDL": 12,
        "stoppedDL": 13,
        "stoppedUP": 14,
        "moving": 15,
        "missingFiles": 16,
        "error": 17
    });

    let syncMainDataTimeoutID = -1;
    let syncRequestInProgress = false;
    const syncMainData = () => {
        const url = new URI("api/v2/sync/maindata");
        url.setData("rid", syncMainDataLastResponseId);
        const request = new Request.JSON({
            url: url,
            noCache: true,
            method: "get",
            onFailure: () => {
                const errorDiv = $("error_div");
                if (errorDiv)
                    errorDiv.textContent = "QBT_TR(qBittorrent client is not reachable)QBT_TR[CONTEXT=HttpServer]";
                syncRequestInProgress = false;
                syncData(2000);
            },
            onSuccess: (response) => {
                $("error_div").textContent = "";
                if (response) {
                    clearTimeout(torrentsFilterInputTimer);
                    torrentsFilterInputTimer = -1;

                    let torrentsTableSelectedRows;
                    let update_categories = false;
                    let updateTags = false;
                    let updateTrackers = false;
                    let updateTorrents = false;
                    const full_update = (response["full_update"] === true);
                    if (full_update) {
                        torrentsTableSelectedRows = torrentsTable.selectedRowsIds();
                        update_categories = true;
                        updateTags = true;
                        updateTrackers = true;
                        updateTorrents = true;
                        torrentsTable.clear();
                        category_list.clear();
                        tagList.clear();
                        trackerList.clear();
                    }
                    if (response["rid"])
                        syncMainDataLastResponseId = response["rid"];
                    if (response["categories"]) {
                        for (const key in response["categories"]) {
                            if (!Object.hasOwn(response["categories"], key))
                                continue;

                            const responseCategory = response["categories"][key];
                            const categoryHash = window.qBittorrent.Misc.genHash(key);
                            const category = category_list.get(categoryHash);
                            if (category !== undefined) {
                                // only the save path can change for existing categories
                                category.savePath = responseCategory.savePath;
                            }
                            else {
                                category_list.set(categoryHash, {
                                    name: responseCategory.name,
                                    savePath: responseCategory.savePath,
                                    torrents: new Set()
                                });
                            }
                        }
                        update_categories = true;
                    }
                    if (response["categories_removed"]) {
                        response["categories_removed"].each((category) => {
                            const categoryHash = window.qBittorrent.Misc.genHash(category);
                            category_list.delete(categoryHash);
                        });
                        update_categories = true;
                    }
                    if (response["tags"]) {
                        for (const tag of response["tags"]) {
                            const tagHash = window.qBittorrent.Misc.genHash(tag);
                            if (!tagList.has(tagHash)) {
                                tagList.set(tagHash, {
                                    name: tag,
                                    torrents: new Set()
                                });
                            }
                        }
                        updateTags = true;
                    }
                    if (response["tags_removed"]) {
                        for (let i = 0; i < response["tags_removed"].length; ++i) {
                            const tagHash = window.qBittorrent.Misc.genHash(response["tags_removed"][i]);
                            tagList.delete(tagHash);
                        }
                        updateTags = true;
                    }
                    if (response["trackers"]) {
                        for (const [tracker, torrents] of Object.entries(response["trackers"])) {
                            const host = window.qBittorrent.Misc.getHost(tracker);
                            const hash = window.qBittorrent.Misc.genHash(host);

                            let trackerListItem = trackerList.get(hash);
                            if (trackerListItem === undefined) {
                                trackerListItem = { host: host, trackerTorrentMap: new Map() };
                                trackerList.set(hash, trackerListItem);
                            }
                            trackerListItem.trackerTorrentMap.set(tracker, new Set(torrents));
                        }
                        updateTrackers = true;
                    }
                    if (response["trackers_removed"]) {
                        for (let i = 0; i < response["trackers_removed"].length; ++i) {
                            const tracker = response["trackers_removed"][i];
                            const host = window.qBittorrent.Misc.getHost(tracker);
                            const hash = window.qBittorrent.Misc.genHash(host);
                            const trackerListEntry = trackerList.get(hash);
                            if (trackerListEntry) {
                                trackerListEntry.trackerTorrentMap.delete(tracker);
                                // Remove unused trackers
                                if (trackerListEntry.trackerTorrentMap.size === 0) {
                                    trackerList.delete(hash);
                                    if (selectedTracker === hash) {
                                        selectedTracker = TRACKERS_ALL;
                                        LocalPreferences.set("selected_tracker", selectedTracker.toString());
                                    }
                                }
                            }
                        }
                        updateTrackers = true;
                    }
                    if (response["torrents"]) {
                        for (const key in response["torrents"]) {
                            if (!Object.hasOwn(response["torrents"], key))
                                continue;

                            response["torrents"][key]["hash"] = key;
                            response["torrents"][key]["rowId"] = key;
                            if (response["torrents"][key]["state"]) {
                                const state = response["torrents"][key]["state"];
                                response["torrents"][key]["status"] = state;
                                response["torrents"][key]["_statusOrder"] = statusSortOrder[state];
                            }
                            torrentsTable.updateRowData(response["torrents"][key]);
                            if (addTorrentToCategoryList(response["torrents"][key]))
                                update_categories = true;
                            if (addTorrentToTagList(response["torrents"][key]))
                                updateTags = true;
                            updateTorrents = true;
                        }
                    }
                    if (response["torrents_removed"]) {
                        response["torrents_removed"].each((hash) => {
                            torrentsTable.removeRow(hash);
                            removeTorrentFromCategoryList(hash);
                            update_categories = true; // Always to update All category
                            removeTorrentFromTagList(hash);
                            updateTags = true; // Always to update All tag
                        });
                        updateTorrents = true;
                    }

                    // don't update the table unnecessarily
                    if (updateTorrents)
                        torrentsTable.updateTable(full_update);

                    if (response["server_state"]) {
                        const tmp = response["server_state"];
                        for (const k in tmp) {
                            if (!Object.hasOwn(tmp, k))
                                continue;
                            serverState[k] = tmp[k];
                        }
                        processServerState();
                    }
                    updateFiltersList();
                    if (update_categories) {
                        updateCategoryList();
                        window.qBittorrent.TransferList.contextMenu.updateCategoriesSubMenu(category_list);
                    }
                    if (updateTags) {
                        updateTagList();
                        window.qBittorrent.TransferList.contextMenu.updateTagsSubMenu(tagList);
                    }
                    if (updateTrackers)
                        updateTrackerList();

                    if (full_update)
                        // re-select previously selected rows
                        torrentsTable.reselectRows(torrentsTableSelectedRows);
                }
                syncRequestInProgress = false;
                syncData(window.qBittorrent.Client.getSyncMainDataInterval());
            }
        });
        syncRequestInProgress = true;
        request.send();
    };

    updateMainData = () => {
        torrentsTable.updateTable();
        syncData(100);
    };

    const syncData = (delay) => {
        if (syncRequestInProgress)
            return;

        clearTimeout(syncMainDataTimeoutID);
        syncMainDataTimeoutID = -1;

        if (window.qBittorrent.Client.isStopped())
            return;

        syncMainDataTimeoutID = syncMainData.delay(delay);
    };

    const processServerState = () => {
        let transfer_info = window.qBittorrent.Misc.friendlyUnit(serverState.dl_info_speed, true);
        if (serverState.dl_rate_limit > 0)
            transfer_info += " [" + window.qBittorrent.Misc.friendlyUnit(serverState.dl_rate_limit, true) + "]";
        transfer_info += " (" + window.qBittorrent.Misc.friendlyUnit(serverState.dl_info_data, false) + ")";
        $("DlInfos").textContent = transfer_info;
        transfer_info = window.qBittorrent.Misc.friendlyUnit(serverState.up_info_speed, true);
        if (serverState.up_rate_limit > 0)
            transfer_info += " [" + window.qBittorrent.Misc.friendlyUnit(serverState.up_rate_limit, true) + "]";
        transfer_info += " (" + window.qBittorrent.Misc.friendlyUnit(serverState.up_info_data, false) + ")";
        $("UpInfos").textContent = transfer_info;

        document.title = (speedInTitle
                ? (`QBT_TR([D: %1, U: %2])QBT_TR[CONTEXT=MainWindow] `
                    .replace("%1", window.qBittorrent.Misc.friendlyUnit(serverState.dl_info_speed, true))
                    .replace("%2", window.qBittorrent.Misc.friendlyUnit(serverState.up_info_speed, true)))
                : "")
            + window.qBittorrent.Client.mainTitle();

        $("freeSpaceOnDisk").textContent = "QBT_TR(Free space: %1)QBT_TR[CONTEXT=HttpServer]".replace("%1", window.qBittorrent.Misc.friendlyUnit(serverState.free_space_on_disk));

        const externalIPsElement = document.getElementById("externalIPs");
        if (window.qBittorrent.Cache.preferences.get().status_bar_external_ip) {
            const lastExternalAddressV4 = serverState.last_external_address_v4;
            const lastExternalAddressV6 = serverState.last_external_address_v6;
            const hasIPv4Address = lastExternalAddressV4 !== "";
            const hasIPv6Address = lastExternalAddressV6 !== "";
            let lastExternalAddressLabel = "QBT_TR(External IP: N/A)QBT_TR[CONTEXT=HttpServer]";
            if (hasIPv4Address && hasIPv6Address)
                lastExternalAddressLabel = "QBT_TR(External IPs: %1, %2)QBT_TR[CONTEXT=HttpServer]";
            else if (hasIPv4Address || hasIPv6Address)
                lastExternalAddressLabel = "QBT_TR(External IP: %1%2)QBT_TR[CONTEXT=HttpServer]";
            // replace in reverse order ('%2' before '%1') in case address contains a % character.
            // for example, see https://en.wikipedia.org/wiki/IPv6_address#Scoped_literal_IPv6_addresses_(with_zone_index)
            externalIPsElement.textContent = lastExternalAddressLabel.replace("%2", lastExternalAddressV6).replace("%1", lastExternalAddressV4);
            externalIPsElement.classList.remove("invisible");
            externalIPsElement.previousElementSibling.classList.remove("invisible");
        }
        else {
            externalIPsElement.classList.add("invisible");
            externalIPsElement.previousElementSibling.classList.add("invisible");
        }

        const dhtElement = document.getElementById("DHTNodes");
        if (window.qBittorrent.Cache.preferences.get().dht) {
            dhtElement.textContent = "QBT_TR(DHT: %1 nodes)QBT_TR[CONTEXT=StatusBar]".replace("%1", serverState.dht_nodes);
            dhtElement.classList.remove("invisible");
            dhtElement.previousElementSibling.classList.remove("invisible");
        }
        else {
            dhtElement.classList.add("invisible");
            dhtElement.previousElementSibling.classList.add("invisible");
        }

        // Statistics dialog
        if (document.getElementById("statisticsContent")) {
            $("AlltimeDL").textContent = window.qBittorrent.Misc.friendlyUnit(serverState.alltime_dl, false);
            $("AlltimeUL").textContent = window.qBittorrent.Misc.friendlyUnit(serverState.alltime_ul, false);
            $("TotalWastedSession").textContent = window.qBittorrent.Misc.friendlyUnit(serverState.total_wasted_session, false);
            $("GlobalRatio").textContent = serverState.global_ratio;
            $("TotalPeerConnections").textContent = serverState.total_peer_connections;
            $("ReadCacheHits").textContent = serverState.read_cache_hits + "%";
            $("TotalBuffersSize").textContent = window.qBittorrent.Misc.friendlyUnit(serverState.total_buffers_size, false);
            $("WriteCacheOverload").textContent = serverState.write_cache_overload + "%";
            $("ReadCacheOverload").textContent = serverState.read_cache_overload + "%";
            $("QueuedIOJobs").textContent = serverState.queued_io_jobs;
            $("AverageTimeInQueue").textContent = serverState.average_time_queue + " ms";
            $("TotalQueuedSize").textContent = window.qBittorrent.Misc.friendlyUnit(serverState.total_queued_size, false);
        }

        switch (serverState.connection_status) {
            case "connected":
                $("connectionStatus").src = "images/connected.svg";
                $("connectionStatus").alt = "QBT_TR(Connection status: Connected)QBT_TR[CONTEXT=MainWindow]";
                $("connectionStatus").title = "QBT_TR(Connection status: Connected)QBT_TR[CONTEXT=MainWindow]";
                break;
            case "firewalled":
                $("connectionStatus").src = "images/firewalled.svg";
                $("connectionStatus").alt = "QBT_TR(Connection status: Firewalled)QBT_TR[CONTEXT=MainWindow]";
                $("connectionStatus").title = "QBT_TR(Connection status: Firewalled)QBT_TR[CONTEXT=MainWindow]";
                break;
            default:
                $("connectionStatus").src = "images/disconnected.svg";
                $("connectionStatus").alt = "QBT_TR(Connection status: Disconnected)QBT_TR[CONTEXT=MainWindow]";
                $("connectionStatus").title = "QBT_TR(Connection status: Disconnected)QBT_TR[CONTEXT=MainWindow]";
                break;
        }

        if (queueing_enabled !== serverState.queueing) {
            queueing_enabled = serverState.queueing;
            torrentsTable.columns["priority"].force_hide = !queueing_enabled;
            torrentsTable.updateColumn("priority");
            if (queueing_enabled) {
                $("topQueuePosItem").removeClass("invisible");
                $("increaseQueuePosItem").removeClass("invisible");
                $("decreaseQueuePosItem").removeClass("invisible");
                $("bottomQueuePosItem").removeClass("invisible");
                $("queueingButtons").removeClass("invisible");
                $("queueingMenuItems").removeClass("invisible");
            }
            else {
                $("topQueuePosItem").addClass("invisible");
                $("increaseQueuePosItem").addClass("invisible");
                $("decreaseQueuePosItem").addClass("invisible");
                $("bottomQueuePosItem").addClass("invisible");
                $("queueingButtons").addClass("invisible");
                $("queueingMenuItems").addClass("invisible");
            }
        }

        if (alternativeSpeedLimits !== serverState.use_alt_speed_limits) {
            alternativeSpeedLimits = serverState.use_alt_speed_limits;
            updateAltSpeedIcon(alternativeSpeedLimits);
        }

        if (useSubcategories !== serverState.use_subcategories) {
            useSubcategories = serverState.use_subcategories;
            updateCategoryList();
        }

        serverSyncMainDataInterval = Math.max(serverState.refresh_interval, 500);
    };

    const updateAltSpeedIcon = (enabled) => {
        if (enabled) {
            $("alternativeSpeedLimits").src = "images/slow.svg";
            $("alternativeSpeedLimits").alt = "QBT_TR(Alternative speed limits: On)QBT_TR[CONTEXT=MainWindow]";
            $("alternativeSpeedLimits").title = "QBT_TR(Alternative speed limits: On)QBT_TR[CONTEXT=MainWindow]";
        }
        else {
            $("alternativeSpeedLimits").src = "images/slow_off.svg";
            $("alternativeSpeedLimits").alt = "QBT_TR(Alternative speed limits: Off)QBT_TR[CONTEXT=MainWindow]";
            $("alternativeSpeedLimits").title = "QBT_TR(Alternative speed limits: Off)QBT_TR[CONTEXT=MainWindow]";
        }
    };

    $("alternativeSpeedLimits").addEventListener("click", () => {
        // Change icon immediately to give some feedback
        updateAltSpeedIcon(!alternativeSpeedLimits);

        new Request({
            url: "api/v2/transfer/toggleSpeedLimitsMode",
            method: "post",
            onComplete: () => {
                alternativeSpeedLimits = !alternativeSpeedLimits;
                updateMainData();
            },
            onFailure: () => {
                // Restore icon in case of failure
                updateAltSpeedIcon(alternativeSpeedLimits);
            }
        }).send();
    });

    $("DlInfos").addEventListener("click", () => { globalDownloadLimitFN(); });
    $("UpInfos").addEventListener("click", () => { globalUploadLimitFN(); });

    $("showTopToolbarLink").addEventListener("click", (e) => {
        showTopToolbar = !showTopToolbar;
        LocalPreferences.set("show_top_toolbar", showTopToolbar.toString());
        if (showTopToolbar) {
            $("showTopToolbarLink").firstChild.style.opacity = "1";
            $("mochaToolbar").removeClass("invisible");
        }
        else {
            $("showTopToolbarLink").firstChild.style.opacity = "0";
            $("mochaToolbar").addClass("invisible");
        }
        MochaUI.Desktop.setDesktopSize();
    });

    $("showStatusBarLink").addEventListener("click", (e) => {
        showStatusBar = !showStatusBar;
        LocalPreferences.set("show_status_bar", showStatusBar.toString());
        if (showStatusBar) {
            $("showStatusBarLink").firstChild.style.opacity = "1";
            $("desktopFooterWrapper").removeClass("invisible");
        }
        else {
            $("showStatusBarLink").firstChild.style.opacity = "0";
            $("desktopFooterWrapper").addClass("invisible");
        }
        MochaUI.Desktop.setDesktopSize();
    });

    const registerMagnetHandler = () => {
        if (typeof navigator.registerProtocolHandler !== "function") {
            if (window.location.protocol !== "https:")
                alert("QBT_TR(To use this feature, the WebUI needs to be accessed over HTTPS)QBT_TR[CONTEXT=MainWindow]");
            else
                alert("QBT_TR(Your browser does not support this feature)QBT_TR[CONTEXT=MainWindow]");
            return;
        }

        const hashString = location.hash ? location.hash.replace(/^#/, "") : "";
        const hashParams = new URLSearchParams(hashString);
        hashParams.set("download", "");

        const templateHashString = hashParams.toString().replace("download=", "download=%s");
        const templateUrl = location.origin + location.pathname
            + location.search + "#" + templateHashString;

        navigator.registerProtocolHandler("magnet", templateUrl,
            "qBittorrent WebUI magnet handler");
    };
    $("registerMagnetHandlerLink").addEventListener("click", (e) => {
        registerMagnetHandler();
    });

    $("showFiltersSidebarLink").addEventListener("click", (e) => {
        showFiltersSidebar = !showFiltersSidebar;
        LocalPreferences.set("show_filters_sidebar", showFiltersSidebar.toString());
        if (showFiltersSidebar) {
            $("showFiltersSidebarLink").firstChild.style.opacity = "1";
            $("filtersColumn").removeClass("invisible");
            $("filtersColumn_handle").removeClass("invisible");
        }
        else {
            $("showFiltersSidebarLink").firstChild.style.opacity = "0";
            $("filtersColumn").addClass("invisible");
            $("filtersColumn_handle").addClass("invisible");
        }
        MochaUI.Desktop.setDesktopSize();
    });

    $("speedInBrowserTitleBarLink").addEventListener("click", (e) => {
        speedInTitle = !speedInTitle;
        LocalPreferences.set("speed_in_browser_title_bar", speedInTitle.toString());
        if (speedInTitle)
            $("speedInBrowserTitleBarLink").firstChild.style.opacity = "1";
        else
            $("speedInBrowserTitleBarLink").firstChild.style.opacity = "0";
        processServerState();
    });

    $("showSearchEngineLink").addEventListener("click", (e) => {
        window.qBittorrent.Client.showSearchEngine(!window.qBittorrent.Client.isShowSearchEngine());
        LocalPreferences.set("show_search_engine", window.qBittorrent.Client.isShowSearchEngine().toString());
        updateTabDisplay();
    });

    $("showRssReaderLink").addEventListener("click", (e) => {
        window.qBittorrent.Client.showRssReader(!window.qBittorrent.Client.isShowRssReader());
        LocalPreferences.set("show_rss_reader", window.qBittorrent.Client.isShowRssReader().toString());
        updateTabDisplay();
    });

    $("showLogViewerLink").addEventListener("click", (e) => {
        window.qBittorrent.Client.showLogViewer(!window.qBittorrent.Client.isShowLogViewer());
        LocalPreferences.set("show_log_viewer", window.qBittorrent.Client.isShowLogViewer().toString());
        updateTabDisplay();
    });

    const updateTabDisplay = () => {
        if (window.qBittorrent.Client.isShowRssReader()) {
            $("showRssReaderLink").firstChild.style.opacity = "1";
            $("mainWindowTabs").removeClass("invisible");
            $("rssTabLink").removeClass("invisible");
            if (!MochaUI.Panels.instances.RssPanel)
                addRssPanel();
        }
        else {
            $("showRssReaderLink").firstChild.style.opacity = "0";
            $("rssTabLink").addClass("invisible");
            if ($("rssTabLink").hasClass("selected"))
                $("transfersTabLink").click();
        }

        if (window.qBittorrent.Client.isShowSearchEngine()) {
            $("showSearchEngineLink").firstChild.style.opacity = "1";
            $("mainWindowTabs").removeClass("invisible");
            $("searchTabLink").removeClass("invisible");
            if (!MochaUI.Panels.instances.SearchPanel)
                addSearchPanel();
        }
        else {
            $("showSearchEngineLink").firstChild.style.opacity = "0";
            $("searchTabLink").addClass("invisible");
            if ($("searchTabLink").hasClass("selected"))
                $("transfersTabLink").click();
        }

        if (window.qBittorrent.Client.isShowLogViewer()) {
            $("showLogViewerLink").firstChild.style.opacity = "1";
            $("mainWindowTabs").removeClass("invisible");
            $("logTabLink").removeClass("invisible");
            if (!MochaUI.Panels.instances.LogPanel)
                addLogPanel();
        }
        else {
            $("showLogViewerLink").firstChild.style.opacity = "0";
            $("logTabLink").addClass("invisible");
            if ($("logTabLink").hasClass("selected"))
                $("transfersTabLink").click();
        }

        // display no tabs
        if (!window.qBittorrent.Client.isShowRssReader() && !window.qBittorrent.Client.isShowSearchEngine() && !window.qBittorrent.Client.isShowLogViewer())
            $("mainWindowTabs").addClass("invisible");
    };

    $("StatisticsLink").addEventListener("click", () => { StatisticsLinkFN(); });

    // main window tabs

    const showTransfersTab = () => {
        const showFiltersSidebar = LocalPreferences.get("show_filters_sidebar", "true") === "true";
        if (showFiltersSidebar) {
            $("filtersColumn").removeClass("invisible");
            $("filtersColumn_handle").removeClass("invisible");
        }
        $("mainColumn").removeClass("invisible");
        $("torrentsFilterToolbar").removeClass("invisible");

        customSyncMainDataInterval = null;
        syncData(100);

        hideSearchTab();
        hideRssTab();
        hideLogTab();

        LocalPreferences.set("selected_window_tab", "transfers");
    };

    const hideTransfersTab = () => {
        $("filtersColumn").addClass("invisible");
        $("filtersColumn_handle").addClass("invisible");
        $("mainColumn").addClass("invisible");
        $("torrentsFilterToolbar").addClass("invisible");
        MochaUI.Desktop.resizePanels();
    };

    const showSearchTab = (() => {
        let searchTabInitialized = false;

        return () => {
            // we must wait until the panel is fully loaded before proceeding.
            // this include's the panel's custom js, which is loaded via MochaUI.Panel's 'require' field.
            // MochaUI loads these files asynchronously and thus all required libs may not be available immediately
            if (!isSearchPanelLoaded) {
                setTimeout(() => {
                    showSearchTab();
                }, 100);
                return;
            }

            if (!searchTabInitialized) {
                window.qBittorrent.Search.init();
                searchTabInitialized = true;
            }

            $("searchTabColumn").removeClass("invisible");
            customSyncMainDataInterval = 30000;
            hideTransfersTab();
            hideRssTab();
            hideLogTab();

            LocalPreferences.set("selected_window_tab", "search");
        };
    })();

    const hideSearchTab = () => {
        $("searchTabColumn").addClass("invisible");
        MochaUI.Desktop.resizePanels();
    };

    const showRssTab = (() => {
        let rssTabInitialized = false;

        return () => {
            // we must wait until the panel is fully loaded before proceeding.
            // this include's the panel's custom js, which is loaded via MochaUI.Panel's 'require' field.
            // MochaUI loads these files asynchronously and thus all required libs may not be available immediately
            if (!isRssPanelLoaded) {
                setTimeout(() => {
                    showRssTab();
                }, 100);
                return;
            }

            if (!rssTabInitialized) {
                window.qBittorrent.Rss.init();
                rssTabInitialized = true;
            }
            else {
                window.qBittorrent.Rss.load();
            }

            $("rssTabColumn").removeClass("invisible");
            customSyncMainDataInterval = 30000;
            hideTransfersTab();
            hideSearchTab();
            hideLogTab();

            LocalPreferences.set("selected_window_tab", "rss");
        };
    })();

    const hideRssTab = () => {
        $("rssTabColumn").addClass("invisible");
        window.qBittorrent.Rss && window.qBittorrent.Rss.unload();
        MochaUI.Desktop.resizePanels();
    };

    const showLogTab = (() => {
        let logTabInitialized = false;

        return () => {
            // we must wait until the panel is fully loaded before proceeding.
            // this include's the panel's custom js, which is loaded via MochaUI.Panel's 'require' field.
            // MochaUI loads these files asynchronously and thus all required libs may not be available immediately
            if (!isLogPanelLoaded) {
                setTimeout(() => {
                    showLogTab();
                }, 100);
                return;
            }

            if (!logTabInitialized) {
                window.qBittorrent.Log.init();
                logTabInitialized = true;
            }
            else {
                window.qBittorrent.Log.load();
            }

            $("logTabColumn").removeClass("invisible");
            customSyncMainDataInterval = 30000;
            hideTransfersTab();
            hideSearchTab();
            hideRssTab();

            LocalPreferences.set("selected_window_tab", "log");
        };
    })();

    const hideLogTab = () => {
        $("logTabColumn").addClass("invisible");
        MochaUI.Desktop.resizePanels();
        window.qBittorrent.Log && window.qBittorrent.Log.unload();
    };

    const addSearchPanel = () => {
        new MochaUI.Panel({
            id: "SearchPanel",
            title: "Search",
            header: false,
            padding: {
                top: 0,
                right: 0,
                bottom: 0,
                left: 0
            },
            loadMethod: "xhr",
            contentURL: "views/search.html",
            require: {
                js: ["scripts/search.js"],
                onload: () => {
                    isSearchPanelLoaded = true;
                },
            },
            content: "",
            column: "searchTabColumn",
            height: null
        });
    };

    const addRssPanel = () => {
        new MochaUI.Panel({
            id: "RssPanel",
            title: "Rss",
            header: false,
            padding: {
                top: 0,
                right: 0,
                bottom: 0,
                left: 0
            },
            loadMethod: "xhr",
            contentURL: "views/rss.html",
            onContentLoaded: () => {
                isRssPanelLoaded = true;
            },
            content: "",
            column: "rssTabColumn",
            height: null
        });
    };

    const addLogPanel = () => {
        new MochaUI.Panel({
            id: "LogPanel",
            title: "Log",
            header: true,
            padding: {
                top: 0,
                right: 0,
                bottom: 0,
                left: 0
            },
            loadMethod: "xhr",
            contentURL: "views/log.html",
            require: {
                css: ["css/vanillaSelectBox.css"],
                js: ["scripts/lib/vanillaSelectBox.js"],
                onload: () => {
                    isLogPanelLoaded = true;
                },
            },
            tabsURL: "views/logTabs.html",
            tabsOnload: () => {
                MochaUI.initializeTabs("panelTabs");

                $("logMessageLink").addEventListener("click", (e) => {
                    window.qBittorrent.Log.setCurrentTab("main");
                });

                $("logPeerLink").addEventListener("click", (e) => {
                    window.qBittorrent.Log.setCurrentTab("peer");
                });
            },
            collapsible: false,
            content: "",
            column: "logTabColumn",
            height: null
        });
    };

    const handleDownloadParam = () => {
        // Extract torrent URL from download param in WebUI URL hash
        const downloadHash = "#download=";
        if (location.hash.indexOf(downloadHash) !== 0)
            return;

        const url = decodeURIComponent(location.hash.substring(downloadHash.length));
        // Remove the processed hash from the URL
        history.replaceState("", document.title, (location.pathname + location.search));
        showDownloadPage([url]);
    };

    new MochaUI.Panel({
        id: "transferList",
        title: "Panel",
        header: false,
        padding: {
            top: 0,
            right: 0,
            bottom: 0,
            left: 0
        },
        loadMethod: "xhr",
        contentURL: "views/transferlist.html",
        onContentLoaded: () => {
            handleDownloadParam();
            updateMainData();
        },
        column: "mainColumn",
        onResize: window.qBittorrent.Misc.createDebounceHandler(500, (e) => {
            saveColumnSizes();
        }),
        height: null
    });
    let prop_h = LocalPreferences.get("properties_height_rel");
    if (prop_h !== null)
        prop_h = prop_h.toFloat() * Window.getSize().y;
    else
        prop_h = Window.getSize().y / 2.0;
    new MochaUI.Panel({
        id: "propertiesPanel",
        title: "Panel",
        padding: {
            top: 0,
            right: 0,
            bottom: 0,
            left: 0
        },
        contentURL: "views/properties.html",
        require: {
            js: ["scripts/prop-general.js", "scripts/prop-trackers.js", "scripts/prop-peers.js", "scripts/prop-webseeds.js", "scripts/prop-files.js"],
            onload: () => {
                updatePropertiesPanel = () => {
                    switch (LocalPreferences.get("selected_properties_tab")) {
                        case "propGeneralLink":
                            window.qBittorrent.PropGeneral.updateData();
                            break;
                        case "propTrackersLink":
                            window.qBittorrent.PropTrackers.updateData();
                            break;
                        case "propPeersLink":
                            window.qBittorrent.PropPeers.updateData();
                            break;
                        case "propWebSeedsLink":
                            window.qBittorrent.PropWebseeds.updateData();
                            break;
                        case "propFilesLink":
                            window.qBittorrent.PropFiles.updateData();
                            break;
                    }
                };
            }
        },
        tabsURL: "views/propertiesToolbar.html",
        tabsOnload: () => {}, // must be included, otherwise panel won't load properly
        onContentLoaded: function() {
            this.panelHeaderCollapseBoxEl.classList.add("invisible");

            const togglePropertiesPanel = () => {
                this.collapseToggleEl.click();
                LocalPreferences.set("properties_panel_collapsed", this.isCollapsed.toString());
            };

            const selectTab = (tabID) => {
                const isAlreadySelected = this.panelHeaderEl.getElementById(tabID).classList.contains("selected");
                if (!isAlreadySelected) {
                    for (const tab of this.panelHeaderEl.getElementById("propertiesTabs").children)
                        tab.classList.toggle("selected", tab.id === tabID);

                    const tabContentID = tabID.replace("Link", "");
                    for (const tabContent of this.contentEl.children)
                        tabContent.classList.toggle("invisible", tabContent.id !== tabContentID);

                    LocalPreferences.set("selected_properties_tab", tabID);
                }

                if (isAlreadySelected || this.isCollapsed)
                    togglePropertiesPanel();
            };

            const lastUsedTab = LocalPreferences.get("selected_properties_tab", "propGeneralLink");
            selectTab(lastUsedTab);

            const startCollapsed = LocalPreferences.get("properties_panel_collapsed", "false") === "true";
            if (startCollapsed)
                togglePropertiesPanel();

            this.panelHeaderContentEl.addEventListener("click", (e) => {
                const selectedTab = e.target.closest("li");
                if (!selectedTab)
                    return;

                selectTab(selectedTab.id);
                updatePropertiesPanel();

                const showFilesFilter = (selectedTab.id === "propFilesLink") && !this.isCollapsed;
                document.getElementById("torrentFilesFilterToolbar").classList.toggle("invisible", !showFilesFilter);
            });

            const showFilesFilter = (lastUsedTab === "propFilesLink") && !this.isCollapsed;
            if (showFilesFilter)
                document.getElementById("torrentFilesFilterToolbar").classList.remove("invisible");
        },
        column: "mainColumn",
        height: prop_h
    });

    // listen for changes to torrentsFilterInput
    let torrentsFilterInputTimer = -1;
    $("torrentsFilterInput").addEventListener("input", () => {
        clearTimeout(torrentsFilterInputTimer);
        torrentsFilterInputTimer = setTimeout(() => {
            torrentsFilterInputTimer = -1;
            torrentsTable.updateTable();
        }, window.qBittorrent.Misc.FILTER_INPUT_DELAY);
    });

    document.getElementById("torrentsFilterToolbar").addEventListener("change", (e) => { torrentsTable.updateTable(); });

    $("transfersTabLink").addEventListener("click", () => { showTransfersTab(); });
    $("searchTabLink").addEventListener("click", () => { showSearchTab(); });
    $("rssTabLink").addEventListener("click", () => { showRssTab(); });
    $("logTabLink").addEventListener("click", () => { showLogTab(); });
    updateTabDisplay();

    const registerDragAndDrop = () => {
        $("desktop").addEventListener("dragover", (ev) => {
            if (ev.preventDefault)
                ev.preventDefault();
        });

        $("desktop").addEventListener("dragenter", (ev) => {
            if (ev.preventDefault)
                ev.preventDefault();
        });

        $("desktop").addEventListener("drop", (ev) => {
            if (ev.preventDefault)
                ev.preventDefault();

            const droppedFiles = ev.dataTransfer.files;

            if (droppedFiles.length > 0) {
                // dropped files or folders

                // can't handle folder due to cannot put the filelist (from dropped folder)
                // to <input> `files` field
                for (const item of ev.dataTransfer.items) {
                    if (item.webkitGetAsEntry().isDirectory)
                        return;
                }

                const id = "uploadPage";
                new MochaUI.Window({
                    id: id,
                    icon: "images/qbittorrent-tray.svg",
                    title: "QBT_TR(Upload local torrent)QBT_TR[CONTEXT=HttpServer]",
                    loadMethod: "iframe",
                    contentURL: new URI("upload.html").toString(),
                    addClass: "windowFrame", // fixes iframe scrolling on iOS Safari
                    scrollbars: true,
                    maximizable: false,
                    paddingVertical: 0,
                    paddingHorizontal: 0,
                    width: loadWindowWidth(id, 500),
                    height: loadWindowHeight(id, 460),
                    onResize: window.qBittorrent.Misc.createDebounceHandler(500, (e) => {
                        saveWindowSize(id);
                    }),
                    onContentLoaded: () => {
                        const fileInput = $(`${id}_iframe`).contentDocument.getElementById("fileselect");
                        fileInput.files = droppedFiles;
                    }
                });
            }

            const droppedText = ev.dataTransfer.getData("text");
            if (droppedText.length > 0) {
                // dropped text

                const urls = droppedText.split("\n")
                    .map((str) => str.trim())
                    .filter((str) => {
                        const lowercaseStr = str.toLowerCase();
                        return lowercaseStr.startsWith("http:")
                            || lowercaseStr.startsWith("https:")
                            || lowercaseStr.startsWith("magnet:")
                            || ((str.length === 40) && !(/[^0-9A-F]/i.test(str))) // v1 hex-encoded SHA-1 info-hash
                            || ((str.length === 32) && !(/[^2-7A-Z]/i.test(str))); // v1 Base32 encoded SHA-1 info-hash
                    });

                if (urls.length <= 0)
                    return;

                const id = "downloadPage";
                const contentURI = new URI("download.html").setData("urls", urls.map(encodeURIComponent).join("|"));
                new MochaUI.Window({
                    id: id,
                    icon: "images/qbittorrent-tray.svg",
                    title: "QBT_TR(Download from URLs)QBT_TR[CONTEXT=downloadFromURL]",
                    loadMethod: "iframe",
                    contentURL: contentURI.toString(),
                    addClass: "windowFrame", // fixes iframe scrolling on iOS Safari
                    scrollbars: true,
                    maximizable: false,
                    closable: true,
                    paddingVertical: 0,
                    paddingHorizontal: 0,
                    width: loadWindowWidth(id, 500),
                    height: loadWindowHeight(id, 600),
                    onResize: window.qBittorrent.Misc.createDebounceHandler(500, (e) => {
                        saveWindowSize(id);
                    })
                });
            }
        });
    };
    registerDragAndDrop();

    new Keyboard({
        defaultEventType: "keydown",
        events: {
            "ctrl+a": function(event) {
                if ((event.target.nodeName === "INPUT") || (event.target.nodeName === "TEXTAREA"))
                    return;
                if (event.target.isContentEditable)
                    return;
                torrentsTable.selectAll();
                event.preventDefault();
            },
            "delete": function(event) {
                if ((event.target.nodeName === "INPUT") || (event.target.nodeName === "TEXTAREA"))
                    return;
                if (event.target.isContentEditable)
                    return;
                deleteSelectedTorrentsFN();
                event.preventDefault();
            },
            "shift+delete": (event) => {
                if ((event.target.nodeName === "INPUT") || (event.target.nodeName === "TEXTAREA"))
                    return;
                if (event.target.isContentEditable)
                    return;
                deleteSelectedTorrentsFN(true);
                event.preventDefault();
            }
        }
    }).activate();

    new ClipboardJS(".copyToClipboard", {
        text: (trigger) => {
            switch (trigger.id) {
                case "copyName":
                    return copyNameFN();
                case "copyInfohash1":
                    return copyInfohashFN(1);
                case "copyInfohash2":
                    return copyInfohashFN(2);
                case "copyMagnetLink":
                    return copyMagnetLinkFN();
                case "copyID":
                    return copyIdFN();
                case "copyComment":
                    return copyCommentFN();
                default:
                    return "";
            }
        }
    });
});

window.addEventListener("load", () => {
    // switch to previously used tab
    const previouslyUsedTab = LocalPreferences.get("selected_window_tab", "transfers");
    switch (previouslyUsedTab) {
        case "search":
            if (window.qBittorrent.Client.isShowSearchEngine())
                $("searchTabLink").click();
            break;
        case "rss":
            if (window.qBittorrent.Client.isShowRssReader())
                $("rssTabLink").click();
            break;
        case "log":
            if (window.qBittorrent.Client.isShowLogViewer())
                $("logTabLink").click();
            break;
        case "transfers":
            $("transfersTabLink").click();
            break;
        default:
            console.error(`Unexpected 'selected_window_tab' value: ${previouslyUsedTab}`);
            $("transfersTabLink").click();
            break;
    };
});
