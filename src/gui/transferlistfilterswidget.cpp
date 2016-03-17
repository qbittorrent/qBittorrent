/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
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
 * Contact : chris@qbittorrent.org
 */

#include "transferlistfilterswidget.h"

#include <QDebug>
#include <QListWidgetItem>
#include <QIcon>
#include <QVBoxLayout>
#include <QMenu>
#include <QMessageBox>
#include <QCheckBox>
#include <QScrollArea>

#include "transferlistdelegate.h"
#include "transferlistwidget.h"
#include "base/preferences.h"
#include "torrentmodel.h"
#include "guiiconprovider.h"
#include "base/utils/fs.h"
#include "base/utils/string.h"
#include "autoexpandabledialog.h"
#include "base/torrentfilter.h"
#include "base/bittorrent/trackerentry.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrenthandle.h"
#include "base/net/downloadmanager.h"
#include "base/net/downloadhandler.h"
#include "base/utils/misc.h"
#include "base/logger.h"

FiltersBase::FiltersBase(QWidget *parent, TransferListWidget *transferList)
    : QListWidget(parent)
    , transferList(transferList)
{
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setUniformItemSizes(true);
    setSpacing(0);

    setIconSize(Utils::Misc::smallIconSize());

#if defined(Q_OS_MAC)
    setAttribute(Qt::WA_MacShowFocusRect, false);
#endif

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, SIGNAL(customContextMenuRequested(QPoint)), SLOT(showMenu(QPoint)));
    connect(this, SIGNAL(currentRowChanged(int)), SLOT(applyFilter(int)));

    connect(BitTorrent::Session::instance(), SIGNAL(torrentAdded(BitTorrent::TorrentHandle *const)), SLOT(handleNewTorrent(BitTorrent::TorrentHandle *const)));
    connect(BitTorrent::Session::instance(), SIGNAL(torrentAboutToBeRemoved(BitTorrent::TorrentHandle *const)), SLOT(torrentAboutToBeDeleted(BitTorrent::TorrentHandle *const)));
}

QSize FiltersBase::sizeHint() const
{
    QSize size;
    // Height should be exactly the height of the content
    size.setHeight(((sizeHintForRow(0) + 2 * spacing()) * (count() + 0.5)));
    // Width should be exactly the width of the content
    size.setWidth(sizeHintForColumn(0));
    return size;
}

QSize FiltersBase::minimumSizeHint() const
{
    QSize size = sizeHint();
    size.setWidth(6);
    return size;
}

void FiltersBase::toggleFilter(bool checked)
{
    setVisible(checked);
    if (checked)
        applyFilter(currentRow());
    else
        applyFilter(0);
}

StatusFiltersWidget::StatusFiltersWidget(QWidget *parent, TransferListWidget *transferList)
    : FiltersBase(parent, transferList)
{
    connect(BitTorrent::Session::instance(), SIGNAL(torrentsUpdated()), SLOT(updateTorrentNumbers()));

    // Add status filters
    QListWidgetItem *all = new QListWidgetItem(this);
    all->setData(Qt::DisplayRole, QVariant(tr("All (0)", "this is for the status filter")));
    all->setData(Qt::DecorationRole, QIcon(":/icons/skin/filterall.png"));
    QListWidgetItem *downloading = new QListWidgetItem(this);
    downloading->setData(Qt::DisplayRole, QVariant(tr("Downloading (0)")));
    downloading->setData(Qt::DecorationRole, QIcon(":/icons/skin/downloading.png"));
    QListWidgetItem *seeding = new QListWidgetItem(this);
    seeding->setData(Qt::DisplayRole, QVariant(tr("Seeding (0)")));
    seeding->setData(Qt::DecorationRole, QIcon(":/icons/skin/uploading.png"));
    QListWidgetItem *completed = new QListWidgetItem(this);
    completed->setData(Qt::DisplayRole, QVariant(tr("Completed (0)")));
    completed->setData(Qt::DecorationRole, QIcon(":/icons/skin/completed.png"));
    QListWidgetItem *resumed = new QListWidgetItem(this);
    resumed->setData(Qt::DisplayRole, QVariant(tr("Resumed (0)")));
    resumed->setData(Qt::DecorationRole, QIcon(":/icons/skin/resumed.png"));
    QListWidgetItem *paused = new QListWidgetItem(this);
    paused->setData(Qt::DisplayRole, QVariant(tr("Paused (0)")));
    paused->setData(Qt::DecorationRole, QIcon(":/icons/skin/paused.png"));
    QListWidgetItem *active = new QListWidgetItem(this);
    active->setData(Qt::DisplayRole, QVariant(tr("Active (0)")));
    active->setData(Qt::DecorationRole, QIcon(":/icons/skin/filteractive.png"));
    QListWidgetItem *inactive = new QListWidgetItem(this);
    inactive->setData(Qt::DisplayRole, QVariant(tr("Inactive (0)")));
    inactive->setData(Qt::DecorationRole, QIcon(":/icons/skin/filterinactive.png"));
    QListWidgetItem *errored = new QListWidgetItem(this);
    errored->setData(Qt::DisplayRole, QVariant(tr("Errored (0)")));
    errored->setData(Qt::DecorationRole, QIcon(":/icons/skin/error.png"));

    const Preferences* const pref = Preferences::instance();
    setCurrentRow(pref->getTransSelFilter(), QItemSelectionModel::SelectCurrent);
    toggleFilter(pref->getStatusFilterState());
}

