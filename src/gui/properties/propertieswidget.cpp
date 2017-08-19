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

#include "propertieswidget.h"

#include <QAction>
#include <QBitArray>
#include <QDebug>
#include <QFileDialog>
#include <QHeaderView>
#include <QListWidgetItem>
#include <QMenu>
#include <QSplitter>
#include <QStackedWidget>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include "base/bittorrent/session.h"
#include "base/preferences.h"
#include "base/unicodestrings.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "base/utils/string.h"
#include "autoexpandabledialog.h"
#include "downloadedpiecesbar.h"
#include "guiiconprovider.h"
#include "lineedit.h"
#include "mainwindow.h"
#include "messageboxraised.h"
#include "peerlistwidget.h"
#include "pieceavailabilitybar.h"
#include "proplistdelegate.h"
#include "proptabbar.h"
#include "speedwidget.h"
#include "torrentcontentfiltermodel.h"
#include "torrentcontentmodel.h"
#include "trackerlist.h"
#include "transferlistwidget.h"

#include "ui_propertieswidget.h"

PropertiesWidget::PropertiesWidget(QWidget *parent, MainWindow *main_window, TransferListWidget *transferList)
    : QWidget(parent)
    , m_ui(new Ui::PropertiesWidget())
    , transferList(transferList)
    , main_window(main_window)
    , m_torrent(nullptr)
{
    m_ui->setupUi(this);
    setAutoFillBackground(true);

    state = VISIBLE;

    // Set Properties list model
    PropListModel = new TorrentContentFilterModel();
    m_ui->filesList->setModel(PropListModel);
    PropDelegate = new PropListDelegate(this);
    m_ui->filesList->setItemDelegate(PropDelegate);
    m_ui->filesList->setSortingEnabled(true);
    // Torrent content filtering
    m_contentFilterLine = new LineEdit(this);
    m_contentFilterLine->setPlaceholderText(tr("Filter files..."));
    m_contentFilterLine->setMaximumSize(300, m_contentFilterLine->size().height());
    connect(m_contentFilterLine, SIGNAL(textChanged(QString)), this, SLOT(filterText(QString)));
    m_ui->contentFilterLayout->insertWidget(3, m_contentFilterLine);

    // SIGNAL/SLOTS
    connect(m_ui->filesList, SIGNAL(clicked(const QModelIndex&)), m_ui->filesList, SLOT(edit(const QModelIndex&)));
    connect(m_ui->selectAllButton, SIGNAL(clicked()), PropListModel, SLOT(selectAll()));
    connect(m_ui->selectNoneButton, SIGNAL(clicked()), PropListModel, SLOT(selectNone()));
    connect(m_ui->filesList, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayFilesListMenu(const QPoint&)));
    connect(m_ui->filesList, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(openDoubleClickedFile(const QModelIndex&)));
    connect(PropListModel, SIGNAL(filteredFilesChanged()), this, SLOT(filteredFilesChanged()));
    connect(m_ui->listWebSeeds, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayWebSeedListMenu(const QPoint&)));
    connect(transferList, SIGNAL(currentTorrentChanged(BitTorrent::TorrentHandle * const)), this, SLOT(loadTorrentInfos(BitTorrent::TorrentHandle * const)));
    connect(PropDelegate, SIGNAL(filteredFilesChanged()), this, SLOT(filteredFilesChanged()));
    connect(m_ui->stackedProperties, SIGNAL(currentChanged(int)), this, SLOT(loadDynamicData()));
    connect(BitTorrent::Session::instance(), SIGNAL(torrentSavePathChanged(BitTorrent::TorrentHandle * const)), this, SLOT(updateSavePath(BitTorrent::TorrentHandle * const)));
    connect(BitTorrent::Session::instance(), SIGNAL(torrentMetadataLoaded(BitTorrent::TorrentHandle * const)), this, SLOT(updateTorrentInfos(BitTorrent::TorrentHandle * const)));
    connect(m_ui->filesList->header(), SIGNAL(sectionMoved(int,int,int)), this, SLOT(saveSettings()));
    connect(m_ui->filesList->header(), SIGNAL(sectionResized(int,int,int)), this, SLOT(saveSettings()));
    connect(m_ui->filesList->header(), SIGNAL(sortIndicatorChanged(int,Qt::SortOrder)), this, SLOT(saveSettings()));

    // set bar height relative to screen dpi
    int barHeight = devicePixelRatio() * 18;

    // Downloaded pieces progress bar
    m_ui->tempProgressBarArea->setVisible(false);
    downloaded_pieces = new DownloadedPiecesBar(this);
    m_ui->groupBarLayout->addWidget(downloaded_pieces, 0, 1);
    downloaded_pieces->setFixedHeight(barHeight);
    downloaded_pieces->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Pieces availability bar
    m_ui->tempAvailabilityBarArea->setVisible(false);
    pieces_availability = new PieceAvailabilityBar(this);
    m_ui->groupBarLayout->addWidget(pieces_availability, 1, 1);
    pieces_availability->setFixedHeight(barHeight);
    pieces_availability->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Tracker list
    trackerList = new TrackerList(this);
    m_ui->trackerUpButton->setIcon(GuiIconProvider::instance()->getIcon("go-up"));
    m_ui->trackerUpButton->setIconSize(Utils::Misc::smallIconSize());
    m_ui->trackerDownButton->setIcon(GuiIconProvider::instance()->getIcon("go-down"));
    m_ui->trackerDownButton->setIconSize(Utils::Misc::smallIconSize());
    connect(m_ui->trackerUpButton, SIGNAL(clicked()), trackerList, SLOT(moveSelectionUp()));
    connect(m_ui->trackerDownButton, SIGNAL(clicked()), trackerList, SLOT(moveSelectionDown()));
    m_ui->horizontalLayout_trackers->insertWidget(0, trackerList);
    connect(trackerList->header(), SIGNAL(sectionMoved(int,int,int)), trackerList, SLOT(saveSettings()));
    connect(trackerList->header(), SIGNAL(sectionResized(int,int,int)), trackerList, SLOT(saveSettings()));
    connect(trackerList->header(), SIGNAL(sortIndicatorChanged(int,Qt::SortOrder)), trackerList, SLOT(saveSettings()));
    // Peers list
    peersList = new PeerListWidget(this);
    m_ui->peerpage_layout->addWidget(peersList);
    connect(peersList->header(), SIGNAL(sectionMoved(int,int,int)), peersList, SLOT(saveSettings()));
    connect(peersList->header(), SIGNAL(sectionResized(int,int,int)), peersList, SLOT(saveSettings()));
    connect(peersList->header(), SIGNAL(sortIndicatorChanged(int,Qt::SortOrder)), peersList, SLOT(saveSettings()));
    // Speed widget
    speedWidget = new SpeedWidget(this);
    m_ui->speed_layout->addWidget(speedWidget);
    // Tab bar
    m_tabBar = new PropTabBar();
    m_tabBar->setContentsMargins(0, 5, 0, 0);
    m_ui->verticalLayout->addLayout(m_tabBar);
    connect(m_tabBar, SIGNAL(tabChanged(int)), m_ui->stackedProperties, SLOT(setCurrentIndex(int)));
    connect(m_tabBar, SIGNAL(tabChanged(int)), this, SLOT(saveSettings()));
    connect(m_tabBar, SIGNAL(visibilityToggled(bool)), SLOT(setVisibility(bool)));
    connect(m_tabBar, SIGNAL(visibilityToggled(bool)), this, SLOT(saveSettings()));
    // Dynamic data refresher
    refreshTimer = new QTimer(this);
    connect(refreshTimer, SIGNAL(timeout()), this, SLOT(loadDynamicData()));
    refreshTimer->start(3000); // 3sec
    editHotkeyFile = new QShortcut(Qt::Key_F2, m_ui->filesList, 0, 0, Qt::WidgetShortcut);
    connect(editHotkeyFile, SIGNAL(activated()), SLOT(renameSelectedFile()));
    editHotkeyWeb = new QShortcut(Qt::Key_F2, m_ui->listWebSeeds, 0, 0, Qt::WidgetShortcut);
    connect(editHotkeyWeb, SIGNAL(activated()), SLOT(editWebSeed()));
    connect(m_ui->listWebSeeds, SIGNAL(doubleClicked(QModelIndex)), SLOT(editWebSeed()));
    deleteHotkeyWeb = new QShortcut(QKeySequence::Delete, m_ui->listWebSeeds, 0, 0, Qt::WidgetShortcut);
    connect(deleteHotkeyWeb, SIGNAL(activated()), SLOT(deleteSelectedUrlSeeds()));
    openHotkeyFile = new QShortcut(Qt::Key_Return, m_ui->filesList, 0, 0, Qt::WidgetShortcut);
    connect(openHotkeyFile, SIGNAL(activated()), SLOT(openSelectedFile()));
}

