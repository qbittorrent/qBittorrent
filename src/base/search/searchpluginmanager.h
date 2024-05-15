/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2024  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#include <QHash>
#include <QMetaType>
#include <QObject>

#include "base/path.h"
#include "base/utils/version.h"

using PluginVersion = Utils::Version<2>;
Q_DECLARE_METATYPE(PluginVersion)

namespace Net
{
    struct DownloadResult;
}

struct PluginInfo
{
    QString name;
    PluginVersion version;
    QString fullName;
    QString url;
    QStringList supportedCategories;
    Path iconPath;
    bool enabled = false;
};

class SearchDownloadHandler;
class SearchHandler;

class SearchPluginManager final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SearchPluginManager)

public:
    SearchPluginManager();
    ~SearchPluginManager() override;

    static SearchPluginManager *instance();
    static void freeInstance();

    QStringList allPlugins() const;
    QStringList enabledPlugins() const;
    QStringList supportedCategories() const;
    QStringList getPluginCategories(const QString &pluginName) const;
    PluginInfo *pluginInfo(const QString &name) const;
    QString pluginNameBySiteURL(const QString &siteURL) const;

    void enablePlugin(const QString &name, bool enabled = true);
    void updatePlugin(const QString &name);
    void installPlugin(const QString &source);
    bool uninstallPlugin(const QString &name);
    static void updateIconPath(PluginInfo *plugin);
    void checkForUpdates();

    SearchHandler *startSearch(const QString &pattern, const QString &category, const QStringList &usedPlugins);
    SearchDownloadHandler *downloadTorrent(const QString &pluginName, const QString &url);

    static PluginVersion getPluginVersion(const Path &filePath);
    static QString categoryFullName(const QString &categoryName);
    QString pluginFullName(const QString &pluginName) const;
    static Path pluginsLocation();
    static Path engineLocation();

signals:
    void pluginEnabled(const QString &name, bool enabled);
    void pluginInstalled(const QString &name);
    void pluginInstallationFailed(const QString &name, const QString &reason);
    void pluginUninstalled(const QString &name);
    void pluginUpdated(const QString &name);
    void pluginUpdateFailed(const QString &name, const QString &reason);

    void checkForUpdatesFinished(const QHash<QString, PluginVersion> &updateInfo);
    void checkForUpdatesFailed(const QString &reason);

private:
    void applyProxySettings();
    void update();
    void updateNova();
    void parseVersionInfo(const QByteArray &info);
    void installPlugin_impl(const QString &name, const Path &path);
    bool isUpdateNeeded(const QString &pluginName, const PluginVersion &newVersion) const;

    void versionInfoDownloadFinished(const Net::DownloadResult &result);
    void pluginDownloadFinished(const Net::DownloadResult &result);

    static Path pluginPath(const QString &name);

    static QPointer<SearchPluginManager> m_instance;

    const QString m_updateUrl;

    QHash<QString, PluginInfo*> m_plugins;
};