StatusFiltersWidget::~StatusFiltersWidget()
{
    Preferences::instance()->setTransSelFilter(currentRow());
}

void StatusFiltersWidget::updateTorrentNumbers()
{
    auto report = BitTorrent::Session::instance()->torrentStatusReport();

    item(TorrentFilter::All)->setData(Qt::DisplayRole, QVariant(tr("All (%1)").arg(report.nbActive + report.nbInactive)));
    item(TorrentFilter::Downloading)->setData(Qt::DisplayRole, QVariant(tr("Downloading (%1)").arg(report.nbDownloading)));
    item(TorrentFilter::Seeding)->setData(Qt::DisplayRole, QVariant(tr("Seeding (%1)").arg(report.nbSeeding)));
    item(TorrentFilter::Completed)->setData(Qt::DisplayRole, QVariant(tr("Completed (%1)").arg(report.nbCompleted)));
    item(TorrentFilter::Paused)->setData(Qt::DisplayRole, QVariant(tr("Paused (%1)").arg(report.nbPaused)));
    item(TorrentFilter::Resumed)->setData(Qt::DisplayRole, QVariant(tr("Resumed (%1)").arg(report.nbResumed)));
    item(TorrentFilter::Active)->setData(Qt::DisplayRole, QVariant(tr("Active (%1)").arg(report.nbActive)));
    item(TorrentFilter::Inactive)->setData(Qt::DisplayRole, QVariant(tr("Inactive (%1)").arg(report.nbInactive)));
    item(TorrentFilter::Errored)->setData(Qt::DisplayRole, QVariant(tr("Errored (%1)").arg(report.nbErrored)));
}

void StatusFiltersWidget::showMenu(QPoint) {}

void StatusFiltersWidget::applyFilter(int row)
{
    transferList->applyStatusFilter(row);
}

void StatusFiltersWidget::handleNewTorrent(BitTorrent::TorrentHandle *const) {}

void StatusFiltersWidget::torrentAboutToBeDeleted(BitTorrent::TorrentHandle *const) {}

CategoryFiltersList::CategoryFiltersList(QWidget *parent, TransferListWidget *transferList)
    : FiltersBase(parent, transferList)
{
    connect(BitTorrent::Session::instance(), SIGNAL(torrentCategoryChanged(BitTorrent::TorrentHandle *const, QString)), SLOT(torrentCategoryChanged(BitTorrent::TorrentHandle *const, QString)));
    connect(BitTorrent::Session::instance(), SIGNAL(categoryAdded(QString)), SLOT(addItem(QString)));
    connect(BitTorrent::Session::instance(), SIGNAL(categoryRemoved(QString)), SLOT(categoryRemoved(QString)));
    connect(BitTorrent::Session::instance(), SIGNAL(subcategoriesSupportChanged()), SLOT(subcategoriesSupportChanged()));

    refresh();
    toggleFilter(Preferences::instance()->getCategoryFilterState());
}