PropertiesWidget::~PropertiesWidget()
{
    qDebug() << Q_FUNC_INFO << "ENTER";
    delete refreshTimer;
    delete trackerList;
    delete peersList;
    delete speedWidget;
    delete downloaded_pieces;
    delete pieces_availability;
    delete PropListModel;
    delete PropDelegate;
    delete m_tabBar;
    delete editHotkeyFile;
    delete editHotkeyWeb;
    delete deleteHotkeyWeb;
    delete openHotkeyFile;
    delete m_ui;
    qDebug() << Q_FUNC_INFO << "EXIT";
}

void PropertiesWidget::showPiecesAvailability(bool show)
{
    m_ui->avail_pieces_lbl->setVisible(show);
    pieces_availability->setVisible(show);
    m_ui->avail_average_lbl->setVisible(show);
    if (show || !downloaded_pieces->isVisible())
        m_ui->line_2->setVisible(show);
}

void PropertiesWidget::showPiecesDownloaded(bool show)
{
    m_ui->downloaded_pieces_lbl->setVisible(show);
    downloaded_pieces->setVisible(show);
    m_ui->progress_lbl->setVisible(show);
    if (show || !pieces_availability->isVisible())
        m_ui->line_2->setVisible(show);
}

void PropertiesWidget::setVisibility(bool visible)
{
    if (!visible && (state == VISIBLE)) {
        QSplitter *hSplitter = static_cast<QSplitter *>(parentWidget());
        m_ui->stackedProperties->setVisible(false);
        slideSizes = hSplitter->sizes();
        hSplitter->handle(1)->setVisible(false);
        hSplitter->handle(1)->setDisabled(true);
        QList<int> sizes = QList<int>() << hSplitter->geometry().height() - 30 << 30;
        hSplitter->setSizes(sizes);
        state = REDUCED;
        return;
    }

    if (visible && (state == REDUCED)) {
        m_ui->stackedProperties->setVisible(true);
        QSplitter *hSplitter = static_cast<QSplitter *>(parentWidget());
        hSplitter->handle(1)->setDisabled(false);
        hSplitter->handle(1)->setVisible(true);
        hSplitter->setSizes(slideSizes);
        state = VISIBLE;
        // Force refresh
        loadDynamicData();
    }
}

