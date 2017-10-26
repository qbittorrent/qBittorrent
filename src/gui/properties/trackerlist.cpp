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

#include "trackerlist.h"

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QDebug>
#include <QHash>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QStringList>
#include <QTableView>
#include <QTreeWidgetItem>
#include <QUrl>

#include "base/bittorrent/peerinfo.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrenthandle.h"
#include "base/bittorrent/trackerentry.h"
#include "base/preferences.h"
#include "base/utils/misc.h"
#include "autoexpandabledialog.h"
#include "guiiconprovider.h"
#include "propertieswidget.h"
#include "trackersadditiondlg.h"

TrackerList::TrackerList(PropertiesWidget *properties)
    : QTreeWidget()
    , m_properties(properties)
{
    // Set header
    // Must be set before calling loadSettings() otherwise the header is reset on restart
    setHeaderLabels(headerLabels());
    // Load settings
    loadSettings();
    // Graphical settings
    setRootIsDecorated(false);
    setAllColumnsShowFocus(true);
    setItemsExpandable(false);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    header()->setStretchLastSection(false); // Must be set after loadSettings() in order to work
    // Ensure that at least one column is visible at all times
    if (visibleColumnsCount() == 0)
        setColumnHidden(COL_URL, false);
    // To also mitigate the above issue, we have to resize each column when
    // its size is 0, because explicitly 'showing' the column isn't enough
    // in the above scenario.
    for (unsigned int i = 0; i < COL_COUNT; ++i)
        if ((columnWidth(i) <= 0) && !isColumnHidden(i))
            resizeColumnToContents(i);
    // Context menu
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showTrackerListMenu(QPoint)));
    // Header context menu
    header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header(), SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayToggleColumnsMenu(const QPoint&)));
    // Set DHT, PeX, LSD items
    m_DHTItem = new QTreeWidgetItem({ "",  "** [DHT] **", "", "0", "", "", "0" });
    insertTopLevelItem(0, m_DHTItem);
    setRowColor(0, QColor("grey"));
    m_PEXItem = new QTreeWidgetItem({ "",  "** [PeX] **", "", "0", "", "", "0" });
    insertTopLevelItem(1, m_PEXItem);
    setRowColor(1, QColor("grey"));
    m_LSDItem = new QTreeWidgetItem({ "",  "** [LSD] **", "", "0", "", "", "0" });
    insertTopLevelItem(2, m_LSDItem);
    setRowColor(2, QColor("grey"));
    // Set static items alignment
    m_DHTItem->setTextAlignment(COL_RECEIVED, (Qt::AlignRight | Qt::AlignVCenter));
    m_PEXItem->setTextAlignment(COL_RECEIVED, (Qt::AlignRight | Qt::AlignVCenter));
    m_LSDItem->setTextAlignment(COL_RECEIVED, (Qt::AlignRight | Qt::AlignVCenter));
    m_DHTItem->setTextAlignment(COL_SEEDS, (Qt::AlignRight | Qt::AlignVCenter));
    m_PEXItem->setTextAlignment(COL_SEEDS, (Qt::AlignRight | Qt::AlignVCenter));
    m_LSDItem->setTextAlignment(COL_SEEDS, (Qt::AlignRight | Qt::AlignVCenter));
    m_DHTItem->setTextAlignment(COL_PEERS, (Qt::AlignRight | Qt::AlignVCenter));
    m_PEXItem->setTextAlignment(COL_PEERS, (Qt::AlignRight | Qt::AlignVCenter));
    m_LSDItem->setTextAlignment(COL_PEERS, (Qt::AlignRight | Qt::AlignVCenter));
    m_DHTItem->setTextAlignment(COL_DOWNLOADED, (Qt::AlignRight | Qt::AlignVCenter));
    m_PEXItem->setTextAlignment(COL_DOWNLOADED, (Qt::AlignRight | Qt::AlignVCenter));
    m_LSDItem->setTextAlignment(COL_DOWNLOADED, (Qt::AlignRight | Qt::AlignVCenter));
    // Set header alignment
    headerItem()->setTextAlignment(COL_TIER, (Qt::AlignRight | Qt::AlignVCenter));
    headerItem()->setTextAlignment(COL_RECEIVED, (Qt::AlignRight | Qt::AlignVCenter));
    headerItem()->setTextAlignment(COL_SEEDS, (Qt::AlignRight | Qt::AlignVCenter));
    headerItem()->setTextAlignment(COL_PEERS, (Qt::AlignRight | Qt::AlignVCenter));
    headerItem()->setTextAlignment(COL_DOWNLOADED, (Qt::AlignRight | Qt::AlignVCenter));
    // Set hotkeys
    m_editHotkey = new QShortcut(Qt::Key_F2, this, SLOT(editSelectedTracker()), 0, Qt::WidgetShortcut);
    connect(this, SIGNAL(doubleClicked(QModelIndex)), SLOT(editSelectedTracker()));
    m_deleteHotkey = new QShortcut(QKeySequence::Delete, this, SLOT(deleteSelectedTrackers()), 0, Qt::WidgetShortcut);
    m_copyHotkey = new QShortcut(QKeySequence::Copy, this, SLOT(copyTrackerUrl()), 0, Qt::WidgetShortcut);

    // This hack fixes reordering of first column with Qt5.
    // https://github.com/qtproject/qtbase/commit/e0fc088c0c8bc61dbcaf5928b24986cd61a22777
    QTableView unused;
    unused.setVerticalHeader(header());
    header()->setParent(this);
    unused.setVerticalHeader(new QHeaderView(Qt::Horizontal));
}

