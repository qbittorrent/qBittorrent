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
#include <QHash>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QShortcut>
#include <QStringList>
#include <QTableView>
#include <QTreeWidgetItem>
#include <QUrl>

#include "base/bittorrent/peerinfo.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrenthandle.h"
#include "base/bittorrent/trackerentry.h"
#include "base/global.h"
#include "base/preferences.h"
#include "gui/autoexpandabledialog.h"
#include "gui/uithememanager.h"
#include "propertieswidget.h"
#include "trackersadditiondialog.h"

#define NB_STICKY_ITEM 3

TrackerListWidget::TrackerListWidget(PropertiesWidget *properties)
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
    for (int i = 0; i < COL_COUNT; ++i)
        if ((columnWidth(i) <= 0) && !isColumnHidden(i))
            resizeColumnToContents(i);
    // Context menu
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, &TrackerListWidget::showTrackerListMenu);
    // Header
    header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header(), &QWidget::customContextMenuRequested, this, &TrackerListWidget::displayToggleColumnsMenu);
    connect(header(), &QHeaderView::sectionMoved, this, &TrackerListWidget::saveSettings);
    connect(header(), &QHeaderView::sectionResized, this, &TrackerListWidget::saveSettings);
    connect(header(), &QHeaderView::sortIndicatorChanged, this, &TrackerListWidget::saveSettings);

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
    m_DHTItem->setTextAlignment(COL_DOWNLOADED, alignment);
    m_PEXItem->setTextAlignment(COL_DOWNLOADED, alignment);
    m_LSDItem->setTextAlignment(COL_DOWNLOADED, alignment);

    // Set header alignment
    headerItem()->setTextAlignment(COL_TIER, alignment);
    headerItem()->setTextAlignment(COL_PEERS, alignment);
    headerItem()->setTextAlignment(COL_SEEDS, alignment);
    headerItem()->setTextAlignment(COL_LEECHES, alignment);
    headerItem()->setTextAlignment(COL_DOWNLOADED, alignment);

    // Set hotkeys
    const auto *editHotkey = new QShortcut(Qt::Key_F2, this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(editHotkey, &QShortcut::activated, this, &TrackerListWidget::editSelectedTracker);
    const auto *deleteHotkey = new QShortcut(QKeySequence::Delete, this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(deleteHotkey, &QShortcut::activated, this, &TrackerListWidget::deleteSelectedTrackers);
    const auto *copyHotkey = new QShortcut(QKeySequence::Copy, this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(copyHotkey, &QShortcut::activated, this, &TrackerListWidget::copyTrackerUrl);

    connect(this, &QAbstractItemView::doubleClicked, this, &TrackerListWidget::editSelectedTracker);

    // This hack fixes reordering of first column with Qt5.
    // https://github.com/qtproject/qtbase/commit/e0fc088c0c8bc61dbcaf5928b24986cd61a22777
    QTableView unused;
    unused.setVerticalHeader(header());
    header()->setParent(this);
    unused.setVerticalHeader(new QHeaderView(Qt::Horizontal));
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

    for (QTreeWidgetItem *item : selectedTrackerItems) {
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
    BitTorrent::TorrentHandle *const torrent = m_properties->getCurrentTorrent();
    if (!torrent) {
        clear();
        return;
    }
    const QVector<QTreeWidgetItem *> selectedTrackerItems = getSelectedTrackerItems();
    if (selectedTrackerItems.isEmpty()) return;

    bool change = false;
    for (QTreeWidgetItem *item : selectedTrackerItems) {
        int index = indexOfTopLevelItem(item);
        if (index > NB_STICKY_ITEM) {
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
    for (int i = NB_STICKY_ITEM; i < topLevelItemCount(); ++i) {
        const QString trackerURL = topLevelItem(i)->data(COL_URL, Qt::DisplayRole).toString();
        BitTorrent::TrackerEntry e(trackerURL);
        e.setTier(i - NB_STICKY_ITEM);
        trackers.append(e);
    }

    torrent->replaceTrackers(trackers);
    // Reannounce
    if (!torrent->isPaused())
        torrent->forceReannounce();
}

void TrackerListWidget::moveSelectionDown()
{
    BitTorrent::TorrentHandle *const torrent = m_properties->getCurrentTorrent();
    if (!torrent) {
        clear();
        return;
    }
    const QVector<QTreeWidgetItem *> selectedTrackerItems = getSelectedTrackerItems();
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
    for (QTreeWidgetItem *item : selectedTrackerItems)
        selection->select(indexFromItem(item), (QItemSelectionModel::Rows | QItemSelectionModel::Select));

    setSelectionModel(selection);
    // Update torrent trackers
    QVector<BitTorrent::TrackerEntry> trackers;
    trackers.reserve(topLevelItemCount());
    for (int i = NB_STICKY_ITEM; i < topLevelItemCount(); ++i) {
        const QString trackerURL = topLevelItem(i)->data(COL_URL, Qt::DisplayRole).toString();
        BitTorrent::TrackerEntry e(trackerURL);
        e.setTier(i - NB_STICKY_ITEM);
        trackers.append(e);
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

    m_DHTItem->setText(COL_STATUS, "");
    m_DHTItem->setText(COL_SEEDS, "");
    m_DHTItem->setText(COL_LEECHES, "");
    m_DHTItem->setText(COL_MSG, "");
    m_PEXItem->setText(COL_STATUS, "");
    m_PEXItem->setText(COL_SEEDS, "");
    m_PEXItem->setText(COL_LEECHES, "");
    m_PEXItem->setText(COL_MSG, "");
    m_LSDItem->setText(COL_STATUS, "");
    m_LSDItem->setText(COL_SEEDS, "");
    m_LSDItem->setText(COL_LEECHES, "");
    m_LSDItem->setText(COL_MSG, "");
}

void TrackerListWidget::loadStickyItems(const BitTorrent::TorrentHandle *torrent)
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
    for (const BitTorrent::PeerInfo &peer : asConst(torrent->peers())) {
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
    m_DHTItem->setText(COL_LEECHES, QString::number(peersDHT));
    m_PEXItem->setText(COL_SEEDS, QString::number(seedsPeX));
    m_PEXItem->setText(COL_LEECHES, QString::number(peersPeX));
    m_LSDItem->setText(COL_SEEDS, QString::number(seedsLSD));
    m_LSDItem->setText(COL_LEECHES, QString::number(peersLSD));
}

void TrackerListWidget::loadTrackers()
{
    // Load trackers from torrent handle
    const BitTorrent::TorrentHandle *torrent = m_properties->getCurrentTorrent();
    if (!torrent) return;

    loadStickyItems(torrent);

    // Load actual trackers information
    const QHash<QString, BitTorrent::TrackerInfo> trackerData = torrent->trackerInfos();
    QStringList oldTrackerURLs = m_trackerItems.keys();

    for (const BitTorrent::TrackerEntry &entry : asConst(torrent->trackers())) {
        const QString trackerURL = entry.url();

        QTreeWidgetItem *item = m_trackerItems.value(trackerURL, nullptr);
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

        const BitTorrent::TrackerInfo data = trackerData.value(trackerURL);

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
            item->setText(COL_MSG, data.lastMessage.trimmed());
            break;
        case BitTorrent::TrackerEntry::NotContacted:
            item->setText(COL_STATUS, tr("Not contacted yet"));
            item->setText(COL_MSG, "");
            break;
        }

        item->setText(COL_PEERS, QString::number(data.numPeers));
        item->setText(COL_SEEDS, ((entry.numSeeds() > -1)
            ? QString::number(entry.numSeeds())
            : tr("N/A")));
        item->setText(COL_LEECHES, ((entry.numLeeches() > -1)
            ? QString::number(entry.numLeeches())
            : tr("N/A")));
        item->setText(COL_DOWNLOADED, ((entry.numDownloaded() > -1)
            ? QString::number(entry.numDownloaded())
            : tr("N/A")));

        const Qt::Alignment alignment = (Qt::AlignRight | Qt::AlignVCenter);
        item->setTextAlignment(COL_TIER, alignment);
        item->setTextAlignment(COL_PEERS, alignment);
        item->setTextAlignment(COL_SEEDS, alignment);
        item->setTextAlignment(COL_LEECHES, alignment);
        item->setTextAlignment(COL_DOWNLOADED, alignment);
    }

    // Remove old trackers
    for (const QString &tracker : asConst(oldTrackerURLs))
        delete m_trackerItems.take(tracker);
}

// Ask the user for new trackers and add them to the torrent
void TrackerListWidget::askForTrackers()
{
    BitTorrent::TorrentHandle *const torrent = m_properties->getCurrentTorrent();
    if (!torrent) return;

    QVector<BitTorrent::TrackerEntry> trackers;
    for (const QString &tracker : asConst(TrackersAdditionDialog::askForTrackers(this, torrent)))
        trackers << tracker;

    torrent->addTrackers(trackers);
}

void TrackerListWidget::copyTrackerUrl()
{
    const QVector<QTreeWidgetItem *> selectedTrackerItems = getSelectedTrackerItems();
    if (selectedTrackerItems.isEmpty()) return;

    QStringList urlsToCopy;
    for (const QTreeWidgetItem *item : selectedTrackerItems) {
        QString trackerURL = item->data(COL_URL, Qt::DisplayRole).toString();
        qDebug() << QString("Copy: ") + trackerURL;
        urlsToCopy << trackerURL;
    }
    QApplication::clipboard()->setText(urlsToCopy.join('\n'));
}


void TrackerListWidget::deleteSelectedTrackers()
{
    BitTorrent::TorrentHandle *const torrent = m_properties->getCurrentTorrent();
    if (!torrent) {
        clear();
        return;
    }

    const QVector<QTreeWidgetItem *> selectedTrackerItems = getSelectedTrackerItems();
    if (selectedTrackerItems.isEmpty()) return;

    QStringList urlsToRemove;
    for (const QTreeWidgetItem *item : selectedTrackerItems) {
        QString trackerURL = item->data(COL_URL, Qt::DisplayRole).toString();
        urlsToRemove << trackerURL;
        m_trackerItems.remove(trackerURL);
        delete item;
    }

    // Iterate over the trackers and remove the selected ones
    const QVector<BitTorrent::TrackerEntry> trackers = torrent->trackers();
    QVector<BitTorrent::TrackerEntry> remainingTrackers;
    remainingTrackers.reserve(trackers.size());

    for (const BitTorrent::TrackerEntry &entry : trackers) {
        if (!urlsToRemove.contains(entry.url()))
            remainingTrackers.push_back(entry);
    }

    torrent->replaceTrackers(remainingTrackers);

    if (!torrent->isPaused())
        torrent->forceReannounce();
}

void TrackerListWidget::editSelectedTracker()
{
    BitTorrent::TorrentHandle *const torrent = m_properties->getCurrentTorrent();
    if (!torrent) return;

    const QVector<QTreeWidgetItem *> selectedTrackerItems = getSelectedTrackerItems();
    if (selectedTrackerItems.isEmpty()) return;

    // During multi-select only process item selected last
    const QUrl trackerURL = selectedTrackerItems.last()->text(COL_URL);

    bool ok = false;
    const QUrl newTrackerURL = AutoExpandableDialog::getText(this, tr("Tracker editing"), tr("Tracker URL:"),
                                                         QLineEdit::Normal, trackerURL.toString(), &ok).trimmed();
    if (!ok) return;

    if (!newTrackerURL.isValid()) {
        QMessageBox::warning(this, tr("Tracker editing failed"), tr("The tracker URL entered is invalid."));
        return;
    }
    if (newTrackerURL == trackerURL) return;

    QVector<BitTorrent::TrackerEntry> trackers = torrent->trackers();
    bool match = false;
    for (BitTorrent::TrackerEntry &entry : trackers) {
        if (newTrackerURL == QUrl(entry.url())) {
            QMessageBox::warning(this, tr("Tracker editing failed"), tr("The tracker URL already exists."));
            return;
        }

        if (!match && (trackerURL == QUrl(entry.url()))) {
            match = true;
            BitTorrent::TrackerEntry newEntry(newTrackerURL.toString());
            newEntry.setTier(entry.tier());
            entry = newEntry;
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

    BitTorrent::TorrentHandle *const torrent = m_properties->getCurrentTorrent();
    if (!torrent) return;

    const QVector<BitTorrent::TrackerEntry> trackers = torrent->trackers();

    for (const QTreeWidgetItem *item : selItems) {
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

void TrackerListWidget::showTrackerListMenu(const QPoint &)
{
    BitTorrent::TorrentHandle *const torrent = m_properties->getCurrentTorrent();
    if (!torrent) return;

    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    // Add actions
    const QAction *addAct = menu->addAction(UIThemeManager::instance()->getIcon("list-add"), tr("Add a new tracker..."));
    connect(addAct, &QAction::triggered, this, &TrackerListWidget::askForTrackers);

    if (!getSelectedTrackerItems().isEmpty()) {
        const QAction *editAct = menu->addAction(UIThemeManager::instance()->getIcon("edit-rename"),tr("Edit tracker URL..."));
        connect(editAct, &QAction::triggered, this, &TrackerListWidget::editSelectedTracker);

        const QAction *delAct = menu->addAction(UIThemeManager::instance()->getIcon("list-remove"), tr("Remove tracker"));
        connect(delAct, &QAction::triggered, this, &TrackerListWidget::deleteSelectedTrackers);

        const QAction *copyAct = menu->addAction(UIThemeManager::instance()->getIcon("edit-copy"), tr("Copy tracker URL"));
        connect(copyAct, &QAction::triggered, this, &TrackerListWidget::copyTrackerUrl);
    }

    if (!torrent->isPaused()) {
        const QAction *reannounceSelAct = menu->addAction(UIThemeManager::instance()->getIcon("view-refresh"), tr("Force reannounce to selected trackers"));
        connect(reannounceSelAct, &QAction::triggered, this, &TrackerListWidget::reannounceSelected);

        menu->addSeparator();

        const QAction *reannounceAllAct = menu->addAction(UIThemeManager::instance()->getIcon("view-refresh"), tr("Force reannounce to all trackers"));
        connect(reannounceAllAct, &QAction::triggered, this, [this]()
        {
            BitTorrent::TorrentHandle *h = m_properties->getCurrentTorrent();
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
    return {
        tr("Tier")
        , tr("URL")
        , tr("Status")
        , tr("Peers")
        , tr("Seeds")
        , tr("Leeches")
        , tr("Downloaded")
        , tr("Message")
    };
}

int TrackerListWidget::visibleColumnsCount() const
{
    int visibleCols = 0;
    for (int i = 0; i < COL_COUNT; ++i) {
        if (!isColumnHidden(i))
            ++visibleCols;
    }

    return visibleCols;
}

void TrackerListWidget::displayToggleColumnsMenu(const QPoint &)
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setTitle(tr("Column visibility"));

    for (int i = 0; i < COL_COUNT; ++i) {
        QAction *myAct = menu->addAction(headerLabels().at(i));
        myAct->setCheckable(true);
        myAct->setChecked(!isColumnHidden(i));
        myAct->setData(i);
    }

    connect(menu, &QMenu::triggered, this, [this](const QAction *action)
    {
        const int col = action->data().toInt();
        Q_ASSERT(visibleColumnsCount() > 0);

        if (!isColumnHidden(col) && (visibleColumnsCount() == 1))
            return;

        setColumnHidden(col, !isColumnHidden(col));

        if (!isColumnHidden(col) && (columnWidth(col) <= 5))
            resizeColumnToContents(col);

        saveSettings();
    });

    menu->popup(QCursor::pos());
}
