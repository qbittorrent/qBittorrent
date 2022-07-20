/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016  Eugene Shalygin <eugene.shalygin@gmail.com>
 * Copyright (C) 2012  Christophe Dumez
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

#include "profile.h"

#include "base/path.h"
#include "base/utils/fs.h"
#include "profile_p.h"

Profile *Profile::m_instance = nullptr;

Profile::Profile(const Path &rootProfilePath, const QString &configurationName, const bool convertPathsToProfileRelative)
{
    if (rootProfilePath.isEmpty())
        m_profileImpl = std::make_unique<Private::DefaultProfile>(configurationName);
    else
        m_profileImpl = std::make_unique<Private::CustomProfile>(rootProfilePath, configurationName);

    ensureDirectoryExists(SpecialFolder::Cache);
    ensureDirectoryExists(SpecialFolder::Config);
    ensureDirectoryExists(SpecialFolder::Data);

    if (convertPathsToProfileRelative)
        m_pathConverterImpl = std::make_unique<Private::Converter>(m_profileImpl->basePath());
    else
        m_pathConverterImpl = std::make_unique<Private::NoConvertConverter>();
}

void Profile::initInstance(const Path &rootProfilePath, const QString &configurationName,
    const bool convertPathsToProfileRelative)
{
    if (m_instance)
        return;
    m_instance = new Profile(rootProfilePath, configurationName, convertPathsToProfileRelative);
}

void Profile::freeInstance()
{
    delete m_instance;
    m_instance = nullptr;
}

const Profile *Profile::instance()
{
    return m_instance;
}

Path Profile::location(const SpecialFolder folder) const
{
    switch (folder)
    {
    case SpecialFolder::Cache:
        return m_profileImpl->cacheLocation();

    case SpecialFolder::Config:
        return m_profileImpl->configLocation();

    case SpecialFolder::Data:
        return m_profileImpl->dataLocation();

    case SpecialFolder::Downloads:
        return m_profileImpl->downloadLocation();

    }

    Q_ASSERT_X(false, Q_FUNC_INFO, "Unknown SpecialFolder value.");
    return {};
}

Path Profile::rootPath() const
{
    return m_profileImpl->rootPath();
}

QString Profile::configurationName() const
{
    return m_profileImpl->configurationName();
}

QString Profile::profileName() const
{
    return m_profileImpl->profileName();
}

std::unique_ptr<QSettings> Profile::applicationSettings(const QString &name) const
{
    return m_profileImpl->applicationSettings(name);
}

void Profile::ensureDirectoryExists(const SpecialFolder folder) const
{
    const Path locationPath = location(folder);
    if (!locationPath.isEmpty() && !Utils::Fs::mkpath(locationPath))
        qFatal("Could not create required directory '%s'", qUtf8Printable(locationPath.toString()));
}

Path Profile::toPortablePath(const Path &absolutePath) const
{
    return m_pathConverterImpl->toPortablePath(absolutePath);
}

Path Profile::fromPortablePath(const Path &portablePath) const
{
    return m_pathConverterImpl->fromPortablePath(portablePath);
}

Path specialFolderLocation(const SpecialFolder folder)
{
    return Profile::instance()->location(folder);
}
