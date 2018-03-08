/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015, 2018  Vladimir Golovnev <glassez@yandex.ru>
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

#include "searchpluginmanager.h"

#include <QDebug>
#include <QDir>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <QList>
#include <QPointer>
#include <QProcess>

#include "base/logger.h"
#include "base/net/downloadmanager.h"
#include "base/net/downloadhandler.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "searchdownloadhandler.h"
#include "searchhandler.h"

namespace
{
    inline void removePythonScriptIfExists(const QString &scriptPath)
    {
        Utils::Fs::forceRemove(scriptPath);
        Utils::Fs::forceRemove(scriptPath + "c");
    }
}

QPointer<SearchPluginManager> SearchPluginManager::m_instance = nullptr;

const QHash<QString, QString> SearchPluginManager::m_categoryNames {
    {"all", QT_TRANSLATE_NOOP("SearchEngine", "All categories")},
    {"movies", QT_TRANSLATE_NOOP("SearchEngine", "Movies")},
    {"tv", QT_TRANSLATE_NOOP("SearchEngine", "TV shows")},
    {"music", QT_TRANSLATE_NOOP("SearchEngine", "Music")},
    {"games", QT_TRANSLATE_NOOP("SearchEngine", "Games")},
    {"anime", QT_TRANSLATE_NOOP("SearchEngine", "Anime")},
    {"software", QT_TRANSLATE_NOOP("SearchEngine", "Software")},
    {"pictures", QT_TRANSLATE_NOOP("SearchEngine", "Pictures")},
    {"books", QT_TRANSLATE_NOOP("SearchEngine", "Books")}
};

SearchPluginManager::SearchPluginManager()
    : m_updateUrl(QString("http://searchplugins.qbittorrent.org/%1/engines/").arg(Utils::Misc::pythonVersion() >= 3 ? "nova3" : "nova"))
{
    Q_ASSERT(!m_instance); // only one instance is allowed
    m_instance = this;

    updateNova();
    update();
}

SearchPluginManager::~SearchPluginManager()
{
    qDeleteAll(m_plugins);
}

SearchPluginManager *SearchPluginManager::instance()
{
    return m_instance;
}

QStringList SearchPluginManager::allPlugins() const
{
    return m_plugins.keys();
}

QStringList SearchPluginManager::enabledPlugins() const
{
    QStringList plugins;
    foreach (const PluginInfo *plugin, m_plugins.values()) {
        if (plugin->enabled)
            plugins << plugin->name;
    }

    return plugins;
}

QStringList SearchPluginManager::supportedCategories() const
{
    QStringList result;
    foreach (const PluginInfo *plugin, m_plugins.values()) {
        if (plugin->enabled) {
            foreach (QString cat, plugin->supportedCategories) {
                if (!result.contains(cat))
                    result << cat;
            }
        }
    }

    return result;
}

PluginInfo *SearchPluginManager::pluginInfo(const QString &name) const
{
    return m_plugins.value(name);
}

void SearchPluginManager::enablePlugin(const QString &name, bool enabled)
{
    PluginInfo *plugin = m_plugins.value(name, 0);
    if (plugin) {
        plugin->enabled = enabled;
        // Save to Hard disk
        Preferences *const pref = Preferences::instance();
        QStringList disabledPlugins = pref->getSearchEngDisabled();
        if (enabled)
            disabledPlugins.removeAll(name);
        else if (!disabledPlugins.contains(name))
            disabledPlugins.append(name);
        pref->setSearchEngDisabled(disabledPlugins);

        emit pluginEnabled(name, enabled);
    }
}

// Updates shipped plugin
void SearchPluginManager::updatePlugin(const QString &name)
{
    installPlugin(QString("%1%2.py").arg(m_updateUrl, name));
}

// Install or update plugin from file or url
void SearchPluginManager::installPlugin(const QString &source)
{
    qDebug("Asked to install plugin at %s", qUtf8Printable(source));

    if (Utils::Misc::isUrl(source)) {
        using namespace Net;
        DownloadHandler *handler = DownloadManager::instance()->downloadUrl(source, true);
        connect(handler, static_cast<void (DownloadHandler::*)(const QString &, const QString &)>(&DownloadHandler::downloadFinished)
                , this, &SearchPluginManager::pluginDownloaded);
        connect(handler, &DownloadHandler::downloadFailed, this, &SearchPluginManager::pluginDownloadFailed);
    }
    else {
        QString path = source;
        if (path.startsWith("file:", Qt::CaseInsensitive))
            path = QUrl(path).toLocalFile();

        QString pluginName = Utils::Fs::fileName(path);
        pluginName.chop(pluginName.size() - pluginName.lastIndexOf("."));

        if (!path.endsWith(".py", Qt::CaseInsensitive))
            emit pluginInstallationFailed(pluginName, tr("Unknown search engine plugin file format."));
        else
            installPlugin_impl(pluginName, path);
    }
}

