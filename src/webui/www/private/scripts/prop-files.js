/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2009  Christophe Dumez <chris@qbittorrent.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

'use strict';

var is_seed = true;
var current_hash = "";

var FilePriority = {
    "Ignored": 0,
    "Normal": 1,
    "High": 6,
    "Maximum": 7,
    "Mixed": -1
};

var normalizePriority = function(priority) {
    switch (priority) {
        case FilePriority.Ignored:
        case FilePriority.Normal:
        case FilePriority.High:
        case FilePriority.Maximum:
        case FilePriority.Mixed:
            return priority;
        default:
            return FilePriority.Normal;
    }
};

var fileCheckboxChanged = function(e) {
    var checkbox = e.target;
    var priority = checkbox.checked ? FilePriority.Normal : FilePriority.Ignored;
    var id = checkbox.get('data-id');

    setFilePriority(id, priority);
    setGlobalCheckboxState();
    return true;
};

var fileComboboxChanged = function(e) {
    var combobox = e.target;
    var newPriority = combobox.value;
    var id = combobox.get('data-id');

    setFilePriority(id, newPriority);
};

var isDownloadCheckboxExists = function(id) {
    return ($('cbPrio' + id) !== null);
};

var createDownloadCheckbox = function(id, download) {
    var checkbox = new Element('input');
    checkbox.set('type', 'checkbox');
    if (download)
        checkbox.set('checked', 'checked');
    checkbox.set('id', 'cbPrio' + id);
    checkbox.set('data-id', id);
    checkbox.set('class', 'DownloadedCB');
    checkbox.addEvent('change', fileCheckboxChanged);
    return checkbox;
};

var updateDownloadCheckbox = function(id, download) {
    var checkbox = $('cbPrio' + id);
    checkbox.checked = download;
};

var isPriorityComboExists = function(id) {
    return ($('comboPrio' + id) !== null);
};

var createPriorityOptionElement = function(priority, selected, html) {
    var elem = new Element('option');
    elem.set('value', priority.toString());
    elem.set('html', html);
    if (selected)
        elem.setAttribute('selected', '');
    return elem;
};

var createPriorityCombo = function(id, selectedPriority) {
    var select = new Element('select');
    select.set('id', 'comboPrio' + id);
    select.set('data-id', id);
    select.set('disabled', is_seed);
    select.addClass('combo_priority');
    select.addEvent('change', fileComboboxChanged);

    createPriorityOptionElement(FilePriority.Ignored, (FilePriority.Ignored === selectedPriority), 'QBT_TR(Do not download)QBT_TR[CONTEXT=PropListDelegate]').injectInside(select);
    createPriorityOptionElement(FilePriority.Normal, (FilePriority.Normal === selectedPriority), 'QBT_TR(Normal)QBT_TR[CONTEXT=PropListDelegate]').injectInside(select);
    createPriorityOptionElement(FilePriority.High, (FilePriority.High === selectedPriority), 'QBT_TR(High)QBT_TR[CONTEXT=PropListDelegate]').injectInside(select);
    createPriorityOptionElement(FilePriority.Maximum, (FilePriority.Maximum === selectedPriority), 'QBT_TR(Maximum)QBT_TR[CONTEXT=PropListDelegate]').injectInside(select);

    return select;
};

var updatePriorityCombo = function(id, selectedPriority) {
    var combobox = $('comboPrio' + id);

    if (parseInt(combobox.value) !== selectedPriority)
        selectComboboxPriority(combobox, selectedPriority);

    if (combobox.disabled !== is_seed)
        combobox.disabled = is_seed;
};

var selectComboboxPriority = function(combobox, priority) {
    var options = combobox.options;
    for (var i = 0; i < options.length; ++i) {
        var option = options[i];
        if (parseInt(option.value) === priority)
            option.setAttribute('selected', '');
        else
            option.removeAttribute('selected');
    }

    combobox.value = priority;
};

var switchCheckboxState = function() {
    var rows = [];
    var priority = FilePriority.Ignored;

    if ($('tristate_cb').state === "checked") {
        setGlobalCheckboxUnchecked();
        // set file priority for all checked to Ignored
        torrentFilesTable.getFilteredAndSortedRows().forEach(function(row) {
            if (row.full_data.checked)
                rows.push(row.full_data.rowId);
        });
    }
    else {
        setGlobalCheckboxChecked();
        priority = FilePriority.Normal;
        // set file priority for all unchecked to Normal
        torrentFilesTable.getFilteredAndSortedRows().forEach(function(row) {
            if (!row.full_data.checked)
                rows.push(row.full_data.rowId);
        });
    }

    if (rows.length > 0)
        setFilePriority(rows, priority);
};

var setGlobalCheckboxState = function() {
    if (isAllCheckboxesChecked())
        setGlobalCheckboxChecked();
    else if (isAllCheckboxesUnchecked())
        setGlobalCheckboxUnchecked();
    else
        setGlobalCheckboxPartial();
};

var setGlobalCheckboxChecked = function() {
    $('tristate_cb').state = "checked";
    $('tristate_cb').indeterminate = false;
    $('tristate_cb').checked = true;
};

var setGlobalCheckboxUnchecked = function() {
    $('tristate_cb').state = "unchecked";
    $('tristate_cb').indeterminate = false;
    $('tristate_cb').checked = false;
};

var setGlobalCheckboxPartial = function() {
    $('tristate_cb').state = "partial";
    $('tristate_cb').indeterminate = true;
};

