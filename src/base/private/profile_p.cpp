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
 *
 */

#include "profile_p.h"

#include <QCoreApplication>

#include <QStandardPaths>

#ifdef Q_OS_MAC
#include <CoreServices/CoreServices.h>
#include <Carbon/Carbon.h>
#endif

#ifdef Q_OS_WIN
#include <shlobj.h>
#endif

#include "base/utils/fs.h"

Private::Profile::Profile(const QString &configurationName)
    : m_configurationName {configurationName.isEmpty()
                           ? QCoreApplication::applicationName()
                           : QCoreApplication::applicationName() + QLatin1Char('_') + configurationName}
{
}

QString Private::Profile::configurationName() const
{
    return m_configurationName;
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
    QString result;
#if defined(Q_OS_WIN) || defined(Q_OS_OS2)
    result = dataLocation() + QLatin1String("cache");
#else
#ifdef Q_OS_MAC
    // http://developer.apple.com/documentation/Carbon/Reference/Folder_Manager/Reference/reference.html
    FSRef ref;
    OSErr err = FSFindFolder(kUserDomain, kCachedDataFolderType, false, &ref);
    if (err)
        return QString();
    QByteArray ba(2048, 0);
    if (FSRefMakePath(&ref, reinterpret_cast<UInt8 *>(ba.data()), ba.size()) == noErr)
        result = QString::fromUtf8(ba).normalized(QString::NormalizationForm_C);
    result += QLatin1Char('/') + configurationName();
#else
    QString xdgCacheHome = QLatin1String(qgetenv("XDG_CACHE_HOME"));
    if (xdgCacheHome.isEmpty())
        xdgCacheHome = QDir::homePath() + QLatin1String("/.cache");
    xdgCacheHome += QLatin1Char('/') + configurationName();
    result = xdgCacheHome;
#endif
#endif
    if (!result.endsWith("/"))
        result += "/";
    return result;
}

QString Private::DefaultProfile::configLocation() const
{
    QString result;
#if defined(Q_OS_WIN) || defined(Q_OS_OS2)
    result = dataLocation() + QLatin1String("config");
#else
#ifdef Q_OS_MAC
    result = QDir::homePath() + QLatin1String("/Library/Preferences/") + configurationName();
#else
    QString xdgConfigHome = QLatin1String(qgetenv("XDG_CONFIG_HOME"));
    if (xdgConfigHome.isEmpty())
        xdgConfigHome = QDir::homePath() + QLatin1String("/.config");
    xdgConfigHome += QLatin1Char('/') + configurationName();
    result = xdgConfigHome;
#endif
#endif
    return result;
}

QString Private::DefaultProfile::dataLocation() const
{
    QString result;
#if defined(Q_OS_WIN)
    wchar_t path[MAX_PATH + 1] = {L'\0'};
    if (SHGetSpecialFolderPathW(0, path, CSIDL_LOCAL_APPDATA, FALSE))
        result = Utils::Fs::fromNativePath(QString::fromWCharArray(path));
    if (!QCoreApplication::applicationName().isEmpty())
        result += QLatin1String("/") + qApp->applicationName();
#elif defined(Q_OS_MAC)
    FSRef ref;
    OSErr err = FSFindFolder(kUserDomain, kApplicationSupportFolderType, false, &ref);
    if (err)
        return QString();
    QByteArray ba(2048, 0);
    if (FSRefMakePath(&ref, reinterpret_cast<UInt8 *>(ba.data()), ba.size()) == noErr)
        result = QString::fromUtf8(ba).normalized(QString::NormalizationForm_C);
    result += QLatin1Char('/') + qApp->applicationName();
#else
    QString xdgDataHome = QLatin1String(qgetenv("XDG_DATA_HOME"));
    if (xdgDataHome.isEmpty())
        xdgDataHome = QDir::homePath() + QLatin1String("/.local/share");
    xdgDataHome += QLatin1String("/data/")
            + qApp->applicationName();
    result = xdgDataHome;
#endif
    if (!result.endsWith("/"))
        result += "/";
    return result;
}

QString Private::DefaultProfile::downloadLocation() const
{
#if defined(Q_OS_WIN)
    if (QSysInfo::windowsVersion() <= QSysInfo::WV_XP)  // Windows XP
        return QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).absoluteFilePath(
            QCoreApplication::translate("fsutils", "Downloads"));
#endif
    return QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
}

SettingsPtr Private::DefaultProfile::applicationSettings(const QString &name) const
{
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    return SettingsPtr(new QSettings(QSettings::IniFormat, QSettings::UserScope, configurationName(), name));
#else
    return SettingsPtr(new QSettings(configurationName(), name));
#endif
}

Private::CustomProfile::CustomProfile(const QString &rootPath, const QString &configurationName)
    : Profile {configurationName}
    , m_rootDirectory {QDir(rootPath).absoluteFilePath(this->configurationName())}
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
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
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
    if (QDir::isAbsolutePath(path)) {
        QChar driveLeter = path[0].toUpper();
        QChar baseDriveLetter = m_baseDir.path()[0].toUpper();
        bool onSameDrive = (driveLeter.category() == QChar::Letter_Uppercase) && (driveLeter == baseDriveLetter);
        if (!onSameDrive)
            return path;
    }
#endif
    return m_baseDir.relativeFilePath(path);
}

QString Private::Converter::fromPortablePath(const QString &portablePath) const
{
    if (QDir::isAbsolutePath(portablePath))
        return portablePath;

    return QDir::cleanPath(m_baseDir.absoluteFilePath(portablePath));
}