void SearchPluginManager::installPlugin_impl(const QString &name, const QString &path)
{
    PluginVersion newVersion = getPluginVersion(path);
    qDebug() << "Version to be installed:" << newVersion;

    PluginInfo *plugin = pluginInfo(name);
    if (plugin && !(plugin->version < newVersion)) {
        qDebug("Apparently update is not needed, we have a more recent version");
        emit pluginUpdateFailed(name, tr("A more recent version of this plugin is already installed."));
        return;
    }

    // Process with install
    QString destPath = pluginPath(name);
    bool updated = false;
    if (QFile::exists(destPath)) {
        // Backup in case install fails
        QFile::copy(destPath, destPath + ".bak");
        Utils::Fs::forceRemove(destPath);
        Utils::Fs::forceRemove(destPath + "c");
        updated = true;
    }
    // Copy the plugin
    QFile::copy(path, destPath);
    // Update supported plugins
    update();
    // Check if this was correctly installed
    if (!m_plugins.contains(name)) {
        // Remove broken file
        Utils::Fs::forceRemove(destPath);
        if (updated) {
            // restore backup
            QFile::copy(destPath + ".bak", destPath);
            Utils::Fs::forceRemove(destPath + ".bak");
            // Update supported plugins
            update();
            emit pluginUpdateFailed(name, tr("Plugin is not supported."));
        }
        else {
            emit pluginInstallationFailed(name, tr("Plugin is not supported."));
        }
    }
    else {
        // Install was successful, remove backup
        if (updated)
            Utils::Fs::forceRemove(destPath + ".bak");
    }
}

bool SearchPluginManager::uninstallPlugin(const QString &name)
{
    // remove it from hard drive
    QDir pluginsFolder(pluginsLocation());
    QStringList filters;
    filters << name + ".*";
    QStringList files = pluginsFolder.entryList(filters, QDir::Files, QDir::Unsorted);
    QString file;
    foreach (file, files)
        Utils::Fs::forceRemove(pluginsFolder.absoluteFilePath(file));
    // Remove it from supported engines
    delete m_plugins.take(name);

    emit pluginUninstalled(name);
    return true;
}

void SearchPluginManager::updateIconPath(PluginInfo * const plugin)
{
    if (!plugin) return;
    QString iconPath = QString("%1/%2.png").arg(pluginsLocation(), plugin->name);
    if (QFile::exists(iconPath)) {
        plugin->iconPath = iconPath;
    }
    else {
        iconPath = QString("%1/%2.ico").arg(pluginsLocation(), plugin->name);
        if (QFile::exists(iconPath))
            plugin->iconPath = iconPath;
    }
}

void SearchPluginManager::checkForUpdates()
{
    // Download version file from update server
    using namespace Net;
    DownloadHandler *handler = DownloadManager::instance()->downloadUrl(m_updateUrl + "versions.txt");
    connect(handler, static_cast<void (DownloadHandler::*)(const QString &, const QByteArray &)>(&DownloadHandler::downloadFinished)
            , this, &SearchPluginManager::versionInfoDownloaded);
    connect(handler, &DownloadHandler::downloadFailed, this, &SearchPluginManager::versionInfoDownloadFailed);
}

SearchDownloadHandler *SearchPluginManager::downloadTorrent(const QString &siteUrl, const QString &url)
{
    return new SearchDownloadHandler {siteUrl, url, this};
}

SearchHandler *SearchPluginManager::startSearch(const QString &pattern, const QString &category, const QStringList &usedPlugins)
{
    // No search pattern entered
    Q_ASSERT(!pattern.isEmpty());

    return new SearchHandler {pattern, category, usedPlugins, this};
}

QString SearchPluginManager::categoryFullName(const QString &categoryName)
{
    return tr(m_categoryNames.value(categoryName).toUtf8().constData());
}

QString SearchPluginManager::pluginFullName(const QString &pluginName)
{
    return pluginInfo(pluginName) ? pluginInfo(pluginName)->fullName : QString();
}

QString SearchPluginManager::pluginsLocation()
{
    return QString("%1/engines").arg(engineLocation());
}

QString SearchPluginManager::engineLocation()
{
    QString folder = "nova";
    if (Utils::Misc::pythonVersion() >= 3)
        folder = "nova3";
    const QString location = Utils::Fs::expandPathAbs(specialFolderLocation(SpecialFolder::Data) + folder);
    QDir locationDir(location);
    if (!locationDir.exists())
        locationDir.mkpath(locationDir.absolutePath());
    return location;
}

