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

#include "propertieswidget.h"

#include <QAction>
#include <QDebug>
#include <QDir>
#include <QHeaderView>
#include <QListWidgetItem>
#include <QMenu>
#include <QSplitter>
#include <QStackedWidget>
#include <QThread>
#include <QUrl>

#include "base/bittorrent/downloadpriority.h"
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
#include "peerlistwidget.h"
#include "pieceavailabilitybar.h"
#include "proplistdelegate.h"
#include "proptabbar.h"
#include "raisedmessagebox.h"
#include "speedwidget.h"
#include "torrentcontentfiltermodel.h"
#include "torrentcontentmodel.h"
#include "trackerlistwidget.h"
#include "utils.h"

#include "ui_propertieswidget.h"

#ifdef Q_OS_MAC
#include "macutilities.h"
#endif

PropertiesWidget::PropertiesWidget(QWidget *parent)
    : QWidget(parent)
    , m_ui(new Ui::PropertiesWidget())
    , m_torrent(nullptr)
{
    m_ui->setupUi(this);
    setAutoFillBackground(true);

    m_state = VISIBLE;

    // Set Properties list model
    m_propListModel = new TorrentContentFilterModel();
    m_ui->filesList->setModel(m_propListModel);
    m_propListDelegate = new PropListDelegate(this);
    m_ui->filesList->setItemDelegate(m_propListDelegate);
    m_ui->filesList->setSortingEnabled(true);

    // Torrent content filtering
    m_contentFilterLine = new LineEdit(this);
    m_contentFilterLine->setPlaceholderText(tr("Filter files..."));
    m_contentFilterLine->setFixedWidth(Utils::Gui::scaledSize(this, 300));
    connect(m_contentFilterLine, &LineEdit::textChanged, this, &PropertiesWidget::filterText);
    m_ui->contentFilterLayout->insertWidget(3, m_contentFilterLine);

    // SIGNAL/SLOTS
    connect(m_ui->filesList, &QAbstractItemView::clicked
            , m_ui->filesList, static_cast<void (QAbstractItemView::*)(const QModelIndex &)>(&QAbstractItemView::edit));
    connect(m_ui->selectAllButton, &QPushButton::clicked, m_propListModel, &TorrentContentFilterModel::selectAll);
    connect(m_ui->selectNoneButton, &QPushButton::clicked, m_propListModel, &TorrentContentFilterModel::selectNone);
    connect(m_ui->filesList, &QWidget::customContextMenuRequested, this, &PropertiesWidget::displayFilesListMenu);
    connect(m_ui->filesList, &QAbstractItemView::doubleClicked, this, &PropertiesWidget::openDoubleClickedFile);
    connect(m_propListModel, &TorrentContentFilterModel::filteredFilesChanged, this, &PropertiesWidget::filteredFilesChanged);
    connect(m_ui->listWebSeeds, &QWidget::customContextMenuRequested, this, &PropertiesWidget::displayWebSeedListMenu);
    connect(m_propListDelegate, &PropListDelegate::filteredFilesChanged, this, &PropertiesWidget::filteredFilesChanged);
    connect(m_ui->stackedProperties, &QStackedWidget::currentChanged, this, &PropertiesWidget::loadDynamicData);
    connect(BitTorrent::Session::instance(), &BitTorrent::Session::torrentSavePathChanged, this, &PropertiesWidget::updateSavePath);
    connect(BitTorrent::Session::instance(), &BitTorrent::Session::torrentMetadataLoaded, this, &PropertiesWidget::updateTorrentInfos);
    connect(m_ui->filesList->header(), &QHeaderView::sectionMoved, this, &PropertiesWidget::saveSettings);
    connect(m_ui->filesList->header(), &QHeaderView::sectionResized, this, &PropertiesWidget::saveSettings);
    connect(m_ui->filesList->header(), &QHeaderView::sortIndicatorChanged, this, &PropertiesWidget::saveSettings);

    // set bar height relative to screen dpi
    const int barHeight = Utils::Gui::scaledSize(this, 18);

    // Downloaded pieces progress bar
    m_ui->tempProgressBarArea->setVisible(false);
    m_downloadedPieces = new DownloadedPiecesBar(this);
    m_ui->groupBarLayout->addWidget(m_downloadedPieces, 0, 1);
    m_downloadedPieces->setFixedHeight(barHeight);
    m_downloadedPieces->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Pieces availability bar
    m_ui->tempAvailabilityBarArea->setVisible(false);
    m_piecesAvailability = new PieceAvailabilityBar(this);
    m_ui->groupBarLayout->addWidget(m_piecesAvailability, 1, 1);
    m_piecesAvailability->setFixedHeight(barHeight);
    m_piecesAvailability->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Tracker list
    m_trackerList = new TrackerListWidget(this);
    m_ui->trackerUpButton->setIcon(GuiIconProvider::instance()->getIcon("go-up"));
    m_ui->trackerUpButton->setIconSize(Utils::Gui::smallIconSize());
    m_ui->trackerDownButton->setIcon(GuiIconProvider::instance()->getIcon("go-down"));
    m_ui->trackerDownButton->setIconSize(Utils::Gui::smallIconSize());
    connect(m_ui->trackerUpButton, &QPushButton::clicked, m_trackerList, &TrackerListWidget::moveSelectionUp);
    connect(m_ui->trackerDownButton, &QPushButton::clicked, m_trackerList, &TrackerListWidget::moveSelectionDown);
    m_ui->hBoxLayoutTrackers->insertWidget(0, m_trackerList);
    // Peers list
    m_peerList = new PeerListWidget(this);
    m_ui->vBoxLayoutPeerPage->addWidget(m_peerList);
    // Tab bar
    m_tabBar = new PropTabBar();
    m_tabBar->setContentsMargins(0, 5, 0, 0);
    m_ui->verticalLayout->addLayout(m_tabBar);
    connect(m_tabBar, &PropTabBar::tabChanged, m_ui->stackedProperties, &QStackedWidget::setCurrentIndex);
    connect(m_tabBar, &PropTabBar::tabChanged, this, &PropertiesWidget::saveSettings);
    connect(m_tabBar, &PropTabBar::visibilityToggled, this, &PropertiesWidget::setVisibility);
    connect(m_tabBar, &PropTabBar::visibilityToggled, this, &PropertiesWidget::saveSettings);

    m_editHotkeyFile = new QShortcut(Qt::Key_F2, m_ui->filesList, nullptr, nullptr, Qt::WidgetShortcut);
    connect(m_editHotkeyFile, &QShortcut::activated, this, &PropertiesWidget::renameSelectedFile);
    m_editHotkeyWeb = new QShortcut(Qt::Key_F2, m_ui->listWebSeeds, nullptr, nullptr, Qt::WidgetShortcut);
    connect(m_editHotkeyWeb, &QShortcut::activated, this, &PropertiesWidget::editWebSeed);
    connect(m_ui->listWebSeeds, &QListWidget::doubleClicked, this, &PropertiesWidget::editWebSeed);
    m_deleteHotkeyWeb = new QShortcut(QKeySequence::Delete, m_ui->listWebSeeds, nullptr, nullptr, Qt::WidgetShortcut);
    connect(m_deleteHotkeyWeb, &QShortcut::activated, this, &PropertiesWidget::deleteSelectedUrlSeeds);
    m_openHotkeyFile = new QShortcut(Qt::Key_Return, m_ui->filesList, nullptr, nullptr, Qt::WidgetShortcut);
    connect(m_openHotkeyFile, &QShortcut::activated, this, &PropertiesWidget::openSelectedFile);

    configure();
    connect(Preferences::instance(), &Preferences::changed, this, &PropertiesWidget::configure);
}

