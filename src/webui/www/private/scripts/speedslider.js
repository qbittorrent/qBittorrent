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

MochaUI.extend({
    addUpLimitSlider: (hashes) => {
        if ($("uplimitSliderarea")) {
            // Get global upload limit
            fetch("api/v2/transfer/uploadLimit", {
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

                    // Get torrents upload limit
                    // And create slider
                    if (hashes[0] === "global") {
                        let up_limit = maximum;
                        if (up_limit < 0)
                            up_limit = 0;
                        maximum = 10000;
                        new Slider($("uplimitSliderarea"), $("uplimitSliderknob"), {
                            steps: maximum,
                            offset: 0,
                            initialStep: Math.round(up_limit),
                            onChange: (pos) => {
                                if (pos > 0) {
                                    $("uplimitUpdatevalue").value = pos;
                                    $("upLimitUnit").style.visibility = "visible";
                                }
                                else {
                                    $("uplimitUpdatevalue").value = "∞";
                                    $("upLimitUnit").style.visibility = "hidden";
                                }
                            }
                        });
                        // Set default value
                        if (up_limit === 0) {
                            $("uplimitUpdatevalue").value = "∞";
                            $("upLimitUnit").style.visibility = "hidden";
                        }
                        else {
                            $("uplimitUpdatevalue").value = Math.round(up_limit);
                            $("upLimitUnit").style.visibility = "visible";
                        }
                    }
                    else {
                        fetch("api/v2/torrents/uploadLimit", {
                                method: "POST",
                                body: new URLSearchParams({
                                    hashes: hashes.join("|")
                                })
                            })
                            .then(async (response) => {
                                if (!response.ok)
                                    return;

                                const data = await response.json();

                                let up_limit = data[hashes[0]];
                                for (const key in data) {
                                    if (up_limit !== data[key]) {
                                        up_limit = 0;
                                        break;
                                    }
                                }
                                if (up_limit < 0)
                                    up_limit = 0;
                                new Slider($("uplimitSliderarea"), $("uplimitSliderknob"), {
                                    steps: maximum,
                                    offset: 0,
                                    initialStep: Math.round(up_limit / 1024),
                                    onChange: (pos) => {
                                        if (pos > 0) {
                                            $("uplimitUpdatevalue").value = pos;
                                            $("upLimitUnit").style.visibility = "visible";
                                        }
                                        else {
                                            $("uplimitUpdatevalue").value = "∞";
                                            $("upLimitUnit").style.visibility = "hidden";
                                        }
                                    }
                                });
                                // Set default value
                                if (up_limit === 0) {
                                    $("uplimitUpdatevalue").value = "∞";
                                    $("upLimitUnit").style.visibility = "hidden";
                                }
                                else {
                                    $("uplimitUpdatevalue").value = Math.round(up_limit / 1024);
                                    $("upLimitUnit").style.visibility = "visible";
                                }
                            });
                    }
                });
        }
    },

    addDlLimitSlider: (hashes) => {
        if ($("dllimitSliderarea")) {
            // Get global upload limit
            fetch("api/v2/transfer/downloadLimit", {
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
                        let dl_limit = maximum;
                        if (dl_limit < 0)
                            dl_limit = 0;
                        maximum = 10000;
                        new Slider($("dllimitSliderarea"), $("dllimitSliderknob"), {
                            steps: maximum,
                            offset: 0,
                            initialStep: Math.round(dl_limit),
                            onChange: (pos) => {
                                if (pos > 0) {
                                    $("dllimitUpdatevalue").value = pos;
                                    $("dlLimitUnit").style.visibility = "visible";
                                }
                                else {
                                    $("dllimitUpdatevalue").value = "∞";
                                    $("dlLimitUnit").style.visibility = "hidden";
                                }
                            }
                        });
                        // Set default value
                        if (dl_limit === 0) {
                            $("dllimitUpdatevalue").value = "∞";
                            $("dlLimitUnit").style.visibility = "hidden";
                        }
                        else {
                            $("dllimitUpdatevalue").value = Math.round(dl_limit);
                            $("dlLimitUnit").style.visibility = "visible";
                        }
                    }
                    else {
                        fetch("api/v2/torrents/downloadLimit", {
                                method: "POST",
                                body: new URLSearchParams({
                                    hashes: hashes.join("|")
                                })
                            })
                            .then(async (response) => {
                                if (!response.ok)
                                    return;

                                const data = await response.json();

                                let dl_limit = data[hashes[0]];
                                for (const key in data) {
                                    if (dl_limit !== data[key]) {
                                        dl_limit = 0;
                                        break;
                                    }
                                }
                                if (dl_limit < 0)
                                    dl_limit = 0;
                                new Slider($("dllimitSliderarea"), $("dllimitSliderknob"), {
                                    steps: maximum,
                                    offset: 0,
                                    initialStep: Math.round(dl_limit / 1024),
                                    onChange: (pos) => {
                                        if (pos > 0) {
                                            $("dllimitUpdatevalue").value = pos;
                                            $("dlLimitUnit").style.visibility = "visible";
                                        }
                                        else {
                                            $("dllimitUpdatevalue").value = "∞";
                                            $("dlLimitUnit").style.visibility = "hidden";
                                        }
                                    }
                                });
                                // Set default value
                                if (dl_limit === 0) {
                                    $("dllimitUpdatevalue").value = "∞";
                                    $("dlLimitUnit").style.visibility = "hidden";
                                }
                                else {
                                    $("dllimitUpdatevalue").value = Math.round(dl_limit / 1024);
                                    $("dlLimitUnit").style.visibility = "visible";
                                }
                            });
                    }
                });
        }
    }
});
