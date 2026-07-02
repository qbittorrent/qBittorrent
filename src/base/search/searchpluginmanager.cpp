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

#include "searchpluginmanager.h"

#include <memory>

#include <QtLogging>
#include <QDir>
#include <QDirIterator>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <QFile>
#include <QPointer>
#include <QProcess>
#include <QUrl>

#include "base/global.h"
#include "base/logger.h"
#include "base/net/downloadmanager.h"
#include "base/net/proxyconfigurationmanager.h"
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
            if (dir.filename() == u"__pycache__")
            {
                Utils::Fs::removeDirRecursively(dir);
                continue;
            }

            // python 2: remove "*.pyc" files
            QDirIterator it {dir.data(), {u"*.pyc"_s}, QDir::Files};
            while (it.hasNext())
            {
                const QString filePath = it.next();
                Utils::Fs::removeFile(Path(filePath));
            }
        }
    }
}

QPointer<SearchPluginManager> SearchPluginManager::m_instance = nullptr;

SearchPluginManager::SearchPluginManager()
    : m_updateUrl(u"https://raw.githubusercontent.com/qbittorrent/search-plugins/refs/heads/master/nova3/engines/"_s)
    , m_proxyEnv {QProcessEnvironment::systemEnvironment()}
{
    Q_ASSERT(!m_instance); // only one instance is allowed
    m_instance = this;

    connect(Net::ProxyConfigurationManager::instance(), &Net::ProxyConfigurationManager::proxyConfigurationChanged
            , this, &SearchPluginManager::applyProxySettings);
    connect(Preferences::instance(), &Preferences::changed
            , this, &SearchPluginManager::applyProxySettings);
    applyProxySettings();

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
    for (const SearchPluginInfo *plugin : asConst(m_plugins))
    {
        if (plugin->enabled)
            plugins << plugin->name;
    }

    return plugins;
}

QStringList SearchPluginManager::supportedCategories() const
{
    QStringList result;
    for (const SearchPluginInfo *plugin : asConst(m_plugins))
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
    if (pluginName == u"all")
        plugins = allPlugins();
    else if ((pluginName == u"enabled") || (pluginName == u"multi"))
        plugins = enabledPlugins();
    else
        plugins << pluginName.trimmed();

    QSet<QString> categories;
    for (const QString &name : asConst(plugins))
    {
        const SearchPluginInfo *plugin = pluginInfo(name);
        if (!plugin) continue; // plugin wasn't found
        for (const QString &category : plugin->supportedCategories)
            categories << category;
    }

    return categories.values();
}

SearchPluginInfo *SearchPluginManager::pluginInfo(const QString &name) const
{
    return m_plugins.value(name);
}

QString SearchPluginManager::pluginNameBySiteURL(const QString &siteURL) const
{
    for (const SearchPluginInfo *plugin : asConst(m_plugins))
    {
        if (plugin->url == siteURL)
            return plugin->name;
    }

    return {};
}

void SearchPluginManager::enablePlugin(const QString &name, const bool enabled)
{
    SearchPluginInfo *plugin = m_plugins.value(name, nullptr);
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
    installPlugin(u"%1%2.py"_s.arg(m_updateUrl, name));
}

// Install or update plugin from file or url
void SearchPluginManager::installPlugin(const QString &source)
{
    clearPythonCache(engineLocation());

    if (Net::DownloadManager::hasSupportedScheme(source))
    {
        using namespace Net;
        DownloadManager::instance()->download(DownloadRequest(source).saveToFile(true)
                , Preferences::instance()->useProxyForGeneralPurposes()
                , this, &SearchPluginManager::pluginDownloadFinished);
    }
    else
    {
        const Path path {source.startsWith(u"file:", Qt::CaseInsensitive) ? QUrl(source).toLocalFile() : source};
        if (const QString pyExt = u".py"_s; path.hasExtension(pyExt))
            installPlugin_impl(path.removedExtension(pyExt).filename(), path);
        else
            emit pluginInstallationFailed(path.filename(), tr("Unknown search engine plugin file format."));
    }
}

