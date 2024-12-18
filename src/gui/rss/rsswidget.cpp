/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
 * Copyright (C) 2006  Arnaud Demaiziere <arnaud@qbittorrent.org>
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

#include "rsswidget.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QDragMoveEvent>
#include <QMenu>
#include <QMessageBox>
#include <QRegularExpression>
#include <QShortcut>
#include <QString>

#include "base/global.h"
#include "base/net/downloadmanager.h"
#include "base/preferences.h"
#include "base/rss/rss_article.h"
#include "base/rss/rss_feed.h"
#include "base/rss/rss_folder.h"
#include "base/rss/rss_session.h"
#include "gui/autoexpandabledialog.h"
#include "gui/interfaces/iguiapplication.h"
#include "gui/uithememanager.h"
#include "articlelistwidget.h"
#include "automatedrssdownloader.h"
#include "feedlistwidget.h"
#include "ui_rsswidget.h"

namespace
{
    void convertRelativeUrlToAbsolute(QString &html, const QString &baseUrl)
    {
        const QRegularExpression rx {uR"(((<a\s+[^>]*?href|<img\s+[^>]*?src)\s*=\s*["'])((https?|ftp):)?(\/\/[^\/]*)?(\/?[^\/"].*?)(["']))"_s
            , QRegularExpression::CaseInsensitiveOption};

        const QString normalizedBaseUrl = baseUrl.endsWith(u'/') ? baseUrl : (baseUrl + u'/');
        const QUrl url {normalizedBaseUrl};
        const QString defaultScheme = url.scheme();
        QRegularExpressionMatchIterator iter = rx.globalMatch(html);

        while (iter.hasNext())
        {
            const QRegularExpressionMatch match = iter.next();
            const QString scheme = match.captured(4);
            const QString host = match.captured(5);
            if (!scheme.isEmpty())
            {
                if (host.isEmpty())
                    break; // invalid URL, should never happen

                // already absolute URL
                continue;
            }

            QString relativePath = match.captured(6);
            if (relativePath.startsWith(u'/'))
                relativePath = relativePath.mid(1);

            const QString absoluteUrl = !host.isEmpty()
                    ? QString(defaultScheme + u':' + host) : (normalizedBaseUrl + relativePath);
            const QString fullMatch = match.captured(0);
            const QString prefix = match.captured(1);
            const QString suffix = match.captured(7);

            html.replace(fullMatch, (prefix + absoluteUrl + suffix));
        }
    }
}