void CategoryFiltersList::refresh()
{
    clear();
    m_categories.clear();
    m_totalTorrents = 0;
    m_totalCategorized = 0;

    QListWidgetItem *allCategories = new QListWidgetItem(this);
    allCategories->setData(Qt::DisplayRole, QVariant(tr("All (0)", "this is for the category filter")));
    allCategories->setData(Qt::DecorationRole, GuiIconProvider::instance()->getIcon("inode-directory"));
    QListWidgetItem *noCategory = new QListWidgetItem(this);
    noCategory->setData(Qt::DisplayRole, QVariant(tr("Uncategorized (0)")));
    noCategory->setData(Qt::DecorationRole, GuiIconProvider::instance()->getIcon("inode-directory"));

    foreach (const QString &category, BitTorrent::Session::instance()->categories())
        addItem(category, false);

    foreach (BitTorrent::TorrentHandle *const torrent, BitTorrent::Session::instance()->torrents())
        handleNewTorrent(torrent);

    setCurrentRow(0, QItemSelectionModel::SelectCurrent);
}

void CategoryFiltersList::addItem(const QString &category, bool hasTorrent)
{
    if (category.isEmpty()) return;

    int torrentsInCategory = 0;
    QListWidgetItem *categoryItem = 0;

    bool exists = m_categories.contains(category);
    if (exists) {
        torrentsInCategory = m_categories.value(category);
        categoryItem = item(rowFromCategory(category));
    }
    else {
        categoryItem = new QListWidgetItem();
        categoryItem->setData(Qt::DecorationRole, GuiIconProvider::instance()->getIcon("inode-directory"));
    }

    if (hasTorrent)
        ++torrentsInCategory;

    m_categories.insert(category, torrentsInCategory);
    categoryItem->setText(tr("%1 (%2)", "category_name (10)").arg(category).arg(torrentsInCategory));
    if (exists) return;

    Q_ASSERT(count() >= 2);
    for (int i = 2; i < count(); ++i) {
        bool less = false;
        if (!(Utils::String::naturalSort(category, item(i)->text(), less)))
            less = (category.localeAwareCompare(item(i)->text()) < 0);
        if (less) {
            insertItem(i, categoryItem);
            updateGeometry();
            return;
        }
    }
    QListWidget::addItem(categoryItem);
    updateGeometry();
}

void CategoryFiltersList::removeItem(const QString &category)
{
    if (category.isEmpty()) return;

    int torrentsInCategory = m_categories.value(category) - 1;
    int row = rowFromCategory(category);
    if (row < 2) return;

    QListWidgetItem *categoryItem = item(row);
    categoryItem->setText(tr("%1 (%2)", "category_name (10)").arg(category).arg(torrentsInCategory));
    m_categories.insert(category, torrentsInCategory);
}

void CategoryFiltersList::removeSelectedCategory()
{
    QList<QListWidgetItem*> items = selectedItems();
    if (items.size() == 0) return;

    const int categoryRow = row(items.first());
    if (categoryRow < 2) return;

    BitTorrent::Session::instance()->removeCategory(categoryFromRow(categoryRow));
    updateGeometry();
}

void CategoryFiltersList::removeUnusedCategories()
{
    foreach (const QString &category, m_categories.keys())
        if (m_categories[category] == 0)
            BitTorrent::Session::instance()->removeCategory(category);
    updateGeometry();
}

void CategoryFiltersList::torrentCategoryChanged(BitTorrent::TorrentHandle *const torrent, const QString &oldCategory)
{
    qDebug() << "Torrent category changed from" << oldCategory << "to" << torrent->category();

    if (torrent->category().isEmpty() && !oldCategory.isEmpty())
        --m_totalCategorized;
    else if (!torrent->category().isEmpty() && oldCategory.isEmpty())
        ++m_totalCategorized;

    item(1)->setText(tr("Uncategorized (%1)").arg(m_totalTorrents - m_totalCategorized));

    if (BitTorrent::Session::instance()->isSubcategoriesEnabled()) {
        foreach (const QString &subcategory, BitTorrent::Session::expandCategory(oldCategory))
            removeItem(subcategory);
        foreach (const QString &subcategory, BitTorrent::Session::expandCategory(torrent->category()))
            addItem(subcategory, true);
    }
    else {
        removeItem(oldCategory);
        addItem(torrent->category(), true);
    }
}

void CategoryFiltersList::categoryRemoved(const QString &category)
{
    m_categories.remove(category);
    delete takeItem(rowFromCategory(category));
}

void CategoryFiltersList::subcategoriesSupportChanged()
{
    refresh();
}