void SearchPluginManager::installPlugin_impl(const QString &name, const Path &srcPath)
{
    const SearchPluginVersion incomingVersion = getPluginVersion(srcPath);
    const SearchPluginInfo *plugin = pluginInfo(name);
    if (plugin && (plugin->version >= incomingVersion))
    {
        LogMsg(tr("Same or newer version of search plugin is already installed. Plugin name: \"%1\". Current version: %2. Incoming version: %3")
            .arg(plugin->name, plugin->version.toString(), incomingVersion.toString()), Log::INFO);
        emit pluginUpdateFailed(name, tr("A more recent version of this plugin is already installed."));
        return;
    }

    // Proceed to install
    const Path destPath = pluginPath(name);
    const Path backupPath = destPath + u".bak";
    const bool hasExistingPlugin = destPath.exists();
    bool hasBackup = false;

    if (destPath != srcPath)
    {
        // Plugin is not at the dest path, otherwise there is nothing to do here

        // Backup in case install fails
        if (hasExistingPlugin)
        {
            hasBackup = Utils::Fs::copyFile(destPath, backupPath);
            Utils::Fs::removeFile(destPath);
        }

        // Copy the plugin to dest path
        if (!Utils::Fs::copyFile(srcPath, destPath))
        {
            // Roll back
            Utils::Fs::removeFile(destPath);
            if (hasBackup)
            {
                // restore backup
                if (Utils::Fs::copyFile(backupPath, destPath))
                    Utils::Fs::removeFile(backupPath);
                else
                    Utils::Fs::removeFile(destPath);
            }

            const QString errMsg = tr("Search plugin installation failed.");
            if (hasExistingPlugin)
                emit pluginUpdateFailed(name, errMsg);
            else
                emit pluginInstallationFailed(name, errMsg);

            return;
        }
    }

    // Update supported plugins
    update();

    // Check if it was correctly installed
    if (m_plugins.contains(name))
    {
        // installation successful
        LogMsg(tr("Search plugin has been updated. Plugin name: \"%1\". Version: %2.").arg(name, incomingVersion.toString()), Log::INFO);

        if (hasBackup)
            Utils::Fs::removeFile(backupPath);
    }
    else
    {
        LogMsg(tr("Search plugin installation failed. Plugin name: \"%1\"").arg(name), Log::INFO);

        // Roll back
        Utils::Fs::removeFile(destPath);
        if (hasBackup)
        {
            // restore backup
            if (Utils::Fs::copyFile(backupPath, destPath))
            {
                Utils::Fs::removeFile(backupPath);
                update();  // Update supported plugins
            }
            else
            {
                Utils::Fs::removeFile(destPath);
            }
        }

        const QString errMsg = tr("Plugin is not supported.");
        if (hasExistingPlugin)
            emit pluginUpdateFailed(name, errMsg);
        else
            emit pluginInstallationFailed(name, errMsg);
    }
}

bool SearchPluginManager::uninstallPlugin(const QString &name)
{
    clearPythonCache(engineLocation());

    // remove it from hard drive
    QDirIterator iter {pluginsLocation().data(), {name + u".*"}, QDir::Files};
    while (iter.hasNext())
    {
        const QString filePath = iter.next();
        Utils::Fs::removeFile(Path(filePath));
    }

    // Remove it from supported engines
    delete m_plugins.take(name);

    emit pluginUninstalled(name);
    return true;
}

void SearchPluginManager::updateIconPath(SearchPluginInfo *const plugin)
{
    if (!plugin) return;

    const Path pluginsPath = pluginsLocation();
    Path iconPath = pluginsPath / Path(plugin->name + u".png");
    if (iconPath.exists())
    {
        plugin->iconPath = iconPath;
    }
    else
    {
        iconPath = pluginsPath / Path(plugin->name + u".ico");
        if (iconPath.exists())
            plugin->iconPath = iconPath;
    }
}