RSSWidget::RSSWidget(IGUIApplication *app, QWidget *parent)
    : GUIApplicationComponent(app, parent)
    , m_ui {new Ui::RSSWidget}
{
    m_ui->setupUi(this);

    // Icons
    m_ui->actionCopyFeedURL->setIcon(UIThemeManager::instance()->getIcon(u"edit-copy"_s));
    m_ui->actionDelete->setIcon(UIThemeManager::instance()->getIcon(u"edit-clear"_s));
    m_ui->actionDownloadTorrent->setIcon(UIThemeManager::instance()->getIcon(u"downloading"_s, u"download"_s));
    m_ui->actionEditFeedURL->setIcon(UIThemeManager::instance()->getIcon(u"edit-rename"_s));
    m_ui->actionMarkItemsRead->setIcon(UIThemeManager::instance()->getIcon(u"task-complete"_s, u"mail-mark-read"_s));
    m_ui->actionNewFolder->setIcon(UIThemeManager::instance()->getIcon(u"folder-new"_s));
    m_ui->actionNewSubscription->setIcon(UIThemeManager::instance()->getIcon(u"list-add"_s));
    m_ui->actionOpenNewsURL->setIcon(UIThemeManager::instance()->getIcon(u"application-url"_s));
    m_ui->actionRename->setIcon(UIThemeManager::instance()->getIcon(u"edit-rename"_s));
    m_ui->actionUpdate->setIcon(UIThemeManager::instance()->getIcon(u"view-refresh"_s));
    m_ui->actionUpdateAllFeeds->setIcon(UIThemeManager::instance()->getIcon(u"view-refresh"_s));
#ifndef Q_OS_MACOS
    m_ui->newFeedButton->setIcon(UIThemeManager::instance()->getIcon(u"list-add"_s));
    m_ui->markReadButton->setIcon(UIThemeManager::instance()->getIcon(u"task-complete"_s, u"mail-mark-read"_s));
    m_ui->updateAllButton->setIcon(UIThemeManager::instance()->getIcon(u"view-refresh"_s));
    m_ui->rssDownloaderBtn->setIcon(UIThemeManager::instance()->getIcon(u"downloading"_s, u"download"_s));
#endif

    connect(m_ui->articleListWidget, &ArticleListWidget::customContextMenuRequested, this, &RSSWidget::displayItemsListMenu);
    connect(m_ui->articleListWidget, &ArticleListWidget::currentItemChanged, this, &RSSWidget::handleCurrentArticleItemChanged);
    connect(m_ui->articleListWidget, &ArticleListWidget::itemDoubleClicked, this, &RSSWidget::downloadSelectedTorrents);

    connect(m_ui->feedListWidget, &QAbstractItemView::doubleClicked, this, &RSSWidget::renameSelectedRSSItem);
    connect(m_ui->feedListWidget, &QTreeWidget::currentItemChanged, this, &RSSWidget::handleCurrentFeedItemChanged);
    connect(m_ui->feedListWidget, &QWidget::customContextMenuRequested, this, &RSSWidget::displayRSSListMenu);
    loadFoldersOpenState();
    m_ui->feedListWidget->setCurrentItem(m_ui->feedListWidget->stickyUnreadItem());

    const auto *editHotkey = new QShortcut(Qt::Key_F2, m_ui->feedListWidget, nullptr, nullptr, Qt::WidgetShortcut);
    connect(editHotkey, &QShortcut::activated, this, &RSSWidget::renameSelectedRSSItem);
    const auto *deleteHotkey = new QShortcut(QKeySequence::Delete, m_ui->feedListWidget, nullptr, nullptr, Qt::WidgetShortcut);
    connect(deleteHotkey, &QShortcut::activated, this, &RSSWidget::deleteSelectedItems);

    // Feeds list actions
    connect(m_ui->actionDelete, &QAction::triggered, this, &RSSWidget::deleteSelectedItems);
    connect(m_ui->actionRename, &QAction::triggered, this, &RSSWidget::renameSelectedRSSItem);
    connect(m_ui->actionEditFeedURL, &QAction::triggered, this, &RSSWidget::editSelectedRSSFeedURL);
    connect(m_ui->actionUpdate, &QAction::triggered, this, &RSSWidget::refreshSelectedItems);
    connect(m_ui->actionNewFolder, &QAction::triggered, this, &RSSWidget::askNewFolder);
    connect(m_ui->actionNewSubscription, &QAction::triggered, this, &RSSWidget::on_newFeedButton_clicked);
    connect(m_ui->actionUpdateAllFeeds, &QAction::triggered, this, &RSSWidget::refreshAllFeeds);
    connect(m_ui->updateAllButton, &QAbstractButton::clicked, this, &RSSWidget::refreshAllFeeds);
    connect(m_ui->actionCopyFeedURL, &QAction::triggered, this, &RSSWidget::copySelectedFeedsURL);
    connect(m_ui->actionMarkItemsRead, &QAction::triggered, this, &RSSWidget::on_markReadButton_clicked);

    // News list actions
    connect(m_ui->actionOpenNewsURL, &QAction::triggered, this, &RSSWidget::openSelectedArticlesUrls);
    connect(m_ui->actionDownloadTorrent, &QAction::triggered, this, &RSSWidget::downloadSelectedTorrents);

    // Restore sliders position
    restoreSlidersPosition();
    // Bind saveSliders slots
    connect(m_ui->splitterMain, &QSplitter::splitterMoved, this, &RSSWidget::saveSlidersPosition);
    connect(m_ui->splitterSide, &QSplitter::splitterMoved, this, &RSSWidget::saveSlidersPosition);

    if (RSS::Session::instance()->isProcessingEnabled())
        m_ui->labelWarn->hide();
    connect(RSS::Session::instance(), &RSS::Session::processingStateChanged
            , this, &RSSWidget::handleSessionProcessingStateChanged);
    connect(RSS::Session::instance()->rootFolder(), &RSS::Folder::unreadCountChanged
            , this, &RSSWidget::handleUnreadCountChanged);

    m_ui->textBrowser->installEventFilter(this);
}

