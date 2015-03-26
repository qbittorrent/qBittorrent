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

    connect(this, SIGNAL(customContextMenuRequested(QPoint)), SLOT(showMenu(QPoint)));
    connect(this, SIGNAL(currentRowChanged(int)), SLOT(applyFilter(int)));
    connect(transferList->getSourceModel(), SIGNAL(torrentAdded(TorrentModelItem*)), SLOT(handleNewTorrent(TorrentModelItem*)));
    connect(transferList->getSourceModel(), SIGNAL(torrentAboutToBeRemoved(TorrentModelItem*)), SLOT(torrentAboutToBeDeleted(TorrentModelItem*)));
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

StatusFiltersWidget::StatusFiltersWidget(QWidget *parent, TransferListWidget *transferList)
    : FiltersBase(parent, transferList)
{
    setUniformItemSizes(true);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // Height is fixed (sizeHint().height() is used)
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setSpacing(0);
    connect(transferList->getSourceModel(), SIGNAL(modelRefreshed()), SLOT(updateTorrentNumbers()));

    // Add status filters
    QListWidgetItem *all = new QListWidgetItem(this);
    all->setData(Qt::DisplayRole, QVariant(tr("All") + " (0)"));
    all->setData(Qt::DecorationRole, QIcon(":/icons/skin/filterall.png"));
    QListWidgetItem *downloading = new QListWidgetItem(this);
    downloading->setData(Qt::DisplayRole, QVariant(tr("Downloading") + " (0)"));
    downloading->setData(Qt::DecorationRole, QIcon(":/icons/skin/downloading.png"));
    QListWidgetItem *completed = new QListWidgetItem(this);
    completed->setData(Qt::DisplayRole, QVariant(tr("Completed") + " (0)"));
    completed->setData(Qt::DecorationRole, QIcon(":/icons/skin/uploading.png"));
    QListWidgetItem *resumed = new QListWidgetItem(this);
    resumed->setData(Qt::DisplayRole, QVariant(tr("Resumed") + " (0)"));
    resumed->setData(Qt::DecorationRole, QIcon(":/icons/skin/resumed.png"));
    QListWidgetItem *paused = new QListWidgetItem(this);
    paused->setData(Qt::DisplayRole, QVariant(tr("Paused") + " (0)"));
    paused->setData(Qt::DecorationRole, QIcon(":/icons/skin/paused.png"));
    QListWidgetItem *active = new QListWidgetItem(this);
    active->setData(Qt::DisplayRole, QVariant(tr("Active") + " (0)"));
    active->setData(Qt::DecorationRole, QIcon(":/icons/skin/filteractive.png"));
    QListWidgetItem *inactive = new QListWidgetItem(this);
    inactive->setData(Qt::DisplayRole, QVariant(tr("Inactive") + " (0)"));
    inactive->setData(Qt::DecorationRole, QIcon(":/icons/skin/filterinactive.png"));

    setCurrentRow(Preferences::instance()->getTransSelFilter(), QItemSelectionModel::SelectCurrent);
}

StatusFiltersWidget::~StatusFiltersWidget()
{
    Preferences::instance()->setTransSelFilter(currentRow());
}

void StatusFiltersWidget::updateTorrentNumbers()
{
    const TorrentStatusReport report = transferList->getSourceModel()->getTorrentStatusReport();
    item(TorrentFilter::ALL)->setData(Qt::DisplayRole, QVariant(tr("All (%1)").arg(report.nb_active + report.nb_inactive)));
    item(TorrentFilter::DOWNLOADING)->setData(Qt::DisplayRole, QVariant(tr("Downloading (%1)").arg(report.nb_downloading)));
    item(TorrentFilter::COMPLETED)->setData(Qt::DisplayRole, QVariant(tr("Completed (%1)").arg(report.nb_seeding)));
    item(TorrentFilter::PAUSED)->setData(Qt::DisplayRole, QVariant(tr("Paused (%1)").arg(report.nb_paused)));
    item(TorrentFilter::RESUMED)->setData(Qt::DisplayRole, QVariant(tr("Resumed (%1)").arg(report.nb_active + report.nb_inactive - report.nb_paused)));
    item(TorrentFilter::ACTIVE)->setData(Qt::DisplayRole, QVariant(tr("Active (%1)").arg(report.nb_active)));
    item(TorrentFilter::INACTIVE)->setData(Qt::DisplayRole, QVariant(tr("Inactive (%1)").arg(report.nb_inactive)));
}

void StatusFiltersWidget::showMenu(QPoint) {}

void StatusFiltersWidget::applyFilter(int row)
{
    transferList->applyStatusFilter(row);
}

void StatusFiltersWidget::handleNewTorrent(TorrentModelItem*) {}

