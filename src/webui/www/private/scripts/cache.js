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

'use strict';

if (window.qBittorrent === undefined)
    window.qBittorrent = {};

window.qBittorrent.Cache = (() => {
    const exports = () => {
        return {
            buildInfo: new BuildInfoCache(),
            preferences: new PreferencesCache(),
            qbtVersion: new QbtVersionCache()
        };
    };

    class BuildInfoCache {
        #m_store = {};

        init() {
            new Request.JSON({
                url: 'api/v2/app/buildInfo',
                method: 'get',
                noCache: true,
                onSuccess: (responseJSON) => {
                    if (!responseJSON)
                        return;
                    this.#m_store = responseJSON;
                }
            }).send();
        }

        get() {
            return structuredClone(this.#m_store);
        }
    }

    class PreferencesCache {
        #m_store = {};

        // obj: {
        //   onFailure: () => {},
        //   onSuccess: () => {}
        // }
        init(obj = {}) {
            new Request.JSON({
                url: 'api/v2/app/preferences',
                method: 'get',
                noCache: true,
                onFailure: (xhr) => {
                    if (typeof obj.onFailure === 'function')
                        obj.onFailure(xhr);
                },
                onSuccess: (responseJSON, responseText) => {
                    if (!responseJSON)
                        return;
                    this.#m_store = structuredClone(responseJSON);

                    if (typeof obj.onSuccess === 'function')
                        obj.onSuccess(responseJSON, responseText);
                }
            }).send();
        }

        get() {
            return structuredClone(this.#m_store);
        }

        // obj: {
        //   data: {},
        //   onFailure: () => {},
        //   onSuccess: () => {}
        // }
        set(obj) {
            if (typeof obj !== 'object')
                throw new Error('`obj` is not an object.');
            if (typeof obj.data !== 'object')
                throw new Error('`data` is not an object.');

            new Request({
                url: 'api/v2/app/setPreferences',
                method: 'post',
                data: {
                    'json': JSON.stringify(obj.data)
                },
                onFailure: (xhr) => {
                    if (typeof obj.onFailure === 'function')
                        obj.onFailure(xhr);
                },
                onSuccess: (responseText, responseXML) => {
                    for (const key in obj.data) {
                        if (!Object.hasOwn(obj.data, key))
                            continue;

                        const value = obj.data[key];
                        this.#m_store[key] = value;
                    }

                    if (typeof obj.onSuccess === 'function')
                        obj.onSuccess(responseText, responseXML);
                }
            }).send();
        }
    }

    class QbtVersionCache {
        #m_store = '';

        init() {
            new Request({
                url: 'api/v2/app/version',
                method: 'get',
                noCache: true,
                onSuccess: (responseText) => {
                    if (!responseText)
                        return;
                    this.#m_store = responseText;
                }
            }).send();
        }

        get() {
            return this.#m_store;
        }
    }

    return exports();
})();

Object.freeze(window.qBittorrent.Cache);
