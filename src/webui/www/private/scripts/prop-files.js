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

let is_seed = true;
this.current_hash = "";

const FilePriority = {
    "Ignored": 0,
    "Normal": 1,
    "High": 6,
    "Maximum": 7,
    "Mixed": -1
};

const normalizePriority = function(priority) {
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

const fileCheckboxChanged = function(e) {
    const checkbox = e.target;
    const priority = checkbox.checked ? FilePriority.Normal : FilePriority.Ignored;
    const id = checkbox.get('data-id');

    setFilePriority(id, priority);
    setGlobalCheckboxState();
    return true;
};

const fileComboboxChanged = function(e) {
    const combobox = e.target;
    const newPriority = combobox.value;
    const id = combobox.get('data-id');

    setFilePriority(id, newPriority);
};

const isDownloadCheckboxExists = function(id) {
    return ($('cbPrio' + id) !== null);
};

const createDownloadCheckbox = function(id, download) {
    const checkbox = new Element('input');
    checkbox.set('type', 'checkbox');
    if (download)
        checkbox.set('checked', 'checked');
    checkbox.set('id', 'cbPrio' + id);
    checkbox.set('data-id', id);
    checkbox.set('class', 'DownloadedCB');
    checkbox.addEvent('change', fileCheckboxChanged);
    return checkbox;
};

const updateDownloadCheckbox = function(id, download) {
    const checkbox = $('cbPrio' + id);
    checkbox.checked = download;
};

const isPriorityComboExists = function(id) {
    return ($('comboPrio' + id) !== null);
};

const createPriorityOptionElement = function(priority, selected, html) {
    const elem = new Element('option');
    elem.set('value', priority.toString());
    elem.set('html', html);
    if (selected)
        elem.setAttribute('selected', '');
    return elem;
};

const createPriorityCombo = function(id, selectedPriority) {
    const select = new Element('select');
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

const updatePriorityCombo = function(id, selectedPriority) {
    const combobox = $('comboPrio' + id);

    if (parseInt(combobox.value) !== selectedPriority)
        selectComboboxPriority(combobox, selectedPriority);

    if (combobox.disabled !== is_seed)
        combobox.disabled = is_seed;
};

const selectComboboxPriority = function(combobox, priority) {
    const options = combobox.options;
    for (let i = 0; i < options.length; ++i) {
        const option = options[i];
        if (parseInt(option.value) === priority)
            option.setAttribute('selected', '');
        else
            option.removeAttribute('selected');
    }

    combobox.value = priority;
};

const switchCheckboxState = function() {
    const rows = [];
    let priority = FilePriority.Ignored;

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

const setGlobalCheckboxState = function() {
    if (isAllCheckboxesChecked())
        setGlobalCheckboxChecked();
    else if (isAllCheckboxesUnchecked())
        setGlobalCheckboxUnchecked();
    else
        setGlobalCheckboxPartial();
};

const setGlobalCheckboxChecked = function() {
    $('tristate_cb').state = "checked";
    $('tristate_cb').indeterminate = false;
    $('tristate_cb').checked = true;
};

const setGlobalCheckboxUnchecked = function() {
    $('tristate_cb').state = "unchecked";
    $('tristate_cb').indeterminate = false;
    $('tristate_cb').checked = false;
};

const setGlobalCheckboxPartial = function() {
    $('tristate_cb').state = "partial";
    $('tristate_cb').indeterminate = true;
};

const isAllCheckboxesChecked = function() {
    const checkboxes = $$('input.DownloadedCB');
    for (let i = 0; i < checkboxes.length; ++i) {
        if (!checkboxes[i].checked)
            return false;
    }
    return true;
};

const isAllCheckboxesUnchecked = function() {
    const checkboxes = $$('input.DownloadedCB');
    for (let i = 0; i < checkboxes.length; ++i) {
        if (checkboxes[i].checked)
            return false;
    }
    return true;
};

const setFilePriority = function(id, priority) {
    if (current_hash === "") return;
    const ids = Array.isArray(id) ? id : [id];

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
        const combobox = $('comboPrio' + _id);
        if (combobox !== null)
            selectComboboxPriority(combobox, priority);
    });
};

let loadTorrentFilesDataTimer;
const loadTorrentFilesData = function() {
    if ($('prop_files').hasClass('invisible')
        || $('propertiesPanel_collapseToggle').hasClass('panel-expand')) {
        // Tab changed, don't do anything
        return;
    }
    const new_hash = torrentsTable.getCurrentTorrentHash();
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
    const url = new URI('api/v2/torrents/files?hash=' + current_hash);
    new Request.JSON({
        url: url,
        noCache: true,
        method: 'get',
        onComplete: function() {
            clearTimeout(loadTorrentFilesDataTimer);
            loadTorrentFilesDataTimer = loadTorrentFilesData.delay(5000);
        },
        onSuccess: function(files) {
            const selectedFiles = torrentFilesTable.selectedRowsIds();

            if (!files) {
                torrentFilesTable.clear();
                return;
            }

            let i = 0;
            files.each(function(file) {
                if (i === 0)
                    is_seed = file.is_seed;

                const row = {
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

updateTorrentFilesData = function() {
    clearTimeout(loadTorrentFilesDataTimer);
    loadTorrentFilesData();
};

const torrentFilesContextMenu = new ContextMenu({
    targets: '#torrentFilesTableDiv tr',
    menu: 'torrentFilesMenu',
    actions: {
        FilePrioIgnore: function(element, ref) {
            const selectedRows = torrentFilesTable.selectedRowsIds();
            if (selectedRows.length === 0) return;

            setFilePriority(selectedRows, FilePriority.Ignored);
        },
        FilePrioNormal: function(element, ref) {
            const selectedRows = torrentFilesTable.selectedRowsIds();
            if (selectedRows.length === 0) return;

            setFilePriority(selectedRows, FilePriority.Normal);
        },
        FilePrioHigh: function(element, ref) {
            const selectedRows = torrentFilesTable.selectedRowsIds();
            if (selectedRows.length === 0) return;

            setFilePriority(selectedRows, FilePriority.High);
        },
        FilePrioMaximum: function(element, ref) {
            const selectedRows = torrentFilesTable.selectedRowsIds();
            if (selectedRows.length === 0) return;

            setFilePriority(selectedRows, FilePriority.Maximum);
        }
    },
    offsets: {
        x: -15,
        y: 2
    },
    onShow: function() {
        if (is_seed)
            this.hideItem('FilePrio');
        else
            this.showItem('FilePrio');
    }
});

torrentFilesTable.setup('torrentFilesTableDiv', 'torrentFilesTableFixedHeaderDiv', torrentFilesContextMenu);
// inject checkbox into table header
const tableHeaders = $$('#torrentFilesTableFixedHeaderDiv .dynamicTableHeader th');
if (tableHeaders.length > 0) {
    const checkbox = new Element('input');
    checkbox.set('type', 'checkbox');
    checkbox.set('id', 'tristate_cb');
    checkbox.addEvent('click', switchCheckboxState);

    const checkboxTH = tableHeaders[0];
    checkbox.injectInside(checkboxTH);
}

// default sort by name column
if (torrentFilesTable.getSortedColumn() === null)
    torrentFilesTable.setSortedColumn('name');
