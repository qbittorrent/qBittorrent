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

const submitLoginForm = (event) => {
    event.preventDefault();

    const errorMsgElement = document.getElementById("error_msg");
    errorMsgElement.textContent = ""; // clear previous error

    const usernameElement = document.getElementById("username");
    const passwordElement = document.getElementById("password");

    fetch("api/v2/auth/login", {
            method: "POST",
            body: new URLSearchParams({
                username: usernameElement.value,
                password: passwordElement.value
            })
        })
        .then(async (response) => {
                const responseText = await response.text();
                if (response.ok && (responseText === "Ok.")) {
                    location.replace(location); // redirect
                    location.reload(true);
                }
                else {
                    errorMsgElement.textContent = `QBT_TR(Invalid Username or Password.)QBT_TR[CONTEXT=Login]\nQBT_TR(Server response:)QBT_TR[CONTEXT=Login] ${responseText}`;
                }
            },
            (error) => {
                errorMsgElement.textContent = `QBT_TR(Unable to log in, server is probably unreachable.)QBT_TR[CONTEXT=Login]\n${error}`;
            });

    passwordElement.value = ""; // clear previous value
};

document.addEventListener("DOMContentLoaded", () => {
    const loginForm = document.getElementById("loginform");
    loginForm.method = "POST";
    loginForm.addEventListener("submit", submitLoginForm);
});
