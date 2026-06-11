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

    const exports = () => {
        return {
            Action: Action,
            Mode: Mode,
            NoLimit: NoLimit,
            UseGlobalLimit: UseGlobalLimit,
            actionFromGlobalPreference: actionFromGlobalPreference,
            defaultActionText: defaultActionText,
            effectiveShareLimitAction: effectiveShareLimitAction,
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

    const defaultActionText = ({ categoryName, categoryList, globalAction, actionText, defaultText, fromCategoryText, defaultWithActionText, fromCategoryWithActionText }) => {
        const normalizedCategoryName = String(categoryName ?? "");
        const effectiveAction = (normalizedCategoryName === "")
            ? globalAction
            : effectiveShareLimitAction(categoryList, normalizedCategoryName, globalAction);
        if (effectiveAction === Action.Default)
            return (normalizedCategoryName === "") ? defaultText : fromCategoryText;

        const effectiveActionText = actionText(effectiveAction);

        if (normalizedCategoryName === "")
            return effectiveActionText ? defaultWithActionText.replace("%1", effectiveActionText) : defaultText;

        return effectiveActionText ? fromCategoryWithActionText.replace("%1", effectiveActionText) : fromCategoryText;
    };

    const effectiveShareLimitAction = (categoryList, categoryName, globalAction) => {
        let currentCategoryName = String(categoryName ?? "");
        while (currentCategoryName !== "") {
            const shareLimitAction = String(categoryList[currentCategoryName]?.share_limit_action ?? Action.Default);
            if (shareLimitAction !== Action.Default)
                return shareLimitAction;

            currentCategoryName = parentCategoryName(currentCategoryName);
        }

        return globalAction;
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

    return exports();
})();
Object.freeze(window.qBittorrent.RssRuleShareLimit);
