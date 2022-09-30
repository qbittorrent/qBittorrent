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

#include "transferlistfilterswidget.h"

#include <QCheckBox>
#include <QIcon>
#include <QListWidgetItem>
#include <QMenu>
#include <QPainter>
#include <QScrollArea>
#include <QStyleOptionButton>
#include <QUrl>
#include <QVBoxLayout>

#include "base/algorithm.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/net/downloadmanager.h"
#include "base/preferences.h"
#include "base/torrentfilter.h"
#include "base/utils/compare.h"
#include "base/utils/fs.h"
#include "categoryfilterwidget.h"
#include "tagfilterwidget.h"
#include "transferlistwidget.h"
#include "uithememanager.h"
#include "utils.h"

namespace
{
    enum TRACKER_FILTER_ROW
    {
        ALL_ROW,
        TRACKERLESS_ROW,
        ERROR_ROW,
        WARNING_ROW
    };

    QString getScheme(const QString &tracker)
    {
        const QUrl url {tracker};
        QString scheme = url.scheme();
        if (scheme.isEmpty())
            scheme = u"http"_qs;
        return scheme;
    }

    QString getHost(const QString &tracker)
    {
        // We want the domain + tld. Subdomains should be disregarded
        const QUrl url {tracker};
        const QString host {url.host()};

        // host is in IP format
        if (!QHostAddress(host).isNull())
            return host;

        return host.section(u'.', -2, -1);
    }

    class ArrowCheckBox final : public QCheckBox
    {
    public:
        using QCheckBox::QCheckBox;

    private:
        void paintEvent(QPaintEvent *) override
        {
            QPainter painter(this);

            QStyleOptionViewItem indicatorOption;
            indicatorOption.initFrom(this);
            indicatorOption.rect = style()->subElementRect(QStyle::SE_CheckBoxIndicator, &indicatorOption, this);
            indicatorOption.state |= (QStyle::State_Children
                                      | (isChecked() ? QStyle::State_Open : QStyle::State_None));
            style()->drawPrimitive(QStyle::PE_IndicatorBranch, &indicatorOption, &painter, this);

            QStyleOptionButton labelOption;
            initStyleOption(&labelOption);
            labelOption.rect = style()->subElementRect(QStyle::SE_CheckBoxContents, &labelOption, this);
            style()->drawControl(QStyle::CE_CheckBoxLabel, &labelOption, &painter, this);
        }
    };

    const QString NULL_HOST = u""_qs;
}

BaseFilterWidget::BaseFilterWidget(QWidget *parent, TransferListWidget *transferList)
    : QListWidget(parent)
    , transferList(transferList)
{
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setUniformItemSizes(true);
    setSpacing(0);

    setIconSize(Utils::Gui::smallIconSize());

#if defined(Q_OS_MACOS)
    setAttribute(Qt::WA_MacShowFocusRect, false);
#endif

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &BaseFilterWidget::customContextMenuRequested, this, &BaseFilterWidget::showMenu);
    connect(this, &BaseFilterWidget::currentRowChanged, this, &BaseFilterWidget::applyFilter);

    connect(BitTorrent::Session::instance(), &BitTorrent::Session::torrentsLoaded
            , this, &BaseFilterWidget::handleTorrentsLoaded);
    connect(BitTorrent::Session::instance(), &BitTorrent::Session::torrentAboutToBeRemoved
            , this, &BaseFilterWidget::torrentAboutToBeDeleted);
}

QSize BaseFilterWidget::sizeHint() const
{
    return
    {
        // Width should be exactly the width of the content
        sizeHintForColumn(0),
        // Height should be exactly the height of the content
        static_cast<int>((sizeHintForRow(0) + 2 * spacing()) * (count() + 0.5)),
    };
}

QSize BaseFilterWidget::minimumSizeHint() const
{
    QSize size = sizeHint();
    size.setWidth(6);
    return size;
}

void BaseFilterWidget::toggleFilter(bool checked)
{
    setVisible(checked);
    if (checked)
        applyFilter(currentRow());
    else
        applyFilter(ALL_ROW);
}

