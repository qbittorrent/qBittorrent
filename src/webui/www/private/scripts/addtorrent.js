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
            loadMetadata: loadMetadata,
            metadataCompleted: metadataCompleted,
            populateMetadata: populateMetadata,
            setWindowId: setWindowId,
            submitForm: submitForm
        };
    };

    let categories = {};
    let defaultSavePath = "";
    let windowId = "";
    let source = "";

    const getCategories = () => {
        fetch("api/v2/torrents/categories", {
                method: "GET",
                cache: "no-store"
            })
            .then(async (response) => {
                if (!response.ok)
                    return;

                const data = await response.json();

                categories = data;
                for (const i in data) {
                    if (!Object.hasOwn(data, i))
                        continue;

                    const category = data[i];
                    const option = document.createElement("option");
                    option.value = category.name;
                    option.textContent = category.name;
                    document.getElementById("categorySelect").appendChild(option);
                }
            });
    };

    const getPreferences = () => {
        const pref = window.parent.qBittorrent.Cache.preferences.get();

        defaultSavePath = pref.save_path;
        document.getElementById("savepath").value = defaultSavePath;
        document.getElementById("startTorrent").checked = !pref.add_stopped_enabled;
        document.getElementById("addToTopOfQueue").checked = pref.add_to_top_of_queue;

        if (pref.auto_tmm_enabled) {
            document.getElementById("autoTMM").selectedIndex = 1;
            document.getElementById("savepath").disabled = true;
        }
        else {
            document.getElementById("autoTMM").selectedIndex = 0;
        }

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

    const changeCategorySelect = (item) => {
        if (item.value === "\\other") {
            item.nextElementSibling.hidden = false;
            item.nextElementSibling.value = "";
            item.nextElementSibling.select();

            if (document.getElementById("autoTMM").selectedIndex === 1)
                document.getElementById("savepath").value = defaultSavePath;
        }
        else {
            item.nextElementSibling.hidden = true;
            const text = item.options[item.selectedIndex].textContent;
            item.nextElementSibling.value = text;

            if (document.getElementById("autoTMM").selectedIndex === 1) {
                const categoryName = item.value;
                const category = categories[categoryName];
                let savePath = defaultSavePath;
                if (category !== undefined)
                    savePath = (category["savePath"] !== "") ? category["savePath"] : `${defaultSavePath}/${categoryName}`;
                document.getElementById("savepath").value = savePath;
            }
        }
    };

    const changeTMM = (item) => {
        if (item.selectedIndex === 1) {
            document.getElementById("savepath").disabled = true;

            const categorySelect = document.getElementById("categorySelect");
            const categoryName = categorySelect.options[categorySelect.selectedIndex].value;
            const category = categories[categoryName];
            document.getElementById("savepath").value = (category === undefined) ? "" : category["savePath"];
        }
        else {
            document.getElementById("savepath").disabled = false;
            document.getElementById("savepath").value = defaultSavePath;
        }
    };

    let loadMetadataTimer = -1;
    const loadMetadata = (sourceUrl = undefined) => {
        if (sourceUrl !== undefined)
            source = sourceUrl;

        fetch("api/v2/torrents/fetchMetadata", {
                method: "POST",
                body: new URLSearchParams({
                    source: source
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

        document.getElementById("filePriorities").value = [...document.getElementsByClassName("combo_priority")]
            .filter((el) => !window.qBittorrent.TorrentContent.isFolder(Number(el.dataset.fileId)))
            .sort((el1, el2) => Number(el1.dataset.fileId) - Number(el2.dataset.fileId))
            .map((el) => Number(el.value));

        document.getElementById("loadingSpinner").style.display = "block";
    };

    window.addEventListener("load", async (event) => {
        // user might load this page directly (via browser magnet handler)
        // so wait for crucial initialization to complete
        await window.parent.qBittorrent.Client.initializeCaches();

        getPreferences();
        getCategories();
    });

    return exports();
})();
Object.freeze(window.qBittorrent.AddTorrent);