void CategoryFiltersList::showMenu(QPoint)
{
    QMenu menu(this);
    QAction *addAct = menu.addAction(GuiIconProvider::instance()->getIcon("list-add"), tr("Add category..."));
    QAction *removeAct = 0;
    QAction *removeUnusedAct = 0;
    if (!selectedItems().empty() && row(selectedItems().first()) > 1)
        removeAct = menu.addAction(GuiIconProvider::instance()->getIcon("list-remove"), tr("Remove category"));
    removeUnusedAct = menu.addAction(GuiIconProvider::instance()->getIcon("list-remove"), tr("Remove unused categories"));
    menu.addSeparator();
    QAction *startAct = menu.addAction(GuiIconProvider::instance()->getIcon("media-playback-start"), tr("Resume torrents"));
    QAction *pauseAct = menu.addAction(GuiIconProvider::instance()->getIcon("media-playback-pause"), tr("Pause torrents"));
    QAction *deleteTorrentsAct = menu.addAction(GuiIconProvider::instance()->getIcon("edit-delete"), tr("Delete torrents"));
    QAction *act = 0;
    act = menu.exec(QCursor::pos());
    if (!act)
        return;

    if (act == removeAct) {
        removeSelectedCategory();
    }
    else if (act == removeUnusedAct) {
        removeUnusedCategories();
    }
    else if (act == deleteTorrentsAct) {
        transferList->deleteVisibleTorrents();
    }
    else if (act == startAct) {
        transferList->startVisibleTorrents();
    }
    else if (act == pauseAct) {
        transferList->pauseVisibleTorrents();
    }
    else if (act == addAct) {
        bool ok;
        QString category = "";
        bool invalid;
        do {
            invalid = false;
            category = AutoExpandableDialog::getText(this, tr("New Category"), tr("Category:"), QLineEdit::Normal, category, &ok);
            if (ok && !category.isEmpty()) {
                if (!BitTorrent::Session::isValidCategoryName(category)) {
                    QMessageBox::warning(this, tr("Invalid category name"),
                                         tr("Category name must not contain '\\'.\n"
                                            "Category name must not start/end with '/'.\n"
                                            "Category name must not contain '//' sequence."));
                    invalid = true;
                }
                else {
                    BitTorrent::Session::instance()->addCategory(category);
                }
            }
        } while (invalid);
    }
}

void CategoryFiltersList::applyFilter(int row)
{
    if (row >= 0)
        transferList->applyCategoryFilter(categoryFromRow(row));
}

void CategoryFiltersList::handleNewTorrent(BitTorrent::TorrentHandle *const torrent)
{
    Q_ASSERT(torrent);

    ++m_totalTorrents;
    if (!torrent->category().isEmpty())
        ++m_totalCategorized;

    item(0)->setText(tr("All (%1)", "this is for the category filter").arg(m_totalTorrents));
    item(1)->setText(tr("Uncategorized (%1)").arg(m_totalTorrents - m_totalCategorized));

    if (BitTorrent::Session::instance()->isSubcategoriesEnabled()) {
        foreach (const QString &subcategory, BitTorrent::Session::expandCategory(torrent->category()))
            addItem(subcategory, true);
    }
    else {
        addItem(torrent->category(), true);
    }
}

void CategoryFiltersList::torrentAboutToBeDeleted(BitTorrent::TorrentHandle *const torrent)
{
    Q_ASSERT(torrent);

    --m_totalTorrents;
    if (!torrent->category().isEmpty())
        --m_totalCategorized;

    item(0)->setText(tr("All (%1)", "this is for the category filter").arg(m_totalTorrents));
    item(1)->setText(tr("Uncategorized (%1)").arg(m_totalTorrents - m_totalCategorized));

    if (BitTorrent::Session::instance()->isSubcategoriesEnabled()) {
        foreach (const QString &subcategory, BitTorrent::Session::expandCategory(torrent->category()))
            removeItem(subcategory);
    }
    else {
        removeItem(torrent->category());
    }
}

QString CategoryFiltersList::categoryFromRow(int row) const
{
    if (row == 0) return QString(); // All
    if (row == 1) return QLatin1String(""); // Uncategorized

    const QString &category = item(row)->text();
    QStringList parts = category.split(" ");
    Q_ASSERT(parts.size() >= 2);
    parts.removeLast(); // Remove trailing number
    return parts.join(" ");
}

