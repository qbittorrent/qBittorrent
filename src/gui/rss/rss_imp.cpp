/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez, Arnaud Demaiziere
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
 *
 * Contact : chris@qbittorrent.org arnaud@qbittorrent.org
 */

#include <QDesktopServices>
#include <QMenu>
#include <QStandardItemModel>
#include <QMessageBox>
#include <QString>
#include <QClipboard>
#include <QDragMoveEvent>
#include <QDebug>

#include "rss_imp.h"
#include "rssfeedlistwidgetitem.h"
#include "base/bittorrent/session.h"
#include "base/net/downloadmanager.h"
#include "base/preferences.h"
#include "rsssettingsdlg.h"
#include "base/rss/rssmanager.h"
#include "base/rss/rssfolder.h"
#include "base/rss/rssarticle.h"
#include "base/rss/rssfeed.h"
#include "automatedrssdownloader.h"
#include "guiiconprovider.h"
#include "autoexpandabledialog.h"
#include "addnewtorrentdialog.h"

namespace Article
{
    enum ArticleRoles
    {
        TitleRole = Qt::DisplayRole,
        IconRole = Qt::DecorationRole,
        ColorRole = Qt::ForegroundRole,
        ArticleRole = Qt::UserRole
    };
}

// display a right-click menu
void RSSImp::displayRSSListMenu(const QPoint &pos)
{
    QMenu myRSSListMenu(this);
    myRSSListMenu.addAction(actionUpdate);
    myRSSListMenu.addSeparator();

    QModelIndex target = feedWidget->indexAt(pos);
    if (target.isValid()) {
        myRSSListMenu.addAction(actionMark_items_read);
        myRSSListMenu.addSeparator();

        QList<QTreeWidgetItem*> selectedItems = feedWidget->selectedItems();
        if (selectedItems.size() == 1) {
            if (!feedWidget->isRoot(selectedItems.first())) {
                myRSSListMenu.addAction(actionRename);
                myRSSListMenu.addAction(actionDelete);
                myRSSListMenu.addSeparator();
                if (feedWidget->isFolder(selectedItems.first()))
                    myRSSListMenu.addAction(actionNew_folder);
            }
        }
        else {
            myRSSListMenu.addAction(actionDelete);
            myRSSListMenu.addSeparator();
        }
        myRSSListMenu.addAction(actionNew_subscription);
        if (feedWidget->isFeed(selectedItems.first())) {
            myRSSListMenu.addSeparator();
            myRSSListMenu.addAction(actionCopy_feed_URL);
        }
    }
    else {
        myRSSListMenu.addAction(actionNew_subscription);
        myRSSListMenu.addAction(actionNew_folder);
    }

    myRSSListMenu.exec(QCursor::pos());
}

void RSSImp::displayItemsListMenu(const QPoint &)
{
    QMenu myItemListMenu(this);
    QList<QListWidgetItem*> selectedItems = listArticles->selectedItems();
    if (selectedItems.size() <= 0)
        return;

    bool hasTorrent = false;
    bool hasLink = false;
    foreach (const QListWidgetItem *item, selectedItems) {
        if (!item) continue;
        Rss::ArticlePtr article = item->data(Article::ArticleRole).value<Rss::ArticlePtr>();
        if (!article) continue;

        if (!article->torrentUrl().isEmpty())
            hasTorrent = true;
        if (!article->link().isEmpty())
            hasLink = true;
        if (hasTorrent && hasLink)
            break;
    }
    if (hasTorrent)
        myItemListMenu.addAction(actionDownload_torrent);
    if (hasLink)
        myItemListMenu.addAction(actionOpen_news_URL);
    if (hasTorrent || hasLink)
        myItemListMenu.exec(QCursor::pos());
}

void RSSImp::askNewFolder()
{
    bool ok;
    QString newName = AutoExpandableDialog::getText(this, tr("Please choose a folder name"), tr("Folder name:"), QLineEdit::Normal, tr("New folder"), &ok);
    if (!ok)
        return;

    FolderPtr newFolder(new Rss::Folder(newName));
    if (addItemToSelected(newFolder))
        m_rssManager->saveStreamList();
}

