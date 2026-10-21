/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Andy Ye
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

// GIO has a member named "signals"; this translation unit does not use Qt keywords.
#ifndef QT_NO_KEYWORDS
#define QT_NO_KEYWORDS
#endif

#include "launch.h"

#include <algorithm>
#include <memory>
#include <vector>

#include <gio/gio.h>

#include <QByteArray>

namespace
{
    using AppInfoPtr = std::unique_ptr<GAppInfo, decltype(&g_object_unref)>;

    struct ApplicationFiles
    {
        AppInfoPtr application;
        QStringList urls;
    };
}

QStringList Utils::Gui::launchWithDefaultApplications(const QStringList &urls)
{
    std::vector<ApplicationFiles> applications;
    QStringList unhandled;

    for (const QString &url : urls)
    {
        GFile *file = g_file_new_for_uri(url.toUtf8().constData());
        AppInfoPtr application {g_file_query_default_handler(file, nullptr, nullptr), &g_object_unref};
        g_object_unref(file);
        if (!application)
        {
            unhandled.append(url);
            continue;
        }

        const auto it = std::find_if(applications.begin(), applications.end(), [&application](const auto &entry)
        {
            return g_app_info_equal(entry.application.get(), application.get());
        });
        if (it != applications.end())
            it->urls.append(url);
        else
            applications.push_back({std::move(application), {url}});
    }

    for (const ApplicationFiles &entry : applications)
    {
        GList *uris = nullptr;
        for (const QString &url : entry.urls)
            uris = g_list_prepend(uris, g_strdup(url.toUtf8().constData()));
        uris = g_list_reverse(uris);

        // Let GIO handle desktop-file field codes and D-Bus activation.
        if (!g_app_info_launch_uris(entry.application.get(), uris, nullptr, nullptr))
            unhandled.append(entry.urls);
        g_list_free_full(uris, g_free);
    }

    return unhandled;
}