int CategoryFiltersList::rowFromCategory(const QString &category) const
{
    Q_ASSERT(!category.isEmpty());
    for (int i = 2; i<count(); ++i)
        if (category == categoryFromRow(i)) return i;
    return -1;
}

TrackerFiltersList::TrackerFiltersList(QWidget *parent, TransferListWidget *transferList)
    : FiltersBase(parent, transferList)
    , m_totalTorrents(0)
{
    QListWidgetItem *allTrackers = new QListWidgetItem(this);
    allTrackers->setData(Qt::DisplayRole, QVariant(tr("All (0)", "this is for the tracker filter")));
    allTrackers->setData(Qt::DecorationRole, GuiIconProvider::instance()->getIcon("network-server"));
    QListWidgetItem *noTracker = new QListWidgetItem(this);
    noTracker->setData(Qt::DisplayRole, QVariant(tr("Trackerless (0)")));
    noTracker->setData(Qt::DecorationRole, GuiIconProvider::instance()->getIcon("network-server"));
    QListWidgetItem *errorTracker = new QListWidgetItem(this);
    errorTracker->setData(Qt::DisplayRole, QVariant(tr("Error (0)")));
    errorTracker->setData(Qt::DecorationRole, style()->standardIcon(QStyle::SP_MessageBoxCritical));
    QListWidgetItem *warningTracker = new QListWidgetItem(this);
    warningTracker->setData(Qt::DisplayRole, QVariant(tr("Warning (0)")));
    warningTracker->setData(Qt::DecorationRole, style()->standardIcon(QStyle::SP_MessageBoxWarning));
    m_trackers.insert("", QStringList());

    setCurrentRow(0, QItemSelectionModel::SelectCurrent);
    toggleFilter(Preferences::instance()->getTrackerFilterState());
}

TrackerFiltersList::~TrackerFiltersList()
{
    foreach (const QString &iconPath, m_iconPaths)
        Utils::Fs::forceRemove(iconPath);
}

void TrackerFiltersList::addItem(const QString &tracker, const QString &hash)
{
    QStringList tmp;
    QListWidgetItem *trackerItem = 0;
    QString host = getHost(tracker);
    bool exists = m_trackers.contains(host);

    if (exists) {
        tmp = m_trackers.value(host);
        if (tmp.contains(hash))
            return;

        if (host != "") {
            trackerItem = item(rowFromTracker(host));
            if (!trackerItem) return;
        }
        else {
            trackerItem = item(1);
        }
    }
    else {
        trackerItem = new QListWidgetItem();
        trackerItem->setData(Qt::DecorationRole, GuiIconProvider::instance()->getIcon("network-server"));

        downloadFavicon(QString("http://%1/favicon.ico").arg(host));
    }

    tmp.append(hash);
    m_trackers.insert(host, tmp);
    if (host == "") {
        trackerItem->setText(tr("Trackerless (%1)").arg(tmp.size()));
        if (currentRow() == 1)
            applyFilter(1);
        return;
    }

    trackerItem->setText(tr("%1 (%2)", "openbittorrent.com (10)").arg(host).arg(tmp.size()));
    if (exists) {
        if (currentRow() == rowFromTracker(host))
            applyFilter(currentRow());
        return;
    }

    Q_ASSERT(count() >= 4);
    for (int i = 4; i<count(); ++i) {
        bool less = false;
        if (!(Utils::String::naturalSort(host, item(i)->text(), less)))
            less = (host.localeAwareCompare(item(i)->text()) < 0);
        if (less) {
            insertItem(i, trackerItem);
            updateGeometry();
            return;
        }
    }
    QListWidget::addItem(trackerItem);
    updateGeometry();
}

void TrackerFiltersList::removeItem(const QString &tracker, const QString &hash)
{
    QString host = getHost(tracker);
    QListWidgetItem *trackerItem = 0;
    QStringList tmp = m_trackers.value(host);
    int row = 0;

    if (tmp.empty())
        return;
    tmp.removeAll(hash);

    if (host != "") {
        // Remove from 'Error' and 'Warning' view
        trackerSuccess(hash, tracker);
        row = rowFromTracker(host);
        trackerItem = item(row);
        if (tmp.empty()) {
            if (currentRow() == row)
                setCurrentRow(0, QItemSelectionModel::SelectCurrent);
            delete trackerItem;
            m_trackers.remove(host);
            updateGeometry();
            return;
        }
        trackerItem->setText(tr("%1 (%2)", "openbittorrent.com (10)").arg(host).arg(tmp.size()));
    }
    else {
        row = 1;
        trackerItem = item(1);
        trackerItem->setText(tr("Trackerless (%1)").arg(tmp.size()));
    }

    m_trackers.insert(host, tmp);
    if (currentRow() == row)
        applyFilter(row);
}