PropertiesWidget::~PropertiesWidget()
{
    qDebug() << Q_FUNC_INFO << "ENTER";
    delete m_trackerList;
    delete m_peerList;
    delete m_speedWidget;
    delete m_downloadedPieces;
    delete m_piecesAvailability;
    delete m_propListModel;
    delete m_propListDelegate;
    delete m_tabBar;
    delete m_editHotkeyFile;
    delete m_editHotkeyWeb;
    delete m_deleteHotkeyWeb;
    delete m_openHotkeyFile;
    delete m_ui;
    qDebug() << Q_FUNC_INFO << "EXIT";
}

void PropertiesWidget::showPiecesAvailability(bool show)
{
    m_ui->labelPiecesAvailability->setVisible(show);
    m_piecesAvailability->setVisible(show);
    m_ui->labelAverageAvailabilityVal->setVisible(show);
    if (show || !m_downloadedPieces->isVisible())
        m_ui->lineBelowBars->setVisible(show);
}

void PropertiesWidget::showPiecesDownloaded(bool show)
{
    m_ui->labelDownloadedPieces->setVisible(show);
    m_downloadedPieces->setVisible(show);
    m_ui->labelProgressVal->setVisible(show);
    if (show || !m_piecesAvailability->isVisible())
        m_ui->lineBelowBars->setVisible(show);
}