// add a stream by a button
void RSSImp::on_newFeedButton_clicked()
{
    // Ask for feed URL
    bool ok;
    QString clip_txt = qApp->clipboard()->text();
    QString default_url = "http://";
    if (clip_txt.startsWith("http://", Qt::CaseInsensitive) || clip_txt.startsWith("https://", Qt::CaseInsensitive) || clip_txt.startsWith("ftp://", Qt::CaseInsensitive))
        default_url = clip_txt;

    QString newUrl = AutoExpandableDialog::getText(this, tr("Please type a RSS stream URL"), tr("Stream URL:"), QLineEdit::Normal, default_url, &ok);
    if (!ok)
        return;

    newUrl = newUrl.trimmed();
    if (newUrl.isEmpty())
        return;

    Rss::FeedPtr stream(new Rss::Feed(newUrl, m_rssManager.data()));
    if (addItemToSelected(stream))
        m_rssManager->saveStreamList();
    else
        QMessageBox::warning(this, "qBittorrent",
                             tr("This RSS feed is already in the list."),
                             QMessageBox::Ok);
}

QStringList RSSImp::getOpenFolderPaths() const
{
    QStringList open;
    feedWidget->walkFolders([&open](RssFeedListWidgetItem *const item) {
        if (item->isExpanded())
            open << item->rssFile()->pathHierarchy().join("\\");
    });
    return open;
}

void RSSImp::loadFoldersOpenState()
{
    feedWidget->collapseAll();
    QStringList opened = Preferences::instance()->getRssOpenFolders();
    feedWidget->walkFolders([&opened](RssFeedListWidgetItem *item) {
        QString path = item->rssFile()->pathHierarchy().join("\\");
        if (opened.contains(path))
            item->setExpanded(true);
    });
}

void RSSImp::downloadSelectedTorrents()
{
    QList<QListWidgetItem *> selected_items = listArticles->selectedItems();
    if (selected_items.size() <= 0)
        return;
    foreach (QListWidgetItem *item, selected_items) {
        if (!item) continue;
        Rss::ArticlePtr article = item->data(Article::ArticleRole).value<Rss::ArticlePtr>();
        if (!article) continue;

        // Mark as read
        article->markAsRead();
        item->setData(Article::ColorRole, QVariant(QColor("grey")));
        item->setData(Article::IconRole, QVariant(QIcon(":/icons/sphere.png")));

        if (article->torrentUrl().isEmpty())
            continue;
        if (AddNewTorrentDialog::isEnabled())
            AddNewTorrentDialog::show(article->torrentUrl());
        else
            BitTorrent::Session::instance()->addTorrent(article->torrentUrl());
    }
}

// open the url of the selected RSS articles in the Web browser
void RSSImp::openSelectedArticlesUrls()
{
    QList<QListWidgetItem * > selected_items = listArticles->selectedItems();
    if (selected_items.size() <= 0)
        return;
    foreach (QListWidgetItem *item, selected_items) {
        if (!item) continue;
        Rss::ArticlePtr article = item->data(Article::ArticleRole).value<Rss::ArticlePtr>();
        if (!article) continue;

        // Mark as read
        article->markAsRead();
        item->setData(Article::ColorRole, QVariant(QColor("grey")));
        item->setData(Article::IconRole, QVariant(QIcon(":/icons/sphere.png")));

        const QString link = article->link();
        if (!link.isEmpty())
            QDesktopServices::openUrl(QUrl(link));
    }
}

void RSSImp::copySelectedFeedsURL()
{
    QStringList URLs;
    QList<RssFeedListWidgetItem*> selectedItems = feedWidget->selectedItemsNoRoot();
    for (auto item : selectedItems)
        if (feedWidget->isFeed(item))
            URLs << item->rssFile()->id();
    qApp->clipboard()->setText(URLs.join("\n"));
}

void RSSImp::on_markReadButton_clicked()
{
    QList<RssFeedListWidgetItem*> selectedItems = feedWidget->selectedItemsNoRoot();
    if (selectedItems.isEmpty()) {
        feedWidget->getRoot()->markAsRead();
    }
    else {
        for (auto item : selectedItems)
            item->rssFile()->markAsRead();
    }
    populateArticleList(feedWidget->currentItem());
}