StatusFilterWidget::StatusFilterWidget(QWidget *parent, TransferListWidget *transferList)
    : BaseFilterWidget(parent, transferList)
{
    // Add status filters
    auto *all = new QListWidgetItem(this);
    all->setData(Qt::DisplayRole, tr("All (0)", "this is for the status filter"));
    all->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"filter-all"_qs));
    auto *downloading = new QListWidgetItem(this);
    downloading->setData(Qt::DisplayRole, tr("Downloading (0)"));
    downloading->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"downloading"_qs));
    auto *seeding = new QListWidgetItem(this);
    seeding->setData(Qt::DisplayRole, tr("Seeding (0)"));
    seeding->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"upload"_qs));
    auto *completed = new QListWidgetItem(this);
    completed->setData(Qt::DisplayRole, tr("Completed (0)"));
    completed->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"checked-completed"_qs));
    auto *resumed = new QListWidgetItem(this);
    resumed->setData(Qt::DisplayRole, tr("Resumed (0)"));
    resumed->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"torrent-start"_qs));
    auto *paused = new QListWidgetItem(this);
    paused->setData(Qt::DisplayRole, tr("Paused (0)"));
    paused->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"torrent-stop"_qs));
    auto *active = new QListWidgetItem(this);
    active->setData(Qt::DisplayRole, tr("Active (0)"));
    active->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"filter-active"_qs));
    auto *inactive = new QListWidgetItem(this);
    inactive->setData(Qt::DisplayRole, tr("Inactive (0)"));
    inactive->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"filter-inactive"_qs));
    auto *stalled = new QListWidgetItem(this);
    stalled->setData(Qt::DisplayRole, tr("Stalled (0)"));
    stalled->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"filter-stalled"_qs));
    auto *stalledUploading = new QListWidgetItem(this);
    stalledUploading->setData(Qt::DisplayRole, tr("Stalled Uploading (0)"));
    stalledUploading->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"stalledUP"_qs));
    auto *stalledDownloading = new QListWidgetItem(this);
    stalledDownloading->setData(Qt::DisplayRole, tr("Stalled Downloading (0)"));
    stalledDownloading->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"stalledDL"_qs));
    auto *checking = new QListWidgetItem(this);
    checking->setData(Qt::DisplayRole, tr("Checking (0)"));
    checking->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"force-recheck"_qs));
    auto *errored = new QListWidgetItem(this);
    errored->setData(Qt::DisplayRole, tr("Errored (0)"));
    errored->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"error"_qs));

    const Preferences *const pref = Preferences::instance();
    setCurrentRow(pref->getTransSelFilter(), QItemSelectionModel::SelectCurrent);
    toggleFilter(pref->getStatusFilterState());

    populate();

    connect(BitTorrent::Session::instance(), &BitTorrent::Session::torrentsUpdated
            , this, &StatusFilterWidget::handleTorrentsUpdated);
}

StatusFilterWidget::~StatusFilterWidget()
{
    Preferences::instance()->setTransSelFilter(currentRow());
}

void StatusFilterWidget::populate()
{
    m_torrentsStatus.clear();

    const QVector<BitTorrent::Torrent *> torrents = BitTorrent::Session::instance()->torrents();
    for (const BitTorrent::Torrent *torrent : torrents)
        updateTorrentStatus(torrent);

    updateTexts();
}

void StatusFilterWidget::updateTorrentStatus(const BitTorrent::Torrent *torrent)
{
    TorrentFilterBitset &torrentStatus = m_torrentsStatus[torrent];

    const auto update = [torrent, &torrentStatus](const TorrentFilter::Type status, int &counter)
    {
        const bool hasStatus = torrentStatus[status];
        const bool needStatus = TorrentFilter(status).match(torrent);
        if (needStatus && !hasStatus)
        {
            ++counter;
            torrentStatus.set(status);
        }
        else if (!needStatus && hasStatus)
        {
            --counter;
            torrentStatus.reset(status);
        }
    };

    update(TorrentFilter::Downloading, m_nbDownloading);
    update(TorrentFilter::Seeding, m_nbSeeding);
    update(TorrentFilter::Completed, m_nbCompleted);
    update(TorrentFilter::Resumed, m_nbResumed);
    update(TorrentFilter::Paused, m_nbPaused);
    update(TorrentFilter::Active, m_nbActive);
    update(TorrentFilter::Inactive, m_nbInactive);
    update(TorrentFilter::StalledUploading, m_nbStalledUploading);
    update(TorrentFilter::StalledDownloading, m_nbStalledDownloading);
    update(TorrentFilter::Checking, m_nbChecking);
    update(TorrentFilter::Errored, m_nbErrored);

    m_nbStalled = m_nbStalledUploading + m_nbStalledDownloading;
}

