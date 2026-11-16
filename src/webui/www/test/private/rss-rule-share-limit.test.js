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

import { beforeEach, expect, test } from "vitest";

await import("../../private/scripts/rss-rule-share-limit.js");

const helper = window.qBittorrent.RssRuleShareLimit;

const preferences = {
    max_inactive_seeding_time: 30,
    max_ratio: 1,
    max_ratio_act: 0,
    max_seeding_time: 60,
    share_limits_mode: helper.ShareLimitsMode.MatchAny
};

const createController = () => {
    document.body.innerHTML = `
        <select id="ruleShareLimitRatioMode">
            <option value="default">Default</option>
            <option value="unlimited">Unlimited</option>
            <option value="set">Set to</option>
        </select>
        <input type="number" id="ruleShareLimitRatio" value="1.00" step=".01" min="0">
        <select id="ruleShareLimitTotalMinutesMode">
            <option value="default">Default</option>
            <option value="unlimited">Unlimited</option>
            <option value="set">Set to</option>
        </select>
        <input type="number" id="ruleShareLimitTotalMinutes" value="1440" step="1" min="0">
        <select id="ruleShareLimitInactiveMinutesMode">
            <option value="default">Default</option>
            <option value="unlimited">Unlimited</option>
            <option value="set">Set to</option>
        </select>
        <input type="number" id="ruleShareLimitInactiveMinutes" value="1440" step="1" min="0">
        <select id="ruleShareLimitsMode">
            <option value="Default">Default</option>
            <option value="MatchAny">Match any limit</option>
            <option value="MatchAll">Match all the limits</option>
        </select>
        <select id="ruleShareLimitAction">
            <option value="Default">Default</option>
            <option value="Stop">Stop torrent</option>
            <option value="Remove">Remove torrent</option>
            <option value="RemoveWithContent">Remove torrent and its content</option>
            <option value="EnableSuperSeeding">Enable super seeding for torrent</option>
        </select>
        <button id="saveButton">Save</button>
    `;

    return new helper.Controller(
        document,
        () => preferences, {
            fromCategory: "From category",
            defaultWithValue: "Default (%1)",
            fromCategoryWithValue: "From category (%1)"
        });
};

const changeValue = (element, value) => {
    element.value = value;
    element.dispatchEvent(new Event("change"));
};

beforeEach(() => {
    document.body.replaceChildren();
    Object.assign(preferences, {
        max_inactive_seeding_time: 30,
        max_ratio: 1,
        max_ratio_act: 0,
        max_seeding_time: 60,
        share_limits_mode: helper.ShareLimitsMode.MatchAny
    });
});

