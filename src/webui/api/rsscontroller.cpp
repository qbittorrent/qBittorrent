/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2018  Vladimir Golovnev <glassez@yandex.ru>
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

#include "rsscontroller.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QVector>

#include "base/rss/rss_article.h"
#include "base/rss/rss_autodownloader.h"
#include "base/rss/rss_autodownloadrule.h"
#include "base/rss/rss_feed.h"
#include "base/rss/rss_folder.h"
#include "base/rss/rss_session.h"
#include "base/utils/string.h"
#include "apierror.h"

using Utils::String::parseBool;

void RSSController::addFolderAction()
{
    requireParams({"path"});

    const QString path = params()["path"].trimmed();
    QString error;
    if (!RSS::Session::instance()->addFolder(path, &error))
        throw APIError(APIErrorType::Conflict, error);
}

void RSSController::addFeedAction()
{
    requireParams({"url", "path"});

    const QString url = params()["url"].trimmed();
    const QString path = params()["path"].trimmed();
    QString error;
    if (!RSS::Session::instance()->addFeed(url, (path.isEmpty() ? url : path), &error))
        throw APIError(APIErrorType::Conflict, error);
}

void RSSController::removeItemAction()
{
    requireParams({"path"});

    const QString path = params()["path"].trimmed();
    QString error;
    if (!RSS::Session::instance()->removeItem(path, &error))
        throw APIError(APIErrorType::Conflict, error);
}

void RSSController::moveItemAction()
{
    requireParams({"itemPath", "destPath"});

    const QString itemPath = params()["itemPath"].trimmed();
    const QString destPath = params()["destPath"].trimmed();
    QString error;
    if (!RSS::Session::instance()->moveItem(itemPath, destPath, &error))
        throw APIError(APIErrorType::Conflict, error);
}

void RSSController::itemsAction()
{
    const bool withData {parseBool(params()["withData"]).value_or(false)};

    const auto jsonVal = RSS::Session::instance()->rootFolder()->toJsonValue(withData);
    setResult(jsonVal.toObject());
}

void RSSController::markAsReadAction()
{
    requireParams({"itemPath"});

    const QString itemPath {params()["itemPath"]};
    const QString articleId {params()["articleId"]};

    RSS::Item *item = RSS::Session::instance()->itemByPath(itemPath);
    if (!item) return;

    if (!articleId.isNull())
    {
        RSS::Feed *feed = qobject_cast<RSS::Feed *>(item);
        if (feed)
        {
            RSS::Article *article = feed->articleByGUID(articleId);
            if (article)
                article->markAsRead();
        }
    }
    else
    {
        item->markAsRead();
    }
}

void RSSController::refreshItemAction()
{
    requireParams({"itemPath"});

    const QString itemPath {params()["itemPath"]};
    RSS::Item *item = RSS::Session::instance()->itemByPath(itemPath);
    if (item)
        item->refresh();
}

void RSSController::setRuleAction()
{
    requireParams({"ruleName", "ruleDef"});

    const QString ruleName {params()["ruleName"].trimmed()};
    const QByteArray ruleDef {params()["ruleDef"].trimmed().toUtf8()};

    const auto jsonObj = QJsonDocument::fromJson(ruleDef).object();
    RSS::AutoDownloader::instance()->insertRule(RSS::AutoDownloadRule::fromJsonObject(jsonObj, ruleName));
}

void RSSController::renameRuleAction()
{
    requireParams({"ruleName", "newRuleName"});

    const QString ruleName {params()["ruleName"].trimmed()};
    const QString newRuleName {params()["newRuleName"].trimmed()};

    RSS::AutoDownloader::instance()->renameRule(ruleName, newRuleName);
}

void RSSController::removeRuleAction()
{
    requireParams({"ruleName"});

    const QString ruleName {params()["ruleName"].trimmed()};
    RSS::AutoDownloader::instance()->removeRule(ruleName);
}

void RSSController::rulesAction()
{
    const QList<RSS::AutoDownloadRule> rules {RSS::AutoDownloader::instance()->rules()};
    QJsonObject jsonObj;
    for (const auto &rule : rules)
        jsonObj.insert(rule.name(), rule.toJsonObject());

    setResult(jsonObj);
}

void RSSController::matchingArticlesAction()
{
    requireParams({"ruleName"});

    const QString ruleName {params()["ruleName"]};
    const RSS::AutoDownloadRule rule = RSS::AutoDownloader::instance()->ruleByName(ruleName);

    QJsonObject jsonObj;
    for (const QString &feedURL : rule.feedURLs())
    {
        const RSS::Feed *feed = RSS::Session::instance()->feedByURL(feedURL);
        if (!feed) continue; // feed doesn't exist

        QJsonArray matchingArticles;
        for (const RSS::Article *article : feed->articles())
        {
            if (rule.matches(article->data()))
                matchingArticles << article->title();
        }
        if (!matchingArticles.isEmpty())
            jsonObj.insert(feed->name(), matchingArticles);
    }

    setResult(jsonObj);
}