void StatusFiltersWidget::torrentAboutToBeDeleted(TorrentModelItem*) {}

LabelFiltersList::LabelFiltersList(QWidget *parent, TransferListWidget *transferList)
    : FiltersBase(parent, transferList)
    , m_totalTorrents(0)
    , m_totalLabeled(0)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    connect(transferList->getSourceModel(), SIGNAL(torrentChangedLabel(TorrentModelItem*,QString,QString)), SLOT(torrentChangedLabel(TorrentModelItem*, QString, QString)));

    // Add Label filters
    QListWidgetItem *allLabels = new QListWidgetItem(this);
    allLabels->setData(Qt::DisplayRole, QVariant(tr("All (0)", "this is for the label filter")));
    allLabels->setData(Qt::DecorationRole, IconProvider::instance()->getIcon("inode-directory"));
    QListWidgetItem *noLabel = new QListWidgetItem(this);
    noLabel->setData(Qt::DisplayRole, QVariant(tr("Unlabeled (0)")));
    noLabel->setData(Qt::DecorationRole, IconProvider::instance()->getIcon("inode-directory"));

    QStringList labelList = Preferences::instance()->getTorrentLabels();
    for (int i=0; i < labelList.size(); ++i)
        addItem(labelList[i], false);

    setCurrentRow(0, QItemSelectionModel::SelectCurrent);
}

LabelFiltersList::~LabelFiltersList()
{
    Preferences::instance()->setTorrentLabels(m_labels.keys());
}