void PropertiesWidget::clear()
{
    qDebug("Clearing torrent properties");
    m_ui->save_path->clear();
    m_ui->lbl_creationDate->clear();
    m_ui->label_total_pieces_val->clear();
    m_ui->hash_lbl->clear();
    m_ui->comment_text->clear();
    m_ui->progress_lbl->clear();
    trackerList->clear();
    downloaded_pieces->clear();
    pieces_availability->clear();
    m_ui->avail_average_lbl->clear();
    m_ui->wasted->clear();
    m_ui->upTotal->clear();
    m_ui->dlTotal->clear();
    peersList->clear();
    m_ui->lbl_uplimit->clear();
    m_ui->lbl_dllimit->clear();
    m_ui->lbl_elapsed->clear();
    m_ui->lbl_connections->clear();
    m_ui->reannounce_lbl->clear();
    m_ui->shareRatio->clear();
    m_ui->listWebSeeds->clear();
    m_contentFilterLine->clear();
    PropListModel->model()->clear();
    m_ui->label_eta_val->clear();
    m_ui->label_seeds_val->clear();
    m_ui->label_peers_val->clear();
    m_ui->label_dl_speed_val->clear();
    m_ui->label_upload_speed_val->clear();
    m_ui->label_total_size_val->clear();
    m_ui->label_completed_on_val->clear();
    m_ui->label_last_complete_val->clear();
    m_ui->label_created_by_val->clear();
    m_ui->label_added_on_val->clear();
}

BitTorrent::TorrentHandle *PropertiesWidget::getCurrentTorrent() const
{
    return m_torrent;
}

QTreeView *PropertiesWidget::getFilesList() const
{
    return m_ui->filesList;
}

void PropertiesWidget::updateSavePath(BitTorrent::TorrentHandle *const torrent)
{
    if (m_torrent == torrent)
        m_ui->save_path->setText(Utils::Fs::toNativePath(m_torrent->savePath()));
}

void PropertiesWidget::loadTrackers(BitTorrent::TorrentHandle *const torrent)
{
    if (torrent == m_torrent)
        trackerList->loadTrackers();
}

void PropertiesWidget::updateTorrentInfos(BitTorrent::TorrentHandle *const torrent)
{
    if (m_torrent == torrent)
        loadTorrentInfos(m_torrent);
}

void PropertiesWidget::loadTorrentInfos(BitTorrent::TorrentHandle *const torrent)
{
    clear();
    m_torrent = torrent;
    downloaded_pieces->setTorrent(m_torrent);
    pieces_availability->setTorrent(m_torrent);
    if (!m_torrent) return;

    // Save path
    updateSavePath(m_torrent);
    // Hash
    m_ui->hash_lbl->setText(m_torrent->hash());
    PropListModel->model()->clear();
    if (m_torrent->hasMetadata()) {
        // Creation date
        m_ui->lbl_creationDate->setText(m_torrent->creationDate().toString(Qt::DefaultLocaleShortDate));

        m_ui->label_total_size_val->setText(Utils::Misc::friendlyUnit(m_torrent->totalSize()));

        // Comment
        m_ui->comment_text->setText(Utils::Misc::parseHtmlLinks(m_torrent->comment().toHtmlEscaped()));

        // URL seeds
        loadUrlSeeds();

        m_ui->label_created_by_val->setText(m_torrent->creator().toHtmlEscaped());

        // List files in torrent
        PropListModel->model()->setupModelData(m_torrent->info());
        if (PropListModel->model()->rowCount() == 1)
            m_ui->filesList->setExpanded(PropListModel->index(0, 0), true);

        // Load file priorities
        PropListModel->model()->updateFilesPriorities(m_torrent->filePriorities());
    }
    // Load dynamic data
    loadDynamicData();
}