QListWidgetItem *RSSImp::createArticleListItem(const Rss::ArticlePtr &article)
{
    Q_ASSERT(article);
    QListWidgetItem *item = new QListWidgetItem;

    item->setData(Article::TitleRole, article->title());
    item->setData(Article::ArticleRole, QVariant::fromValue(article));
    if (article->isRead()) {
        item->setData(Article::ColorRole, QVariant(QColor("grey")));
        item->setData(Article::IconRole, QVariant(QIcon(":/icons/sphere.png")));
    }
    else {
        item->setData(Article::ColorRole, QVariant(QColor("blue")));
        item->setData(Article::IconRole, QVariant(QIcon(":/icons/sphere2.png")));
    }

    return item;
}

// fills the newsList
void RSSImp::populateArticleList(QTreeWidgetItem *item)
{
    if (!item) {
        listArticles->clear();
        return;
    }

    Rss::FilePtr rssItem = RSS_WIDGET_ITEM(item)->rssFile();

    // Clear the list first
    textBrowser->clear();
    m_currentArticle = 0;
    listArticles->clear();

    qDebug("Getting the list of news");
    Rss::ArticleList articles;
    if (rssItem == m_rssManager->rootFolder())
        articles = rssItem->unreadArticleListByDateDesc();
    else
        articles = rssItem->articleListByDateDesc();

    qDebug("Got the list of news");
    foreach (const Rss::ArticlePtr &article, articles) {
        QListWidgetItem *articleItem = createArticleListItem(article);
        listArticles->addItem(articleItem);
    }
    qDebug("Added all news to the GUI");
}

// display a news
void RSSImp::refreshTextBrowser()
{
    QList<QListWidgetItem * > selection = listArticles->selectedItems();
    if (selection.empty()) return;
    QListWidgetItem *item = selection.first();
    Q_ASSERT(item);
    if (item == m_currentArticle) return;
    m_currentArticle = item;

    Rss::ArticlePtr article = item->data(Article::ArticleRole).value<Rss::ArticlePtr>();
    if (!article) return;
    QString html;
    html += "<div style='border: 2px solid red; margin-left: 5px; margin-right: 5px; margin-bottom: 5px;'>";
    html += "<div style='background-color: #678db2; font-weight: bold; color: #fff;'>" + article->title() + "</div>";
    if (article->date().isValid())
        html += "<div style='background-color: #efefef;'><b>" + tr("Date: ") + "</b>" + article->date().toLocalTime().toString(Qt::SystemLocaleLongDate) + "</div>";
    if (!article->author().isEmpty())
        html += "<div style='background-color: #efefef;'><b>" + tr("Author: ") + "</b>" + article->author() + "</div>";
    html += "</div>";
    html += "<div style='margin-left: 5px; margin-right: 5px;'>";
    if (Qt::mightBeRichText(article->description())) {
        html += article->description();
    }
    else {
        QString description = article->description();
        QRegExp rx;
        // If description is plain text, replace BBCode tags with HTML and wrap everything in <pre></pre> so it looks nice
        rx.setMinimal(true);
        rx.setCaseSensitivity(Qt::CaseInsensitive);

        rx.setPattern("\\[img\\](.+)\\[/img\\]");
        description = description.replace(rx, "<img src=\"\\1\">");

        rx.setPattern("\\[url=(\")?(.+)\\1\\]");
        description = description.replace(rx, "<a href=\"\\2\">");
        description = description.replace("[/url]", "</a>", Qt::CaseInsensitive);

        rx.setPattern("\\[(/)?([bius])\\]");
        description = description.replace(rx, "<\\1\\2>");

        rx.setPattern("\\[color=(\")?(.+)\\1\\]");
        description = description.replace(rx, "<span style=\"color:\\2\">");
        description = description.replace("[/color]", "</span>", Qt::CaseInsensitive);

        rx.setPattern("\\[size=(\")?(.+)\\d\\1\\]");
        description = description.replace(rx, "<span style=\"font-size:\\2px\">");
        description = description.replace("[/size]", "</span>", Qt::CaseInsensitive);

        html += "<pre>" + description + "</pre>";
    }
    html += "</div>";
    textBrowser->setHtml(html);
    article->markAsRead();
    item->setData(Article::ColorRole, QVariant(QColor("grey")));
    item->setData(Article::IconRole, QVariant(QIcon(":/icons/sphere.png")));
}

