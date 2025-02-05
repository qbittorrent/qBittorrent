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
            initializeCaches: initializeCaches,
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

    let cacheAllSettled;
    const setup = () => {
        // fetch various data and store it in memory
        cacheAllSettled = Promise.allSettled([
            window.qBittorrent.Cache.buildInfo.init(),
            window.qBittorrent.Cache.preferences.init(),
            window.qBittorrent.Cache.qbtVersion.init()
        ]);
    };

    const initializeCaches = async () => {
        const results = await cacheAllSettled;
        for (const [idx, result] of results.entries()) {
            if (result.status === "rejected")
                console.error(`Failed to initialize cache. Index: ${idx}. Reason: "${result.reason}".`);
        }
    };

    const closeWindow = (window) => {
        MochaUI.closeWindow(window);
    };

    const closeFrameWindow = (window) => {
        MochaUI.closeWindow(window.frameElement.closest("div.mocha"));
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
        let suffix = window.qBittorrent.Cache.preferences.get()["app_instance_name"] || "";
        if (suffix.length > 0)
            suffix = ` ${emDash} ${suffix}`;
        const title = `qBittorrent ${qbtVersion} QBT_TR(WebUI)QBT_TR[CONTEXT=OptionsDialog]${suffix}`;
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
const CATEGORIES_ALL = "b4af0e4c-e76d-4bac-a392-46cbc18d9655";
const CATEGORIES_UNCATEGORIZED = "e24bd469-ea22-404c-8e2e-a17c82f37ea0";

// Map<category: String, {savePath: String, torrents: Set}>
const categoryMap = new Map();

let selectedCategory = LocalPreferences.get("selected_category", CATEGORIES_ALL);
let setCategoryFilter = () => {};

/* Tags filter */
const TAGS_ALL = "b4af0e4c-e76d-4bac-a392-46cbc18d9655";
const TAGS_UNTAGGED = "e24bd469-ea22-404c-8e2e-a17c82f37ea0";

// Map<tag: String, torrents: Set>
const tagMap = new Map();

let selectedTag = LocalPreferences.get("selected_tag", TAGS_ALL);
let setTagFilter = () => {};

/* Trackers filter */
const TRACKERS_ALL = "b4af0e4c-e76d-4bac-a392-46cbc18d9655";
const TRACKERS_TRACKERLESS = "e24bd469-ea22-404c-8e2e-a17c82f37ea0";

// Map<trackerHost: String, Map<trackerURL: String, torrents: Set>>
const trackerMap = new Map();

let selectedTracker = LocalPreferences.get("selected_tracker", TRACKERS_ALL);
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
        if (!$("mainColumn").classList.contains("invisible"))
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
        $("searchTabColumn").classList.add("invisible");
    };

    const buildRssTab = () => {
        new MochaUI.Column({
            id: "rssTabColumn",
            placement: "main",
            width: null
        });

        // start off hidden
        $("rssTabColumn").classList.add("invisible");
    };

    const buildLogTab = () => {
        new MochaUI.Column({
            id: "logTabColumn",
            placement: "main",
            width: null
        });

        // start off hidden
        $("logTabColumn").classList.add("invisible");
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

    setCategoryFilter = (category) => {
        const currentHash = torrentsTable.getCurrentTorrentID();

        LocalPreferences.set("selected_category", category);
        selectedCategory = category;
        highlightSelectedCategory();
        updateMainData();

        const newHash = torrentsTable.getCurrentTorrentID();
        handleFilterSelectionChange(currentHash, newHash);
    };

    setTagFilter = (tag) => {
        const currentHash = torrentsTable.getCurrentTorrentID();

        LocalPreferences.set("selected_tag", tag);
        selectedTag = tag;
        highlightSelectedTag();
        updateMainData();

        const newHash = torrentsTable.getCurrentTorrentID();
        handleFilterSelectionChange(currentHash, newHash);
    };

    setTrackerFilter = (tracker) => {
        const currentHash = torrentsTable.getCurrentTorrentID();

        LocalPreferences.set("selected_tracker", tracker);
        selectedTracker = tracker;
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
        $("showTopToolbarLink").firstElementChild.style.opacity = "0";
        $("mochaToolbar").classList.add("invisible");
    }

    // Show Status Bar is enabled by default
    let showStatusBar = LocalPreferences.get("show_status_bar", "true") === "true";
    if (!showStatusBar) {
        $("showStatusBarLink").firstElementChild.style.opacity = "0";
        $("desktopFooterWrapper").classList.add("invisible");
    }

    // Show Filters Sidebar is enabled by default
    let showFiltersSidebar = LocalPreferences.get("show_filters_sidebar", "true") === "true";
    if (!showFiltersSidebar) {
        $("showFiltersSidebarLink").firstElementChild.style.opacity = "0";
        $("filtersColumn").classList.add("invisible");
        $("filtersColumn_handle").classList.add("invisible");
    }

    let speedInTitle = LocalPreferences.get("speed_in_browser_title_bar") === "true";
    if (!speedInTitle)
        $("speedInBrowserTitleBarLink").firstElementChild.style.opacity = "0";

    // After showing/hiding the toolbar + status bar
    window.qBittorrent.Client.showSearchEngine(LocalPreferences.get("show_search_engine") !== "false");
    window.qBittorrent.Client.showRssReader(LocalPreferences.get("show_rss_reader") !== "false");
    window.qBittorrent.Client.showLogViewer(LocalPreferences.get("show_log_viewer") === "true");

    // After Show Top Toolbar
    MochaUI.Desktop.setDesktopSize();

    let syncMainDataLastResponseId = 0;
    const serverState = {};

    const removeTorrentFromCategoryList = (hash) => {
        if (hash === undefined)
            return false;

        let removed = false;
        for (const data of categoryMap.values())
            removed ||= data.torrents.delete(hash);
        return removed;
    };

    const addTorrentToCategoryList = (torrent) => {
        const category = torrent["category"];
        if (category === undefined)
            return false;

        const hash = torrent["hash"];
        if (category.length === 0) { // Empty category
            removeTorrentFromCategoryList(hash);
            return true;
        }

        let categoryData = categoryMap.get(category);
        if (categoryData === undefined) { // This should not happen
            categoryData = {
                torrents: new Set()
            };
            categoryMap.set(category, categoryData);
        }

        if (categoryData.torrents.has(hash))
            return false;

        removeTorrentFromCategoryList(hash);
        categoryData.torrents.add(hash);
        return true;
    };

    const removeTorrentFromTagList = (hash) => {
        if (hash === undefined)
            return false;

        let removed = false;
        for (const torrents of tagMap.values())
            removed ||= torrents.delete(hash);
        return removed;
    };

    const addTorrentToTagList = (torrent) => {
        if (torrent["tags"] === undefined) // Tags haven't changed
            return false;

        const hash = torrent["hash"];
        removeTorrentFromTagList(hash);

        if (torrent["tags"].length === 0) // No tags
            return true;

        const tags = torrent["tags"].split(", ");
        let added = false;
        for (const tag of tags) {
            let torrents = tagMap.get(tag);
            if (torrents === undefined) { // This should not happen
                torrents = new Set();
                tagMap.set(tag, torrents);
            }

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
        filterEl.firstElementChild.lastChild.textContent = filterTitle.replace("%1", filterTorrentCount);
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

        [...categoryList.children].forEach((el) => { el.destroy(); });

        const categoryItemTemplate = document.getElementById("categoryFilterItem");

        const createLink = (category, text, count) => {
            const categoryFilterItem = categoryItemTemplate.content.cloneNode(true).firstElementChild;
            categoryFilterItem.id = category;
            categoryFilterItem.classList.toggle("selectedFilter", (category === selectedCategory));

            const span = categoryFilterItem.firstElementChild;
            span.lastElementChild.textContent = `${text} (${count})`;

            return categoryFilterItem;
        };

        const createCategoryTree = (category) => {
            const stack = [{ parent: categoriesFragment, category: category }];
            while (stack.length > 0) {
                const { parent, category } = stack.pop();
                const displayName = category.nameSegments.at(-1);
                const listItem = createLink(category.categoryName, displayName, category.categoryCount);
                listItem.firstElementChild.style.paddingLeft = `${(category.nameSegments.length - 1) * 20 + 6}px`;

                parent.appendChild(listItem);

                if (category.children.length > 0) {
                    listItem.querySelector(".categoryToggle").style.visibility = "visible";
                    const unorderedList = document.createElement("ul");
                    listItem.appendChild(unorderedList);
                    for (const subcategory of category.children.reverse())
                        stack.push({ parent: unorderedList, category: subcategory });
                }
                const categoryLocalPref = `category_${category.categoryName}_collapsed`;
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
        for (const [category, categoryData] of categoryMap) {
            sortedCategories.push({
                categoryName: category,
                categoryCount: categoryData.torrents.size,
                nameSegments: category.split("/"),
                ...(useSubcategories && {
                    children: [],
                    isRoot: true,
                    forceExpand: LocalPreferences.get(`category_${category}_collapsed`) === null
                })
            });
        }
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
        categoriesFragment.appendChild(createLink(CATEGORIES_ALL, "QBT_TR(All)QBT_TR[CONTEXT=CategoryFilterModel]", torrentsTable.getRowSize()));
        categoriesFragment.appendChild(createLink(CATEGORIES_UNCATEGORIZED, "QBT_TR(Uncategorized)QBT_TR[CONTEXT=CategoryFilterModel]", uncategorized));

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
                        subcategory.isRoot = false;
                        category.children.push(subcategory);
                    }
                }
            }
            for (const category of sortedCategories) {
                if (category.isRoot)
                    createCategoryTree(category);
            }
        }
        else {
            categoryList.classList.remove("subcategories");
            for (const { categoryName, categoryCount } of sortedCategories)
                categoriesFragment.appendChild(createLink(categoryName, categoryName, categoryCount));
        }

        categoryList.appendChild(categoriesFragment);
        window.qBittorrent.Filters.categoriesFilterContextMenu.searchAndAddTargets();
    };

    const highlightSelectedCategory = () => {
        const categoryList = document.getElementById("categoryFilterList");
        if (!categoryList)
            return;

        for (const category of categoryList.getElementsByTagName("li"))
            category.classList.toggle("selectedFilter", (category.id === selectedCategory));
    };

    const updateTagList = () => {
        const tagFilterList = $("tagFilterList");
        if (tagFilterList === null)
            return;

        [...tagFilterList.children].forEach((el) => { el.destroy(); });

        const tagItemTemplate = document.getElementById("tagFilterItem");

        const createLink = (tag, text, count) => {
            const tagFilterItem = tagItemTemplate.content.cloneNode(true).firstElementChild;
            tagFilterItem.id = tag;
            tagFilterItem.classList.toggle("selectedFilter", (tag === selectedTag));

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
        for (const [tag, torrents] of tagMap) {
            sortedTags.push({
                tagName: tag,
                tagSize: torrents.size
            });
        }
        sortedTags.sort((left, right) => window.qBittorrent.Misc.naturalSortCollator.compare(left.tagName, right.tagName));

        for (const { tagName, tagSize } of sortedTags)
            tagFilterList.appendChild(createLink(tagName, tagName, tagSize));

        window.qBittorrent.Filters.tagsFilterContextMenu.searchAndAddTargets();
    };

    const highlightSelectedTag = () => {
        const tagFilterList = document.getElementById("tagFilterList");
        if (!tagFilterList)
            return;

        for (const tag of tagFilterList.children)
            tag.classList.toggle("selectedFilter", (tag.id === selectedTag));
    };

    const updateTrackerList = () => {
        const trackerFilterList = $("trackerFilterList");
        if (trackerFilterList === null)
            return;

        [...trackerFilterList.children].forEach((el) => { el.destroy(); });

        const trackerItemTemplate = document.getElementById("trackerFilterItem");

        const createLink = (host, text, count) => {
            const trackerFilterItem = trackerItemTemplate.content.cloneNode(true).firstElementChild;
            trackerFilterItem.id = host;
            trackerFilterItem.classList.toggle("selectedFilter", (host === selectedTracker));

            const span = trackerFilterItem.firstElementChild;
            span.lastChild.textContent = `${text} (${count})`;

            return trackerFilterItem;
        };

        let trackerlessTorrentsCount = 0;
        for (const { full_data: { trackers_count: trackersCount } } of torrentsTable.getRowValues()) {
            if (trackersCount === 0)
                trackerlessTorrentsCount += 1;
        }

        trackerFilterList.appendChild(createLink(TRACKERS_ALL, "QBT_TR(All)QBT_TR[CONTEXT=TrackerFiltersList]", torrentsTable.getRowSize()));
        trackerFilterList.appendChild(createLink(TRACKERS_TRACKERLESS, "QBT_TR(Trackerless)QBT_TR[CONTEXT=TrackerFiltersList]", trackerlessTorrentsCount));

        // Sort trackers by hostname
        const sortedList = [];
        for (const [host, trackerTorrentMap] of trackerMap) {
            const uniqueTorrents = new Set();
            for (const torrents of trackerTorrentMap.values()) {
                for (const torrent of torrents)
                    uniqueTorrents.add(torrent);
            }

            sortedList.push({
                trackerHost: host,
                trackerCount: uniqueTorrents.size,
            });
        }
        sortedList.sort((left, right) => window.qBittorrent.Misc.naturalSortCollator.compare(left.trackerHost, right.trackerHost));
        for (const { trackerHost, trackerCount } of sortedList)
            trackerFilterList.appendChild(createLink(trackerHost, trackerHost, trackerCount));

        window.qBittorrent.Filters.trackersFilterContextMenu.searchAndAddTargets();
    };

    const highlightSelectedTracker = () => {
        const trackerFilterList = document.getElementById("trackerFilterList");
        if (!trackerFilterList)
            return;

        for (const tracker of trackerFilterList.children)
            tracker.classList.toggle("selectedFilter", (tracker.id === selectedTracker));
    };

    const statusSortOrder = Object.freeze({
        unknown: -1,
        forcedDL: 0,
        downloading: 1,
        forcedMetaDL: 2,
        metaDL: 3,
        stalledDL: 4,
        forcedUP: 5,
        uploading: 6,
        stalledUP: 7,
        checkingResumeData: 8,
        queuedDL: 9,
        queuedUP: 10,
        checkingUP: 11,
        checkingDL: 12,
        stoppedDL: 13,
        stoppedUP: 14,
        moving: 15,
        missingFiles: 16,
        error: 17
    });

    let syncMainDataTimeoutID = -1;
    let syncRequestInProgress = false;
    const syncMainData = () => {
        syncRequestInProgress = true;
        const url = new URL("api/v2/sync/maindata", window.location);
        url.search = new URLSearchParams({
            rid: syncMainDataLastResponseId
        });
        fetch(url, {
                method: "GET",
                cache: "no-store"
            })
            .then(async (response) => {
                    if (response.ok) {
                        $("error_div").textContent = "";

                        const responseJSON = await response.json();

                        clearTimeout(torrentsFilterInputTimer);
                        torrentsFilterInputTimer = -1;

                        let torrentsTableSelectedRows;
                        let updateStatuses = false;
                        let updateCategories = false;
                        let updateTags = false;
                        let updateTrackers = false;
                        let updateTorrents = false;
                        const fullUpdate = (responseJSON["fullUpdate"] === true);
                        if (fullUpdate) {
                            torrentsTableSelectedRows = torrentsTable.selectedRowsIds();
                            updateStatuses = true;
                            updateCategories = true;
                            updateTags = true;
                            updateTrackers = true;
                            updateTorrents = true;
                            torrentsTable.clear();
                            categoryMap.clear();
                            tagMap.clear();
                            trackerMap.clear();
                        }
                        if (responseJSON["rid"])
                            syncMainDataLastResponseId = responseJSON["rid"];
                        if (responseJSON["categories"]) {
                            for (const responseName in responseJSON["categories"]) {
                                if (!Object.hasOwn(responseJSON["categories"], responseName))
                                    continue;

                                const responseData = responseJSON["categories"][responseName];
                                const categoryData = categoryMap.get(responseName);
                                if (categoryData === undefined) {
                                    categoryMap.set(responseName, {
                                        savePath: responseData.savePath,
                                        torrents: new Set()
                                    });
                                }
                                else {
                                    // only the save path can change for existing categories
                                    categoryData.savePath = responseData.savePath;
                                }
                            }
                            updateCategories = true;
                        }
                        if (responseJSON["categories_removed"]) {
                            for (const category of responseJSON["categories_removed"])
                                categoryMap.delete(category);
                            updateCategories = true;
                        }
                        if (responseJSON["tags"]) {
                            for (const tag of responseJSON["tags"]) {
                                if (!tagMap.has(tag))
                                    tagMap.set(tag, new Set());
                            }
                            updateTags = true;
                        }
                        if (responseJSON["tags_removed"]) {
                            for (const tag of responseJSON["tags_removed"])
                                tagMap.delete(tag);
                            updateTags = true;
                        }
                        if (responseJSON["trackers"]) {
                            for (const [tracker, torrents] of Object.entries(responseJSON["trackers"])) {
                                const host = window.qBittorrent.Misc.getHost(tracker);

                                let trackerListItem = trackerMap.get(host);
                                if (trackerListItem === undefined) {
                                    trackerListItem = new Map();
                                    trackerMap.set(host, trackerListItem);
                                }
                                trackerListItem.set(tracker, new Set(torrents));
                            }
                            updateTrackers = true;
                        }
                        if (responseJSON["trackers_removed"]) {
                            for (let i = 0; i < responseJSON["trackers_removed"].length; ++i) {
                                const tracker = responseJSON["trackers_removed"][i];
                                const host = window.qBittorrent.Misc.getHost(tracker);

                                const trackerTorrentMap = trackerMap.get(host);
                                if (trackerTorrentMap !== undefined) {
                                    trackerTorrentMap.delete(tracker);
                                    // Remove unused trackers
                                    if (trackerTorrentMap.size === 0) {
                                        trackerMap.delete(host);
                                        if (selectedTracker === host) {
                                            selectedTracker = TRACKERS_ALL;
                                            LocalPreferences.set("selected_tracker", selectedTracker);
                                        }
                                    }
                                }
                            }
                            updateTrackers = true;
                        }
                        if (responseJSON["torrents"]) {
                            for (const key in responseJSON["torrents"]) {
                                if (!Object.hasOwn(responseJSON["torrents"], key))
                                    continue;

                                responseJSON["torrents"][key]["hash"] = key;
                                responseJSON["torrents"][key]["rowId"] = key;
                                if (responseJSON["torrents"][key]["state"]) {
                                    const state = responseJSON["torrents"][key]["state"];
                                    responseJSON["torrents"][key]["status"] = state;
                                    responseJSON["torrents"][key]["_statusOrder"] = statusSortOrder[state];
                                    updateStatuses = true;
                                }
                                torrentsTable.updateRowData(responseJSON["torrents"][key]);
                                if (addTorrentToCategoryList(responseJSON["torrents"][key]))
                                    updateCategories = true;
                                if (addTorrentToTagList(responseJSON["torrents"][key]))
                                    updateTags = true;
                                updateTrackers = true;
                                updateTorrents = true;
                            }
                        }
                        if (responseJSON["torrents_removed"]) {
                            responseJSON["torrents_removed"].each((hash) => {
                                torrentsTable.removeRow(hash);
                                removeTorrentFromCategoryList(hash);
                                updateCategories = true; // Always to update All category
                                removeTorrentFromTagList(hash);
                                updateTags = true; // Always to update All tag
                                updateTrackers = true;
                            });
                            updateTorrents = true;
                            updateStatuses = true;
                        }

                        // don't update the table unnecessarily
                        if (updateTorrents)
                            torrentsTable.updateTable(fullUpdate);

                        if (responseJSON["server_state"]) {
                            const tmp = responseJSON["server_state"];
                            for (const k in tmp) {
                                if (!Object.hasOwn(tmp, k))
                                    continue;
                                serverState[k] = tmp[k];
                            }
                            processServerState();
                        }

                        if (updateStatuses)
                            updateFiltersList();

                        if (updateCategories) {
                            updateCategoryList();
                            window.qBittorrent.TransferList.contextMenu.updateCategoriesSubMenu(categoryMap);
                        }
                        if (updateTags) {
                            updateTagList();
                            window.qBittorrent.TransferList.contextMenu.updateTagsSubMenu(tagMap);
                        }
                        if (updateTrackers)
                            updateTrackerList();

                        if (fullUpdate)
                            // re-select previously selected rows
                            torrentsTable.reselectRows(torrentsTableSelectedRows);
                    }

                    syncRequestInProgress = false;
                    syncData(window.qBittorrent.Client.getSyncMainDataInterval());
                },
                (error) => {
                    const errorDiv = $("error_div");
                    if (errorDiv)
                        errorDiv.textContent = "QBT_TR(qBittorrent client is not reachable)QBT_TR[CONTEXT=HttpServer]";
                    syncRequestInProgress = false;
                    syncData(2000);
                });
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
            transfer_info += ` [${window.qBittorrent.Misc.friendlyUnit(serverState.dl_rate_limit, true)}]`;
        transfer_info += ` (${window.qBittorrent.Misc.friendlyUnit(serverState.dl_info_data, false)})`;
        $("DlInfos").textContent = transfer_info;
        transfer_info = window.qBittorrent.Misc.friendlyUnit(serverState.up_info_speed, true);
        if (serverState.up_rate_limit > 0)
            transfer_info += ` [${window.qBittorrent.Misc.friendlyUnit(serverState.up_rate_limit, true)}]`;
        transfer_info += ` (${window.qBittorrent.Misc.friendlyUnit(serverState.up_info_data, false)})`;
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
            $("ReadCacheHits").textContent = `${serverState.read_cache_hits}%`;
            $("TotalBuffersSize").textContent = window.qBittorrent.Misc.friendlyUnit(serverState.total_buffers_size, false);
            $("WriteCacheOverload").textContent = `${serverState.write_cache_overload}%`;
            $("ReadCacheOverload").textContent = `${serverState.read_cache_overload}%`;
            $("QueuedIOJobs").textContent = serverState.queued_io_jobs;
            $("AverageTimeInQueue").textContent = `${serverState.average_time_queue} ms`;
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
                $("topQueuePosItem").classList.remove("invisible");
                $("increaseQueuePosItem").classList.remove("invisible");
                $("decreaseQueuePosItem").classList.remove("invisible");
                $("bottomQueuePosItem").classList.remove("invisible");
                $("queueingButtons").classList.remove("invisible");
                $("queueingMenuItems").classList.remove("invisible");
            }
            else {
                $("topQueuePosItem").classList.add("invisible");
                $("increaseQueuePosItem").classList.add("invisible");
                $("decreaseQueuePosItem").classList.add("invisible");
                $("bottomQueuePosItem").classList.add("invisible");
                $("queueingButtons").classList.add("invisible");
                $("queueingMenuItems").classList.add("invisible");
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

        fetch("api/v2/transfer/toggleSpeedLimitsMode", {
                method: "POST"
            })
            .then((response) => {
                if (!response.ok) {
                    // Restore icon in case of failure
                    updateAltSpeedIcon(alternativeSpeedLimits);
                    return;
                }

                alternativeSpeedLimits = !alternativeSpeedLimits;
                updateMainData();
            });
    });

    $("DlInfos").addEventListener("click", () => { globalDownloadLimitFN(); });
    $("UpInfos").addEventListener("click", () => { globalUploadLimitFN(); });

    $("showTopToolbarLink").addEventListener("click", (e) => {
        showTopToolbar = !showTopToolbar;
        LocalPreferences.set("show_top_toolbar", showTopToolbar.toString());
        if (showTopToolbar) {
            $("showTopToolbarLink").firstElementChild.style.opacity = "1";
            $("mochaToolbar").classList.remove("invisible");
        }
        else {
            $("showTopToolbarLink").firstElementChild.style.opacity = "0";
            $("mochaToolbar").classList.add("invisible");
        }
        MochaUI.Desktop.setDesktopSize();
    });

    $("showStatusBarLink").addEventListener("click", (e) => {
        showStatusBar = !showStatusBar;
        LocalPreferences.set("show_status_bar", showStatusBar.toString());
        if (showStatusBar) {
            $("showStatusBarLink").firstElementChild.style.opacity = "1";
            $("desktopFooterWrapper").classList.remove("invisible");
        }
        else {
            $("showStatusBarLink").firstElementChild.style.opacity = "0";
            $("desktopFooterWrapper").classList.add("invisible");
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
        const templateUrl = `${location.origin}${location.pathname}${location.search}#${templateHashString}`;

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
            $("showFiltersSidebarLink").firstElementChild.style.opacity = "1";
            $("filtersColumn").classList.remove("invisible");
            $("filtersColumn_handle").classList.remove("invisible");
        }
        else {
            $("showFiltersSidebarLink").firstElementChild.style.opacity = "0";
            $("filtersColumn").classList.add("invisible");
            $("filtersColumn_handle").classList.add("invisible");
        }
        MochaUI.Desktop.setDesktopSize();
    });

    $("speedInBrowserTitleBarLink").addEventListener("click", (e) => {
        speedInTitle = !speedInTitle;
        LocalPreferences.set("speed_in_browser_title_bar", speedInTitle.toString());
        if (speedInTitle)
            $("speedInBrowserTitleBarLink").firstElementChild.style.opacity = "1";
        else
            $("speedInBrowserTitleBarLink").firstElementChild.style.opacity = "0";
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
            $("showRssReaderLink").firstElementChild.style.opacity = "1";
            $("mainWindowTabs").classList.remove("invisible");
            $("rssTabLink").classList.remove("invisible");
            if (!MochaUI.Panels.instances.RssPanel)
                addRssPanel();
        }
        else {
            $("showRssReaderLink").firstElementChild.style.opacity = "0";
            $("rssTabLink").classList.add("invisible");
            if ($("rssTabLink").classList.contains("selected"))
                $("transfersTabLink").click();
        }

        if (window.qBittorrent.Client.isShowSearchEngine()) {
            $("showSearchEngineLink").firstElementChild.style.opacity = "1";
            $("mainWindowTabs").classList.remove("invisible");
            $("searchTabLink").classList.remove("invisible");
            if (!MochaUI.Panels.instances.SearchPanel)
                addSearchPanel();
        }
        else {
            $("showSearchEngineLink").firstElementChild.style.opacity = "0";
            $("searchTabLink").classList.add("invisible");
            if ($("searchTabLink").classList.contains("selected"))
                $("transfersTabLink").click();
        }

        if (window.qBittorrent.Client.isShowLogViewer()) {
            $("showLogViewerLink").firstElementChild.style.opacity = "1";
            $("mainWindowTabs").classList.remove("invisible");
            $("logTabLink").classList.remove("invisible");
            if (!MochaUI.Panels.instances.LogPanel)
                addLogPanel();
        }
        else {
            $("showLogViewerLink").firstElementChild.style.opacity = "0";
            $("logTabLink").classList.add("invisible");
            if ($("logTabLink").classList.contains("selected"))
                $("transfersTabLink").click();
        }

        // display no tabs
        if (!window.qBittorrent.Client.isShowRssReader() && !window.qBittorrent.Client.isShowSearchEngine() && !window.qBittorrent.Client.isShowLogViewer())
            $("mainWindowTabs").classList.add("invisible");
    };

    $("StatisticsLink").addEventListener("click", () => { StatisticsLinkFN(); });

    // main window tabs

    const showTransfersTab = () => {
        const showFiltersSidebar = LocalPreferences.get("show_filters_sidebar", "true") === "true";
        if (showFiltersSidebar) {
            $("filtersColumn").classList.remove("invisible");
            $("filtersColumn_handle").classList.remove("invisible");
        }
        $("mainColumn").classList.remove("invisible");
        $("torrentsFilterToolbar").classList.remove("invisible");

        customSyncMainDataInterval = null;
        syncData(100);

        hideSearchTab();
        hideRssTab();
        hideLogTab();

        LocalPreferences.set("selected_window_tab", "transfers");
    };

    const hideTransfersTab = () => {
        $("filtersColumn").classList.add("invisible");
        $("filtersColumn_handle").classList.add("invisible");
        $("mainColumn").classList.add("invisible");
        $("torrentsFilterToolbar").classList.add("invisible");
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

            $("searchTabColumn").classList.remove("invisible");
            customSyncMainDataInterval = 30000;
            hideTransfersTab();
            hideRssTab();
            hideLogTab();

            LocalPreferences.set("selected_window_tab", "search");
        };
    })();

    const hideSearchTab = () => {
        $("searchTabColumn").classList.add("invisible");
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

            $("rssTabColumn").classList.remove("invisible");
            customSyncMainDataInterval = 30000;
            hideTransfersTab();
            hideSearchTab();
            hideLogTab();

            LocalPreferences.set("selected_window_tab", "rss");
        };
    })();

    const hideRssTab = () => {
        $("rssTabColumn").classList.add("invisible");
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

            $("logTabColumn").classList.remove("invisible");
            customSyncMainDataInterval = 30000;
            hideTransfersTab();
            hideSearchTab();
            hideRssTab();

            LocalPreferences.set("selected_window_tab", "log");
        };
    })();

    const hideLogTab = () => {
        $("logTabColumn").classList.add("invisible");
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
        if (!location.hash.startsWith(downloadHash))
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
        prop_h = Number(prop_h) * Window.getSize().y;
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
                    contentURL: "upload.html",
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
                const contentURL = new URL("download.html", window.location);
                contentURL.search = new URLSearchParams({
                    urls: urls.map(encodeURIComponent).join("|")
                });
                new MochaUI.Window({
                    id: id,
                    icon: "images/qbittorrent-tray.svg",
                    title: "QBT_TR(Download from URLs)QBT_TR[CONTEXT=downloadFromURL]",
                    loadMethod: "iframe",
                    contentURL: contentURL.toString(),
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

    window.addEventListener("keydown", (event) => {
        switch (event.key) {
            case "a":
            case "A":
                if (event.ctrlKey) {
                    if ((event.target.nodeName === "INPUT") || (event.target.nodeName === "TEXTAREA"))
                        return;
                    if (event.target.isContentEditable)
                        return;
                    event.preventDefault();
                    torrentsTable.selectAll();
                }
                break;

            case "Delete":
                if ((event.target.nodeName === "INPUT") || (event.target.nodeName === "TEXTAREA"))
                    return;
                if (event.target.isContentEditable)
                    return;
                event.preventDefault();
                deleteSelectedTorrentsFN(event.shiftKey);
                break;
        }
    });

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

window.addEventListener("load", async () => {
    await window.qBittorrent.Client.initializeCaches();

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