void StatusFilterWidget::updateTexts()
{
    const qsizetype torrentsCount = BitTorrent::Session::instance()->torrentsCount();
    item(TorrentFilter::All)->setData(Qt::DisplayRole, tr("All (%1)").arg(torrentsCount));
    item(TorrentFilter::Downloading)->setData(Qt::DisplayRole, tr("Downloading (%1)").arg(m_nbDownloading));
    item(TorrentFilter::Seeding)->setData(Qt::DisplayRole, tr("Seeding (%1)").arg(m_nbSeeding));
    item(TorrentFilter::Completed)->setData(Qt::DisplayRole, tr("Completed (%1)").arg(m_nbCompleted));
    item(TorrentFilter::Resumed)->setData(Qt::DisplayRole, tr("Resumed (%1)").arg(m_nbResumed));
    item(TorrentFilter::Paused)->setData(Qt::DisplayRole, tr("Paused (%1)").arg(m_nbPaused));
    item(TorrentFilter::Active)->setData(Qt::DisplayRole, tr("Active (%1)").arg(m_nbActive));
    item(TorrentFilter::Inactive)->setData(Qt::DisplayRole, tr("Inactive (%1)").arg(m_nbInactive));
    item(TorrentFilter::Stalled)->setData(Qt::DisplayRole, tr("Stalled (%1)").arg(m_nbStalled));
    item(TorrentFilter::StalledUploading)->setData(Qt::DisplayRole, tr("Stalled Uploading (%1)").arg(m_nbStalledUploading));
    item(TorrentFilter::StalledDownloading)->setData(Qt::DisplayRole, tr("Stalled Downloading (%1)").arg(m_nbStalledDownloading));
    item(TorrentFilter::Checking)->setData(Qt::DisplayRole, tr("Checking (%1)").arg(m_nbChecking));
    item(TorrentFilter::Errored)->setData(Qt::DisplayRole, tr("Errored (%1)").arg(m_nbErrored));
}

void StatusFilterWidget::handleTorrentsUpdated(const QVector<BitTorrent::Torrent *> torrents)
{
    for (const BitTorrent::Torrent *torrent : torrents)
        updateTorrentStatus(torrent);

    updateTexts();
}

void StatusFilterWidget::showMenu()
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    menu->addAction(UIThemeManager::instance()->getIcon(u"torrent-start"_qs), tr("Resume torrents")
        , transferList, &TransferListWidget::startVisibleTorrents);
    menu->addAction(UIThemeManager::instance()->getIcon(u"torrent-stop"_qs), tr("Pause torrents")
        , transferList, &TransferListWidget::pauseVisibleTorrents);
    menu->addAction(UIThemeManager::instance()->getIcon(u"list-remove"_qs), tr("Remove torrents")
        , transferList, &TransferListWidget::deleteVisibleTorrents);

    menu->popup(QCursor::pos());
}

void StatusFilterWidget::applyFilter(int row)
{
    transferList->applyStatusFilter(row);
}

void StatusFilterWidget::handleTorrentsLoaded(const QVector<BitTorrent::Torrent *> &torrents)
{
    for (const BitTorrent::Torrent *torrent : torrents)
        updateTorrentStatus(torrent);

    updateTexts();
}

void StatusFilterWidget::torrentAboutToBeDeleted(BitTorrent::Torrent *const torrent)
{
    const TorrentFilterBitset status = m_torrentsStatus.take(torrent);

    if (status[TorrentFilter::Downloading])
        --m_nbDownloading;
    if (status[TorrentFilter::Seeding])
        --m_nbSeeding;
    if (status[TorrentFilter::Completed])
        --m_nbCompleted;
    if (status[TorrentFilter::Resumed])
        --m_nbResumed;
    if (status[TorrentFilter::Paused])
        --m_nbPaused;
    if (status[TorrentFilter::Active])
        --m_nbActive;
    if (status[TorrentFilter::Inactive])
        --m_nbInactive;
    if (status[TorrentFilter::StalledUploading])
        --m_nbStalledUploading;
    if (status[TorrentFilter::StalledDownloading])
        --m_nbStalledDownloading;
    if (status[TorrentFilter::Checking])
        --m_nbChecking;
    if (status[TorrentFilter::Errored])
        --m_nbErrored;

    m_nbStalled = m_nbStalledUploading + m_nbStalledDownloading;

    updateTexts();
}

