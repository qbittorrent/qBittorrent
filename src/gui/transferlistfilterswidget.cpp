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
#include "core/preferences.h"
#include "torrentmodel.h"
#include "guiiconprovider.h"
#include "core/utils/fs.h"
#include "core/utils/string.h"
#include "autoexpandabledialog.h"
#include "core/torrentfilter.h"
#include "core/bittorrent/trackerentry.h"
#include "core/bittorrent/session.h"
#include "core/bittorrent/torrenthandle.h"
#include "core/net/downloadmanager.h"
#include "core/net/downloadhandler.h"
#include "core/utils/misc.h"
#include "core/logger.h"

FiltersBase::FiltersBase(QWidget *parent, TransferListWidget *transferList)
    : QListWidget(parent)
    , transferList(transferList)
{
    setStyleSheet("QListWidget { background: transparent; border: 0 }");
#if defined(Q_OS_MAC)
    setAttribute(Qt::WA_MacShowFocusRect, false);
#endif
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setContextMenuPolicy(Qt::CustomContextMenu);

    setIconSize(QSize(16, 16));

    connect(this, SIGNAL(customContextMenuRequested(QPoint)), SLOT(showMenu(QPoint)));
    connect(this, SIGNAL(currentRowChanged(int)), SLOT(applyFilter(int)));

    connect(BitTorrent::Session::instance(), SIGNAL(torrentAdded(BitTorrent::TorrentHandle *const)), SLOT(handleNewTorrent(BitTorrent::TorrentHandle *const)));
    connect(BitTorrent::Session::instance(), SIGNAL(torrentAboutToBeRemoved(BitTorrent::TorrentHandle *const)), SLOT(torrentAboutToBeDeleted(BitTorrent::TorrentHandle *const)));
}

QSize FiltersBase::sizeHint() const
{
    QSize size;
    // Height should be exactly the height of the content
    size.setHeight((sizeHintForRow(0) * count()) + (2 * frameWidth()) + 6);
    // Width should be exactly the height of the content
    size.setWidth(sizeHintForColumn(0) + (2 * frameWidth()));
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
    setUniformItemSizes(true);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // Height is fixed (sizeHint().height() is used)
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setSpacing(0);
    connect(BitTorrent::Session::instance(), SIGNAL(torrentsUpdated(const BitTorrent::TorrentStatusReport &)), SLOT(updateTorrentNumbers(const BitTorrent::TorrentStatusReport &)));

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

    const Preferences* const pref = Preferences::instance();
    setCurrentRow(pref->getTransSelFilter(), QItemSelectionModel::SelectCurrent);
    toggleFilter(pref->getStatusFilterState());
}

StatusFiltersWidget::~StatusFiltersWidget()
{
    Preferences::instance()->setTransSelFilter(currentRow());
}

void StatusFiltersWidget::updateTorrentNumbers(const BitTorrent::TorrentStatusReport &report)
{
    item(TorrentFilter::All)->setData(Qt::DisplayRole, QVariant(tr("All (%1)").arg(report.nbActive + report.nbInactive)));
    item(TorrentFilter::Downloading)->setData(Qt::DisplayRole, QVariant(tr("Downloading (%1)").arg(report.nbDownloading)));
    item(TorrentFilter::Seeding)->setData(Qt::DisplayRole, QVariant(tr("Seeding (%1)").arg(report.nbSeeding)));
    item(TorrentFilter::Completed)->setData(Qt::DisplayRole, QVariant(tr("Completed (%1)").arg(report.nbCompleted)));
    item(TorrentFilter::Paused)->setData(Qt::DisplayRole, QVariant(tr("Paused (%1)").arg(report.nbPaused)));
    item(TorrentFilter::Resumed)->setData(Qt::DisplayRole, QVariant(tr("Resumed (%1)").arg(report.nbResumed)));
    item(TorrentFilter::Active)->setData(Qt::DisplayRole, QVariant(tr("Active (%1)").arg(report.nbActive)));
    item(TorrentFilter::Inactive)->setData(Qt::DisplayRole, QVariant(tr("Inactive (%1)").arg(report.nbInactive)));
}

void StatusFiltersWidget::showMenu(QPoint) {}

void StatusFiltersWidget::applyFilter(int row)
{
    transferList->applyStatusFilter(row);
}

void StatusFiltersWidget::handleNewTorrent(BitTorrent::TorrentHandle *const) {}

void StatusFiltersWidget::torrentAboutToBeDeleted(BitTorrent::TorrentHandle *const) {}

