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

    class ProgressBar extends HTMLElement {
        static #progressBarUniqueId = 0;
        static #styles = {
            height: 12,
            darkbg: "var(--color-background-blue)",
            darkfg: "var(--color-text-white)",
            lightbg: "var(--color-background-default)",
            lightfg: "var(--color-text-default)",
        };

        #value = 0;

        #id = ++ProgressBar.#progressBarUniqueId;

        #light = document.createElement("div");
        #dark = document.createElement("div");

        constructor(value) {
            super();

            this.#dark.style.width = "100%";
            this.#dark.style.height = `${ProgressBar.#styles.height}px`;
            this.#dark.style.background = ProgressBar.#styles.darkbg;
            this.#dark.style.boxSizing = "content-box";
            this.#dark.style.color = ProgressBar.#styles.darkfg;
            this.#dark.style.position = "absolute";
            this.#dark.style.textAlign = "center";
            this.#dark.style.left = "0";
            this.#dark.style.top = "0";
            this.#dark.style.lineHeight = `${ProgressBar.#styles.height}px`;

            this.#light.style.width = "100%";
            this.#light.style.height = `${ProgressBar.#styles.height}px`;
            this.#light.style.background = ProgressBar.#styles.lightbg;
            this.#light.style.boxSizing = "content-box";
            this.#light.style.color = ProgressBar.#styles.lightfg;
            this.#light.style.position = "absolute";
            this.#light.style.textAlign = "center";
            this.#light.style.left = "0";
            this.#light.style.top = "0";
            this.#light.style.lineHeight = `${ProgressBar.#styles.height}px`;

            this.attachShadow({ mode: "open" });
            this.shadowRoot.host.id = this.#id;
            this.shadowRoot.host.style.display = "block";
            this.shadowRoot.host.style.border = "1px solid var(--color-border-default)";
            this.shadowRoot.host.style.boxSizing = "content-box";
            this.shadowRoot.host.style.height = `${ProgressBar.#styles.height}px`;
            this.shadowRoot.host.style.position = "relative";
            this.shadowRoot.host.style.margin = "0 auto";
            this.shadowRoot.appendChild(this.#dark);
            this.shadowRoot.appendChild(this.#light);

            this.setValue(value);
        }

        getValue() {
            return this.#value;
        }

        setValue(value) {
            value = Number(value);
            if (Number.isNaN(value))
                value = 0;
            this.#value = Math.min(Math.max(value, 0), 100);

            const displayedValue = `${window.qBittorrent.Misc.toFixedPointString(this.#value, 1)}%`;
            this.#dark.textContent = displayedValue;
            this.#light.textContent = displayedValue;

            this.#dark.style.clipPath = `inset(0 ${100 - this.#value}% 0 0)`;
            this.#light.style.clipPath = `inset(0 0 0 ${this.#value}%)`;
        }
    }

    customElements.define("progress-bar", ProgressBar);

    return exports();
})();
Object.freeze(window.qBittorrent.ProgressBar);
