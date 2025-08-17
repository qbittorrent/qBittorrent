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
        return {
            getCached: (...args) => instance.getCached(...args),
            get: (...args) => instance.get(...args),
            set: (...args) => instance.set(...args),
        };
    };

    // this is exposed as a singleton
    class ClientData {
        /**
         * @type Map<string, any>
         */
        #cache = new Map();

        /**
         * @param {string[]} keys
         * @returns {Record<string, any>}
         */
        async #fetch(keys) {
            return await fetch("api/v2/app/clientData", {
                    method: "POST",
                    body: new URLSearchParams({
                        keys: JSON.stringify(keys)
                    })
                })
                .then(async (response) => {
                    if (!response.ok)
                        return;

                    return await response.json();
                });
        }

        /**
         * @param {Record<string, any>} data
         */
        async #set(data) {
            await fetch("api/v2/app/setClientData", {
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
        getCached(key) {
            return this.#cache.get(key);
        }

        /**
         * @param {string[]} keys
         * @returns {Record<string, any>}
         */
        async get(keys = []) {
            const keysToFetch = keys.filter((key) => !this.#cache.has(key));
            if (keysToFetch.length > 0) {
                const fetchedData = await this.#fetch(keysToFetch);
                for (const [key, value] of Object.entries(fetchedData))
                    this.#cache.set(key, value);
            }

            return Object.fromEntries(keys.map((key) => ([key, this.#cache.get(key)])));
        }

        /**
         * @param {Record<string, any>} data
         */
        async set(data) {
            try {
                await this.#set(data);
            }
            catch (err) {
                console.error(err);
                return;
            }

            // update cache
            for (const [key, value] of Object.entries(data))
                this.#cache.set(key, value);
        }
    }

    const instance = new ClientData();

    return exports();
})();
Object.freeze(window.qBittorrent.ClientData);
