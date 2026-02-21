/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2024  Paweł Kotiuk <kotiuk@zohomail.eu>
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
 * link this program with the OpenSSL project"s "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

"use strict";

/*
File implementing recalculating the free space when the save path changes in the add torrent dialog.
*/

window.qBittorrent ??= {};
window.qBittorrent.freespaceAutofill ??= (() => {
    const exports = () => {
        return {
            attachFreeSpaceAutofill: attachFreeSpaceAutofill,
            showFreeSpace: showFreeSpace
        };
    };

    const showFreeSpace = (element) => {
        const partialPath = element.value;
        if (partialPath === "")
            return;

        fetch(`api/v2/app/getFreeSpaceAtPath?path=${partialPath}`, {
                method: "GET",
                cache: "no-store"
            })
            .then(response => response.text())
            .then(freeSpace => { filloutFreeSpace(freeSpace); })
            .catch(error => {});
    };

    const filloutFreeSpace = (freeSpace) => {
        const size = document.getElementById("size");
        size.textContent = `${" QBT_TR(%1 (Free space on disk: %2))QBT_TR[CONTEXT=pathAutofill]"
            .replace("%1", size.getAttribute("size"))
            .replace("%2", window.qBittorrent.Misc.friendlyUnit(freeSpace, false))}`;
    };

    const attachFreeSpaceAutofill = () => {
        const freeSpaceInputs = document.querySelectorAll(".pathFreeSpace:not(.freeSpaceAutoFillInitialized)");
        for (const input of freeSpaceInputs) {
            input.addEventListener("input", function(event) { showFreeSpace(this); });
            input.classList.add("freeSpaceAutoFillInitialized");
        }
    };

    return exports();
})();
Object.freeze(window.qBittorrent.pathAutofill);
