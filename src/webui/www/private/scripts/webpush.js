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

window.qBittorrent ??= {};
window.qBittorrent.WebPush ??= (() => {
    const exports = () => {
        return {
            isSupported: isSupported,
            isSubscribed: isSubscribed,
            registerServiceWorker: registerServiceWorker,
            sendTestNotification: sendTestNotification,
            subscribe: subscribe,
            unsubscribe: unsubscribe,
        };
    };

    const isSupported = () => {
        return (
            window.isSecureContext
            && ("serviceWorker" in navigator)
            && ("PushManager" in window)
            && ("Notification" in window)
        );
    };

    const registerServiceWorker = async () => {
        const officialWebUIServiceWorkerScript = "/sw-webui.js";
        const registrations = await navigator.serviceWorker.getRegistrations();
        let registered = false;
        for (const registration of registrations) {
            const isOfficialWebUI = registration.active && registration.active.scriptURL.endsWith(officialWebUIServiceWorkerScript);
            if (isOfficialWebUI) {
                registered = true;
                continue;
            }
            else {
                await registration.unregister();
            }
        }
        if (!registered)
            await navigator.serviceWorker.register(officialWebUIServiceWorkerScript);
    };

    const urlBase64ToUint8Array = (base64String) => {
        const padding = "=".repeat((4 - base64String.length % 4) % 4);
        const base64 = (base64String + padding)
            .replace(/-/g, "+")
            .replace(/_/g, "/");

        const rawData = window.atob(base64);
        const outputArray = new Uint8Array(rawData.length);

        for (let i = 0; i < rawData.length; ++i)
            outputArray[i] = rawData.charCodeAt(i);

        return outputArray;
    };

    const requestNotificationPermission = async () => {
        if (Notification.permission === "granted")
            return true;
        const permission = await Notification.requestPermission();
        return permission === "granted";
    };

    const fetchVapidPublicKey = async () => {
        const url = new URL("api/v2/push/vapidPublicKey", window.location);
        const response = await fetch(url, {
            method: "GET",
            cache: "no-store"
        });
        if (!response.ok)
            throw new Error("QBT_TR(Failed to fetch VAPID public key)QBT_TR[CONTEXT=PushNotification]");
        const responseJSON = await response.json();
        return responseJSON["vapidPublicKey"];
    };

    const getPushManager = async () => {
        const registration = await navigator.serviceWorker.ready;
        return registration.pushManager;
    };

    const subscribeToPushManager = async (vapidPublicKey) => {
        const pushManager = await getPushManager();
        const subscription = await pushManager.getSubscription();
        if (subscription !== null)
            return subscription;
        const convertedVapidKey = urlBase64ToUint8Array(vapidPublicKey);

        return pushManager.subscribe({
            userVisibleOnly: true,
            applicationServerKey: convertedVapidKey
        });
    };

    const subscribeToServer = async (subscription) => {
        const formData = new FormData();
        formData.append("subscription", JSON.stringify(subscription));
        const url = new URL("api/v2/push/subscribe", window.location);
        const response = await fetch(url, {
            method: "post",
            body: formData,
        });
        if (!response.ok)
            throw new Error(await response.text());
    };

    const unsubscribeFromServer = async (subscription) => {
        const formData = new FormData();
        formData.append("endpoint", subscription.endpoint);
        const url = new URL("api/v2/push/unsubscribe", window.location);
        const response = await fetch(url, {
            method: "post",
            body: formData,
        });
        if (!response.ok)
            throw new Error(await response.text());
    };

    const sendTestNotification = async () => {
        const url = new URL("api/v2/push/test", window.location);
        const response = await fetch(url, {
            method: "GET",
            cache: "no-store"
        });
        if (!response.ok)
            throw new Error(await response.text());
    };

    const subscribe = async () => {
        const permissionGranted = await requestNotificationPermission();
        if (!permissionGranted)
            throw new Error("QBT_TR(Notification permission denied.)QBT_TR[CONTEXT=PushNotification]");
        const vapidPublicKey = await fetchVapidPublicKey();
        const subscription = await subscribeToPushManager(vapidPublicKey);
        await subscribeToServer(subscription);
    };

    const isSubscribed = async () => {
        const pushManager = await getPushManager();
        const subscription = await pushManager.getSubscription();
        return subscription !== null;
    };

    const unsubscribe = async () => {
        const pushManager = await getPushManager();
        const subscription = await pushManager.getSubscription();
        if (subscription !== null) {
            await subscription.unsubscribe();
            await unsubscribeFromServer(subscription);
        }
    };

    return exports();
})();
Object.freeze(window.qBittorrent.WebPush);

document.addEventListener("DOMContentLoaded", () => {
    if (window.qBittorrent.WebPush.isSupported()) {
        window.qBittorrent.WebPush.registerServiceWorker().catch((error) => {
            console.error("Failed to register service worker:", error);
        });
    }
});
