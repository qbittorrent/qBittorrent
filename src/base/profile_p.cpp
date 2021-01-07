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

Private::Profile::Profile(const QString &configurationName)
    : m_configurationSuffix {configurationName.isEmpty() ? QString() : QLatin1Char('_') + configurationName}
{
}

QString Private::Profile::configurationSuffix() const
{
    return m_configurationSuffix;
}

QString Private::Profile::profileName() const
{
    return QCoreApplication::applicationName() + configurationSuffix();
}

Private::DefaultProfile::DefaultProfile(const QString &configurationName)
    : Profile(configurationName)
{
}

QString Private::DefaultProfile::baseDirectory() const
{
    return QDir::homePath();
}

QString Private::DefaultProfile::cacheLocation() const
{
    return locationWithConfigurationName(QStandardPaths::CacheLocation);
}

QString Private::DefaultProfile::configLocation() const
{
#if defined(Q_OS_WIN)
    // On Windows QSettings stores files in FOLDERID_RoamingAppData\AppName
    return locationWithConfigurationName(QStandardPaths::AppDataLocation);
#else
    return locationWithConfigurationName(QStandardPaths::AppConfigLocation);
#endif
}

QString Private::DefaultProfile::dataLocation() const
{
#if defined(Q_OS_WIN) || defined (Q_OS_MACOS)
    return locationWithConfigurationName(QStandardPaths::AppLocalDataLocation);
#else
    // On Linux keep using the legacy directory ~/.local/share/data/ if it exists
    const QString legacyDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QLatin1String("/data/") + profileName() + QLatin1Char('/');

    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QLatin1Char('/') + profileName() + QLatin1Char('/');

    if (!QDir(dataDir).exists() && QDir(legacyDir).exists())
    {
        qWarning("The legacy data directory '%s' is used. It is recommended to move its content to '%s'",
            qUtf8Printable(legacyDir), qUtf8Printable(dataDir));

        return legacyDir;
    }

    return dataDir;
#endif
}

QString Private::DefaultProfile::downloadLocation() const
{
    return QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
}

SettingsPtr Private::DefaultProfile::applicationSettings(const QString &name) const
{
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    return SettingsPtr(new QSettings(QSettings::IniFormat, QSettings::UserScope, profileName(), name));
#else
    return SettingsPtr(new QSettings(profileName(), name));
#endif
}

QString Private::DefaultProfile::locationWithConfigurationName(const QStandardPaths::StandardLocation location) const
{
    return QStandardPaths::writableLocation(location) + configurationSuffix();
}

Private::CustomProfile::CustomProfile(const QString &rootPath, const QString &configurationName)
    : Profile {configurationName}
    , m_rootDirectory {QDir(rootPath).absoluteFilePath(this->profileName())}
{
}

QString Private::CustomProfile::baseDirectory() const
{
    return m_rootDirectory.canonicalPath();
}

QString Private::CustomProfile::cacheLocation() const
{
    return m_rootDirectory.absoluteFilePath(QLatin1String(cacheDirName));
}

QString Private::CustomProfile::configLocation() const
{
    return m_rootDirectory.absoluteFilePath(QLatin1String(configDirName));
}

QString Private::CustomProfile::dataLocation() const
{
    return m_rootDirectory.absoluteFilePath(QLatin1String(dataDirName));
}

QString Private::CustomProfile::downloadLocation() const
{
    return m_rootDirectory.absoluteFilePath(QLatin1String(downloadsDirName));
}

SettingsPtr Private::CustomProfile::applicationSettings(const QString &name) const
{
    // here we force QSettings::IniFormat format always because we need it to be portable across platforms
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    constexpr const char *CONF_FILE_EXTENSION = ".ini";
#else
    constexpr const char *CONF_FILE_EXTENSION = ".conf";
#endif
    const QString settingsFileName {QDir(configLocation()).absoluteFilePath(name + QLatin1String(CONF_FILE_EXTENSION))};
    return SettingsPtr(new QSettings(settingsFileName, QSettings::IniFormat));
}

QString Private::NoConvertConverter::fromPortablePath(const QString &portablePath) const
{
    return portablePath;
}

QString Private::NoConvertConverter::toPortablePath(const QString &path) const
{
    return path;
}

Private::Converter::Converter(const QString &basePath)
    : m_baseDir {basePath}
{
    m_baseDir.makeAbsolute();
}

QString Private::Converter::toPortablePath(const QString &path) const
{
    if (path.isEmpty() || m_baseDir.path().isEmpty())
        return path;

#ifdef Q_OS_WIN
    if (QDir::isAbsolutePath(path))
    {
        const QChar driveLeter = path[0].toUpper();
        const QChar baseDriveLetter = m_baseDir.path()[0].toUpper();
        const bool onSameDrive = (driveLeter.category() == QChar::Letter_Uppercase) && (driveLeter == baseDriveLetter);
        if (!onSameDrive)
            return path;
    }
#endif
    return m_baseDir.relativeFilePath(path);
}

QString Private::Converter::fromPortablePath(const QString &portablePath) const
{
    if (portablePath.isEmpty() || QDir::isAbsolutePath(portablePath))
        return portablePath;

    return QDir::cleanPath(m_baseDir.absoluteFilePath(portablePath));
}