void PropertiesWidget::readSettings()
{
    const Preferences *const pref = Preferences::instance();
    // Restore splitter sizes
    QStringList sizes_str = pref->getPropSplitterSizes().split(",");
    if (sizes_str.size() == 2) {
        slideSizes << sizes_str.first().toInt();
        slideSizes << sizes_str.last().toInt();
        QSplitter *hSplitter = static_cast<QSplitter *>(parentWidget());
        hSplitter->setSizes(slideSizes);
    }
    const int current_tab = pref->getPropCurTab();
    const bool visible = pref->getPropVisible();
    // the following will call saveSettings but shouldn't change any state
    if (!m_ui->filesList->header()->restoreState(pref->getPropFileListState()))
        m_ui->filesList->header()->resizeSection(0, 400); // Default
    m_tabBar->setCurrentIndex(current_tab);
    if (!visible)
        setVisibility(false);
}

void PropertiesWidget::saveSettings()
{
    Preferences *const pref = Preferences::instance();
    pref->setPropVisible(state == VISIBLE);
    // Splitter sizes
    QSplitter *hSplitter = static_cast<QSplitter *>(parentWidget());
    QList<int> sizes;
    if (state == VISIBLE)
        sizes = hSplitter->sizes();
    else
        sizes = slideSizes;
    qDebug("Sizes: %d", sizes.size());
    if (sizes.size() == 2)
        pref->setPropSplitterSizes(QString::number(sizes.first()) + ',' + QString::number(sizes.last()));
    pref->setPropFileListState(m_ui->filesList->header()->saveState());
    // Remember current tab
    pref->setPropCurTab(m_tabBar->currentIndex());
}

void PropertiesWidget::reloadPreferences()
{
    // Take program preferences into consideration
    peersList->updatePeerHostNameResolutionState();
    peersList->updatePeerCountryResolutionState();
}

