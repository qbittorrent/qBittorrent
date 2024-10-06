/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022-2024  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QClipboard>
#include <QDateTime>
#include <QDebug>
#include <QListWidgetItem>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QSplitter>
#include <QShortcut>
#include <QStackedWidget>
#include <QUrl>

#include "base/bittorrent/infohash.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/path.h"
#include "base/preferences.h"
#include "base/types.h"
#include "base/unicodestrings.h"
#include "base/utils/misc.h"
#include "base/utils/string.h"
#include "gui/autoexpandabledialog.h"
#include "gui/filterpatternformatmenu.h"
#include "gui/lineedit.h"
#include "gui/trackerlist/trackerlistwidget.h"
#include "gui/uithememanager.h"
#include "gui/utils.h"
#include "downloadedpiecesbar.h"
#include "peerlistwidget.h"
#include "pieceavailabilitybar.h"
#include "proptabbar.h"
#include "speedwidget.h"
#include "ui_propertieswidget.h"

PropertiesWidget::PropertiesWidget(QWidget *parent)
    : QWidget(parent)
    , m_ui {new Ui::PropertiesWidget}
    , m_storeFilterPatternFormat {u"GUI/PropertiesWidget/FilterPatternFormat"_s}
{
    m_ui->setupUi(this);
#ifndef Q_OS_MACOS
    setAutoFillBackground(true);
#endif

    m_state = VISIBLE;

    // Torrent content filtering
    m_contentFilterLine = new LineEdit(this);
    m_contentFilterLine->setPlaceholderText(tr("Filter files..."));
    m_contentFilterLine->setFixedWidth(300);
    m_contentFilterLine->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_contentFilterLine, &QWidget::customContextMenuRequested, this, &PropertiesWidget::showContentFilterContextMenu);
    connect(m_contentFilterLine, &LineEdit::textChanged, this, &PropertiesWidget::setContentFilterPattern);
    m_ui->contentFilterLayout->insertWidget(3, m_contentFilterLine);

    m_ui->filesList->setDoubleClickAction(TorrentContentWidget::DoubleClickAction::Open);
    m_ui->filesList->setOpenByEnterKey(true);

    // SIGNAL/SLOTS
    connect(m_ui->selectAllButton, &QPushButton::clicked, m_ui->filesList, &TorrentContentWidget::checkAll);
    connect(m_ui->selectNoneButton, &QPushButton::clicked, m_ui->filesList, &TorrentContentWidget::checkNone);
    connect(m_ui->listWebSeeds, &QWidget::customContextMenuRequested, this, &PropertiesWidget::displayWebSeedListMenu);
    connect(m_ui->stackedProperties, &QStackedWidget::currentChanged, this, &PropertiesWidget::loadDynamicData);
    connect(BitTorrent::Session::instance(), &BitTorrent::Session::torrentSavePathChanged, this, &PropertiesWidget::updateSavePath);
    connect(BitTorrent::Session::instance(), &BitTorrent::Session::torrentMetadataReceived, this, &PropertiesWidget::updateTorrentInfos);
    connect(m_ui->filesList, &TorrentContentWidget::stateChanged, this, &PropertiesWidget::saveSettings);

    // set bar height relative to screen dpi
    const int barHeight = 18;

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
    m_ui->trackerUpButton->setIcon(UIThemeManager::instance()->getIcon(u"go-up"_s));
    m_ui->trackerUpButton->setIconSize(Utils::Gui::smallIconSize());
    m_ui->trackerDownButton->setIcon(UIThemeManager::instance()->getIcon(u"go-down"_s));
    m_ui->trackerDownButton->setIconSize(Utils::Gui::smallIconSize());
    connect(m_ui->trackerUpButton, &QPushButton::clicked, m_trackerList, &TrackerListWidget::decreaseSelectedTrackerTiers);
    connect(m_ui->trackerDownButton, &QPushButton::clicked, m_trackerList, &TrackerListWidget::increaseSelectedTrackerTiers);
    m_ui->hBoxLayoutTrackers->insertWidget(0, m_trackerList);
    // Peers list
    m_peerList = new PeerListWidget(this);
    m_ui->vBoxLayoutPeerPage->addWidget(m_peerList);
    // Tab bar
    m_tabBar = new PropTabBar(nullptr);
    m_tabBar->setContentsMargins(0, 5, 0, 5);
    m_ui->verticalLayout->addLayout(m_tabBar);
    connect(m_tabBar, &PropTabBar::tabChanged, m_ui->stackedProperties, &QStackedWidget::setCurrentIndex);
    connect(m_tabBar, &PropTabBar::tabChanged, this, &PropertiesWidget::saveSettings);
    connect(m_tabBar, &PropTabBar::visibilityToggled, this, &PropertiesWidget::setVisibility);
    connect(m_tabBar, &PropTabBar::visibilityToggled, this, &PropertiesWidget::saveSettings);

    const auto *editWebSeedsHotkey = new QShortcut(Qt::Key_F2, m_ui->listWebSeeds, nullptr, nullptr, Qt::WidgetShortcut);
    connect(editWebSeedsHotkey, &QShortcut::activated, this, &PropertiesWidget::editWebSeed);
    const auto *deleteWebSeedsHotkey = new QShortcut(QKeySequence::Delete, m_ui->listWebSeeds, nullptr, nullptr, Qt::WidgetShortcut);
    connect(deleteWebSeedsHotkey, &QShortcut::activated, this, &PropertiesWidget::deleteSelectedUrlSeeds);
    connect(m_ui->listWebSeeds, &QListWidget::doubleClicked, this, &PropertiesWidget::editWebSeed);

    configure();
    connect(Preferences::instance(), &Preferences::changed, this, &PropertiesWidget::configure);
}