RSSWidget::~RSSWidget()
{
    // we need it here to properly mark latest article
    // as read without having additional code
    m_ui->articleListWidget->clear();

    saveFoldersOpenState();

    delete m_ui;
}

// display a right-click menu
void RSSWidget::displayRSSListMenu(const QPoint &pos)
{
    if (!m_ui->feedListWidget->indexAt(pos).isValid())
        // No item under the mouse, clear selection
        m_ui->feedListWidget->clearSelection();

    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    const QList<QTreeWidgetItem *> selectedItems = m_ui->feedListWidget->selectedItems();
    if (!selectedItems.isEmpty())
    {
        menu->addAction(m_ui->actionUpdate);
        menu->addAction(m_ui->actionMarkItemsRead);
        menu->addSeparator();

        if (selectedItems.size() == 1)
        {
            QTreeWidgetItem *selectedItem = selectedItems.first();
            if (selectedItem != m_ui->feedListWidget->stickyUnreadItem())
            {
                menu->addAction(m_ui->actionRename);
                if (m_ui->feedListWidget->isFeed(selectedItem))
                    menu->addAction(m_ui->actionEditFeedURL);
                menu->addAction(m_ui->actionDelete);
                menu->addSeparator();
                if (m_ui->feedListWidget->isFolder(selectedItem))
                    menu->addAction(m_ui->actionNewFolder);
            }
        }
        else
        {
            menu->addAction(m_ui->actionDelete);
            menu->addSeparator();
        }

        menu->addAction(m_ui->actionNewSubscription);

        if (m_ui->feedListWidget->isFeed(selectedItems.first()))
        {
            menu->addSeparator();
            menu->addAction(m_ui->actionCopyFeedURL);
        }
    }
    else
    {
        menu->addAction(m_ui->actionNewSubscription);
        menu->addAction(m_ui->actionNewFolder);
        menu->addSeparator();
        menu->addAction(m_ui->actionUpdateAllFeeds);
    }

    menu->popup(QCursor::pos());
}

void RSSWidget::displayItemsListMenu()
{
    bool hasTorrent = false;
    bool hasLink = false;
    for (const QListWidgetItem *item : asConst(m_ui->articleListWidget->selectedItems()))
    {
        auto *article = item->data(Qt::UserRole).value<RSS::Article *>();
        Q_ASSERT(article);

        if (!article->torrentUrl().isEmpty())
            hasTorrent = true;
        if (!article->link().isEmpty())
            hasLink = true;
        if (hasTorrent && hasLink)
            break;
    }

    QMenu *myItemListMenu = new QMenu(this);
    myItemListMenu->setAttribute(Qt::WA_DeleteOnClose);

    if (hasTorrent)
        myItemListMenu->addAction(m_ui->actionDownloadTorrent);
    if (hasLink)
        myItemListMenu->addAction(m_ui->actionOpenNewsURL);

    if (!myItemListMenu->isEmpty())
        myItemListMenu->popup(QCursor::pos());
}