void RSSImp::saveSlidersPosition()
{
    // Remember sliders positions
    Preferences *const pref = Preferences::instance();
    pref->setRssSideSplitterState(splitterSide->saveState());
    pref->setRssMainSplitterState(splitterMain->saveState());
    pref->setRssFeedWidgetState(feedWidget->header()->saveState());
    qDebug("Splitters position saved");
}

void RSSImp::restoreSlidersPosition()
{
    const Preferences *const pref = Preferences::instance();
    const QByteArray stateSide = pref->getRssSideSplitterState();
    if (!stateSide.isEmpty())
        splitterSide->restoreState(stateSide);
    const QByteArray stateMain = pref->getRssMainSplitterState();
    if (!stateMain.isEmpty())
        splitterMain->restoreState(stateMain);
    const QByteArray stateFeed = pref->getRssFeedWidgetState();
    if (!stateFeed.isEmpty())
        feedWidget->header()->restoreState(stateFeed);
}

void RSSImp::updateRefreshInterval(uint val)
{
    m_rssManager->updateRefreshInterval(val);
}

RssFeedListWidgetItem* RSSImp::addItemToSelected(const Rss::FilePtr &file)
{
    qDebug() << Q_FUNC_INFO << file->id();

    RssFeedListWidgetItem *parent = nullptr;

    QList<RssFeedListWidgetItem*> selected = feedWidget->selectedItemsNoRoot();
    if (!selected.empty()) {
        parent = selected.first();
        if (!feedWidget->isFolder(parent))
            parent = RSS_WIDGET_ITEM(parent->QTreeWidgetItem::parent());
    }

    FolderPtr rssParent(parent ? parent->rssFile().staticCast<Rss::Folder>() : feedWidget->getRoot());

    RssFeedListWidgetItem *item = nullptr;
    if (!(rssParent->hasChild(file->id()) || feedWidget->hasFeed(file->id()))) {
        rssParent->addFile(file);
        item = new RssFeedListWidgetItem(file);
        if (parent) {
            parent->addChild(item);
            parent->setExpanded(true);
        }
        else {
            feedWidget->addTopLevelItem(item);
        }
        feedWidget->setCurrentItem(item);
    }

    return item;
}