void TrackerFiltersList::changeTrackerless(bool trackerless, const QString &hash)
{
    if (trackerless)
        addItem("", hash);
    else
        removeItem("", hash);
}

void TrackerFiltersList::trackerSuccess(const QString &hash, const QString &tracker)
{
    QStringList errored = m_errors.value(hash);
    QStringList warned = m_warnings.value(hash);

    if (errored.contains(tracker)) {
        errored.removeAll(tracker);
        if (errored.empty()) {
            m_errors.remove(hash);
            item(2)->setText(tr("Error (%1)").arg(m_errors.size()));
            if (currentRow() == 2)
                applyFilter(2);
        }
        else {
            m_errors.insert(hash, errored);
        }
    }

    if (warned.contains(tracker)) {
        warned.removeAll(tracker);
        if (warned.empty()) {
            m_warnings.remove(hash);
            item(3)->setText(tr("Warning (%1)").arg(m_warnings.size()));
            if (currentRow() == 3)
                applyFilter(3);
        }
        else {
            m_warnings.insert(hash, warned);
        }
    }
}

void TrackerFiltersList::trackerError(const QString &hash, const QString &tracker)
{
    QStringList trackers = m_errors.value(hash);

    if (trackers.contains(tracker))
        return;

    trackers.append(tracker);
    m_errors.insert(hash, trackers);
    item(2)->setText(tr("Error (%1)").arg(m_errors.size()));

    if (currentRow() == 2)
        applyFilter(2);
}

void TrackerFiltersList::trackerWarning(const QString &hash, const QString &tracker)
{
    QStringList trackers = m_warnings.value(hash);

    if (trackers.contains(tracker))
        return;

    trackers.append(tracker);
    m_warnings.insert(hash, trackers);
    item(3)->setText(tr("Warning (%1)").arg(m_warnings.size()));

    if (currentRow() == 3)
        applyFilter(3);
}

void TrackerFiltersList::downloadFavicon(const QString& url)
{
    Net::DownloadHandler *h = Net::DownloadManager::instance()->downloadUrl(url, true);
    connect(h, SIGNAL(downloadFinished(QString, QString)), this, SLOT(handleFavicoDownload(QString, QString)));
    connect(h, SIGNAL(downloadFailed(QString, QString)), this, SLOT(handleFavicoFailure(QString, QString)));
}

void TrackerFiltersList::handleFavicoDownload(const QString& url, const QString& filePath)
{
    QString host = getHost(url);
    if (!m_trackers.contains(host)) {
        Utils::Fs::forceRemove(filePath);
        return;
    }

    QListWidgetItem *trackerItem = item(rowFromTracker(host));
    if (!trackerItem) return;

    QIcon icon(filePath);
    //Detect a non-decodable icon
    QList<QSize> sizes = icon.availableSizes();
    bool invalid = (sizes.isEmpty() || icon.pixmap(sizes.first()).isNull());
    if (invalid) {
        if (url.endsWith(".ico", Qt::CaseInsensitive)) {
            Logger::instance()->addMessage(tr("Couldn't decode favicon for URL '%1'. Trying to download favicon in PNG format.").arg(url),
                                           Log::WARNING);
            downloadFavicon(url.left(url.size() - 4) + ".png");
        }
        else {
            Logger::instance()->addMessage(tr("Couldn't decode favicon for URL '%1'.").arg(url), Log::WARNING);
        }
        Utils::Fs::forceRemove(filePath);
    }
    else {
        trackerItem->setData(Qt::DecorationRole, QVariant(QIcon(filePath)));
        m_iconPaths.append(filePath);
    }
}

