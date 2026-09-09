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

import "../../private/scripts/pathAutofill.js";

const attachPathAutofill = window.qBittorrent.pathAutofill.attachPathAutofill;

const listingResponse = (entries) => Promise.resolve(new Response(JSON.stringify(entries)));

// lets the fetch promise chain settle
const flush = () => new Promise((resolve) => setTimeout(resolve, 0));

let input;
let fetchMock;
let finePointer;

const type = async (value) => {
    input.value = value;
    input.dispatchEvent(new Event("input"));
    await flush();
};

const options = () => [...document.getElementById("savepathSuggestions").options].map((option) => [option.value, option.getAttribute("label")]);

beforeEach(() => {
    document.body.innerHTML = `<input type="text" id="savepath" class="pathDirectory">`;
    input = document.getElementById("savepath");
    fetchMock = vi.fn();
    vi.stubGlobal("fetch", fetchMock);
    finePointer = true;
    vi.stubGlobal("matchMedia", (query) => ({ matches: (query === "(pointer: fine)") && finePointer }));
    attachPathAutofill();
});

afterEach(() => {
    vi.unstubAllGlobals();
});

test("keeps the full path as the value and labels each suggestion with its name", async () => {
    fetchMock.mockReturnValueOnce(listingResponse(["/data/movies/Some.Really.Long.Movie.Name.2024.1080p.BluRay.x264-OTHER", "/data/movies/Alien (1979)"]));

    await type("/data/movies/");

    expect(options()).toEqual([
        ["/data/movies/Some.Really.Long.Movie.Name.2024.1080p.BluRay.x264-OTHER", "Some.Really.Long.Movie.Name.2024.1080p.BluRay.x264-OTHER"],
        ["/data/movies/Alien (1979)", "Alien (1979)"]
    ]);
});

test("labels Windows paths with the last segment", async () => {
    fetchMock.mockReturnValueOnce(listingResponse(["C:\\Users\\alice\\Downloads", "D:\\"]));

    await type("C:\\Users\\alice\\");

    expect(options()).toEqual([
        ["C:\\Users\\alice\\Downloads", "Downloads"],
        ["D:\\", null]
    ]);
});

test("does not label suggestions on touch devices", async () => {
    finePointer = false;
    fetchMock.mockReturnValueOnce(listingResponse(["/data/movies/Alien (1979)"]));

    await type("/data/movies/");

    expect(options()).toEqual([
        ["/data/movies/Alien (1979)", null]
    ]);
});
