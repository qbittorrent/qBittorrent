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

#include <QStandardPaths>

#include "base/path.h"
#include "profile.h"

namespace Private
{
    class Profile
    {
    public:
        virtual ~Profile() = default;

        virtual Path rootPath() const = 0;

        /**
         * @brief The base path against to which portable (relative) paths are resolved
         */
        virtual Path basePath() const = 0;

        virtual Path cacheLocation() const = 0;
        virtual Path configLocation() const = 0;
        virtual Path dataLocation() const = 0;
        virtual Path downloadLocation() const = 0;

        virtual std::unique_ptr<QSettings> applicationSettings(const QString &name) const = 0;

        QString configurationName() const;

        /**
         * @brief QCoreApplication::applicationName() with optional configuration name appended
         */
        QString profileName() const;

    protected:
        explicit Profile(const QString &configurationName);

        QString configurationSuffix() const;

    private:
        QString m_configurationName;
    };

    /// Default implementation. Takes paths from system
    class DefaultProfile final : public Profile
    {
    public:
        explicit DefaultProfile(const QString &configurationName);

        Path rootPath() const override;
        Path basePath() const override;
        Path cacheLocation() const override;
        Path configLocation() const override;
        Path dataLocation() const override;
        Path downloadLocation() const override;
        std::unique_ptr<QSettings> applicationSettings(const QString &name) const override;

    private:
        /**
         * @brief Standard path writable location for profile files
         *
         * @param location location kind
         * @return QStandardPaths::writableLocation(location) / configurationName()
         */
        Path locationWithConfigurationName(QStandardPaths::StandardLocation location) const;
    };

    /// Custom tree: creates directories under the specified root directory
    class CustomProfile final : public Profile
    {
    public:
        CustomProfile(const Path &rootPath, const QString &configurationName);

        Path rootPath() const override;
        Path basePath() const override;
        Path cacheLocation() const override;
        Path configLocation() const override;
        Path dataLocation() const override;
        Path downloadLocation() const override;
        std::unique_ptr<QSettings> applicationSettings(const QString &name) const override;

    private:
        const Path m_rootPath;
        const Path m_basePath;
        const Path m_cacheLocation;
        const Path m_configLocation;
        const Path m_dataLocation;
        const Path m_downloadLocation;
    };

    class PathConverter
    {
    public:
        virtual Path toPortablePath(const Path &path) const = 0;
        virtual Path fromPortablePath(const Path &portablePath) const = 0;
        virtual ~PathConverter() = default;
    };

    class NoConvertConverter final : public PathConverter
    {
    public:
        Path toPortablePath(const Path &path) const override;
        Path fromPortablePath(const Path &portablePath) const override;
    };

    class Converter final : public PathConverter
    {
    public:
        explicit Converter(const Path &basePath);
        Path toPortablePath(const Path &path) const override;
        Path fromPortablePath(const Path &portablePath) const override;

    private:
        Path m_basePath;
    };
}
