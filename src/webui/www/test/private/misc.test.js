/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2025  Mike Tzou (Chocobo1)
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

import { expect, test, vi } from "vitest";

import "../../private/scripts/misc.js";

test("Test filterInPlace()", () => {
    const filterInPlace = (array, predicate) => {
        window.qBittorrent.Misc.filterInPlace(array, predicate);
        return array;
    };

    expect(filterInPlace([], (() => true))).toStrictEqual([]);
    expect(filterInPlace([], (() => false))).toStrictEqual([]);
    expect(filterInPlace([1, 2, 3, 4], (() => true))).toStrictEqual([1, 2, 3, 4]);
    expect(filterInPlace([1, 2, 3, 4], (() => false))).toStrictEqual([]);
    expect(filterInPlace([1, 2, 3, 4], (x => (x % 2) === 0))).toStrictEqual([2, 4]);
});

test("Test getTorrentStateInfo()", () => {
    const getTorrentStateInfo = window.qBittorrent.Misc.getTorrentStateInfo;
    const fallbackStateInfo = getTorrentStateInfo("notARealState");

    const apiStates = [
        "error",
        "missingFiles",
        "uploading",
        "stoppedUP",
        "queuedUP",
        "stalledUP",
        "checkingUP",
        "forcedUP",
        "downloading",
        "metaDL",
        "forcedMetaDL",
        "stoppedDL",
        "queuedDL",
        "stalledDL",
        "checkingDL",
        "forcedDL",
        "checkingResumeData",
        "moving",
        "unknown"
    ];

    for (const state of apiStates) {
        const stateInfo = getTorrentStateInfo(state);

        expect(stateInfo).not.toBe(fallbackStateInfo);
        expect(stateInfo.stateIconClass).not.toBe("stateUnknown");
        expect(stateInfo.statusText).toBeTypeOf("string");
    }

    expect(getTorrentStateInfo("downloading")).toMatchObject({
        sortOrder: 1,
        stateIconClass: "stateDownloading",
        progressColor: "var(--color-progress-downloading)",
        statusText: "QBT_TR(Downloading)QBT_TR[CONTEXT=TransferListDelegate]",
        matchesDownloadFilter: true
    });

    expect(getTorrentStateInfo("checkingUP")).toMatchObject({
        sortOrder: 11,
        progressColor: "var(--color-progress-checking)",
        matchesSeedingFilter: true,
        matchesCompletedFilter: true,
        isChecking: true
    });

    expect(getTorrentStateInfo("queuedForChecking")).toMatchObject({
        sortOrder: 10,
        stateIconClass: "stateChecking",
        progressColor: "var(--color-progress-checking)",
        statusText: "QBT_TR(Queued for checking)QBT_TR[CONTEXT=TransferListDelegate]",
        isQueued: true,
        isChecking: false
    });

    expect(getTorrentStateInfo("stoppedUP")).toMatchObject({
        sortOrder: 14,
        stateIconClass: "stateStoppedUP",
        progressColor: "var(--color-progress-stopped)",
        matchesCompletedFilter: true,
        isStopped: true
    });

    expect(getTorrentStateInfo("metaDL")).toMatchObject({
        stateIconClass: "stateDownloading",
        isMetadataDownloading: true
    });

    expect(getTorrentStateInfo("unknown")).toMatchObject({
        sortOrder: -1,
        stateIconClass: "stateError",
        progressColor: "var(--color-progress-error)",
        statusText: "QBT_TR(Unknown)QBT_TR[CONTEXT=HttpServer]",
        isErrored: true,
        hidesAvailability: false
    });

    expect(fallbackStateInfo).toMatchObject({
        sortOrder: -1,
        stateIconClass: "stateUnknown",
        progressColor: undefined,
        hidesAvailability: false,
        isStopped: false
    });
});

test("Test shouldShowAvailability()", () => {
    const shouldShowAvailability = window.qBittorrent.Misc.shouldShowAvailability;

    expect(shouldShowAvailability({
        hasMetadata: true,
        progress: 0.4,
        state: "downloading"
    })).toBe(true);

    expect(shouldShowAvailability({
        hasMetadata: true,
        progress: 0.4,
        state: "unknown"
    })).toBe(true);

    expect(shouldShowAvailability({
        hasMetadata: true,
        progress: 0.4,
        state: "error"
    })).toBe(false);

    expect(shouldShowAvailability({
        hasMetadata: true,
        progress: 0.4,
        state: "missingFiles"
    })).toBe(false);

    expect(shouldShowAvailability({
        hasMetadata: true,
        progress: 0.4,
        state: "queuedDL"
    })).toBe(false);

    expect(shouldShowAvailability({
        hasMetadata: false,
        progress: 0.4,
        state: "downloading"
    })).toBe(false);

    expect(shouldShowAvailability({
        hasMetadata: true,
        progress: 1,
        state: "downloading"
    })).toBe(false);
});

