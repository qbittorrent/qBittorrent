/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017-2024  Vladimir Golovnev <glassez@yandex.ru>
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

#include "articlelistwidget.h"

#include <QApplication>
#include <QListWidgetItem>

#include "base/global.h"
#include "base/rss/rss_article.h"
#include "base/rss/rss_item.h"
#include "gui/uithememanager.h"

ArticleListWidget::ArticleListWidget(QWidget *parent)
    : QListWidget(parent)
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    setSelectionMode(QAbstractItemView::ExtendedSelection);

    checkInvariant();

    connect(UIThemeManager::instance(), &UIThemeManager::themeChanged, this, [this]
    {
        for (int row = 0; row < count(); ++row)
            applyUITheme(item(row));
    });
}

RSS::Article *ArticleListWidget::getRSSArticle(QListWidgetItem *item) const
{
    Q_ASSERT(item);
    return item->data(Qt::UserRole).value<RSS::Article *>();
}

QListWidgetItem *ArticleListWidget::mapRSSArticle(RSS::Article *rssArticle) const
{
    return m_rssArticleToListItemMapping.value(rssArticle);
}

void ArticleListWidget::setRSSItem(RSS::Item *rssItem, bool unreadOnly)
{
    // Clear the list first
    clear();
    m_rssArticleToListItemMapping.clear();
    if (m_rssItem)
        m_rssItem->disconnect(this);

    m_unreadOnly = unreadOnly;
    m_rssItem = rssItem;
    if (m_rssItem)
    {
        connect(m_rssItem, &RSS::Item::newArticle, this, &ArticleListWidget::handleArticleAdded);
        connect(m_rssItem, &RSS::Item::articleRead, this, &ArticleListWidget::handleArticleRead);
        connect(m_rssItem, &RSS::Item::articleAboutToBeRemoved, this, &ArticleListWidget::handleArticleAboutToBeRemoved);

        for (auto *article : asConst(rssItem->articles()))
        {
            if (!(m_unreadOnly && article->isRead()))
            {
                auto *item = createItem(article);
                addItem(item);
                m_rssArticleToListItemMapping.insert(article, item);
            }
        }
    }

    checkInvariant();
}

void ArticleListWidget::handleArticleAdded(RSS::Article *rssArticle)
{
    if (!(m_unreadOnly && rssArticle->isRead()))
    {
        auto *item = createItem(rssArticle);
        insertItem(0, item);
        m_rssArticleToListItemMapping.insert(rssArticle, item);
    }

    checkInvariant();
}

void ArticleListWidget::handleArticleRead(RSS::Article *rssArticle)
{
    auto *item = mapRSSArticle(rssArticle);
    if (!item)
        return;

    applyUITheme(item);

    checkInvariant();
}

void ArticleListWidget::handleArticleAboutToBeRemoved(RSS::Article *rssArticle)
{
    delete m_rssArticleToListItemMapping.take(rssArticle);
    checkInvariant();
}

void ArticleListWidget::checkInvariant() const
{
    Q_ASSERT(count() == m_rssArticleToListItemMapping.count());
}

QListWidgetItem *ArticleListWidget::createItem(RSS::Article *article) const
{
    Q_ASSERT(article);
    auto *item = new QListWidgetItem;

    item->setData(Qt::DisplayRole, article->title());
    item->setData(Qt::UserRole, QVariant::fromValue(article));
    applyUITheme(item);

    return item;
}

void ArticleListWidget::applyUITheme(QListWidgetItem *item) const
{
    const bool isRead = getRSSArticle(item)->isRead();
    const auto *themeManager = UIThemeManager::instance();
    if (isRead)
    {
        const QColor color = themeManager->getColor(u"RSS.ReadArticle"_s);
        item->setData(Qt::ForegroundRole, (color.isValid() ? color : QApplication::palette().color(QPalette::Inactive, QPalette::WindowText)));
        item->setData(Qt::DecorationRole, themeManager->getIcon(u"rss_read_article"_s, u"sphere"_s));
    }
    else
    {
        const QColor color = themeManager->getColor(u"RSS.UnreadArticle"_s);
        item->setData(Qt::ForegroundRole, (color.isValid() ? color : QApplication::palette().color(QPalette::Active, QPalette::Link)));
        item->setData(Qt::DecorationRole, themeManager->getIcon(u"rss_unread_article"_s, u"sphere"_s));
    }
}