void PropertiesWidget::loadDynamicData()
{
    // Refresh only if the torrent handle is valid and if visible
    if (!m_torrent || (main_window->currentTabWidget() != transferList) || (state != VISIBLE)) return;

    // Transfer infos
    switch (m_ui->stackedProperties->currentIndex()) {
    case PropTabBar::MAIN_TAB: {
        m_ui->wasted->setText(Utils::Misc::friendlyUnit(m_torrent->wastedSize()));

        m_ui->upTotal->setText(tr("%1 (%2 this session)").arg(Utils::Misc::friendlyUnit(m_torrent->totalUpload()))
                               .arg(Utils::Misc::friendlyUnit(m_torrent->totalPayloadUpload())));

        m_ui->dlTotal->setText(tr("%1 (%2 this session)").arg(Utils::Misc::friendlyUnit(m_torrent->totalDownload()))
                               .arg(Utils::Misc::friendlyUnit(m_torrent->totalPayloadDownload())));

        m_ui->lbl_uplimit->setText(m_torrent->uploadLimit() <= 0 ? QString::fromUtf8(C_INFINITY) : Utils::Misc::friendlyUnit(m_torrent->uploadLimit(), true));

        m_ui->lbl_dllimit->setText(m_torrent->downloadLimit() <= 0 ? QString::fromUtf8(C_INFINITY) : Utils::Misc::friendlyUnit(m_torrent->downloadLimit(), true));

        QString elapsed_txt;
        if (m_torrent->isSeed())
            elapsed_txt = tr("%1 (seeded for %2)", "e.g. 4m39s (seeded for 3m10s)")
                          .arg(Utils::Misc::userFriendlyDuration(m_torrent->activeTime()))
                          .arg(Utils::Misc::userFriendlyDuration(m_torrent->seedingTime()));
        else
            elapsed_txt = Utils::Misc::userFriendlyDuration(m_torrent->activeTime());
        m_ui->lbl_elapsed->setText(elapsed_txt);

        m_ui->lbl_connections->setText(tr("%1 (%2 max)", "%1 and %2 are numbers, e.g. 3 (10 max)")
                                       .arg(m_torrent->connectionsCount())
                                       .arg(m_torrent->connectionsLimit() < 0 ? QString::fromUtf8(C_INFINITY) : QString::number(m_torrent->connectionsLimit())));

        m_ui->label_eta_val->setText(Utils::Misc::userFriendlyDuration(m_torrent->eta()));

        // Update next announce time
        m_ui->reannounce_lbl->setText(Utils::Misc::userFriendlyDuration(m_torrent->nextAnnounce()));

        // Update ratio info
        const qreal ratio = m_torrent->realRatio();
        m_ui->shareRatio->setText(ratio > BitTorrent::TorrentHandle::MAX_RATIO ? QString::fromUtf8(C_INFINITY) : Utils::String::fromDouble(ratio, 2));

        m_ui->label_seeds_val->setText(tr("%1 (%2 total)", "%1 and %2 are numbers, e.g. 3 (10 total)")
                                       .arg(QString::number(m_torrent->seedsCount()))
                                       .arg(QString::number(m_torrent->totalSeedsCount())));

        m_ui->label_peers_val->setText(tr("%1 (%2 total)", "%1 and %2 are numbers, e.g. 3 (10 total)")
                                       .arg(QString::number(m_torrent->leechsCount()))
                                       .arg(QString::number(m_torrent->totalLeechersCount())));

        m_ui->label_dl_speed_val->setText(tr("%1 (%2 avg.)", "%1 and %2 are speed rates, e.g. 200KiB/s (100KiB/s avg.)")
                                          .arg(Utils::Misc::friendlyUnit(m_torrent->downloadPayloadRate(), true))
                                          .arg(Utils::Misc::friendlyUnit(m_torrent->totalDownload() / (1 + m_torrent->activeTime() - m_torrent->finishedTime()), true)));

        m_ui->label_upload_speed_val->setText(tr("%1 (%2 avg.)", "%1 and %2 are speed rates, e.g. 200KiB/s (100KiB/s avg.)")
                                              .arg(Utils::Misc::friendlyUnit(m_torrent->uploadPayloadRate(), true))
                                              .arg(Utils::Misc::friendlyUnit(m_torrent->totalUpload() / (1 + m_torrent->activeTime()), true)));

        m_ui->label_last_complete_val->setText(m_torrent->lastSeenComplete().isValid() ? m_torrent->lastSeenComplete().toString(Qt::DefaultLocaleShortDate) : tr("Never"));

        m_ui->label_completed_on_val->setText(m_torrent->completedTime().isValid() ? m_torrent->completedTime().toString(Qt::DefaultLocaleShortDate) : "");

        m_ui->label_added_on_val->setText(m_torrent->addedTime().toString(Qt::DefaultLocaleShortDate));

        if (m_torrent->hasMetadata()) {
            m_ui->label_total_pieces_val->setText(tr("%1 x %2 (have %3)", "(torrent pieces) eg 152 x 4MB (have 25)").arg(m_torrent->piecesCount()).arg(Utils::Misc::friendlyUnit(m_torrent->pieceLength())).arg(m_torrent->piecesHave()));

            if (!m_torrent->isSeed() && !m_torrent->isPaused() && !m_torrent->isQueued() && !m_torrent->isChecking()) {
                // Pieces availability
                showPiecesAvailability(true);
                pieces_availability->setAvailability(m_torrent->pieceAvailability());
                m_ui->avail_average_lbl->setText(Utils::String::fromDouble(m_torrent->distributedCopies(), 3));
            }
            else {
                showPiecesAvailability(false);
            }

            // Progress
            qreal progress = m_torrent->progress() * 100.;
            m_ui->progress_lbl->setText(Utils::String::fromDouble(progress, 1) + "%");
            downloaded_pieces->setProgress(m_torrent->pieces(), m_torrent->downloadingPieces());
        }
        else {
            showPiecesAvailability(false);
        }

        break;
    }

    case PropTabBar::TRACKERS_TAB: {
        // Trackers
        trackerList->loadTrackers();
        break;
    }

    case PropTabBar::PEERS_TAB: {
        // Load peers
        peersList->loadPeers(m_torrent);
        break;
    }

    case PropTabBar::FILES_TAB: {
        // Files progress
        if (m_torrent->hasMetadata()) {
            qDebug("Updating priorities in files tab");
            m_ui->filesList->setUpdatesEnabled(false);
            PropListModel->model()->updateFilesProgress(m_torrent->filesProgress());
            PropListModel->model()->updateFilesAvailability(m_torrent->availableFileFractions());
            // XXX: We don't update file priorities regularly for performance
            // reasons. This means that priorities will not be updated if
            // set from the Web UI.
            // PropListModel->model()->updateFilesPriorities(h.file_priorities());
            m_ui->filesList->setUpdatesEnabled(true);
        }
        break;
    }

    default:;
    }
}

void PropertiesWidget::loadUrlSeeds()
{
    m_ui->listWebSeeds->clear();
    qDebug("Loading URL seeds");
    const QList<QUrl> hc_seeds = m_torrent->urlSeeds();
    // Add url seeds
    foreach (const QUrl &hc_seed, hc_seeds) {
        qDebug("Loading URL seed: %s", qUtf8Printable(hc_seed.toString()));
        new QListWidgetItem(hc_seed.toString(), m_ui->listWebSeeds);
    }
}

