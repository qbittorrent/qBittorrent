/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2024  Jonathan Ketchker
 * Copyright (C) 2017  Vladimir Golovnev <glassez@yandex.ru>
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

#include "rss_folder.h"

#include <algorithm>

#include <QJsonObject>
#include <QJsonValue>

#include "base/global.h"
#include "rss_article.h"

using namespace RSS;

Folder::Folder(const QString &path)
    : Item(path)
{
}

Folder::~Folder()
{
    emit aboutToBeDestroyed(this);

    for (auto *item : asConst(items()))
        delete item;
}

QList<Article *> Folder::articles() const
{
    QList<Article *> news;

    for (Item *item : asConst(items()))
    {
        int n = news.size();
        news << item->articles();
        std::inplace_merge(news.begin(), news.begin() + n, news.end()
                           , [](Article *a1, Article *a2)
        {
            return Article::articleDateRecentThan(a1, a2->date());
        });
    }
    return news;
}

int Folder::unreadCount() const
{
    const auto itemList = items();
    return std::accumulate(itemList.cbegin(), itemList.cend(), 0, [](const int acc, const Item *item)
    {
        return (acc + item->unreadCount());
    });
}

void Folder::markAsRead()
{
    for (Item *item : asConst(items()))
        item->markAsRead();
}

void Folder::refresh()
{
    for (Item *item : asConst(items()))
        item->refresh();
}

void Folder::updateFetchDelay()
{
    for (Item *item : asConst(items()))
        item->updateFetchDelay();
}

QList<Item *> Folder::items() const
{
    return m_items;
}

QJsonValue Folder::toJsonValue(bool withData) const
{
    QJsonObject jsonObj;
    for (Item *item : asConst(items()))
        jsonObj.insert(item->name(), item->toJsonValue(withData));

    return jsonObj;
}

void Folder::handleItemUnreadCountChanged()
{
    emit unreadCountChanged(this);
}

void Folder::cleanup()
{
    for (Item *item : asConst(items()))
        item->cleanup();
}

void Folder::addItem(Item *item)
{
    Q_ASSERT(item);
    Q_ASSERT(!m_items.contains(item));

    m_items.append(item);
    connect(item, &Item::newArticle, this, &Item::newArticle);
    connect(item, &Item::articleRead, this, &Item::articleRead);
    connect(item, &Item::articleAboutToBeRemoved, this, &Item::articleAboutToBeRemoved);
    connect(item, &Item::unreadCountChanged, this, &Folder::handleItemUnreadCountChanged);

    for (auto *article : asConst(item->articles()))
        emit newArticle(article);

    if (item->unreadCount() > 0)
        emit unreadCountChanged(this);
}

void Folder::removeItem(Item *item)
{
    Q_ASSERT(m_items.contains(item));

    for (auto *article : asConst(item->articles()))
        emit articleAboutToBeRemoved(article);

    item->disconnect(this);
    m_items.removeOne(item);
    if (item->unreadCount() > 0)
        emit unreadCountChanged(this);
}