RSSImp::RSSImp(QWidget *parent)
    : QWidget(parent)
    , m_rssManager(new Rss::Manager)
{
    setupUi(this);

    // Icons
    actionCopy_feed_URL->setIcon(GuiIconProvider::instance()->getIcon("edit-copy"));
    actionDelete->setIcon(GuiIconProvider::instance()->getIcon("edit-delete"));
    actionDownload_torrent->setIcon(GuiIconProvider::instance()->getIcon("download"));
    actionMark_items_read->setIcon(GuiIconProvider::instance()->getIcon("mail-mark-read"));
    actionNew_folder->setIcon(GuiIconProvider::instance()->getIcon("folder-new"));
    actionNew_subscription->setIcon(GuiIconProvider::instance()->getIcon("list-add"));
    actionOpen_news_URL->setIcon(GuiIconProvider::instance()->getIcon("application-x-mswinurl"));
    actionRename->setIcon(GuiIconProvider::instance()->getIcon("edit-rename"));
    actionUpdate->setIcon(GuiIconProvider::instance()->getIcon("view-refresh"));
    newFeedButton->setIcon(GuiIconProvider::instance()->getIcon("list-add"));
    markReadButton->setIcon(GuiIconProvider::instance()->getIcon("mail-mark-read"));
    updateAllButton->setIcon(GuiIconProvider::instance()->getIcon("view-refresh"));
    rssDownloaderBtn->setIcon(GuiIconProvider::instance()->getIcon("download"));
    settingsButton->setIcon(GuiIconProvider::instance()->getIcon("configure", "preferences-system"));

    m_rssManager->loadStreamList();
    feedWidget->setRoot(m_rssManager->rootFolder());
    connect(feedWidget, SIGNAL(unreadCountChanged(int)), SIGNAL(updateRSSCount(int)));

    connect(feedWidget, SIGNAL(itemExpanded(QTreeWidgetItem*)), SLOT(onFolderExpanded(QTreeWidgetItem*)));
    connect(feedWidget, SIGNAL(itemCollapsed(QTreeWidgetItem*)), SLOT(onFolderCollapsed(QTreeWidgetItem*)));
    loadFoldersOpenState();

    m_editHotkey = new QShortcut(Qt::Key_F2, feedWidget, 0, 0, Qt::WidgetShortcut);
    feedWidget->connect(m_editHotkey, SIGNAL(activated()), SLOT(renameSelected()));

    m_deleteHotkey = new QShortcut(Qt::Key_Delete, feedWidget, 0, 0, Qt::WidgetShortcut);
    feedWidget->connect(m_deleteHotkey, SIGNAL(activated()), SLOT(deleteSelected()));

    m_refreshHotkey = new QShortcut(Qt::Key_F5, feedWidget, 0, 0, Qt::WidgetShortcut);
    feedWidget->connect(m_refreshHotkey, SIGNAL(activated()), SLOT(refreshSelected()));

    connect(feedWidget, SIGNAL(customContextMenuRequested(const QPoint&)), SLOT(displayRSSListMenu(const QPoint&)));
    connect(listArticles, SIGNAL(customContextMenuRequested(const QPoint&)), SLOT(displayItemsListMenu(const QPoint&)));

    // Feeds list actions
    feedWidget->connect(actionDelete, SIGNAL(triggered()), SLOT(deleteSelected()));
    feedWidget->connect(actionRename, SIGNAL(triggered()), SLOT(renameSelected()));
    feedWidget->connect(actionUpdate, SIGNAL(triggered()), SLOT(refreshSelected()));
    feedWidget->connect(updateAllButton, SIGNAL(clicked()), SLOT(refreshAll()));

    connect(actionNew_folder, SIGNAL(triggered()), SLOT(askNewFolder()));
    connect(actionNew_subscription, SIGNAL(triggered()), SLOT(on_newFeedButton_clicked()));
    connect(actionCopy_feed_URL, SIGNAL(triggered()), SLOT(copySelectedFeedsURL()));
    connect(actionMark_items_read, SIGNAL(triggered()), SLOT(on_markReadButton_clicked()));

    // News list actions
    connect(actionOpen_news_URL, SIGNAL(triggered()), SLOT(openSelectedArticlesUrls()));
    connect(actionDownload_torrent, SIGNAL(triggered()), SLOT(downloadSelectedTorrents()));

    connect(feedWidget, SIGNAL(currentItemChanged(QTreeWidgetItem *,QTreeWidgetItem *)), SLOT(populateArticleList(QTreeWidgetItem *)));

    connect(listArticles, SIGNAL(itemSelectionChanged()), SLOT(refreshTextBrowser()));
    connect(listArticles, SIGNAL(itemDoubleClicked(QListWidgetItem *)), SLOT(downloadSelectedTorrents()));

    // Restore sliders position
    restoreSlidersPosition();
    // Bind saveSliders slots
    connect(splitterMain, SIGNAL(splitterMoved(int,int)), SLOT(saveSlidersPosition()));
    connect(splitterSide, SIGNAL(splitterMoved(int,int)), SLOT(saveSlidersPosition()));
    connect(feedWidget->header(), SIGNAL(geometriesChanged()), SLOT(saveSlidersPosition()));

    qDebug("RSSImp constructed");
}

RSSImp::~RSSImp()
{
    qDebug("Deleting RSSImp...");
    Preferences::instance()->setRssOpenFolders(getOpenFolderPaths());
    delete m_editHotkey;
    delete m_deleteHotkey;
    delete m_refreshHotkey;
    qDebug("RSSImp deleted");
}

void RSSImp::on_settingsButton_clicked()
{
    RssSettingsDlg dlg(this);
    if (dlg.exec())
        updateRefreshInterval(Preferences::instance()->getRSSRefreshInterval());
}

void RSSImp::on_rssDownloaderBtn_clicked()
{
    AutomatedRssDownloader dlg(m_rssManager, this);
    dlg.exec();
    if (dlg.isRssDownloaderEnabled()) {
        m_rssManager->rootFolder()->recheckRssItemsForDownload();
        m_rssManager->refresh();
    }
}