void PropertiesWidget::openDoubleClickedFile(const QModelIndex &index)
{
    if (!index.isValid()) return;
    if (!m_torrent || !m_torrent->hasMetadata()) return;
    if (PropListModel->itemType(index) == TorrentContentModelItem::FileType)
        openFile(index);
    else
        openFolder(index, false);
}

void PropertiesWidget::openFile(const QModelIndex &index)
{
    int i = PropListModel->getFileIndex(index);
    const QDir saveDir(m_torrent->savePath(true));
    const QString filename = m_torrent->filePath(i);
    const QString file_path = Utils::Fs::expandPath(saveDir.absoluteFilePath(filename));
    qDebug("Trying to open file at %s", qUtf8Printable(file_path));
    // Flush data
    m_torrent->flushCache();
    Utils::Misc::openPath(file_path);
}

void PropertiesWidget::openFolder(const QModelIndex &index, bool containing_folder)
{
    QString absolute_path;
    // FOLDER
    if (PropListModel->itemType(index) == TorrentContentModelItem::FolderType) {
        // Generate relative path to selected folder
        QStringList path_items;
        path_items << index.data().toString();
        QModelIndex parent = PropListModel->parent(index);
        while (parent.isValid()) {
            path_items.prepend(parent.data().toString());
            parent = PropListModel->parent(parent);
        }
        if (path_items.isEmpty())
            return;
        const QDir saveDir(m_torrent->savePath(true));
        const QString relative_path = path_items.join("/");
        absolute_path = Utils::Fs::expandPath(saveDir.absoluteFilePath(relative_path));
    }
    else {
        int i = PropListModel->getFileIndex(index);
        const QDir saveDir(m_torrent->savePath(true));
        const QString relative_path = m_torrent->filePath(i);
        absolute_path = Utils::Fs::expandPath(saveDir.absoluteFilePath(relative_path));
    }

    // Flush data
    m_torrent->flushCache();
    if (containing_folder)
        Utils::Misc::openFolderSelect(absolute_path);
    else
        Utils::Misc::openPath(absolute_path);
}

void PropertiesWidget::displayFilesListMenu(const QPoint &)
{
    if (!m_torrent) return;

    QModelIndexList selectedRows = m_ui->filesList->selectionModel()->selectedRows(0);
    if (selectedRows.empty())
        return;
    QMenu myFilesLlistMenu;
    QAction *actOpen = nullptr;
    QAction *actOpenContainingFolder = nullptr;
    QAction *actRename = nullptr;
    if (selectedRows.size() == 1) {
        actOpen = myFilesLlistMenu.addAction(GuiIconProvider::instance()->getIcon("folder-documents"), tr("Open"));
        actOpenContainingFolder = myFilesLlistMenu.addAction(GuiIconProvider::instance()->getIcon("inode-directory"), tr("Open Containing Folder"));
        actRename = myFilesLlistMenu.addAction(GuiIconProvider::instance()->getIcon("edit-rename"), tr("Rename..."));
        myFilesLlistMenu.addSeparator();
    }
    QMenu subMenu;
    if (!m_torrent->isSeed()) {
        subMenu.setTitle(tr("Priority"));
        subMenu.addAction(m_ui->actionNot_downloaded);
        subMenu.addAction(m_ui->actionNormal);
        subMenu.addAction(m_ui->actionHigh);
        subMenu.addAction(m_ui->actionMaximum);
        myFilesLlistMenu.addMenu(&subMenu);
    }
    // Call menu
    const QAction *act = myFilesLlistMenu.exec(QCursor::pos());
    // The selected torrent might have disappeared during exec()
    // from the current view thus leaving invalid indices.
    const QModelIndex index = *(selectedRows.begin());
    if (!index.isValid())
        return;
    if (act) {
        if (act == actOpen) {
            openDoubleClickedFile(index);
        }
        else if (act == actOpenContainingFolder) {
            openFolder(index, true);
        }
        else if (act == actRename) {
            renameSelectedFile();
        }
        else {
            int prio = prio::NORMAL;
            if (act == m_ui->actionHigh)
                prio = prio::HIGH;
            else if (act == m_ui->actionMaximum)
                prio = prio::MAXIMUM;
            else if (act == m_ui->actionNot_downloaded)
                prio = prio::IGNORED;

            qDebug("Setting files priority");
            foreach (QModelIndex index, selectedRows) {
                qDebug("Setting priority(%d) for file at row %d", prio, index.row());
                PropListModel->setData(PropListModel->index(index.row(), PRIORITY, index.parent()), prio);
            }
            // Save changes
            filteredFilesChanged();
        }
    }
}