TrackerList::~TrackerList()
{
    saveSettings();
}

QList<QTreeWidgetItem*> TrackerList::getSelectedTrackerItems() const
{
    const QList<QTreeWidgetItem *> selectedTrackerItems = selectedItems();
    QList<QTreeWidgetItem *> selectedTrackers;
    foreach (QTreeWidgetItem *item, selectedTrackerItems) {
        if (indexOfTopLevelItem(item) >= NB_STICKY_ITEM) // Ignore STICKY ITEMS
            selectedTrackers << item;
    }

    return selectedTrackers;
}

void TrackerList::setRowColor(int row, QColor color)
{
    unsigned int nbColumns = columnCount();
    QTreeWidgetItem *item = topLevelItem(row);
    for (unsigned int i = 0; i < nbColumns; ++i)
        item->setData(i, Qt::ForegroundRole, color);
}

void TrackerList::moveSelectionUp()
{
    BitTorrent::TorrentHandle *const torrent = m_properties->getCurrentTorrent();
    if (!torrent) {
        clear();
        return;
    }
    QList<QTreeWidgetItem *> selectedTrackerItems = getSelectedTrackerItems();
    if (selectedTrackerItems.isEmpty()) return;

    bool change = false;
    foreach (QTreeWidgetItem *item, selectedTrackerItems) {
        int index = indexOfTopLevelItem(item);
        if (index > NB_STICKY_ITEM) {
            insertTopLevelItem(index - 1, takeTopLevelItem(index));
            change = true;
        }
    }
    if (!change) return;

    // Restore selection
    QItemSelectionModel *selection = selectionModel();
    foreach (QTreeWidgetItem *item, selectedTrackerItems)
        selection->select(indexFromItem(item), (QItemSelectionModel::Rows | QItemSelectionModel::Select));

    setSelectionModel(selection);
    // Update torrent trackers
    QList<BitTorrent::TrackerEntry> trackers;
    for (int i = NB_STICKY_ITEM; i < topLevelItemCount(); ++i) {
        QString trackerURL = topLevelItem(i)->data(COL_URL, Qt::DisplayRole).toString();
        BitTorrent::TrackerEntry e(trackerURL);
        e.setTier(i - NB_STICKY_ITEM);
        trackers.append(e);
    }

    torrent->replaceTrackers(trackers);
    // Reannounce
    if (!torrent->isPaused())
        torrent->forceReannounce();
}