void RSSWidget::askNewFolder()
{
    bool ok = false;
    QString newName = AutoExpandableDialog::getText(
                this, tr("Please choose a folder name"), tr("Folder name:"), QLineEdit::Normal
                , tr("New folder"), &ok);
    if (!ok) return;

    newName = newName.trimmed();
    if (newName.isEmpty()) return;

    // Determine destination folder for new item
    QTreeWidgetItem *destItem = nullptr;
    QList<QTreeWidgetItem *> selectedItems = m_ui->feedListWidget->selectedItems();
    if (!selectedItems.empty())
    {
        destItem = selectedItems.first();
        if (!m_ui->feedListWidget->isFolder(destItem))
            destItem = destItem->parent();
    }
    // Consider the case where the user clicked on Unread item
    RSS::Folder *rssDestFolder = ((!destItem || (destItem == m_ui->feedListWidget->stickyUnreadItem()))
                                  ? RSS::Session::instance()->rootFolder()
                                  : qobject_cast<RSS::Folder *>(m_ui->feedListWidget->getRSSItem(destItem)));

    const QString newFolderPath = RSS::Item::joinPath(rssDestFolder->path(), newName);
    const nonstd::expected<void, QString> result = RSS::Session::instance()->addFolder(newFolderPath);
    if (!result)
        QMessageBox::warning(this, u"qBittorrent"_s, result.error(), QMessageBox::Ok);

    // Expand destination folder to display new feed
    if (destItem && (destItem != m_ui->feedListWidget->stickyUnreadItem()))
        destItem->setExpanded(true);
    // As new RSS items are added synchronously, we can do the following here.
    m_ui->feedListWidget->setCurrentItem(m_ui->feedListWidget->mapRSSItem(RSS::Session::instance()->itemByPath(newFolderPath)));
}

// add a stream by a button
void RSSWidget::on_newFeedButton_clicked()
{
    // Ask for feed URL
    const QString clipText = qApp->clipboard()->text();
    const QString defaultURL = Net::DownloadManager::hasSupportedScheme(clipText) ? clipText : u"http://"_s;

    bool ok = false;
    QString newURL = AutoExpandableDialog::getText(
                this, tr("Please type a RSS feed URL"), tr("Feed URL:"), QLineEdit::Normal, defaultURL, &ok);
    if (!ok) return;

    newURL = newURL.trimmed();
    if (newURL.isEmpty()) return;

    // Determine destination folder for new item
    QTreeWidgetItem *destItem = nullptr;
    QList<QTreeWidgetItem *> selectedItems = m_ui->feedListWidget->selectedItems();
    if (!selectedItems.empty())
    {
        destItem = selectedItems.first();
        if (!m_ui->feedListWidget->isFolder(destItem))
            destItem = destItem->parent();
    }
    // Consider the case where the user clicked on Unread item
    RSS::Folder *rssDestFolder = ((!destItem || (destItem == m_ui->feedListWidget->stickyUnreadItem()))
                                  ? RSS::Session::instance()->rootFolder()
                                  : qobject_cast<RSS::Folder *>(m_ui->feedListWidget->getRSSItem(destItem)));

    // NOTE: We still add feed using legacy way (with URL as feed name)
    const QString newFeedPath = RSS::Item::joinPath(rssDestFolder->path(), newURL);
    const nonstd::expected<void, QString> result = RSS::Session::instance()->addFeed(newURL, newFeedPath);
    if (!result)
        QMessageBox::warning(this, u"qBittorrent"_s, result.error(), QMessageBox::Ok);

    // Expand destination folder to display new feed
    if (destItem && (destItem != m_ui->feedListWidget->stickyUnreadItem()))
        destItem->setExpanded(true);
    // As new RSS items are added synchronously, we can do the following here.
    m_ui->feedListWidget->setCurrentItem(m_ui->feedListWidget->mapRSSItem(RSS::Session::instance()->itemByPath(newFeedPath)));
}

void RSSWidget::deleteSelectedItems()
{
    const QList<QTreeWidgetItem *> selectedItems = m_ui->feedListWidget->selectedItems();
    if (selectedItems.isEmpty())
        return;
    if ((selectedItems.size() == 1) && (selectedItems.first() == m_ui->feedListWidget->stickyUnreadItem()))
        return;

    QMessageBox::StandardButton answer = QMessageBox::question(
                this, tr("Deletion confirmation"), tr("Are you sure you want to delete the selected RSS feeds?")
                , QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer == QMessageBox::No)
        return;

    for (QTreeWidgetItem *item : selectedItems)
        if (item != m_ui->feedListWidget->stickyUnreadItem())
            RSS::Session::instance()->removeItem(m_ui->feedListWidget->itemPath(item));
}

