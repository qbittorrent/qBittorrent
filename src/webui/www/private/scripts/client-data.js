/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2025 Thomas Piccirello <thomas@piccirello.com>
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
window.qBittorrent.ClientData ??= (() => {
    const exports = () => {
        return new ClientData();
    };

    // this is exposed as a singleton
    class ClientData {
        /**
         * @type Map<string, any>
         */
        #cache = new Map();

        #keyPrefix = "qbt_";

        #addKeyPrefix(data) {
            return Object.fromEntries(Object.entries(data).map(([key, value]) => ([`${this.#keyPrefix}${key}`, value])));
        }

        #removeKeyPrefix(data) {
            return Object.fromEntries(Object.entries(data).map(([key, value]) => ([key.substring(this.#keyPrefix.length), value])));
        }

        /**
         * @param {string[]} keys
         * @returns {Record<string, any>}
         */
        async #fetch(keys) {
            keys = keys.map(key => `${this.#keyPrefix}${key}`);
            return await fetch("api/v2/clientdata/load", {
                    method: "POST",
                    body: new URLSearchParams({
                        keys: JSON.stringify(keys)
                    })
                })
                .then(async (response) => {
                    if (!response.ok)
                        return;

                    const data = await response.json();
                    return this.#removeKeyPrefix(data);
                });
        }

        /**
         * @param {Record<string, any>} data
         */
        async #set(data) {
            data = this.#addKeyPrefix(data);
            await fetch("api/v2/clientdata/store", {
                    method: "POST",
                    body: new URLSearchParams({
                        data: JSON.stringify(data)
                    })
                })
                .then((response) => {
                    if (!response.ok)
                        throw new Error("Failed to store client data");
                });
        }

        /**
         * @param {string} key
         * @returns {any}
         */
        get(key) {
            return this.#cache.get(key);
        }

        /**
         * @param {string[]} keys
         */
        async fetch(keys = []) {
            const keysToFetch = keys.filter((key) => !this.#cache.has(key));
            if (keysToFetch.length > 0) {
                const fetchedData = await this.#fetch(keysToFetch);
                for (const [key, value] of Object.entries(fetchedData))
                    this.#cache.set(key, value);
            }
        }

        /**
         * @param {Record<string, any>} data
         */
        async set(data) {
            await this.#set(data);

            // update cache
            for (const [key, value] of Object.entries(data))
                this.#cache.set(key, value);
        }
    }

    return exports();
})();
Object.freeze(window.qBittorrent.ClientData);