void PropertiesWidget::displayWebSeedListMenu(const QPoint &)
{
    if (!m_torrent) return;

    QMenu seedMenu;
    QModelIndexList rows = m_ui->listWebSeeds->selectionModel()->selectedRows();
    QAction *actAdd = seedMenu.addAction(GuiIconProvider::instance()->getIcon("list-add"), tr("New Web seed"));
    QAction *actDel = nullptr;
    QAction *actCpy = nullptr;
    QAction *actEdit = nullptr;

    if (rows.size()) {
        actDel = seedMenu.addAction(GuiIconProvider::instance()->getIcon("list-remove"), tr("Remove Web seed"));
        seedMenu.addSeparator();
        actCpy = seedMenu.addAction(GuiIconProvider::instance()->getIcon("edit-copy"), tr("Copy Web seed URL"));
        actEdit = seedMenu.addAction(GuiIconProvider::instance()->getIcon("edit-rename"), tr("Edit Web seed URL"));
    }

    const QAction *act = seedMenu.exec(QCursor::pos());
    if (act) {
        if (act == actAdd)
            askWebSeed();
        else if (act == actDel)
            deleteSelectedUrlSeeds();
        else if (act == actCpy)
            copySelectedWebSeedsToClipboard();
        else if (act == actEdit)
            editWebSeed();
    }
}

