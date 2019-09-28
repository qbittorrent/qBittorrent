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

'use strict';

if (window.qBittorrent === undefined) {
    window.qBittorrent = {};
}

window.qBittorrent.Misc = (function() {
    const exports = function() {
        return {
            friendlyUnit: friendlyUnit,
            friendlyDuration: friendlyDuration,
            friendlyPercentage: friendlyPercentage,
            friendlyFloat: friendlyFloat,
            parseHtmlLinks: parseHtmlLinks,
            escapeHtml: escapeHtml,
            safeTrim: safeTrim,
            toFixedPointString: toFixedPointString,
            containsAllTerms: containsAllTerms
        };
    };

    /*
    * JS counterpart of the function in src/misc.cpp
    */
    const friendlyUnit = function(value, isSpeed) {
        const units = [
            "QBT_TR(B)QBT_TR[CONTEXT=misc]",
            "QBT_TR(KiB)QBT_TR[CONTEXT=misc]",
            "QBT_TR(MiB)QBT_TR[CONTEXT=misc]",
            "QBT_TR(GiB)QBT_TR[CONTEXT=misc]",
            "QBT_TR(TiB)QBT_TR[CONTEXT=misc]",
            "QBT_TR(PiB)QBT_TR[CONTEXT=misc]",
            "QBT_TR(EiB)QBT_TR[CONTEXT=misc]"
        ];

        if ((value === undefined) || (value === null) || (value < 0))
            return "QBT_TR(Unknown)QBT_TR[CONTEXT=misc]";

        let i = 0;
        while (value >= 1024.0 && i < 6) {
            value /= 1024.0;
            ++i;
        }

        function friendlyUnitPrecision(sizeUnit) {
            if (sizeUnit <= 2) return 1; // KiB, MiB
            else if (sizeUnit === 3) return 2; // GiB
            else return 3; // TiB, PiB, EiB
        }

        let ret;
        if (i === 0)
            ret = value + " " + units[i];
        else {
            const precision = friendlyUnitPrecision(i);
            const offset = Math.pow(10, precision);
            // Don't round up
            ret = (Math.floor(offset * value) / offset).toFixed(precision) + " " + units[i];
        }

        if (isSpeed)
            ret += "QBT_TR(/s)QBT_TR[CONTEXT=misc]";
        return ret;
    }

    /*
    * JS counterpart of the function in src/misc.cpp
    */
    const friendlyDuration = function(seconds) {
        const MAX_ETA = 8640000;
        if (seconds < 0 || seconds >= MAX_ETA)
            return "∞";
        if (seconds === 0)
            return "0";
        if (seconds < 60)
            return "QBT_TR(< 1m)QBT_TR[CONTEXT=misc]";
        let minutes = seconds / 60;
        if (minutes < 60)
            return "QBT_TR(%1m)QBT_TR[CONTEXT=misc]".replace("%1", parseInt(minutes));
        let hours = minutes / 60;
        minutes = minutes % 60;
        if (hours < 24)
            return "QBT_TR(%1h %2m)QBT_TR[CONTEXT=misc]".replace("%1", parseInt(hours)).replace("%2", parseInt(minutes));
        const days = hours / 24;
        hours = hours % 24;
        if (days < 100)
            return "QBT_TR(%1d %2h)QBT_TR[CONTEXT=misc]".replace("%1", parseInt(days)).replace("%2", parseInt(hours));
        return "∞";
    }

    const friendlyPercentage = function(value) {
        let percentage = (value * 100).round(1);
        if (isNaN(percentage) || (percentage < 0))
            percentage = 0;
        if (percentage > 100)
            percentage = 100;
        return percentage.toFixed(1) + "%";
    }

    const friendlyFloat = function(value, precision) {
        return parseFloat(value).toFixed(precision);
    }

    /*
    * From: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/toISOString
    */
    if (!Date.prototype.toISOString) {
        (function() {

            function pad(number) {
                if (number < 10) {
                    return '0' + number;
                }
                return number;
            }

            Date.prototype.toISOString = function() {
                return this.getUTCFullYear()
                    + '-' + pad(this.getUTCMonth() + 1)
                    + '-' + pad(this.getUTCDate())
                    + 'T' + pad(this.getUTCHours())
                    + ':' + pad(this.getUTCMinutes())
                    + ':' + pad(this.getUTCSeconds())
                    + '.' + (this.getUTCMilliseconds() / 1000).toFixed(3).slice(2, 5)
                    + 'Z';
            };

        }());
    }

    /*
    * JS counterpart of the function in src/misc.cpp
    */
    const parseHtmlLinks = function(text) {
        const exp = /(\b(https?|ftp|file):\/\/[-A-Z0-9+&@#\/%?=~_|!:,.;]*[-A-Z0-9+&@#\/%=~_|])/ig;
        return text.replace(exp, "<a target='_blank' href='$1'>$1</a>");
    }

    const escapeHtml = function(str) {
        const div = document.createElement('div');
        div.appendChild(document.createTextNode(str));
        return div.innerHTML;
    }

    const safeTrim = function(value) {
        try {
            return value.trim();
        }
        catch (e) {
            if (e instanceof TypeError)
                return "";
            throw e;
        }
    }

    const toFixedPointString = function(number, digits) {
        // Do not round up number
        const power = Math.pow(10, digits);
        return (Math.floor(power * number) / power).toFixed(digits);
    }

    /**
     *
     * @param {String} text the text to search
     * @param {Array<String>} terms terms to search for within the text
     * @returns {Boolean} true if all terms match the text, false otherwise
     */
    const containsAllTerms = function(text, terms) {
        const textToSearch = text.toLowerCase();
        return terms.every((function(term) {
            const isTermRequired = (term[0] === '+');
            const isTermExcluded = (term[0] === '-');
            if (isTermRequired || isTermExcluded) {
                // ignore lonely +/-
                if (term.length === 1)
                    return true;

                term = term.substring(1);
            }

            const textContainsTerm = (textToSearch.indexOf(term) !== -1);
            return isTermExcluded ? !textContainsTerm : textContainsTerm;
        }));
    }

    return exports();
})();
