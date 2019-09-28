/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2008  Christophe Dumez <chris@qbittorrent.org>
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

window.qBittorrent.ProgressBar = (function() {
    const exports = function() {
        return {
            ProgressBar: ProgressBar
        };
    };

    let ProgressBars = 0;
    const ProgressBar = new Class({
        initialize: function(value, parameters) {
            const vals = {
                'id': 'progressbar_' + (ProgressBars++),
                'value': $pick(value, 0),
                'width': 0,
                'height': 0,
                'darkbg': '#006',
                'darkfg': '#fff',
                'lightbg': '#fff',
                'lightfg': '#000'
            };
            if (parameters && $type(parameters) == 'object') $extend(vals, parameters);
            if (vals.height < 12) vals.height = 12;
            const obj = new Element('div', {
                'id': vals.id,
                'class': 'progressbar_wrapper',
                'styles': {
                    'border': '1px solid #000',
                    'width': vals.width,
                    'height': vals.height,
                    'position': 'relative',
                    'margin': '0 auto'
                }
            });
            obj.vals = vals;
            obj.vals.value = $pick(value, 0); // Fix by Chris
            obj.vals.dark = new Element('div', {
                'id': vals.id + '_dark',
                'class': 'progressbar_dark',
                'styles': {
                    'width': vals.width,
                    'height': vals.height,
                    'background': vals.darkbg,
                    'color': vals.darkfg,
                    'position': 'absolute',
                    'text-align': 'center',
                    'left': 0,
                    'top': 0,
                    'line-height': vals.height
                }
            });
            obj.vals.light = new Element('div', {
                'id': vals.id + '_light',
                'class': 'progressbar_light',
                'styles': {
                    'width': vals.width,
                    'height': vals.height,
                    'background': vals.lightbg,
                    'color': vals.lightfg,
                    'position': 'absolute',
                    'text-align': 'center',
                    'left': 0,
                    'top': 0,
                    'line-height': vals.height
                }
            });
            obj.appendChild(obj.vals.dark);
            obj.appendChild(obj.vals.light);
            obj.getValue = ProgressBar_getValue;
            obj.setValue = ProgressBar_setValue;
            obj.setWidth = ProgressBar_setWidth;
            if (vals.width) obj.setValue(vals.value);
            else setTimeout('ProgressBar_checkForParent("' + obj.id + '")', 1);
            return obj;
        }
    });

    function ProgressBar_getValue() {
        return this.vals.value;
    }

    function ProgressBar_setValue(value) {
        value = parseFloat(value);
        if (isNaN(value)) value = 0;
        if (value > 100) value = 100;
        if (value < 0) value = 0;
        this.vals.value = value;
        this.vals.dark.empty();
        this.vals.light.empty();
        this.vals.dark.appendText(value.round(1).toFixed(1) + '%');
        this.vals.light.appendText(value.round(1).toFixed(1) + '%');
        const r = parseInt(this.vals.width * (value / 100));
        this.vals.dark.setStyle('clip', 'rect(0,' + r + 'px,' + this.vals.height + 'px,0)');
        this.vals.light.setStyle('clip', 'rect(0,' + this.vals.width + 'px,' + this.vals.height + 'px,' + r + 'px)');
    }

    function ProgressBar_setWidth(value) {
        if (this.vals.width !== value) {
            this.vals.width = value;
            this.setStyle('width', value);
            this.vals.dark.setStyle('width', value);
            this.vals.light.setStyle('width', value);
            this.setValue(this.vals.value);
        }
    }

    function ProgressBar_checkForParent(id) {
        const obj = $(id);
        if (!obj) return;
        if (!obj.parentNode) return setTimeout('ProgressBar_checkForParent("' + id + '")', 1);
        obj.setStyle('width', '100%');
        const w = obj.offsetWidth;
        obj.vals.dark.setStyle('width', w);
        obj.vals.light.setStyle('width', w);
        obj.vals.width = w;
        obj.setValue(obj.vals.value);
    }

    return exports();
})();
