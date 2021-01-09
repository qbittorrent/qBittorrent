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

'use strict';

if (window.qBittorrent === undefined) {
    window.qBittorrent = {};
}

window.qBittorrent.Download = (function() {
    const exports = function() {
        return {
            changeCategorySelect: changeCategorySelect,
            changeTMM: changeTMM
        };
    };

    let categories = {};
    let defaultSavePath = "";

    const getCategories = function() {
        new Request.JSON({
            url: 'api/v2/torrents/categories',
            noCache: true,
            method: 'get',
            onSuccess: function(data) {
                if (data) {
                    categories = data;
                    for (const i in data) {
                        const category = data[i];
                        const option = new Element("option");
                        option.set('value', category.name);
                        option.set('html', category.name);
                        $('categorySelect').appendChild(option);
                    }
                }
            }
        }).send();
    };

    const getPreferences = function() {
        new Request.JSON({
            url: 'api/v2/app/preferences',
            method: 'get',
            noCache: true,
            onFailure: function() {
                alert("Could not contact qBittorrent");
            },
            onSuccess: function(pref) {
                if (!pref)
                    return;

                defaultSavePath = pref.save_path;
                $('savepath').setProperty('value', defaultSavePath);
                $('startTorrent').checked = !pref.start_paused_enabled;

                if (pref.auto_tmm_enabled == 1) {
                    $('autoTMM').selectedIndex = 1;
                    $('savepath').disabled = true;
                }
                else {
                    $('autoTMM').selectedIndex = 0;
                }

                if (pref.torrent_content_layout === "Subfolder") {
                    $('contentLayout').selectedIndex = 1;
                }
                else if (pref.torrent_content_layout === "NoSubfolder") {
                    $('contentLayout').selectedIndex = 2;
                }
                else {
                    $('contentLayout').selectedIndex = 0;
                }
            }
        }).send();
    };

    const changeCategorySelect = function(item) {
        if (item.value == "\\other") {
            item.nextElementSibling.hidden = false;
            item.nextElementSibling.value = "";
            item.nextElementSibling.select();

            if ($('autoTMM').selectedIndex == 1)
                $('savepath').value = defaultSavePath;
        }
        else {
            item.nextElementSibling.hidden = true;
            const text = item.options[item.selectedIndex].innerHTML;
            item.nextElementSibling.value = text;

            if ($('autoTMM').selectedIndex == 1) {
                const categoryName = item.value;
                const category = categories[categoryName];
                let savePath = defaultSavePath;
                if (category !== undefined)
                    savePath = (category['savePath'] !== "") ? category['savePath'] : (defaultSavePath + categoryName);
                $('savepath').value = savePath;
            }
        }
    };

    const changeTMM = function(item) {
        if (item.selectedIndex == 1) {
            $('savepath').disabled = true;

            const categorySelect = $('categorySelect');
            const categoryName = categorySelect.options[categorySelect.selectedIndex].value;
            const category = categories[categoryName];
            $('savepath').value = (category === undefined) ? "" : category['savePath'];
        }
        else {
            $('savepath').disabled = false;
            $('savepath').value = defaultSavePath;
        }
    };

    $(window).addEventListener("load", function() {
        getPreferences();
        getCategories();
    });

    return exports();
})();