TrackerFiltersList::TrackerFiltersList(QWidget *parent, TransferListWidget *transferList, const bool downloadFavicon)
    : BaseFilterWidget(parent, transferList)
    , m_totalTorrents(0)
    , m_downloadTrackerFavicon(downloadFavicon)
{
    auto *allTrackers = new QListWidgetItem(this);
    allTrackers->setData(Qt::DisplayRole, tr("All (0)", "this is for the tracker filter"));
    allTrackers->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"trackers"_qs));
    auto *noTracker = new QListWidgetItem(this);
    noTracker->setData(Qt::DisplayRole, tr("Trackerless (0)"));
    noTracker->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"trackerless"_qs));
    auto *errorTracker = new QListWidgetItem(this);
    errorTracker->setData(Qt::DisplayRole, tr("Error (0)"));
    errorTracker->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"tracker-error"_qs));
    auto *warningTracker = new QListWidgetItem(this);
    warningTracker->setData(Qt::DisplayRole, tr("Warning (0)"));
    warningTracker->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"tracker-warning"_qs));

    m_trackers[NULL_HOST] = {{}, noTracker};

    handleTorrentsLoaded(BitTorrent::Session::instance()->torrents());

    setCurrentRow(0, QItemSelectionModel::SelectCurrent);
    toggleFilter(Preferences::instance()->getTrackerFilterState());
}

TrackerFiltersList::~TrackerFiltersList()
{
    for (const Path &iconPath : asConst(m_iconPaths))
        Utils::Fs::removeFile(iconPath);
}

void TrackerFiltersList::addTrackers(const BitTorrent::Torrent *torrent, const QVector<BitTorrent::TrackerEntry> &trackers)
{
    const BitTorrent::TorrentID torrentID = torrent->id();
    for (const BitTorrent::TrackerEntry &tracker : trackers)
        addItems(tracker.url, {torrentID});
}

void TrackerFiltersList::removeTrackers(const BitTorrent::Torrent *torrent, const QStringList &trackers)
{
    const BitTorrent::TorrentID torrentID = torrent->id();
    for (const QString &tracker : trackers)
        removeItem(tracker, torrentID);
}

void TrackerFiltersList::refreshTrackers(const BitTorrent::Torrent *torrent)
{
    const BitTorrent::TorrentID torrentID = torrent->id();

    m_errors.remove(torrentID);
    m_warnings.remove(torrentID);

    Algorithm::removeIf(m_trackers, [this, &torrentID](const QString &host, TrackerData &trackerData)
    {
        QSet<BitTorrent::TorrentID> &torrentIDs = trackerData.torrents;
        if (!torrentIDs.remove(torrentID))
            return false;

        QListWidgetItem *trackerItem = trackerData.item;

        if (!host.isEmpty() && torrentIDs.isEmpty())
        {
            if (currentItem() == trackerItem)
                setCurrentRow(0, QItemSelectionModel::SelectCurrent);
            delete trackerItem;
            return true;
        }

        trackerItem->setText(u"%1 (%2)"_qs.arg((host.isEmpty() ? tr("Trackerless") : host), QString::number(torrentIDs.size())));
        return false;
    });

    const QVector<BitTorrent::TrackerEntry> trackerEntries = torrent->trackers();
    const bool isTrackerless = trackerEntries.isEmpty();
    if (isTrackerless)
    {
        addItems(NULL_HOST, {torrentID});
    }
    else
    {
        for (const BitTorrent::TrackerEntry &trackerEntry : trackerEntries)
            addItems(trackerEntry.url, {torrentID});
    }

    updateGeometry();
}

void TrackerFiltersList::changeTrackerless(const BitTorrent::Torrent *torrent, const bool trackerless)
{
    if (trackerless)
        addItems(NULL_HOST, {torrent->id()});
    else
        removeItem(NULL_HOST, torrent->id());
}

void TrackerFiltersList::addItems(const QString &trackerURL, const QVector<BitTorrent::TorrentID> &torrents)
{
    const QString host = getHost(trackerURL);
    auto trackersIt = m_trackers.find(host);
    const bool exists = (trackersIt != m_trackers.end());
    QListWidgetItem *trackerItem = nullptr;

    if (exists)
    {
        trackerItem = trackersIt->item;
    }
    else
    {
        trackerItem = new QListWidgetItem();
        trackerItem->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"trackers"_qs));

        const TrackerData trackerData {{}, trackerItem};
        trackersIt = m_trackers.insert(host, trackerData);

        const QString scheme = getScheme(trackerURL);
        downloadFavicon(u"%1://%2/favicon.ico"_qs.arg((scheme.startsWith(u"http") ? scheme : u"http"_qs), host));
    }

    Q_ASSERT(trackerItem);

    QSet<BitTorrent::TorrentID> &torrentIDs = trackersIt->torrents;
    for (const BitTorrent::TorrentID &torrentID : torrents)
        torrentIDs.insert(torrentID);

    trackerItem->setText(u"%1 (%2)"_qs.arg(((host == NULL_HOST) ? tr("Trackerless") : host), QString::number(torrentIDs.size())));
    if (exists)
    {
        if (item(currentRow()) == trackerItem)
            applyFilter(currentRow());
        return;
    }

    Q_ASSERT(count() >= 4);
    const Utils::Compare::NaturalLessThan<Qt::CaseSensitive> naturalLessThan {};
    int insPos = count();
    for (int i = 4; i < count(); ++i)
    {
        if (naturalLessThan(host, item(i)->text()))
        {
            insPos = i;
            break;
        }
    }
    QListWidget::insertItem(insPos, trackerItem);
    updateGeometry();
}