void PropertiesWidget::setVisibility(bool visible)
{
    if (!visible && (m_state == VISIBLE)) {
        auto *hSplitter = static_cast<QSplitter *>(parentWidget());
        m_ui->stackedProperties->setVisible(false);
        m_slideSizes = hSplitter->sizes();
        hSplitter->handle(1)->setVisible(false);
        hSplitter->handle(1)->setDisabled(true);
        QList<int> sizes = QList<int>() << hSplitter->geometry().height() - 30 << 30;
        hSplitter->setSizes(sizes);
        m_state = REDUCED;
        return;
    }

    if (visible && (m_state == REDUCED)) {
        m_ui->stackedProperties->setVisible(true);
        auto *hSplitter = static_cast<QSplitter *>(parentWidget());
        hSplitter->handle(1)->setDisabled(false);
        hSplitter->handle(1)->setVisible(true);
        hSplitter->setSizes(m_slideSizes);
        m_state = VISIBLE;
        // Force refresh
        loadDynamicData();
    }
}

void PropertiesWidget::clear()
{
    qDebug("Clearing torrent properties");
    m_ui->labelSavePathVal->clear();
    m_ui->labelCreatedOnVal->clear();
    m_ui->labelTotalPiecesVal->clear();
    m_ui->labelHashVal->clear();
    m_ui->labelCommentVal->clear();
    m_ui->labelProgressVal->clear();
    m_ui->labelAverageAvailabilityVal->clear();
    m_ui->labelWastedVal->clear();
    m_ui->labelUpTotalVal->clear();
    m_ui->labelDlTotalVal->clear();
    m_ui->labelUpLimitVal->clear();
    m_ui->labelDlLimitVal->clear();
    m_ui->labelElapsedVal->clear();
    m_ui->labelConnectionsVal->clear();
    m_ui->labelReannounceInVal->clear();
    m_ui->labelShareRatioVal->clear();
    m_ui->listWebSeeds->clear();
    m_ui->labelETAVal->clear();
    m_ui->labelSeedsVal->clear();
    m_ui->labelPeersVal->clear();
    m_ui->labelDlSpeedVal->clear();
    m_ui->labelUpSpeedVal->clear();
    m_ui->labelTotalSizeVal->clear();
    m_ui->labelCompletedOnVal->clear();
    m_ui->labelLastSeenCompleteVal->clear();
    m_ui->labelCreatedByVal->clear();
    m_ui->labelAddedOnVal->clear();
    m_trackerList->clear();
    m_downloadedPieces->clear();
    m_piecesAvailability->clear();
    m_peerList->clear();
    m_contentFilterLine->clear();
    m_propListModel->model()->clear();
}

BitTorrent::TorrentHandle *PropertiesWidget::getCurrentTorrent() const
{
    return m_torrent;
}

TrackerListWidget *PropertiesWidget::getTrackerList() const
{
    return m_trackerList;
}

PeerListWidget *PropertiesWidget::getPeerList() const
{
    return m_peerList;
}

QTreeView *PropertiesWidget::getFilesList() const
{
    return m_ui->filesList;
}

void PropertiesWidget::updateSavePath(BitTorrent::TorrentHandle *const torrent)
{
    if (torrent == m_torrent)
        m_ui->labelSavePathVal->setText(Utils::Fs::toNativePath(m_torrent->savePath()));
}

void PropertiesWidget::loadTrackers(BitTorrent::TorrentHandle *const torrent)
{
    if (torrent == m_torrent)
        m_trackerList->loadTrackers();
}

void PropertiesWidget::updateTorrentInfos(BitTorrent::TorrentHandle *const torrent)
{
    if (torrent == m_torrent)
        loadTorrentInfos(m_torrent);
}

void PropertiesWidget::loadTorrentInfos(BitTorrent::TorrentHandle *const torrent)
{
    clear();
    m_torrent = torrent;
    m_downloadedPieces->setTorrent(m_torrent);
    m_piecesAvailability->setTorrent(m_torrent);
    if (!m_torrent) return;

    // Save path
    updateSavePath(m_torrent);
    // Hash
    m_ui->labelHashVal->setText(m_torrent->hash());
    m_propListModel->model()->clear();
    if (m_torrent->hasMetadata()) {
        // Creation date
        m_ui->labelCreatedOnVal->setText(m_torrent->creationDate().toString(Qt::DefaultLocaleShortDate));

        m_ui->labelTotalSizeVal->setText(Utils::Misc::friendlyUnit(m_torrent->totalSize()));

        // Comment
        m_ui->labelCommentVal->setText(Utils::Misc::parseHtmlLinks(m_torrent->comment().toHtmlEscaped()));

        // URL seeds
        loadUrlSeeds();

        m_ui->labelCreatedByVal->setText(m_torrent->creator().toHtmlEscaped());

        // List files in torrent
        m_propListModel->model()->setupModelData(m_torrent->info());
        if (m_propListModel->model()->rowCount() == 1)
            m_ui->filesList->setExpanded(m_propListModel->index(0, 0), true);

        // Load file priorities
        m_propListModel->model()->updateFilesPriorities(m_torrent->filePriorities());
    }
    // Load dynamic data
    loadDynamicData();
}

