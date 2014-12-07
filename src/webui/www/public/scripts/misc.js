/*
 * JS counterpart of the function in src/misc.cpp
 */
function friendlyUnit(value, isSpeed) {
    units = [
        "_(B)",
        "_(KiB)",
        "_(MiB)",
        "_(GiB)",
        "_(TiB)",
    ];

    if (value < 0)
        return "_(Unknown)";
    var i = 0;
    while (value >= 1024. && i++ < 6)
        value /= 1024.;
    var ret;
    ret = value.toFixed(1) + " " + units[i];
    if (isSpeed)
        ret += "_(/s)";
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
        return "< " + "_(%1m)".replace("%1", "1"); //translation of "< 1m" not working
    var minutes = seconds / 60;
    if (minutes < 60)
        return "_(%1m)".replace("%1", parseInt(minutes));
    var hours = minutes / 60;
    minutes = minutes - hours * 60;
    if (hours < 24)
        return "_(%1h %2m)".replace("%1", parseInt(hours)).replace("%2", parseInt(minutes))
    var days = hours / 24;
    hours = hours - days * 24;
    if (days < 100)
        return "_(%1d %2h)".replace("%1", parseInt(days)).replace("%2", parseInt(hours))
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