void TrackerFiltersList::removeItem(const QString &trackerURL, const BitTorrent::TorrentID &id)
{
    const QString host = getHost(trackerURL);
    QSet<BitTorrent::TorrentID> torrentIDs = m_trackers.value(host).torrents;
    if (torrentIDs.empty())
        return;

    torrentIDs.remove(id);

    QListWidgetItem *trackerItem = nullptr;

    if (!host.isEmpty())
    {
        // Remove from 'Error' and 'Warning' view
        const auto errorHashesIt = m_errors.find(id);
        if (errorHashesIt != m_errors.end())
        {
            QSet<QString> &errored = errorHashesIt.value();
            errored.remove(trackerURL);
            if (errored.isEmpty())
            {
                m_errors.erase(errorHashesIt);
                item(ERROR_ROW)->setText(tr("Error (%1)").arg(m_errors.size()));
                if (currentRow() == ERROR_ROW)
                    applyFilter(ERROR_ROW);
            }
        }

        const auto warningHashesIt = m_warnings.find(id);
        if (warningHashesIt != m_warnings.end())
        {
            QSet<QString> &warned = *warningHashesIt;
            warned.remove(trackerURL);
            if (warned.isEmpty())
            {
                m_warnings.erase(warningHashesIt);
                item(WARNING_ROW)->setText(tr("Warning (%1)").arg(m_warnings.size()));
                if (currentRow() == WARNING_ROW)
                    applyFilter(WARNING_ROW);
            }
        }

        trackerItem = m_trackers.value(host).item;

        if (torrentIDs.isEmpty())
        {
            if (currentItem() == trackerItem)
                setCurrentRow(0, QItemSelectionModel::SelectCurrent);
            delete trackerItem;
            m_trackers.remove(host);
            updateGeometry();
            return;
        }

        if (trackerItem)
            trackerItem->setText(u"%1 (%2)"_qs.arg(host, QString::number(torrentIDs.size())));
    }
    else
    {
        trackerItem = item(TRACKERLESS_ROW);
        trackerItem->setText(tr("Trackerless (%1)").arg(torrentIDs.size()));
    }

    m_trackers.insert(host, {torrentIDs, trackerItem});

    if (currentItem() == trackerItem)
        applyFilter(currentRow());
}

void TrackerFiltersList::setDownloadTrackerFavicon(bool value)
{
    if (value == m_downloadTrackerFavicon) return;
    m_downloadTrackerFavicon = value;

    if (m_downloadTrackerFavicon)
    {
        for (auto i = m_trackers.cbegin(); i != m_trackers.cend(); ++i)
        {
            const QString &tracker = i.key();
            if (!tracker.isEmpty())
            {
                const QString scheme = getScheme(tracker);
                downloadFavicon(u"%1://%2/favicon.ico"_qs
                                .arg((scheme.startsWith(u"http") ? scheme : u"http"_qs), getHost(tracker)));
             }
        }
    }
}

