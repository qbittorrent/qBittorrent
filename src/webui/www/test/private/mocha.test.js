/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Peder
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
 * License in all respects for all of the code used other than the "OpenSSL" library.
 * If you modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

import { readFileSync } from "node:fs";
import { resolve } from "node:path";

import { expect, test } from "vitest";
import { Window } from "happy-dom";

const readScript = fileName => readFileSync(resolve("private/scripts/lib", fileName), "utf8");

const createWindow = mochaFileName => {
    const window = new Window();

    for (const fileName of [
        "MooTools-Core-1.6.0-compat-compressed.js",
        "MooTools-More-1.6.0-compat-compressed.js",
        mochaFileName
    ])
        window.eval(readScript(fileName));

    return window;
};

test.each(["mocha-0.9.6.js", "mocha.min.js"])("%s keeps sortable panels idle after a header anchor click", mochaFileName => {
    const window = createWindow(mochaFileName);
    const container = new window.Element("div", { styles: { height: 200 } }).inject(window.document.body);

    window.MochaUI.Desktop.desktop = container;

    const column = new window.MochaUI.Column({
        container: container,
        id: "column",
        placement: "main"
    });
    const panelWrapper = new window.Element("div", { class: "panelWrapper" }).inject(column.columnEl);
    const panel = new window.Element("div", { class: "panel" }).inject(panelWrapper);
    const panelHeader = new window.Element("div", { class: "panel-header" }).inject(panel);
    const tab = new window.Element("a", { href: "#tab" }).inject(panelHeader);
    const sortables = container.retrieve("sortables");

    sortables.addItems(panelWrapper);

    tab.dispatchEvent(new window.MouseEvent("mousedown", { bubbles: true }));
    tab.dispatchEvent(new window.MouseEvent("mouseup", { bubbles: true }));

    expect(sortables.idle).toBe(true);

    panelHeader.dispatchEvent(new window.MouseEvent("mousedown", { bubbles: true }));

    expect(sortables.idle).toBe(false);

    panelHeader.dispatchEvent(new window.MouseEvent("mouseup", { bubbles: true }));
    window.close();
});
