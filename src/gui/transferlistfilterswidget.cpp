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
#include <QResizeEvent>
#include <QMessageBox>
#include <QLabel>

#include "transferlistdelegate.h"
#include "transferlistwidget.h"
#include "preferences.h"
#include "torrentmodel.h"
#include "iconprovider.h"
#include "fs_utils.h"
#include "autoexpandabledialog.h"
#include "torrentfilterenum.h"
#include "misc.h"
#include "downloadthread.h"
#include "logger.h"

FiltersBase::FiltersBase(QWidget *parent)
    : QListWidget(parent)
{
    setStyleSheet("QListWidget { background: transparent; border: 0 }");
#if defined(Q_OS_MAC)
    setAttribute(Qt::WA_MacShowFocusRect, false);
#endif
}

QSize FiltersBase::sizeHint() const
{
    QSize size = QListWidget::sizeHint();
    // Height should be exactly the height of the content
    size.setHeight((sizeHintForRow(0) * count()) + (2 * frameWidth()) + 6);
    return size;
}

QSize FiltersBase::minimumSizeHint() const
{
    QSize size = QListWidget::minimumSizeHint();
    // Minimum height should be exactly the sticky labels height
    size.setHeight((sizeHintForRow(0) * 2) + (2 * frameWidth()) + 6);
    return size;
}

