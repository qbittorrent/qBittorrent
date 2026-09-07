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

// suggestions are refreshed this long after the last keystroke
const REFRESH_DELAY_MS = 400;

const listingResponse = (entries) => Promise.resolve(new Response(JSON.stringify(entries)));
const notFoundResponse = () => Promise.resolve(new Response("Directory does not exist", { status: 404 }));

// resolves the fetch mock with a directory listing when `resolve()` is called
const deferredListing = () => {
    let resolve;
    const promise = new Promise((r) => { resolve = r; });
    return {
        promise: promise,
        resolve: (entries) => resolve(new Response(JSON.stringify(entries)))
    };
};

// like real fetch, rejects as soon as the request is aborted
const abortableListing = (entries) => (url, { signal }) => new Promise((resolve, reject) => {
    signal.addEventListener("abort", () => reject(new DOMException("Aborted", "AbortError")));
    setTimeout(() => resolve(new Response(JSON.stringify(entries))), 1000);
});

const requestedDirPath = (call) => new URL(call[0], "http://localhost/").searchParams.get("dirPath");
const requestedMode = (call) => new URL(call[0], "http://localhost/").searchParams.get("mode");

let input;
let fetchMock;

// types a value without waiting for the refresh
const typeNow = (value) => {
    input.value = value;
    input.dispatchEvent(new Event("input"));
};

// lets the fetch promise chain settle and the refresh timer fire
const settle = () => vi.advanceTimersByTimeAsync(REFRESH_DELAY_MS);

const type = async (value) => {
    typeNow(value);
    await settle();
};

const suggestions = () => [...input.list.options].map((option) => option.value);

beforeEach(() => {
    vi.useFakeTimers();
    document.body.innerHTML = `<div><input type="text" id="savepath" class="pathDirectory"><button id="after">x</button></div>`;
    input = document.getElementById("savepath");
    fetchMock = vi.fn();
    vi.stubGlobal("fetch", fetchMock);
    attachPathAutofill();
});

afterEach(() => {
    vi.unstubAllGlobals();
    vi.useRealTimers();
});

test("attaches an empty datalist next to the input, not inside it", () => {
    const datalist = document.getElementById("savepathSuggestions");

    expect(input.list).toBe(datalist);
    expect(input.nextElementSibling).toBe(datalist);
    expect(input.childElementCount).toBe(0);
    expect(datalist.options).toHaveLength(0);
});

test("fetches the typed directory and shows its entries sorted", async () => {
    fetchMock.mockReturnValueOnce(listingResponse(["/data/b", "/data/a"]));

    await type("/data/");

    expect(fetchMock).toHaveBeenCalledTimes(1);
    expect(requestedDirPath(fetchMock.mock.calls[0])).toBe("/data/");
    expect(requestedMode(fetchMock.mock.calls[0])).toBe("dirs");
    expect(suggestions()).toEqual(["/data/a", "/data/b"]);
});

test("requests mode=all for file inputs", async () => {
    document.body.innerHTML = `<input type="text" id="cert" class="pathFile">`;
    input = document.getElementById("cert");
    attachPathAutofill();
    fetchMock.mockReturnValueOnce(listingResponse(["/etc/ssl/cert.pem"]));

    await type("/etc/ssl/");

    expect(requestedMode(fetchMock.mock.calls[0])).toBe("all");
    expect(suggestions()).toEqual(["/etc/ssl/cert.pem"]);
});

test("refreshes suggestions only after typing pauses", async () => {
    fetchMock.mockReturnValueOnce(listingResponse(["/data/movies", "/data/music"]));
    await type("/data/");
    const replaceChildren = vi.spyOn(input.list, "replaceChildren");

    typeNow("/data/m");
    typeNow("/data/mu");
    await vi.advanceTimersByTimeAsync(REFRESH_DELAY_MS - 1);
    expect(replaceChildren).not.toHaveBeenCalled();
    expect(suggestions()).toEqual(["/data/movies", "/data/music"]);

    await vi.advanceTimersByTimeAsync(1);
    expect(replaceChildren).toHaveBeenCalledTimes(1);
    expect(suggestions()).toEqual(["/data/music"]);
});

test("does not rewrite the datalist when the suggestions are unchanged", async () => {
    fetchMock.mockReturnValueOnce(listingResponse(["/data/movies", "/data/music"]));
    await type("/data/");
    const replaceChildren = vi.spyOn(input.list, "replaceChildren");

    await type("/data/m");
    await type("/data/mo");
    await type("/data/mov");

    expect(replaceChildren).toHaveBeenCalledTimes(1);
    expect(suggestions()).toEqual(["/data/movies"]);
});

test("filters the cached listing locally while typing inside the same directory", async () => {
    fetchMock.mockReturnValueOnce(listingResponse(["/data/movies", "/data/music", "/data/Media"]));

    await type("/data/");
    await type("/data/m");
    await type("/data/mo");

    expect(fetchMock).toHaveBeenCalledTimes(1);
    expect(suggestions()).toEqual(["/data/movies"]);
});