void TrackerFiltersList::handleTrackerEntriesUpdated(const QHash<BitTorrent::Torrent *, QSet<QString>> &updateInfos)
{
    for (auto torrentsIt = updateInfos.cbegin(); torrentsIt != updateInfos.cend(); ++torrentsIt)
    {
        const BitTorrent::Torrent *torrent = torrentsIt.key();
        const QSet<QString> &trackerURLs = torrentsIt.value();
        const BitTorrent::TorrentID id = torrent->id();

        auto errorHashesIt = m_errors.find(id);
        auto warningHashesIt = m_warnings.find(id);

        const QVector<BitTorrent::TrackerEntry> trackers = torrent->trackers();
        for (const BitTorrent::TrackerEntry &trackerEntry : trackers)
        {
            if (!trackerURLs.contains(trackerEntry.url))
                continue;

            if (trackerEntry.status == BitTorrent::TrackerEntry::Working)
            {
                if (errorHashesIt != m_errors.end())
                {
                    QSet<QString> &errored = errorHashesIt.value();
                    errored.remove(trackerEntry.url);
                }

                if (trackerEntry.message.isEmpty())
                {
                    if (warningHashesIt != m_warnings.end())
                    {
                        QSet<QString> &warned = *warningHashesIt;
                        warned.remove(trackerEntry.url);
                    }
                }
                else
                {
                    if (warningHashesIt == m_warnings.end())
                        warningHashesIt = m_warnings.insert(id, {});
                    warningHashesIt.value().insert(trackerEntry.url);
                }
            }
            else if (trackerEntry.status == BitTorrent::TrackerEntry::NotWorking)
            {
                if (errorHashesIt == m_errors.end())
                    errorHashesIt = m_errors.insert(id, {});
                errorHashesIt.value().insert(trackerEntry.url);
            }
        }

        if ((errorHashesIt != m_errors.end()) && errorHashesIt.value().isEmpty())
            m_errors.erase(errorHashesIt);
        if ((warningHashesIt != m_warnings.end()) && warningHashesIt.value().isEmpty())
            m_warnings.erase(warningHashesIt);
    }

    item(ERROR_ROW)->setText(tr("Error (%1)").arg(m_errors.size()));
    item(WARNING_ROW)->setText(tr("Warning (%1)").arg(m_warnings.size()));

    if (currentRow() == ERROR_ROW)
        applyFilter(ERROR_ROW);
    else if (currentRow() == WARNING_ROW)
        applyFilter(WARNING_ROW);
}

void TrackerFiltersList::downloadFavicon(const QString &url)
{
    if (!m_downloadTrackerFavicon) return;
    Net::DownloadManager::instance()->download(
                Net::DownloadRequest(url).saveToFile(true)
                , this, &TrackerFiltersList::handleFavicoDownloadFinished);
}

void TrackerFiltersList::handleFavicoDownloadFinished(const Net::DownloadResult &result)
{
    if (result.status != Net::DownloadStatus::Success)
    {
        if (result.url.endsWith(u".ico", Qt::CaseInsensitive))
            downloadFavicon(result.url.left(result.url.size() - 4) + u".png");
        return;
    }

    const QString host = getHost(result.url);

    if (!m_trackers.contains(host))
    {
        Utils::Fs::removeFile(result.filePath);
        return;
    }

    QListWidgetItem *trackerItem = item(rowFromTracker(host));
    if (!trackerItem) return;

    const QIcon icon {result.filePath.data()};
    //Detect a non-decodable icon
    QList<QSize> sizes = icon.availableSizes();
    bool invalid = (sizes.isEmpty() || icon.pixmap(sizes.first()).isNull());
    if (invalid)
    {
        if (result.url.endsWith(u".ico", Qt::CaseInsensitive))
            downloadFavicon(result.url.left(result.url.size() - 4) + u".png");
        Utils::Fs::removeFile(result.filePath);
    }
    else
    {
        trackerItem->setData(Qt::DecorationRole, QIcon(result.filePath.data()));
        m_iconPaths.append(result.filePath);
    }
}

void TrackerFiltersList::showMenu()
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    menu->addAction(UIThemeManager::instance()->getIcon(u"torrent-start"_qs), tr("Resume torrents")
        , transferList, &TransferListWidget::startVisibleTorrents);
    menu->addAction(UIThemeManager::instance()->getIcon(u"torrent-stop"_qs), tr("Pause torrents")
        , transferList, &TransferListWidget::pauseVisibleTorrents);
    menu->addAction(UIThemeManager::instance()->getIcon(u"list-remove"_qs), tr("Remove torrents")
        , transferList, &TransferListWidget::deleteVisibleTorrents);

    menu->popup(QCursor::pos());
}

void TrackerFiltersList::applyFilter(const int row)
{
    if (row == ALL_ROW)
        transferList->applyTrackerFilterAll();
    else if (isVisible())
        transferList->applyTrackerFilter(getTorrentIDs(row));
}

void TrackerFiltersList::handleTorrentsLoaded(const QVector<BitTorrent::Torrent *> &torrents)
{
    QHash<QString, QVector<BitTorrent::TorrentID>> torrentsPerTracker;
    for (const BitTorrent::Torrent *torrent : torrents)
    {
        const BitTorrent::TorrentID torrentID = torrent->id();
        const QVector<BitTorrent::TrackerEntry> trackers = torrent->trackers();
        for (const BitTorrent::TrackerEntry &tracker : trackers)
            torrentsPerTracker[tracker.url].append(torrentID);

        // Check for trackerless torrent
        if (trackers.isEmpty())
            torrentsPerTracker[NULL_HOST].append(torrentID);
    }

    for (auto it = torrentsPerTracker.cbegin(); it != torrentsPerTracker.cend(); ++it)
    {
        const QString &trackerURL = it.key();
        const QVector<BitTorrent::TorrentID> &torrents = it.value();
        addItems(trackerURL, torrents);
    }

    m_totalTorrents += torrents.count();
    item(ALL_ROW)->setText(tr("All (%1)", "this is for the tracker filter").arg(m_totalTorrents));
}

