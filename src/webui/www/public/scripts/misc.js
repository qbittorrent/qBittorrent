/*
 * JS counterpart of the function in src/misc.cpp
 */
function friendlyUnit(value, isSpeed) {
    units = [
        "QBT_TR(B)QBT_TR[CONTEXT=misc]",
        "QBT_TR(KiB)QBT_TR[CONTEXT=misc]",
        "QBT_TR(MiB)QBT_TR[CONTEXT=misc]",
        "QBT_TR(GiB)QBT_TR[CONTEXT=misc]",
        "QBT_TR(TiB)QBT_TR[CONTEXT=misc]",
        "QBT_TR(PiB)QBT_TR[CONTEXT=misc]",
        "QBT_TR(EiB)QBT_TR[CONTEXT=misc]"
    ];

    if (value < 0)
        return "QBT_TR(Unknown)QBT_TR[CONTEXT=misc]";

    var i = 0;
    while (value >= 1024. && i < 6) {
        value /= 1024.;
        ++i;
    }
    var ret;
    if (i == 0)
        ret = value + " " + units[i];
    else
        ret = (Math.floor(10 * value) / 10).toFixed(1) //Don't round up
            + " " + units[i];

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
    if (seconds == 0)
        return "0";
    if (seconds < 60)
        return "QBT_TR(< 1m)QBT_TR[CONTEXT=misc]";
    var minutes = seconds / 60;
    if (minutes < 60)
        return "QBT_TR(%1m)QBT_TR[CONTEXT=misc]".replace("%1", parseInt(minutes));
    var hours = minutes / 60;
    minutes = minutes % 60;
    if (hours < 24)
        return "QBT_TR(%1h %2m)QBT_TR[CONTEXT=misc]".replace("%1", parseInt(hours)).replace("%2", parseInt(minutes))
    var days = hours / 24;
    hours = hours % 24;
    if (days < 100)
        return "QBT_TR(%1d %2h)QBT_TR[CONTEXT=misc]".replace("%1", parseInt(days)).replace("%2", parseInt(hours))
    return "∞";
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
            return this.getUTCFullYear() +
                '-' + pad(this.getUTCMonth() + 1) +
                '-' + pad(this.getUTCDate()) +
                'T' + pad(this.getUTCHours()) +
                ':' + pad(this.getUTCMinutes()) +
                ':' + pad(this.getUTCSeconds()) +
                '.' + (this.getUTCMilliseconds() / 1000).toFixed(3).slice(2, 5) +
                'Z';
        };

    }());
}

/*
 * JS counterpart of the function in src/misc.cpp
 */
function parseHtmlLinks(text) {
    var exp = /(\b(https?|ftp|file):\/\/[-A-Z0-9+&@#\/%?=~_|!:,.;]*[-A-Z0-9+&@#\/%=~_|])/ig;
    return text.replace(exp,"<a target='_blank' href='$1'>$1</a>"); 
}

function escapeHtml(str) {
    var div = document.createElement('div');
    div.appendChild(document.createTextNode(str));
    return div.innerHTML;
};