void SearchPluginManager::checkForUpdates()
{
    // Download version file from update server
    using namespace Net;
    DownloadManager::instance()->download({m_updateUrl + u"versions.txt"}
            , Preferences::instance()->useProxyForGeneralPurposes()
            , this, &SearchPluginManager::versionInfoDownloadFinished);
}

SearchDownloadHandler *SearchPluginManager::downloadTorrent(const QString &pluginName, const QString &url)
{
    return new SearchDownloadHandler(pluginName, url, this);
}

SearchHandler *SearchPluginManager::startSearch(const QString &pattern, const QString &category, const QStringList &usedPlugins)
{
    // No search pattern entered
    Q_ASSERT(!pattern.isEmpty());

    return new SearchHandler(pattern, category, usedPlugins, this);
}

QProcessEnvironment SearchPluginManager::proxyEnvironment() const
{
    return m_proxyEnv;
}

QString SearchPluginManager::categoryFullName(const QString &categoryName)
{
    const QHash<QString, QString> categoryTable
    {
        {u"all"_s, tr("All categories")},
        {u"anime"_s, tr("Anime")},
        {u"books"_s, tr("Books")},
        {u"games"_s, tr("Games")},
        {u"movies"_s, tr("Movies")},
        {u"music"_s, tr("Music")},
        {u"pictures"_s, tr("Pictures")},
        {u"software"_s, tr("Software")},
        {u"tv"_s, tr("TV shows")}
    };
    return categoryTable.value(categoryName);
}

QString SearchPluginManager::pluginFullName(const QString &pluginName) const
{
    return pluginInfo(pluginName) ? pluginInfo(pluginName)->fullName : QString();
}

Path SearchPluginManager::pluginsLocation()
{
    return (engineLocation() / Path(u"engines"_s));
}

Path SearchPluginManager::engineLocation()
{
    static Path location;
    if (location.isEmpty())
    {
        location = specialFolderLocation(SpecialFolder::Data) / Path(u"nova3"_s);
        Utils::Fs::mkpath(location);
    }

    return location;
}

