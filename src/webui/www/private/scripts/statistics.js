/*
 * MIT License
 * Copyright (C) 2025 Thomas Piccirello <thomas@piccirello.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

"use strict";

window.qBittorrent ??= {};
window.qBittorrent.Statistics ??= (() => {
    const exports = () => {
        return {
            save: save,
            render: render,
        };
    };

    const statistics = {
        alltimeDL: 0,
        alltimeUL: 0,
        totalWastedSession: 0,
        globalRatio: 0,
        totalPeerConnections: 0,
        readCacheHits: 0,
        totalBuffersSize: 0,
        writeCacheOverload: 0,
        readCacheOverload: 0,
        queuedIOJobs: 0,
        averageTimeInQueue: 0,
        totalQueuedSize: 0,
    };

    const save = (serverState) => {
        statistics.alltimeDL = serverState.alltime_dl;
        statistics.alltimeUL = serverState.alltime_ul;
        statistics.totalWastedSession = serverState.total_wasted_session;
        statistics.globalRatio = serverState.global_ratio;
        statistics.totalPeerConnections = serverState.total_peer_connections;
        statistics.readCacheHits = serverState.read_cache_hits;
        statistics.totalBuffersSize = serverState.total_buffers_size;
        statistics.writeCacheOverload = serverState.write_cache_overload;
        statistics.readCacheOverload = serverState.read_cache_overload;
        statistics.queuedIOJobs = serverState.queued_io_jobs;
        statistics.averageTimeInQueue = serverState.average_time_queue;
        statistics.totalQueuedSize = serverState.total_queued_size;
    };

    const render = () => {
        if (!document.getElementById("statisticsContent"))
            return;

        document.getElementById("AlltimeDL").textContent = window.qBittorrent.Misc.friendlyUnit(statistics.alltimeDL, false);
        document.getElementById("AlltimeUL").textContent = window.qBittorrent.Misc.friendlyUnit(statistics.alltimeUL, false);
        document.getElementById("TotalWastedSession").textContent = window.qBittorrent.Misc.friendlyUnit(statistics.totalWastedSession, false);
        document.getElementById("GlobalRatio").textContent = statistics.globalRatio;
        document.getElementById("TotalPeerConnections").textContent = statistics.totalPeerConnections;
        document.getElementById("ReadCacheHits").textContent = `${statistics.readCacheHits}%`;
        document.getElementById("TotalBuffersSize").textContent = window.qBittorrent.Misc.friendlyUnit(statistics.totalBuffersSize, false);
        document.getElementById("WriteCacheOverload").textContent = `${statistics.writeCacheOverload}%`;
        document.getElementById("ReadCacheOverload").textContent = `${statistics.readCacheOverload}%`;
        document.getElementById("QueuedIOJobs").textContent = statistics.queuedIOJobs;
        document.getElementById("AverageTimeInQueue").textContent = `${statistics.averageTimeInQueue} ms`;
        document.getElementById("TotalQueuedSize").textContent = window.qBittorrent.Misc.friendlyUnit(statistics.totalQueuedSize, false);
    };

    return exports();
})();
Object.freeze(window.qBittorrent.Statistics);
