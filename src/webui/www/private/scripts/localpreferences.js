/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2025  Mike Tzou (Chocobo1)
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
window.qBittorrent.LocalPreferences ??= (() => {
    const exports = () => {
        return {
            LocalPreferences: LocalPreferences,
            upgrade: upgrade
        };
    };

    class LocalPreferences {
        get(key, defaultValue) {
            const value = localStorage.getItem(key);
            return ((value === null) && (defaultValue !== undefined))
                ? defaultValue
                : value;
        }

        set(key, value) {
            try {
                localStorage.setItem(key, value);
            }
            catch (err) {
                console.error(err);
            }
        }

        size() {
            return localStorage.length;
        }

        remove(key) {
            try {
                localStorage.removeItem(key);
            }
            catch (err) {
                console.error(err);
            }
        }
    };

    const localPreferences = new LocalPreferences();

    const upgrade = () => {
        const MIGRATION_VERSION = 1;
        const MIGRATION_VERSION_KEY = "MigrationVersion";

        // clean start
        if (localPreferences.size() === 0) {
            localPreferences.set(MIGRATION_VERSION_KEY, MIGRATION_VERSION);
            return;
        }

        // already in use
        const version = Number(localPreferences.get(MIGRATION_VERSION_KEY)); // `0` on first initialization
        if (version !== MIGRATION_VERSION) {
            if (version < 1)
                resetSideFilters();

            localPreferences.set(MIGRATION_VERSION_KEY, MIGRATION_VERSION);
        }
    };

    const resetSideFilters = () => {
        // conditionally reset the filter to default to avoid none selected
        const clear = (key) => {
            const value = Number(localPreferences.get(key));
            if ((value === 1) || (value === 2)) // affected values
                localPreferences.remove(key);
        };
        clear("selected_category");
        clear("selected_tag");
        clear("selected_tracker");
    };

    return exports();
})();
Object.freeze(window.qBittorrent.LocalPreferences);
