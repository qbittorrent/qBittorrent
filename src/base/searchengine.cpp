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

#include <QDomDocument>
#include <QDomNode>
#include <QDomElement>
#include <QDir>
#include <QProcess>
#include <QDebug>

#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "base/preferences.h"
#include "base/net/downloadmanager.h"
#include "base/net/downloadhandler.h"
#include "searchengine.h"

enum SearchResultColumn
{
    PL_DL_LINK,
    PL_NAME,
    PL_SIZE,
    PL_SEEDS,
    PL_LEECHS,
    PL_ENGINE_URL,
    PL_DESC_LINK,
    NB_PLUGIN_COLUMNS
};

static inline void removePythonScriptIfExists(const QString &scriptPath)
{
    Utils::Fs::forceRemove(scriptPath);
    Utils::Fs::forceRemove(scriptPath + "c");
}

const QHash<QString, QString> SearchEngine::m_categoryNames = SearchEngine::initializeCategoryNames();

SearchEngine::SearchEngine()
    : m_updateUrl(QString("https://raw.github.com/qbittorrent/qBittorrent/master/src/searchengine/%1/engines/").arg(Utils::Misc::pythonVersion() >= 3 ? "nova3" : "nova"))
    , m_searchStopped(false)
{
    updateNova();

    m_searchProcess = new QProcess(this);
    m_searchProcess->setEnvironment(QProcess::systemEnvironment());
    connect(m_searchProcess, SIGNAL(started()), this, SIGNAL(searchStarted()));
    connect(m_searchProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(readSearchOutput()));
    connect(m_searchProcess, SIGNAL(finished(int)), this, SLOT(processFinished(int)));

    m_searchTimeout = new QTimer(this);
    m_searchTimeout->setSingleShot(true);
    connect(m_searchTimeout, SIGNAL(timeout()), this, SLOT(onTimeout()));

    update();
}

SearchEngine::~SearchEngine()
{
    qDeleteAll(m_plugins.values());
    cancelSearch();
}

QStringList SearchEngine::allPlugins() const
{
    return m_plugins.keys();
}

QStringList SearchEngine::enabledPlugins() const
{
    QStringList plugins;
    foreach (const PluginInfo *plugin, m_plugins.values()) {
        if (plugin->enabled)
            plugins << plugin->name;
    }

    return plugins;
}

QStringList SearchEngine::supportedCategories() const
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

PluginInfo *SearchEngine::pluginInfo(const QString &name) const
{
    return m_plugins.value(name, 0);
}

bool SearchEngine::isActive() const
{
    return (m_searchProcess->state() != QProcess::NotRunning);
}

void SearchEngine::enablePlugin(const QString &name, bool enabled)
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
    }
}

// Updates shipped plugin
void SearchEngine::updatePlugin(const QString &name)
{
    installPlugin(QString("%1%2.py").arg(m_updateUrl).arg(name));
}

