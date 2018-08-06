/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2011  Christophe Dumez <chris@qbittorrent.org>
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

#include "iconprovider.h"

#include <array>

#include <QFileInfo>

namespace
{
    const std::array<QLatin1String, 5> THEME_DIRS = {
        QLatin1String("actions/"),
        QLatin1String("mimetypes/"),
        QLatin1String("places/"),
        QLatin1String("preferences/"),
        QLatin1String("status/")
    };
}

IconProvider::IconProvider(QObject *parent)
    : QObject(parent)
{
}

IconProvider::~IconProvider() {}

void IconProvider::initInstance()
{
    if (!m_instance)
        m_instance = new IconProvider;
}

void IconProvider::freeInstance()
{
    if (m_instance) {
        delete m_instance;
        m_instance = nullptr;
    }
}

IconProvider *IconProvider::instance()
{
    return m_instance;
}

QString IconProvider::getIconPath(const QString &iconId) const
{
    const auto it = m_iconFileNames.find(iconId);
    if (it != m_iconFileNames.end())
        return it.value();

    const QString iconThemeDir = QLatin1String(":/icons/qbt-theme/");
    const QString fileSuffix = iconId + QLatin1String(".svg");
    for (const auto dir : THEME_DIRS) {
        const QString probe = iconThemeDir + dir + fileSuffix;
        if (QFileInfo::exists(probe))
            return m_iconFileNames[iconId] = probe;
    }
    throw std::runtime_error("could not find icon " + iconId.toStdString());
}

IconProvider *IconProvider::m_instance = nullptr;