void PropertiesWidget::readSettings()
{
    const Preferences *const pref = Preferences::instance();
    // Restore splitter sizes
    QStringList sizesStr = pref->getPropSplitterSizes().split(',');
    if (sizesStr.size() == 2) {
        m_slideSizes << sizesStr.first().toInt();
        m_slideSizes << sizesStr.last().toInt();
        auto *hSplitter = static_cast<QSplitter *>(parentWidget());
        hSplitter->setSizes(m_slideSizes);
    }
    const int currentTab = pref->getPropCurTab();
    const bool visible = pref->getPropVisible();
    m_ui->filesList->header()->restoreState(pref->getPropFileListState());
    m_tabBar->setCurrentIndex(currentTab);
    if (!visible)
        setVisibility(false);
}

void PropertiesWidget::saveSettings()
{
    Preferences *const pref = Preferences::instance();
    pref->setPropVisible(m_state == VISIBLE);
    // Splitter sizes
    auto *hSplitter = static_cast<QSplitter *>(parentWidget());
    QList<int> sizes;
    if (m_state == VISIBLE)
        sizes = hSplitter->sizes();
    else
        sizes = m_slideSizes;
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
    m_peerList->updatePeerHostNameResolutionState();
    m_peerList->updatePeerCountryResolutionState();
}

