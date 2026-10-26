/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  umutcagand <umutcagand@protonmail.com>
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

import "../../private/scripts/file-tree.js";

const { FileNode, FilePriority, FolderNode, TriState } = window.qBittorrent.FileTree;

const createFile = (size, progress, availability, priority = FilePriority.Normal) => {
    const file = new FileNode();
    file.size = size;
    file.progress = progress;
    file.availability = availability;
    file.priority = priority;
    file.checked = file.isIgnored() ? TriState.Unchecked : TriState.Checked;
    return file;
};

const createFolder = (...children) => {
    const folder = new FolderNode();
    for (const child of children) {
        child.root = folder;
        folder.addChild(child);
    }
    return folder;
};

test("Weight folder progress and availability by file size", () => {
    const folder = createFolder(createFile(100, 40, 0.25), createFile(300, 80, 0.75));
    folder.calculateSize();

    expect(folder.size).toBe(400);
    expect(folder.progress).toBe(70);
    expect(folder.availability).toBe(0.625);
    expect(folder.remaining).toBeCloseTo(120);
    expect(folder.priority).toBe(FilePriority.Normal);
    expect(folder.checked).toBe(TriState.Checked);
});

test.each([FilePriority.Normal, FilePriority.High, FilePriority.Maximum])("Exclude ignored files from folder averages at priority %i", (priority) => {
    const folder = createFolder(createFile(100, 75, 0.5, priority), createFile(300, 100, 1, FilePriority.Ignored));
    folder.calculateSize();

    expect(folder.size).toBe(400);
    expect(folder.progress).toBe(75);
    expect(folder.availability).toBe(0.5);
    expect(folder.remaining).toBe(25);
    expect(folder.priority).toBe(FilePriority.Mixed);
    expect(folder.checked).toBe(TriState.Partial);
});

test("Use selected file sizes through nested folders", () => {
    const mixedFolder = createFolder(createFile(100, 100, 1), createFile(900, 0, 0, FilePriority.Ignored));
    const ignoredFolder = createFolder(createFile(1000, 100, 1, FilePriority.Ignored));
    const root = createFolder(mixedFolder, ignoredFolder, createFile(100, 50, 0.25));
    root.calculateSize();

    expect(mixedFolder.size).toBe(1000);
    expect(mixedFolder.progress).toBe(100);
    expect(mixedFolder.availability).toBe(1);
    expect(root.size).toBe(2100);
    expect(root.progress).toBe(75);
    expect(root.availability).toBe(0.625);
    expect(root.remaining).toBe(50);
    expect(root.priority).toBe(FilePriority.Mixed);
    expect(root.checked).toBe(TriState.Partial);
});

test("Complete folders with no selected data", () => {
    const folder = createFolder(createFile(100, 0, 0, FilePriority.Ignored), createFile(300, 50, 1, FilePriority.Ignored));
    folder.calculateSize();

    expect(folder.size).toBe(400);
    expect(folder.progress).toBe(100);
    expect(folder.availability).toBe(0);
    expect(folder.remaining).toBe(0);
    expect(folder.priority).toBe(FilePriority.Ignored);
    expect(folder.checked).toBe(TriState.Unchecked);
});

test("Complete zero-length folders without producing NaN", () => {
    const folder = createFolder(createFile(0, 0, 0));
    folder.calculateSize();

    expect(folder.size).toBe(0);
    expect(folder.progress).toBe(100);
    expect(folder.availability).toBe(0);
    expect(folder.remaining).toBe(0);
});

test("Recalculate selected sizes after priorities change", () => {
    const ignoredFile = createFile(300, 0, 0, FilePriority.Ignored);
    const nestedFolder = createFolder(createFile(100, 100, 1), ignoredFile);
    const root = createFolder(nestedFolder);
    root.calculateSize();
    expect(root.progress).toBe(100);

    ignoredFile.priority = FilePriority.Normal;
    ignoredFile.checked = TriState.Checked;
    ignoredFile.calculateRemaining();
    root.calculateSize();
    expect(root.progress).toBe(25);
    expect(root.availability).toBe(0.25);
    expect(root.checked).toBe(TriState.Checked);
    expect(root.remaining).toBe(300);

    ignoredFile.priority = FilePriority.Ignored;
    ignoredFile.checked = TriState.Unchecked;
    ignoredFile.calculateRemaining();
    root.calculateSize();
    root.calculateSize();
    expect(root.progress).toBe(100);
    expect(root.availability).toBe(1);
    expect(root.checked).toBe(TriState.Partial);
    expect(root.remaining).toBe(0);
});
