/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022  Prince Gupta <guptaprince8832@gmail.com>
 * Copyright (C) 2015-2022  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2010  Christophe Dumez <chris@qbittorrent.org>
 * Copyright (C) 2010  Arnaud Demaiziere <arnaud@qbittorrent.org>
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

#include "feed_serializer.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>

#include "base/logger.h"
#include "base/path.h"
#include "base/utils/io.h"
#include "rss_article.h"

const int ARTICLEDATALIST_TYPEID = qRegisterMetaType<QList<QVariantHash>>();

void RSS::Private::FeedSerializer::load(const Path &dataFileName, const QString &url)
{
    const auto readResult = Utils::IO::readFile(dataFileName, -1);
    if (!readResult)
    {
        if (readResult.error().status == Utils::IO::ReadError::NotExist)
        {
            emit loadingFinished({});
            return;
        }

        LogMsg(tr("Failed to read RSS session data. %1").arg(readResult.error().message), Log::WARNING);
        return;
    }

    emit loadingFinished(loadArticles(readResult.value(), url));
}

void RSS::Private::FeedSerializer::store(const Path &dataFileName, const QList<QVariantHash> &articlesData)
{
    QJsonArray arr;
    for (const QVariantHash &data : articlesData)
    {
        auto jsonObj = QJsonObject::fromVariantHash(data);
        // JSON object doesn't support DateTime so we need to convert it
        jsonObj[Article::KeyDate] = data[Article::KeyDate].toDateTime().toString(Qt::RFC2822Date);

        arr << jsonObj;
    }

    const nonstd::expected<void, QString> result = Utils::IO::saveToFile(dataFileName, QJsonDocument(arr).toJson(QJsonDocument::Compact));
    if (!result)
    {
       LogMsg(tr("Failed to save RSS feed in '%1', Reason: %2").arg(dataFileName.toString(), result.error())
              , Log::WARNING);
    }
}

QList<QVariantHash> RSS::Private::FeedSerializer::loadArticles(const QByteArray &data, const QString &url)
{
    QJsonParseError jsonError;
    const QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &jsonError);
    if (jsonError.error != QJsonParseError::NoError)
    {
        LogMsg(tr("Couldn't parse RSS Session data. Error: %1").arg(jsonError.errorString())
               , Log::WARNING);
        return {};
    }

    if (!jsonDoc.isArray())
    {
        LogMsg(tr("Couldn't load RSS Session data. Invalid data format."), Log::WARNING);
        return {};
    }

    QList<QVariantHash> result;
    const QJsonArray jsonArr = jsonDoc.array();
    result.reserve(jsonArr.size());
    for (int i = 0; i < jsonArr.size(); ++i)
    {
        const QJsonValue jsonVal = jsonArr[i];
        if (!jsonVal.isObject())
        {
            LogMsg(tr("Couldn't load RSS article '%1#%2'. Invalid data format.")
                   .arg(url, QString::number(i)), Log::WARNING);
            continue;
        }

        const auto jsonObj = jsonVal.toObject();
        auto varHash = jsonObj.toVariantHash();
        // JSON object store DateTime as string so we need to convert it
        varHash[Article::KeyDate] =
                QDateTime::fromString(jsonObj.value(Article::KeyDate).toString(), Qt::RFC2822Date);

        result.push_back(varHash);
    }

    std::sort(result.begin(), result.end(), [](const QVariantHash &left, const QVariantHash &right)
    {
        return (left.value(Article::KeyDate).toDateTime() > right.value(Article::KeyDate).toDateTime());
    });

    return result;
}