void PropertiesWidget::loadDynamicData()
{
    // Refresh only if the torrent handle is valid and visible
    if (!m_torrent || (m_state != VISIBLE)) return;

    // Transfer infos
    switch (m_ui->stackedProperties->currentIndex()) {
    case PropTabBar::MainTab: {
            m_ui->labelWastedVal->setText(Utils::Misc::friendlyUnit(m_torrent->wastedSize()));

            m_ui->labelUpTotalVal->setText(tr("%1 (%2 this session)").arg(Utils::Misc::friendlyUnit(m_torrent->totalUpload())
                , Utils::Misc::friendlyUnit(m_torrent->totalPayloadUpload())));

            m_ui->labelDlTotalVal->setText(tr("%1 (%2 this session)").arg(Utils::Misc::friendlyUnit(m_torrent->totalDownload())
                , Utils::Misc::friendlyUnit(m_torrent->totalPayloadDownload())));

            m_ui->labelUpLimitVal->setText(m_torrent->uploadLimit() <= 0 ? QString::fromUtf8(C_INFINITY) : Utils::Misc::friendlyUnit(m_torrent->uploadLimit(), true));

            m_ui->labelDlLimitVal->setText(m_torrent->downloadLimit() <= 0 ? QString::fromUtf8(C_INFINITY) : Utils::Misc::friendlyUnit(m_torrent->downloadLimit(), true));

            QString elapsedString;
            if (m_torrent->isSeed())
                elapsedString = tr("%1 (seeded for %2)", "e.g. 4m39s (seeded for 3m10s)")
                    .arg(Utils::Misc::userFriendlyDuration(m_torrent->activeTime())
                        , Utils::Misc::userFriendlyDuration(m_torrent->seedingTime()));
            else
                elapsedString = Utils::Misc::userFriendlyDuration(m_torrent->activeTime());
            m_ui->labelElapsedVal->setText(elapsedString);

            m_ui->labelConnectionsVal->setText(tr("%1 (%2 max)", "%1 and %2 are numbers, e.g. 3 (10 max)")
                                           .arg(m_torrent->connectionsCount())
                                           .arg(m_torrent->connectionsLimit() < 0 ? QString::fromUtf8(C_INFINITY) : QString::number(m_torrent->connectionsLimit())));

            m_ui->labelETAVal->setText(Utils::Misc::userFriendlyDuration(m_torrent->eta()));

            // Update next announce time
            m_ui->labelReannounceInVal->setText(Utils::Misc::userFriendlyDuration(m_torrent->nextAnnounce()));

            // Update ratio info
            const qreal ratio = m_torrent->realRatio();
            m_ui->labelShareRatioVal->setText(ratio > BitTorrent::TorrentHandle::MAX_RATIO ? QString::fromUtf8(C_INFINITY) : Utils::String::fromDouble(ratio, 2));

            m_ui->labelSeedsVal->setText(tr("%1 (%2 total)", "%1 and %2 are numbers, e.g. 3 (10 total)")
                .arg(QString::number(m_torrent->seedsCount())
                    , QString::number(m_torrent->totalSeedsCount())));

            m_ui->labelPeersVal->setText(tr("%1 (%2 total)", "%1 and %2 are numbers, e.g. 3 (10 total)")
                .arg(QString::number(m_torrent->leechsCount())
                    , QString::number(m_torrent->totalLeechersCount())));

            const int dlDuration = m_torrent->activeTime() - m_torrent->finishedTime();
            const QString dlAvg = Utils::Misc::friendlyUnit((m_torrent->totalDownload() / ((dlDuration == 0) ? -1 : dlDuration)), true);
            m_ui->labelDlSpeedVal->setText(tr("%1 (%2 avg.)", "%1 and %2 are speed rates, e.g. 200KiB/s (100KiB/s avg.)")
                .arg(Utils::Misc::friendlyUnit(m_torrent->downloadPayloadRate(), true), dlAvg));

            const int ulDuration = m_torrent->activeTime();
            const QString ulAvg = Utils::Misc::friendlyUnit((m_torrent->totalUpload() / ((ulDuration == 0) ? -1 : ulDuration)), true);
            m_ui->labelUpSpeedVal->setText(tr("%1 (%2 avg.)", "%1 and %2 are speed rates, e.g. 200KiB/s (100KiB/s avg.)")
                .arg(Utils::Misc::friendlyUnit(m_torrent->uploadPayloadRate(), true), ulAvg));

            m_ui->labelLastSeenCompleteVal->setText(m_torrent->lastSeenComplete().isValid() ? m_torrent->lastSeenComplete().toString(Qt::DefaultLocaleShortDate) : tr("Never"));

            m_ui->labelCompletedOnVal->setText(m_torrent->completedTime().isValid() ? m_torrent->completedTime().toString(Qt::DefaultLocaleShortDate) : "");

            m_ui->labelAddedOnVal->setText(m_torrent->addedTime().toString(Qt::DefaultLocaleShortDate));

            if (m_torrent->hasMetadata()) {
                m_ui->labelTotalPiecesVal->setText(tr("%1 x %2 (have %3)", "(torrent pieces) eg 152 x 4MB (have 25)").arg(m_torrent->piecesCount()).arg(Utils::Misc::friendlyUnit(m_torrent->pieceLength())).arg(m_torrent->piecesHave()));

                if (!m_torrent->isSeed() && !m_torrent->isPaused() && !m_torrent->isQueued() && !m_torrent->isChecking()) {
                    // Pieces availability
                    showPiecesAvailability(true);
                    m_piecesAvailability->setAvailability(m_torrent->pieceAvailability());
                    m_ui->labelAverageAvailabilityVal->setText(Utils::String::fromDouble(m_torrent->distributedCopies(), 3));
                }
                else {
                    showPiecesAvailability(false);
                }

                // Progress
                qreal progress = m_torrent->progress() * 100.;
                m_ui->labelProgressVal->setText(Utils::String::fromDouble(progress, 1) + '%');
                m_downloadedPieces->setProgress(m_torrent->pieces(), m_torrent->downloadingPieces());
            }
            else {
                showPiecesAvailability(false);
            }
        }
        break;
    case PropTabBar::TrackersTab:
        // Trackers
        m_trackerList->loadTrackers();
        break;
    case PropTabBar::PeersTab:
        // Load peers
        m_peerList->loadPeers(m_torrent);
        break;
    case PropTabBar::FilesTab:
        // Files progress
        if (m_torrent->hasMetadata()) {
            qDebug("Updating priorities in files tab");
            m_ui->filesList->setUpdatesEnabled(false);
            m_propListModel->model()->updateFilesProgress(m_torrent->filesProgress());
            m_propListModel->model()->updateFilesAvailability(m_torrent->availableFileFractions());
            // XXX: We don't update file priorities regularly for performance
            // reasons. This means that priorities will not be updated if
            // set from the Web UI.
            // PropListModel->model()->updateFilesPriorities(h.file_priorities());
            m_ui->filesList->setUpdatesEnabled(true);
        }
        break;
    default:;
    }
}

