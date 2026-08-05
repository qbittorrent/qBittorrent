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

"use strict";

window.qBittorrent ??= {};
window.qBittorrent.RssRuleShareLimit ??= (() => {
    const UseGlobalLimit = -2;
    const NoLimit = -1;

    const Mode = Object.freeze({
        Default: "default",
        Set: "set",
        Unlimited: "unlimited"
    });

    const Action = Object.freeze({
        Default: "Default",
        EnableSuperSeeding: "EnableSuperSeeding",
        Remove: "Remove",
        RemoveWithContent: "RemoveWithContent",
        Stop: "Stop"
    });

    const ShareLimitsMode = Object.freeze({
        Default: "Default",
        MatchAll: "MatchAll",
        MatchAny: "MatchAny"
    });

    const exports = () => {
        return {
            Action: Action,
            Controller: Controller,
            Mode: Mode,
            NoLimit: NoLimit,
            ShareLimitsMode: ShareLimitsMode,
            UseGlobalLimit: UseGlobalLimit,
            actionFromGlobalPreference: actionFromGlobalPreference,
            defaultValueText: defaultValueText,
            effectiveShareLimits: effectiveShareLimits,
            limitFromMode: limitFromMode,
            modeFromLimit: modeFromLimit,
            parentCategoryName: parentCategoryName
        };
    };

    const actionFromGlobalPreference = (preferenceValue) => {
        switch (Number(preferenceValue)) {
            case 0:
                return Action.Stop;
            case 1:
                return Action.Remove;
            case 2:
                return Action.EnableSuperSeeding;
            case 3:
                return Action.RemoveWithContent;
        }

        return Action.Default;
    };

    const defaultValueText = ({ categoryName, effectiveValue, defaultValue, valueText, defaultText, fromCategoryText, defaultWithValueText, fromCategoryWithValueText }) => {
        const normalizedCategoryName = String(categoryName ?? "");
        if (effectiveValue === defaultValue)
            return (normalizedCategoryName === "") ? defaultText : fromCategoryText;

        const effectiveValueText = valueText(effectiveValue);

        if (normalizedCategoryName === "")
            return effectiveValueText ? defaultWithValueText.replace("%1", effectiveValueText) : defaultText;

        return effectiveValueText ? fromCategoryWithValueText.replace("%1", effectiveValueText) : fromCategoryText;
    };

    const effectiveShareLimits = (categoryList, categoryName, globalShareLimits) => {
        const result = { ...globalShareLimits };
        const categoryNames = [];
        for (let currentCategoryName = String(categoryName ?? ""); currentCategoryName !== ""; currentCategoryName = parentCategoryName(currentCategoryName))
            categoryNames.push(currentCategoryName);

        const fields = [
            ["ratio_limit", UseGlobalLimit],
            ["seeding_time_limit", UseGlobalLimit],
            ["inactive_seeding_time_limit", UseGlobalLimit],
            ["share_limits_mode", ShareLimitsMode.Default],
            ["share_limit_action", Action.Default]
        ];

        for (const currentCategoryName of categoryNames.reverse()) {
            const category = categoryList[currentCategoryName];
            if (category === undefined)
                continue;

            for (const [field, defaultValue] of fields) {
                const value = category[field];
                if ((value !== undefined) && (value !== defaultValue))
                    result[field] = value;
            }
        }

        return result;
    };

    const limitFromMode = (mode, value) => {
        switch (mode) {
            case Mode.Default:
                return UseGlobalLimit;
            case Mode.Unlimited:
                return NoLimit;
            case Mode.Set:
                return Number(value);
        }

        return UseGlobalLimit;
    };

    const modeFromLimit = (limitValue) => {
        if (limitValue === UseGlobalLimit)
            return Mode.Default;
        if (limitValue === NoLimit)
            return Mode.Unlimited;

        return Mode.Set;
    };

    const parentCategoryName = (categoryName) => {
        const separatorIndex = categoryName.lastIndexOf("/");
        return (separatorIndex >= 0) ? categoryName.slice(0, separatorIndex) : "";
    };

    class Controller {
        constructor(document, getPreferences, texts) {
            this.getPreferences = getPreferences;
            this.texts = texts;
            this.categoryList = {};

            this.actionElement = document.getElementById("ruleShareLimitAction");
            this.inactiveMinutesElement = document.getElementById("ruleShareLimitInactiveMinutes");
            this.inactiveMinutesModeElement = document.getElementById("ruleShareLimitInactiveMinutesMode");
            this.ratioElement = document.getElementById("ruleShareLimitRatio");
            this.ratioModeElement = document.getElementById("ruleShareLimitRatioMode");
            this.totalMinutesElement = document.getElementById("ruleShareLimitTotalMinutes");
            this.totalMinutesModeElement = document.getElementById("ruleShareLimitTotalMinutesMode");
            this.shareLimitsModeElement = document.getElementById("ruleShareLimitsMode");
            this.saveButton = document.getElementById("saveButton");
            this.defaultText = this.ratioModeElement.options[0].text;

            this.limitControls = [{
                    assignedValue: "1.00",
                    field: "ratio_limit",
                    formatter: (value) => Number(value).toFixed(2),
                    initialAssignedValue: "1.00",
                    modeElement: this.ratioModeElement,
                    previousMode: Mode.Default,
                    valueElement: this.ratioElement
                },
                {
                    assignedValue: "1440",
                    field: "seeding_time_limit",
                    formatter: String,
                    initialAssignedValue: "1440",
                    modeElement: this.totalMinutesModeElement,
                    previousMode: Mode.Default,
                    valueElement: this.totalMinutesElement
                },
                {
                    assignedValue: "1440",
                    field: "inactive_seeding_time_limit",
                    formatter: String,
                    initialAssignedValue: "1440",
                    modeElement: this.inactiveMinutesModeElement,
                    previousMode: Mode.Default,
                    valueElement: this.inactiveMinutesElement
                }
            ];

            for (const control of this.limitControls) {
                control.modeElement.addEventListener("change", () => {
                    if (control.previousMode === Mode.Set)
                        control.assignedValue = control.valueElement.value;
                    control.previousMode = control.modeElement.value;
                    this.displayLimitValue(control);
                    this.updateState();
                });
            }

            this.ratioElement.addEventListener("input", () => this.updateState());
            this.totalMinutesElement.addEventListener("input", () => this.updateState());
            this.inactiveMinutesElement.addEventListener("input", () => this.updateState());

            this.setEnabled(false);
            this.reset();
        }

        defaultValueText(categoryName, effectiveValue, defaultValue, selectElement) {
            return defaultValueText({
                categoryName: categoryName,
                effectiveValue: effectiveValue,
                defaultValue: defaultValue,
                valueText: (value) => this.selectValueText(selectElement, value),
                defaultText: this.defaultText,
                fromCategoryText: this.texts.fromCategory,
                defaultWithValueText: this.texts.defaultWithValue,
                fromCategoryWithValueText: this.texts.fromCategoryWithValue
            });
        }

        displayLimitValue(control) {
            if (control.modeElement.value === Mode.Set)
                control.valueElement.value = control.assignedValue;
            else if ((control.modeElement.value === Mode.Default) && (this.defaultShareLimits[control.field] >= 0))
                control.valueElement.value = control.formatter(this.defaultShareLimits[control.field]);
            else
                control.valueElement.value = "";
        }

        globalShareLimits() {
            const preferences = this.getPreferences();
            return {
                ratio_limit: Number(preferences.max_ratio),
                seeding_time_limit: Number(preferences.max_seeding_time),
                inactive_seeding_time_limit: Number(preferences.max_inactive_seeding_time),
                share_limits_mode: String(preferences.share_limits_mode),
                share_limit_action: actionFromGlobalPreference(preferences.max_ratio_act)
            };
        }

        isValid() {
            return this.isValidMode(this.ratioModeElement, this.ratioElement)
                && this.isValidMode(this.totalMinutesModeElement, this.totalMinutesElement)
                && this.isValidMode(this.inactiveMinutesModeElement, this.inactiveMinutesElement);
        }

        isValidMode(modeElement, valueElement) {
            if (modeElement.value !== Mode.Set)
                return true;

            return (valueElement.value !== "") && valueElement.validity.valid;
        }

        load(torrentParams) {
            const categoryName = String(torrentParams.category ?? "");
            const ratioLimit = Number(torrentParams.ratio_limit ?? UseGlobalLimit);
            const seedingTimeLimit = Number(torrentParams.seeding_time_limit ?? UseGlobalLimit);
            const inactiveSeedingTimeLimit = Number(torrentParams.inactive_seeding_time_limit ?? UseGlobalLimit);
            const shareLimitsMode = String(torrentParams.share_limits_mode ?? ShareLimitsMode.Default);

            this.reset();
            this.updateDefaults(categoryName);
            this.loadLimit(this.limitControls[0], ratioLimit);
            this.loadLimit(this.limitControls[1], seedingTimeLimit);
            this.loadLimit(this.limitControls[2], inactiveSeedingTimeLimit);

            this.shareLimitsModeElement.value = shareLimitsMode;
            if (this.shareLimitsModeElement.value === "")
                this.shareLimitsModeElement.value = ShareLimitsMode.Default;

            this.actionElement.value = (torrentParams.share_limit_action ?? Action.Default);
            if (this.actionElement.value === "")
                this.actionElement.value = Action.Default;

            this.updateState();
        }

        loadLimit(control, limitValue) {
            control.modeElement.value = modeFromLimit(limitValue);
            control.previousMode = control.modeElement.value;
            if (control.modeElement.value === Mode.Set)
                control.assignedValue = control.formatter(limitValue);
            this.displayLimitValue(control);
        }

        reset() {
            for (const control of this.limitControls) {
                control.assignedValue = control.initialAssignedValue;
                control.modeElement.value = Mode.Default;
                control.previousMode = Mode.Default;
            }
            this.shareLimitsModeElement.value = ShareLimitsMode.Default;
            this.actionElement.value = Action.Default;

            this.updateDefaults("");
            this.updateState();
        }

        save(torrentParams) {
            if (!this.isValid())
                return false;

            torrentParams.ratio_limit = limitFromMode(this.ratioModeElement.value, this.ratioElement.value);
            torrentParams.seeding_time_limit = limitFromMode(this.totalMinutesModeElement.value, this.totalMinutesElement.value);
            torrentParams.inactive_seeding_time_limit = limitFromMode(this.inactiveMinutesModeElement.value, this.inactiveMinutesElement.value);
            torrentParams.share_limits_mode = this.shareLimitsModeElement.value;
            torrentParams.share_limit_action = this.actionElement.value;

            return true;
        }

        selectValueText(selectElement, value) {
            for (const option of selectElement.options) {
                if (option.value === value)
                    return option.text;
            }

            return "";
        }

        setCategories(categoryList) {
            this.categoryList = categoryList;
        }

        setEnabled(enabled) {
            this.ratioModeElement.disabled = !enabled;
            this.totalMinutesModeElement.disabled = !enabled;
            this.inactiveMinutesModeElement.disabled = !enabled;
            this.shareLimitsModeElement.disabled = !enabled;

            this.updateState();
        }

        updateDefaults(categoryName) {
            const normalizedCategoryName = String(categoryName ?? "");
            const defaultModeText = (normalizedCategoryName === "") ? this.defaultText : this.texts.fromCategory;
            this.defaultShareLimits = effectiveShareLimits(this.categoryList, normalizedCategoryName, this.globalShareLimits());

            this.ratioModeElement.options[0].text = defaultModeText;
            this.totalMinutesModeElement.options[0].text = defaultModeText;
            this.inactiveMinutesModeElement.options[0].text = defaultModeText;
            this.shareLimitsModeElement.options[0].text = this.defaultValueText(normalizedCategoryName, this.defaultShareLimits.share_limits_mode, ShareLimitsMode.Default, this.shareLimitsModeElement);
            this.actionElement.options[0].text = this.defaultValueText(normalizedCategoryName, this.defaultShareLimits.share_limit_action, Action.Default, this.actionElement);

            for (const control of this.limitControls) {
                if (control.modeElement.value === Mode.Default)
                    this.displayLimitValue(control);
            }
        }

        updateState() {
            const controlsEnabled = !this.ratioModeElement.disabled;

            this.ratioElement.disabled = this.ratioModeElement.disabled || (this.ratioModeElement.value !== Mode.Set);
            this.totalMinutesElement.disabled = this.totalMinutesModeElement.disabled || (this.totalMinutesModeElement.value !== Mode.Set);
            this.inactiveMinutesElement.disabled = this.inactiveMinutesModeElement.disabled || (this.inactiveMinutesModeElement.value !== Mode.Set);
            this.actionElement.disabled = !controlsEnabled;

            this.saveButton.disabled = !controlsEnabled || !this.isValid();
        }
    }

    return exports();
})();
Object.freeze(window.qBittorrent.RssRuleShareLimit);