void RSSWidget::loadFoldersOpenState()
{
    const QStringList openedFolders = Preferences::instance()->getRssOpenFolders();
    for (const QString &varPath : openedFolders)
    {
        QTreeWidgetItem *parent = nullptr;
        for (const QString &name : asConst(varPath.split(u'\\')))
        {
            int nbChildren = (parent ? parent->childCount() : m_ui->feedListWidget->topLevelItemCount());
            for (int i = 0; i < nbChildren; ++i)
            {
                QTreeWidgetItem *child = (parent ? parent->child(i) : m_ui->feedListWidget->topLevelItem(i));
                if (m_ui->feedListWidget->getRSSItem(child)->name() == name)
                {
                    parent = child;
                    parent->setExpanded(true);
                    break;
                }
            }
        }
    }
}

void RSSWidget::saveFoldersOpenState()
{
    QStringList openedFolders;
    for (QTreeWidgetItem *item : asConst(m_ui->feedListWidget->getAllOpenedFolders()))
        openedFolders << m_ui->feedListWidget->itemPath(item);
    Preferences::instance()->setRssOpenFolders(openedFolders);
}

void RSSWidget::refreshAllFeeds()
{
    RSS::Session::instance()->refresh();
}

void RSSWidget::downloadSelectedTorrents()
{
    for (QListWidgetItem *item : asConst(m_ui->articleListWidget->selectedItems()))
    {
        auto *article = item->data(Qt::UserRole).value<RSS::Article *>();
        Q_ASSERT(article);

        // Mark as read
        article->markAsRead();

        app()->addTorrentManager()->addTorrent(article->torrentUrl());
    }
}

// open the url of the selected RSS articles in the Web browser
void RSSWidget::openSelectedArticlesUrls()
{
    for (QListWidgetItem *item : asConst(m_ui->articleListWidget->selectedItems()))
    {
        auto *article = item->data(Qt::UserRole).value<RSS::Article *>();
        Q_ASSERT(article);

        // Mark as read
        article->markAsRead();

        if (!article->link().isEmpty())
            QDesktopServices::openUrl(QUrl(article->link()));
    }
}

void RSSWidget::renameSelectedRSSItem()
{
    QList<QTreeWidgetItem *> selectedItems = m_ui->feedListWidget->selectedItems();
    if (selectedItems.size() != 1) return;

    QTreeWidgetItem *item = selectedItems.first();
    if (item == m_ui->feedListWidget->stickyUnreadItem())
        return;

    RSS::Item *rssItem = m_ui->feedListWidget->getRSSItem(item);
    const QString parentPath = RSS::Item::parentPath(rssItem->path());
    bool ok = false;
    do
    {
        QString newName = AutoExpandableDialog::getText(
                    this, tr("Please choose a new name for this RSS feed"), tr("New feed name:")
                    , QLineEdit::Normal, rssItem->name(), &ok);
        // Check if name is already taken
        if (!ok) return;

        const nonstd::expected<void, QString> result = RSS::Session::instance()->moveItem(rssItem, RSS::Item::joinPath(parentPath, newName));
        if (!result)
        {
            QMessageBox::warning(nullptr, tr("Rename failed"), result.error());
            ok = false;
        }
    } while (!ok);
}