void PropertiesWidget::loadUrlSeeds()
{
    m_ui->listWebSeeds->clear();
    qDebug("Loading URL seeds");
    const QList<QUrl> hcSeeds = m_torrent->urlSeeds();
    // Add url seeds
    for (const QUrl &hcSeed : hcSeeds) {
        qDebug("Loading URL seed: %s", qUtf8Printable(hcSeed.toString()));
        new QListWidgetItem(hcSeed.toString(), m_ui->listWebSeeds);
    }
}

void PropertiesWidget::openDoubleClickedFile(const QModelIndex &index)
{
    if (!index.isValid() || !m_torrent || !m_torrent->hasMetadata()) return;

    if (m_propListModel->itemType(index) == TorrentContentModelItem::FileType)
        openFile(index);
    else
        openFolder(index, false);
}

void PropertiesWidget::openFile(const QModelIndex &index)
{
    int i = m_propListModel->getFileIndex(index);
    const QDir saveDir(m_torrent->savePath(true));
    const QString filename = m_torrent->filePath(i);
    const QString filePath = Utils::Fs::expandPath(saveDir.absoluteFilePath(filename));
    qDebug("Trying to open file at %s", qUtf8Printable(filePath));
    // Flush data
    m_torrent->flushCache();
    Utils::Gui::openPath(filePath);
}

void PropertiesWidget::openFolder(const QModelIndex &index, bool containingFolder)
{
    QString absolutePath;
    // FOLDER
    if (m_propListModel->itemType(index) == TorrentContentModelItem::FolderType) {
        // Generate relative path to selected folder
        QStringList pathItems;
        pathItems << index.data().toString();
        QModelIndex parent = m_propListModel->parent(index);
        while (parent.isValid()) {
            pathItems.prepend(parent.data().toString());
            parent = m_propListModel->parent(parent);
        }
        if (pathItems.isEmpty())
            return;
        const QDir saveDir(m_torrent->savePath(true));
        const QString relativePath = pathItems.join('/');
        absolutePath = Utils::Fs::expandPath(saveDir.absoluteFilePath(relativePath));
    }
    else {
        int i = m_propListModel->getFileIndex(index);
        const QDir saveDir(m_torrent->savePath(true));
        const QString relativePath = m_torrent->filePath(i);
        absolutePath = Utils::Fs::expandPath(saveDir.absoluteFilePath(relativePath));
    }

    // Flush data
    m_torrent->flushCache();
#ifdef Q_OS_MAC
    Q_UNUSED(containingFolder);
    MacUtils::openFiles(QSet<QString>{absolutePath});
#else
    if (containingFolder)
        Utils::Gui::openFolderSelect(absolutePath);
    else
        Utils::Gui::openPath(absolutePath);
#endif
}

