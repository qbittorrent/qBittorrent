/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026 qBittorrent project
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

import { expect, test } from "vitest";

window.qBittorrent ??= {};
window.qBittorrent.FileTree = {
    FilePriority: {
        Ignored: 0,
        Normal: 1,
        High: 6,
        Maximum: 7,
        Mixed: 2
    },
    TriState: {
        Unchecked: 0,
        Checked: 1,
        Partial: 2
    }
};
window.qBittorrent.Filesystem = {
    PathSeparator: "/"
};

await import("../../private/scripts/torrent-content.js");

test("Test getPathRelativeToRootFolder()", () => {
    const getPathRelativeToRootFolder = window.qBittorrent.TorrentContent.getPathRelativeToRootFolder;

    expect(getPathRelativeToRootFolder("torrentA/subdir/file1")).toBe("subdir/file1");
    expect(getPathRelativeToRootFolder("file1")).toBe("");
});

test("Test joinAbsolutePath()", () => {
    const joinAbsolutePath = window.qBittorrent.TorrentContent.joinAbsolutePath;

    expect(joinAbsolutePath("/home/user/torrents", "subdir/file1")).toBe("/home/user/torrents/subdir/file1");
    expect(joinAbsolutePath("C:\\Downloads\\torrentA", "subdir/file1")).toBe("C:\\Downloads\\torrentA\\subdir\\file1");
});

test("Test getNodeAbsolutePath() with root_path", () => {
    const getNodeAbsolutePath = window.qBittorrent.TorrentContent.getNodeAbsolutePath;

    expect(getNodeAbsolutePath({ path: "torrentA/subdir/file1", isFolder: false },
        "/home/user/torrents/torrentA",
        "/home/user/torrents/torrentA",
        false
    )).toBe("/home/user/torrents/torrentA/subdir/file1");
});

test("Test getNodeAbsolutePath() fallback paths", () => {
    const getNodeAbsolutePath = window.qBittorrent.TorrentContent.getNodeAbsolutePath;

    expect(getNodeAbsolutePath({ path: "subdir/file1", isFolder: false },
        "",
        "/home/user/torrents",
        false
    )).toBe("/home/user/torrents/subdir/file1");

    expect(getNodeAbsolutePath({ path: "file1", isFolder: false },
        "",
        "/home/user/torrents/file1",
        true
    )).toBe("/home/user/torrents/file1");
});
