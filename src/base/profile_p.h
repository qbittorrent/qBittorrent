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

#pragma once

#include <QDir>
#include <QStandardPaths>

#include "base/profile.h"

namespace Private
{
    class Profile
    {
    public:
        virtual QString baseDirectory() const = 0;
        virtual QString cacheLocation() const = 0;
        virtual QString configLocation() const = 0;
        virtual QString dataLocation() const = 0;
        virtual QString downloadLocation() const = 0;
        virtual SettingsPtr applicationSettings(const QString &name) const = 0;

        virtual ~Profile() = default;

        /**
         * @brief QCoreApplication::applicationName() with optional configuration name appended
         */
        QString profileName() const;

    protected:
        explicit Profile(const QString &configurationName);

        QString configurationSuffix() const;
    private:
        QString m_configurationSuffix;
    };

    /// Default implementation. Takes paths from system
    class DefaultProfile final : public Profile
    {
    public:
        explicit DefaultProfile(const QString &configurationName);

        QString baseDirectory() const override;
        QString cacheLocation() const override;
        QString configLocation() const override;
        QString dataLocation() const override;
        QString downloadLocation() const override;
        SettingsPtr applicationSettings(const QString &name) const override;

    private:
        /**
         * @brief Standard path writable location for profile files
         *
         * @param location location kind
         * @return QStandardPaths::writableLocation(location) / configurationName()
         */
        QString locationWithConfigurationName(QStandardPaths::StandardLocation location) const;
    };

    /// Custom tree: creates directories under the specified root directory
    class CustomProfile final : public Profile
    {
    public:
        CustomProfile(const QString &rootPath, const QString &configurationName);

        QString baseDirectory() const override;
        QString cacheLocation() const override;
        QString configLocation() const override;
        QString dataLocation() const override;
        QString downloadLocation() const override;
        SettingsPtr applicationSettings(const QString &name) const override;

    private:
        QDir m_rootDirectory;
        static constexpr const char *cacheDirName = "cache";
        static constexpr const char *configDirName = "config";
        static constexpr const char *dataDirName = "data";
        static constexpr const char *downloadsDirName = "downloads";
    };

    class PathConverter
    {
    public:
        virtual QString toPortablePath(const QString &path) const = 0;
        virtual QString fromPortablePath(const QString &portablePath) const = 0;
        virtual ~PathConverter() = default;
    };

    class NoConvertConverter final : public PathConverter
    {
    public:
        QString toPortablePath(const QString &path) const override;
        QString fromPortablePath(const QString &portablePath) const override;
    };

    class Converter final : public PathConverter
    {
    public:
        explicit Converter(const QString &basePath);
        QString toPortablePath(const QString &path) const override;
        QString fromPortablePath(const QString &portablePath) const override;

    private:
        QDir m_baseDir;
    };
}
