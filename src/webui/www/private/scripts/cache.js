/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2024  Mike Tzou (Chocobo1)
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
window.qBittorrent.Cache ??= (() => {
    const exports = () => {
        return {
            buildInfo: new BuildInfoCache(),
            preferences: new PreferencesCache(),
            qbtVersion: new QbtVersionCache()
        };
    };

    const deepFreeze = (obj) => {
        // https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/freeze#examples

        const keys = Reflect.ownKeys(obj);
        for (const key of keys) {
            const value = obj[key];
            if ((value && (typeof value === "object")) || (typeof value === "function"))
                deepFreeze(value);
        }
        Object.freeze(obj);
    };

    class BuildInfoCache {
        #m_store = {};

        async init() {
            return fetch("api/v2/app/buildInfo", {
                    method: "GET",
                    cache: "no-store"
                })
                .then(async (response) => {
                    if (!response.ok)
                        return;

                    const responseJSON = await response.json();
                    deepFreeze(responseJSON);
                    this.#m_store = responseJSON;
                });
        }

        get() {
            return this.#m_store;
        }
    }

    class PreferencesCache {
        #m_store = {};

        // obj: {
        //   onFailure: () => {},
        //   onSuccess: () => {}
        // }
        async init(obj = {}) {
            return fetch("api/v2/app/preferences", {
                    method: "GET",
                    cache: "no-store"
                })
                .then(async (response) => {
                        if (!response.ok)
                            return;

                        const responseText = await response.text();
                        const responseJSON = JSON.parse(responseText);
                        deepFreeze(responseJSON);
                        this.#m_store = responseJSON;

                        if (typeof obj.onSuccess === "function")
                            obj.onSuccess(responseJSON, responseText);
                    },
                    (error) => {
                        if (typeof obj.onFailure === "function")
                            obj.onFailure(error);
                    });
        }

        get() {
            return this.#m_store;
        }

        // obj: {
        //   data: {},
        //   onFailure: () => {},
        //   onSuccess: () => {}
        // }
        set(obj) {
            if (typeof obj !== "object")
                throw new Error("`obj` is not an object.");
            if (typeof obj.data !== "object")
                throw new Error("`data` is not an object.");

            fetch("api/v2/app/setPreferences", {
                    method: "POST",
                    body: new URLSearchParams({
                        json: JSON.stringify(obj.data)
                    })
                })
                .then(async (response) => {
                        if (!response.ok)
                            return;

                        this.#m_store = structuredClone(this.#m_store);
                        for (const key in obj.data) {
                            if (!Object.hasOwn(obj.data, key))
                                continue;

                            const value = obj.data[key];
                            this.#m_store[key] = value;
                        }
                        deepFreeze(this.#m_store);

                        if (typeof obj.onSuccess === "function") {
                            const responseText = await response.text();
                            obj.onSuccess(responseText);
                        }
                    },
                    (error) => {
                        if (typeof obj.onFailure === "function")
                            obj.onFailure(error);
                    });
        }
    }

    class QbtVersionCache {
        #m_store = "";

        async init() {
            return fetch("api/v2/app/version", {
                    method: "GET",
                    cache: "no-store"
                })
                .then(async (response) => {
                    if (!response.ok)
                        return;

                    const responseText = await response.text();
                    this.#m_store = responseText;
                });
        }

        get() {
            return this.#m_store;
        }
    }

    return exports();
})();
Object.freeze(window.qBittorrent.Cache);
