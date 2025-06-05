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

// This file is the JavaScript implementation of base/utils/fs.cpp

window.qBittorrent ??= {};
window.qBittorrent.Filesystem ??= (() => {
    const exports = () => {
        return {
            PathSeparator: PathSeparator,
            fileExtension: fileExtension,
            fileName: fileName,
            folderName: folderName
        };
    };

    const PathSeparator = "/";

    /**
     * Returns the file extension part of a file name.
     */
    const fileExtension = (filename) => {
        const pointIndex = filename.lastIndexOf(".");
        if (pointIndex === -1)
            return "";
        return filename.substring(pointIndex + 1);
    };

    const fileName = (filepath) => {
        const slashIndex = filepath.lastIndexOf(PathSeparator);
        if (slashIndex === -1)
            return filepath;
        return filepath.substring(slashIndex + 1);
    };

    const folderName = (filepath) => {
        const slashIndex = filepath.lastIndexOf(PathSeparator);
        if (slashIndex === -1)
            return "";
        return filepath.substring(0, slashIndex);
    };

    return exports();
})();
Object.freeze(window.qBittorrent.Filesystem);