test("matches case-insensitively", async () => {
    fetchMock.mockReturnValueOnce(listingResponse(["/data/Movies", "/data/music"]));

    await type("/data/");
    await type("/data/M");

    expect(suggestions()).toEqual(["/data/Movies", "/data/music"]);
});

test("treats a backslash as a directory separator", async () => {
    fetchMock.mockReturnValueOnce(listingResponse(["C:\\Users\\alice", "C:\\Users\\bob"]));

    await type("C:\\Users\\");
    expect(requestedDirPath(fetchMock.mock.calls[0])).toBe("C:\\Users\\");
    await type("C:\\Users\\a");

    expect(fetchMock).toHaveBeenCalledTimes(1);
    expect(suggestions()).toEqual(["C:\\Users\\alice"]);
});

test("applies the cap after filtering so matches are never hidden by unrelated entries", async () => {
    const entries = [];
    for (let i = 0; i < 100; ++i)
        entries.push(`/data/other ${String(i).padStart(3, "0")}`);
    entries.push("/data/zebra");
    fetchMock.mockReturnValueOnce(listingResponse(entries));

    await type("/data/");
    expect(suggestions()).toHaveLength(10);

    await type("/data/z");
    expect(suggestions()).toEqual(["/data/zebra"]);
});

test("URL-encodes the directory path", async () => {
    fetchMock.mockReturnValueOnce(listingResponse([]));

    await type("/data/Foo & Bar#1/");

    expect(fetchMock.mock.calls[0][0]).toBe("api/v2/app/getDirectoryContent?dirPath=%2Fdata%2FFoo%20%26%20Bar%231%2F&mode=dirs");
});

test("clears suggestions when the directory does not exist and does not ask again", async () => {
    fetchMock.mockReturnValueOnce(listingResponse(["/data/a"]));
    await type("/data/");
    expect(suggestions()).toEqual(["/data/a"]);

    fetchMock.mockReturnValueOnce(notFoundResponse());
    await type("/data/nope/");
    await type("/data/nope/x");

    expect(fetchMock).toHaveBeenCalledTimes(2);
    expect(suggestions()).toEqual([]);
});

test("shows a listing that arrives after typing has already paused without further delay", async () => {
    fetchMock.mockImplementationOnce(abortableListing(["/data/a"]));

    await type("/data/");
    expect(suggestions()).toEqual([]);

    await vi.advanceTimersByTimeAsync(1000);
    expect(suggestions()).toEqual(["/data/a"]);
});

test("discards the response of a request that was aborted by clearing the value", async () => {
    fetchMock.mockImplementationOnce(abortableListing(["/data/a"]));
    await type("/data/");

    await type("");
    expect(fetchMock.mock.calls[0][1].signal.aborted).toBe(true);
    await vi.advanceTimersByTimeAsync(1000);
    expect(suggestions()).toEqual([]);

    fetchMock.mockReturnValueOnce(listingResponse(["/data/b"]));
    await type("/data/");

    expect(fetchMock).toHaveBeenCalledTimes(2);
    expect(suggestions()).toEqual(["/data/b"]);
});

test("clears suggestions when the value has no directory part", async () => {
    fetchMock.mockReturnValueOnce(listingResponse(["/data/a"]));
    await type("/data/");

    await type("data");

    expect(fetchMock).toHaveBeenCalledTimes(1);
    expect(suggestions()).toEqual([]);
});

test("ignores a stale response that arrives after a newer directory was typed", async () => {
    const slow = deferredListing();
    fetchMock.mockReturnValueOnce(slow.promise);
    await type("/");

    fetchMock.mockReturnValueOnce(listingResponse(["/data/movies"]));
    await type("/data/");
    expect(suggestions()).toEqual(["/data/movies"]);

    // the first request was aborted
    expect(fetchMock.mock.calls[0][1].signal.aborted).toBe(true);

    slow.resolve(["/data", "/etc"]);
    await settle();

    expect(suggestions()).toEqual(["/data/movies"]);
});

test("shows the listing filtered by what was typed while the request was in flight", async () => {
    const slow = deferredListing();
    fetchMock.mockReturnValueOnce(slow.promise);

    await type("/data/");
    await type("/data/mu");
    expect(fetchMock).toHaveBeenCalledTimes(1);

    slow.resolve(["/data/movies", "/data/music"]);
    await settle();

    expect(suggestions()).toEqual(["/data/music"]);
});

test("retries after a server or network error", async () => {
    fetchMock.mockReturnValueOnce(Promise.reject(new TypeError("Failed to fetch")));
    await type("/data/");
    expect(suggestions()).toEqual([]);

    fetchMock.mockReturnValueOnce(Promise.resolve(new Response("", { status: 500 })));
    await type("/data/m");
    expect(suggestions()).toEqual([]);

    fetchMock.mockReturnValueOnce(listingResponse(["/data/movies"]));
    await type("/data/mo");

    expect(fetchMock).toHaveBeenCalledTimes(3);
    expect(suggestions()).toEqual(["/data/movies"]);
});

test("does not attach twice to the same input", () => {
    const addEventListener = vi.spyOn(input, "addEventListener");

    attachPathAutofill();

    expect(addEventListener).not.toHaveBeenCalled();
    expect(document.querySelectorAll("datalist")).toHaveLength(1);
});