void TrackerList::moveSelectionDown()
{
    BitTorrent::TorrentHandle *const torrent = m_properties->getCurrentTorrent();
    if (!torrent) {
        clear();
        return;
    }
    QList<QTreeWidgetItem *> selectedTrackerItems = getSelectedTrackerItems();
    if (selectedTrackerItems.isEmpty()) return;

    bool change = false;
    for (int i = selectedItems().size() - 1; i >= 0; --i) {
        int index = indexOfTopLevelItem(selectedTrackerItems.at(i));
        if (index < (topLevelItemCount() - 1)) {
            insertTopLevelItem(index + 1, takeTopLevelItem(index));
            change = true;
        }
    }
    if (!change) return;

    // Restore selection
    QItemSelectionModel *selection = selectionModel();
    foreach (QTreeWidgetItem *item, selectedTrackerItems)
        selection->select(indexFromItem(item), (QItemSelectionModel::Rows | QItemSelectionModel::Select));

    setSelectionModel(selection);
    // Update torrent trackers
    QList<BitTorrent::TrackerEntry> trackers;
    for (int i = NB_STICKY_ITEM; i < topLevelItemCount(); ++i) {
        QString trackerURL = topLevelItem(i)->data(COL_URL, Qt::DisplayRole).toString();
        BitTorrent::TrackerEntry e(trackerURL);
        e.setTier(i - NB_STICKY_ITEM);
        trackers.append(e);
    }

    torrent->replaceTrackers(trackers);
    // Reannounce
    if (!torrent->isPaused())
        torrent->forceReannounce();
}

void TrackerList::clear()
{
    qDeleteAll(m_trackerItems.values());
    m_trackerItems.clear();
    m_DHTItem->setText(COL_STATUS, "");
    m_DHTItem->setText(COL_SEEDS, "");
    m_DHTItem->setText(COL_PEERS, "");
    m_DHTItem->setText(COL_MSG, "");
    m_PEXItem->setText(COL_STATUS, "");
    m_PEXItem->setText(COL_SEEDS, "");
    m_PEXItem->setText(COL_PEERS, "");
    m_PEXItem->setText(COL_MSG, "");
    m_LSDItem->setText(COL_STATUS, "");
    m_LSDItem->setText(COL_SEEDS, "");
    m_LSDItem->setText(COL_PEERS, "");
    m_LSDItem->setText(COL_MSG, "");
}

void TrackerList::loadStickyItems(BitTorrent::TorrentHandle *const torrent)
{
    QString working = tr("Working");
    QString disabled = tr("Disabled");

    // load DHT information
    if (BitTorrent::Session::instance()->isDHTEnabled() && !torrent->isPrivate())
        m_DHTItem->setText(COL_STATUS, working);
    else
        m_DHTItem->setText(COL_STATUS, disabled);

    // Load PeX Information
    if (BitTorrent::Session::instance()->isPeXEnabled() && !torrent->isPrivate())
        m_PEXItem->setText(COL_STATUS, working);
    else
        m_PEXItem->setText(COL_STATUS, disabled);

    // Load LSD Information
    if (BitTorrent::Session::instance()->isLSDEnabled() && !torrent->isPrivate())
        m_LSDItem->setText(COL_STATUS, working);
    else
        m_LSDItem->setText(COL_STATUS, disabled);

    if (torrent->isPrivate()) {
        QString privateMsg = tr("This torrent is private");
        m_DHTItem->setText(COL_MSG, privateMsg);
        m_PEXItem->setText(COL_MSG, privateMsg);
        m_LSDItem->setText(COL_MSG, privateMsg);
    }

    // XXX: libtorrent should provide this info...
    // Count peers from DHT, PeX, LSD
    uint seedsDHT = 0, seedsPeX = 0, seedsLSD = 0, peersDHT = 0, peersPeX = 0, peersLSD = 0;
    foreach (const BitTorrent::PeerInfo &peer, torrent->peers()) {
        if (peer.isConnecting()) continue;

        if (peer.fromDHT()) {
            if (peer.isSeed())
                ++seedsDHT;
            else
                ++peersDHT;
        }
        if (peer.fromPeX()) {
            if (peer.isSeed())
                ++seedsPeX;
            else
                ++peersPeX;
        }
        if (peer.fromLSD()) {
            if (peer.isSeed())
                ++seedsLSD;
            else
                ++peersLSD;
        }
    }

    m_DHTItem->setText(COL_SEEDS, QString::number(seedsDHT));
    m_DHTItem->setText(COL_PEERS, QString::number(peersDHT));
    m_PEXItem->setText(COL_SEEDS, QString::number(seedsPeX));
    m_PEXItem->setText(COL_PEERS, QString::number(peersPeX));
    m_LSDItem->setText(COL_SEEDS, QString::number(seedsLSD));
    m_LSDItem->setText(COL_PEERS, QString::number(peersLSD));
}