PropertiesWidget::~PropertiesWidget()
{
    delete m_tabBar;
    delete m_ui;
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

void PropertiesWidget::setVisibility(const bool visible)
{
    if (!visible && (m_state == VISIBLE))
    {
        const int tabBarHeight = m_tabBar->geometry().height(); // take height before hiding
        auto *hSplitter = static_cast<QSplitter *>(parentWidget());
        m_ui->stackedProperties->setVisible(false);
        m_slideSizes = hSplitter->sizes();
        hSplitter->handle(1)->setVisible(false);
        hSplitter->handle(1)->setDisabled(true);
        m_handleWidth = hSplitter->handleWidth();
        hSplitter->setHandleWidth(0);
        const QList<int> sizes {(hSplitter->geometry().height() - tabBarHeight), tabBarHeight};
        hSplitter->setSizes(sizes);
        setMaximumSize(maximumSize().width(), tabBarHeight);
        m_state = REDUCED;
        return;
    }

    if (visible && (m_state == REDUCED))
    {
        m_ui->stackedProperties->setVisible(true);
        auto *hSplitter = static_cast<QSplitter *>(parentWidget());
        if (m_handleWidth != -1)
            hSplitter->setHandleWidth(m_handleWidth);
        hSplitter->handle(1)->setDisabled(false);
        hSplitter->handle(1)->setVisible(true);
        hSplitter->setSizes(m_slideSizes);
        m_state = VISIBLE;
        setMaximumSize(maximumSize().width(), QWIDGETSIZE_MAX);
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
    m_ui->labelPrivateVal->clear();
    m_ui->labelInfohash1Val->clear();
    m_ui->labelInfohash2Val->clear();
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
    m_ui->labelPopularityVal->clear();
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
    m_downloadedPieces->clear();
    m_piecesAvailability->clear();
    m_peerList->clear();
    m_contentFilterLine->clear();
}

BitTorrent::Torrent *PropertiesWidget::getCurrentTorrent() const
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

PropTabBar *PropertiesWidget::tabBar() const
{
    return m_tabBar;
}

LineEdit *PropertiesWidget::contentFilterLine() const
{
    return m_contentFilterLine;
}

void PropertiesWidget::updateSavePath(BitTorrent::Torrent *const torrent)
{
    if (torrent == m_torrent)
        m_ui->labelSavePathVal->setText(m_torrent->savePath().toString());
}

void PropertiesWidget::showContentFilterContextMenu()
{
    QMenu *menu = m_contentFilterLine->createStandardContextMenu();

    auto *formatMenu = new FilterPatternFormatMenu(m_storeFilterPatternFormat.get(FilterPatternFormat::Wildcards), menu);
    connect(formatMenu, &FilterPatternFormatMenu::patternFormatChanged, this, [this](const FilterPatternFormat format)
    {
        m_storeFilterPatternFormat = format;
        setContentFilterPattern();
    });

    menu->addSeparator();
    menu->addMenu(formatMenu);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->popup(QCursor::pos());
}

void PropertiesWidget::setContentFilterPattern()
{
    m_ui->filesList->setFilterPattern(m_contentFilterLine->text(), m_storeFilterPatternFormat.get(FilterPatternFormat::Wildcards));
}

void PropertiesWidget::updateTorrentInfos(BitTorrent::Torrent *const torrent)
{
    if (torrent == m_torrent)
        loadTorrentInfos(m_torrent);
}

void PropertiesWidget::loadTorrentInfos(BitTorrent::Torrent *const torrent)
{
    clear();
    m_torrent = torrent;
    m_downloadedPieces->setTorrent(m_torrent);
    m_piecesAvailability->setTorrent(m_torrent);
    m_trackerList->setTorrent(m_torrent);
    m_ui->filesList->setContentHandler(m_torrent);
    if (!m_torrent)
        return;

    // Save path
    updateSavePath(m_torrent);
    // Info hashes
    m_ui->labelInfohash1Val->setText(m_torrent->infoHash().v1().isValid() ? m_torrent->infoHash().v1().toString() : tr("N/A"));
    m_ui->labelInfohash2Val->setText(m_torrent->infoHash().v2().isValid() ? m_torrent->infoHash().v2().toString() : tr("N/A"));
    // URL seeds
    loadUrlSeeds();
    if (m_torrent->hasMetadata())
    {
        // Creation date
        m_ui->labelCreatedOnVal->setText(QLocale().toString(m_torrent->creationDate(), QLocale::ShortFormat));

        m_ui->labelTotalSizeVal->setText(Utils::Misc::friendlyUnit(m_torrent->totalSize()));

        // Comment
        m_ui->labelCommentVal->setText(Utils::Misc::parseHtmlLinks(m_torrent->comment().toHtmlEscaped()));

        m_ui->labelCreatedByVal->setText(m_torrent->creator());

        m_ui->labelPrivateVal->setText(m_torrent->isPrivate() ? tr("Yes") : tr("No"));
    }
    else
    {
        m_ui->labelPrivateVal->setText(tr("N/A"));
    }

    // Load dynamic data
    loadDynamicData();
}

void PropertiesWidget::readSettings()
{
    const Preferences *const pref = Preferences::instance();
    // Restore splitter sizes
    QStringList sizesStr = pref->getPropSplitterSizes().split(u',');
    if (sizesStr.size() == 2)
    {
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

    if (sizes.size() == 2)
        pref->setPropSplitterSizes(QString::number(sizes.first()) + u',' + QString::number(sizes.last()));
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
    switch (m_ui->stackedProperties->currentIndex())
    {
    case PropTabBar::MainTab:
        {
            m_ui->labelWastedVal->setText(Utils::Misc::friendlyUnit(m_torrent->wastedSize()));

            m_ui->labelUpTotalVal->setText(tr("%1 (%2 this session)").arg(Utils::Misc::friendlyUnit(m_torrent->totalUpload())
                , Utils::Misc::friendlyUnit(m_torrent->totalPayloadUpload())));

            m_ui->labelDlTotalVal->setText(tr("%1 (%2 this session)").arg(Utils::Misc::friendlyUnit(m_torrent->totalDownload())
                , Utils::Misc::friendlyUnit(m_torrent->totalPayloadDownload())));

            m_ui->labelUpLimitVal->setText(m_torrent->uploadLimit() <= 0 ? C_INFINITY : Utils::Misc::friendlyUnit(m_torrent->uploadLimit(), true));

            m_ui->labelDlLimitVal->setText(m_torrent->downloadLimit() <= 0 ? C_INFINITY : Utils::Misc::friendlyUnit(m_torrent->downloadLimit(), true));

            QString elapsedString;
            if (m_torrent->isFinished())
            {
                elapsedString = tr("%1 (seeded for %2)", "e.g. 4m39s (seeded for 3m10s)")
                        .arg(Utils::Misc::userFriendlyDuration(m_torrent->activeTime())
                                , Utils::Misc::userFriendlyDuration(m_torrent->finishedTime()));
            }
            else
            {
                elapsedString = Utils::Misc::userFriendlyDuration(m_torrent->activeTime());
            }
            m_ui->labelElapsedVal->setText(elapsedString);

            m_ui->labelConnectionsVal->setText(tr("%1 (%2 max)", "%1 and %2 are numbers, e.g. 3 (10 max)")
                                           .arg(m_torrent->connectionsCount())
                                           .arg(m_torrent->connectionsLimit() < 0 ? C_INFINITY : QString::number(m_torrent->connectionsLimit())));

            m_ui->labelETAVal->setText(Utils::Misc::userFriendlyDuration(m_torrent->eta(), MAX_ETA));

            // Update next announce time
            m_ui->labelReannounceInVal->setText(Utils::Misc::userFriendlyDuration(m_torrent->nextAnnounce()));

            // Update ratio info
            const qreal ratio = m_torrent->realRatio();
            m_ui->labelShareRatioVal->setText(ratio > BitTorrent::Torrent::MAX_RATIO ? C_INFINITY : Utils::String::fromDouble(ratio, 2));

            const qreal popularity = m_torrent->popularity();
            m_ui->labelPopularityVal->setText(popularity > BitTorrent::Torrent::MAX_RATIO ? C_INFINITY : Utils::String::fromDouble(popularity, 2));

            m_ui->labelSeedsVal->setText(tr("%1 (%2 total)", "%1 and %2 are numbers, e.g. 3 (10 total)")
                .arg(QString::number(m_torrent->seedsCount())
                    , QString::number(m_torrent->totalSeedsCount())));

            m_ui->labelPeersVal->setText(tr("%1 (%2 total)", "%1 and %2 are numbers, e.g. 3 (10 total)")
                .arg(QString::number(m_torrent->leechsCount())
                    , QString::number(m_torrent->totalLeechersCount())));

            const qlonglong dlDuration = m_torrent->activeTime() - m_torrent->finishedTime();
            const QString dlAvg = Utils::Misc::friendlyUnit((m_torrent->totalDownload() / ((dlDuration == 0) ? -1 : dlDuration)), true);
            m_ui->labelDlSpeedVal->setText(tr("%1 (%2 avg.)", "%1 and %2 are speed rates, e.g. 200KiB/s (100KiB/s avg.)")
                .arg(Utils::Misc::friendlyUnit(m_torrent->downloadPayloadRate(), true), dlAvg));

            const qlonglong ulDuration = m_torrent->activeTime();
            const QString ulAvg = Utils::Misc::friendlyUnit((m_torrent->totalUpload() / ((ulDuration == 0) ? -1 : ulDuration)), true);
            m_ui->labelUpSpeedVal->setText(tr("%1 (%2 avg.)", "%1 and %2 are speed rates, e.g. 200KiB/s (100KiB/s avg.)")
                .arg(Utils::Misc::friendlyUnit(m_torrent->uploadPayloadRate(), true), ulAvg));

            m_ui->labelLastSeenCompleteVal->setText(m_torrent->lastSeenComplete().isValid() ? QLocale().toString(m_torrent->lastSeenComplete(), QLocale::ShortFormat) : tr("Never"));

            m_ui->labelCompletedOnVal->setText(m_torrent->completedTime().isValid() ? QLocale().toString(m_torrent->completedTime(), QLocale::ShortFormat) : QString {});

            m_ui->labelAddedOnVal->setText(QLocale().toString(m_torrent->addedTime(), QLocale::ShortFormat));

            if (m_torrent->hasMetadata())
            {
                using TorrentPtr = QPointer<BitTorrent::Torrent>;

                m_ui->labelTotalPiecesVal->setText(tr("%1 x %2 (have %3)", "(torrent pieces) eg 152 x 4MB (have 25)").arg(m_torrent->piecesCount()).arg(Utils::Misc::friendlyUnit(m_torrent->pieceLength())).arg(m_torrent->piecesHave()));

                if (!m_torrent->isFinished() && !m_torrent->isStopped() && !m_torrent->isQueued() && !m_torrent->isChecking())
                {
                    // Pieces availability
                    showPiecesAvailability(true);
                    m_torrent->fetchPieceAvailability([this, torrent = TorrentPtr(m_torrent)](const QList<int> &pieceAvailability)
                    {
                        if (torrent == m_torrent)
                            m_piecesAvailability->setAvailability(pieceAvailability);
                    });

                    m_ui->labelAverageAvailabilityVal->setText(Utils::String::fromDouble(m_torrent->distributedCopies(), 3));
                }
                else
                {
                    showPiecesAvailability(false);
                }

                // Progress
                qreal progress = m_torrent->progress() * 100.;
                m_ui->labelProgressVal->setText(Utils::String::fromDouble(progress, 1) + u'%');

                m_torrent->fetchDownloadingPieces([this, torrent = TorrentPtr(m_torrent)](const QBitArray &downloadingPieces)
                {
                    if (torrent == m_torrent)
                        m_downloadedPieces->setProgress(m_torrent->pieces(), downloadingPieces);
                });
            }
            else
            {
                showPiecesAvailability(false);
            }
        }
        break;
    case PropTabBar::PeersTab:
        // Load peers
        m_peerList->loadPeers(m_torrent);
        break;
    case PropTabBar::FilesTab:
        m_ui->filesList->refresh();
        break;
    default:;
    }
}

void PropertiesWidget::loadUrlSeeds()
{
    if (!m_torrent)
        return;

    using TorrentPtr = QPointer<BitTorrent::Torrent>;
    m_torrent->fetchURLSeeds([this, torrent = TorrentPtr(m_torrent)](const QList<QUrl> &urlSeeds)
    {
        if (torrent != m_torrent)
            return;

        m_ui->listWebSeeds->clear();
        qDebug("Loading web seeds");
        // Add url seeds
        for (const QUrl &urlSeed : urlSeeds)
        {
            qDebug("Loading web seed: %s", qUtf8Printable(urlSeed.toString()));
            new QListWidgetItem(urlSeed.toString(), m_ui->listWebSeeds);
        }
    });
}

void PropertiesWidget::displayWebSeedListMenu()
{
    if (!m_torrent) return;

    const QModelIndexList rows = m_ui->listWebSeeds->selectionModel()->selectedRows();

    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    menu->addAction(UIThemeManager::instance()->getIcon(u"list-add"_s), tr("Add web seed..."), this, &PropertiesWidget::askWebSeed);

    if (!rows.isEmpty())
    {
        menu->addAction(UIThemeManager::instance()->getIcon(u"edit-clear"_s, u"list-remove"_s), tr("Remove web seed")
            , this, &PropertiesWidget::deleteSelectedUrlSeeds);
        menu->addSeparator();
        menu->addAction(UIThemeManager::instance()->getIcon(u"edit-copy"_s), tr("Copy web seed URL")
            , this, &PropertiesWidget::copySelectedWebSeedsToClipboard);
        menu->addAction(UIThemeManager::instance()->getIcon(u"edit-rename"_s), tr("Edit web seed URL...")
            , this, &PropertiesWidget::editWebSeed);
    }

    menu->popup(QCursor::pos());
}

void PropertiesWidget::configure()
{
    // Speed widget
    if (Preferences::instance()->isSpeedWidgetEnabled())
    {
        if (!qobject_cast<SpeedWidget *>(m_speedWidget))
        {
            if (m_speedWidget)
            {
                m_ui->speedLayout->removeWidget(m_speedWidget);
                delete m_speedWidget;
            }

            m_speedWidget = new SpeedWidget(this);
            m_ui->speedLayout->addWidget(m_speedWidget);
        }
    }
    else
    {
        if (!qobject_cast<QLabel *>(m_speedWidget))
        {
            if (m_speedWidget)
            {
                m_ui->speedLayout->removeWidget(m_speedWidget);
                delete m_speedWidget;
            }

            const auto displayText = u"<center><b>%1</b><p>%2</p></center>"_s
                .arg(tr("Speed graphs are disabled"), tr("You can enable it in Advanced Options"));
            auto *label = new QLabel(displayText, this);
            label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            m_speedWidget = label;
            m_ui->speedLayout->addWidget(m_speedWidget);
        }
    }
}

void PropertiesWidget::askWebSeed()
{
    bool ok = false;
    // Ask user for a new url seed
    const QString urlSeed = AutoExpandableDialog::getText(this, tr("Add web seed", "Add HTTP source"),
                                                           tr("Add web seed:"), QLineEdit::Normal,
                                                           u"http://www."_s, &ok);
    if (!ok) return;
    qDebug("Adding %s web seed", qUtf8Printable(urlSeed));
    if (!m_ui->listWebSeeds->findItems(urlSeed, Qt::MatchFixedString).empty())
    {
        QMessageBox::warning(this, u"qBittorrent"_s, tr("This web seed is already in the list."), QMessageBox::Ok);
        return;
    }
    if (m_torrent)
        m_torrent->addUrlSeeds({urlSeed});
    // Refresh the seeds list
    loadUrlSeeds();
}

void PropertiesWidget::deleteSelectedUrlSeeds()
{
    const QList<QListWidgetItem *> selectedItems = m_ui->listWebSeeds->selectedItems();
    if (selectedItems.isEmpty()) return;

    QList<QUrl> urlSeeds;
    urlSeeds.reserve(selectedItems.size());

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

    QApplication::clipboard()->setText(urlsToCopy.join(u'\n'));
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

    if (!m_ui->listWebSeeds->findItems(newSeed, Qt::MatchFixedString).empty())
    {
        QMessageBox::warning(this, u"qBittorrent"_s,
                             tr("This web seed is already in the list."),
                             QMessageBox::Ok);
        return;
    }

    m_torrent->removeUrlSeeds({oldSeed});
    m_torrent->addUrlSeeds({newSeed});
    loadUrlSeeds();
}
