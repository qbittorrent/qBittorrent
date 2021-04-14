/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2019  Mike Tzou (Chocobo1)
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

document.addEventListener('DOMContentLoaded', function() {
    document.getElementById('username').focus();
    document.getElementById('username').select();

    document.getElementById('loginform').addEventListener('submit', function(e) {
        e.preventDefault();
    });
});

function submitLoginForm() {
    const errorMsgElement = document.getElementById('error_msg');

    const xhr = new XMLHttpRequest();
    xhr.open('POST', 'api/v2/auth/login', true);
    xhr.setRequestHeader('Content-type', 'application/x-www-form-urlencoded; charset=UTF-8');
    xhr.addEventListener('readystatechange', function() {
        if (xhr.readyState === 4) { // DONE state
            if ((xhr.status === 200) && (xhr.responseText === "Ok."))
                location.reload(true);
            else
                errorMsgElement.innerHTML = 'QBT_TR(Invalid Username or Password.)QBT_TR[CONTEXT=HttpServer]';
        }
    });
    xhr.addEventListener('error', function() {
        errorMsgElement.innerHTML = (xhr.responseText !== "")
            ? xhr.responseText
            : 'QBT_TR(Unable to log in, qBittorrent is probably unreachable.)QBT_TR[CONTEXT=HttpServer]';
    });

    const usernameElement = document.getElementById('username');
    const passwordElement = document.getElementById('password');
    const queryString = "username=" + encodeURIComponent(usernameElement.value) + "&password=" + encodeURIComponent(passwordElement.value);
    xhr.send(queryString);

    // clear the field
    passwordElement.value = '';
}