LabelFiltersList::LabelFiltersList(QWidget *parent, TransferListWidget *transferList)
    : FiltersBase(parent, transferList)
    , m_totalTorrents(0)
    , m_totalLabeled(0)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    connect(BitTorrent::Session::instance(), SIGNAL(torrentLabelChanged(BitTorrent::TorrentHandle *const, QString)), SLOT(torrentChangedLabel(BitTorrent::TorrentHandle *const, QString)));

    // Add Label filters
    QListWidgetItem *allLabels = new QListWidgetItem(this);
    allLabels->setData(Qt::DisplayRole, QVariant(tr("All (0)", "this is for the label filter")));
    allLabels->setData(Qt::DecorationRole, GuiIconProvider::instance()->getIcon("inode-directory"));
    QListWidgetItem *noLabel = new QListWidgetItem(this);
    noLabel->setData(Qt::DisplayRole, QVariant(tr("Unlabeled (0)")));
    noLabel->setData(Qt::DecorationRole, GuiIconProvider::instance()->getIcon("inode-directory"));

    const Preferences* const pref = Preferences::instance();
    QStringList labelList = pref->getTorrentLabels();
    for (int i=0; i < labelList.size(); ++i)
        addItem(labelList[i], false);

    setCurrentRow(0, QItemSelectionModel::SelectCurrent);
    toggleFilter(pref->getLabelFilterState());
}

LabelFiltersList::~LabelFiltersList()
{
    Preferences::instance()->setTorrentLabels(m_labels.keys());
}

void LabelFiltersList::addItem(QString &label, bool hasTorrent)
{
    int labelCount = 0;
    QListWidgetItem *labelItem = 0;
    label = Utils::Fs::toValidFileSystemName(label.trimmed());
    item(0)->setText(tr("All (%1)", "this is for the label filter").arg(m_totalTorrents));

    if (label.isEmpty()) {
        item(1)->setText(tr("Unlabeled (%1)").arg(m_totalTorrents - m_totalLabeled));
        return;
    }

    bool exists = m_labels.contains(label);
    if (exists) {
        labelCount = m_labels.value(label);
        labelItem = item(rowFromLabel(label));
    }
    else {
        labelItem = new QListWidgetItem();
        labelItem->setData(Qt::DecorationRole, GuiIconProvider::instance()->getIcon("inode-directory"));
    }

    if (hasTorrent) {
        ++m_totalLabeled;
        ++labelCount;
    }
    item(1)->setText(tr("Unlabeled (%1)").arg(m_totalTorrents - m_totalLabeled));

    Preferences::instance()->addTorrentLabel(label);
    m_labels.insert(label, labelCount);
    labelItem->setText(tr("%1 (%2)", "label_name (10)").arg(label).arg(labelCount));
    if (exists)
        return;

    Q_ASSERT(count() >= 2);
    for (int i = 2; i<count(); ++i) {
        bool less = false;
        if (!(Utils::String::naturalSort(label, item(i)->text(), less)))
            less = (label.localeAwareCompare(item(i)->text()) < 0);
        if (less) {
            insertItem(i, labelItem);
            updateGeometry();
            return;
        }
    }
    QListWidget::addItem(labelItem);
    updateGeometry();
}

void LabelFiltersList::removeItem(const QString &label)
{
    item(0)->setText(tr("All (%1)", "this is for the label filter").arg(m_totalTorrents));
    if (label.isEmpty()) {
        // In case we here from torrentAboutToBeDeleted()
        item(1)->setText(tr("Unlabeled (%1)").arg(m_totalTorrents - m_totalLabeled));
        return;
    }

    --m_totalLabeled;
    item(1)->setText(tr("Unlabeled (%1)").arg(m_totalTorrents - m_totalLabeled));

    int labelCount = m_labels.value(label) - 1;
    int row = rowFromLabel(label);
    if (row < 2)
        return;

    QListWidgetItem *labelItem = item(row);
    labelItem->setText(tr("%1 (%2)", "label_name (10)").arg(label).arg(labelCount));
    m_labels.insert(label, labelCount);
}

void LabelFiltersList::removeSelectedLabel()
{
    QList<QListWidgetItem*> items = selectedItems();
    if (items.size() == 0) return;

    const int labelRow = row(items.first());
    if (labelRow < 2) return;

    const QString &label = labelFromRow(labelRow);
    Q_ASSERT(m_labels.contains(label));
    m_labels.remove(label);
    // Select first label
    setCurrentRow(0, QItemSelectionModel::SelectCurrent);
    // Un display filter
    delete takeItem(labelRow);
    transferList->removeLabelFromRows(label);
    // Save custom labels to remember it was deleted
    Preferences::instance()->removeTorrentLabel(label);
    updateGeometry();
}