var isAllCheckboxesChecked = function() {
    var checkboxes = $$('input.DownloadedCB');
    for (var i = 0; i < checkboxes.length; ++i) {
        if (!checkboxes[i].checked)
            return false;
    }
    return true;
};

var isAllCheckboxesUnchecked = function() {
    var checkboxes = $$('input.DownloadedCB');
    for (var i = 0; i < checkboxes.length; ++i) {
        if (checkboxes[i].checked)
            return false;
    }
    return true;
};

var setFilePriority = function(id, priority) {
    if (current_hash === "") return;
    var ids = Array.isArray(id) ? id : [id];

    clearTimeout(loadTorrentFilesDataTimer);
    new Request({
        url: 'api/v2/torrents/filePrio',
        method: 'post',
        data: {
            'hash': current_hash,
            'id': ids.join('|'),
            'priority': priority
        },
        onComplete: function() {
            loadTorrentFilesDataTimer = loadTorrentFilesData.delay(1000);
        }
    }).send();

    ids.forEach(function(_id) {
        var combobox = $('comboPrio' + _id);
        if (combobox !== null)
            selectComboboxPriority(combobox, priority);
    });
};

var loadTorrentFilesDataTimer;
var loadTorrentFilesData = function() {
    if ($('prop_files').hasClass('invisible')
        || $('propertiesPanel_collapseToggle').hasClass('panel-expand')) {
        // Tab changed, don't do anything
        return;
    }
    var new_hash = torrentsTable.getCurrentTorrentHash();
    if (new_hash === "") {
        torrentFilesTable.clear();
        clearTimeout(loadTorrentFilesDataTimer);
        loadTorrentFilesDataTimer = loadTorrentFilesData.delay(5000);
        return;
    }
    if (new_hash != current_hash) {
        torrentFilesTable.clear();
        current_hash = new_hash;
    }
    var url = new URI('api/v2/torrents/files?hash=' + current_hash);
    new Request.JSON({
        url: url,
        noCache: true,
        method: 'get',
        onComplete: function() {
            clearTimeout(loadTorrentFilesDataTimer);
            loadTorrentFilesDataTimer = loadTorrentFilesData.delay(5000);
        },
        onSuccess: function(files) {
            var selectedFiles = torrentFilesTable.selectedRowsIds();

            if (!files) {
                torrentFilesTable.clear();
                return;
            }

            var i = 0;
            files.each(function(file) {
                if (i === 0)
                    is_seed = file.is_seed;

                var row = {
                    rowId: i,
                    checked: (file.priority !== FilePriority.Ignored),
                    name: escapeHtml(file.name),
                    size: file.size,
                    progress: (file.progress * 100).round(1),
                    priority: normalizePriority(file.priority),
                    remaining: (file.size * (1.0 - file.progress)),
                    availability: file.availability
                };

                if ((row.progress === 100) && (file.progress < 1))
                    row.progress = 99.9;

                ++i;
                torrentFilesTable.updateRowData(row);
            }.bind(this));

            torrentFilesTable.updateTable(false);
            torrentFilesTable.altRow();

            if (selectedFiles.length > 0)
                torrentFilesTable.reselectRows(selectedFiles);

            setGlobalCheckboxState();
        }
    }).send();
};

var updateTorrentFilesData = function() {
    clearTimeout(loadTorrentFilesDataTimer);
    loadTorrentFilesData();
};

var torrentFilesContextMenu = new ContextMenu({
    targets: '#torrentFilesTableDiv tr',
    menu: 'torrentFilesMenu',
    actions: {
        FilePrioIgnore: function(element, ref) {
            var selectedRows = torrentFilesTable.selectedRowsIds();
            if (selectedRows.length === 0) return;

            setFilePriority(selectedRows, FilePriority.Ignored);
        },
        FilePrioNormal: function(element, ref) {
            var selectedRows = torrentFilesTable.selectedRowsIds();
            if (selectedRows.length === 0) return;

            setFilePriority(selectedRows, FilePriority.Normal);
        },
        FilePrioHigh: function(element, ref) {
            var selectedRows = torrentFilesTable.selectedRowsIds();
            if (selectedRows.length === 0) return;

            setFilePriority(selectedRows, FilePriority.High);
        },
        FilePrioMaximum: function(element, ref) {
            var selectedRows = torrentFilesTable.selectedRowsIds();
            if (selectedRows.length === 0) return;

            setFilePriority(selectedRows, FilePriority.Maximum);
        }
    },
    offsets: {
        x: -15,
        y: 2
    },
    onShow: function() {
        var selectedRows = torrentFilesTable.selectedRowsIds();

        if (is_seed)
            this.hideItem('FilePrio');
        else
            this.showItem('FilePrio');
    }
});

torrentFilesTable.setup('torrentFilesTableDiv', 'torrentFilesTableFixedHeaderDiv', torrentFilesContextMenu);
// inject checkbox into table header
var tableHeaders = $$('#torrentFilesTableFixedHeaderDiv .dynamicTableHeader th');
if (tableHeaders.length > 0) {
    var checkbox = new Element('input');
    checkbox.set('type', 'checkbox');
    checkbox.set('id', 'tristate_cb');
    checkbox.addEvent('click', switchCheckboxState);

    var checkboxTH = tableHeaders[0];
    checkbox.injectInside(checkboxTH);
}

// default sort by name column
if (torrentFilesTable.getSortedColunn() === null)
    torrentFilesTable.setSortedColumn('name');
