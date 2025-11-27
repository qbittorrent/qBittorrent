/*
 * MIT License
 * Copyright (c) 2008 Ishan Arora <ishan@qbittorrent.org>
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
window.qBittorrent.AddTorrent ??= (() => {
    const exports = () => {
        return {
            changeCategorySelect: changeCategorySelect,
            changeTMM: changeTMM,
            metadataCompleted: metadataCompleted,
            populateMetadata: populateMetadata,
            setWindowId: setWindowId,
            submitForm: submitForm,
            init: init
        };
    };

    let table = null;
    let defaultSavePath = "";
    let defaultTempPath = "";
    let defaultTempPathEnabled = false;
    let windowId = "";
    let source = "";
    let downloader = "";

    const localPreferences = new window.qBittorrent.LocalPreferences.LocalPreferences();

    const getCategories = () => {
        const defaultCategory = localPreferences.get("add_torrent_default_category", "");
        const categorySelect = document.getElementById("categorySelect");
        for (const name of window.parent.qBittorrent.Client.categoryMap.keys()) {
            const option = document.createElement("option");
            option.value = name;
            option.textContent = name;
            option.selected = name === defaultCategory;
            categorySelect.appendChild(option);
        }

        if (defaultCategory !== "")
            changeCategorySelect(categorySelect);
    };

    const getTags = () => {
        const tagsSelect = document.getElementById("tagsSelect");
        for (const tag of window.parent.qBittorrent.Client.tagMap.keys()) {
            const option = document.createElement("option");
            option.value = tag;
            option.textContent = tag;
            tagsSelect.appendChild(option);
        }

        new vanillaSelectBox("#tagsSelect", {
            maxHeight: 200,
            search: false,
            disableSelectAll: true,
            translations: {
                all: (window.parent.qBittorrent.Client.tagMap.length === 0) ? "" : "QBT_TR(All)QBT_TR[CONTEXT=AddNewTorrentDialog]",
            },
            keepInlineStyles: false
        });
    };

    const getPreferences = () => {
        const pref = window.parent.qBittorrent.Cache.preferences.get();

        defaultSavePath = pref.save_path;
        defaultTempPath = pref.temp_path;
        defaultTempPathEnabled = pref.temp_path_enabled;
        document.getElementById("startTorrent").checked = !pref.add_stopped_enabled;
        document.getElementById("addToTopOfQueue").checked = pref.add_to_top_of_queue;

        const autoTMM = document.getElementById("autoTMM");
        if (pref.auto_tmm_enabled) {
            autoTMM.selectedIndex = 1;
            document.getElementById("savepath").disabled = true;
        }
        else {
            autoTMM.selectedIndex = 0;
        }
        changeTMM();

        if (pref.torrent_stop_condition === "MetadataReceived")
            document.getElementById("stopCondition").selectedIndex = 1;
        else if (pref.torrent_stop_condition === "FilesChecked")
            document.getElementById("stopCondition").selectedIndex = 2;
        else
            document.getElementById("stopCondition").selectedIndex = 0;

        if (pref.torrent_content_layout === "Subfolder")
            document.getElementById("contentLayout").selectedIndex = 1;
        else if (pref.torrent_content_layout === "NoSubfolder")
            document.getElementById("contentLayout").selectedIndex = 2;
        else
            document.getElementById("contentLayout").selectedIndex = 0;
    };

    const categorySavePath = (categoryName) => {
        const category = window.parent.qBittorrent.Client.categoryMap.get(categoryName);
        return (category === undefined) ? defaultSavePath : (category.savePath || `${defaultSavePath}/${categoryName}`);
    };

    const categoryDownloadPath = (categoryName) => {
        const category = window.parent.qBittorrent.Client.categoryMap.get(categoryName);
        if (category === undefined)
            return defaultTempPath;
        if (category.downloadPath === false)
            return "";
        return category.downloadPath || `${defaultTempPath}/${categoryName}`;
    };

    const categoryDownloadPathEnabled = (categoryName) => {
        const category = window.parent.qBittorrent.Client.categoryMap.get(categoryName);
        if ((category === undefined) || (category.downloadPath === null))
            return defaultTempPathEnabled;
        return category.downloadPath !== false;
    };

    const changeCategorySelect = (item) => {
        const categoryName = item.value;
        if (categoryName === "\\other") {
            item.nextElementSibling.hidden = false;
            item.nextElementSibling.value = "";
            item.nextElementSibling.select();

            if (isAutoTMMEnabled()) {
                document.getElementById("savepath").value = defaultSavePath;

                const downloadPathEnabled = categoryDownloadPathEnabled(categoryName);
                document.getElementById("useDownloadPath").checked = downloadPathEnabled;
                changeUseDownloadPath(downloadPathEnabled);
            }
        }
        else {
            item.nextElementSibling.hidden = true;
            const text = item.options[item.selectedIndex].textContent;
            item.nextElementSibling.value = text;

            if (isAutoTMMEnabled()) {
                document.getElementById("savepath").value = categorySavePath(categoryName);

                const downloadPathEnabled = categoryDownloadPathEnabled(categoryName);
                document.getElementById("useDownloadPath").checked = downloadPathEnabled;
                changeUseDownloadPath(downloadPathEnabled);
            }
        }
    };

    const changeTagsSelect = (element) => {
        const tags = [...element.options].filter(opt => opt.selected).map(opt => opt.value);
        document.getElementById("tags").value = tags.join(",");
    };

    const isAutoTMMEnabled = () => {
        return document.getElementById("autoTMM").selectedIndex === 1;
    };

    const changeTMM = () => {
        const autoTMMEnabled = isAutoTMMEnabled();
        const savepath = document.getElementById("savepath");
        const useDownloadPath = document.getElementById("useDownloadPath");

        if (autoTMMEnabled) {
            const categorySelect = document.getElementById("categorySelect");
            const categoryName = categorySelect.options[categorySelect.selectedIndex].value;
            savepath.value = categorySavePath(categoryName);
            useDownloadPath.checked = categoryDownloadPathEnabled(categoryName);
        }
        else {
            savepath.value = defaultSavePath;
            useDownloadPath.checked = defaultTempPathEnabled;
        }

        savepath.disabled = autoTMMEnabled;
        useDownloadPath.disabled = autoTMMEnabled;

        // only submit this value when using manual tmm
        document.getElementById("useDownloadPathHidden").disabled = autoTMMEnabled;

        changeUseDownloadPath(useDownloadPath.checked);
    };

    const changeUseDownloadPath = (enabled) => {
        const downloadPath = document.getElementById("downloadPath");
        if (isAutoTMMEnabled()) {
            const categorySelect = document.getElementById("categorySelect");
            const categoryName = categorySelect.options[categorySelect.selectedIndex].value;
            downloadPath.value = enabled ? categoryDownloadPath(categoryName) : "";
            downloadPath.disabled = true;
        }
        else {
            downloadPath.value = enabled ? defaultTempPath : "";
            downloadPath.disabled = !enabled;
        }
    };

    let loadMetadataTimer = -1;
    const loadMetadata = (sourceUrl = undefined, downloaderName = undefined) => {
        if (sourceUrl !== undefined)
            source = sourceUrl;
        if (downloaderName !== undefined)
            downloader = downloaderName;

        fetch("api/v2/torrents/fetchMetadata", {
                method: "POST",
                body: new URLSearchParams({
                    source: source,
                    downloader: downloader
                })
            })
            .then(async (response) => {
                if (!response.ok) {
                    metadataFailed();
                    return;
                }

                const data = await response.json();
                populateMetadata(data);

                if (response.status === 200)
                    metadataCompleted();
                else
                    loadMetadataTimer = loadMetadata.delay(1000);
            }, (error) => {
                console.error(error);

                loadMetadataTimer = loadMetadata.delay(1000);
            });
    };

    const metadataCompleted = (showDownloadButton = true) => {
        clearTimeout(loadMetadataTimer);
        loadMetadataTimer = -1;

        document.getElementById("metadataStatus").destroy();
        document.getElementById("loadingSpinner").style.display = "none";

        if (showDownloadButton)
            document.getElementById("saveTorrent").classList.remove("invisible");
    };

    const metadataFailed = () => {
        clearTimeout(loadMetadataTimer);
        loadMetadataTimer = -1;

        document.getElementById("metadataStatus").textContent = "Metadata retrieval failed";
        document.getElementById("metadataStatus").classList.add("red");
        document.getElementById("loadingSpinner").style.display = "none";
        document.getElementById("errorIcon").classList.remove("invisible");
    };

    const populateMetadata = (metadata) => {
        // update window title
        if (metadata.info?.name !== undefined)
            window.parent.document.getElementById(`${windowId}_title`).textContent = metadata.info.name;

        const notAvailable = "QBT_TR(Not available)QBT_TR[CONTEXT=AddNewTorrentDialog]";
        const notApplicable = "QBT_TR(N/A)QBT_TR[CONTEXT=AddNewTorrentDialog]";
        document.getElementById("infoHashV1").textContent = (metadata.infohash_v1 === undefined) ? notAvailable : (metadata.infohash_v1 || notApplicable);
        document.getElementById("infoHashV2").textContent = (metadata.infohash_v2 === undefined) ? notAvailable : (metadata.infohash_v2 || notApplicable);

        if (metadata.info?.length !== undefined)
            document.getElementById("size").textContent = window.qBittorrent.Misc.friendlyUnit(metadata.info.length, false);
        if ((metadata.creation_date !== undefined) && (metadata.creation_date > 1))
            document.getElementById("createdDate").textContent = new Date(metadata.creation_date * 1000).toLocaleString();
        if (metadata.comment !== undefined)
            document.getElementById("comment").textContent = metadata.comment;

        if (metadata.info?.files !== undefined) {
            const files = metadata.info.files.map((file, index) => ({
                index: index,
                name: file.path,
                size: file.length,
                priority: window.qBittorrent.FileTree.FilePriority.Normal,
            }));
            window.qBittorrent.TorrentContent.updateData(files);
        }
    };

    const setWindowId = (id) => {
        windowId = id;
    };

    const submitForm = () => {
        document.getElementById("startTorrentHidden").value = document.getElementById("startTorrent").checked ? "false" : "true";

        document.getElementById("dlLimitHidden").value = Number(document.getElementById("dlLimitText").value) * 1024;
        document.getElementById("upLimitHidden").value = Number(document.getElementById("upLimitText").value) * 1024;

        document.getElementById("filePriorities").value = table.getFileTreeArray()
            .filter((node) => !node.isFolder)
            .sort((node1, node2) => (node1.fileId - node2.fileId))
            .map((node) => node.priority);

        if (!isAutoTMMEnabled())
            document.getElementById("useDownloadPathHidden").value = document.getElementById("useDownloadPath").checked;

        document.getElementById("loadingSpinner").style.display = "block";

        if (document.getElementById("setDefaultCategory").checked) {
            const category = document.getElementById("category").value.trim();
            if (category.length === 0)
                localPreferences.remove("add_torrent_default_category");
            else
                localPreferences.set("add_torrent_default_category", category);
        }
    };

    const init = (source, downloader, fetchMetadata) => {
        table = window.qBittorrent.TorrentContent.init("addTorrentFilesTableDiv", window.qBittorrent.DynamicTable.AddTorrentFilesTable);
        if (fetchMetadata)
            loadMetadata(source, downloader);
    };

    window.addEventListener("load", async (event) => {
        // user might load this page directly (via browser magnet handler)
        // so wait for crucial initialization to complete
        await window.parent.qBittorrent.Client.initializeCaches();

        getPreferences();
        getCategories();
        getTags();
    });

    window.addEventListener("DOMContentLoaded", (event) => {
        document.getElementById("useDownloadPath").addEventListener("change", (e) => changeUseDownloadPath(e.target.checked));
        document.getElementById("tagsSelect").addEventListener("change", (e) => changeTagsSelect(e.target));
    });

    return exports();
})();
Object.freeze(window.qBittorrent.AddTorrent);
