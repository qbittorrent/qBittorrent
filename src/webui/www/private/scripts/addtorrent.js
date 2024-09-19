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
            setWindowId: setWindowId
        };
    };

    let categories = {};
    let defaultSavePath = "";
    let windowId = "";
    let source;

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
                    $("categorySelect").appendChild(option);
                }
            });
    };

    const getPreferences = () => {
        const pref = window.parent.qBittorrent.Cache.preferences.get();

        defaultSavePath = pref.save_path;
        $("savepath").value = defaultSavePath;
        $("startTorrent").checked = !pref.add_stopped_enabled;
        $("addToTopOfQueue").checked = pref.add_to_top_of_queue;

        if (pref.auto_tmm_enabled) {
            $("autoTMM").selectedIndex = 1;
            $("savepath").disabled = true;
        }
        else {
            $("autoTMM").selectedIndex = 0;
        }

        if (pref.torrent_stop_condition === "MetadataReceived")
            $("stopCondition").selectedIndex = 1;
        else if (pref.torrent_stop_condition === "FilesChecked")
            $("stopCondition").selectedIndex = 2;
        else
            $("stopCondition").selectedIndex = 0;

        if (pref.torrent_content_layout === "Subfolder")
            $("contentLayout").selectedIndex = 1;
        else if (pref.torrent_content_layout === "NoSubfolder")
            $("contentLayout").selectedIndex = 2;
        else
            $("contentLayout").selectedIndex = 0;
    };

    const changeCategorySelect = (item) => {
        if (item.value === "\\other") {
            item.nextElementSibling.hidden = false;
            item.nextElementSibling.value = "";
            item.nextElementSibling.select();

            if ($("autoTMM").selectedIndex === 1)
                $("savepath").value = defaultSavePath;
        }
        else {
            item.nextElementSibling.hidden = true;
            const text = item.options[item.selectedIndex].textContent;
            item.nextElementSibling.value = text;

            if ($("autoTMM").selectedIndex === 1) {
                const categoryName = item.value;
                const category = categories[categoryName];
                let savePath = defaultSavePath;
                if (category !== undefined)
                    savePath = (category["savePath"] !== "") ? category["savePath"] : `${defaultSavePath}/${categoryName}`;
                $("savepath").value = savePath;
            }
        }
    };

    const changeTMM = (item) => {
        if (item.selectedIndex === 1) {
            $("savepath").disabled = true;

            const categorySelect = $("categorySelect");
            const categoryName = categorySelect.options[categorySelect.selectedIndex].value;
            const category = categories[categoryName];
            $("savepath").value = (category === undefined) ? "" : category["savePath"];
        }
        else {
            $("savepath").disabled = false;
            $("savepath").value = defaultSavePath;
        }
    };

    let loadMetadataTimer;
    const loadMetadata = (sourceUrl = undefined) => {
        if (sourceUrl)
            source = sourceUrl;

        const request = new Request.JSON({
            url: "api/v2/torrents/fetchMetadata",
            method: "post",
            noCache: true,
            data: {
                source: source,
            },
            onFailure: () => {
                metadataFailed();
            },
            onSuccess: (response) => {
                populateMetadata(response);

                if (request.status === 200)
                    metadataCompleted();
                else
                    loadMetadataTimer = loadMetadata.delay(1000);
            }
        }).send();
    };

    const metadataCompleted = (showDownloadButton = true) => {
        clearTimeout(loadMetadataTimer);

        document.getElementById("metadataStatus").destroy();
        document.getElementById("loading_spinner").style.display = "none";

        if (showDownloadButton)
            document.getElementById("saveTorrent").classList.remove("invisible");
    };

    const metadataFailed = () => {
        clearTimeout(loadMetadataTimer);

        document.getElementById("metadataStatus").textContent = "Metadata retrieval failed";
        document.getElementById("metadataStatus").classList.add("red");
        document.getElementById("loading_spinner").style.display = "none";
        document.getElementById("error_icon").classList.remove("invisible");
    };

    const populateMetadata = (metadata) => {
        // update window title
        if (metadata.info.name)
            window.parent.$(`${windowId}_title`).textContent = metadata.info.name;

        document.getElementById("infoHashV1").textContent = metadata.infohash_v1 || "N/A";
        document.getElementById("infoHashV2").textContent = metadata.infohash_v2 || "N/A";

        if (metadata.info.length)
            document.getElementById("size").textContent = window.qBittorrent.Misc.friendlyUnit(metadata.info.length, false);
        if (metadata.creation_date && (metadata.creation_date > 1))
            document.getElementById("createdDate").textContent = new Date(metadata.creation_date * 1000).toLocaleString();;
        if (metadata.comment)
            document.getElementById("comment").textContent = metadata.comment;

        if (metadata.info.files) {
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

    $(window).addEventListener("load", () => {
        getPreferences();
        getCategories();
    });

    return exports();
})();
Object.freeze(window.qBittorrent.AddTorrent);
