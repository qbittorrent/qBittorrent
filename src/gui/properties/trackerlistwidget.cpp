/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#include "trackerlistwidget.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QDebug>
#include <QHeaderView>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QShortcut>
#include <QStringList>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVector>
#include <QWheelEvent>

#include "base/bittorrent/peerinfo.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/trackerentry.h"
#include "base/global.h"
#include "base/preferences.h"
#include "base/utils/misc.h"
#include "gui/autoexpandabledialog.h"
#include "gui/uithememanager.h"
#include "propertieswidget.h"
#include "trackersadditiondialog.h"

#define NB_STICKY_ITEM 3

TrackerListWidget::TrackerListWidget(PropertiesWidget *properties)
    : m_properties(properties)
{
#ifdef QBT_USES_LIBTORRENT2
    setColumnHidden(COL_PROTOCOL, true); // Must be set before calling loadSettings()
#endif

    // Set header
    // Must be set before calling loadSettings() otherwise the header is reset on restart
    setHeaderLabels(headerLabels());
    // Load settings
    loadSettings();
    // Graphical settings
    setAllColumnsShowFocus(true);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    header()->setFirstSectionMovable(true);
    header()->setStretchLastSection(false); // Must be set after loadSettings() in order to work
    header()->setTextElideMode(Qt::ElideRight);
    // Ensure that at least one column is visible at all times
    if (visibleColumnsCount() == 0)
        setColumnHidden(COL_URL, false);
    // To also mitigate the above issue, we have to resize each column when
    // its size is 0, because explicitly 'showing' the column isn't enough
    // in the above scenario.
    for (int i = 0; i < COL_COUNT; ++i)
    {
        if ((columnWidth(i) <= 0) && !isColumnHidden(i))
            resizeColumnToContents(i);
    }
    // Context menu
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, &TrackerListWidget::showTrackerListMenu);
    // Header
    header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header(), &QWidget::customContextMenuRequested, this, &TrackerListWidget::displayColumnHeaderMenu);
    connect(header(), &QHeaderView::sectionMoved, this, &TrackerListWidget::saveSettings);
    connect(header(), &QHeaderView::sectionResized, this, &TrackerListWidget::saveSettings);
    connect(header(), &QHeaderView::sortIndicatorChanged, this, &TrackerListWidget::saveSettings);

    // Set DHT, PeX, LSD items
    m_DHTItem = new QTreeWidgetItem({ u"** [DHT] **"_s });
    insertTopLevelItem(0, m_DHTItem);
    setRowColor(0, QColorConstants::Svg::grey);
    m_PEXItem = new QTreeWidgetItem({ u"** [PeX] **"_s });
    insertTopLevelItem(1, m_PEXItem);
    setRowColor(1, QColorConstants::Svg::grey);
    m_LSDItem = new QTreeWidgetItem({ u"** [LSD] **"_s });
    insertTopLevelItem(2, m_LSDItem);
    setRowColor(2, QColorConstants::Svg::grey);

    // Set static items alignment
    const Qt::Alignment alignment = (Qt::AlignRight | Qt::AlignVCenter);
    m_DHTItem->setTextAlignment(COL_PEERS, alignment);
    m_PEXItem->setTextAlignment(COL_PEERS, alignment);
    m_LSDItem->setTextAlignment(COL_PEERS, alignment);
    m_DHTItem->setTextAlignment(COL_SEEDS, alignment);
    m_PEXItem->setTextAlignment(COL_SEEDS, alignment);
    m_LSDItem->setTextAlignment(COL_SEEDS, alignment);
    m_DHTItem->setTextAlignment(COL_LEECHES, alignment);
    m_PEXItem->setTextAlignment(COL_LEECHES, alignment);
    m_LSDItem->setTextAlignment(COL_LEECHES, alignment);
    m_DHTItem->setTextAlignment(COL_TIMES_DOWNLOADED, alignment);
    m_PEXItem->setTextAlignment(COL_TIMES_DOWNLOADED, alignment);
    m_LSDItem->setTextAlignment(COL_TIMES_DOWNLOADED, alignment);

    // Set header alignment
    headerItem()->setTextAlignment(COL_TIER, alignment);
    headerItem()->setTextAlignment(COL_PEERS, alignment);
    headerItem()->setTextAlignment(COL_SEEDS, alignment);
    headerItem()->setTextAlignment(COL_LEECHES, alignment);
    headerItem()->setTextAlignment(COL_TIMES_DOWNLOADED, alignment);
    headerItem()->setTextAlignment(COL_NEXT_ANNOUNCE, alignment);
    headerItem()->setTextAlignment(COL_MIN_ANNOUNCE, alignment);

    // Set hotkeys
    const auto *editHotkey = new QShortcut(Qt::Key_F2, this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(editHotkey, &QShortcut::activated, this, &TrackerListWidget::editSelectedTracker);
    const auto *deleteHotkey = new QShortcut(QKeySequence::Delete, this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(deleteHotkey, &QShortcut::activated, this, &TrackerListWidget::deleteSelectedTrackers);
    const auto *copyHotkey = new QShortcut(QKeySequence::Copy, this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(copyHotkey, &QShortcut::activated, this, &TrackerListWidget::copyTrackerUrl);

    connect(this, &QAbstractItemView::doubleClicked, this, &TrackerListWidget::editSelectedTracker);
    connect(this, &QTreeWidget::itemExpanded, this, [](QTreeWidgetItem *item)
    {
        item->setText(COL_PEERS, QString());
        item->setText(COL_SEEDS, QString());
        item->setText(COL_LEECHES, QString());
        item->setText(COL_TIMES_DOWNLOADED, QString());
        item->setText(COL_MSG, QString());
        item->setText(COL_NEXT_ANNOUNCE, QString());
        item->setText(COL_MIN_ANNOUNCE, QString());
    });
    connect(this, &QTreeWidget::itemCollapsed, this, [](QTreeWidgetItem *item)
    {
        item->setText(COL_PEERS, item->data(COL_PEERS, Qt::UserRole).toString());
        item->setText(COL_SEEDS, item->data(COL_SEEDS, Qt::UserRole).toString());
        item->setText(COL_LEECHES, item->data(COL_LEECHES, Qt::UserRole).toString());
        item->setText(COL_TIMES_DOWNLOADED, item->data(COL_TIMES_DOWNLOADED, Qt::UserRole).toString());
        item->setText(COL_MSG, item->data(COL_MSG, Qt::UserRole).toString());

        const auto now = QDateTime::currentDateTime();
        const auto secsToNextAnnounce = now.secsTo(item->data(COL_NEXT_ANNOUNCE, Qt::UserRole).toDateTime());
        item->setText(COL_NEXT_ANNOUNCE, Utils::Misc::userFriendlyDuration(secsToNextAnnounce, -1, Utils::Misc::TimeResolution::Seconds));
        const auto secsToMinAnnounce = now.secsTo(item->data(COL_MIN_ANNOUNCE, Qt::UserRole).toDateTime());
        item->setText(COL_MIN_ANNOUNCE, Utils::Misc::userFriendlyDuration(secsToMinAnnounce, -1, Utils::Misc::TimeResolution::Seconds));
    });
}

TrackerListWidget::~TrackerListWidget()
{
    saveSettings();
}

QVector<QTreeWidgetItem *> TrackerListWidget::getSelectedTrackerItems() const
{
    const QList<QTreeWidgetItem *> selectedTrackerItems = selectedItems();
    QVector<QTreeWidgetItem *> selectedTrackers;
    selectedTrackers.reserve(selectedTrackerItems.size());

    for (QTreeWidgetItem *item : selectedTrackerItems)
    {
        if (indexOfTopLevelItem(item) >= NB_STICKY_ITEM) // Ignore STICKY ITEMS
            selectedTrackers << item;
    }

    return selectedTrackers;
}

void TrackerListWidget::setRowColor(const int row, const QColor &color)
{
    const int nbColumns = columnCount();
    QTreeWidgetItem *item = topLevelItem(row);
    for (int i = 0; i < nbColumns; ++i)
        item->setData(i, Qt::ForegroundRole, color);
}

void TrackerListWidget::moveSelectionUp()
{
    BitTorrent::Torrent *const torrent = m_properties->getCurrentTorrent();
    if (!torrent)
    {
        clear();
        return;
    }
    const QVector<QTreeWidgetItem *> selectedTrackerItems = getSelectedTrackerItems();
    if (selectedTrackerItems.isEmpty()) return;

    bool change = false;
    for (QTreeWidgetItem *item : selectedTrackerItems)
    {
        int index = indexOfTopLevelItem(item);
        if (index > NB_STICKY_ITEM)
        {
            insertTopLevelItem(index - 1, takeTopLevelItem(index));
            change = true;
        }
    }
    if (!change) return;

    // Restore selection
    QItemSelectionModel *selection = selectionModel();
    for (QTreeWidgetItem *item : selectedTrackerItems)
        selection->select(indexFromItem(item), (QItemSelectionModel::Rows | QItemSelectionModel::Select));

    setSelectionModel(selection);
    // Update torrent trackers
    QVector<BitTorrent::TrackerEntry> trackers;
    trackers.reserve(topLevelItemCount());
    for (int i = NB_STICKY_ITEM; i < topLevelItemCount(); ++i)
    {
        const QString trackerURL = topLevelItem(i)->data(COL_URL, Qt::DisplayRole).toString();
        trackers.append({trackerURL, (i - NB_STICKY_ITEM)});
    }

    torrent->replaceTrackers(trackers);
    // Reannounce
    if (!torrent->isPaused())
        torrent->forceReannounce();
}

void TrackerListWidget::moveSelectionDown()
{
    BitTorrent::Torrent *const torrent = m_properties->getCurrentTorrent();
    if (!torrent)
    {
        clear();
        return;
    }
    const QVector<QTreeWidgetItem *> selectedTrackerItems = getSelectedTrackerItems();
    if (selectedTrackerItems.isEmpty()) return;

    bool change = false;
    for (int i = selectedItems().size() - 1; i >= 0; --i)
    {
        int index = indexOfTopLevelItem(selectedTrackerItems.at(i));
        if (index < (topLevelItemCount() - 1))
        {
            insertTopLevelItem(index + 1, takeTopLevelItem(index));
            change = true;
        }
    }
    if (!change) return;

    // Restore selection
    QItemSelectionModel *selection = selectionModel();
    for (QTreeWidgetItem *item : selectedTrackerItems)
        selection->select(indexFromItem(item), (QItemSelectionModel::Rows | QItemSelectionModel::Select));

    setSelectionModel(selection);
    // Update torrent trackers
    QVector<BitTorrent::TrackerEntry> trackers;
    trackers.reserve(topLevelItemCount());
    for (int i = NB_STICKY_ITEM; i < topLevelItemCount(); ++i)
    {
        const QString trackerURL = topLevelItem(i)->data(COL_URL, Qt::DisplayRole).toString();
        trackers.append({trackerURL, (i - NB_STICKY_ITEM)});
    }

    torrent->replaceTrackers(trackers);
    // Reannounce
    if (!torrent->isPaused())
        torrent->forceReannounce();
}

void TrackerListWidget::clear()
{
    qDeleteAll(m_trackerItems);
    m_trackerItems.clear();

    m_DHTItem->setText(COL_STATUS, {});
    m_DHTItem->setText(COL_SEEDS, {});
    m_DHTItem->setText(COL_LEECHES, {});
    m_DHTItem->setText(COL_MSG, {});
    m_PEXItem->setText(COL_STATUS, {});
    m_PEXItem->setText(COL_SEEDS, {});
    m_PEXItem->setText(COL_LEECHES, {});
    m_PEXItem->setText(COL_MSG, {});
    m_LSDItem->setText(COL_STATUS, {});
    m_LSDItem->setText(COL_SEEDS, {});
    m_LSDItem->setText(COL_LEECHES, {});
    m_LSDItem->setText(COL_MSG, {});
}

void TrackerListWidget::loadStickyItems(const BitTorrent::Torrent *torrent)
{
    const QString working {tr("Working")};
    const QString disabled {tr("Disabled")};
    const QString torrentDisabled {tr("Disabled for this torrent")};
    const auto *session = BitTorrent::Session::instance();

    // load DHT information
    if (!session->isDHTEnabled())
        m_DHTItem->setText(COL_STATUS, disabled);
    else if (torrent->isPrivate() || torrent->isDHTDisabled())
        m_DHTItem->setText(COL_STATUS, torrentDisabled);
    else
        m_DHTItem->setText(COL_STATUS, working);

    // Load PeX Information
    if (!session->isPeXEnabled())
        m_PEXItem->setText(COL_STATUS, disabled);
    else if (torrent->isPrivate() || torrent->isPEXDisabled())
        m_PEXItem->setText(COL_STATUS, torrentDisabled);
    else
        m_PEXItem->setText(COL_STATUS, working);

    // Load LSD Information
    if (!session->isLSDEnabled())
        m_LSDItem->setText(COL_STATUS, disabled);
    else if (torrent->isPrivate() || torrent->isLSDDisabled())
        m_LSDItem->setText(COL_STATUS, torrentDisabled);
    else
        m_LSDItem->setText(COL_STATUS, working);

    if (torrent->isPrivate())
    {
        QString privateMsg = tr("This torrent is private");
        m_DHTItem->setText(COL_MSG, privateMsg);
        m_PEXItem->setText(COL_MSG, privateMsg);
        m_LSDItem->setText(COL_MSG, privateMsg);
    }

    using TorrentPtr = QPointer<const BitTorrent::Torrent>;
    torrent->fetchPeerInfo([this, torrent = TorrentPtr(torrent)](const QVector<BitTorrent::PeerInfo> &peers)
    {
        if (torrent != m_properties->getCurrentTorrent())
            return;

        // XXX: libtorrent should provide this info...
        // Count peers from DHT, PeX, LSD
        uint seedsDHT = 0, seedsPeX = 0, seedsLSD = 0, peersDHT = 0, peersPeX = 0, peersLSD = 0;
        for (const BitTorrent::PeerInfo &peer : peers)
        {
            if (peer.isConnecting())
                continue;

            if (peer.fromDHT())
            {
                if (peer.isSeed())
                    ++seedsDHT;
                else
                    ++peersDHT;
            }
            if (peer.fromPeX())
            {
                if (peer.isSeed())
                    ++seedsPeX;
                else
                    ++peersPeX;
            }
            if (peer.fromLSD())
            {
                if (peer.isSeed())
                    ++seedsLSD;
                else
                    ++peersLSD;
            }
        }

        m_DHTItem->setText(COL_SEEDS, QString::number(seedsDHT));
        m_DHTItem->setText(COL_LEECHES, QString::number(peersDHT));
        m_PEXItem->setText(COL_SEEDS, QString::number(seedsPeX));
        m_PEXItem->setText(COL_LEECHES, QString::number(peersPeX));
        m_LSDItem->setText(COL_SEEDS, QString::number(seedsLSD));
        m_LSDItem->setText(COL_LEECHES, QString::number(peersLSD));
    });
}

void TrackerListWidget::loadTrackers()
{
    // Load trackers from torrent handle
    const BitTorrent::Torrent *torrent = m_properties->getCurrentTorrent();
    if (!torrent) return;

    loadStickyItems(torrent);

    const auto setAlignment = [](QTreeWidgetItem *item)
    {
        for (const TrackerListColumn col : {COL_TIER, COL_PROTOCOL, COL_PEERS, COL_SEEDS
                , COL_LEECHES, COL_TIMES_DOWNLOADED, COL_NEXT_ANNOUNCE, COL_MIN_ANNOUNCE})
        {
            item->setTextAlignment(col, (Qt::AlignRight | Qt::AlignVCenter));
        }
    };

    const auto prettyCount = [](const int val)
    {
        return (val > -1) ? QString::number(val) : tr("N/A");
    };

    const auto toString = [](const BitTorrent::TrackerEntry::Status status)
    {
        switch (status)
        {
        case BitTorrent::TrackerEntry::Status::Working:
            return tr("Working");
        case BitTorrent::TrackerEntry::Status::Updating:
            return tr("Updating...");
        case BitTorrent::TrackerEntry::Status::NotWorking:
            return tr("Not working");
        case BitTorrent::TrackerEntry::Status::TrackerError:
            return tr("Tracker error");
        case BitTorrent::TrackerEntry::Status::Unreachable:
            return tr("Unreachable");
        case BitTorrent::TrackerEntry::Status::NotContacted:
            return tr("Not contacted yet");
        }
        return tr("Invalid status!");
    };

    // Load actual trackers information
    QStringList oldTrackerURLs = m_trackerItems.keys();

    for (const BitTorrent::TrackerEntry &entry : asConst(torrent->trackers()))
    {
        const QString trackerURL = entry.url;

        QTreeWidgetItem *item = m_trackerItems.value(trackerURL, nullptr);
        if (!item)
        {
            item = new QTreeWidgetItem();
            item->setText(COL_URL, trackerURL);
            item->setToolTip(COL_URL, trackerURL);
            addTopLevelItem(item);
            m_trackerItems[trackerURL] = item;
        }
        else
        {
            oldTrackerURLs.removeOne(trackerURL);
        }

        const auto now = QDateTime::currentDateTime();

        int peersMax = -1;
        int seedsMax = -1;
        int leechesMax = -1;
        int downloadedMax = -1;
        QDateTime nextAnnounceTime;
        QDateTime minAnnounceTime;
        QString message;

        int index = 0;
        for (const auto &endpoint : entry.stats)
        {
            for (auto it = endpoint.cbegin(), end = endpoint.cend(); it != end; ++it)
            {
                const int protocolVersion = it.key();
                const BitTorrent::TrackerEntry::EndpointStats &protocolStats = it.value();

                peersMax = std::max(peersMax, protocolStats.numPeers);
                seedsMax = std::max(seedsMax, protocolStats.numSeeds);
                leechesMax = std::max(leechesMax, protocolStats.numLeeches);
                downloadedMax = std::max(downloadedMax, protocolStats.numDownloaded);

                if (protocolStats.status == entry.status)
                {
                    if (!nextAnnounceTime.isValid() || (nextAnnounceTime > protocolStats.nextAnnounceTime))
                    {
                        nextAnnounceTime = protocolStats.nextAnnounceTime;
                        minAnnounceTime = protocolStats.minAnnounceTime;
                        if ((protocolStats.status != BitTorrent::TrackerEntry::Status::Working)
                                || !protocolStats.message.isEmpty())
                        {
                            message = protocolStats.message;
                        }
                    }

                    if (protocolStats.status == BitTorrent::TrackerEntry::Status::Working)
                    {
                        if (message.isEmpty())
                            message = protocolStats.message;
                    }
                }

                QTreeWidgetItem *child = (index < item->childCount()) ? item->child(index) : new QTreeWidgetItem(item);
                child->setText(COL_URL, protocolStats.name);
                child->setText(COL_PROTOCOL, tr("v%1").arg(protocolVersion));
                child->setText(COL_STATUS, toString(protocolStats.status));
                child->setText(COL_PEERS, prettyCount(protocolStats.numPeers));
                child->setText(COL_SEEDS, prettyCount(protocolStats.numSeeds));
                child->setText(COL_LEECHES, prettyCount(protocolStats.numLeeches));
                child->setText(COL_TIMES_DOWNLOADED, prettyCount(protocolStats.numDownloaded));
                child->setText(COL_MSG, protocolStats.message);
                child->setToolTip(COL_MSG, protocolStats.message);
                child->setText(COL_NEXT_ANNOUNCE, Utils::Misc::userFriendlyDuration(now.secsTo(protocolStats.nextAnnounceTime), -1, Utils::Misc::TimeResolution::Seconds));
                child->setText(COL_MIN_ANNOUNCE, Utils::Misc::userFriendlyDuration(now.secsTo(protocolStats.minAnnounceTime), -1, Utils::Misc::TimeResolution::Seconds));
                setAlignment(child);
                ++index;
            }
        }

        while (item->childCount() != index)
            delete item->takeChild(index);

        item->setText(COL_TIER, QString::number(entry.tier));
        item->setText(COL_STATUS, toString(entry.status));

        item->setData(COL_PEERS, Qt::UserRole, prettyCount(peersMax));
        item->setData(COL_SEEDS, Qt::UserRole, prettyCount(seedsMax));
        item->setData(COL_LEECHES, Qt::UserRole, prettyCount(leechesMax));
        item->setData(COL_TIMES_DOWNLOADED, Qt::UserRole, prettyCount(downloadedMax));
        item->setData(COL_MSG, Qt::UserRole, message);
        item->setData(COL_NEXT_ANNOUNCE, Qt::UserRole, nextAnnounceTime);
        item->setData(COL_MIN_ANNOUNCE, Qt::UserRole, minAnnounceTime);
        if (!item->isExpanded())
        {
            item->setText(COL_PEERS, item->data(COL_PEERS, Qt::UserRole).toString());
            item->setText(COL_SEEDS, item->data(COL_SEEDS, Qt::UserRole).toString());
            item->setText(COL_LEECHES, item->data(COL_LEECHES, Qt::UserRole).toString());
            item->setText(COL_TIMES_DOWNLOADED, item->data(COL_TIMES_DOWNLOADED, Qt::UserRole).toString());
            item->setText(COL_MSG, item->data(COL_MSG, Qt::UserRole).toString());
            const auto secsToNextAnnounce = now.secsTo(item->data(COL_NEXT_ANNOUNCE, Qt::UserRole).toDateTime());
            item->setText(COL_NEXT_ANNOUNCE, Utils::Misc::userFriendlyDuration(secsToNextAnnounce, -1, Utils::Misc::TimeResolution::Seconds));
            const auto secsToMinAnnounce = now.secsTo(item->data(COL_MIN_ANNOUNCE, Qt::UserRole).toDateTime());
            item->setText(COL_MIN_ANNOUNCE, Utils::Misc::userFriendlyDuration(secsToMinAnnounce, -1, Utils::Misc::TimeResolution::Seconds));
        }
        setAlignment(item);
    }

    // Remove old trackers
    for (const QString &tracker : asConst(oldTrackerURLs))
        delete m_trackerItems.take(tracker);
}

void TrackerListWidget::openAddTrackersDialog()
{
    BitTorrent::Torrent *torrent = m_properties->getCurrentTorrent();
    if (!torrent)
        return;

    auto *dialog = new TrackersAdditionDialog(this, torrent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->open();
}

void TrackerListWidget::copyTrackerUrl()
{
    const QVector<QTreeWidgetItem *> selectedTrackerItems = getSelectedTrackerItems();
    if (selectedTrackerItems.isEmpty()) return;

    QStringList urlsToCopy;
    for (const QTreeWidgetItem *item : selectedTrackerItems)
    {
        QString trackerURL = item->data(COL_URL, Qt::DisplayRole).toString();
        qDebug() << "Copy:" << qUtf8Printable(trackerURL);
        urlsToCopy << trackerURL;
    }
    QApplication::clipboard()->setText(urlsToCopy.join(u'\n'));
}


void TrackerListWidget::deleteSelectedTrackers()
{
    BitTorrent::Torrent *const torrent = m_properties->getCurrentTorrent();
    if (!torrent)
    {
        clear();
        return;
    }

    const QVector<QTreeWidgetItem *> selectedTrackerItems = getSelectedTrackerItems();
    if (selectedTrackerItems.isEmpty()) return;

    QStringList urlsToRemove;
    for (const QTreeWidgetItem *item : selectedTrackerItems)
    {
        QString trackerURL = item->data(COL_URL, Qt::DisplayRole).toString();
        urlsToRemove << trackerURL;
        m_trackerItems.remove(trackerURL);
        delete item;
    }

    torrent->removeTrackers(urlsToRemove);

    if (!torrent->isPaused())
        torrent->forceReannounce();
}

void TrackerListWidget::editSelectedTracker()
{
    BitTorrent::Torrent *const torrent = m_properties->getCurrentTorrent();
    if (!torrent) return;

    const QVector<QTreeWidgetItem *> selectedTrackerItems = getSelectedTrackerItems();
    if (selectedTrackerItems.isEmpty()) return;

    // During multi-select only process item selected last
    const QUrl trackerURL = selectedTrackerItems.last()->text(COL_URL);

    bool ok = false;
    const QUrl newTrackerURL = AutoExpandableDialog::getText(this, tr("Tracker editing"), tr("Tracker URL:"),
                                                         QLineEdit::Normal, trackerURL.toString(), &ok).trimmed();
    if (!ok) return;

    if (!newTrackerURL.isValid())
    {
        QMessageBox::warning(this, tr("Tracker editing failed"), tr("The tracker URL entered is invalid."));
        return;
    }
    if (newTrackerURL == trackerURL) return;

    QVector<BitTorrent::TrackerEntry> trackers = torrent->trackers();
    bool match = false;
    for (BitTorrent::TrackerEntry &entry : trackers)
    {
        if (newTrackerURL == QUrl(entry.url))
        {
            QMessageBox::warning(this, tr("Tracker editing failed"), tr("The tracker URL already exists."));
            return;
        }

        if (!match && (trackerURL == QUrl(entry.url)))
        {
            match = true;
            entry.url = newTrackerURL.toString();
        }
    }

    torrent->replaceTrackers(trackers);

    if (!torrent->isPaused())
        torrent->forceReannounce();
}

void TrackerListWidget::reannounceSelected()
{
    const QList<QTreeWidgetItem *> selItems = selectedItems();
    if (selItems.isEmpty()) return;

    BitTorrent::Torrent *const torrent = m_properties->getCurrentTorrent();
    if (!torrent) return;

    const QVector<BitTorrent::TrackerEntry> trackers = torrent->trackers();

    for (const QTreeWidgetItem *item : selItems)
    {
        // DHT case
        if (item == m_DHTItem)
        {
            torrent->forceDHTAnnounce();
            continue;
        }

        // Trackers case
        for (int i = 0; i < trackers.size(); ++i)
        {
            if (item->text(COL_URL) == trackers[i].url)
            {
                torrent->forceReannounce(i);
                break;
            }
        }
    }

    loadTrackers();
}

void TrackerListWidget::showTrackerListMenu()
{
    BitTorrent::Torrent *const torrent = m_properties->getCurrentTorrent();
    if (!torrent) return;

    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    // Add actions
    menu->addAction(UIThemeManager::instance()->getIcon(u"list-add"_s), tr("Add trackers...")
        , this, &TrackerListWidget::openAddTrackersDialog);

    if (!getSelectedTrackerItems().isEmpty())
    {
        menu->addAction(UIThemeManager::instance()->getIcon(u"edit-rename"_s),tr("Edit tracker URL...")
            , this, &TrackerListWidget::editSelectedTracker);
        menu->addAction(UIThemeManager::instance()->getIcon(u"edit-clear"_s, u"list-remove"_s), tr("Remove tracker")
            , this, &TrackerListWidget::deleteSelectedTrackers);
        menu->addAction(UIThemeManager::instance()->getIcon(u"edit-copy"_s), tr("Copy tracker URL")
            , this, &TrackerListWidget::copyTrackerUrl);
    }

    if (!torrent->isPaused())
    {
        menu->addAction(UIThemeManager::instance()->getIcon(u"reannounce"_s, u"view-refresh"_s), tr("Force reannounce to selected trackers")
            , this, &TrackerListWidget::reannounceSelected);
        menu->addSeparator();
        menu->addAction(UIThemeManager::instance()->getIcon(u"reannounce"_s, u"view-refresh"_s), tr("Force reannounce to all trackers")
            , this, [this]()
        {
            BitTorrent::Torrent *h = m_properties->getCurrentTorrent();
            h->forceReannounce();
            h->forceDHTAnnounce();
        });
    }

    menu->popup(QCursor::pos());
}

void TrackerListWidget::loadSettings()
{
    header()->restoreState(Preferences::instance()->getPropTrackerListState());
}

void TrackerListWidget::saveSettings() const
{
    Preferences::instance()->setPropTrackerListState(header()->saveState());
}

QStringList TrackerListWidget::headerLabels()
{
    return
    {
        tr("URL/Announce endpoint")
        , tr("Tier")
        , tr("Protocol")
        , tr("Status")
        , tr("Peers")
        , tr("Seeds")
        , tr("Leeches")
        , tr("Times Downloaded")
        , tr("Message")
        , tr("Next announce")
        , tr("Min announce")
    };
}

int TrackerListWidget::visibleColumnsCount() const
{
    int count = 0;
    for (int i = 0, iMax = header()->count(); i < iMax; ++i)
    {
        if (!isColumnHidden(i))
            ++count;
    }

    return count;
}

void TrackerListWidget::displayColumnHeaderMenu()
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setTitle(tr("Column visibility"));
    menu->setToolTipsVisible(true);

    for (int i = 0; i < COL_COUNT; ++i)
    {
        QAction *action = menu->addAction(headerLabels().at(i), this, [this, i](const bool checked)
        {
            if (!checked && (visibleColumnsCount() <= 1))
                return;

            setColumnHidden(i, !checked);

            if (checked && (columnWidth(i) <= 5))
                resizeColumnToContents(i);

            saveSettings();
        });
        action->setCheckable(true);
        action->setChecked(!isColumnHidden(i));
    }

    menu->addSeparator();
    QAction *resizeAction = menu->addAction(tr("Resize columns"), this, [this]()
    {
        for (int i = 0, count = header()->count(); i < count; ++i)
        {
            if (!isColumnHidden(i))
                resizeColumnToContents(i);
        }
        saveSettings();
    });
    resizeAction->setToolTip(tr("Resize all non-hidden columns to the size of their contents"));

    menu->popup(QCursor::pos());
}

void TrackerListWidget::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ShiftModifier)
    {
        // Shift + scroll = horizontal scroll
        event->accept();
        QWheelEvent scrollHEvent {event->position(), event->globalPosition()
            , event->pixelDelta(), event->angleDelta().transposed(), event->buttons()
            , event->modifiers(), event->phase(), event->inverted(), event->source()};
        QTreeView::wheelEvent(&scrollHEvent);
        return;
    }

    QTreeView::wheelEvent(event);  // event delegated to base class
}