void LabelFiltersList::removeUnusedLabels()
{
    QStringList unusedLabels;
    QHash<QString, int>::const_iterator i;
    for (i = m_labels.begin(); i != m_labels.end(); ++i) {
        if (i.value() == 0)
            unusedLabels << i.key();
    }
    foreach (const QString &label, unusedLabels) {
        m_labels.remove(label);
        delete takeItem(rowFromLabel(label));
        Preferences::instance()->removeTorrentLabel(label);
    }

    if (!unusedLabels.isEmpty())
        updateGeometry();
}

void LabelFiltersList::torrentChangedLabel(BitTorrent::TorrentHandle *const torrent, const QString &oldLabel)
{
    qDebug("Torrent label changed from %s to %s", qPrintable(oldLabel), qPrintable(torrent->label()));
    removeItem(oldLabel);
    QString newLabel = torrent->label();
    addItem(newLabel, true);
}

void LabelFiltersList::showMenu(QPoint)
{
    QMenu menu(this);
    QAction *addAct = menu.addAction(GuiIconProvider::instance()->getIcon("list-add"), tr("Add label..."));
    QAction *removeAct = 0;
    QAction *removeUnusedAct = 0;
    if (!selectedItems().empty() && row(selectedItems().first()) > 1)
        removeAct = menu.addAction(GuiIconProvider::instance()->getIcon("list-remove"), tr("Remove label"));
    removeUnusedAct = menu.addAction(GuiIconProvider::instance()->getIcon("list-remove"), tr("Remove unused labels"));
    menu.addSeparator();
    QAction *startAct = menu.addAction(GuiIconProvider::instance()->getIcon("media-playback-start"), tr("Resume torrents"));
    QAction *pauseAct = menu.addAction(GuiIconProvider::instance()->getIcon("media-playback-pause"), tr("Pause torrents"));
    QAction *deleteTorrentsAct = menu.addAction(GuiIconProvider::instance()->getIcon("edit-delete"), tr("Delete torrents"));
    QAction *act = 0;
    act = menu.exec(QCursor::pos());
    if (!act)
        return;

    if (act == removeAct) {
        removeSelectedLabel();
    }
    else if (act == removeUnusedAct) {
        removeUnusedLabels();
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
        QString label = "";
        bool invalid;
        do {
            invalid = false;
            label = AutoExpandableDialog::getText(this, tr("New Label"), tr("Label:"), QLineEdit::Normal, label, &ok);
            if (ok && !label.isEmpty()) {
                if (Utils::Fs::isValidFileSystemName(label)) {
                    addItem(label, false);
                }
                else {
                    QMessageBox::warning(this, tr("Invalid label name"), tr("Please don't use any special characters in the label name."));
                    invalid = true;
                }
            }
        } while (invalid);
    }
}

void LabelFiltersList::applyFilter(int row)
{
    switch (row) {
    case 0:
        transferList->applyLabelFilterAll();
        break;
    case 1:
        transferList->applyLabelFilter(QString());
        break;
    default:
        transferList->applyLabelFilter(labelFromRow(row));
    }
}

void LabelFiltersList::handleNewTorrent(BitTorrent::TorrentHandle *const torrent)
{
    Q_ASSERT(torrent);
    ++m_totalTorrents;
    QString label = torrent->label();
    addItem(label, true);
    // FIXME: Drop this confusion.
    // labelFilters->addItem() may have changed the label, update the model accordingly.
    torrent->setLabel(label);
}

void LabelFiltersList::torrentAboutToBeDeleted(BitTorrent::TorrentHandle *const torrent)
{
    Q_ASSERT(torrent);
    --m_totalTorrents;
    removeItem(torrent->label());
}

QString LabelFiltersList::labelFromRow(int row) const
{
    Q_ASSERT(row > 1);
    const QString &label = item(row)->text();
    QStringList parts = label.split(" ");
    Q_ASSERT(parts.size() >= 2);
    parts.removeLast(); // Remove trailing number
    return parts.join(" ");
}

int LabelFiltersList::rowFromLabel(const QString &label) const
{
    Q_ASSERT(!label.isEmpty());
    for (int i = 2; i<count(); ++i)
        if (label == labelFromRow(i)) return i;
    return -1;
}

