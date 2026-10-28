/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2024  sledgehammer999 <hammered999@gmail.com>
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
window.qBittorrent.ColorScheme ??= (() => {
    const exports = () => {
        return {
            update,
        };
    };

    const colorSchemeQuery = window.matchMedia("(prefers-color-scheme: dark)");

    const update = () => {
        const root = document.documentElement;
        // chosen in Options and kept in this browser; none follows the system
        const colorScheme = localStorage.getItem("color_scheme");
        const validScheme = (colorScheme === "light") || (colorScheme === "dark");
        const isDark = colorSchemeQuery.matches;
        root.classList.toggle("dark", ((!validScheme && isDark) || (colorScheme === "dark")));
    };

    colorSchemeQuery.addEventListener("change", (_event) => update());
    // each page loads this script at the top of its head, so that it is drawn in its scheme from the start
    update();

    // dialog frames take the responsive layout from the main window, the only page that loads responsive.js (a
    // framed WebUI's parent is the page that embeds it); once the page is read, so that the layout's stylesheet comes
    // after the page's own
    const addFrame = () => {
        if (window.qBittorrent.Responsive === undefined)
            window.parent.qBittorrent.Responsive?.addFrame(document);
    };
    if (document.readyState === "loading")
        document.addEventListener("DOMContentLoaded", (_event) => addFrame());
    else
        addFrame();

    return exports();
})();
Object.freeze(window.qBittorrent.ColorScheme);