void TrackerList::loadTrackers()
{
    // Load trackers from torrent handle
    BitTorrent::TorrentHandle *const torrent = m_properties->getCurrentTorrent();
    if (!torrent) return;

    loadStickyItems(torrent);

    // Load actual trackers information
    QHash<QString, BitTorrent::TrackerInfo> trackerData = torrent->trackerInfos();
    QStringList oldTrackerURLs = m_trackerItems.keys();
    foreach (const BitTorrent::TrackerEntry &entry, torrent->trackers()) {
        QString trackerURL = entry.url();
        QTreeWidgetItem *item = m_trackerItems.value(trackerURL, 0);
        if (!item) {
            item = new QTreeWidgetItem();
            item->setText(COL_URL, trackerURL);
            addTopLevelItem(item);
            m_trackerItems[trackerURL] = item;
        }
        else {
            oldTrackerURLs.removeOne(trackerURL);
        }
        item->setText(COL_TIER, QString::number(entry.tier()));
        BitTorrent::TrackerInfo data = trackerData.value(trackerURL);
        QString errorMessage = data.lastMessage.trimmed();
        switch (entry.status()) {
        case BitTorrent::TrackerEntry::Working:
            item->setText(COL_STATUS, tr("Working"));
            item->setText(COL_MSG, "");
            break;
        case BitTorrent::TrackerEntry::Updating:
            item->setText(COL_STATUS, tr("Updating..."));
            item->setText(COL_MSG, "");
            break;
        case BitTorrent::TrackerEntry::NotWorking:
            item->setText(COL_STATUS, tr("Not working"));
            item->setText(COL_MSG, errorMessage);
            break;
        case BitTorrent::TrackerEntry::NotContacted:
            item->setText(COL_STATUS, tr("Not contacted yet"));
            item->setText(COL_MSG, "");
            break;
        }

        item->setText(COL_RECEIVED, QString::number(data.numPeers));
#if LIBTORRENT_VERSION_NUM >= 10000
        item->setText(COL_SEEDS, QString::number(entry.nativeEntry().scrape_complete > 0 ? entry.nativeEntry().scrape_complete : 0));
        item->setText(COL_PEERS, QString::number(entry.nativeEntry().scrape_incomplete > 0 ? entry.nativeEntry().scrape_incomplete : 0));
        item->setText(COL_DOWNLOADED, QString::number(entry.nativeEntry().scrape_downloaded > 0 ? entry.nativeEntry().scrape_downloaded : 0));
#else
        item->setText(COL_SEEDS, "0");
        item->setText(COL_PEERS, "0");
        item->setText(COL_DOWNLOADED, "0");
#endif

        item->setTextAlignment(COL_TIER, (Qt::AlignRight | Qt::AlignVCenter));
        item->setTextAlignment(COL_RECEIVED, (Qt::AlignRight | Qt::AlignVCenter));
        item->setTextAlignment(COL_SEEDS, (Qt::AlignRight | Qt::AlignVCenter));
        item->setTextAlignment(COL_PEERS, (Qt::AlignRight | Qt::AlignVCenter));
        item->setTextAlignment(COL_DOWNLOADED, (Qt::AlignRight | Qt::AlignVCenter));
    }
    // Remove old trackers
    foreach (const QString &tracker, oldTrackerURLs)
        delete m_trackerItems.take(tracker);
}

// Ask the user for new trackers and add them to the torrent
void TrackerList::askForTrackers()
{
    BitTorrent::TorrentHandle *const torrent = m_properties->getCurrentTorrent();
    if (!torrent) return;

    QList<BitTorrent::TrackerEntry> trackers;
    foreach (const QString &tracker, TrackersAdditionDlg::askForTrackers(this, torrent))
        trackers << tracker;

    torrent->addTrackers(trackers);
}

