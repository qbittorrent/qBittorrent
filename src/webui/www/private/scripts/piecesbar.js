/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022  Jesse Smick <jesse.smick@gmail.com>
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
window.qBittorrent.PiecesBar ??= (() => {
    const exports = () => {
        return {
            PiecesBar: PiecesBar
        };
    };

    const STATUS_DOWNLOADING = 1;
    const STATUS_DOWNLOADED = 2;

    // absolute max width of 4096
    // this is to support all browsers for size of canvas elements
    // see https://github.com/jhildenbiddle/canvas-size#test-results
    const MAX_CANVAS_WIDTH = 4096;

    let piecesBarUniqueId = 0;
    const PiecesBar = new Class({
        initialize: (pieces, parameters) => {
            const vals = {
                id: `piecesbar_${piecesBarUniqueId++}`,
                width: 0,
                height: 0,
                downloadingColor: "hsl(110deg 94% 27%)", // @TODO palette vars not supported for this value, apply average
                haveColor: "hsl(210deg 55% 55%)", // @TODO palette vars not supported for this value, apply average
                borderSize: 1,
                borderColor: "var(--color-border-default)"
            };

            if (parameters && (typeOf(parameters) === "object"))
                Object.append(vals, parameters);
            vals.height = Math.max(vals.height, 12);

            const obj = document.createElement("div");
            obj.id = vals.id;
            obj.className = "piecesbarWrapper";
            obj.style.border = `${vals.borderSize}px solid ${vals.borderColor}`;
            obj.style.height = `${vals.height}px`;
            obj.vals = vals;
            obj.vals.pieces = [pieces, []].pick();

            const canvas = document.createElement("canvas");
            canvas.id = `${vals.id}_canvas`;
            canvas.className = "piecesbarCanvas";
            canvas.width = `${vals.width - (2 * vals.borderSize)}`;
            canvas.height = "1"; // will stretch vertically to take up the height of the parent
            obj.vals.canvas = canvas;
            obj.appendChild(obj.vals.canvas);

            obj.setPieces = setPieces;
            obj.refresh = refresh;
            obj.clear = setPieces.bind(obj, []);
            obj._drawStatus = drawStatus;

            if (vals.width > 0)
                obj.setPieces(vals.pieces);
            else
                setTimeout(() => { checkForParent(obj.id); });

            return obj;
        }
    });

    function setPieces(pieces) {
        if (!Array.isArray(pieces))
            pieces = [];

        this.vals.pieces = pieces;
        this.refresh(true);
    }

    function refresh(force) {
        if (!this.parentNode)
            return;

        const pieces = this.vals.pieces;

        // if the number of pieces is small, use that for the width,
        // and have it stretch horizontally.
        // this also limits the ratio below to >= 1
        const width = Math.min(this.offsetWidth, pieces.length, MAX_CANVAS_WIDTH);
        if ((this.vals.width === width) && !force)
            return;

        this.vals.width = width;

        // change canvas size to fit exactly in the space
        this.vals.canvas.width = width - (2 * this.vals.borderSize);

        const canvas = this.vals.canvas;
        const ctx = canvas.getContext("2d");
        ctx.clearRect(0, 0, canvas.width, canvas.height);

        const imageWidth = canvas.width;

        if (imageWidth.length === 0)
            return;

        let minStatus = Infinity;
        let maxStatus = 0;

        for (const status of pieces) {
            if (status > maxStatus)
                maxStatus = status;
            if (status < minStatus)
                minStatus = status;
        }

        // if no progress then don't do anything
        if (maxStatus === 0)
            return;

        // if all pieces are downloaded, fill entire image at once
        if (minStatus === STATUS_DOWNLOADED) {
            ctx.fillStyle = this.vals.haveColor;
            ctx.fillRect(0, 0, canvas.width, canvas.height);
            return;
        }

        /* Linear transformation from pieces to pixels.
         *
         * The canvas size can vary in width so this figures out what to draw at each pixel.
         * Inspired by the GUI code here https://github.com/qbittorrent/qBittorrent/blob/25b3f2d1a6b14f0fe098fb79a3d034607e52deae/src/gui/properties/downloadedpiecesbar.cpp#L54
         *
         * example ratio > 1 (at least 2 pieces per pixel)
         *        +-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
         * pieces |  2  |  1  |  2  |  0  |  2  |  0  |  1  |  0  |  1  |  2  |
         *        +---------+---------+---------+---------+---------+---------+
         * pixels |         |         |         |         |         |         |
         *        +---------+---------+---------+---------+---------+---------+
         *
         * example ratio < 1 (at most 2 pieces per pixel)
         * This case shouldn't happen since the max pixels are limited to the number of pieces
         *        +---------+---------+---------+---------+----------+--------+
         * pieces |    2    |    1    |    1    |    0    |    2    |    2    |
         *        +-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
         * pixels |     |     |     |     |     |     |     |     |     |     |
         *        +-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
         */

        const ratio = pieces.length / imageWidth;

        let lastValue = null;
        let rectangleStart = 0;

        // for each pixel compute its status based on the pieces
        for (let x = 0; x < imageWidth; ++x) {
            // find positions in the pieces array
            const piecesFrom = x * ratio;
            const piecesTo = (x + 1) * ratio;
            const piecesToInt = Math.ceil(piecesTo);

            const statusValues = {
                [STATUS_DOWNLOADING]: 0,
                [STATUS_DOWNLOADED]: 0
            };

            // aggregate the status of each piece that contributes to this pixel
            for (let p = piecesFrom; p < piecesToInt; ++p) {
                const piece = Math.floor(p);
                const pieceStart = Math.max(piecesFrom, piece);
                const pieceEnd = Math.min(piece + 1, piecesTo);

                const amount = pieceEnd - pieceStart;
                const status = pieces[piece];

                if (status in statusValues)
                    statusValues[status] += amount;
            }

            // normalize to interval [0, 1]
            statusValues[STATUS_DOWNLOADING] /= ratio;
            statusValues[STATUS_DOWNLOADED] /= ratio;

            // floats accumulate small errors, so smooth it out by rounding to hundredths place
            // this effectively limits each status to a value 1 in 100
            statusValues[STATUS_DOWNLOADING] = Math.round(statusValues[STATUS_DOWNLOADING] * 100) / 100;
            statusValues[STATUS_DOWNLOADED] = Math.round(statusValues[STATUS_DOWNLOADED] * 100) / 100;

            // float precision sometimes _still_ gives > 1
            statusValues[STATUS_DOWNLOADING] = Math.min(statusValues[STATUS_DOWNLOADING], 1);
            statusValues[STATUS_DOWNLOADED] = Math.min(statusValues[STATUS_DOWNLOADED], 1);

            if (!lastValue)
                lastValue = statusValues;

            // group contiguous colors together and draw as a single rectangle
            if ((lastValue[STATUS_DOWNLOADING] === statusValues[STATUS_DOWNLOADING])
                && (lastValue[STATUS_DOWNLOADED] === statusValues[STATUS_DOWNLOADED]))
                continue;

            const rectangleWidth = x - rectangleStart;
            this._drawStatus(ctx, rectangleStart, rectangleWidth, lastValue);

            lastValue = statusValues;
            rectangleStart = x;
        }

        // fill a rect at the end of the canvas
        if (rectangleStart < imageWidth) {
            const rectangleWidth = imageWidth - rectangleStart;
            this._drawStatus(ctx, rectangleStart, rectangleWidth, lastValue);
        }
    }

    function drawStatus(ctx, start, width, statusValues) {
        // mix the colors by using transparency and a composite mode
        ctx.globalCompositeOperation = "lighten";

        if (statusValues[STATUS_DOWNLOADING]) {
            ctx.globalAlpha = statusValues[STATUS_DOWNLOADING];
            ctx.fillStyle = this.vals.downloadingColor;
            ctx.fillRect(start, 0, width, ctx.canvas.height);
        }

        if (statusValues[STATUS_DOWNLOADED]) {
            ctx.globalAlpha = statusValues[STATUS_DOWNLOADED];
            ctx.fillStyle = this.vals.haveColor;
            ctx.fillRect(start, 0, width, ctx.canvas.height);
        }
    }

    const checkForParent = (id) => {
        const obj = $(id);
        if (!obj)
            return;
        if (!obj.parentNode)
            return setTimeout(() => { checkForParent(id); }, 100);

        obj.refresh();
    };

    return exports();
})();
Object.freeze(window.qBittorrent.PiecesBar);