void TrackerFiltersList::handleFavicoFailure(const QString& url, const QString& error)
{
    // Don't use getHost() on the url here. Print the full url. The error might relate to
    // that.
    Logger::instance()->addMessage(tr("Couldn't download favicon for URL '%1'. Reason: %2").arg(url).arg(error),
                                   Log::WARNING);
    if (url.endsWith(".ico", Qt::CaseInsensitive))
        downloadFavicon(url.left(url.size() - 4) + ".png");
}

void TrackerFiltersList::showMenu(QPoint)
{
    QMenu menu(this);
    QAction *startAct = menu.addAction(GuiIconProvider::instance()->getIcon("media-playback-start"), tr("Resume torrents"));
    QAction *pauseAct = menu.addAction(GuiIconProvider::instance()->getIcon("media-playback-pause"), tr("Pause torrents"));
    QAction *deleteTorrentsAct = menu.addAction(GuiIconProvider::instance()->getIcon("edit-delete"), tr("Delete torrents"));
    QAction *act = 0;
    act = menu.exec(QCursor::pos());

    if (!act)
        return;

    if (act == startAct)
        transferList->startVisibleTorrents();
    else if (act == pauseAct)
        transferList->pauseVisibleTorrents();
    else if (act == deleteTorrentsAct)
        transferList->deleteVisibleTorrents();
}

void TrackerFiltersList::applyFilter(int row)
{
    if (row == 0)
        transferList->applyTrackerFilterAll();
    else if (isVisible())
        transferList->applyTrackerFilter(getHashes(row));
}

void TrackerFiltersList::handleNewTorrent(BitTorrent::TorrentHandle *const torrent)
{
    QString hash = torrent->hash();
    QList<BitTorrent::TrackerEntry> trackers = torrent->trackers();
    foreach (const BitTorrent::TrackerEntry &tracker, trackers)
        addItem(tracker.url(), hash);

    //Check for trackerless torrent
    if (trackers.size() == 0)
        addItem("", hash);

    item(0)->setText(tr("All (%1)", "this is for the tracker filter").arg(++m_totalTorrents));
}

void TrackerFiltersList::torrentAboutToBeDeleted(BitTorrent::TorrentHandle *const torrent)
{
    QString hash = torrent->hash();
    QList<BitTorrent::TrackerEntry> trackers = torrent->trackers();
    foreach (const BitTorrent::TrackerEntry &tracker, trackers)
        removeItem(tracker.url(), hash);

    //Check for trackerless torrent
    if (trackers.size() == 0)
        removeItem("", hash);

    item(0)->setText(tr("All (%1)", "this is for the tracker filter").arg(--m_totalTorrents));
}

QString TrackerFiltersList::trackerFromRow(int row) const
{
    Q_ASSERT(row > 1);
    const QString &tracker = item(row)->text();
    QStringList parts = tracker.split(" ");
    Q_ASSERT(parts.size() >= 2);
    parts.removeLast(); // Remove trailing number
    return parts.join(" ");
}

int TrackerFiltersList::rowFromTracker(const QString &tracker) const
{
    Q_ASSERT(!tracker.isEmpty());
    for (int i = 4; i<count(); ++i)
        if (tracker == trackerFromRow(i)) return i;
    return -1;
}

QString TrackerFiltersList::getHost(const QString &tracker) const
{
    QUrl url(tracker);
    QString longHost = url.host();
    QString tld = url.topLevelDomain();
    // We get empty tld when it is invalid or an IPv4/IPv6 address,
    // so just return the full host
    if (tld.isEmpty())
        return longHost;
    // We want the domain + tld. Subdomains should be disregarded
    int index = longHost.lastIndexOf('.', -(tld.size() + 1));
    if (index == -1)
        return longHost;
    return longHost.mid(index + 1);
}

QStringList TrackerFiltersList::getHashes(int row)
{
    if (row == 1)
        return m_trackers.value("");
    else if (row == 2)
        return m_errors.keys();
    else if (row == 3)
        return m_warnings.keys();
    else
        return m_trackers.value(trackerFromRow(row));
}