void PropertiesWidget::renameSelectedFile()
{
    if (!m_torrent) return;

    const QModelIndexList selectedIndexes = m_ui->filesList->selectionModel()->selectedRows(0);
    if (selectedIndexes.size() != 1) return;

    const QModelIndex modelIndex = selectedIndexes.first();
    if (!modelIndex.isValid()) return;

    // Ask for new name
    bool ok = false;
    QString newName = AutoExpandableDialog::getText(this, tr("Renaming"), tr("New name:"), QLineEdit::Normal, modelIndex.data().toString(), &ok)
                      .trimmed();
    if (!ok) return;

    if (newName.isEmpty() || !Utils::Fs::isValidFileSystemName(newName)) {
        MessageBoxRaised::warning(this, tr("Rename error"),
                                  tr("The name is empty or contains forbidden characters, please choose a different one."),
                                  QMessageBox::Ok);
        return;
    }

    if (PropListModel->itemType(modelIndex) == TorrentContentModelItem::FileType) {
        // renaming a file
        const int fileIndex = PropListModel->getFileIndex(modelIndex);

        if (newName.endsWith(QB_EXT))
            newName.chop(QB_EXT.size());
        const QString oldFileName = m_torrent->fileName(fileIndex);
        const QString oldFilePath = m_torrent->filePath(fileIndex);
        const QString newFileName = newName + (BitTorrent::Session::instance()->isAppendExtensionEnabled() ? QB_EXT : QString());
        const QString newFilePath = oldFilePath.leftRef(oldFilePath.size() - oldFileName.size()) + newFileName;

        if (oldFileName == newFileName) {
            qDebug("Name did not change: %s", qUtf8Printable(oldFileName));
            return;
        }

        // check if that name is already used
        for (int i = 0; i < m_torrent->filesCount(); ++i) {
            if (i == fileIndex) continue;
            if (Utils::Fs::sameFileNames(m_torrent->filePath(i), newFilePath)) {
                MessageBoxRaised::warning(this, tr("Rename error"),
                                          tr("This name is already in use in this folder. Please use a different name."),
                                          QMessageBox::Ok);
                return;
            }
        }

        qDebug("Renaming %s to %s", qUtf8Printable(oldFilePath), qUtf8Printable(newFilePath));
        m_torrent->renameFile(fileIndex, newFilePath);

        PropListModel->setData(modelIndex, newName);
    }
    else {
        // renaming a folder
        QStringList pathItems;
        pathItems << modelIndex.data().toString();
        QModelIndex parent = PropListModel->parent(modelIndex);
        while (parent.isValid()) {
            pathItems.prepend(parent.data().toString());
            parent = PropListModel->parent(parent);
        }
        const QString oldPath = pathItems.join("/");
        pathItems.removeLast();
        pathItems << newName;
        QString newPath = pathItems.join("/");
        if (Utils::Fs::sameFileNames(oldPath, newPath)) {
            qDebug("Name did not change");
            return;
        }
        if (!newPath.endsWith("/")) newPath += "/";
        // Check for overwriting
        for (int i = 0; i < m_torrent->filesCount(); ++i) {
            const QString &currentName = m_torrent->filePath(i);
#if defined(Q_OS_UNIX) || defined(Q_WS_QWS)
            if (currentName.startsWith(newPath, Qt::CaseSensitive)) {
#else
            if (currentName.startsWith(newPath, Qt::CaseInsensitive)) {
#endif
                QMessageBox::warning(this, tr("The folder could not be renamed"),
                                     tr("This name is already in use in this folder. Please use a different name."),
                                     QMessageBox::Ok);
                return;
            }
        }
        bool forceRecheck = false;
        // Replace path in all files
        for (int i = 0; i < m_torrent->filesCount(); ++i) {
            const QString currentName = m_torrent->filePath(i);
            if (currentName.startsWith(oldPath)) {
                QString newName = currentName;
                newName.replace(0, oldPath.length(), newPath);
                if (!forceRecheck && QDir(m_torrent->savePath(true)).exists(newName))
                    forceRecheck = true;
                newName = Utils::Fs::expandPath(newName);
                qDebug("Rename %s to %s", qUtf8Printable(currentName), qUtf8Printable(newName));
                m_torrent->renameFile(i, newName);
            }
        }
        // Force recheck
        if (forceRecheck) m_torrent->forceRecheck();
        // Rename folder in torrent files model too
        PropListModel->setData(modelIndex, newName);
        // Remove old folder
        const QDir oldFolder(m_torrent->savePath(true) + "/" + oldPath);
        int timeout = 10;
        while (!QDir().rmpath(oldFolder.absolutePath()) && timeout > 0) {
            // FIXME: We should not sleep here (freezes the UI for 1 second)
            QThread::msleep(100);
            --timeout;
        }
    }
}

void PropertiesWidget::openSelectedFile()
{
    const QModelIndexList selectedIndexes = m_ui->filesList->selectionModel()->selectedRows(0);
    if (selectedIndexes.size() != 1)
        return;
    openDoubleClickedFile(selectedIndexes.first());
}

void PropertiesWidget::askWebSeed()
{
    bool ok;
    // Ask user for a new url seed
    const QString url_seed = AutoExpandableDialog::getText(this, tr("New URL seed", "New HTTP source"),
                                                           tr("New URL seed:"), QLineEdit::Normal,
                                                           QString::fromUtf8("http://www."), &ok);
    if (!ok) return;
    qDebug("Adding %s web seed", qUtf8Printable(url_seed));
    if (!m_ui->listWebSeeds->findItems(url_seed, Qt::MatchFixedString).empty()) {
        QMessageBox::warning(this, "qBittorrent",
                             tr("This URL seed is already in the list."),
                             QMessageBox::Ok);
        return;
    }
    if (m_torrent)
        m_torrent->addUrlSeeds(QList<QUrl>() << url_seed);
    // Refresh the seeds list
    loadUrlSeeds();
}

void PropertiesWidget::deleteSelectedUrlSeeds()
{
    const QList<QListWidgetItem *> selectedItems = m_ui->listWebSeeds->selectedItems();
    if (selectedItems.isEmpty()) return;

    QList<QUrl> urlSeeds;
    foreach (const QListWidgetItem *item, selectedItems)
        urlSeeds << item->text();

    m_torrent->removeUrlSeeds(urlSeeds);
    // Refresh list
    loadUrlSeeds();
}

void PropertiesWidget::copySelectedWebSeedsToClipboard() const
{
    const QList<QListWidgetItem *> selected_items = m_ui->listWebSeeds->selectedItems();
    if (selected_items.isEmpty())
        return;

    QStringList urls_to_copy;
    foreach (QListWidgetItem *item, selected_items)
        urls_to_copy << item->text();

    QApplication::clipboard()->setText(urls_to_copy.join("\n"));
}

void PropertiesWidget::editWebSeed()
{
    const QList<QListWidgetItem *> selected_items = m_ui->listWebSeeds->selectedItems();
    if (selected_items.size() != 1)
        return;

    const QListWidgetItem *selected_item = selected_items.last();
    const QString old_seed = selected_item->text();
    bool result;
    const QString new_seed = AutoExpandableDialog::getText(this, tr("Web seed editing"),
                                                           tr("Web seed URL:"), QLineEdit::Normal,
                                                           old_seed, &result);
    if (!result)
        return;

    if (!m_ui->listWebSeeds->findItems(new_seed, Qt::MatchFixedString).empty()) {
        QMessageBox::warning(this, tr("qBittorrent"),
                             tr("This URL seed is already in the list."),
                             QMessageBox::Ok);
        return;
    }

    m_torrent->removeUrlSeeds(QList<QUrl>() << old_seed);
    m_torrent->addUrlSeeds(QList<QUrl>() << new_seed);
    loadUrlSeeds();
}

bool PropertiesWidget::applyPriorities()
{
    qDebug("Saving files priorities");
    const QVector<int> priorities = PropListModel->model()->getFilePriorities();
    // Prioritize the files
    qDebug("prioritize files: %d", priorities[0]);
    m_torrent->prioritizeFiles(priorities);
    return true;
}

void PropertiesWidget::filteredFilesChanged()
{
    if (m_torrent)
        applyPriorities();
}

void PropertiesWidget::filterText(const QString &filter)
{
    PropListModel->setFilterRegExp(QRegExp(filter, Qt::CaseInsensitive, QRegExp::WildcardUnix));
    if (filter.isEmpty()) {
        m_ui->filesList->collapseAll();
        m_ui->filesList->expand(PropListModel->index(0, 0));
    }
    else {
        m_ui->filesList->expandAll();
    }
}