// Install or update plugin from file or url
void SearchEngine::installPlugin(const QString &source)
{
    qDebug("Asked to install plugin at %s", qPrintable(source));

    if (Utils::Misc::isUrl(source)) {
        Net::DownloadHandler *handler = Net::DownloadManager::instance()->downloadUrl(source, true);
        connect(handler, SIGNAL(downloadFinished(QString, QString)), this, SLOT(pluginDownloaded(QString, QString)));
        connect(handler, SIGNAL(downloadFailed(QString, QString)), this, SLOT(pluginDownloadFailed(QString, QString)));
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

void SearchEngine::installPlugin_impl(const QString &name, const QString &path)
{
    qreal newVersion = getPluginVersion(path);
    qDebug("Version to be installed: %.2f", newVersion);

    PluginInfo *plugin = pluginInfo(name);
    if (plugin && (plugin->version >= newVersion)) {
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

bool SearchEngine::uninstallPlugin(const QString &name)
{
    if (QFile::exists(":/nova/engines/" + name + ".py"))
        return false;

    // Proceed with uninstall
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

    return true;
}

void SearchEngine::checkForUpdates()
{
    // Download version file from update server on sourceforge
    Net::DownloadHandler *handler = Net::DownloadManager::instance()->downloadUrl(m_updateUrl + "versions.txt");
    connect(handler, SIGNAL(downloadFinished(QString, QByteArray)), this, SLOT(versionInfoDownloaded(QString, QByteArray)));
    connect(handler, SIGNAL(downloadFailed(QString, QString)), this, SLOT(versionInfoDownloadFailed(QString, QString)));
}

void SearchEngine::cancelSearch()
{
    if (m_searchProcess->state() != QProcess::NotRunning) {
#ifdef Q_OS_WIN
        m_searchProcess->kill();
#else
        m_searchProcess->terminate();
#endif
        m_searchStopped = true;
        m_searchTimeout->stop();

        m_searchProcess->waitForFinished(1000);
    }
}

void SearchEngine::startSearch(const QString &pattern, const QString &category, const QStringList &usedPlugins)
{
    // Search process already running or
    // No search pattern entered
    if ((m_searchProcess->state() != QProcess::NotRunning) || pattern.isEmpty()) {
        emit searchFailed();
        return;
    }

    // Reload environment variables (proxy)
    m_searchProcess->setEnvironment(QProcess::systemEnvironment());

    QStringList params;
    m_searchStopped = false;
    params << Utils::Fs::toNativePath(engineLocation() + "/nova2.py");
    params << usedPlugins.join(",");
    params << category;
    params << pattern.split(" ");

    // Launch search
    m_searchProcess->start(Utils::Misc::pythonExecutable(), params, QIODevice::ReadOnly);
    m_searchTimeout->start(180000); // 3min
}

QString SearchEngine::categoryFullName(const QString &categoryName)
{
    return tr(m_categoryNames.value(categoryName).toUtf8().constData());
}

QString SearchEngine::pluginsLocation()
{
    return QString("%1/engines").arg(engineLocation());
}

QString SearchEngine::engineLocation()
{
    QString folder = "nova";
    if (Utils::Misc::pythonVersion() >= 3)
        folder = "nova3";
    const QString location = Utils::Fs::expandPathAbs(Utils::Fs::QDesktopServicesDataLocation() + folder);
    QDir locationDir(location);
    if (!locationDir.exists())
        locationDir.mkpath(locationDir.absolutePath());
    return location;
}

// Slot called when QProcess is Finished
// QProcess can be finished for 3 reasons :
// Error | Stopped by user | Finished normally
void SearchEngine::processFinished(int exitcode)
{
    m_searchTimeout->stop();

    if (exitcode == 0)
        emit searchFinished(m_searchStopped);
    else
        emit searchFailed();
}

void SearchEngine::versionInfoDownloaded(const QString &url, const QByteArray &data)
{
    Q_UNUSED(url)
    parseVersionInfo(data);
}

void SearchEngine::versionInfoDownloadFailed(const QString &url, const QString &reason)
{
    Q_UNUSED(url)
    emit checkForUpdatesFailed(tr("Update server is temporarily unavailable. %1").arg(reason));
}

void SearchEngine::pluginDownloaded(const QString &url, QString filePath)
{
    filePath = Utils::Fs::fromNativePath(filePath);

    QString pluginName = Utils::Fs::fileName(url);
    pluginName.chop(pluginName.size() - pluginName.lastIndexOf(".")); // Remove extension
    installPlugin_impl(pluginName, filePath);
    Utils::Fs::forceRemove(filePath);
}

void SearchEngine::pluginDownloadFailed(const QString &url, const QString &reason)
{
    QString pluginName = url.split('/').last();
    pluginName.replace(".py", "", Qt::CaseInsensitive);
    if (pluginInfo(pluginName))
        emit pluginUpdateFailed(pluginName, tr("Failed to download the plugin file. %1").arg(reason));
    else
        emit pluginInstallationFailed(pluginName, tr("Failed to download the plugin file. %1").arg(reason));
}

// Update nova.py search plugin if necessary
void SearchEngine::updateNova()
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
    QDir shippedSubdir(":/" + novaFolder + "/engines/");
    QStringList files = shippedSubdir.entryList();
    foreach (const QString &file, files) {
        QString shippedFile = shippedSubdir.absoluteFilePath(file);
        // Copy python classes
        if (file.endsWith(".py")) {
            const QString destFile = destDir.absoluteFilePath(file);
            if (getPluginVersion(shippedFile) > getPluginVersion(destFile) ) {
                qDebug("shipped %s is more recent then local plugin, updating...", qPrintable(file));
                removePythonScriptIfExists(destFile);
                qDebug("%s copied to %s", qPrintable(shippedFile), qPrintable(destFile));
                QFile::copy(shippedFile, destFile);
            }
        }
        else {
            // Copy icons
            if (file.endsWith(".png"))
                if (!QFile::exists(destDir.absoluteFilePath(file)))
                    QFile::copy(shippedFile, destDir.absoluteFilePath(file));
        }
    }
}

void SearchEngine::onTimeout()
{
    cancelSearch();
}

// search QProcess return output as soon as it gets new
// stuff to read. We split it into lines and parse each
// line to SearchResult calling parseSearchResult().
void SearchEngine::readSearchOutput()
{
    QByteArray output = m_searchProcess->readAllStandardOutput();
    output.replace("\r", "");
    QList<QByteArray> lines = output.split('\n');
    if (!m_searchResultLineTruncated.isEmpty())
        lines.prepend(m_searchResultLineTruncated + lines.takeFirst());
    m_searchResultLineTruncated = lines.takeLast().trimmed();

    QList<SearchResult> searchResultList;
    foreach (const QByteArray &line, lines) {
        SearchResult searchResult;
        if (parseSearchResult(QString::fromUtf8(line), searchResult))
            searchResultList << searchResult;
    }

    if (!searchResultList.isEmpty())
        emit newSearchResults(searchResultList);
}

void SearchEngine::update()
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

            // Handle icon
            QString iconPath = QString("%1/%2.png").arg(pluginsLocation()).arg(pluginName);
            if (QFile::exists(iconPath)) {
                plugin->iconPath = iconPath;
            }
            else {
                iconPath = QString("%1/%2.ico").arg(pluginsLocation()).arg(pluginName);
                if (QFile::exists(iconPath))
                    plugin->iconPath = iconPath;
            }

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

// Parse one line of search results list
// Line is in the following form:
// file url | file name | file size | nb seeds | nb leechers | Search engine url
bool SearchEngine::parseSearchResult(const QString &line, SearchResult &searchResult)
{
    const QStringList parts = line.split("|");
    const int nbFields = parts.size();
    if (nbFields < (NB_PLUGIN_COLUMNS - 1)) return false; // -1 because desc_link is optional

    searchResult = SearchResult();
    searchResult.fileUrl = parts.at(PL_DL_LINK).trimmed(); // download URL
    searchResult.fileName = parts.at(PL_NAME).trimmed(); // Name
    searchResult.fileSize = parts.at(PL_SIZE).trimmed().toLongLong(); // Size
    bool ok = false;
    searchResult.nbSeeders = parts.at(PL_SEEDS).trimmed().toLongLong(&ok); // Seeders
    if (!ok || (searchResult.nbSeeders < 0))
        searchResult.nbSeeders = -1;
    searchResult.nbLeechers = parts.at(PL_LEECHS).trimmed().toLongLong(&ok); // Leechers
    if (!ok || (searchResult.nbLeechers < 0))
        searchResult.nbLeechers = -1;
    searchResult.siteUrl = parts.at(PL_ENGINE_URL).trimmed(); // Search site URL
    if (nbFields == NB_PLUGIN_COLUMNS)
        searchResult.descrLink = parts.at(PL_DESC_LINK).trimmed(); // Description Link

    return true;
}

void SearchEngine::parseVersionInfo(const QByteArray &info)
{
    qDebug("Checking if update is needed");

    QHash<QString, qreal> updateInfo;
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
        bool ok;
        qreal version = list.last().toFloat(&ok);
        qDebug("read line %s: %.2f", qPrintable(pluginName), version);
        if (!ok) continue;

        dataCorrect = true;
        if (isUpdateNeeded(pluginName, version)) {
            qDebug("Plugin: %s is outdated", qPrintable(pluginName));
            updateInfo[pluginName] = version;
        }
    }

    if (!dataCorrect)
        emit checkForUpdatesFailed(tr("An incorrect update info received."));
    else
        emit checkForUpdatesFinished(updateInfo);
}

bool SearchEngine::isUpdateNeeded(QString pluginName, qreal newVersion) const
{
    PluginInfo *plugin = pluginInfo(pluginName);
    if (!plugin) return true;

    qreal oldVersion = plugin->version;
    qDebug("IsUpdate needed? to be installed: %.2f, already installed: %.2f", newVersion, oldVersion);
    return (newVersion > oldVersion);
}

QString SearchEngine::pluginPath(const QString &name)
{
    return QString("%1/%2.py").arg(pluginsLocation()).arg(name);
}

QHash<QString, QString> SearchEngine::initializeCategoryNames()
{
    QHash<QString, QString> result;

    result["all"] = QT_TRANSLATE_NOOP("SearchEngine", "All categories");
    result["movies"] = QT_TRANSLATE_NOOP("SearchEngine", "Movies");
    result["tv"] = QT_TRANSLATE_NOOP("SearchEngine", "TV shows");
    result["music"] = QT_TRANSLATE_NOOP("SearchEngine", "Music");
    result["games"] = QT_TRANSLATE_NOOP("SearchEngine", "Games");
    result["anime"] = QT_TRANSLATE_NOOP("SearchEngine", "Anime");
    result["software"] = QT_TRANSLATE_NOOP("SearchEngine", "Software");
    result["pictures"] = QT_TRANSLATE_NOOP("SearchEngine", "Pictures");
    result["books"] = QT_TRANSLATE_NOOP("SearchEngine", "Books");

    return result;
}

qreal SearchEngine::getPluginVersion(QString filePath)
{
    QFile plugin(filePath);
    if (!plugin.exists()) {
        qDebug("%s plugin does not exist, returning 0.0", qPrintable(filePath));
        return 0.0;
    }

    if (!plugin.open(QIODevice::ReadOnly | QIODevice::Text))
        return 0.0;

    qreal version = 0.0;
    while (!plugin.atEnd()) {
        QByteArray line = plugin.readLine();
        if (line.startsWith("#VERSION: ")) {
            line = line.split(' ').last().trimmed();
            version = line.toFloat();
            qDebug("plugin %s version: %.2f", qPrintable(filePath), version);
            break;
        }
    }

    return version;
}
