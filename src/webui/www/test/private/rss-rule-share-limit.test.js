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

await import("../../private/scripts/rss-rule-share-limit.js");

const helper = window.qBittorrent.RssRuleShareLimit;

test("Test RSS share limit mode conversion", () => {
    expect(helper.modeFromLimit(helper.UseGlobalLimit)).toBe(helper.Mode.Default);
    expect(helper.modeFromLimit(helper.NoLimit)).toBe(helper.Mode.Unlimited);
    expect(helper.modeFromLimit(1.5)).toBe(helper.Mode.Set);

    expect(helper.limitFromMode(helper.Mode.Default, 1.5)).toBe(helper.UseGlobalLimit);
    expect(helper.limitFromMode(helper.Mode.Unlimited, 1.5)).toBe(helper.NoLimit);
    expect(helper.limitFromMode(helper.Mode.Set, "2.25")).toBe(2.25);
    expect(helper.limitFromMode("invalid", "2.25")).toBe(helper.UseGlobalLimit);
});

test("Test RSS share limit action from global preference", () => {
    expect(helper.actionFromGlobalPreference(0)).toBe(helper.Action.Stop);
    expect(helper.actionFromGlobalPreference(1)).toBe(helper.Action.Remove);
    expect(helper.actionFromGlobalPreference(2)).toBe(helper.Action.EnableSuperSeeding);
    expect(helper.actionFromGlobalPreference(3)).toBe(helper.Action.RemoveWithContent);
    expect(helper.actionFromGlobalPreference(undefined)).toBe(helper.Action.Default);
});

test("Test RSS share limit action inheritance", () => {
    const categoryList = {
        "Movies": { share_limit_action: helper.Action.Default },
        "Movies/Archive": { share_limit_action: helper.Action.Remove },
        "Shows/Drama": { share_limit_action: helper.Action.Default }
    };

    expect(helper.parentCategoryName("Shows/Drama/Season 1")).toBe("Shows/Drama");
    expect(helper.parentCategoryName("Movies")).toBe("");
    expect(helper.effectiveShareLimitAction(categoryList, "Movies/Archive/Old", helper.Action.Stop)).toBe(helper.Action.Remove);
    expect(helper.effectiveShareLimitAction(categoryList, "Shows/Drama/Season 1", helper.Action.Stop)).toBe(helper.Action.Stop);
    expect(helper.effectiveShareLimitAction(categoryList, "", helper.Action.Stop)).toBe(helper.Action.Stop);
});

test("Test RSS share limit inherited action labels", () => {
    const labels = {
        [helper.Action.Default]: "",
        [helper.Action.Stop]: "Stop torrent",
        [helper.Action.Remove]: "Remove torrent"
    };
    const actionText = (action) => labels[action] ?? "";
    const labelParams = {
        categoryList: {
            "Movies": { share_limit_action: helper.Action.Remove },
            "Movies/Archive": { share_limit_action: helper.Action.Default }
        },
        actionText: actionText,
        defaultText: "Default",
        fromCategoryText: "From category",
        defaultWithActionText: "Default (%1)",
        fromCategoryWithActionText: "From category (%1)"
    };

    expect(helper.defaultActionText({
        ...labelParams,
        categoryName: "",
        globalAction: helper.Action.Stop
    })).toBe("Default (Stop torrent)");

    expect(helper.defaultActionText({
        ...labelParams,
        categoryName: "Movies/Archive/Old",
        globalAction: helper.Action.Stop
    })).toBe("From category (Remove torrent)");

    expect(helper.defaultActionText({
        ...labelParams,
        categoryName: "Uncategorized",
        globalAction: helper.Action.Default
    })).toBe("From category");
});