void SearchPluginManager::versionInfoDownloaded(const QString &url, const QByteArray &data)
{
    Q_UNUSED(url)
    parseVersionInfo(data);
}

void SearchPluginManager::versionInfoDownloadFailed(const QString &url, const QString &reason)
{
    Q_UNUSED(url)
    emit checkForUpdatesFailed(tr("Update server is temporarily unavailable. %1").arg(reason));
}

void SearchPluginManager::pluginDownloaded(const QString &url, QString filePath)
{
    filePath = Utils::Fs::fromNativePath(filePath);

    QString pluginName = Utils::Fs::fileName(url);
    pluginName.chop(pluginName.size() - pluginName.lastIndexOf(".")); // Remove extension
    installPlugin_impl(pluginName, filePath);
    Utils::Fs::forceRemove(filePath);
}

void SearchPluginManager::pluginDownloadFailed(const QString &url, const QString &reason)
{
    QString pluginName = url.split('/').last();
    pluginName.replace(".py", "", Qt::CaseInsensitive);
    if (pluginInfo(pluginName))
        emit pluginUpdateFailed(pluginName, tr("Failed to download the plugin file. %1").arg(reason));
    else
        emit pluginInstallationFailed(pluginName, tr("Failed to download the plugin file. %1").arg(reason));
}

// Update nova.py search plugin if necessary
void SearchPluginManager::updateNova()
{
    qDebug("Updating nova");

    // create nova directory if necessary
    QDir searchDir(engineLocation());
    QString novaFolder = Utils::Misc::pythonVersion() >= 3 ? "searchengine/nova3" : "searchengine/nova";
    QFile packageFile(searchDir.absoluteFilePath("__init__.py"));
    packageFile.open(QIODevice::WriteOnly | QIODevice::Text);
    packageFile.close();
    if (!searchDir.exists("engines"))
        searchDir.mkdir("engines");
    Utils::Fs::removeDirRecursive(searchDir.absoluteFilePath("__pycache__"));

    QFile packageFile2(searchDir.absolutePath() + "/engines/__init__.py");
    packageFile2.open(QIODevice::WriteOnly | QIODevice::Text);
    packageFile2.close();

    // Copy search plugin files (if necessary)
    QString filePath = searchDir.absoluteFilePath("nova2.py");
    if (getPluginVersion(":/" + novaFolder + "/nova2.py") > getPluginVersion(filePath)) {
        removePythonScriptIfExists(filePath);
        QFile::copy(":/" + novaFolder + "/nova2.py", filePath);
    }

    filePath = searchDir.absoluteFilePath("nova2dl.py");
    if (getPluginVersion(":/" + novaFolder + "/nova2dl.py") > getPluginVersion(filePath)) {
        removePythonScriptIfExists(filePath);
        QFile::copy(":/" + novaFolder + "/nova2dl.py", filePath);
    }

    filePath = searchDir.absoluteFilePath("fix_encoding.py");
    QFile::copy(":/" + novaFolder + "/fix_encoding.py", filePath);

    filePath = searchDir.absoluteFilePath("novaprinter.py");
    if (getPluginVersion(":/" + novaFolder + "/novaprinter.py") > getPluginVersion(filePath)) {
        removePythonScriptIfExists(filePath);
        QFile::copy(":/" + novaFolder + "/novaprinter.py", filePath);
    }

    filePath = searchDir.absoluteFilePath("helpers.py");
    if (getPluginVersion(":/" + novaFolder + "/helpers.py") > getPluginVersion(filePath)) {
        removePythonScriptIfExists(filePath);
        QFile::copy(":/" + novaFolder + "/helpers.py", filePath);
    }

    filePath = searchDir.absoluteFilePath("socks.py");
    removePythonScriptIfExists(filePath);
    QFile::copy(":/" + novaFolder + "/socks.py", filePath);

    if (novaFolder.endsWith("nova")) {
        filePath = searchDir.absoluteFilePath("fix_encoding.py");
        removePythonScriptIfExists(filePath);
        QFile::copy(":/" + novaFolder + "/fix_encoding.py", filePath);
    }
    else if (novaFolder.endsWith("nova3")) {
        filePath = searchDir.absoluteFilePath("sgmllib3.py");
        removePythonScriptIfExists(filePath);
        QFile::copy(":/" + novaFolder + "/sgmllib3.py", filePath);
    }

    QDir destDir(pluginsLocation());
    Utils::Fs::removeDirRecursive(destDir.absoluteFilePath("__pycache__"));
}