TransferListFiltersWidget::TransferListFiltersWidget(QWidget *parent, TransferListWidget *transferList)
    : QFrame(parent)
    , trackerFilters(0)
{
    Preferences* const pref = Preferences::instance();

    // Construct lists
    QVBoxLayout *vLayout = new QVBoxLayout(this);
    QScrollArea *scroll = new QScrollArea(this);
    QFrame *frame = new QFrame(scroll);
    QVBoxLayout *frameLayout = new QVBoxLayout(frame);
    QFont font;
    font.setBold(true);
    font.setCapitalization(QFont::AllUppercase);

    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    setStyleSheet("QFrame {background: transparent;}");
    scroll->setStyleSheet("QFrame {border: none;}");
    vLayout->setContentsMargins(0, 0, 0, 0);
    frameLayout->setContentsMargins(0, 2, 0, 0);
    frameLayout->setSpacing(2);
    frameLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    frame->setLayout(frameLayout);
    scroll->setWidget(frame);
    vLayout->addWidget(scroll);
    setLayout(vLayout);

    QCheckBox * statusLabel = new QCheckBox(tr("Status"), this);
    statusLabel->setChecked(pref->getStatusFilterState());
    statusLabel->setFont(font);
    frameLayout->addWidget(statusLabel);

    StatusFiltersWidget *statusFilters = new StatusFiltersWidget(this, transferList);
    frameLayout->addWidget(statusFilters);

    QCheckBox *categoryLabel = new QCheckBox(tr("Categories"), this);
    categoryLabel->setChecked(pref->getCategoryFilterState());
    categoryLabel->setFont(font);
    frameLayout->addWidget(categoryLabel);

    CategoryFiltersList *categoryFilters = new CategoryFiltersList(this, transferList);
    frameLayout->addWidget(categoryFilters);

    QCheckBox *trackerLabel = new QCheckBox(tr("Trackers"), this);
    trackerLabel->setChecked(pref->getTrackerFilterState());
    trackerLabel->setFont(font);
    frameLayout->addWidget(trackerLabel);

    trackerFilters = new TrackerFiltersList(this, transferList);
    frameLayout->addWidget(trackerFilters);

    connect(statusLabel, SIGNAL(toggled(bool)), statusFilters, SLOT(toggleFilter(bool)));
    connect(statusLabel, SIGNAL(toggled(bool)), pref, SLOT(setStatusFilterState(const bool)));
    connect(categoryLabel, SIGNAL(toggled(bool)), categoryFilters, SLOT(toggleFilter(bool)));
    connect(categoryLabel, SIGNAL(toggled(bool)), pref, SLOT(setCategoryFilterState(const bool)));
    connect(trackerLabel, SIGNAL(toggled(bool)), trackerFilters, SLOT(toggleFilter(bool)));
    connect(trackerLabel, SIGNAL(toggled(bool)), pref, SLOT(setTrackerFilterState(const bool)));
    connect(this, SIGNAL(trackerSuccess(const QString &, const QString &)), trackerFilters, SLOT(trackerSuccess(const QString &, const QString &)));
    connect(this, SIGNAL(trackerError(const QString &, const QString &)), trackerFilters, SLOT(trackerError(const QString &, const QString &)));
    connect(this, SIGNAL(trackerWarning(const QString &, const QString &)), trackerFilters, SLOT(trackerWarning(const QString &, const QString &)));
}

void TransferListFiltersWidget::addTrackers(BitTorrent::TorrentHandle *const torrent, const QList<BitTorrent::TrackerEntry> &trackers)
{
    foreach (const BitTorrent::TrackerEntry &tracker, trackers)
        trackerFilters->addItem(tracker.url(), torrent->hash());
}

void TransferListFiltersWidget::removeTrackers(BitTorrent::TorrentHandle *const torrent, const QList<BitTorrent::TrackerEntry> &trackers)
{
    foreach (const BitTorrent::TrackerEntry &tracker, trackers)
        trackerFilters->removeItem(tracker.url(), torrent->hash());
}

void TransferListFiltersWidget::changeTrackerless(BitTorrent::TorrentHandle *const torrent, bool trackerless)
{
    trackerFilters->changeTrackerless(trackerless, torrent->hash());
}

void TransferListFiltersWidget::trackerSuccess(BitTorrent::TorrentHandle *const torrent, const QString &tracker)
{
    emit trackerSuccess(torrent->hash(), tracker);
}

void TransferListFiltersWidget::trackerWarning(BitTorrent::TorrentHandle *const torrent, const QString &tracker)
{
    emit trackerWarning(torrent->hash(), tracker);
}

void TransferListFiltersWidget::trackerError(BitTorrent::TorrentHandle *const torrent, const QString &tracker)
{
    emit trackerError(torrent->hash(), tracker);
}