void RSSWidget::editSelectedRSSFeedURL()
{
    QList<QTreeWidgetItem *> selectedItems = m_ui->feedListWidget->selectedItems();
    if (selectedItems.size() != 1)
        return;

    QTreeWidgetItem *item = selectedItems.first();
    RSS::Feed *rssFeed = qobject_cast<RSS::Feed *>(m_ui->feedListWidget->getRSSItem(item));
    Q_ASSERT(rssFeed);
    if (!rssFeed) [[unlikely]]
        return;

    bool ok = false;
    QString newURL = AutoExpandableDialog::getText(this, tr("Please type a RSS feed URL")
            , tr("Feed URL:"), QLineEdit::Normal, rssFeed->url(), &ok).trimmed();
    if (!ok || newURL.isEmpty())
        return;

    const nonstd::expected<void, QString> result = RSS::Session::instance()->setFeedURL(rssFeed, newURL);
    if (!result)
        QMessageBox::warning(this, u"qBittorrent"_s, result.error(), QMessageBox::Ok);
}

void RSSWidget::refreshSelectedItems()
{
    for (QTreeWidgetItem *item : asConst(m_ui->feedListWidget->selectedItems()))
    {
        if (item == m_ui->feedListWidget->stickyUnreadItem())
        {
            refreshAllFeeds();
            return;
        }

        m_ui->feedListWidget->getRSSItem(item)->refresh();
    }
}

void RSSWidget::copySelectedFeedsURL()
{
    QStringList URLs;
    for (QTreeWidgetItem *item : asConst(m_ui->feedListWidget->selectedItems()))
    {
        if (auto *feed = qobject_cast<RSS::Feed *>(m_ui->feedListWidget->getRSSItem(item)))
            URLs << feed->url();
    }
    qApp->clipboard()->setText(URLs.join(u'\n'));
}

void RSSWidget::handleCurrentFeedItemChanged(QTreeWidgetItem *currentItem)
{
    m_ui->articleListWidget->setRSSItem(m_ui->feedListWidget->getRSSItem(currentItem)
                                    , (currentItem == m_ui->feedListWidget->stickyUnreadItem()));
}

void RSSWidget::on_markReadButton_clicked()
{
    for (QTreeWidgetItem *item : asConst(m_ui->feedListWidget->selectedItems()))
    {
        m_ui->feedListWidget->getRSSItem(item)->markAsRead();
        if (item == m_ui->feedListWidget->stickyUnreadItem())
            break; // all items was read
    }
}

// display a news
void RSSWidget::handleCurrentArticleItemChanged(QListWidgetItem *currentItem, QListWidgetItem *previousItem)
{
    m_ui->textBrowser->clear();

    if (previousItem)
    {
        auto *article = m_ui->articleListWidget->getRSSArticle(previousItem);
        Q_ASSERT(article);
        article->markAsRead();
    }

    if (!currentItem)
        return;

    auto *article = m_ui->articleListWidget->getRSSArticle(currentItem);
    renderArticle(article);
}

void RSSWidget::saveSlidersPosition()
{
    // Remember sliders positions
    Preferences *const pref = Preferences::instance();
    pref->setRssSideSplitterState(m_ui->splitterSide->saveState());
    pref->setRssMainSplitterState(m_ui->splitterMain->saveState());
}

void RSSWidget::restoreSlidersPosition()
{
    const Preferences *const pref = Preferences::instance();
    const QByteArray stateSide = pref->getRssSideSplitterState();
    if (!stateSide.isEmpty())
        m_ui->splitterSide->restoreState(stateSide);
    const QByteArray stateMain = pref->getRssMainSplitterState();
    if (!stateMain.isEmpty())
        m_ui->splitterMain->restoreState(stateMain);
}

void RSSWidget::updateRefreshInterval(int val) const
{
    RSS::Session::instance()->setRefreshInterval(val);
}

void RSSWidget::on_rssDownloaderBtn_clicked()
{
    auto *downloader = new AutomatedRssDownloader(this);
    downloader->setAttribute(Qt::WA_DeleteOnClose);
    downloader->open();
}

void RSSWidget::handleSessionProcessingStateChanged(bool enabled)
{
    m_ui->labelWarn->setVisible(!enabled);
}

void RSSWidget::handleUnreadCountChanged()
{
    emit unreadCountUpdated(RSS::Session::instance()->rootFolder()->unreadCount());
}