void TrackerFiltersList::torrentAboutToBeDeleted(BitTorrent::Torrent *const torrent)
{
    const BitTorrent::TorrentID torrentID = torrent->id();
    const QVector<BitTorrent::TrackerEntry> trackers = torrent->trackers();
    for (const BitTorrent::TrackerEntry &tracker : trackers)
        removeItem(tracker.url, torrentID);

    // Check for trackerless torrent
    if (trackers.isEmpty())
        removeItem(NULL_HOST, torrentID);

    item(ALL_ROW)->setText(tr("All (%1)", "this is for the tracker filter").arg(--m_totalTorrents));
}

QString TrackerFiltersList::trackerFromRow(int row) const
{
    Q_ASSERT(row > 1);
    const QString tracker = item(row)->text();
    QStringList parts = tracker.split(u' ');
    Q_ASSERT(parts.size() >= 2);
    parts.removeLast(); // Remove trailing number
    return parts.join(u' ');
}

int TrackerFiltersList::rowFromTracker(const QString &tracker) const
{
    Q_ASSERT(!tracker.isEmpty());
    for (int i = 4; i < count(); ++i)
    {
        if (tracker == trackerFromRow(i))
            return i;
    }
    return -1;
}

QSet<BitTorrent::TorrentID> TrackerFiltersList::getTorrentIDs(const int row) const
{
    switch (row)
    {
    case TRACKERLESS_ROW:
        return m_trackers.value(NULL_HOST).torrents;
    case ERROR_ROW:
        return {m_errors.keyBegin(), m_errors.keyEnd()};
    case WARNING_ROW:
        return {m_warnings.keyBegin(), m_warnings.keyEnd()};
    default:
        return m_trackers.value(trackerFromRow(row)).torrents;
    }
}

