/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2022  Vladimir Golovnev <glassez@yandex.ru>
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

#include "searchengine.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPointer>

#include "base/global.h"
#include "base/logger.h"
#include "base/net/downloadmanager.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/utils/bytearray.h"
#include "base/utils/fs.h"
#include "base/utils/io.h"
#include "searchhandler.h"

const QString CONF_FILE_NAME = u"searchengine.json"_qs;

const QString INFO_ENABLED = u"enabled"_qs;
const QString INFO_OPTIONS = u"options"_qs;
const QString OPTION_APIKEY = u"api_key"_qs;
const QString OPTION_URL = u"url"_qs;

QPointer<SearchEngine> SearchEngine::m_instance = nullptr;

namespace
{
    IndexerOptions parseIndexerOptions(const QJsonObject &jsonObj)
    {
        IndexerOptions indexerOptions;
        indexerOptions.apiKey = jsonObj.value(OPTION_APIKEY).toString();
        indexerOptions.url = jsonObj.value(OPTION_URL).toString();

        return indexerOptions;
    }

    IndexerInfo parseIndexerInfo(const QJsonObject &jsonObj)
    {
        IndexerInfo indexerInfo;
        indexerInfo.options = parseIndexerOptions(jsonObj.value(INFO_OPTIONS).toObject());
        indexerInfo.enabled = jsonObj.value(INFO_ENABLED).toBool();

        return indexerInfo;
    }

    QJsonObject serializeIndexerOptions(const IndexerOptions &indexerOptions)
    {
        return {
            {OPTION_APIKEY, indexerOptions.apiKey},
            {OPTION_URL, indexerOptions.url}
        };
    }

    QJsonObject serializeIndexerInfo(const IndexerInfo &indexerInfo)
    {
        return {
            {INFO_OPTIONS, serializeIndexerOptions(indexerInfo.options)},
            {INFO_ENABLED, indexerInfo.enabled}
        };
    }

    bool operator !=(const IndexerOptions &lhs, const IndexerOptions &rhs)
    {
        return ((lhs.apiKey != rhs.apiKey) || (lhs.url != rhs.url));
    }
}

SearchEngine::SearchEngine()
{
    Q_ASSERT(!m_instance); // only one instance is allowed
    m_instance = this;

    load();
}

SearchEngine *SearchEngine::instance()
{
    if (!m_instance)
        m_instance = new SearchEngine;
    return m_instance;
}

void SearchEngine::freeInstance()
{
    delete m_instance;
}

void SearchEngine::load()
{
    QFile confFile {(specialFolderLocation(SpecialFolder::Config) / Path(CONF_FILE_NAME)).data()};
    if (!confFile.exists())
        return;

    if (!confFile.open(QFile::ReadOnly))
    {
        LogMsg(tr("Couldn't load Search Engine configuration from %1. Error: %2")
               .arg(confFile.fileName(), confFile.errorString()), Log::WARNING);
        return;
    }

    QJsonParseError jsonError;
    const QJsonDocument jsonDoc = QJsonDocument::fromJson(confFile.readAll(), &jsonError);
    if (jsonError.error != QJsonParseError::NoError)
    {
        LogMsg(tr("Couldn't parse  Search Engine configuration from %1. Error: %2")
               .arg(confFile.fileName(), jsonError.errorString()), Log::WARNING);
        return;
    }

    if (!jsonDoc.isObject())
    {
        LogMsg(tr("Couldn't load Search Engine configuration from %1. Invalid data format.")
               .arg(confFile.fileName()), Log::WARNING);
        return;
    }

    const QJsonObject jsonObj = jsonDoc.object();
    for (auto it = jsonObj.constBegin(); it != jsonObj.constEnd(); ++it)
    {
        const QString &indexerName = it.key();
        const IndexerInfo indexerInfo = parseIndexerInfo(it.value().toObject());
        m_indexers[indexerName] = indexerInfo;
    }
}