test("Test toFixedPointString()", () => {
    const toFixedPointString = window.qBittorrent.Misc.toFixedPointString;

    expect(toFixedPointString(0, 0)).toBe("0");
    expect(toFixedPointString(0, 1)).toBe("0.0");
    expect(toFixedPointString(0, 2)).toBe("0.00");

    expect(toFixedPointString(0.1, 0)).toBe("0");
    expect(toFixedPointString(0.1, 1)).toBe("0.1");
    expect(toFixedPointString(0.1, 2)).toBe("0.10");

    expect(toFixedPointString(-0.1, 0)).toBe("0");
    expect(toFixedPointString(-0.1, 1)).toBe("-0.1");
    expect(toFixedPointString(-0.1, 2)).toBe("-0.10");

    expect(toFixedPointString(1.005, 0)).toBe("1");
    expect(toFixedPointString(1.005, 1)).toBe("1.0");
    expect(toFixedPointString(1.005, 2)).toBe("1.00");
    expect(toFixedPointString(1.005, 3)).toBe("1.005");
    expect(toFixedPointString(1.005, 4)).toBe("1.0050");

    expect(toFixedPointString(-1.005, 0)).toBe("-1");
    expect(toFixedPointString(-1.005, 1)).toBe("-1.0");
    expect(toFixedPointString(-1.005, 2)).toBe("-1.00");
    expect(toFixedPointString(-1.005, 3)).toBe("-1.005");
    expect(toFixedPointString(-1.005, 4)).toBe("-1.0050");

    expect(toFixedPointString(35.855, 0)).toBe("35");
    expect(toFixedPointString(35.855, 1)).toBe("35.8");
    expect(toFixedPointString(35.855, 2)).toBe("35.85");
    expect(toFixedPointString(35.855, 3)).toBe("35.855");
    expect(toFixedPointString(35.855, 4)).toBe("35.8550");

    expect(toFixedPointString(-35.855, 0)).toBe("-35");
    expect(toFixedPointString(-35.855, 1)).toBe("-35.8");
    expect(toFixedPointString(-35.855, 2)).toBe("-35.85");
    expect(toFixedPointString(-35.855, 3)).toBe("-35.855");
    expect(toFixedPointString(-35.855, 4)).toBe("-35.8550");

    expect(toFixedPointString(100, 0)).toBe("100");
    expect(toFixedPointString(100, 1)).toBe("100.0");
    expect(toFixedPointString(100, 2)).toBe("100.00");

    expect(toFixedPointString(-100, 0)).toBe("-100");
    expect(toFixedPointString(-100, 1)).toBe("-100.0");
    expect(toFixedPointString(-100, 2)).toBe("-100.00");
});

test("Test formatDate() - Format Coverage", () => {
    const formatDate = window.qBittorrent.Misc.formatDate;
    const testDate = new Date(2025, 7, 23, 22, 32, 46); // Aug 23, 2025 10:32:46 PM

    expect(formatDate(testDate, "MM/dd/yyyy, h:mm:ss AM/PM")).toBe("08/23/2025, 10:32:46 PM");
    expect(formatDate(testDate, "MM/dd/yyyy, HH:mm:ss")).toBe("08/23/2025, 22:32:46");
    expect(formatDate(testDate, "dd/MM/yyyy, HH:mm:ss")).toBe("23/08/2025, 22:32:46");
    expect(formatDate(testDate, "yyyy-MM-dd HH:mm:ss")).toBe("2025-08-23 22:32:46");
    expect(formatDate(testDate, "yyyy/MM/dd HH:mm:ss")).toBe("2025/08/23 22:32:46");
    expect(formatDate(testDate, "dd.MM.yyyy, HH:mm:ss")).toBe("23.08.2025, 22:32:46");
    expect(formatDate(testDate, "MMM dd, yyyy, h:mm:ss AM/PM")).toBe("Aug 23, 2025, 10:32:46 PM");
    expect(formatDate(testDate, "dd MMM yyyy, HH:mm:ss")).toBe("23 Aug 2025, 22:32:46");
});

test("Test formatDate() - Fallback Behavior", () => {
    // Mock ClientData.get
    const mockGet = vi.fn().mockReturnValue("default");
    const originalParent = window.parent;

    window.parent = {
        qBittorrent: {
            ClientData: {
                get: mockGet
            }
        }
    };

    const formatDate = window.qBittorrent.Misc.formatDate;
    const testDate = new Date(2025, 7, 23, 22, 32, 46); // Aug 23, 2025 10:32:46 PM
    const expectedDefault = testDate.toLocaleString();

    // Test that "default" format uses toLocaleString()
    expect(formatDate(testDate, "default")).toBe(expectedDefault);

    // Test default behavior when no format argument is provided
    expect(mockGet).toHaveBeenCalledTimes(0);
    expect(formatDate(testDate)).toBe(expectedDefault);
    expect(mockGet).toHaveBeenCalledWith("date_format");
    expect(mockGet).toHaveBeenCalledTimes(1);

    // Test with unknown/invalid format strings
    expect(formatDate(testDate, "invalid-format")).toBe(expectedDefault);
    expect(formatDate(testDate, "")).toBe(expectedDefault);
    expect(formatDate(testDate, null)).toBe(expectedDefault);

    expect(mockGet).toHaveBeenCalledTimes(1);
    expect(formatDate(testDate, undefined)).toBe(expectedDefault);
    expect(mockGet).toHaveBeenCalledWith("date_format");
    expect(mockGet).toHaveBeenCalledTimes(2);

    // Restore original window.parent
    window.parent = originalParent;
});