TransferListFiltersWidget::TransferListFiltersWidget(QWidget *parent, TransferListWidget *transferList, const bool downloadFavicon)
    : QFrame(parent)
    , m_transferList(transferList)
{
    Preferences *const pref = Preferences::instance();

    // Construct lists
    auto *vLayout = new QVBoxLayout(this);
    auto *scroll = new QScrollArea(this);
    QFrame *frame = new QFrame(scroll);
    auto *frameLayout = new QVBoxLayout(frame);
    QFont font;
    font.setBold(true);
    font.setCapitalization(QFont::AllUppercase);

    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    setStyleSheet(u"QFrame {background: transparent;}"_qs);
    scroll->setStyleSheet(u"QFrame {border: none;}"_qs);
    vLayout->setContentsMargins(0, 0, 0, 0);
    frameLayout->setContentsMargins(0, 2, 0, 0);
    frameLayout->setSpacing(2);
    frameLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    frame->setLayout(frameLayout);
    scroll->setWidget(frame);
    vLayout->addWidget(scroll);
    setLayout(vLayout);

    QCheckBox *statusLabel = new ArrowCheckBox(tr("Status"), this);
    statusLabel->setChecked(pref->getStatusFilterState());
    statusLabel->setFont(font);
    frameLayout->addWidget(statusLabel);

    auto *statusFilters = new StatusFilterWidget(this, transferList);
    frameLayout->addWidget(statusFilters);

    QCheckBox *categoryLabel = new ArrowCheckBox(tr("Categories"), this);
    categoryLabel->setChecked(pref->getCategoryFilterState());
    categoryLabel->setFont(font);
    connect(categoryLabel, &QCheckBox::toggled, this
            , &TransferListFiltersWidget::onCategoryFilterStateChanged);
    frameLayout->addWidget(categoryLabel);

    m_categoryFilterWidget = new CategoryFilterWidget(this);
    connect(m_categoryFilterWidget, &CategoryFilterWidget::actionDeleteTorrentsTriggered
            , transferList, &TransferListWidget::deleteVisibleTorrents);
    connect(m_categoryFilterWidget, &CategoryFilterWidget::actionPauseTorrentsTriggered
            , transferList, &TransferListWidget::pauseVisibleTorrents);
    connect(m_categoryFilterWidget, &CategoryFilterWidget::actionResumeTorrentsTriggered
            , transferList, &TransferListWidget::startVisibleTorrents);
    connect(m_categoryFilterWidget, &CategoryFilterWidget::categoryChanged
            , transferList, &TransferListWidget::applyCategoryFilter);
    toggleCategoryFilter(pref->getCategoryFilterState());
    frameLayout->addWidget(m_categoryFilterWidget);

    QCheckBox *tagsLabel = new ArrowCheckBox(tr("Tags"), this);
    tagsLabel->setChecked(pref->getTagFilterState());
    tagsLabel->setFont(font);
    connect(tagsLabel, &QCheckBox::toggled, this, &TransferListFiltersWidget::onTagFilterStateChanged);
    frameLayout->addWidget(tagsLabel);

    m_tagFilterWidget = new TagFilterWidget(this);
    connect(m_tagFilterWidget, &TagFilterWidget::actionDeleteTorrentsTriggered
            , transferList, &TransferListWidget::deleteVisibleTorrents);
    connect(m_tagFilterWidget, &TagFilterWidget::actionPauseTorrentsTriggered
            , transferList, &TransferListWidget::pauseVisibleTorrents);
    connect(m_tagFilterWidget, &TagFilterWidget::actionResumeTorrentsTriggered
            , transferList, &TransferListWidget::startVisibleTorrents);
    connect(m_tagFilterWidget, &TagFilterWidget::tagChanged
            , transferList, &TransferListWidget::applyTagFilter);
    toggleTagFilter(pref->getTagFilterState());
    frameLayout->addWidget(m_tagFilterWidget);

    QCheckBox *trackerLabel = new ArrowCheckBox(tr("Trackers"), this);
    trackerLabel->setChecked(pref->getTrackerFilterState());
    trackerLabel->setFont(font);
    frameLayout->addWidget(trackerLabel);

    m_trackerFilters = new TrackerFiltersList(this, transferList, downloadFavicon);
    frameLayout->addWidget(m_trackerFilters);

    connect(statusLabel, &QCheckBox::toggled, statusFilters, &StatusFilterWidget::toggleFilter);
    connect(statusLabel, &QCheckBox::toggled, pref, &Preferences::setStatusFilterState);
    connect(trackerLabel, &QCheckBox::toggled, m_trackerFilters, &TrackerFiltersList::toggleFilter);
    connect(trackerLabel, &QCheckBox::toggled, pref, &Preferences::setTrackerFilterState);
}

void TransferListFiltersWidget::setDownloadTrackerFavicon(bool value)
{
    m_trackerFilters->setDownloadTrackerFavicon(value);
}

void TransferListFiltersWidget::addTrackers(const BitTorrent::Torrent *torrent, const QVector<BitTorrent::TrackerEntry> &trackers)
{
    m_trackerFilters->addTrackers(torrent, trackers);
}

void TransferListFiltersWidget::removeTrackers(const BitTorrent::Torrent *torrent, const QStringList &trackers)
{
    m_trackerFilters->removeTrackers(torrent, trackers);
}

void TransferListFiltersWidget::refreshTrackers(const BitTorrent::Torrent *torrent)
{
    m_trackerFilters->refreshTrackers(torrent);
}

void TransferListFiltersWidget::changeTrackerless(const BitTorrent::Torrent *torrent, const bool trackerless)
{
    m_trackerFilters->changeTrackerless(torrent, trackerless);
}

void TransferListFiltersWidget::trackerEntriesUpdated(const QHash<BitTorrent::Torrent *, QSet<QString>> &updateInfos)
{
    m_trackerFilters->handleTrackerEntriesUpdated(updateInfos);
}

void TransferListFiltersWidget::onCategoryFilterStateChanged(bool enabled)
{
    toggleCategoryFilter(enabled);
    Preferences::instance()->setCategoryFilterState(enabled);
}

void TransferListFiltersWidget::toggleCategoryFilter(bool enabled)
{
    m_categoryFilterWidget->setVisible(enabled);
    m_transferList->applyCategoryFilter(enabled ? m_categoryFilterWidget->currentCategory() : QString());
}

void TransferListFiltersWidget::onTagFilterStateChanged(bool enabled)
{
    toggleTagFilter(enabled);
    Preferences::instance()->setTagFilterState(enabled);
}

void TransferListFiltersWidget::toggleTagFilter(bool enabled)
{
    m_tagFilterWidget->setVisible(enabled);
    m_transferList->applyTagFilter(enabled ? m_tagFilterWidget->currentTag() : QString());
}
