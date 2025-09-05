/*
 * MIT License
 * Copyright (C) 2025 tehcneko
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

self.addEventListener("install", (event) => {
    event.waitUntil(self.skipWaiting());
});

self.addEventListener("activate", (event) => {
    event.waitUntil(self.clients.claim());
});

self.addEventListener("push", (e) => {
    if (e.data === null)
        return;

    const data = e.data.json();
    if (data.event === undefined)
        return;

    const event = data.event;
    const payload = data.payload || {};

    let notificationTitle;
    let notificationBody;
    switch (event) {
        case "test":
            notificationTitle = "QBT_TR(Test Notification)QBT_TR[CONTEXT=PushNotification]";
            notificationBody = "QBT_TR(This is a test notification. Thank you for using qBittorrent.)QBT_TR[CONTEXT=PushNotification]";
            break;
        case "torrent_added":
            // ignore for now.
            return;
        case "torrent_finished":
            notificationTitle = "QBT_TR(Download completed)QBT_TR[CONTEXT=PushNotification]";
            notificationBody = "QBT_TR(%1 has finished downloading.)QBT_TR[CONTEXT=PushNotification]"
                .replace("%1", `"${payload.torrent_name}"`);
            break;
        case "full_disk_error":
            notificationTitle = "QBT_TR(I/O Error)QBT_TR[CONTEXT=PushNotification]";
            notificationBody = "QBT_TR(An I/O error occurred for torrent %1.\n Reason: %2)QBT_TR[CONTEXT=PushNotification]"
                .replace("%1", `"${payload.torrent_name}"`)
                .replace("%2", payload.reason);
            break;
        case "add_torrent_failed":
            notificationTitle = "QBT_TR(Add torrent failed)QBT_TR[CONTEXT=PushNotification]";
            notificationBody = "QBT_TR(Couldn't add torrent '%1', reason: %2.)QBT_TR[CONTEXT=PushNotification]"
                .replace("%1", payload.source)
                .replace("%2", payload.reason);
            break;
        default:
            notificationTitle = "QBT_TR(Unsupported notification)QBT_TR[CONTEXT=PushNotification]";
            notificationBody = "QBT_TR(An unsupported notification was received.)QBT_TR[CONTEXT=PushNotification]";
            break;
    }

    // Keep the service worker alive until the notification is created.
    e.waitUntil(
        self.registration.showNotification(notificationTitle, {
            body: notificationBody,
            icon: "images/qbittorrent-tray.svg"
        })
    );
});

self.addEventListener("notificationclick", (e) => {
    e.waitUntil(
        self.clients.matchAll({
            type: "window"
        }).then((clientList) => {
            for (const client of clientList) {
                if ("focus" in client)
                    return client.focus();
            }
            if (clients.openWindow)
                return clients.openWindow("/");
        })
    );
});
