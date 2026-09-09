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

    // Safari on iOS blocks input for about 18 ms per option whenever the
    // suggestions of the focused input change, so the list is kept short and
    // is only refreshed once typing pauses.
    const MAX_SUGGESTIONS = 10;
    const REFRESH_DELAY_MS = 400;

    // Per-input state: the datalist, the loaded directory listing and the pending refresh.
    const states = new WeakMap();

    // Returns the directory part of `path` including its trailing separator,
    // or an empty string when `path` contains no separator.
    const parentDirectory = (path) => {
        const index = Math.max(path.lastIndexOf("/"), path.lastIndexOf("\\"));
        return (index === -1) ? "" : path.substring(0, index + 1);
    };

    // Returns up to MAX_SUGGESTIONS entries that start with `prefix`.
    // The match is case-insensitive, like the browser's own datalist filtering.
    const filterSuggestions = (entries, prefix) => {
        const lowerCasePrefix = prefix.toLowerCase();
        const matches = [];
        for (const entry of entries) {
            if (!entry.toLowerCase().startsWith(lowerCasePrefix))
                continue;
            matches.push(entry);
            if (matches.length >= MAX_SUGGESTIONS)
                break;
        }
        return matches;
    };

    const refreshSuggestions = (inputElement, state) => {
        const names = filterSuggestions(state.entries, inputElement.value);
        const shown = names.join("\n");
        if (shown === state.shown)
            return;
        state.shown = shown;

        const fragment = document.createDocumentFragment();
        for (const name of names) {
            const option = document.createElement("option");
            option.value = name;
            fragment.appendChild(option);
        }
        state.datalist.replaceChildren(fragment);
    };

    const scheduleRefresh = (inputElement, state) => {
        clearTimeout(state.refreshTimer);
        state.refreshTimer = setTimeout(() => {
            state.refreshTimer = null;
            refreshSuggestions(inputElement, state);
        }, REFRESH_DELAY_MS);
    };

    const loadDirectory = (inputElement, state, dirPath, mode) => {
        state.abortController?.abort();
        const abortController = new AbortController();
        state.abortController = abortController;
        state.dirPath = dirPath;
        state.entries = [];

        fetch(`api/v2/app/getDirectoryContent?dirPath=${encodeURIComponent(dirPath)}&mode=${mode}`, {
                method: "GET",
                cache: "no-store",
                signal: abortController.signal
            })
            .then(response => {
                if (response.ok)
                    return response.json();
                // client errors are final, e.g. the directory does not exist
                if (response.status < 500)
                    return [];
                throw new Error(response.statusText);
            })
            .then(entries => {
                // a newer request replaced this one
                if (state.abortController !== abortController)
                    return;
                state.entries = entries.sort((a, b) => a.localeCompare(b));
                // while typing continues, the pending refresh picks the entries up
                if (state.refreshTimer === null)
                    refreshSuggestions(inputElement, state);
            })
            .catch(error => {
                // forget the failed request so the next keystroke retries it
                if (state.abortController === abortController)
                    state.dirPath = null;
            });
    };

    const onInput = (inputElement, mode) => {
        const state = states.get(inputElement);
        const dirPath = parentDirectory(inputElement.value);
        if (dirPath !== state.dirPath) {
            if (dirPath === "") {
                state.abortController?.abort();
                state.abortController = null;
                state.dirPath = dirPath;
                state.entries = [];
            }
            else {
                loadDirectory(inputElement, state, dirPath, mode);
            }
        }
        scheduleRefresh(inputElement, state);
    };

    const attach = (inputElement, mode) => {
        // <input> is a void element, so the datalist goes next to it
        const datalist = document.createElement("datalist");
        datalist.id = `${inputElement.id}Suggestions`;
        inputElement.insertAdjacentElement("afterend", datalist);
        inputElement.setAttribute("list", datalist.id);

        states.set(inputElement, {
            datalist: datalist,
            dirPath: "",
            entries: [],
            abortController: null,
            refreshTimer: null,
            shown: ""
        });
        inputElement.addEventListener("input", () => { onInput(inputElement, mode); });
        inputElement.classList.add("pathAutoFillInitialized");
    };

    const attachPathAutofill = () => {
        for (const input of document.querySelectorAll(".pathDirectory:not(.pathAutoFillInitialized)"))
            attach(input, "dirs");
        for (const input of document.querySelectorAll(".pathFile:not(.pathAutoFillInitialized)"))
            attach(input, "all");
    };

    return exports();
})();
Object.freeze(window.qBittorrent.pathAutofill);
