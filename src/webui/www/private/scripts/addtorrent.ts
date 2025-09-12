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

window.qBittorrent ??= {};
const addTorrentModule = (() => {
    const exports = () => {
        return {
            changeCategorySelect: changeCategorySelect,
            changeTMM: changeTMM,
            loadMetadata: loadMetadata,
            metadataCompleted: metadataCompleted,
            populateMetadata: populateMetadata,
            setWindowId: setWindowId,
            submitForm: submitForm,
        };
    };

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
            keepInlineStyles: false,
        });
    };

    const getPreferences = () => {
        const pref = window.parent.qBittorrent.Cache.preferences.get();

        defaultSavePath = pref.save_path;
        defaultTempPath = pref.temp_path;
        defaultTempPathEnabled = pref.temp_path_enabled;
        window.qBittorrent.Misc.getElementById("startTorrent", "input").checked = !pref.add_stopped_enabled;
        window.qBittorrent.Misc.getElementById("addToTopOfQueue", "input").checked = pref.add_to_top_of_queue;

        const autoTMM = window.qBittorrent.Misc.getElementById("autoTMM", "select");
        if (pref.auto_tmm_enabled) {
            autoTMM.selectedIndex = 1;
            window.qBittorrent.Misc.getElementById("savepath", "input").disabled = true;
        }
        else {
            autoTMM.selectedIndex = 0;
        }
        changeTMM();

        const stopConditionSelect = window.qBittorrent.Misc.getElementById("stopCondition", "select");
        if (pref.torrent_stop_condition === "MetadataReceived")
            stopConditionSelect.selectedIndex = 1;
        else if (pref.torrent_stop_condition === "FilesChecked")
            stopConditionSelect.selectedIndex = 2;
        else
            stopConditionSelect.selectedIndex = 0;

        const contentLayoutSelect = window.qBittorrent.Misc.getElementById("contentLayout", "select");
        if (pref.torrent_content_layout === "Subfolder")
            contentLayoutSelect.selectedIndex = 1;
        else if (pref.torrent_content_layout === "NoSubfolder")
            contentLayoutSelect.selectedIndex = 2;
        else
            contentLayoutSelect.selectedIndex = 0;
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
                window.qBittorrent.Misc.getElementById("savepath", "input").value = defaultSavePath;

                const downloadPathEnabled = categoryDownloadPathEnabled(categoryName);
                window.qBittorrent.Misc.getElementById("useDownloadPath", "input").checked = downloadPathEnabled;
                changeUseDownloadPath(downloadPathEnabled);
            }
        }
        else {
            item.nextElementSibling.hidden = true;
            const text = item.options[item.selectedIndex].textContent;
            item.nextElementSibling.value = text;

            if (isAutoTMMEnabled()) {
                window.qBittorrent.Misc.getElementById("savepath", "input").value = categorySavePath(categoryName);

                const downloadPathEnabled = categoryDownloadPathEnabled(categoryName);
                window.qBittorrent.Misc.getElementById("useDownloadPath", "input").checked = downloadPathEnabled;
                changeUseDownloadPath(downloadPathEnabled);
            }
        }
    };

    const changeTagsSelect = (element) => {
        const tags = [...element.options].filter(opt => opt.selected).map(opt => opt.value);
        window.qBittorrent.Misc.getElementById("tags", "input").value = tags.join(",");
    };

    const isAutoTMMEnabled = () => {
        return window.qBittorrent.Misc.getElementById("autoTMM", "select").selectedIndex === 1;
    };

    const changeTMM = () => {
        const autoTMMEnabled = isAutoTMMEnabled();
        const savepath = window.qBittorrent.Misc.getElementById("savepath", "input");
        const useDownloadPath = window.qBittorrent.Misc.getElementById("useDownloadPath", "input");

        if (autoTMMEnabled) {
            const categorySelect = window.qBittorrent.Misc.getElementById("categorySelect", "select");
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
        window.qBittorrent.Misc.getElementById("useDownloadPathHidden", "input").disabled = autoTMMEnabled;

        changeUseDownloadPath(useDownloadPath.checked);
    };

    const changeUseDownloadPath = (enabled) => {
        const downloadPath = window.qBittorrent.Misc.getElementById("downloadPath", "input");
        if (isAutoTMMEnabled()) {
            const categorySelect = window.qBittorrent.Misc.getElementById("categorySelect", "select");
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
                downloader: downloader,
            }),
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
                    loadMetadataTimer = window.setTimeout(loadMetadata, 1000);
            }, (error) => {
                console.error(error);

                loadMetadataTimer = window.setTimeout(loadMetadata, 1000);
            });
    };

    const metadataCompleted = (showDownloadButton = true) => {
        clearTimeout(loadMetadataTimer);
        loadMetadataTimer = -1;

        document.getElementById("metadataStatus").remove();
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
        window.qBittorrent.Misc.getElementById("startTorrentHidden", "input").value = window.qBittorrent.Misc.getElementById("startTorrent", "input").checked ? "false" : "true";

        window.qBittorrent.Misc.getElementById("dlLimitHidden", "input").value = String(Number(window.qBittorrent.Misc.getElementById("dlLimitText", "input").value) * 1024);
        window.qBittorrent.Misc.getElementById("upLimitHidden", "input").value = String(Number(window.qBittorrent.Misc.getElementById("upLimitText", "input").value) * 1024);

        window.qBittorrent.Misc.getElementById("filePriorities", "input").value = [...document.getElementsByClassName("combo_priority") as unknown as HTMLInputElement[]]
            .filter((el) => !window.qBittorrent.TorrentContent.isFolder(Number(el.dataset.fileId)))
            .sort((el1, el2) => Number(el1.dataset.fileId) - Number(el2.dataset.fileId))
            .map((el) => (el.value))
            .toString();

        if (!isAutoTMMEnabled())
            window.qBittorrent.Misc.getElementById("useDownloadPathHidden", "input").value = String(window.qBittorrent.Misc.getElementById("useDownloadPath", "input").checked);

        document.getElementById("loadingSpinner").style.display = "block";

        if (window.qBittorrent.Misc.getElementById("setDefaultCategory", "input").checked) {
            const category = window.qBittorrent.Misc.getElementById("category", "input").value.trim();
            if (category.length === 0)
                localPreferences.remove("add_torrent_default_category");
            else
                localPreferences.set("add_torrent_default_category", category);
        }
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
        document.getElementById("useDownloadPath").addEventListener("change", (e) => changeUseDownloadPath((e.target as HTMLInputElement).checked));
        document.getElementById("tagsSelect").addEventListener("change", (e) => changeTagsSelect(e.target));
    });

    return exports();
})();

window.qBittorrent.AddTorrent ??= addTorrentModule;
Object.freeze(window.qBittorrent.AddTorrent);
