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

var categories = {};
var defaultSavePath = "";

getCategories = function() {
    new Request.JSON({
        url: 'api/v2/torrents/categories',
        noCache: true,
        method: 'get',
        onSuccess: function(data) {
            if (data) {
                categories = data;
                for (var i in data) {
                    var category = data[i];
                    var option = new Element("option");
                    option.set('value', category.name);
                    option.set('html', category.name);
                    $('categorySelect').appendChild(option);
                }
            }
        }
    }).send();
};

getPreferences = function() {
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
            $('rootFolder').checked = pref.create_subfolder_enabled;
            $('startTorrent').checked = !pref.start_paused_enabled;

            if (pref.auto_tmm_enabled == 1) {
                $('autoTMM').selectedIndex = 1;
                $('savepath').disabled = true;
            }
            else {
                $('autoTMM').selectedIndex = 0;
            }
        }
    }).send();
};

changeCategorySelect = function(item) {
    if (item.value == "\\other") {
        item.nextElementSibling.hidden = false;
        item.nextElementSibling.value = "";
        item.nextElementSibling.select();

        if ($('autotmm').selectedIndex == 1)
            $('savepath').value = defaultSavePath;
    }
    else {
        item.nextElementSibling.hidden = true;
        var text = item.options[item.selectedIndex].innerHTML;
        item.nextElementSibling.value = text;

        if ($('autotmm').selectedIndex == 1) {
            var categoryName = item.value;
            var category = categories[categoryName];
            var savePath = defaultSavePath;
            if (category !== undefined)
                savePath = (category['savePath'] !== "") ? category['savePath'] : (defaultSavePath + categoryName);
            $('savepath').value = savePath;
        }
    }
};

changeTMM = function(item) {
    if (item.selectedIndex == 1) {
        $('savepath').disabled = true;

        var categorySelect = $('categorySelect');
        var categoryName = categorySelect.options[categorySelect.selectedIndex].value;
        var category = categories[categoryName];
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
