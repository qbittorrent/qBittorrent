/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2019-2024  Mike Tzou (Chocobo1)
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

async function setupI18n() {
    const languages = (() => {
        const langs = new Set();

        // list of available languages: https://github.com/qbittorrent/qBittorrent/tree/master/src/webui/www/public/lang
        const queryLang = new URLSearchParams(window.location.search).get("lang");
        if (queryLang !== null) {
            // use the fallback lang if `queryLang` is present but empty
            // limit the length of the language string to prevent Client-side Request Forgery
            if ((queryLang.length > 0) && (queryLang.length <= 8))
                langs.add(queryLang.replace("-", "_"));
        }
        else {
            for (const lang of navigator.languages) {
                langs.add(lang.replace("-", "_"));

                const idx = lang.indexOf("-");
                if (idx > 0)
                    langs.add(lang.slice(0, idx));
            }
        }

        langs.add("en"); // fallback
        return Array.from(langs);
    })();

    // it is faster to fetch all translation files at once than one by one
    const fetches = languages.map(lang => fetch(`lang/${lang}.json`));
    const fetchResults = await Promise.allSettled(fetches);
    const translations = fetchResults
        .map((value, idx) => ({ lang: languages[idx], result: value }))
        .filter(v => (v.result.value.status === 200));
    const translation = {
        lang: (translations.length > 0) ? translations[0].lang.replace("_", "-") : undefined,
        data: (translations.length > 0) ? (await translations[0].result.value.json()) : {}
    };

    // present it to i18next
    const i18nextOptions = {
        lng: translation.lang,
        fallbackLng: false,
        load: "currentOnly",
        resources: {
            [translation.lang]: { translation: translation.data }
        },
        returnEmptyString: false
    };
    i18next.init(i18nextOptions, replaceI18nText);
}

function replaceI18nText() {
    const tr = i18next.t; // workaround for warnings from i18next-parser
    for (const element of document.getElementsByClassName("qbt-translatable")) {
        const translationKey = element.getAttribute("data-i18n");
        const translatedValue = tr(translationKey);
        switch (element.nodeName) {
            case "INPUT":
                element.value = translatedValue;
                break;
            case "LABEL":
                element.textContent = translatedValue;
                break;
            default:
                console.error(`Unhandled element: ${element}`);
                break;
        }
    }

    document.documentElement.lang = i18next.language.split("-")[0];
}

function submitLoginForm(event) {
    event.preventDefault();
    const errorMsgElement = document.getElementById("error_msg");

    const xhr = new XMLHttpRequest();
    xhr.open("POST", "api/v2/auth/login", true);
    xhr.setRequestHeader("Content-type", "application/x-www-form-urlencoded; charset=UTF-8");
    xhr.addEventListener("readystatechange", () => {
        if (xhr.readyState === 4) { // DONE state
            if ((xhr.status === 200) && (xhr.responseText === "Ok."))
                location.replace(location);
            else
                errorMsgElement.textContent = i18next.t("Invalid Username or Password.");
        }
    });
    xhr.addEventListener("error", () => {
        errorMsgElement.textContent = (xhr.responseText !== "")
            ? xhr.responseText
            : i18next.t("Unable to log in, server is probably unreachable.");
    });

    const usernameElement = document.getElementById("username");
    const passwordElement = document.getElementById("password");
    const queryString = "username=" + encodeURIComponent(usernameElement.value) + "&password=" + encodeURIComponent(passwordElement.value);
    xhr.send(queryString);

    // clear the field
    passwordElement.value = "";
}

document.addEventListener("DOMContentLoaded", () => {
    const loginForm = document.getElementById("loginform");
    loginForm.setAttribute("method", "POST");
    loginForm.addEventListener("submit", submitLoginForm);

    setupI18n();
});