void TrackerList::copyTrackerUrl()
{
    QList<QTreeWidgetItem *> selectedTrackerItems = getSelectedTrackerItems();
    if (selectedTrackerItems.isEmpty()) return;

    QStringList urlsToCopy;
    foreach (QTreeWidgetItem *item, selectedTrackerItems) {
        QString trackerURL = item->data(COL_URL, Qt::DisplayRole).toString();
        qDebug() << QString("Copy: ") + trackerURL;
        urlsToCopy << trackerURL;
    }
    QApplication::clipboard()->setText(urlsToCopy.join("\n"));
}


void TrackerList::deleteSelectedTrackers()
{
    BitTorrent::TorrentHandle *const torrent = m_properties->getCurrentTorrent();
    if (!torrent) {
        clear();
        return;
    }

    QList<QTreeWidgetItem *> selectedTrackerItems = getSelectedTrackerItems();
    if (selectedTrackerItems.isEmpty()) return;

    QStringList urlsToRemove;
    foreach (QTreeWidgetItem *item, selectedTrackerItems) {
        QString trackerURL = item->data(COL_URL, Qt::DisplayRole).toString();
        urlsToRemove << trackerURL;
        m_trackerItems.remove(trackerURL);
        delete item;
    }

    // Iterate over the trackers and remove the selected ones
    QList<BitTorrent::TrackerEntry> remainingTrackers;
    QList<BitTorrent::TrackerEntry> trackers = torrent->trackers();
    foreach (const BitTorrent::TrackerEntry &entry, trackers) {
        if (!urlsToRemove.contains(entry.url()))
            remainingTrackers.push_back(entry);
    }

    torrent->replaceTrackers(remainingTrackers);
    if (!torrent->isPaused())
        torrent->forceReannounce();
}

void TrackerList::editSelectedTracker()
{
    BitTorrent::TorrentHandle *const torrent = m_properties->getCurrentTorrent();
    if (!torrent) return;

    QString hash = torrent->hash();

    QList<QTreeWidgetItem *> selectedTrackerItems = getSelectedTrackerItems();
    if (selectedTrackerItems.isEmpty()) return;

    // During multi-select only process item selected last
    QUrl trackerURL = selectedTrackerItems.last()->text(COL_URL);

    bool ok;
    QUrl newTrackerURL = AutoExpandableDialog::getText(this, tr("Tracker editing"), tr("Tracker URL:"),
                                                         QLineEdit::Normal, trackerURL.toString(), &ok).trimmed();
    if (!ok) return;

    if (!newTrackerURL.isValid()) {
        QMessageBox::warning(this, tr("Tracker editing failed"), tr("The tracker URL entered is invalid."));
        return;
    }
    if (newTrackerURL == trackerURL) return;

    QList<BitTorrent::TrackerEntry> trackers = torrent->trackers();
    bool match = false;
    for (int i = 0; i < trackers.size(); ++i) {
        BitTorrent::TrackerEntry &entry = trackers[i];
        if (newTrackerURL == QUrl(entry.url())) {
            QMessageBox::warning(this, tr("Tracker editing failed"), tr("The tracker URL already exists."));
            return;
        }

        if (trackerURL == QUrl(entry.url()) && !match) {
            BitTorrent::TrackerEntry newEntry(newTrackerURL.toString());
            newEntry.setTier(entry.tier());
            match = true;
            entry = newEntry;
        }
    }

    torrent->replaceTrackers(trackers);
    if (!torrent->isPaused())
        torrent->forceReannounce();
}

void TrackerList::reannounceSelected()
{
    QList<QTreeWidgetItem *> selItems = selectedItems();
    if (selItems.isEmpty()) return;

    BitTorrent::TorrentHandle *const torrent = m_properties->getCurrentTorrent();
    if (!torrent) return;

    QList<BitTorrent::TrackerEntry> trackers = torrent->trackers();

    foreach (QTreeWidgetItem* item, selItems) {
        // DHT case
        if (item == m_DHTItem) {
            torrent->forceDHTAnnounce();
            continue;
        }

        // Trackers case
        for (int i = 0; i < trackers.size(); ++i) {
            if (item->text(COL_URL) == trackers[i].url()) {
                torrent->forceReannounce(i);
                break;
            }
        }
    }

    loadTrackers();
}

