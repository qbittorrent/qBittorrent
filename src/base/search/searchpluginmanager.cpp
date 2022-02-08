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

#include <memory>

#include <QDir>
#include <QDirIterator>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <QPointer>
#include <QProcess>

#include "base/global.h"
#include "base/logger.h"
#include "base/net/downloadmanager.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/utils/bytearray.h"
#include "base/utils/foreignapps.h"
#include "base/utils/fs.h"
#include "searchdownloadhandler.h"
#include "searchhandler.h"

namespace
{
    void clearPythonCache(const Path &path)
    {
        // remove python cache artifacts in `path` and subdirs

        PathList dirs = {path};
        QDirIterator iter {path.data(), (QDir::AllDirs | QDir::NoDotAndDotDot), QDirIterator::Subdirectories};
        while (iter.hasNext())
            dirs += Path(iter.next());

        for (const Path &dir : asConst(dirs))
        {
            // python 3: remove "__pycache__" folders
            if (dir.filename() == QLatin1String("__pycache__"))
            {
                Utils::Fs::removeDirRecursively(dir);
                continue;
            }

            // python 2: remove "*.pyc" files
            const QStringList files = QDir(dir.data()).entryList(QDir::Files);
            for (const QString &file : files)
            {
                const Path path {file};
                if (path.hasExtension(QLatin1String(".pyc")))
                    Utils::Fs::removeFile(path);
            }
        }
    }
}

QPointer<SearchPluginManager> SearchPluginManager::m_instance = nullptr;

SearchPluginManager::SearchPluginManager()
    : m_updateUrl(QLatin1String("http://searchplugins.qbittorrent.org/nova3/engines/"))
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
    if (!m_instance)
        m_instance = new SearchPluginManager;
    return m_instance;
}

void SearchPluginManager::freeInstance()
{
    delete m_instance;
}

QStringList SearchPluginManager::allPlugins() const
{
    return m_plugins.keys();
}

QStringList SearchPluginManager::enabledPlugins() const
{
    QStringList plugins;
    for (const PluginInfo *plugin : asConst(m_plugins))
    {
        if (plugin->enabled)
            plugins << plugin->name;
    }

    return plugins;
}

QStringList SearchPluginManager::supportedCategories() const
{
    QStringList result;
    for (const PluginInfo *plugin : asConst(m_plugins))
    {
        if (plugin->enabled)
        {
            for (const QString &cat : plugin->supportedCategories)
            {
                if (!result.contains(cat))
                    result << cat;
            }
        }
    }

    return result;
}

QStringList SearchPluginManager::getPluginCategories(const QString &pluginName) const
{
    QStringList plugins;
    if (pluginName == "all")
        plugins = allPlugins();
    else if ((pluginName == "enabled") || (pluginName == "multi"))
        plugins = enabledPlugins();
    else
        plugins << pluginName.trimmed();

    QSet<QString> categories;
    for (const QString &name : asConst(plugins))
    {
        const PluginInfo *plugin = pluginInfo(name);
        if (!plugin) continue; // plugin wasn't found
        for (const QString &category : plugin->supportedCategories)
            categories << category;
    }

    return categories.values();
}

PluginInfo *SearchPluginManager::pluginInfo(const QString &name) const
{
    return m_plugins.value(name);
}