TrackerFiltersList::TrackerFiltersList(QWidget *parent, TransferListWidget *transferList)
    : FiltersBase(parent, transferList)
    , m_totalTorrents(0)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    QListWidgetItem *allTrackers = new QListWidgetItem(this);
    allTrackers->setData(Qt::DisplayRole, QVariant(tr("All (0)", "this is for the label filter")));
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

        if (host != "")
            trackerItem = item(rowFromTracker(host));
        else
            trackerItem = item(1);
    }
    else {
        trackerItem = new QListWidgetItem();
        trackerItem->setData(Qt::DecorationRole, GuiIconProvider::instance()->getIcon("network-server"));

        Net::DownloadHandler *h = Net::DownloadManager::instance()->downloadUrl(QString("http://%1/favicon.ico").arg(host));
        connect(h, SIGNAL(downloadFinished(QString, QString)), this, SLOT(handleFavicoDownload(QString, QString)));
        connect(h, SIGNAL(downloadFailed(QString, QString)), this, SLOT(handleFavicoFailure(QString, QString)));
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

void TrackerFiltersList::handleFavicoDownload(const QString& url, const QString& filePath)
{
    QString host = getHost(url);
    if (!m_trackers.contains(host)) {
        Utils::Fs::forceRemove(filePath);
        return;
    }

    QListWidgetItem *trackerItem = item(rowFromTracker(host));
    QIcon icon(filePath);
    //Detect a non-decodable icon
    QList<QSize> sizes = icon.availableSizes();
    bool invalid = (sizes.size() > 0 ? icon.pixmap(sizes.first()).isNull() : true);
    if (invalid) {
        if (url.endsWith(".ico", Qt::CaseInsensitive)) {
            Logger::instance()->addMessage(tr("Couldn't decode favicon for URL `%1`. Trying to download favicon in PNG format.").arg(url),
                                           Log::WARNING);
            Net::DownloadHandler *h = Net::DownloadManager::instance()->downloadUrl(url.left(url.size() - 4) + ".png");
            connect(h, SIGNAL(downloadFinished(QString, QString)), this, SLOT(handleFavicoDownload(QString, QString)));
            connect(h, SIGNAL(downloadFailed(QString, QString)), this, SLOT(handleFavicoFailure(QString, QString)));
        }
        else {
            Logger::instance()->addMessage(tr("Couldn't decode favicon for URL `%1`.").arg(url), Log::WARNING);
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
    Logger::instance()->addMessage(tr("Couldn't download favicon for URL `%1`. Reason: `%2`").arg(url).arg(error),
                                   Log::WARNING);
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

QString TrackerFiltersList::getHost(const QString &trakcer) const
{
    QUrl url(trakcer);
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

    frame->setFrameShadow(QFrame::Plain);
    frame->setFrameShape(QFrame::NoFrame);
    scroll->setFrameShadow(QFrame::Plain);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QFrame { background: transparent; }");
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    vLayout->setContentsMargins(0, 4, 0, 0);
    frameLayout->setContentsMargins(0, 4, 0, 0);
    frameLayout->setSpacing(2);

    frame->setLayout(frameLayout);
    scroll->setWidget(frame);
    vLayout->addWidget(scroll);
    setLayout(vLayout);
    setContentsMargins(0,0,0,0);

    QCheckBox * statusLabel = new QCheckBox(tr("Status"), this);
    statusLabel->setChecked(pref->getStatusFilterState());
    statusLabel->setFont(font);
    frameLayout->addWidget(statusLabel);

    StatusFiltersWidget *statusFilters = new StatusFiltersWidget(this, transferList);
    frameLayout->addWidget(statusFilters);

    QCheckBox *labelLabel = new QCheckBox(tr("Labels"), this);
    labelLabel->setChecked(pref->getLabelFilterState());
    labelLabel->setFont(font);
    frameLayout->addWidget(labelLabel);

    LabelFiltersList *labelFilters = new LabelFiltersList(this, transferList);
    frameLayout->addWidget(labelFilters);

    QCheckBox *trackerLabel = new QCheckBox(tr("Trackers"), this);
    trackerLabel->setChecked(pref->getTrackerFilterState());
    trackerLabel->setFont(font);
    frameLayout->addWidget(trackerLabel);

    trackerFilters = new TrackerFiltersList(this, transferList);
    frameLayout->addWidget(trackerFilters);

    frameLayout->addStretch();

    connect(statusLabel, SIGNAL(toggled(bool)), statusFilters, SLOT(toggleFilter(bool)));
    connect(statusLabel, SIGNAL(toggled(bool)), pref, SLOT(setStatusFilterState(const bool)));
    connect(labelLabel, SIGNAL(toggled(bool)), labelFilters, SLOT(toggleFilter(bool)));
    connect(labelLabel, SIGNAL(toggled(bool)), pref, SLOT(setLabelFilterState(const bool)));
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