void SearchPluginManager::update()
{
    QProcess nova;
    nova.setEnvironment(QProcess::systemEnvironment());
    QStringList params;
    params << Utils::Fs::toNativePath(engineLocation() + "/nova2.py");
    params << "--capabilities";
    nova.start(Utils::Misc::pythonExecutable(), params, QIODevice::ReadOnly);
    nova.waitForStarted();
    nova.waitForFinished();

    QString capabilities = QString(nova.readAll());
    QDomDocument xmlDoc;
    if (!xmlDoc.setContent(capabilities)) {
        qWarning() << "Could not parse Nova search engine capabilities, msg: " << capabilities.toLocal8Bit().data();
        qWarning() << "Error: " << nova.readAllStandardError().constData();
        return;
    }

    QDomElement root = xmlDoc.documentElement();
    if (root.tagName() != "capabilities") {
        qWarning() << "Invalid XML file for Nova search engine capabilities, msg: " << capabilities.toLocal8Bit().data();
        return;
    }

    for (QDomNode engineNode = root.firstChild(); !engineNode.isNull(); engineNode = engineNode.nextSibling()) {
        QDomElement engineElem = engineNode.toElement();
        if (!engineElem.isNull()) {
            QString pluginName = engineElem.tagName();

            PluginInfo *plugin = new PluginInfo;
            plugin->name = pluginName;
            plugin->version = getPluginVersion(pluginPath(pluginName));
            plugin->fullName = engineElem.elementsByTagName("name").at(0).toElement().text();
            plugin->url = engineElem.elementsByTagName("url").at(0).toElement().text();

            foreach (QString cat, engineElem.elementsByTagName("categories").at(0).toElement().text().split(" ")) {
                cat = cat.trimmed();
                if (!cat.isEmpty())
                    plugin->supportedCategories << cat;
            }

            QStringList disabledEngines = Preferences::instance()->getSearchEngDisabled();
            plugin->enabled = !disabledEngines.contains(pluginName);

            updateIconPath(plugin);

            if (!m_plugins.contains(pluginName)) {
                m_plugins[pluginName] = plugin;
                emit pluginInstalled(pluginName);
            }
            else if (m_plugins[pluginName]->version != plugin->version) {
                delete m_plugins.take(pluginName);
                m_plugins[pluginName] = plugin;
                emit pluginUpdated(pluginName);
            }
        }
    }
}

void SearchPluginManager::parseVersionInfo(const QByteArray &info)
{
    qDebug("Checking if update is needed");

    QHash<QString, PluginVersion> updateInfo;
    bool dataCorrect = false;
    QList<QByteArray> lines = info.split('\n');
    foreach (QByteArray line, lines) {
        line = line.trimmed();
        if (line.isEmpty()) continue;
        if (line.startsWith("#")) continue;

        QList<QByteArray> list = line.split(' ');
        if (list.size() != 2) continue;

        QString pluginName = QString(list.first());
        if (!pluginName.endsWith(":")) continue;

        pluginName.chop(1); // remove trailing ':'
        PluginVersion version = PluginVersion::tryParse(list.last(), {});
        if (version == PluginVersion()) continue;

        dataCorrect = true;
        if (isUpdateNeeded(pluginName, version)) {
            qDebug("Plugin: %s is outdated", qUtf8Printable(pluginName));
            updateInfo[pluginName] = version;
        }
    }

    if (!dataCorrect)
        emit checkForUpdatesFailed(tr("An incorrect update info received."));
    else
        emit checkForUpdatesFinished(updateInfo);
}

bool SearchPluginManager::isUpdateNeeded(QString pluginName, PluginVersion newVersion) const
{
    PluginInfo *plugin = pluginInfo(pluginName);
    if (!plugin) return true;

    PluginVersion oldVersion = plugin->version;
    qDebug() << "IsUpdate needed? to be installed:" << newVersion << ", already installed:" << oldVersion;
    return (newVersion > oldVersion);
}

QString SearchPluginManager::pluginPath(const QString &name)
{
    return QString("%1/%2.py").arg(pluginsLocation(), name);
}

PluginVersion SearchPluginManager::getPluginVersion(QString filePath)
{
    QFile plugin(filePath);
    if (!plugin.exists()) {
        qDebug("%s plugin does not exist, returning 0.0", qUtf8Printable(filePath));
        return {};
    }

    if (!plugin.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    const PluginVersion invalidVersion;

    PluginVersion version;
    while (!plugin.atEnd()) {
        QByteArray line = plugin.readLine();
        if (line.startsWith("#VERSION: ")) {
            line = line.split(' ').last().trimmed();
            version = PluginVersion::tryParse(line, invalidVersion);
            if (version == invalidVersion) {
                LogMsg(tr("Search plugin '%1' contains invalid version string ('%2')")
                    .arg(Utils::Fs::fileName(filePath), QString::fromUtf8(line)), Log::MsgType::WARNING);
            }
            else {
                qDebug() << "plugin" << filePath << "version: " << version;
            }
            break;
        }
    }
    return version;
}
