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
import { join } from "node:path";
import { readFileSync } from "node:fs";

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

test("labels a name that has no separator with the whole name", async () => {
    fetchMock.mockReturnValueOnce(listingResponse(["Downloads"]));

    await type("D");

    expect(options()).toEqual([
        ["Downloads", "Downloads"]
    ]);
});

test("does not label the filesystem root", async () => {
    fetchMock.mockReturnValueOnce(listingResponse(["/", "C:/"]));

    await type("/");

    expect(options()).toEqual([
        ["/", null],
        ["C:/", null]
    ]);
});

test("labels paths with mixed separators with the part after the last one", async () => {
    fetchMock.mockReturnValueOnce(listingResponse(["C:\\Users/alice\\Downloads/Movies", "C:/Users\\alice/Downloads\\Music"]));

    await type("C:\\Users/");

    expect(options()).toEqual([
        ["C:\\Users/alice\\Downloads/Movies", "Movies"],
        ["C:/Users\\alice/Downloads\\Music", "Music"]
    ]);
});

test("labels unicode names without changing them", async () => {
    fetchMock.mockReturnValueOnce(listingResponse(["/data/映画/Amélie (2001) 🎬", "/data/Ψ/ñ"]));

    await type("/data/");

    expect(options()).toEqual([
        ["/data/映画/Amélie (2001) 🎬", "Amélie (2001) 🎬"],
        ["/data/Ψ/ñ", "ñ"]
    ]);
});

test("puts names that look like markup into the label as plain text", async () => {
    const name = "/data/<img src=x onerror=\"window.pwned = true\">";
    fetchMock.mockReturnValueOnce(listingResponse([name]));

    await type("/data/");

    expect(options()).toEqual([
        [name, "<img src=x onerror=\"window.pwned = true\">"]
    ]);
    expect(document.querySelector("img")).toBeNull();
    expect(window.pwned).toBeUndefined();
});

test("caps the suggestions at 25 and labels every one that is shown", async () => {
    const names = Array.from({ length: 30 }, (_, i) => `/data/movies/Movie.${String(i).padStart(2, "0")}.2024.1080p.BluRay.x264`);
    fetchMock.mockReturnValueOnce(listingResponse(names));

    await type("/data/movies/");

    expect(options()).toEqual(names.slice(0, 25).map((name) => [name, name.substring("/data/movies/".length)]));
});

test("replaces the labels when the suggestions update", async () => {
    fetchMock.mockReturnValueOnce(listingResponse(["/data/movies/Alien (1979)"]));
    await type("/data/movies/");

    fetchMock.mockReturnValueOnce(listingResponse(["/data/shows/Severance", "/data/shows/Andor"]));
    await type("/data/shows/");

    expect(options()).toEqual([
        ["/data/shows/Severance", "Severance"],
        ["/data/shows/Andor", "Andor"]
    ]);
    expect(document.querySelectorAll("datalist")).toHaveLength(1);
    expect(input.getAttribute("list")).toBe("savepathSuggestions");
});

test("follows the current pointer each time the suggestions update", async () => {
    fetchMock.mockReturnValueOnce(listingResponse(["/data/movies/Alien (1979)"]));
    await type("/data/movies/");
    expect(options()).toEqual([
        ["/data/movies/Alien (1979)", "Alien (1979)"]
    ]);

    finePointer = false;
    fetchMock.mockReturnValueOnce(listingResponse(["/data/movies/Alien (1979)"]));
    await type("/data/movies/A");
    expect(options()).toEqual([
        ["/data/movies/Alien (1979)", null]
    ]);
});

test("does not label suggestions when matchMedia is unavailable", async () => {
    vi.stubGlobal("matchMedia", undefined);
    fetchMock.mockReturnValueOnce(listingResponse(["/data/movies/Alien (1979)"]));

    await type("/data/movies/");

    expect(options()).toEqual([
        ["/data/movies/Alien (1979)", null]
    ]);
});

test("labels file suggestions the same way as directory suggestions", async () => {
    document.body.innerHTML = `<input type="text" id="certpath" class="pathFile">`;
    input = document.getElementById("certpath");
    attachPathAutofill();
    fetchMock.mockReturnValueOnce(listingResponse(["/etc/ssl/certs/qbittorrent-webui.pem"]));

    await type("/etc/ssl/certs/");

    expect(fetchMock).toHaveBeenCalledOnce();
    expect(fetchMock.mock.calls[0][0]).toContain("mode=all");
    expect([...document.getElementById("certpathSuggestions").options].map((option) => [option.value, option.getAttribute("label")])).toEqual([
        ["/etc/ssl/certs/qbittorrent-webui.pem", "qbittorrent-webui.pem"]
    ]);
});

// The label is drawn as a second line inside the field's width, so a field
// left at the default width cuts the name off after a few characters.
test("gives every path field in Options a width", () => {
    const html = readFileSync(join(import.meta.dirname, "../../private/views/preferences.html"), "utf8");
    const doc = new DOMParser().parseFromString(html, "text/html");
    const fields = [...doc.querySelectorAll("input.pathDirectory, input.pathFile")];

    expect(fields.length).toBeGreaterThan(0);
    expect(fields.filter((field) => field.style.width === "").map((field) => field.id)).toEqual([]);
});
