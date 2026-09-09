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
File implementing auto-fill for the path input field in the path dialogs.
*/

window.qBittorrent ??= {};
window.qBittorrent.pathAutofill ??= (() => {
    const exports = () => {
        return {
            attachPathAutofill: attachPathAutofill
        };
    };

    // Safari shows only the first line of a wrapped suggestion. For a long
    // name that line ends at the directory, so a label gives it a line that
    // starts with the name itself. Touch devices skip the label: the keyboard
    // bar cuts it off and it doubles the time Safari on iOS blocks input.
    const showLabels = () => (window.matchMedia?.("(pointer: fine)").matches === true);

    const showInputSuggestions = (inputElement, names) => {
        const datalist = document.createElement("datalist");
        datalist.id = `${inputElement.id}Suggestions`;
        const withLabels = showLabels();
        for (const name of names) {
            const option = document.createElement("option");
            option.value = name;
            if (withLabels) {
                const label = name.substring(Math.max(name.lastIndexOf("/"), name.lastIndexOf("\\")) + 1);
                if (label !== "")
                    option.setAttribute("label", label);
            }
            datalist.appendChild(option);
        }

        const oldDatalist = document.getElementById(`${inputElement.id}Suggestions`);
        if (oldDatalist !== null) {
            oldDatalist.replaceWith(datalist);
        }
        else {
            inputElement.appendChild(datalist);
            inputElement.setAttribute("list", datalist.id);
        }
    };

    const showPathSuggestions = (element, mode) => {
        const partialPath = element.value;
        if (partialPath === "")
            return;

        fetch(`api/v2/app/getDirectoryContent?dirPath=${partialPath}&mode=${mode}`, {
                method: "GET",
                cache: "no-store"
            })
            .then(response => response.json())
            .then(filesList => { showInputSuggestions(element, filesList); })
            .catch(error => {});
    };

    const attachPathAutofill = () => {
        const directoryInputs = document.querySelectorAll(".pathDirectory:not(.pathAutoFillInitialized)");
        for (const input of directoryInputs) {
            input.addEventListener("input", function(event) { showPathSuggestions(this, "dirs"); });
            input.classList.add("pathAutoFillInitialized");
        }

        const fileInputs = document.querySelectorAll(".pathFile:not(.pathAutoFillInitialized)");
        for (const input of fileInputs) {
            input.addEventListener("input", function(event) { showPathSuggestions(this, "all"); });
            input.classList.add("pathAutoFillInitialized");
        }
    };

    return exports();
})();
Object.freeze(window.qBittorrent.pathAutofill);
