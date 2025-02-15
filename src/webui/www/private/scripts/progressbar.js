/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2008  Christophe Dumez <chris@qbittorrent.org>
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
window.qBittorrent.ProgressBar ??= (() => {
    const exports = () => {
        return {
            ProgressBar: ProgressBar
        };
    };

    let progressBars = 0;
    const ProgressBar = new Class({
        initialize: (value, parameters) => {
            const vals = {
                id: `progressbar_${progressBars++}`,
                value: [value, 0].pick(),
                width: 0,
                height: 0,
                darkbg: "var(--color-background-blue)",
                darkfg: "var(--color-text-white)",
                lightbg: "var(--color-background-default)",
                lightfg: "var(--color-text-default)"
            };
            if (parameters && (typeOf(parameters) === "object"))
                Object.append(vals, parameters);
            if (vals.height < 12)
                vals.height = 12;

            const obj = document.createElement("div");
            obj.id = vals.id;
            obj.className = "progressbar_wrapper";
            obj.style.border = "1px solid var(--color-border-default)";
            obj.style.boxSizing = "content-box";
            obj.style.width = `${vals.width}px`;
            obj.style.height = `${vals.height}px`;
            obj.style.position = "relative";
            obj.style.margin = "0 auto";
            obj.vals = vals;
            obj.vals.value = [value, 0].pick();

            const dark = document.createElement("div");
            dark.id = `${vals.id}_dark`;
            dark.className = "progressbar_dark";
            dark.style.width = `${vals.width}px`;
            dark.style.height = `${vals.height}px`;
            dark.style.background = vals.darkbg;
            dark.style.boxSizing = "content-box";
            dark.style.color = vals.darkfg;
            dark.style.position = "absolute";
            dark.style.textAlign = "center";
            dark.style.left = "0";
            dark.style.top = "0";
            dark.style.lineHeight = `${vals.height}px`;

            obj.vals.dark = dark;

            const light = document.createElement("div");
            light.id = `${vals.id}_light`;
            light.className = "progressbar_light";
            light.style.width = `${vals.width}px`;
            light.style.height = `${vals.height}px`;
            light.style.background = vals.lightbg;
            light.style.boxSizing = "content-box";
            light.style.color = vals.lightfg;
            light.style.position = "absolute";
            light.style.textAlign = "center";
            light.style.left = "0";
            light.style.top = "0";
            light.style.lineHeight = `${vals.height}px`;

            obj.vals.light = light;

            obj.appendChild(obj.vals.dark);
            obj.appendChild(obj.vals.light);
            obj.getValue = ProgressBar_getValue;
            obj.setValue = ProgressBar_setValue;
            obj.setWidth = ProgressBar_setWidth;
            if (vals.width)
                obj.setValue(vals.value);
            else
                setTimeout(ProgressBar_checkForParent, 0, obj.id);
            return obj;
        }
    });

    function ProgressBar_getValue() {
        return this.vals.value;
    }

    function ProgressBar_setValue(value) {
        value = Number(value);
        if (Number.isNaN(value))
            value = 0;
        value = Math.min(Math.max(value, 0), 100);
        this.vals.value = value;

        const displayedValue = `${window.qBittorrent.Misc.toFixedPointString(value, 1)}%`;
        this.vals.dark.textContent = displayedValue;
        this.vals.light.textContent = displayedValue;

        const r = Number(this.vals.width * (value / 100));
        this.vals.dark.style.clipPath = `inset(0 calc(100% - ${r}px) 0 0)`;
        this.vals.light.style.clipPath = `inset(0 0 0 ${r}px)`;
    }

    function ProgressBar_setWidth(value) {
        if (this.vals.width !== value) {
            this.vals.width = value;
            this.style.width = `${value}px`;
            this.vals.dark.style.width = `${value}px`;
            this.vals.light.style.width = `${value}px`;
            this.setValue(this.vals.value);
        }
    }

    const ProgressBar_checkForParent = (id) => {
        const obj = $(id);
        if (!obj)
            return;
        if (!obj.parentNode)
            return setTimeout(ProgressBar_checkForParent, 100, id);
        obj.style.width = "100%";
        const w = obj.offsetWidth;
        obj.vals.dark.style.width = `${w}px`;
        obj.vals.light.style.width = `${w}px`;
        obj.vals.width = w;
        obj.setValue(obj.vals.value);
    };

    return exports();
})();
Object.freeze(window.qBittorrent.ProgressBar);