void SearchPluginManager::enablePlugin(const QString &name, const bool enabled)
{
    PluginInfo *plugin = m_plugins.value(name, nullptr);
    if (plugin)
    {
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
    installPlugin(QString::fromLatin1("%1%2.py").arg(m_updateUrl, name));
}

// Install or update plugin from file or url
void SearchPluginManager::installPlugin(const QString &source)
{
    clearPythonCache(engineLocation());

    if (Net::DownloadManager::hasSupportedScheme(source))
    {
        using namespace Net;
        DownloadManager::instance()->download(DownloadRequest(source).saveToFile(true)
                                              , this, &SearchPluginManager::pluginDownloadFinished);
    }
    else
    {
        const Path path {source.startsWith("file:", Qt::CaseInsensitive) ? QUrl(source).toLocalFile() : source};

        QString pluginName = path.filename();
        if (pluginName.endsWith(".py", Qt::CaseInsensitive))
        {
            pluginName.chop(pluginName.size() - pluginName.lastIndexOf('.'));
            installPlugin_impl(pluginName, path);
        }
        else
        {
            emit pluginInstallationFailed(pluginName, tr("Unknown search engine plugin file format."));
        }
    }
}

void SearchPluginManager::installPlugin_impl(const QString &name, const Path &path)
{
    const PluginVersion newVersion = getPluginVersion(path);
    const PluginInfo *plugin = pluginInfo(name);
    if (plugin && !(plugin->version < newVersion))
    {
        LogMsg(tr("Plugin already at version %1, which is greater than %2").arg(plugin->version, newVersion), Log::INFO);
        emit pluginUpdateFailed(name, tr("A more recent version of this plugin is already installed."));
        return;
    }

    // Process with install
    const Path destPath = pluginPath(name);
    const Path backupPath = destPath + ".bak";
    bool updated = false;
    if (destPath.exists())
    {
        // Backup in case install fails
        Utils::Fs::copyFile(destPath, backupPath);
        Utils::Fs::removeFile(destPath);
        updated = true;
    }
    // Copy the plugin
    Utils::Fs::copyFile(path, destPath);
    // Update supported plugins
    update();
    // Check if this was correctly installed
    if (!m_plugins.contains(name))
    {
        // Remove broken file
        Utils::Fs::removeFile(destPath);
        LogMsg(tr("Plugin %1 is not supported.").arg(name), Log::INFO);
        if (updated)
        {
            // restore backup
            Utils::Fs::copyFile(backupPath, destPath);
            Utils::Fs::removeFile(backupPath);
            // Update supported plugins
            update();
            emit pluginUpdateFailed(name, tr("Plugin is not supported."));
        }
        else
        {
            emit pluginInstallationFailed(name, tr("Plugin is not supported."));
        }
    }
    else
    {
        // Install was successful, remove backup
        if (updated)
        {
            LogMsg(tr("Plugin %1 has been successfully updated.").arg(name), Log::INFO);
            Utils::Fs::removeFile(backupPath);
        }
    }
}

bool SearchPluginManager::uninstallPlugin(const QString &name)
{
    clearPythonCache(engineLocation());

    // remove it from hard drive
    const Path pluginsPath = pluginsLocation();
    const QStringList filters {name + QLatin1String(".*")};
    const QStringList files = QDir(pluginsPath.data()).entryList(filters, QDir::Files, QDir::Unsorted);
    for (const QString &file : files)
        Utils::Fs::removeFile(pluginsPath / Path(file));
    // Remove it from supported engines
    delete m_plugins.take(name);

    emit pluginUninstalled(name);
    return true;
}

void SearchPluginManager::updateIconPath(PluginInfo *const plugin)
{
    if (!plugin) return;

    const Path pluginsPath = pluginsLocation();
    Path iconPath = pluginsPath / Path(plugin->name + QLatin1String(".png"));
    if (iconPath.exists())
    {
        plugin->iconPath = iconPath;
    }
    else
    {
        iconPath = pluginsPath / Path(plugin->name + QLatin1String(".ico"));
        if (iconPath.exists())
            plugin->iconPath = iconPath;
    }
}

void SearchPluginManager::checkForUpdates()
{
    // Download version file from update server
    using namespace Net;
    DownloadManager::instance()->download({m_updateUrl + "versions.txt"}
                                          , this, &SearchPluginManager::versionInfoDownloadFinished);
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
    const QHash<QString, QString> categoryTable
    {
        {"all", tr("All categories")},
        {"movies", tr("Movies")},
        {"tv", tr("TV shows")},
        {"music", tr("Music")},
        {"games", tr("Games")},
        {"anime", tr("Anime")},
        {"software", tr("Software")},
        {"pictures", tr("Pictures")},
        {"books", tr("Books")}
    };
    return categoryTable.value(categoryName);
}

QString SearchPluginManager::pluginFullName(const QString &pluginName)
{
    return pluginInfo(pluginName) ? pluginInfo(pluginName)->fullName : QString();
}

Path SearchPluginManager::pluginsLocation()
{
    return (engineLocation() / Path("engines"));
}

Path SearchPluginManager::engineLocation()
{
    static Path location;
    if (location.isEmpty())
    {
        location = specialFolderLocation(SpecialFolder::Data) / Path("nova3");
        Utils::Fs::mkpath(location);
    }

    return location;
}

void SearchPluginManager::versionInfoDownloadFinished(const Net::DownloadResult &result)
{
    if (result.status == Net::DownloadStatus::Success)
        parseVersionInfo(result.data);
    else
        emit checkForUpdatesFailed(tr("Update server is temporarily unavailable. %1").arg(result.errorString));
}

void SearchPluginManager::pluginDownloadFinished(const Net::DownloadResult &result)
{
    if (result.status == Net::DownloadStatus::Success)
    {
        const Path filePath = result.filePath;

        Path pluginPath {QUrl(result.url).path()};
        pluginPath.removeExtension(); // Remove extension
        installPlugin_impl(pluginPath.filename(), filePath);
        Utils::Fs::removeFile(filePath);
    }
    else
    {
        const QString url = result.url;
        QString pluginName = url.mid(url.lastIndexOf('/') + 1);
        pluginName.replace(".py", "", Qt::CaseInsensitive);

        if (pluginInfo(pluginName))
            emit pluginUpdateFailed(pluginName, tr("Failed to download the plugin file. %1").arg(result.errorString));
        else
            emit pluginInstallationFailed(pluginName, tr("Failed to download the plugin file. %1").arg(result.errorString));
    }
}

// Update nova.py search plugin if necessary
void SearchPluginManager::updateNova()
{
    // create nova directory if necessary
    const Path enginePath = engineLocation();

    QFile packageFile {(enginePath / Path("__init__.py")).data()};
    packageFile.open(QIODevice::WriteOnly);
    packageFile.close();

    Utils::Fs::mkdir(enginePath / Path("engines"));

    QFile packageFile2 {(enginePath / Path("engines/__init__.py")).data()};
    packageFile2.open(QIODevice::WriteOnly);
    packageFile2.close();

    // Copy search plugin files (if necessary)
    const auto updateFile = [&enginePath](const Path &filename, const bool compareVersion)
    {
        const Path filePathBundled = Path(":/searchengine/nova3") / filename;
        const Path filePathDisk = enginePath / filename;

        if (compareVersion && (getPluginVersion(filePathBundled) <= getPluginVersion(filePathDisk)))
            return;

        Utils::Fs::removeFile(filePathDisk);
        Utils::Fs::copyFile(filePathBundled, filePathDisk);
    };

    updateFile(Path("helpers.py"), true);
    updateFile(Path("nova2.py"), true);
    updateFile(Path("nova2dl.py"), true);
    updateFile(Path("novaprinter.py"), true);
    updateFile(Path("sgmllib3.py"), false);
    updateFile(Path("socks.py"), false);
}

void SearchPluginManager::update()
{
    QProcess nova;
    nova.setProcessEnvironment(QProcessEnvironment::systemEnvironment());

    const QStringList params {(engineLocation() / Path("/nova2.py")).toString(), QLatin1String("--capabilities")};
    nova.start(Utils::ForeignApps::pythonInfo().executableName, params, QIODevice::ReadOnly);
    nova.waitForFinished();

    const QString capabilities = nova.readAll();
    QDomDocument xmlDoc;
    if (!xmlDoc.setContent(capabilities))
    {
        qWarning() << "Could not parse Nova search engine capabilities, msg: " << capabilities.toLocal8Bit().data();
        qWarning() << "Error: " << nova.readAllStandardError().constData();
        return;
    }

    const QDomElement root = xmlDoc.documentElement();
    if (root.tagName() != "capabilities")
    {
        qWarning() << "Invalid XML file for Nova search engine capabilities, msg: " << capabilities.toLocal8Bit().data();
        return;
    }

    for (QDomNode engineNode = root.firstChild(); !engineNode.isNull(); engineNode = engineNode.nextSibling())
    {
        const QDomElement engineElem = engineNode.toElement();
        if (!engineElem.isNull())
        {
            const QString pluginName = engineElem.tagName();

            auto plugin = std::make_unique<PluginInfo>();
            plugin->name = pluginName;
            plugin->version = getPluginVersion(pluginPath(pluginName));
            plugin->fullName = engineElem.elementsByTagName("name").at(0).toElement().text();
            plugin->url = engineElem.elementsByTagName("url").at(0).toElement().text();

            const QStringList categories = engineElem.elementsByTagName("categories").at(0).toElement().text().split(' ');
            for (QString cat : categories)
            {
                cat = cat.trimmed();
                if (!cat.isEmpty())
                    plugin->supportedCategories << cat;
            }

            const QStringList disabledEngines = Preferences::instance()->getSearchEngDisabled();
            plugin->enabled = !disabledEngines.contains(pluginName);

            updateIconPath(plugin.get());

            if (!m_plugins.contains(pluginName))
            {
                m_plugins[pluginName] = plugin.release();
                emit pluginInstalled(pluginName);
            }
            else if (m_plugins[pluginName]->version != plugin->version)
            {
                delete m_plugins.take(pluginName);
                m_plugins[pluginName] = plugin.release();
                emit pluginUpdated(pluginName);
            }
        }
    }
}

void SearchPluginManager::parseVersionInfo(const QByteArray &info)
{
    QHash<QString, PluginVersion> updateInfo;
    int numCorrectData = 0;

    const QVector<QByteArray> lines = Utils::ByteArray::splitToViews(info, "\n", Qt::SkipEmptyParts);
    for (QByteArray line : lines)
    {
        line = line.trimmed();
        if (line.isEmpty()) continue;
        if (line.startsWith('#')) continue;

        const QVector<QByteArray> list = Utils::ByteArray::splitToViews(line, ":", Qt::SkipEmptyParts);
        if (list.size() != 2) continue;

        const QString pluginName = list.first().trimmed();
        const PluginVersion version = PluginVersion::tryParse(list.last().trimmed(), {});

        if (!version.isValid()) continue;

        ++numCorrectData;
        if (isUpdateNeeded(pluginName, version))
        {
            LogMsg(tr("Plugin \"%1\" is outdated, updating to version %2").arg(pluginName, version), Log::INFO);
            updateInfo[pluginName] = version;
        }
    }

    if (numCorrectData < lines.size())
    {
        emit checkForUpdatesFailed(tr("Incorrect update info received for %1 out of %2 plugins.")
            .arg(QString::number(lines.size() - numCorrectData), QString::number(lines.size())));
    }
    else
    {
        emit checkForUpdatesFinished(updateInfo);
    }
}

bool SearchPluginManager::isUpdateNeeded(const QString &pluginName, const PluginVersion newVersion) const
{
    const PluginInfo *plugin = pluginInfo(pluginName);
    if (!plugin) return true;

    PluginVersion oldVersion = plugin->version;
    return (newVersion > oldVersion);
}

Path SearchPluginManager::pluginPath(const QString &name)
{
    return (pluginsLocation() / Path(name + QLatin1String(".py")));
}

PluginVersion SearchPluginManager::getPluginVersion(const Path &filePath)
{
    QFile pluginFile {filePath.data()};
    if (!pluginFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    while (!pluginFile.atEnd())
    {
        const QString line = QString(pluginFile.readLine()).remove(' ');
        if (!line.startsWith("#VERSION:", Qt::CaseInsensitive)) continue;

        const QString versionStr = line.mid(9);
        const PluginVersion version = PluginVersion::tryParse(versionStr, {});
        if (version.isValid())
            return version;

        LogMsg(tr("Search plugin '%1' contains invalid version string ('%2')")
            .arg(filePath.filename(), versionStr), Log::MsgType::WARNING);
        break;
    }

    return {};
}
