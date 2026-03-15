/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014  Gabriele <pmzqla.git@gmail.com>
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
window.qBittorrent.Misc ??= (() => {
    const exports = () => {
        return {
            getHost: getHost,
            getTorrentStateInfo: getTorrentStateInfo,
            shouldShowAvailability: shouldShowAvailability,
            createDebounceHandler: createDebounceHandler,
            filterInPlace: filterInPlace,
            friendlyUnit: friendlyUnit,
            friendlyDuration: friendlyDuration,
            friendlyPercentage: friendlyPercentage,
            parseHtmlLinks: parseHtmlLinks,
            parseVersion: parseVersion,
            escapeHtml: escapeHtml,
            naturalSortCollator: naturalSortCollator,
            safeTrim: safeTrim,
            toFixedPointString: toFixedPointString,
            containsAllTerms: containsAllTerms,
            sleep: sleep,
            DateFormatOptions: DateFormatOptions,
            downloadFile: downloadFile,
            formatDate: formatDate,
            // variables
            FILTER_INPUT_DELAY: 400,
            MAX_ETA: 8640000
        };
    };

    // getHost emulate the GUI version `QString getHost(const QString &url)`
    const getHost = (url) => {
        // We want the hostname.
        // If failed to parse the domain, original input should be returned

        if (!/^(?:https?|udp):/i.test(url))
            return url;

        try {
            // hack: URL can not get hostname from udp protocol
            const parsedUrl = new URL(url.replace(/^udp:/i, "https:"));
            // host: "example.com:8443"
            // hostname: "example.com"
            const host = parsedUrl.hostname;
            if (!host)
                return url;

            return host;
        }
        catch (error) {
            return url;
        }
    };

    const createDebounceHandler = (delay, func) => {
        let timer = -1;
        return (...params) => {
            clearTimeout(timer);
            timer = setTimeout(() => {
                func(...params);

                timer = -1;
            }, delay);
        };
    };

    const createTorrentStateInfo = ({
        sortOrder = -1,
        stateIconClass = "stateUnknown",
        progressColor = undefined,
        statusText = "QBT_TR(Unknown)QBT_TR[CONTEXT=HttpServer]",
        matchesDownloadFilter = false,
        matchesSeedingFilter = false,
        matchesCompletedFilter = false,
        isStopped = false,
        isQueued = false,
        isChecking = false,
        isErrored = false,
        isMetadataDownloading = false,
        isStalled = false,
        isStalledUploading = false,
        isStalledDownloading = false,
        isMoving = false,
        hidesAvailability = false,
    } = {}) => {
        return Object.freeze({
            sortOrder,
            stateIconClass,
            progressColor,
            statusText,
            matchesDownloadFilter,
            matchesSeedingFilter,
            matchesCompletedFilter,
            isStopped,
            isQueued,
            isChecking,
            isErrored,
            isMetadataDownloading,
            isStalled,
            isStalledUploading,
            isStalledDownloading,
            isMoving,
            hidesAvailability
        });
    };

    // Frontend-only normalization for the raw state strings exposed by the WebUI API.
    // Keep this in sync with torrentStateToString() in serialize_torrent.cpp.
    const defaultTorrentStateInfo = createTorrentStateInfo();
    const torrentStateInfo = Object.freeze({
        unknown: createTorrentStateInfo({
            stateIconClass: "stateError",
            progressColor: "var(--color-progress-error)",
            statusText: "QBT_TR(Unknown)QBT_TR[CONTEXT=HttpServer]",
            isErrored: true
        }),
        forcedDL: createTorrentStateInfo({
            sortOrder: 0,
            stateIconClass: "stateDownloading",
            progressColor: "var(--color-progress-downloading)",
            statusText: "QBT_TR([F] Downloading)QBT_TR[CONTEXT=TransferListDelegate]",
            matchesDownloadFilter: true
        }),
        downloading: createTorrentStateInfo({
            sortOrder: 1,
            stateIconClass: "stateDownloading",
            progressColor: "var(--color-progress-downloading)",
            statusText: "QBT_TR(Downloading)QBT_TR[CONTEXT=TransferListDelegate]",
            matchesDownloadFilter: true
        }),
        forcedMetaDL: createTorrentStateInfo({
            sortOrder: 2,
            stateIconClass: "stateDownloading",
            progressColor: "var(--color-progress-downloading)",
            statusText: "QBT_TR([F] Downloading metadata)QBT_TR[CONTEXT=TransferListDelegate]",
            matchesDownloadFilter: true,
            isMetadataDownloading: true
        }),
        metaDL: createTorrentStateInfo({
            sortOrder: 3,
            stateIconClass: "stateDownloading",
            progressColor: "var(--color-progress-downloading)",
            statusText: "QBT_TR(Downloading metadata)QBT_TR[CONTEXT=TransferListDelegate]",
            matchesDownloadFilter: true,
            isMetadataDownloading: true
        }),
        stalledDL: createTorrentStateInfo({
            sortOrder: 4,
            stateIconClass: "stateStalledDL",
            progressColor: "var(--color-progress-stalled)",
            statusText: "QBT_TR(Stalled)QBT_TR[CONTEXT=TransferListDelegate]",
            matchesDownloadFilter: true,
            isStalled: true,
            isStalledDownloading: true
        }),
        forcedUP: createTorrentStateInfo({
            sortOrder: 5,
            stateIconClass: "stateUploading",
            progressColor: "var(--color-progress-seeding)",
            statusText: "QBT_TR([F] Seeding)QBT_TR[CONTEXT=TransferListDelegate]",
            matchesSeedingFilter: true,
            matchesCompletedFilter: true
        }),
        uploading: createTorrentStateInfo({
            sortOrder: 6,
            stateIconClass: "stateUploading",
            progressColor: "var(--color-progress-seeding)",
            statusText: "QBT_TR(Seeding)QBT_TR[CONTEXT=TransferListDelegate]",
            matchesSeedingFilter: true,
            matchesCompletedFilter: true
        }),
        stalledUP: createTorrentStateInfo({
            sortOrder: 7,
            stateIconClass: "stateStalledUP",
            progressColor: "var(--color-progress-seeding)",
            statusText: "QBT_TR(Seeding)QBT_TR[CONTEXT=TransferListDelegate]",
            matchesSeedingFilter: true,
            matchesCompletedFilter: true,
            isStalled: true,
            isStalledUploading: true
        }),
        checkingResumeData: createTorrentStateInfo({
            sortOrder: 8,
            stateIconClass: "stateChecking",
            progressColor: "var(--color-progress-checking)",
            statusText: "QBT_TR(Checking resume data)QBT_TR[CONTEXT=TransferListDelegate]",
            isChecking: true
        }),
        queuedDL: createTorrentStateInfo({
            sortOrder: 9,
            stateIconClass: "stateQueued",
            progressColor: "var(--color-progress-queued)",
            statusText: "QBT_TR(Queued)QBT_TR[CONTEXT=TransferListDelegate]",
            matchesDownloadFilter: true,
            isQueued: true
        }),
        queuedUP: createTorrentStateInfo({
            sortOrder: 10,
            stateIconClass: "stateQueued",
            progressColor: "var(--color-progress-queued)",
            statusText: "QBT_TR(Queued)QBT_TR[CONTEXT=TransferListDelegate]",
            matchesSeedingFilter: true,
            matchesCompletedFilter: true,
            isQueued: true
        }),
        checkingUP: createTorrentStateInfo({
            sortOrder: 11,
            stateIconClass: "stateChecking",
            progressColor: "var(--color-progress-checking)",
            statusText: "QBT_TR(Checking)QBT_TR[CONTEXT=TransferListDelegate]",
            matchesSeedingFilter: true,
            matchesCompletedFilter: true,
            isChecking: true
        }),
        checkingDL: createTorrentStateInfo({
            sortOrder: 12,
            stateIconClass: "stateChecking",
            progressColor: "var(--color-progress-checking)",
            statusText: "QBT_TR(Checking)QBT_TR[CONTEXT=TransferListDelegate]",
            matchesDownloadFilter: true,
            isChecking: true
        }),
        stoppedDL: createTorrentStateInfo({
            sortOrder: 13,
            stateIconClass: "stateStoppedDL",
            progressColor: "var(--color-progress-stopped)",
            statusText: "QBT_TR(Stopped)QBT_TR[CONTEXT=TransferListDelegate]",
            matchesDownloadFilter: true,
            isStopped: true
        }),
        stoppedUP: createTorrentStateInfo({
            sortOrder: 14,
            stateIconClass: "stateStoppedUP",
            progressColor: "var(--color-progress-stopped)",
            statusText: "QBT_TR(Completed)QBT_TR[CONTEXT=TransferListDelegate]",
            matchesCompletedFilter: true,
            isStopped: true
        }),
        moving: createTorrentStateInfo({
            sortOrder: 15,
            stateIconClass: "stateMoving",
            progressColor: "var(--color-progress-checking)",
            statusText: "QBT_TR(Moving)QBT_TR[CONTEXT=TransferListDelegate]",
            isMoving: true
        }),
        missingFiles: createTorrentStateInfo({
            sortOrder: 16,
            stateIconClass: "stateError",
            progressColor: "var(--color-progress-error)",
            statusText: "QBT_TR(Missing Files)QBT_TR[CONTEXT=TransferListDelegate]",
            isErrored: true,
            hidesAvailability: true
        }),
        error: createTorrentStateInfo({
            sortOrder: 17,
            stateIconClass: "stateError",
            progressColor: "var(--color-progress-error)",
            statusText: "QBT_TR(Errored)QBT_TR[CONTEXT=TransferListDelegate]",
            isErrored: true,
            hidesAvailability: true
        }),
        queuedForChecking: createTorrentStateInfo({
            sortOrder: 10,
            stateIconClass: "stateChecking",
            progressColor: "var(--color-progress-checking)",
            statusText: "QBT_TR(Queued for checking)QBT_TR[CONTEXT=TransferListDelegate]",
            isQueued: true
        })
    });

    const getTorrentStateInfo = (state) => {
        return torrentStateInfo[state] ?? defaultTorrentStateInfo;
    };

    const shouldShowAvailability = ({ hasMetadata, progress, state }) => {
        const stateInfo = getTorrentStateInfo(state);
        return hasMetadata
            && (progress < 1)
            && !stateInfo.isStopped
            && !stateInfo.isQueued
            && !stateInfo.isChecking
            && !stateInfo.hidesAvailability;
    };

    const filterInPlace = (array, predicate) => {
        let j = 0;
        for (let i = 0; i < array.length; ++i) {
            if (predicate(array[i])) {
                if (i > j)
                    array[j] = array[i];
                ++j;
            }
        }
        array.splice(j, (array.length - j));
    };

    /*
     * JS counterpart of the function in src/misc.cpp
     */
    const friendlyUnit = (value, isSpeed) => {
        if ((value === undefined) || (value === null) || Number.isNaN(value) || (value < 0))
            return "QBT_TR(Unknown)QBT_TR[CONTEXT=misc]";

        const units = [
            "QBT_TR(B)QBT_TR[CONTEXT=misc]",
            "QBT_TR(KiB)QBT_TR[CONTEXT=misc]",
            "QBT_TR(MiB)QBT_TR[CONTEXT=misc]",
            "QBT_TR(GiB)QBT_TR[CONTEXT=misc]",
            "QBT_TR(TiB)QBT_TR[CONTEXT=misc]",
            "QBT_TR(PiB)QBT_TR[CONTEXT=misc]",
            "QBT_TR(EiB)QBT_TR[CONTEXT=misc]"
        ];

        const friendlyUnitPrecision = (sizeUnit) => {
            if (sizeUnit <= 2) // KiB, MiB
                return 1;
            else if (sizeUnit === 3) // GiB
                return 2;
            else // TiB, PiB, EiB
                return 3;
        };

        let i = 0;
        while ((value >= 1024) && (i < 6)) {
            value /= 1024;
            ++i;
        }

        let ret;
        if (i === 0) {
            ret = `${value} ${units[i]}`;
        }
        else {
            const precision = friendlyUnitPrecision(i);
            // Don't round up
            ret = `${toFixedPointString(value, precision)} ${units[i]}`;
        }

        if (isSpeed)
            ret += "QBT_TR(/s)QBT_TR[CONTEXT=misc]";
        return ret;
    };

    /*
     * JS counterpart of the function in src/misc.cpp
     */
    const friendlyDuration = (seconds, maxCap = -1) => {
        if ((seconds < 0) || ((seconds >= maxCap) && (maxCap >= 0)))
            return "∞";
        if (seconds === 0)
            return "0";
        if (seconds < 60)
            return "QBT_TR(< 1m)QBT_TR[CONTEXT=misc]";
        let minutes = seconds / 60;
        if (minutes < 60)
            return "QBT_TR(%1m)QBT_TR[CONTEXT=misc]".replace("%1", Math.floor(minutes));
        let hours = minutes / 60;
        minutes %= 60;
        if (hours < 24)
            return "QBT_TR(%1h %2m)QBT_TR[CONTEXT=misc]".replace("%1", Math.floor(hours)).replace("%2", Math.floor(minutes));
        let days = hours / 24;
        hours %= 24;
        if (days < 365)
            return "QBT_TR(%1d %2h)QBT_TR[CONTEXT=misc]".replace("%1", Math.floor(days)).replace("%2", Math.floor(hours));
        const years = days / 365;
        days %= 365;
        return "QBT_TR(%1y %2d)QBT_TR[CONTEXT=misc]".replace("%1", Math.floor(years)).replace("%2", Math.floor(days));
    };

    const friendlyPercentage = (value) => {
        let percentage = value * 100;
        if (Number.isNaN(percentage) || (percentage < 0))
            percentage = 0;
        if (percentage > 100)
            percentage = 100;
        return `${toFixedPointString(percentage, 1)}%`;
    };

    /*
     * JS counterpart of the function in src/misc.cpp
     */
    const parseHtmlLinks = (text) => {
        const exp = /(\b(https?|ftp|file):\/\/[-\w+&@#/%?=~|!:,.;]*[-\w+&@#/%=~|])/gi;
        return text.replace(exp, "<a target='_blank' rel='noopener noreferrer' href='$1'>$1</a>");
    };

    const parseVersion = (versionString) => {
        const failure = {
            valid: false
        };

        if (typeof versionString !== "string")
            return failure;

        const tryToNumber = (str) => {
            const num = Number(str);
            return (Number.isNaN(num) ? str : num);
        };

        const ver = versionString.split(".", 4).map(val => tryToNumber(val));
        return {
            valid: true,
            major: ver[0],
            minor: ver[1],
            fix: ver[2],
            patch: ver[3]
        };
    };

    const escapeHtml = (() => {
        const div = document.createElement("div");
        return (str) => {
            div.textContent = str;
            return div.innerHTML;
        };
    })();

    // https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Intl/Collator/Collator#parameters
    const naturalSortCollator = new Intl.Collator(undefined, { numeric: true, usage: "sort" });

    const safeTrim = (value) => {
        try {
            return value.trim();
        }
        catch (e) {
            if (e instanceof TypeError)
                return "";
            throw e;
        }
    };

    const toFixedPointString = (number, digits) => {
        if (Number.isNaN(number))
            return number.toString();

        const sign = (number < 0) ? "-" : "";
        // Do not round up `number`
        // Small floating point numbers are imprecise, thus process as a String
        const tmp = Math.trunc(`${Math.abs(number)}e${digits}`).toString();
        if (digits <= 0) {
            return (tmp === "0") ? tmp : `${sign}${tmp}`;
        }
        else if (digits < tmp.length) {
            const idx = tmp.length - digits;
            return `${sign}${tmp.slice(0, idx)}.${tmp.slice(idx)}`;
        }
        else {
            const zeros = "0".repeat(digits - tmp.length);
            return `${sign}0.${zeros}${tmp}`;
        }
    };

    /**
     * @param {String} text the text to search
     * @param {Array<String>} terms terms to search for within the text
     * @returns {Boolean} true if all terms match the text, false otherwise
     */
    const containsAllTerms = (text, terms) => {
        const textToSearch = text.toLowerCase();
        return terms.every((term) => {
            const isTermRequired = term.startsWith("+");
            const isTermExcluded = term.startsWith("-");
            if (isTermRequired || isTermExcluded) {
                // ignore lonely +/-
                if (term.length === 1)
                    return true;

                term = term.substring(1);
            }

            const textContainsTerm = textToSearch.includes(term);
            return isTermExcluded ? !textContainsTerm : textContainsTerm;
        });
    };

    const sleep = (ms) => {
        return new Promise((resolve) => {
            setTimeout(resolve, ms);
        });
    };

    const downloadFile = async (url, defaultFileName, errorMessage = "QBT_TR(Unable to download file)QBT_TR[CONTEXT=HttpServer]") => {
        try {
            const response = await fetch(url, { method: "GET" });
            if (!response.ok) {
                alert(errorMessage);
                return;
            }

            const blob = await response.blob();
            const fileNamePrefix = "attachment; filename=";
            const fileNameHeader = response.headers.get("content-disposition");
            let fileName = defaultFileName;
            if (fileNameHeader.startsWith(fileNamePrefix)) {
                fileName = fileNameHeader.substring(fileNamePrefix.length);
                if (fileName.startsWith("\"") && fileName.endsWith("\""))
                    fileName = fileName.slice(1, -1);
            }

            const link = document.createElement("a");
            link.href = window.URL.createObjectURL(blob);
            link.download = fileName;
            link.click();
            link.remove();
        }
        catch (error) {
            alert(errorMessage);
        }
    };

    /**
     * @param {Date} date
     * @param {string} format
     * @returns {string}
     */
    const formatDate = (date, format = window.parent.qBittorrent.ClientData.get("date_format")) => {
        if ((format === "default") || !Object.hasOwn(DateFormatOptions, format))
            return date.toLocaleString();

        const { locale, options } = DateFormatOptions[format];
        const formatter = new Intl.DateTimeFormat(locale, options);
        const formatted = formatter.format(date).replace(" at ", ", ");
        return format.includes(".") ? formatted.replaceAll("/", ".") : formatted;
    };

    /**
     * @type Record<string, {locale: string, options: {}}>
     */
    const DateFormatOptions = Object.freeze({
        "MM/dd/yyyy, h:mm:ss AM/PM": {
            locale: "en-US",
            options: {
                year: "numeric",
                month: "2-digit",
                day: "2-digit",
                hour: "numeric",
                minute: "2-digit",
                second: "2-digit",
                hour12: true
            }
        },
        "MM/dd/yyyy, HH:mm:ss": {
            locale: "en-US",
            options: {
                year: "numeric",
                month: "2-digit",
                day: "2-digit",
                hour: "2-digit",
                minute: "2-digit",
                second: "2-digit",
                hour12: false
            }
        },
        "dd/MM/yyyy, HH:mm:ss": {
            locale: "en-GB",
            options: {
                year: "numeric",
                month: "2-digit",
                day: "2-digit",
                hour: "2-digit",
                minute: "2-digit",
                second: "2-digit",
                hour12: false
            }
        },
        "yyyy-MM-dd HH:mm:ss": {
            locale: "sv-SE",
            options: {
                year: "numeric",
                month: "2-digit",
                day: "2-digit",
                hour: "2-digit",
                minute: "2-digit",
                second: "2-digit",
                hour12: false
            }
        },
        "yyyy/MM/dd HH:mm:ss": {
            locale: "ja-JP",
            options: {
                year: "numeric",
                month: "2-digit",
                day: "2-digit",
                hour: "2-digit",
                minute: "2-digit",
                second: "2-digit",
                hour12: false
            }
        },
        "dd.MM.yyyy, HH:mm:ss": {
            locale: "en-GB",
            options: {
                year: "numeric",
                month: "2-digit",
                day: "2-digit",
                hour: "2-digit",
                minute: "2-digit",
                second: "2-digit",
                hour12: false
            }
        },
        "MMM dd, yyyy, h:mm:ss AM/PM": {
            locale: "en-US",
            options: {
                year: "numeric",
                month: "short",
                day: "2-digit",
                hour: "numeric",
                minute: "2-digit",
                second: "2-digit",
                hour12: true
            }
        },
        "dd MMM yyyy, HH:mm:ss": {
            locale: "en-GB",
            options: {
                year: "numeric",
                month: "short",
                day: "2-digit",
                hour: "2-digit",
                minute: "2-digit",
                second: "2-digit",
                hour12: false
            }
        },
    });

    return exports();
})();
Object.freeze(window.qBittorrent.Misc);