void SearchPluginManager::applyProxySettings()
{
    // for python `urllib`: https://docs.python.org/3/library/urllib.request.html#urllib.request.ProxyHandler
    const QString HTTP_PROXY = u"http_proxy"_s;
    const QString HTTPS_PROXY = u"https_proxy"_s;
    // for `helpers.setupSOCKSProxy()`: https://everything.curl.dev/usingcurl/proxies/socks.html
    const QString SOCKS_PROXY = u"qbt_socks_proxy"_s;

    if (!Preferences::instance()->useProxyForGeneralPurposes())
    {
        m_proxyEnv.remove(HTTP_PROXY);
        m_proxyEnv.remove(HTTPS_PROXY);
        m_proxyEnv.remove(SOCKS_PROXY);
        return;
    }

    const Net::ProxyConfiguration proxyConfig = Net::ProxyConfigurationManager::instance()->proxyConfiguration();
    switch (proxyConfig.type)
    {
    case Net::ProxyType::None:
        m_proxyEnv.remove(HTTP_PROXY);
        m_proxyEnv.remove(HTTPS_PROXY);
        m_proxyEnv.remove(SOCKS_PROXY);
        break;

    case Net::ProxyType::HTTP:
        {
            const QString credential = proxyConfig.authEnabled
                ? (proxyConfig.username + u':' + proxyConfig.password + u'@')
                : QString();
            const QString proxyURL = u"http://%1%2:%3"_s
                .arg(credential, proxyConfig.ip, QString::number(proxyConfig.port));

            m_proxyEnv.insert(HTTP_PROXY, proxyURL);
            m_proxyEnv.insert(HTTPS_PROXY, proxyURL);
            m_proxyEnv.remove(SOCKS_PROXY);
        }
        break;

    case Net::ProxyType::SOCKS5:
        {
            const QString scheme = proxyConfig.hostnameLookupEnabled ? u"socks5h"_s : u"socks5"_s;
            const QString credential = proxyConfig.authEnabled
                ? (proxyConfig.username + u':' + proxyConfig.password + u'@')
                : QString();
            const QString proxyURL = u"%1://%2%3:%4"_s
                .arg(scheme, credential, proxyConfig.ip, QString::number(proxyConfig.port));

            m_proxyEnv.remove(HTTP_PROXY);
            m_proxyEnv.remove(HTTPS_PROXY);
            m_proxyEnv.insert(SOCKS_PROXY, proxyURL);
        }
        break;

    case Net::ProxyType::SOCKS4:
        {
            const QString scheme = proxyConfig.hostnameLookupEnabled ? u"socks4a"_s : u"socks4"_s;
            const QString proxyURL = u"%1://%2:%3"_s
                .arg(scheme, proxyConfig.ip, QString::number(proxyConfig.port));

            m_proxyEnv.remove(HTTP_PROXY);
            m_proxyEnv.remove(HTTPS_PROXY);
            m_proxyEnv.insert(SOCKS_PROXY, proxyURL);
        }
        break;
    }
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

        const auto pluginPath = Path(QUrl(result.url).path()).removedExtension();
        installPlugin_impl(pluginPath.filename(), filePath);
        Utils::Fs::removeFile(filePath);
    }
    else
    {
        const QString &url = result.url;
        const QString pluginName = url.sliced(url.lastIndexOf(u'/') + 1)
            .replace(u".py"_s, u""_s, Qt::CaseInsensitive);

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

    QFile packageFile {(enginePath / Path(u"__init__.py"_s)).data()};
    if (packageFile.open(QIODevice::WriteOnly))
        packageFile.close();

    Utils::Fs::mkdir(enginePath / Path(u"engines"_s));

    QFile packageFile2 {(enginePath / Path(u"engines/__init__.py"_s)).data()};
    if (packageFile2.open(QIODevice::WriteOnly))
        packageFile2.close();

    // Copy search plugin files (if necessary)
    const auto updateFile = [&enginePath](const Path &filename)
    {
        const Path filePathBundled = Path(u":/searchengine/nova3"_s) / filename;
        const Path filePathDisk = enginePath / filename;

        if (getPluginVersion(filePathBundled) <= getPluginVersion(filePathDisk))
            return;

        Utils::Fs::removeFile(filePathDisk);
        Utils::Fs::copyFile(filePathBundled, filePathDisk);
    };

    updateFile(Path(u"helpers.py"_s));
    updateFile(Path(u"nova2.py"_s));
    updateFile(Path(u"nova2dl.py"_s));
    updateFile(Path(u"novaprinter.py"_s));
    updateFile(Path(u"socks.py"_s));
}

