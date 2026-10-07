/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Andy Ye
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
 * modified versions of it that use the same license as the OpenSSL library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

import { beforeEach, expect, test, vi } from "vitest";

import "../../private/scripts/contextmenu.js";

beforeEach(() => {
    document.body.innerHTML = `
        <div id="target"></div>
        <ul id="menu" class="contextMenu">
            <li class="checkableMenuItem">
                <input type="checkbox">
                <a href="#sequentialDownload">Sequential download</a>
            </li>
        </ul>`;
});

test("Checkable menu items execute their action", () => {
    const requestedValues = [];
    const action = vi.fn((element, ref) => {
        requestedValues.push(ref.getNextItemChecked("sequentialDownload"));
    });
    const contextMenu = new window.qBittorrent.ContextMenu.ContextMenu({
        actions: { sequentialDownload: action },
        menu: "menu",
        targets: "#target"
    });
    const checkbox = document.querySelector("input[type='checkbox']");

    contextMenu.setItemCheckState("sequentialDownload", "unchecked");
    expect(contextMenu.getItemCheckState("sequentialDownload")).toBe("unchecked");
    expect(contextMenu.getNextItemChecked("sequentialDownload")).toBe(true);
    checkbox.click();

    contextMenu.setItemCheckState("sequentialDownload", "checked");
    expect(contextMenu.getItemCheckState("sequentialDownload")).toBe("checked");
    expect(contextMenu.getNextItemChecked("sequentialDownload")).toBe(false);
    checkbox.click();

    contextMenu.setItemCheckState("sequentialDownload", "partial");
    expect(contextMenu.getItemCheckState("sequentialDownload")).toBe("partial");
    expect(contextMenu.getNextItemChecked("sequentialDownload")).toBe(true);
    checkbox.click();

    document.querySelector("a[href$='sequentialDownload']").click();

    expect(action).toHaveBeenCalledTimes(4);
    expect(requestedValues).toStrictEqual([true, false, true, true]);
});