void PropertiesWidget::displayFilesListMenu(const QPoint &)
{
    if (!m_torrent) return;

    const QModelIndexList selectedRows = m_ui->filesList->selectionModel()->selectedRows(0);
    if (selectedRows.empty()) return;

    QMenu myFilesListMenu;
    QAction *actOpen = nullptr;
    QAction *actOpenContainingFolder = nullptr;
    QAction *actRename = nullptr;
    if (selectedRows.size() == 1) {
        actOpen = myFilesListMenu.addAction(GuiIconProvider::instance()->getIcon("folder-documents"), tr("Open"));
        actOpenContainingFolder = myFilesListMenu.addAction(GuiIconProvider::instance()->getIcon("inode-directory"), tr("Open Containing Folder"));
        actRename = myFilesListMenu.addAction(GuiIconProvider::instance()->getIcon("edit-rename"), tr("Rename..."));
        myFilesListMenu.addSeparator();
    }
    QMenu subMenu;
    if (!m_torrent->isSeed()) {
        subMenu.setTitle(tr("Priority"));
        subMenu.addAction(m_ui->actionNotDownloaded);
        subMenu.addAction(m_ui->actionNormal);
        subMenu.addAction(m_ui->actionHigh);
        subMenu.addAction(m_ui->actionMaximum);
        myFilesListMenu.addMenu(&subMenu);
    }

    // The selected torrent might have disappeared during exec()
    // so we just close menu when an appropriate model is reset
    connect(m_ui->filesList->model(), &QAbstractItemModel::modelAboutToBeReset
            , &myFilesListMenu, [&myFilesListMenu]()
    {
        myFilesListMenu.setActiveAction(nullptr);
        myFilesListMenu.close();
    });
    // Call menu
    const QAction *act = myFilesListMenu.exec(QCursor::pos());
    if (!act) return;

    const QModelIndex index = selectedRows[0];
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
        BitTorrent::DownloadPriority prio = BitTorrent::DownloadPriority::Normal;
        if (act == m_ui->actionHigh)
            prio = BitTorrent::DownloadPriority::High;
        else if (act == m_ui->actionMaximum)
            prio = BitTorrent::DownloadPriority::Maximum;
        else if (act == m_ui->actionNotDownloaded)
            prio = BitTorrent::DownloadPriority::Ignored;

        qDebug("Setting files priority");
        for (const QModelIndex &index : selectedRows) {
            qDebug("Setting priority(%d) for file at row %d", static_cast<int>(prio), index.row());
            m_propListModel->setData(m_propListModel->index(index.row(), PRIORITY, index.parent()), static_cast<int>(prio));
        }
        // Save changes
        filteredFilesChanged();
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

    if (!rows.isEmpty()) {
        actDel = seedMenu.addAction(GuiIconProvider::instance()->getIcon("list-remove"), tr("Remove Web seed"));
        seedMenu.addSeparator();
        actCpy = seedMenu.addAction(GuiIconProvider::instance()->getIcon("edit-copy"), tr("Copy Web seed URL"));
        actEdit = seedMenu.addAction(GuiIconProvider::instance()->getIcon("edit-rename"), tr("Edit Web seed URL"));
    }

    const QAction *act = seedMenu.exec(QCursor::pos());
    if (!act) return;

    if (act == actAdd)
        askWebSeed();
    else if (act == actDel)
        deleteSelectedUrlSeeds();
    else if (act == actCpy)
        copySelectedWebSeedsToClipboard();
    else if (act == actEdit)
        editWebSeed();
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
    const bool isFile = (m_propListModel->itemType(modelIndex) == TorrentContentModelItem::FileType);
    QString newName = AutoExpandableDialog::getText(this, tr("Renaming"), tr("New name:"), QLineEdit::Normal
            , modelIndex.data().toString(), &ok, isFile).trimmed();
    if (!ok) return;

    if (newName.isEmpty() || !Utils::Fs::isValidFileSystemName(newName)) {
        RaisedMessageBox::warning(this, tr("Rename error"),
                                  tr("The name is empty or contains forbidden characters, please choose a different one."),
                                  QMessageBox::Ok);
        return;
    }

    if (isFile) {
        const int fileIndex = m_propListModel->getFileIndex(modelIndex);

        if (newName.endsWith(QB_EXT))
            newName.chop(QB_EXT.size());
        const QString oldFileName = m_torrent->fileName(fileIndex);
        const QString oldFilePath = m_torrent->filePath(fileIndex);

        const bool useFilenameExt = BitTorrent::Session::instance()->isAppendExtensionEnabled()
            && (m_torrent->filesProgress()[fileIndex] != 1);
        const QString newFileName = newName + (useFilenameExt ? QB_EXT : QString());
        const QString newFilePath = oldFilePath.leftRef(oldFilePath.size() - oldFileName.size()) + newFileName;

        if (oldFileName == newFileName) {
            qDebug("Name did not change: %s", qUtf8Printable(oldFileName));
            return;
        }

        // check if that name is already used
        for (int i = 0; i < m_torrent->filesCount(); ++i) {
            if (i == fileIndex) continue;
            if (Utils::Fs::sameFileNames(m_torrent->filePath(i), newFilePath)) {
                RaisedMessageBox::warning(this, tr("Rename error"),
                                          tr("This name is already in use in this folder. Please use a different name."),
                                          QMessageBox::Ok);
                return;
            }
        }

        qDebug("Renaming %s to %s", qUtf8Printable(oldFilePath), qUtf8Printable(newFilePath));
        m_torrent->renameFile(fileIndex, newFilePath);

        m_propListModel->setData(modelIndex, newName);
    }
    else {
        // renaming a folder
        QStringList pathItems;
        pathItems << modelIndex.data().toString();
        QModelIndex parent = m_propListModel->parent(modelIndex);
        while (parent.isValid()) {
            pathItems.prepend(parent.data().toString());
            parent = m_propListModel->parent(parent);
        }
        const QString oldPath = pathItems.join('/');
        pathItems.removeLast();
        pathItems << newName;
        QString newPath = pathItems.join('/');
        if (Utils::Fs::sameFileNames(oldPath, newPath)) {
            qDebug("Name did not change");
            return;
        }
        if (!newPath.endsWith('/')) newPath += '/';
        // Check for overwriting
        for (int i = 0; i < m_torrent->filesCount(); ++i) {
            const QString currentName = m_torrent->filePath(i);
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
        m_propListModel->setData(modelIndex, newName);
        // Remove old folder
        const QDir oldFolder(m_torrent->savePath(true) + '/' + oldPath);
        int timeout = 10;
        while (!QDir().rmpath(oldFolder.absolutePath()) && (timeout > 0)) {
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

void PropertiesWidget::configure()
{
    // Speed widget
    if (Preferences::instance()->isSpeedWidgetEnabled()) {
        if (!m_speedWidget || !qobject_cast<SpeedWidget *>(m_speedWidget)) {
            m_ui->speedLayout->removeWidget(m_speedWidget);
            delete m_speedWidget;
            m_speedWidget = new SpeedWidget {this};
            m_ui->speedLayout->addWidget(m_speedWidget);
        }
    }
    else {
        if (!m_speedWidget || !qobject_cast<QLabel *>(m_speedWidget)) {
            m_ui->speedLayout->removeWidget(m_speedWidget);
            delete m_speedWidget;
            auto *label = new QLabel(tr("<center><b>Speed graphs are disabled</b><p>You may change this setting in Advanced Options </center>"), this);
            label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            m_speedWidget = label;
            m_ui->speedLayout->addWidget(m_speedWidget);
        }
    }
}

void PropertiesWidget::askWebSeed()
{
    bool ok;
    // Ask user for a new url seed
    const QString urlSeed = AutoExpandableDialog::getText(this, tr("New URL seed", "New HTTP source"),
                                                           tr("New URL seed:"), QLineEdit::Normal,
                                                           QLatin1String("http://www."), &ok);
    if (!ok) return;
    qDebug("Adding %s web seed", qUtf8Printable(urlSeed));
    if (!m_ui->listWebSeeds->findItems(urlSeed, Qt::MatchFixedString).empty()) {
        QMessageBox::warning(this, "qBittorrent",
                             tr("This URL seed is already in the list."),
                             QMessageBox::Ok);
        return;
    }
    if (m_torrent)
        m_torrent->addUrlSeeds(QList<QUrl>() << urlSeed);
    // Refresh the seeds list
    loadUrlSeeds();
}

void PropertiesWidget::deleteSelectedUrlSeeds()
{
    const QList<QListWidgetItem *> selectedItems = m_ui->listWebSeeds->selectedItems();
    if (selectedItems.isEmpty()) return;

    QList<QUrl> urlSeeds;
    for (const QListWidgetItem *item : selectedItems)
        urlSeeds << item->text();

    m_torrent->removeUrlSeeds(urlSeeds);
    // Refresh list
    loadUrlSeeds();
}

void PropertiesWidget::copySelectedWebSeedsToClipboard() const
{
    const QList<QListWidgetItem *> selectedItems = m_ui->listWebSeeds->selectedItems();
    if (selectedItems.isEmpty()) return;

    QStringList urlsToCopy;
    for (const QListWidgetItem *item : selectedItems)
        urlsToCopy << item->text();

    QApplication::clipboard()->setText(urlsToCopy.join('\n'));
}

void PropertiesWidget::editWebSeed()
{
    const QList<QListWidgetItem *> selectedItems = m_ui->listWebSeeds->selectedItems();
    if (selectedItems.size() != 1) return;

    const QListWidgetItem *selectedItem = selectedItems.last();
    const QString oldSeed = selectedItem->text();
    bool result;
    const QString newSeed = AutoExpandableDialog::getText(this, tr("Web seed editing"),
                                                           tr("Web seed URL:"), QLineEdit::Normal,
                                                           oldSeed, &result);
    if (!result) return;

    if (!m_ui->listWebSeeds->findItems(newSeed, Qt::MatchFixedString).empty()) {
        QMessageBox::warning(this, tr("qBittorrent"),
                             tr("This URL seed is already in the list."),
                             QMessageBox::Ok);
        return;
    }

    m_torrent->removeUrlSeeds(QList<QUrl>() << oldSeed);
    m_torrent->addUrlSeeds(QList<QUrl>() << newSeed);
    loadUrlSeeds();
}

void PropertiesWidget::applyPriorities()
{
    m_torrent->prioritizeFiles(m_propListModel->model()->getFilePriorities());
}

void PropertiesWidget::filteredFilesChanged()
{
    if (m_torrent)
        applyPriorities();
}

void PropertiesWidget::filterText(const QString &filter)
{
    m_propListModel->setFilterRegExp(QRegExp(filter, Qt::CaseInsensitive, QRegExp::WildcardUnix));
    if (filter.isEmpty()) {
        m_ui->filesList->collapseAll();
        m_ui->filesList->expand(m_propListModel->index(0, 0));
    }
    else {
        m_ui->filesList->expandAll();
    }
}
