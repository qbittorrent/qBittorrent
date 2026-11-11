/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Randall Degges <r@rdegges.com>
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

import { afterEach, beforeEach, expect, test, vi } from "vitest";

import "../../private/scripts/client-data.js";
import "../../private/scripts/pathAutofill.js";

const attachPathAutofill = window.qBittorrent.pathAutofill.attachPathAutofill;
// the real singleton; beforeEach swaps in a Map for most tests
const realClientData = window.qBittorrent.ClientData;

const listingResponse = (entries) => Promise.resolve(new Response(JSON.stringify(entries)));

// lets the fetch promise chain settle
const flush = () => new Promise((resolve) => setTimeout(resolve, 0));

let input;
let fetchMock;
let clientData;

const type = async (value) => {
    input.value = value;
    input.dispatchEvent(new Event("input"));
    await flush();
};

const suggestions = () => {
    const datalist = document.getElementById("savepathSuggestions");
    return (datalist === null) ? null : [...datalist.options].map((option) => option.value);
};

beforeEach(() => {
    document.body.innerHTML = `<input type="text" id="savepath" class="pathDirectory">`;
    input = document.getElementById("savepath");
    fetchMock = vi.fn();
    vi.stubGlobal("fetch", fetchMock);
    // the main window exposes the client data singleton; dialogs reach it through window.parent
    clientData = new Map();
    window.qBittorrent.ClientData = clientData;
    attachPathAutofill();
});

afterEach(() => {
    delete window.qBittorrent.ClientData;
    vi.restoreAllMocks();
    vi.unstubAllGlobals();
});

test("suggests paths by default when the setting was never stored", async () => {
    fetchMock.mockReturnValueOnce(listingResponse(["/data/a"]));

    await type("/data/");

    expect(fetchMock).toHaveBeenCalledTimes(1);
    expect(suggestions()).toEqual(["/data/a"]);
});

test("reads the setting from the main window when running inside a dialog iframe", async () => {
    // dialogs do not load client-data.js; the singleton lives on window.parent
    delete window.qBittorrent.ClientData;
    const parentClientData = new Map([
        ["path_autocomplete_enabled", false]
    ]);
    vi.spyOn(window, "parent", "get").mockReturnValue({ qBittorrent: { ClientData: parentClientData } });

    await type("/data/");

    expect(fetchMock).not.toHaveBeenCalled();
    expect(suggestions()).toBeNull();
});

test("neither requests nor shows suggestions when the setting is disabled", async () => {
    clientData.set("path_autocomplete_enabled", false);

    await type("/data/");

    expect(fetchMock).not.toHaveBeenCalled();
    expect(suggestions()).toBeNull();
});

test("resumes suggesting once the setting is enabled again", async () => {
    clientData.set("path_autocomplete_enabled", false);
    await type("/data/");
    expect(fetchMock).not.toHaveBeenCalled();

    clientData.set("path_autocomplete_enabled", true);
    fetchMock.mockReturnValueOnce(listingResponse(["/data/a"]));
    await type("/data/");

    expect(suggestions()).toEqual(["/data/a"]);
});

test("stays enabled when no client data is available", async () => {
    delete window.qBittorrent.ClientData;
    fetchMock.mockReturnValueOnce(listingResponse(["/data/a"]));

    await type("/data/");

    expect(suggestions()).toEqual(["/data/a"]);
});

test("disabling the setting stops suggestions for file inputs too", async () => {
    document.body.innerHTML = `<input type="text" id="file" class="pathFile">`;
    attachPathAutofill();
    clientData.set("path_autocomplete_enabled", false);

    const element = document.getElementById("file");
    element.value = "/data/";
    element.dispatchEvent(new Event("input"));
    await flush();

    expect(fetchMock).not.toHaveBeenCalled();
});

test("honors the setting stored by the Options page through the real client data", async () => {
    // exercises key prefixing and the cache of client-data.js instead of a Map stand-in
    window.qBittorrent.ClientData = realClientData;
    fetchMock.mockImplementation((url, options) => {
        if (url === "api/v2/clientdata/load")
            return Promise.resolve(new Response(JSON.stringify({ qbt_path_autocomplete_enabled: true })));
        if (url === "api/v2/clientdata/store")
            return Promise.resolve(new Response(""));
        return listingResponse(["/data/a"]);
    });

    await realClientData.fetch(["path_autocomplete_enabled"]);
    await type("/data/");
    expect(suggestions()).toEqual(["/data/a"]);

    // what preferences.html does on Save
    await realClientData.set({ path_autocomplete_enabled: false });
    const storeCall = fetchMock.mock.calls.find(([url]) => url === "api/v2/clientdata/store");
    expect(JSON.parse(storeCall[1].body.get("data"))).toEqual({ qbt_path_autocomplete_enabled: false });

    // Save reloads the page, so the field starts without suggestions
    document.body.innerHTML = `<input type="text" id="savepath" class="pathDirectory">`;
    input = document.getElementById("savepath");
    attachPathAutofill();
    fetchMock.mockClear();
    await type("/data/a");
    expect(fetchMock).not.toHaveBeenCalled();
    expect(suggestions()).toBeNull();
});
