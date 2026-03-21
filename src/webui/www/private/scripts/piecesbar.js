/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022  Jesse Smick <jesse.smick@gmail.com>
 * Copyright (C) 2026  Thomas Piccirello <thomas@piccirello.com>
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
            PiecesBar: PiecesBar,
            AvailabilityBar: AvailabilityBar
        };
    };

    /**
     * Base class for piece-based progress bars.
     * Uses template method pattern - subclasses implement abstract methods.
     */
    class Base extends HTMLElement {
        // absolute max width of 4096
        // this is to support all browsers for size of canvas elements
        // see https://github.com/jhildenbiddle/canvas-size#test-results
        static #MAX_CANVAS_WIDTH = 4096;
        static #piecesBarUniqueId = 0;

        _canvasEl;
        _ctx;
        _styles;
        #id = ++Base.#piecesBarUniqueId;
        #resizeObserver;

        constructor(styles = {}) {
            super();
            this._styles = {
                height: 12,
                borderSize: 1,
                borderColor: "var(--color-border-default)",
                ...styles
            };

            this._canvasEl = document.createElement("canvas");
            this._canvasEl.style.height = "100%";
            this._canvasEl.style.imageRendering = "pixelated";
            this._canvasEl.style.width = "100%";
            this._ctx = this._canvasEl.getContext("2d");

            this.attachShadow({ mode: "open" });
            this.shadowRoot.host.id = `piecesbar_${this.#id}`;
            this.shadowRoot.host.style.display = "block";
            this.shadowRoot.host.style.height = `${this._styles.height}px`;
            this.shadowRoot.host.style.border = `${this._styles.borderSize}px solid ${this._styles.borderColor}`;
            this.shadowRoot.append(this._canvasEl);

            this.#resizeObserver = new ResizeObserver(window.qBittorrent.Misc.createDebounceHandler(100, () => {
                this._refresh();
            }));
        }

        connectedCallback() {
            this.#resizeObserver.observe(this);
            this._refresh();
        }

        // Template method - orchestrates the rendering
        _refresh() {
            if (!this.isConnected)
                return;

            const data = this._getData();
            const width = Math.min(this.offsetWidth, data.length, Base.#MAX_CANVAS_WIDTH);

            this._canvasEl.width = width - (2 * this._styles.borderSize);
            this._ctx.clearRect(0, 0, this._canvasEl.width, this._canvasEl.height);

            const imageWidth = this._canvasEl.width;
            if (imageWidth <= 0)
                return;

            // Subclass-specific early exit or fill
            if (!this._shouldRender(imageWidth))
                return;

            const ratio = data.length / imageWidth;

            let lastValue = null;
            let rectangleStart = 0;

            // for each pixel compute its value based on the pieces
            for (let x = 0; x < imageWidth; ++x) {
                const piecesFrom = x * ratio;
                const piecesTo = (x + 1) * ratio;

                const value = this._computePixelValue(piecesFrom, piecesTo, ratio);

                if (lastValue === null)
                    lastValue = value;

                // group contiguous colors together and draw as a single rectangle
                if (this._valuesEqual(lastValue, value))
                    continue;

                const rectangleWidth = x - rectangleStart;
                this._drawRectangle(rectangleStart, rectangleWidth, lastValue);

                lastValue = value;
                rectangleStart = x;
            }

            // fill a rect at the end of the canvas
            if (rectangleStart < imageWidth) {
                const rectangleWidth = imageWidth - rectangleStart;
                this._drawRectangle(rectangleStart, rectangleWidth, lastValue);
            }
        }

        // Abstract methods - must be implemented by subclasses
        _getData() {
            throw new Error("_getData must be implemented");
        }
        _shouldRender(imageWidth) {
            throw new Error("_shouldRender must be implemented");
        }
        _computePixelValue(piecesFrom, piecesTo, ratio) {
            throw new Error("_computePixelValue must be implemented");
        }
        _valuesEqual(left, right) {
            throw new Error("_valuesEqual must be implemented");
        }
        _drawRectangle(start, width, value) {
            throw new Error("_drawRectangle must be implemented");
        }
    }

    /**
     * Progress bar showing download status of pieces.
     * Colors indicate downloading (green) vs downloaded (blue) states.
     */
    class PiecesBar extends Base {
        static #STATUS_DOWNLOADING = 1;
        static #STATUS_DOWNLOADED = 2;

        #pieces = [];

        constructor(pieces = [], styles = {}) {
            super({
                downloadingColor: "hsl(110deg 94% 27%)",
                haveColor: "hsl(210deg 55% 55%)",
                ...styles
            });
            this.#pieces = Array.isArray(pieces) ? pieces : [];
        }

        clear() {
            this.#pieces = [];
            this._refresh();
        }

        setPieces(pieces) {
            this.#pieces = Array.isArray(pieces) ? pieces : [];
            this._refresh();
        }

        _getData() {
            return this.#pieces;
        }

        _shouldRender(imageWidth) {
            let minStatus = Infinity;
            let maxStatus = 0;

            for (const status of this.#pieces) {
                if (status > maxStatus)
                    maxStatus = status;
                if (status < minStatus)
                    minStatus = status;
            }

            // if no progress then don't do anything
            if (maxStatus === 0)
                return false;

            // if all pieces are downloaded, fill entire image at once
            if (minStatus === PiecesBar.#STATUS_DOWNLOADED) {
                this._ctx.fillStyle = this._styles.haveColor;
                this._ctx.fillRect(0, 0, this._canvasEl.width, this._canvasEl.height);
                return false;
            }

            return true;
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
        _computePixelValue(piecesFrom, piecesTo, ratio) {
            const piecesToInt = Math.ceil(piecesTo);
            const statusValues = {
                [PiecesBar.#STATUS_DOWNLOADING]: 0,
                [PiecesBar.#STATUS_DOWNLOADED]: 0
            };

            // aggregate the status of each piece that contributes to this pixel
            for (let p = piecesFrom; p < piecesToInt; ++p) {
                const piece = Math.floor(p);
                const pieceStart = Math.max(piecesFrom, piece);
                const pieceEnd = Math.min(piece + 1, piecesTo);

                const amount = pieceEnd - pieceStart;
                const status = this.#pieces[piece];

                if (status in statusValues)
                    statusValues[status] += amount;
            }

            // normalize to interval [0, 1]
            statusValues[PiecesBar.#STATUS_DOWNLOADING] /= ratio;
            statusValues[PiecesBar.#STATUS_DOWNLOADED] /= ratio;

            // floats accumulate small errors, so smooth it out by rounding to hundredths place
            statusValues[PiecesBar.#STATUS_DOWNLOADING] = Math.round(statusValues[PiecesBar.#STATUS_DOWNLOADING] * 100) / 100;
            statusValues[PiecesBar.#STATUS_DOWNLOADED] = Math.round(statusValues[PiecesBar.#STATUS_DOWNLOADED] * 100) / 100;

            // float precision sometimes _still_ gives > 1
            statusValues[PiecesBar.#STATUS_DOWNLOADING] = Math.min(statusValues[PiecesBar.#STATUS_DOWNLOADING], 1);
            statusValues[PiecesBar.#STATUS_DOWNLOADED] = Math.min(statusValues[PiecesBar.#STATUS_DOWNLOADED], 1);

            return statusValues;
        }

        _valuesEqual(left, right) {
            return (left[PiecesBar.#STATUS_DOWNLOADING] === right[PiecesBar.#STATUS_DOWNLOADING])
                && (left[PiecesBar.#STATUS_DOWNLOADED] === right[PiecesBar.#STATUS_DOWNLOADED]);
        }

        _drawRectangle(start, width, statusValues) {
            // mix the colors by using transparency and a composite mode
            this._ctx.globalCompositeOperation = "lighten";

            if (statusValues[PiecesBar.#STATUS_DOWNLOADING]) {
                this._ctx.globalAlpha = statusValues[PiecesBar.#STATUS_DOWNLOADING];
                this._ctx.fillStyle = this._styles.downloadingColor;
                this._ctx.fillRect(start, 0, width, this._canvasEl.height);
            }

            if (statusValues[PiecesBar.#STATUS_DOWNLOADED]) {
                this._ctx.globalAlpha = statusValues[PiecesBar.#STATUS_DOWNLOADED];
                this._ctx.fillStyle = this._styles.haveColor;
                this._ctx.fillRect(start, 0, width, this._canvasEl.height);
            }
        }
    }

    /**
     * Availability bar showing peer availability of pieces.
     * Color gradient from gray (no peers) to blue (many peers).
     */
    class AvailabilityBar extends Base {
        #availability = [];
        #maxAvailability = 0;

        constructor(styles = {}) {
            super(styles);
        }

        clear() {
            this.#availability = [];
            this.#maxAvailability = 0;
            this._refresh();
        }

        setAvailability(availability) {
            this.#availability = Array.isArray(availability) ? availability : [];
            this.#maxAvailability = this.#availability.reduce((acc, val) => Math.max(acc, val), 0);
            this._refresh();
        }

        _getData() {
            return this.#availability;
        }

        _shouldRender(imageWidth) {
            if (this.#maxAvailability === 0)
                return false;
            return true;
        }

        _computePixelValue(piecesFrom, piecesTo, ratio) {
            const piecesToInt = Math.ceil(piecesTo);

            let totalAvailability = 0;
            let totalWeight = 0;

            // aggregate the availability of each piece that contributes to this pixel
            for (let p = piecesFrom; p < piecesToInt; ++p) {
                const piece = Math.floor(p);
                const pieceStart = Math.max(piecesFrom, piece);
                const pieceEnd = Math.min(piece + 1, piecesTo);

                const weight = pieceEnd - pieceStart;
                totalAvailability += this.#availability[piece] * weight;
                totalWeight += weight;
            }

            // calculate weighted average availability for this pixel
            const avgAvailability = (totalWeight > 0) ? (totalAvailability / totalWeight) : 0;
            // normalize to 0-1 range based on max availability
            return Math.round((avgAvailability / this.#maxAvailability) * 100) / 100;
        }

        _valuesEqual(left, right) {
            return left === right;
        }

        _drawRectangle(start, width, normalizedValue) {
            // Interpolate between background color (gray) and available color (blue)
            // normalizedValue is 0-1, where 0 = no peers, 1 = max peers
            // Use HSL interpolation for smooth gradient
            // bgColor: hsl(0, 0%, 50%) - gray
            // targetColor: hsl(210, 55%, 55%) - blue
            const hue = 210 * normalizedValue; // 0 -> 210
            const saturation = 55 * normalizedValue; // 0% -> 55%
            const lightness = 50 + (5 * normalizedValue); // 50% -> 55%

            this._ctx.fillStyle = `hsl(${hue}, ${saturation}%, ${lightness}%)`;
            this._ctx.fillRect(start, 0, width, this._canvasEl.height);
        }
    }

    customElements.define("pieces-bar", PiecesBar);
    customElements.define("availability-bar", AvailabilityBar);

    return exports();
})();
Object.freeze(window.qBittorrent.PiecesBar);