test("Test RSS share limit mode conversion", () => {
    expect(helper.ShareLimitsMode).toStrictEqual({
        Default: "Default",
        MatchAll: "MatchAll",
        MatchAny: "MatchAny"
    });

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

test("Test RSS share limit category inheritance", () => {
    const globalShareLimits = {
        ratio_limit: 1,
        seeding_time_limit: 60,
        inactive_seeding_time_limit: 30,
        share_limits_mode: helper.ShareLimitsMode.MatchAny,
        share_limit_action: helper.Action.Stop
    };
    const categoryList = {
        "Movies": {
            ratio_limit: 2,
            seeding_time_limit: helper.UseGlobalLimit,
            inactive_seeding_time_limit: helper.NoLimit,
            share_limits_mode: helper.ShareLimitsMode.MatchAll,
            share_limit_action: helper.Action.Default
        },
        "Movies/Archive": {
            ratio_limit: helper.UseGlobalLimit,
            seeding_time_limit: 120,
            inactive_seeding_time_limit: helper.UseGlobalLimit,
            share_limits_mode: helper.ShareLimitsMode.Default,
            share_limit_action: helper.Action.Remove
        }
    };

    expect(helper.parentCategoryName("Shows/Drama/Season 1")).toBe("Shows/Drama");
    expect(helper.parentCategoryName("Movies")).toBe("");
    expect(helper.effectiveShareLimits(categoryList, "Movies/Archive/Old", globalShareLimits)).toStrictEqual({
        ratio_limit: 2,
        seeding_time_limit: 120,
        inactive_seeding_time_limit: helper.NoLimit,
        share_limits_mode: helper.ShareLimitsMode.MatchAll,
        share_limit_action: helper.Action.Remove
    });
    expect(helper.effectiveShareLimits(categoryList, "", globalShareLimits)).toStrictEqual(globalShareLimits);
});

test("Test RSS share limit inherited value labels", () => {
    const labels = {
        [helper.Action.Default]: "",
        [helper.Action.Stop]: "Stop torrent",
        [helper.Action.Remove]: "Remove torrent"
    };
    const actionText = (action) => labels[action] ?? "";
    const labelParams = {
        defaultValue: helper.Action.Default,
        valueText: actionText,
        defaultText: "Default",
        fromCategoryText: "From category",
        defaultWithValueText: "Default (%1)",
        fromCategoryWithValueText: "From category (%1)"
    };

    expect(helper.defaultValueText({
        ...labelParams,
        categoryName: "",
        effectiveValue: helper.Action.Stop
    })).toBe("Default (Stop torrent)");

    expect(helper.defaultValueText({
        ...labelParams,
        categoryName: "Movies/Archive/Old",
        effectiveValue: helper.Action.Remove
    })).toBe("From category (Remove torrent)");

    expect(helper.defaultValueText({
        ...labelParams,
        categoryName: "Uncategorized",
        effectiveValue: helper.Action.Default
    })).toBe("From category");

    expect(helper.defaultValueText({
        ...labelParams,
        categoryName: "Movies",
        effectiveValue: helper.ShareLimitsMode.MatchAll,
        defaultValue: helper.ShareLimitsMode.Default,
        valueText: (mode) => (mode === helper.ShareLimitsMode.MatchAll) ? "Match all the limits" : ""
    })).toBe("From category (Match all the limits)");
});

test("RSS share limit controller preserves explicitly assigned values across mode changes", () => {
    const controller = createController();
    controller.setEnabled(true);
    controller.load({
        ratio_limit: 2.25,
        seeding_time_limit: 90,
        inactive_seeding_time_limit: 45
    });

    const ratioMode = document.getElementById("ruleShareLimitRatioMode");
    const ratio = document.getElementById("ruleShareLimitRatio");
    expect(ratioMode.value).toBe(helper.Mode.Set);
    expect(ratio.value).toBe("2.25");

    changeValue(ratioMode, helper.Mode.Unlimited);
    expect(ratio.value).toBe("");
    expect(ratio.disabled).toBe(true);

    changeValue(ratioMode, helper.Mode.Default);
    expect(ratio.value).toBe("1.00");

    changeValue(ratioMode, helper.Mode.Set);
    expect(ratio.value).toBe("2.25");
    expect(ratio.disabled).toBe(false);
});

test("RSS share limit controller resets assigned state between rules and displays inherited limits", () => {
    const controller = createController();
    controller.setEnabled(true);
    controller.setCategories({
        Movies: {
            ratio_limit: helper.NoLimit,
            seeding_time_limit: 120,
            inactive_seeding_time_limit: helper.UseGlobalLimit,
            share_limits_mode: helper.ShareLimitsMode.MatchAll,
            share_limit_action: helper.Action.Remove
        }
    });

    controller.load({ ratio_limit: 8, category: "" });
    expect(document.getElementById("ruleShareLimitRatio").value).toBe("8.00");

    controller.load({
        category: "Movies",
        ratio_limit: helper.UseGlobalLimit,
        seeding_time_limit: helper.UseGlobalLimit,
        inactive_seeding_time_limit: helper.UseGlobalLimit
    });

    const ratioMode = document.getElementById("ruleShareLimitRatioMode");
    const ratio = document.getElementById("ruleShareLimitRatio");
    expect(ratioMode.options[0].text).toBe("From category");
    expect(ratio.value).toBe("");
    expect(document.getElementById("ruleShareLimitTotalMinutes").value).toBe("120");
    expect(document.getElementById("ruleShareLimitInactiveMinutes").value).toBe("30");
    expect(document.getElementById("ruleShareLimitsMode").options[0].text).toBe("From category (Match all the limits)");
    expect(document.getElementById("ruleShareLimitAction").options[0].text).toBe("From category (Remove torrent)");

    changeValue(ratioMode, helper.Mode.Set);
    expect(ratio.value).toBe("1.00");
});

test("RSS share limit controller gates invalid saves and writes canonical torrent parameters", () => {
    const controller = createController();
    controller.setEnabled(true);

    const totalMinutesMode = document.getElementById("ruleShareLimitTotalMinutesMode");
    const totalMinutes = document.getElementById("ruleShareLimitTotalMinutes");
    changeValue(totalMinutesMode, helper.Mode.Set);
    totalMinutes.value = "";
    totalMinutes.dispatchEvent(new Event("input"));

    const untouchedParams = { category: "Movies" };
    expect(document.getElementById("saveButton").disabled).toBe(true);
    expect(controller.save(untouchedParams)).toBe(false);
    expect(untouchedParams).toStrictEqual({ category: "Movies" });

    totalMinutes.value = "90";
    totalMinutes.dispatchEvent(new Event("input"));
    changeValue(document.getElementById("ruleShareLimitRatioMode"), helper.Mode.Unlimited);
    changeValue(document.getElementById("ruleShareLimitInactiveMinutesMode"), helper.Mode.Default);
    document.getElementById("ruleShareLimitsMode").value = helper.ShareLimitsMode.MatchAll;
    document.getElementById("ruleShareLimitAction").value = helper.Action.RemoveWithContent;

    const torrentParams = { category: "Movies", tags: ["release"] };
    expect(totalMinutes.validity.valid).toBe(true);
    expect(totalMinutesMode.disabled).toBe(false);
    expect(document.getElementById("saveButton").disabled).toBe(false);
    expect(controller.save(torrentParams)).toBe(true);
    expect(torrentParams).toStrictEqual({
        category: "Movies",
        tags: ["release"],
        ratio_limit: helper.NoLimit,
        seeding_time_limit: 90,
        inactive_seeding_time_limit: helper.UseGlobalLimit,
        share_limits_mode: helper.ShareLimitsMode.MatchAll,
        share_limit_action: helper.Action.RemoveWithContent
    });
});
