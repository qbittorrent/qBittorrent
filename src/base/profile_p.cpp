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

#include "profile_p.h"

#include <QCoreApplication>

#include "base/global.h"
#include "base/utils/fs.h"

Private::Profile::Profile(const QString &configurationName)
    : m_configurationName {configurationName}
{
}

QString Private::Profile::configurationName() const
{
    return m_configurationName;
}

QString Private::Profile::configurationSuffix() const
{
    return (m_configurationName.isEmpty() ? QString() : (u'_' + m_configurationName));
}

QString Private::Profile::profileName() const
{
    return QCoreApplication::applicationName() + configurationSuffix();
}

Private::DefaultProfile::DefaultProfile(const QString &configurationName)
    : Profile {configurationName}
{
}

Path Private::DefaultProfile::rootPath() const
{
    return {};
}

Path Private::DefaultProfile::basePath() const
{
    return Utils::Fs::homePath();
}

Path Private::DefaultProfile::cacheLocation() const
{
    return locationWithConfigurationName(QStandardPaths::CacheLocation);
}

Path Private::DefaultProfile::configLocation() const
{
#if defined(Q_OS_WIN)
    // On Windows QSettings stores files in FOLDERID_RoamingAppData\AppName
    return locationWithConfigurationName(QStandardPaths::AppDataLocation);
#else
    return locationWithConfigurationName(QStandardPaths::AppConfigLocation);
#endif
}

Path Private::DefaultProfile::dataLocation() const
{
#if defined(Q_OS_WIN) || defined (Q_OS_MACOS)
    return locationWithConfigurationName(QStandardPaths::AppLocalDataLocation);
#else
    // On Linux keep using the legacy directory ~/.local/share/data/ if it exists
    const Path genericDataPath {QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)};
    const Path profilePath {profileName()};
    const Path legacyDir = genericDataPath / Path(u"data"_s) / profilePath;

    const Path dataDir = genericDataPath / profilePath;

    if (!dataDir.exists() && legacyDir.exists())
    {
        qWarning("The legacy data directory '%s' is used. It is recommended to move its content to '%s'",
            qUtf8Printable(legacyDir.toString()), qUtf8Printable(dataDir.toString()));

        return legacyDir;
    }

    return dataDir;
#endif
}

Path Private::DefaultProfile::downloadLocation() const
{
    return Path(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
}

std::unique_ptr<QSettings> Private::DefaultProfile::applicationSettings(const QString &name) const
{
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    return std::make_unique<QSettings>(QSettings::IniFormat, QSettings::UserScope, profileName(), name);
#else
    return std::make_unique<QSettings>(profileName(), name);
#endif
}

Path Private::DefaultProfile::locationWithConfigurationName(const QStandardPaths::StandardLocation location) const
{
    return Path(QStandardPaths::writableLocation(location) + configurationSuffix());
}

Private::CustomProfile::CustomProfile(const Path &rootPath, const QString &configurationName)
    : Profile {configurationName}
    , m_rootPath {rootPath}
    , m_basePath {m_rootPath / Path(profileName())}
    , m_cacheLocation {m_basePath / Path(u"cache"_s)}
    , m_configLocation {m_basePath / Path(u"config"_s)}
    , m_dataLocation {m_basePath / Path(u"data"_s)}
    , m_downloadLocation {m_basePath / Path(u"downloads"_s)}
{
}

Path Private::CustomProfile::rootPath() const
{
    return m_rootPath;
}

Path Private::CustomProfile::basePath() const
{
    return m_basePath;
}

Path Private::CustomProfile::cacheLocation() const
{
    return m_cacheLocation;
}

Path Private::CustomProfile::configLocation() const
{
    return m_configLocation;
}

Path Private::CustomProfile::dataLocation() const
{
    return m_dataLocation;
}

Path Private::CustomProfile::downloadLocation() const
{
    return m_downloadLocation;
}

std::unique_ptr<QSettings> Private::CustomProfile::applicationSettings(const QString &name) const
{
    // here we force QSettings::IniFormat format always because we need it to be portable across platforms
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    const auto CONF_FILE_EXTENSION = u".ini"_s;
#else
    const auto CONF_FILE_EXTENSION = u".conf"_s;
#endif
    const Path settingsFilePath = configLocation() / Path(name + CONF_FILE_EXTENSION);
    return std::make_unique<QSettings>(settingsFilePath.data(), QSettings::IniFormat);
}

Path Private::NoConvertConverter::fromPortablePath(const Path &portablePath) const
{
    return portablePath;
}

Path Private::NoConvertConverter::toPortablePath(const Path &path) const
{
    return path;
}

Private::Converter::Converter(const Path &basePath)
    : m_basePath {basePath}
{
    Q_ASSERT(basePath.isAbsolute());
}

Path Private::Converter::toPortablePath(const Path &path) const
{
    if (path.isEmpty())
        return path;

#ifdef Q_OS_WIN
    if (path.isAbsolute())
    {
        const QChar driveLetter = path.data()[0].toUpper();
        const QChar baseDriveLetter = m_basePath.data()[0].toUpper();
        const bool onSameDrive = (driveLetter.category() == QChar::Letter_Uppercase) && (driveLetter == baseDriveLetter);
        if (!onSameDrive)
            return path;
    }
#endif
    return m_basePath.relativePathOf(path);
}

Path Private::Converter::fromPortablePath(const Path &portablePath) const
{
    if (portablePath.isEmpty() || portablePath.isAbsolute())
        return portablePath;

    return m_basePath / portablePath;
}
