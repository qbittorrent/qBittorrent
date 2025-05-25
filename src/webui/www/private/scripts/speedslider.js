/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2019  Thomas Piccirello <thomas.piccirello@gmail.com>
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

"use strict";

window.qBittorrent ??= {};
window.qBittorrent.SpeedSlider ??= (() => {
    const exports = () => {
        return {
            setup: setup,
            setLimit: setLimit,
        };
    };

    const hashes = new URLSearchParams(window.location.search).get("hashes").split("|");

    const setup = (type) => {
        window.addEventListener("keydown", (event) => {
            switch (event.key) {
                case "Enter":
                    event.preventDefault();
                    document.getElementById("applyButton").click();
                    break;
                case "Escape":
                    event.preventDefault();
                    window.parent.qBittorrent.Client.closeFrameWindow(window);
                    break;
            }
        });

        const method = type === "upload" ? "uploadLimit" : "downloadLimit";

        // Get global upload limit
        fetch(`api/v2/transfer/${method}`, {
                method: "GET",
                cache: "no-store"
            })
            .then(async (response) => {
                if (!response.ok)
                    return;

                const data = await response.text();

                let maximum = 500;
                const tmp = Number(data);
                if (tmp > 0) {
                    maximum = tmp / 1024.0;
                }
                else {
                    if (hashes[0] === "global")
                        maximum = 10000;
                    else
                        maximum = 1000;
                }

                // Get torrents download limit
                // And create slider
                if (hashes[0] === "global") {
                    let limit = maximum;
                    if (limit < 0)
                        limit = 0;
                    maximum = 10000;

                    setupSlider(Math.round(limit), maximum);
                }
                else {
                    fetch(`api/v2/torrents/${method}`, {
                            method: "POST",
                            body: new URLSearchParams({
                                hashes: hashes.join("|")
                            })
                        })
                        .then(async (response) => {
                            if (!response.ok)
                                return;

                            const data = await response.json();

                            let limit = data[hashes[0]];
                            for (const key in data) {
                                if (limit !== data[key]) {
                                    limit = 0;
                                    break;
                                }
                            }
                            if (limit < 0)
                                limit = 0;

                            setupSlider(Math.round(limit / 1024), maximum);
                        });
                }
            });
    };

    const setupSlider = (limit, maximum) => {
        const input = document.getElementById("limitSliderInput");
        input.setAttribute("max", maximum);
        input.setAttribute("min", 0);
        input.value = limit;
        input.addEventListener("input", (event) => {
            const pos = Number(event.target.value);
            if (pos > 0) {
                document.getElementById("limitUpdatevalue").value = pos;
                document.getElementById("limitUnit").style.visibility = "visible";
            }
            else {
                document.getElementById("limitUpdatevalue").value = "∞";
                document.getElementById("limitUnit").style.visibility = "hidden";
            }
        });
        // Set default value
        if (limit === 0) {
            document.getElementById("limitUpdatevalue").value = "∞";
            document.getElementById("limitUnit").style.visibility = "hidden";
        }
        else {
            document.getElementById("limitUpdatevalue").value = limit;
            document.getElementById("limitUnit").style.visibility = "visible";
        }
    };

    const setLimit = (type) => {
        const limit = Number(document.getElementById("limitUpdatevalue").value) * 1024;
        const method = type === "upload" ? "setUploadLimit" : "setDownloadLimit";
        if (hashes[0] === "global") {
            fetch(`api/v2/transfer/${method}`, {
                    method: "POST",
                    body: new URLSearchParams({
                        limit: limit
                    })
                })
                .then((response) => {
                    if (!response.ok)
                        return;

                    window.parent.updateMainData();
                    window.parent.qBittorrent.Client.closeFrameWindow(window);
                });
        }
        else {
            fetch(`api/v2/torrents/${method}`, {
                    method: "POST",
                    body: new URLSearchParams({
                        hashes: hashes.join("|"),
                        limit: limit
                    })
                })
                .then((response) => {
                    if (!response.ok)
                        return;

                    window.parent.qBittorrent.Client.closeFrameWindow(window);
                });
        }
    };

    return exports();
})();
Object.freeze(window.qBittorrent.SpeedSlider);