bool RSSWidget::eventFilter(QObject *obj, QEvent *event)
{
    if ((obj == m_ui->textBrowser) && (event->type() == QEvent::PaletteChange))
    {
        QListWidgetItem *currentItem = m_ui->articleListWidget->currentItem();
        if (currentItem)
        {
            const RSS::Article *article = m_ui->articleListWidget->getRSSArticle(currentItem);
            renderArticle(article);
        }
    }

    return false;
}

void RSSWidget::renderArticle(const RSS::Article *article) const
{
    Q_ASSERT(article);

    const QString highlightedBaseColor = m_ui->textBrowser->palette().color(QPalette::Active, QPalette::Highlight).name();
    const QString highlightedBaseTextColor = m_ui->textBrowser->palette().color(QPalette::Active, QPalette::HighlightedText).name();
    const QString alternateBaseColor = m_ui->textBrowser->palette().color(QPalette::Active, QPalette::AlternateBase).name();

    QString html =
        u"<div style='border: 2px solid red; margin-left: 5px; margin-right: 5px; margin-bottom: 5px;'>" +
        u"<div style='background-color: \"%1\"; font-weight: bold; color: \"%2\";'>%3</div>"_s.arg(highlightedBaseColor, highlightedBaseTextColor, article->title());
    if (article->date().isValid())
        html += u"<div style='background-color: \"%1\";'><b>%2</b>%3</div>"_s.arg(alternateBaseColor, tr("Date: "), QLocale::system().toString(article->date().toLocalTime()));
    if (m_ui->feedListWidget->currentItem() == m_ui->feedListWidget->stickyUnreadItem())
        html += u"<div style='background-color: \"%1\";'><b>%2</b>%3</div>"_s.arg(alternateBaseColor, tr("Feed: "), article->feed()->title());
    if (!article->author().isEmpty())
        html += u"<div style='background-color: \"%1\";'><b>%2</b>%3</div>"_s.arg(alternateBaseColor, tr("Author: "), article->author());
    html += u"</div>"
            u"<div style='margin-left: 5px; margin-right: 5px;'>";
    if (Qt::mightBeRichText(article->description()))
    {
        html += article->description();
    }
    else
    {
        QString description = article->description();
        QRegularExpression rx;
        // If description is plain text, replace BBCode tags with HTML and wrap everything in <pre></pre> so it looks nice
        rx.setPatternOptions(QRegularExpression::InvertedGreedinessOption
                             | QRegularExpression::CaseInsensitiveOption);

        rx.setPattern(u"\\[img\\](.+)\\[/img\\]"_s);
        description = description.replace(rx, u"<img src=\"\\1\">"_s);

        rx.setPattern(u"\\[url=(\")?(.+)\\1\\]"_s);
        description = description.replace(rx, u"<a href=\"\\2\">"_s);
        description = description.replace(u"[/url]"_s, u"</a>"_s, Qt::CaseInsensitive);

        rx.setPattern(u"\\[(/)?([bius])\\]"_s);
        description = description.replace(rx, u"<\\1\\2>"_s);

        rx.setPattern(u"\\[color=(\")?(.+)\\1\\]"_s);
        description = description.replace(rx, u"<span style=\"color:\\2\">"_s);
        description = description.replace(u"[/color]"_s, u"</span>"_s, Qt::CaseInsensitive);

        rx.setPattern(u"\\[size=(\")?(.+)\\d\\1\\]"_s);
        description = description.replace(rx, u"<span style=\"font-size:\\2px\">"_s);
        description = description.replace(u"[/size]"_s, u"</span>"_s, Qt::CaseInsensitive);

        html += u"<pre>" + description + u"</pre>";
    }

    // Supplement relative URLs to absolute ones
    const QUrl url {article->link()};
    const QString baseUrl = url.toString(QUrl::RemovePath | QUrl::RemoveQuery);
    convertRelativeUrlToAbsolute(html, baseUrl);
    html += u"</div>";
    m_ui->textBrowser->setHtml(html);
}
