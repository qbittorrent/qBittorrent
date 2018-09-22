/*
 * JS counterpart of the function in src/misc.cpp
 */
function friendlyUnit(value, isSpeed) {
    var units = [
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

    var i = 0;
    while (value >= 1024.0 && i < 6) {
        value /= 1024.0;
        ++i;
    }

    function friendlyUnitPrecision(sizeUnit) {
        if (sizeUnit <= 2) return 1; // KiB, MiB
        else if (sizeUnit === 3) return 2; // GiB
        else return 3; // TiB, PiB, EiB
    }

    var ret;
    if (i === 0)
        ret = value + " " + units[i];
    else {
        var precision = friendlyUnitPrecision(i);
        var offset = Math.pow(10, precision);
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
function friendlyDuration(seconds) {
    var MAX_ETA = 8640000;
    if (seconds < 0 || seconds >= MAX_ETA)
        return "∞";
    if (seconds === 0)
        return "0";
    if (seconds < 60)
        return "QBT_TR(< 1m)QBT_TR[CONTEXT=misc]";
    var minutes = seconds / 60;
    if (minutes < 60)
        return "QBT_TR(%1m)QBT_TR[CONTEXT=misc]".replace("%1", parseInt(minutes));
    var hours = minutes / 60;
    minutes = minutes % 60;
    if (hours < 24)
        return "QBT_TR(%1h %2m)QBT_TR[CONTEXT=misc]".replace("%1", parseInt(hours)).replace("%2", parseInt(minutes));
    var days = hours / 24;
    hours = hours % 24;
    if (days < 100)
        return "QBT_TR(%1d %2h)QBT_TR[CONTEXT=misc]".replace("%1", parseInt(days)).replace("%2", parseInt(hours));
    return "∞";
}

function friendlyPercentage(value) {
    var percentage = (value * 100).round(1);
    if (isNaN(percentage) || (percentage < 0))
        percentage = 0;
    if (percentage > 100)
        percentage = 100;
    return percentage.toFixed(1) + "%";
}

function friendlyFloat(value, precision) {
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
function parseHtmlLinks(text) {
    var exp = /(\b(https?|ftp|file):\/\/[-A-Z0-9+&@#\/%?=~_|!:,.;]*[-A-Z0-9+&@#\/%=~_|])/ig;
    return text.replace(exp, "<a target='_blank' href='$1'>$1</a>");
}

function escapeHtml(str) {
    var div = document.createElement('div');
    div.appendChild(document.createTextNode(str));
    return div.innerHTML;
}

function safeTrim(value) {
    try {
        return value.trim();
    }
    catch (e) {
        if (e instanceof TypeError)
            return "";
        throw e;
    }
}