void SearchEngine::store() const
{
    QJsonObject jsonObj;
    for (auto it = m_indexers.cbegin(); it != m_indexers.cend(); ++it)
    {
        const QString &indexerName = it.key();
        const IndexerInfo &indexerInfo = it.value();
        jsonObj[indexerName] = serializeIndexerInfo(indexerInfo);
    }

    const QByteArray data = QJsonDocument(jsonObj).toJson();
    const Path filePath = specialFolderLocation(SpecialFolder::Config) / Path(CONF_FILE_NAME);
    const nonstd::expected<void, QString> result = Utils::IO::saveToFile(filePath, data);
    if (!result)
    {
        LogMsg(tr("Couldn't store Search Engine configuration to %1. Error: %2")
            .arg(filePath.toString(), result.error()), Log::WARNING);
    }
}

QHash<QString, IndexerInfo> SearchEngine::indexers() const
{
    return m_indexers;
}

QStringList SearchEngine::supportedCategories() const
{
    return {
        u"all"_qs,
        u"movies"_qs,
        u"tv"_qs,
        u"music"_qs,
        u"games"_qs,
        u"anime"_qs,
        u"software"_qs,
        u"books"_qs
    };
}

void SearchEngine::addIndexer(const QString &name, const IndexerOptions &options)
{
    if (m_indexers.contains(name))
        throw IndexerAlreadyExistsError(tr("Indexer '%1' is already registered").arg(name));

    m_indexers[name].options = options;
    emit indexerAdded(name);
    store();
}

void SearchEngine::setIndexerOptions(const QString &name, const IndexerOptions &options)
{
    const auto it = m_indexers.find(name);
    if (it == m_indexers.end())
        throw IndexerNotFoundError(tr("Indexer '%1' isn't registered").arg(name));

    IndexerOptions &indexerOptions = it.value().options;
    if (indexerOptions != options)
    {
        indexerOptions = options;
        emit indexerOptionsChanged(name);
        store();
    }
}

void SearchEngine::enableIndexer(const QString &name, const bool enabled)
{
    const auto it = m_indexers.find(name);
    if (it == m_indexers.end())
        throw IndexerNotFoundError(tr("Indexer '%1' isn't registered").arg(name));

    IndexerInfo &indexerInfo = it.value();
    if (indexerInfo.enabled != enabled)
    {
        indexerInfo.enabled = enabled;
        emit indexerStateChanged(name);
        store();
    }
}

void SearchEngine::removeIndexer(const QString &name)
{
    const auto it = m_indexers.find(name);
    if (it == m_indexers.end())
        throw IndexerNotFoundError(tr("Indexer '%1' isn't registered").arg(name));

    m_indexers.erase(it);
    emit indexerRemoved(name);
    store();
}

bool SearchEngine::hasEnabledIndexers() const
{
    return std::any_of(m_indexers.cbegin(), m_indexers.cend(), [](const IndexerInfo &indexerInfo)
    {
        return indexerInfo.enabled;
    });
}

SearchHandler *SearchEngine::startSearch(const QString &pattern, const QString &category)
{
    if (!hasEnabledIndexers())
        return nullptr;

    QHash<QString, IndexerOptions> enabledIndexers;
    for (auto it = m_indexers.cbegin(); it != m_indexers.cend(); ++it)
    {
        const QString &indexerName = it.key();
        const IndexerInfo &indexerInfo = it.value();
        if (indexerInfo.enabled)
            enabledIndexers[indexerName] = indexerInfo.options;
    }

    return new SearchHandler(this, pattern, category, enabledIndexers);
}

QString SearchEngine::categoryFullName(const QString &categoryName)
{
    const QHash<QString, QString> categoryTable {
        {u"all"_qs, tr("All categories")},
        {u"movies"_qs, tr("Movies")},
        {u"tv"_qs, tr("TV shows")},
        {u"music"_qs, tr("Music")},
        {u"games"_qs, tr("Games")},
        {u"anime"_qs, tr("Anime")},
        {u"software"_qs, tr("Software")},
        {u"books"_qs, tr("Books")}
    };
    return categoryTable.value(categoryName);
}