void TrackerList::showTrackerListMenu(QPoint)
{
    BitTorrent::TorrentHandle *const torrent = m_properties->getCurrentTorrent();
    if (!torrent) return;

    //QList<QTreeWidgetItem*> selected_items = getSelectedTrackerItems();
    QMenu menu;
    // Add actions
    QAction *addAct = menu.addAction(GuiIconProvider::instance()->getIcon("list-add"), tr("Add a new tracker..."));
    QAction *copyAct = nullptr;
    QAction *delAct = nullptr;
    QAction *editAct = nullptr;
    if (!getSelectedTrackerItems().isEmpty()) {
        delAct = menu.addAction(GuiIconProvider::instance()->getIcon("list-remove"), tr("Remove tracker"));
        copyAct = menu.addAction(GuiIconProvider::instance()->getIcon("edit-copy"), tr("Copy tracker URL"));
        editAct = menu.addAction(GuiIconProvider::instance()->getIcon("edit-rename"),tr("Edit selected tracker URL"));
    }
    QAction *reannounceSelAct = nullptr;
    QAction *reannounceAllAct = nullptr;
    if (!torrent->isPaused()) {
        reannounceSelAct = menu.addAction(GuiIconProvider::instance()->getIcon("view-refresh"), tr("Force reannounce to selected trackers"));
        menu.addSeparator();
        reannounceAllAct = menu.addAction(GuiIconProvider::instance()->getIcon("view-refresh"), tr("Force reannounce to all trackers"));
    }
    QAction *act = menu.exec(QCursor::pos());
    if (act == nullptr) return;

    if (act == addAct) {
        askForTrackers();
        return;
    }
    if (act == copyAct) {
        copyTrackerUrl();
        return;
    }
    if (act == delAct) {
        deleteSelectedTrackers();
        return;
    }
    if (act == reannounceSelAct) {
        reannounceSelected();
        return;
    }
    if (act == reannounceAllAct) {
        BitTorrent::TorrentHandle *h = m_properties->getCurrentTorrent();
        h->forceReannounce();
        h->forceDHTAnnounce();
        return;
    }
    if (act == editAct) {
        editSelectedTracker();
        return;
    }
}

void TrackerList::loadSettings()
{
    header()->restoreState(Preferences::instance()->getPropTrackerListState());
}

void TrackerList::saveSettings() const
{
    Preferences::instance()->setPropTrackerListState(header()->saveState());
}

QStringList TrackerList::headerLabels()
{
    static const QStringList header {
        "#"
        , tr("URL")
        , tr("Status")
        , tr("Received")
        , tr("Seeds")
        , tr("Peers")
        , tr("Downloaded")
        , tr("Message")
    };

    return header;
}

int TrackerList::visibleColumnsCount() const
{
    int visibleCols = 0;
    for (unsigned int i = 0; i < COL_COUNT; ++i) {
        if (!isColumnHidden(i))
            ++visibleCols;
    }

    return visibleCols;
}

void TrackerList::displayToggleColumnsMenu(const QPoint &)
{
    QMenu hideshowColumn(this);
    hideshowColumn.setTitle(tr("Column visibility"));
    for (int i = 0; i < COL_COUNT; ++i) {
        QAction *myAct = hideshowColumn.addAction(headerLabels().at(i));
        myAct->setCheckable(true);
        myAct->setChecked(!isColumnHidden(i));
        myAct->setData(i);
    }

    // Call menu
    QAction *act = hideshowColumn.exec(QCursor::pos());
    if (!act) return;

    int col = act->data().toInt();
    Q_ASSERT(visibleColumnsCount() > 0);
    if (!isColumnHidden(col) && (visibleColumnsCount() == 1))
        return;
    qDebug("Toggling column %d visibility", col);
    setColumnHidden(col, !isColumnHidden(col));
    if (!isColumnHidden(col) && (columnWidth(col) <= 5))
        setColumnWidth(col, 100);
    saveSettings();
}