void LabelFiltersList::addItem(QString &label, bool hasTorrent)
{
    int labelCount = 0;
    QListWidgetItem *labelItem = 0;
    label = fsutils::toValidFileSystemName(label.trimmed());
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
        labelItem->setData(Qt::DecorationRole, IconProvider::instance()->getIcon("inode-directory"));
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
        if (!(misc::naturalSort(label, item(i)->text(), less)))
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
    const int labelRow = row(selectedItems().first());
    if (labelRow < 2)
        return;
    const QString &label = labelFromRow(labelRow);
    Q_ASSERT(m_labels.contains(label));
    m_labels.remove(label);
    // Select first label
    setCurrentRow(0, QItemSelectionModel::SelectCurrent);
    applyFilter(0);
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

void LabelFiltersList::torrentChangedLabel(TorrentModelItem *torrentItem, QString old_label, QString new_label)
{
    Q_UNUSED(torrentItem);
    qDebug("Torrent label changed from %s to %s", qPrintable(old_label), qPrintable(new_label));
    removeItem(old_label);
    addItem(new_label, true);
}

void LabelFiltersList::showMenu(QPoint)
{
    QMenu menu(this);
    QAction *addAct = menu.addAction(IconProvider::instance()->getIcon("list-add"), tr("Add label..."));
    QAction *removeAct = 0;
    QAction *removeUnusedAct = 0;
    if (!selectedItems().empty() && row(selectedItems().first()) > 1)
        removeAct = menu.addAction(IconProvider::instance()->getIcon("list-remove"), tr("Remove label"));
    removeUnusedAct = menu.addAction(IconProvider::instance()->getIcon("list-remove"), tr("Remove unused labels"));
    menu.addSeparator();
    QAction *startAct = menu.addAction(IconProvider::instance()->getIcon("media-playback-start"), tr("Resume torrents"));
    QAction *pauseAct = menu.addAction(IconProvider::instance()->getIcon("media-playback-pause"), tr("Pause torrents"));
    QAction *deleteTorrentsAct = menu.addAction(IconProvider::instance()->getIcon("edit-delete"), tr("Delete torrents"));
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
                if (fsutils::isValidFileSystemName(label)) {
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

void LabelFiltersList::handleNewTorrent(TorrentModelItem* torrentItem)
{
    ++m_totalTorrents;
    QString label = torrentItem->data(TorrentModelItem::TR_LABEL).toString();
    addItem(label, true);
    // labelFilters->addItem() may have changed the label, update the model accordingly.
    torrentItem->setData(TorrentModelItem::TR_LABEL, label);
}

void LabelFiltersList::torrentAboutToBeDeleted(TorrentModelItem* torrentItem)
{
    --m_totalTorrents;
    Q_ASSERT(torrentItem);
    QString label = torrentItem->data(TorrentModelItem::TR_LABEL).toString();
    removeItem(label);
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
    , m_downloader(new DownloadThread(this))
    , m_totalTorrents(0)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    QListWidgetItem *allTrackers = new QListWidgetItem(this);
    allTrackers->setData(Qt::DisplayRole, QVariant(tr("All (0)", "this is for the label filter")));
    allTrackers->setData(Qt::DecorationRole, IconProvider::instance()->getIcon("network-server"));
    QListWidgetItem *noTracker = new QListWidgetItem(this);
    noTracker->setData(Qt::DisplayRole, QVariant(tr("Trackerless (0)")));
    noTracker->setData(Qt::DecorationRole, IconProvider::instance()->getIcon("network-server"));
    m_trackers.insert("", QStringList());

    setCurrentRow(0, QItemSelectionModel::SelectCurrent);
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
    if (exists) {
        if (currentRow() == rowFromTracker(host))
            emit currentRowChanged(currentRow());
        return;
    }

    Q_ASSERT(count() >= 2);
    for (int i = 2; i<count(); ++i) {
        bool less = false;
        if (!(misc::naturalSort(host, item(i)->text(), less)))
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
        emit currentRowChanged(row);
}

void TrackerFiltersList::changeTrackerless(bool trackerless, const QString &hash)
{
    if (trackerless)
        addItem("", hash);
    else
        removeItem("", hash);
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

void TrackerFiltersList::showMenu(QPoint)
{
    QMenu menu(this);
    QAction *startAct = menu.addAction(IconProvider::instance()->getIcon("media-playback-start"), tr("Resume torrents"));
    QAction *pauseAct = menu.addAction(IconProvider::instance()->getIcon("media-playback-pause"), tr("Pause torrents"));
    QAction *deleteTorrentsAct = menu.addAction(IconProvider::instance()->getIcon("edit-delete"), tr("Delete torrents"));
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
    else
        transferList->applyTrackerFilter(getHashes(row));
}

void TrackerFiltersList::handleNewTorrent(TorrentModelItem* torrentItem)
{
    QTorrentHandle handle = torrentItem->torrentHandle();
    QString hash = handle.hash();
    std::vector<libtorrent::announce_entry> trackers = handle.trackers();
    for (std::vector<libtorrent::announce_entry>::iterator i = trackers.begin(), e = trackers.end(); i != e; ++i)
        addItem(misc::toQStringU(i->url), hash);

    //Check for trackerless torrent
    if (trackers.size() == 0)
        addItem("", hash);

    item(0)->setText(tr("All (%1)", "this is for the tracker filter").arg(++m_totalTorrents));
}

void TrackerFiltersList::torrentAboutToBeDeleted(TorrentModelItem* torrentItem)
{
    QTorrentHandle handle = torrentItem->torrentHandle();
    QString hash = handle.hash();
    std::vector<libtorrent::announce_entry> trackers = handle.trackers();
    for (std::vector<libtorrent::announce_entry>::iterator i = trackers.begin(), e = trackers.end(); i != e; ++i)
        removeItem(misc::toQStringU(i->url), hash);

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

QStringList TrackerFiltersList::getHashes(int row)
{
    if (row == 1)
        return m_trackers.value("");
    else
        return m_trackers.value(trackerFromRow(row));
}

TransferListFiltersWidget::TransferListFiltersWidget(QWidget *parent, TransferListWidget *transferList)
    : QFrame(parent)
    , statusFilters(0)
    , trackerFilters(0)
    , torrentsLabel(0)
{
    // Construct lists
    QVBoxLayout *vLayout = new QVBoxLayout(this);
    vLayout->setContentsMargins(0, 4, 0, 0);
    QFont font;
    font.setBold(true);
    font.setCapitalization(QFont::SmallCaps);
    torrentsLabel = new QLabel(tr("Torrents"), this);
    torrentsLabel->setIndent(2);
    torrentsLabel->setFont(font);
    vLayout->addWidget(torrentsLabel);
    statusFilters = new StatusFiltersWidget(this, transferList);
    vLayout->addWidget(statusFilters);
    QLabel *labelsLabel = new QLabel(tr("Labels"), this);
    labelsLabel->setIndent(2);
    labelsLabel->setFont(font);
    vLayout->addWidget(labelsLabel);
    LabelFiltersList *labelFilters = new LabelFiltersList(this, transferList);
    vLayout->addWidget(labelFilters);
    QLabel *trackersLabel = new QLabel(tr("Trackers"), this);
    trackersLabel->setIndent(2);
    trackersLabel->setFont(font);
    vLayout->addWidget(trackersLabel);
    trackerFilters = new TrackerFiltersList(this, transferList);
    vLayout->addWidget(trackerFilters);
    setLayout(vLayout);
    setContentsMargins(0,0,0,0);
    vLayout->setSpacing(2);
}

void TransferListFiltersWidget::resizeEvent(QResizeEvent *event)
{
    int height = event->size().height();
    int minHeight = statusFilters->height() + (3 * torrentsLabel->height());
    trackerFilters->setMinimumHeight((height - minHeight) / 2);
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
