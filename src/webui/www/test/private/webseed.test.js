/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  umutcagand <umutcagand@protonmail.com>
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
import { Window } from "happy-dom";

import html from "../../private/editwebseed.html?raw";

const documentTemplate = document.createElement("template");
documentTemplate.innerHTML = html;
const script = documentTemplate.content.querySelector("script:not([src])").textContent;
const hash = "a".repeat(40);

let page;

beforeEach(() => {
    page = new Window({
        url: "http://localhost/editwebseed.html",
        settings: {
            enableJavaScriptEvaluation: true,
            suppressInsecureJavaScriptEnvironmentWarning: true
        }
    });
    page.document.body.innerHTML = documentTemplate.content.querySelector("div").outerHTML;
    page.fetch = vi.fn().mockResolvedValue({ ok: true });
    page.qBittorrent = { Client: { closeFrameWindow: vi.fn() } };
});

afterEach(async () => {
    await page.happyDOM.close();
});

const openEditor = (url) => {
    page.location.search = new URLSearchParams({ hash: hash, url: url }).toString();
    page.eval(script);
    page.dispatchEvent(new page.Event("DOMContentLoaded"));
};

test.each([
    "https://example.com/files/",
    "https://example.com/a%2Fb/",
    "https://example.com/100%25/",
    "https://example.com/files/?token=a%26b%3Dc%2Bd%25",
    "https://example.com/%E2%82%AC/",
    "https://example.com/日本語/"
])("Preserve the web seed URL when opening and submitting the editor: %s", async (url) => {
    openEditor(url);

    expect(page.document.getElementById("url").value).toBe(url);

    page.document.getElementById("editWebSeedButton").click();

    expect(page.fetch).toHaveBeenCalledTimes(1);
    const [endpoint, options] = page.fetch.mock.calls[0];
    expect(endpoint).toBe("api/v2/torrents/editWebSeed");
    expect(options.method).toBe("POST");

    // The WebAPI decodes each URL after decoding the form parameters.
    const params = new URLSearchParams(options.body.toString());
    expect(params.get("hash")).toBe(hash);
    expect(decodeURIComponent(params.get("origUrl"))).toBe(url);
    expect(decodeURIComponent(params.get("newUrl"))).toBe(url);

    await vi.waitFor(() => expect(page.qBittorrent.Client.closeFrameWindow).toHaveBeenCalledWith(page));
});

test("Keep the original URL separate from the edited URL", () => {
    const originalUrl = "https://example.com/a%2Fb/?token=100%25";
    const newUrl = "https://example.com/c%2Fd/?token=200%25";
    openEditor(originalUrl);
    page.document.getElementById("url").value = `  ${newUrl}  `;

    page.document.getElementById("editWebSeedButton").click();

    const params = new URLSearchParams(page.fetch.mock.calls[0][1].body.toString());
    expect(decodeURIComponent(params.get("origUrl"))).toBe(originalUrl);
    expect(decodeURIComponent(params.get("newUrl"))).toBe(newUrl);
});