LabelFiltersList::LabelFiltersList(QWidget *parent)
    : FiltersBase(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

void LabelFiltersList::addItem(QListWidgetItem *it)
{
    Q_ASSERT(count() >= 2);
    for (int i = 2; i<count(); ++i) {
        if (item(i)->text().localeAwareCompare(it->text()) >= 0) {
            insertItem(i, it);
            updateGeometry();
            return;
        }
    }
    QListWidget::addItem(it);
    updateGeometry();
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

int LabelFiltersList::rowFromLabel(QString label) const
{
    Q_ASSERT(!label.isEmpty());
    for (int i = 2; i<count(); ++i)
        if (label == labelFromRow(i)) return i;
    return -1;
}

StatusFiltersWidget::StatusFiltersWidget(QWidget *parent)
    : FiltersBase(parent)
{
    setUniformItemSizes(true);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // Height is fixed (sizeHint().height() is used)
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

TrackerFiltersList::TrackerFiltersList(QWidget *parent)
    : FiltersBase(parent)
    , m_downloader(new DownloadThread(this))
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    QListWidgetItem *allTrackers = new QListWidgetItem(this);
    allTrackers->setData(Qt::DisplayRole, QVariant(tr("All trackers (0)")));
    allTrackers->setData(Qt::DecorationRole, IconProvider::instance()->getIcon("network-server"));
    QListWidgetItem *noTracker = new QListWidgetItem(this);
    noTracker->setData(Qt::DisplayRole, QVariant(tr("Trackerless (0)")));
    noTracker->setData(Qt::DecorationRole, IconProvider::instance()->getIcon("network-server"));
    m_trackers.insert("", QStringList());
    connect(m_downloader, SIGNAL(downloadFinished(QString, QString)), SLOT(handleFavicoDownload(QString, QString)));
    connect(m_downloader, SIGNAL(downloadFailure(QString, QString)), SLOT(handleFavicoFailure(QString, QString)));
}

TrackerFiltersList::~TrackerFiltersList()
{
    delete m_downloader;
    foreach (const QString &iconPath, m_iconPaths)
        fsutils::forceRemove(iconPath);
}

void TrackerFiltersList::addItem(const QString &tracker, const QString &hash)
{
    QStringList tmp;
    QListWidgetItem *trackerItem = 0;
    QString host = getHost(tracker);

    if (m_trackers.contains(host)) {
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
        trackerItem->setData(Qt::DecorationRole, IconProvider::instance()->getIcon("network-server"));
        m_downloader->downloadUrl(QString("http://") + host + QString("/favicon.ico"));
    }

    tmp.append(hash);
    m_trackers.insert(host, tmp);
    if (host == "") {
        trackerItem->setText(tr("Trackerless (%1)").arg(tmp.size()));
        if (currentRow() == 1)
            emit currentRowChanged(1);
        return;
    }

    trackerItem->setText(tr("%1 (%2)", "openbittorrent.com (10)").arg(host).arg(tmp.size()));

    Q_ASSERT(count() >= 2);
    for (int i = 2; i<count(); ++i) {
        bool less = false;
        if (!(misc::naturalSort(host, item(i)->text(), less)))
            less = (host.localeAwareCompare(item(i)->text()) < 0);
        if (less) {
            insertItem(i, trackerItem);
            if (currentRow() == i)
                emit currentRowChanged(i);
            return;
        }
    }
    QListWidget::addItem(trackerItem);
}

void TrackerFiltersList::addItem(const QTorrentHandle& handle)
{
    QString hash = handle.hash();
    std::vector<libtorrent::announce_entry> trackers = handle.trackers();
    for (std::vector<libtorrent::announce_entry>::iterator i = trackers.begin(), e = trackers.end(); i != e; ++i)
        addItem(misc::toQStringU(i->url), hash);

    //Check for trackerless torrent
    if (trackers.size() == 0)
        addItem("", hash);
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
        row = rowFromTracker(host);
        trackerItem = item(row);
        if (tmp.empty()) {
            if (currentRow() == row)
                setCurrentRow(0);
            delete trackerItem;
            m_trackers.remove(host);
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
        emit currentRowChanged(row);
}

void TrackerFiltersList::removeItem(const QTorrentHandle& handle)
{
    QString hash = handle.hash();
    std::vector<libtorrent::announce_entry> trackers = handle.trackers();
    for (std::vector<libtorrent::announce_entry>::iterator i = trackers.begin(), e = trackers.end(); i != e; ++i)
        removeItem(misc::toQStringU(i->url), hash);

    //Check for trackerless torrent
    if (trackers.size() == 0)
        removeItem("", hash);
}

void TrackerFiltersList::setTorrentCount(int all)
{
    item(0)->setText(tr("All trackers (%1)").arg(all));
}

QStringList TrackerFiltersList::getHashes(int row)
{
    if (row == 1)
        return m_trackers.value("");
    else
        return m_trackers.value(trackerFromRow(row));
}

void TrackerFiltersList::handleFavicoDownload(const QString& url, const QString& filePath)
{
    QString host = getHost(url);
    if (!m_trackers.contains(host))
        return;

    QListWidgetItem *trackerItem = item(rowFromTracker(host));
    QIcon icon(filePath);
    //Detect a non-decodable icon
    bool invalid = icon.pixmap(icon.availableSizes().first()).isNull();
    if (invalid) {
        if (url.endsWith(".ico", Qt::CaseInsensitive)) {
            Logger::instance()->addMessage(tr("Couldn't decode favico for url `%1`. Trying to download favico in PNG format.").arg(url),
                                           Log::WARNING);
            m_downloader->downloadUrl(url.left(url.size() - 4) + ".png");
        }
        else {
            Logger::instance()->addMessage(tr("Couldn't decode favico for url `%1`.").arg(url), Log::WARNING);
        }
        fsutils::forceRemove(filePath);
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
    Logger::instance()->addMessage(tr("Couldn't download favico for url `%1`. Reason: `%2`").arg(url).arg(error),
                                   Log::WARNING);
}

void TrackerFiltersList::changeTrackerless(bool trackerless, const QString &hash)
{
    if (trackerless)
        addItem("", hash);
    else
        removeItem("", hash);
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
    for (int i = 2; i<count(); ++i)
        if (tracker == trackerFromRow(i)) return i;
    return -1;
}

QString TrackerFiltersList::getHost(const QString &trakcer) const
{
    QUrl url(trakcer);
    QString longHost = url.host();
    QString tld = url.topLevelDomain();
    // We want the domain + tld. Subdomains should be disregarded
    int index = longHost.lastIndexOf('.', -(tld.size() + 1));
    if (index == -1)
        return longHost;
    return longHost.mid(index + 1);
}

TransferListFiltersWidget::TransferListFiltersWidget(QWidget *parent, TransferListWidget *transferList): QFrame(parent), transferList(transferList), nb_labeled(0), nb_torrents(0)
{
    // Construct lists
    vLayout = new QVBoxLayout();
    vLayout->setContentsMargins(0, 4, 0, 0);
    QFont font;
    font.setBold(true);
    font.setCapitalization(QFont::SmallCaps);
    torrentsLabel = new QLabel(tr("Torrents"));
    torrentsLabel->setIndent(2);
    torrentsLabel->setFont(font);
    vLayout->addWidget(torrentsLabel);
    statusFilters = new StatusFiltersWidget(this);
    vLayout->addWidget(statusFilters);
    QLabel *labelsLabel = new QLabel(tr("Labels"));
    labelsLabel->setIndent(2);
    labelsLabel->setFont(font);
    vLayout->addWidget(labelsLabel);
    labelFilters = new LabelFiltersList(this);
    vLayout->addWidget(labelFilters);
    QLabel *trackersLabel = new QLabel(tr("Trackers"));
    trackersLabel->setIndent(2);
    trackersLabel->setFont(font);
    vLayout->addWidget(trackersLabel);
    trackerFilters = new TrackerFiltersList(this);
    vLayout->addWidget(trackerFilters);
    setLayout(vLayout);
    labelFilters->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    trackerFilters->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    statusFilters->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    statusFilters->setSpacing(0);
    setContentsMargins(0,0,0,0);
    vLayout->setSpacing(2);
    // Add status filters
    QListWidgetItem *all = new QListWidgetItem(statusFilters);
    all->setData(Qt::DisplayRole, QVariant(tr("All") + " (0)"));
    all->setData(Qt::DecorationRole, QIcon(":/icons/skin/filterall.png"));
    QListWidgetItem *downloading = new QListWidgetItem(statusFilters);
    downloading->setData(Qt::DisplayRole, QVariant(tr("Downloading") + " (0)"));
    downloading->setData(Qt::DecorationRole, QIcon(":/icons/skin/downloading.png"));
    QListWidgetItem *completed = new QListWidgetItem(statusFilters);
    completed->setData(Qt::DisplayRole, QVariant(tr("Completed") + " (0)"));
    completed->setData(Qt::DecorationRole, QIcon(":/icons/skin/uploading.png"));
    QListWidgetItem *resumed = new QListWidgetItem(statusFilters);
    resumed->setData(Qt::DisplayRole, QVariant(tr("Resumed") + " (0)"));
    resumed->setData(Qt::DecorationRole, QIcon(":/icons/skin/resumed.png"));
    QListWidgetItem *paused = new QListWidgetItem(statusFilters);
    paused->setData(Qt::DisplayRole, QVariant(tr("Paused") + " (0)"));
    paused->setData(Qt::DecorationRole, QIcon(":/icons/skin/paused.png"));
    QListWidgetItem *active = new QListWidgetItem(statusFilters);
    active->setData(Qt::DisplayRole, QVariant(tr("Active") + " (0)"));
    active->setData(Qt::DecorationRole, QIcon(":/icons/skin/filteractive.png"));
    QListWidgetItem *inactive = new QListWidgetItem(statusFilters);
    inactive->setData(Qt::DisplayRole, QVariant(tr("Inactive") + " (0)"));
    inactive->setData(Qt::DecorationRole, QIcon(":/icons/skin/filterinactive.png"));

    // SIGNAL/SLOT
    connect(statusFilters, SIGNAL(currentRowChanged(int)), transferList, SLOT(applyStatusFilter(int)));
    connect(transferList->getSourceModel(), SIGNAL(modelRefreshed()), SLOT(updateTorrentNumbers()));
    connect(transferList->getSourceModel(), SIGNAL(torrentAdded(TorrentModelItem*)), SLOT(handleNewTorrent(TorrentModelItem*)));
    connect(labelFilters, SIGNAL(currentRowChanged(int)), this, SLOT(applyLabelFilter(int)));
    connect(trackerFilters, SIGNAL(currentRowChanged(int)), this, SLOT(applyTrackerFilter(int)));
    connect(transferList->getSourceModel(), SIGNAL(torrentAboutToBeRemoved(TorrentModelItem*)), SLOT(torrentAboutToBeDeleted(TorrentModelItem*)));
    connect(transferList->getSourceModel(), SIGNAL(torrentChangedLabel(TorrentModelItem*,QString,QString)), SLOT(torrentChangedLabel(TorrentModelItem*, QString, QString)));

    // Add Label filters
    QListWidgetItem *allLabels = new QListWidgetItem(labelFilters);
    allLabels->setData(Qt::DisplayRole, QVariant(tr("All labels") + " (0)"));
    allLabels->setData(Qt::DecorationRole, IconProvider::instance()->getIcon("inode-directory"));
    QListWidgetItem *noLabel = new QListWidgetItem(labelFilters);
    noLabel->setData(Qt::DisplayRole, QVariant(tr("Unlabeled") + " (0)"));
    noLabel->setData(Qt::DecorationRole, IconProvider::instance()->getIcon("inode-directory"));

    // Load settings
    loadSettings();

    labelFilters->setCurrentRow(0);
    trackerFilters->setCurrentRow(0);
    //labelFilters->selectionModel()->select(labelFilters->model()->index(0,0), QItemSelectionModel::Select);

    // Label menu
    labelFilters->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(labelFilters, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showLabelMenu(QPoint)));

    // Tracker menu
    trackerFilters->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(trackerFilters, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showTrackerMenu(QPoint)));
}

TransferListFiltersWidget::~TransferListFiltersWidget()
{
    saveSettings();
    delete statusFilters;
    delete labelFilters;
    delete trackerFilters;
    delete vLayout;
}

void TransferListFiltersWidget::resizeEvent(QResizeEvent *event)
{
    int height = event->size().height();
    int minHeight = statusFilters->height() + (3 * torrentsLabel->height());
    trackerFilters->setMinimumHeight((height - minHeight) / 2);
}

StatusFiltersWidget* TransferListFiltersWidget::getStatusFilters() const
{
    return statusFilters;
}

void TransferListFiltersWidget::saveSettings() const
{
    Preferences* const pref = Preferences::instance();
    pref->setTransSelFilter(statusFilters->currentRow());
    pref->setTorrentLabels(customLabels.keys());
}

void TransferListFiltersWidget::loadSettings()
{
    statusFilters->setCurrentRow(Preferences::instance()->getTransSelFilter());
    const QStringList label_list = Preferences::instance()->getTorrentLabels();
    foreach (const QString &label, label_list) {
        customLabels.insert(label, 0);
        qDebug("Creating label QListWidgetItem: %s", qPrintable(label));
        QListWidgetItem *newLabel = new QListWidgetItem();
        newLabel->setText(label + " (0)");
        newLabel->setData(Qt::DecorationRole, IconProvider::instance()->getIcon("inode-directory"));
        labelFilters->addItem(newLabel);
    }
}

void TransferListFiltersWidget::updateTorrentNumbers()
{
    const TorrentStatusReport report = transferList->getSourceModel()->getTorrentStatusReport();
    statusFilters->item(TorrentFilter::ALL)->setData(Qt::DisplayRole, QVariant(tr("All") + " (" + QString::number(report.nb_active + report.nb_inactive) + ")"));
    statusFilters->item(TorrentFilter::DOWNLOADING)->setData(Qt::DisplayRole, QVariant(tr("Downloading") + " (" + QString::number(report.nb_downloading) + ")"));
    statusFilters->item(TorrentFilter::COMPLETED)->setData(Qt::DisplayRole, QVariant(tr("Completed") + " (" + QString::number(report.nb_seeding) + ")"));
    statusFilters->item(TorrentFilter::PAUSED)->setData(Qt::DisplayRole, QVariant(tr("Paused") + " (" + QString::number(report.nb_paused) + ")"));
    statusFilters->item(TorrentFilter::RESUMED)->setData(Qt::DisplayRole, QVariant(tr("Resumed") + " (" + QString::number(report.nb_active + report.nb_inactive - report.nb_paused) + ")"));
    statusFilters->item(TorrentFilter::ACTIVE)->setData(Qt::DisplayRole, QVariant(tr("Active") + " (" + QString::number(report.nb_active) + ")"));
    statusFilters->item(TorrentFilter::INACTIVE)->setData(Qt::DisplayRole, QVariant(tr("Inactive") + " (" + QString::number(report.nb_inactive) + ")"));
}

void TransferListFiltersWidget::addLabel(QString& label)
{
    label = fsutils::toValidFileSystemName(label.trimmed());
    if (label.isEmpty() || customLabels.contains(label)) return;
    QListWidgetItem *newLabel = new QListWidgetItem();
    newLabel->setText(label + " (0)");
    newLabel->setData(Qt::DecorationRole, IconProvider::instance()->getIcon("inode-directory"));
    labelFilters->addItem(newLabel);
    customLabels.insert(label, 0);
    Preferences::instance()->addTorrentLabel(label);
}

void TransferListFiltersWidget::addTrackers(const QStringList &trackers, const QString &hash)
{
    foreach (const QString &tracker, trackers)
        trackerFilters->addItem(tracker, hash);
}

void TransferListFiltersWidget::removeTrackers(const QStringList &trackers, const QString &hash)
{
    foreach (const QString &tracker, trackers)
        trackerFilters->removeItem(tracker, hash);
}

void TransferListFiltersWidget::changeTrackerless(bool trackerless, const QString &hash)
{
    trackerFilters->changeTrackerless(trackerless, hash);
}

void TransferListFiltersWidget::showLabelMenu(QPoint)
{
    QMenu labelMenu(labelFilters);
    QAction *addAct = labelMenu.addAction(IconProvider::instance()->getIcon("list-add"), tr("Add label..."));
    QAction *removeAct = 0;
    QAction *removeUnusedAct = 0;
    if (!labelFilters->selectedItems().empty() && labelFilters->row(labelFilters->selectedItems().first()) > 1)
        removeAct = labelMenu.addAction(IconProvider::instance()->getIcon("list-remove"), tr("Remove label"));
    else
        removeUnusedAct = labelMenu.addAction(IconProvider::instance()->getIcon("list-remove"), tr("Remove unused labels"));
    labelMenu.addSeparator();
    QAction *startAct = labelMenu.addAction(IconProvider::instance()->getIcon("media-playback-start"), tr("Resume torrents"));
    QAction *pauseAct = labelMenu.addAction(IconProvider::instance()->getIcon("media-playback-pause"), tr("Pause torrents"));
    QAction *deleteTorrentsAct = labelMenu.addAction(IconProvider::instance()->getIcon("edit-delete"), tr("Delete torrents"));
    QAction *act = 0;
    act = labelMenu.exec(QCursor::pos());
    if (act) {
        if (act == removeAct) {
            removeSelectedLabel();
            return;
        }
        if (act == removeUnusedAct) {
            removeUnusedLabels();
            return;
        }
        if (act == deleteTorrentsAct) {
            transferList->deleteVisibleTorrents();
            return;
        }
        if (act == startAct) {
            transferList->startVisibleTorrents();
            return;
        }
        if (act == pauseAct) {
            transferList->pauseVisibleTorrents();
            return;
        }
        if (act == addAct) {
            bool ok;
            QString label = "";
            bool invalid;
            do {
                invalid = false;
                label = AutoExpandableDialog::getText(this, tr("New Label"), tr("Label:"), QLineEdit::Normal, label, &ok);
                if (ok && !label.isEmpty()) {
                    if (fsutils::isValidFileSystemName(label)) {
                        addLabel(label);
                    }
                    else {
                        QMessageBox::warning(this, tr("Invalid label name"), tr("Please don't use any special characters in the label name."));
                        invalid = true;
                    }
                }
            } while (invalid);
            return;
        }
    }
}

void TransferListFiltersWidget::showTrackerMenu(QPoint)
{
    QMenu trackerMenu(trackerFilters);
    QAction *startAct = trackerMenu.addAction(IconProvider::instance()->getIcon("media-playback-start"), tr("Resume torrents"));
    QAction *pauseAct = trackerMenu.addAction(IconProvider::instance()->getIcon("media-playback-pause"), tr("Pause torrents"));
    QAction *deleteTorrentsAct = trackerMenu.addAction(IconProvider::instance()->getIcon("edit-delete"), tr("Delete torrents"));
    QAction *act = 0;
    act = trackerMenu.exec(QCursor::pos());

    if (act) {
        if (act == startAct)
            transferList->startVisibleTorrents();
        else if (act == pauseAct)
            transferList->pauseVisibleTorrents();
        else if (act == deleteTorrentsAct)
            transferList->deleteVisibleTorrents();
    }
}

void TransferListFiltersWidget::removeSelectedLabel()
{
    const int row = labelFilters->row(labelFilters->selectedItems().first());
    Q_ASSERT(row > 1);
    const QString &label = labelFilters->labelFromRow(row);
    Q_ASSERT(customLabels.contains(label));
    customLabels.remove(label);
    transferList->removeLabelFromRows(label);
    // Select first label
    labelFilters->setCurrentItem(labelFilters->item(0));
    labelFilters->selectionModel()->select(labelFilters->model()->index(0,0), QItemSelectionModel::Select);
    applyLabelFilter(0);
    // Un display filter
    delete labelFilters->takeItem(row);
    labelFilters->updateGeometry();
    // Save custom labels to remember it was deleted
    Preferences::instance()->removeTorrentLabel(label);
}

void TransferListFiltersWidget::removeUnusedLabels()
{
    QStringList unusedLabels;
    QHash<QString, int>::const_iterator i;
    for (i = customLabels.begin(); i != customLabels.end(); ++i)
        if (i.value() == 0)
            unusedLabels << i.key();
    foreach (const QString &label, unusedLabels) {
        customLabels.remove(label);
        delete labelFilters->takeItem(labelFilters->rowFromLabel(label));
        Preferences::instance()->removeTorrentLabel(label);
    }
    if (!unusedLabels.empty())
        labelFilters->updateGeometry();
}

void TransferListFiltersWidget::applyLabelFilter(int row)
{
    switch (row) {
    case 0:
        transferList->applyLabelFilterAll();
        break;
    case 1:
        transferList->applyLabelFilter(QString());
        break;
    default:
        transferList->applyLabelFilter(labelFilters->labelFromRow(row));
    }
}

void TransferListFiltersWidget::applyTrackerFilter(int row)
{
    if (row == 0)
        transferList->applyTrackerFilterAll();
    else
        transferList->applyTrackerFilter(trackerFilters->getHashes(row));
}

void TransferListFiltersWidget::torrentChangedLabel(TorrentModelItem *torrentItem, QString old_label, QString new_label)
{
    Q_UNUSED(torrentItem);
    qDebug("Torrent label changed from %s to %s", qPrintable(old_label), qPrintable(new_label));
    if (!old_label.isEmpty()) {
        if (customLabels.contains(old_label)) {
            const int new_count = customLabels.value(old_label, 0) - 1;
            Q_ASSERT(new_count >= 0);
            customLabels.insert(old_label, new_count);
            const int row = labelFilters->rowFromLabel(old_label);
            Q_ASSERT(row >= 2);
            labelFilters->item(row)->setText(old_label + " (" + QString::number(new_count) + ")");
        }
        --nb_labeled;
    }
    if (!new_label.isEmpty()) {
        if (!customLabels.contains(new_label))
            addLabel(new_label);
        const int new_count = customLabels.value(new_label, 0) + 1;
        Q_ASSERT(new_count >= 1);
        customLabels.insert(new_label, new_count);
        const int row = labelFilters->rowFromLabel(new_label);
        Q_ASSERT(row >= 2);
        labelFilters->item(row)->setText(new_label + " (" + QString::number(new_count) + ")");
        ++nb_labeled;
    }
    updateStickyLabelCounters();
}

void TransferListFiltersWidget::handleNewTorrent(TorrentModelItem* torrentItem)
{
    QString label = torrentItem->data(TorrentModelItem::TR_LABEL).toString();
    qDebug("New torrent was added with label: %s", qPrintable(label));
    if (!label.isEmpty()) {
        if (!customLabels.contains(label)) {
            addLabel(label);
            // addLabel may have changed the label, update the model accordingly.
            torrentItem->setData(TorrentModelItem::TR_LABEL, label);
        }
        // Update label counter
        Q_ASSERT(customLabels.contains(label));
        const int new_count = customLabels.value(label, 0) + 1;
        customLabels.insert(label, new_count);
        const int row = labelFilters->rowFromLabel(label);
        qDebug("torrentAdded, Row: %d", row);
        Q_ASSERT(row >= 2);
        Q_ASSERT(labelFilters->item(row));
        labelFilters->item(row)->setText(label + " (" + QString::number(new_count) + ")");
        ++nb_labeled;
    }
    ++nb_torrents;
    Q_ASSERT(nb_torrents >= 0);
    Q_ASSERT(nb_labeled >= 0);
    Q_ASSERT(nb_labeled <= nb_torrents);
    updateStickyLabelCounters();
    trackerFilters->addItem(torrentItem->torrentHandle());
    trackerFilters->setTorrentCount(nb_torrents);
}

void TransferListFiltersWidget::torrentAboutToBeDeleted(TorrentModelItem* torrentItem)
{
    Q_ASSERT(torrentItem);
    QString label = torrentItem->data(TorrentModelItem::TR_LABEL).toString();
    if (!label.isEmpty()) {
        // Update label counter
        const int new_count = customLabels.value(label, 0) - 1;
        customLabels.insert(label, new_count);
        const int row = labelFilters->rowFromLabel(label);
        Q_ASSERT(row >= 2);
        labelFilters->item(row)->setText(label + " (" + QString::number(new_count) + ")");
        --nb_labeled;
    }
    --nb_torrents;
    qDebug("nb_torrents: %d, nb_labeled: %d", nb_torrents, nb_labeled);
    Q_ASSERT(nb_torrents >= 0);
    Q_ASSERT(nb_labeled >= 0);
    Q_ASSERT(nb_labeled <= nb_torrents);
    updateStickyLabelCounters();
    trackerFilters->removeItem(torrentItem->torrentHandle());
    trackerFilters->setTorrentCount(nb_torrents);
}

void TransferListFiltersWidget::updateStickyLabelCounters()
{
    labelFilters->item(0)->setText(tr("All labels") + " (" + QString::number(nb_torrents) + ")");
    labelFilters->item(1)->setText(tr("Unlabeled") + " (" + QString::number(nb_torrents - nb_labeled) + ")");
}
