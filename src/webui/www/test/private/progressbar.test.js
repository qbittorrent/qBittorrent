/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Muhammad Hassan Raza <raihassanraza10@gmail.com>
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

import "../../private/scripts/misc.js";
import "../../private/scripts/progressbar.js";

test("Test ProgressBar numeric value handling", () => {
    const progressBar = new window.qBittorrent.ProgressBar.ProgressBar("12.5");

    expect(progressBar.getValue()).toBe(12.5);
    expect(progressBar.shadowRoot.firstElementChild.textContent).toBe("12.5%");
    expect(progressBar.shadowRoot.lastElementChild.textContent).toBe("12.5%");

    progressBar.setValue("invalid");
    expect(progressBar.getValue()).toBe(0);
    expect(progressBar.shadowRoot.firstElementChild.textContent).toBe("0.0%");

    progressBar.setValue(250);
    expect(progressBar.getValue()).toBe(100);
    expect(progressBar.shadowRoot.firstElementChild.textContent).toBe("100.0%");
});

test("Test ProgressBar bar color setter", () => {
    const progressBar = new window.qBittorrent.ProgressBar.ProgressBar(50);
    const barElement = progressBar.shadowRoot.firstElementChild;
    const trackElement = progressBar.shadowRoot.lastElementChild;

    expect(barElement.style.backgroundColor).toBe("var(--color-background-blue)");
    expect(trackElement.style.backgroundColor).toBe("var(--color-background-default)");

    progressBar.setBarColor("rgb(255, 0, 0)");

    expect(barElement.style.backgroundColor).toBe("rgb(255, 0, 0)");
    expect(trackElement.style.backgroundColor).toBe("var(--color-background-default)");
});
