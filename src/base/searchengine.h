/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
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

#ifndef SEARCHENGINE_H
#define SEARCHENGINE_H

#include <QObject>
#include <QHash>
#include <QStringList>
#include <QList>

class QProcess;
class QTimer;

struct PluginInfo
{
    QString name;
    qreal version;
    QString fullName;
    QString url;
    QStringList supportedCategories;
    QString iconPath;
    bool enabled;
};

struct SearchResult
{
    QString fileName;
    QString fileUrl;
    qlonglong fileSize;
    qlonglong nbSeeders;
    qlonglong nbLeechers;
    QString siteUrl;
    QString descrLink;
};

class SearchEngine: public QObject
{
    Q_OBJECT

public:
    SearchEngine();
    ~SearchEngine();

    QStringList allPlugins() const;
    QStringList enabledPlugins() const;
    QStringList supportedCategories() const;
    PluginInfo *pluginInfo(const QString &name) const;

    bool isActive() const;

    void enablePlugin(const QString &name, bool enabled = true);
    void updatePlugin(const QString &name);
    void installPlugin(const QString &source);
    bool uninstallPlugin(const QString &name);
    void checkForUpdates();

    void startSearch(const QString &pattern, const QString &category, const QStringList &usedPlugins);
    void cancelSearch();

    static qreal getPluginVersion(QString filePath);
    static QString categoryFullName(const QString &categoryName);
    static QString pluginsLocation();

signals:
    void searchStarted();
    void searchFinished(bool cancelled);
    void searchFailed();
    void newSearchResults(const QList<SearchResult> &results);

    void pluginInstalled(const QString &name);
    void pluginInstallationFailed(const QString &name, const QString &reason);
    void pluginUpdated(const QString &name);
    void pluginUpdateFailed(const QString &name, const QString &reason);

    void checkForUpdatesFinished(const QHash<QString, qreal> &updateInfo);
    void checkForUpdatesFailed(const QString &reason);

private slots:
    void onTimeout();
    void readSearchOutput();
    void processFinished(int exitcode);
    void versionInfoDownloaded(const QString &url, const QByteArray &data);
    void versionInfoDownloadFailed(const QString &url, const QString &reason);
    void pluginDownloaded(const QString &url, QString filePath);
    void pluginDownloadFailed(const QString &url, const QString &reason);

private:
    void update();
    void updateNova();
    bool parseSearchResult(const QString &line, SearchResult &searchResult);
    void parseVersionInfo(const QByteArray &info);
    void installPlugin_impl(const QString &name, const QString &path);
    bool isUpdateNeeded(QString pluginName, qreal newVersion) const;

    static QString engineLocation();
    static QString pluginPath(const QString &name);
    static QHash<QString, QString> initializeCategoryNames();

    static const QHash<QString, QString> m_categoryNames;

    const QString m_updateUrl;

    QHash<QString, PluginInfo*> m_plugins;
    QProcess *m_searchProcess;
    bool m_searchStopped;
    QTimer *m_searchTimeout;
    QByteArray m_searchResultLineTruncated;
};

#endif // SEARCHENGINE_H