void SearchPluginManager::update()
{
    QProcess nova;
    nova.setProcessEnvironment(proxyEnvironment());
#ifdef Q_OS_UNIX
    nova.setUnixProcessParameters(QProcess::UnixProcessFlag::CloseFileDescriptors);
#endif

    const QStringList params
    {
        Utils::ForeignApps::PYTHON_ISOLATE_MODE_FLAG,
        Utils::ForeignApps::PYTHON_UTF8_MODE_FLAG,
        (engineLocation() / Path(u"nova2.py"_s)).toString(),
        u"--capabilities"_s
    };
    nova.start(Utils::ForeignApps::pythonInfo().executablePath.data(), params, QIODevice::ReadOnly);
    nova.waitForFinished();

    if (const auto errMsg = QString::fromUtf8(nova.readAllStandardError()).trimmed()
        ; !errMsg.isEmpty())
    {
        qWarning("%s", qUtf8Printable(errMsg));
        LogMsg(tr("Error occurred when fetching search engine capabilities. Error: \"%1\".").arg(errMsg), Log::WARNING);
    }

    const auto capabilities = QString::fromUtf8(nova.readAllStandardOutput());
    QDomDocument xmlDoc;
    if (!xmlDoc.setContent(capabilities))
    {
        qWarning() << "Could not parse Nova search engine capabilities, msg: " << capabilities.toLocal8Bit().data();
        qWarning() << "Error: " << nova.readAllStandardError().constData();
        return;
    }

    const QDomElement root = xmlDoc.documentElement();
    if (root.tagName() != u"capabilities")
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

            auto plugin = std::make_unique<SearchPluginInfo>();
            plugin->name = pluginName;
            plugin->version = getPluginVersion(pluginPath(pluginName));
            plugin->fullName = engineElem.elementsByTagName(u"name"_s).at(0).toElement().text();
            plugin->url = engineElem.elementsByTagName(u"url"_s).at(0).toElement().text();

            const QStringList categories = engineElem.elementsByTagName(u"categories"_s).at(0).toElement().text().split(u' ');
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
    QHash<QString, SearchPluginVersion> updateInfo;
    int numCorrectData = 0;
    int numInvalidData = 0;

    const QList<QByteArrayView> lines = Utils::ByteArray::splitToViews(info, "\n");
    for (QByteArrayView line : lines)
    {
        line = line.trimmed();

        if (line.isEmpty())
            continue;
        if (line.startsWith('#'))
            continue;

        const QList<QByteArrayView> list = Utils::ByteArray::splitToViews(line, ":");
        if (list.size() != 2)
        {
            ++numInvalidData;
            continue;
        }

        const auto version = SearchPluginVersion::fromString(QString::fromLatin1(list.last().trimmed()));
        if (!version.isValid())
        {
            ++numInvalidData;
            continue;
        }

        ++numCorrectData;

        const auto pluginName = QString::fromUtf8(list.first().trimmed());
        if (isUpdateNeeded(pluginName, version))
        {
            LogMsg(tr("Plugin \"%1\" is outdated, updating to version %2").arg(pluginName, version.toString()), Log::INFO);
            updateInfo[pluginName] = version;
        }
    }

    if (numInvalidData > 0)
    {
        emit checkForUpdatesFailed(tr("Incorrect update info received for %1 out of %2 plugins.")
            .arg(QString::number(numInvalidData), QString::number(numCorrectData + numInvalidData)));
    }
    else
    {
        emit checkForUpdatesFinished(updateInfo);
    }
}

bool SearchPluginManager::isUpdateNeeded(const QString &pluginName, const SearchPluginVersion &newVersion) const
{
    const SearchPluginInfo *plugin = pluginInfo(pluginName);
    if (!plugin) return true;

    SearchPluginVersion oldVersion = plugin->version;
    return (newVersion > oldVersion);
}

Path SearchPluginManager::pluginPath(const QString &name)
{
    return (pluginsLocation() / Path(name + u".py"));
}

SearchPluginVersion SearchPluginManager::getPluginVersion(const Path &filePath)
{
    const int lineMaxLength = 16;

    QFile pluginFile {filePath.data()};
    if (!pluginFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    while (!pluginFile.atEnd())
    {
        const auto line = QString::fromUtf8(pluginFile.readLine(lineMaxLength)).remove(u' ');
        if (!line.startsWith(u"#VERSION:", Qt::CaseInsensitive))
            continue;

        const QString versionStr = line.sliced(9);
        const auto version = SearchPluginVersion::fromString(versionStr);
        if (version.isValid())
            return version;

        LogMsg(tr("Search plugin '%1' contains invalid version string ('%2')")
            .arg(filePath.filename(), versionStr), Log::MsgType::WARNING);
        break;
    }

    return {};
}
