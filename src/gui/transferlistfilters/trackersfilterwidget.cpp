/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023-2025  Vladimir Golovnev <glassez@yandex.ru>
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

#include "trackersfilterwidget.h"

#include <QCheckBox>
#include <QIcon>
#include <QListWidgetItem>
#include <QMenu>
#include <QMessageBox>
#include <QUrl>

#include "base/bittorrent/session.h"
#include "base/bittorrent/trackerentry.h"
#include "base/bittorrent/trackerentrystatus.h"
#include "base/global.h"
#include "base/net/downloadmanager.h"
#include "base/preferences.h"
#include "base/torrentfilter.h"
#include "base/utils/compare.h"
#include "base/utils/fs.h"
#include "gui/transferlistwidget.h"
#include "gui/uithememanager.h"

namespace
{
    enum TRACKER_FILTER_ROW
    {
        ALL_ROW,
        TRACKERLESS_ROW,
        TRACKERERROR_ROW,
        OTHERERROR_ROW,
        WARNING_ROW,

        NUM_SPECIAL_ROWS
    };

    const QString NULL_HOST = u""_s;

    QString getScheme(const QString &tracker)
    {
        const QString scheme = QUrl(tracker).scheme();
        return !scheme.isEmpty() ? scheme : u"http"_s;
    }

    template <typename T>
    concept HasUrlMember = requires (T t) { { t.url } -> std::convertible_to<QString>; };

    template <HasUrlMember T>
    QString getTrackerHost(const T &t)
    {
        return getTrackerHost(t.url);
    }

    template <typename T>
    QSet<QString> extractTrackerHosts(const T &trackerEntries)
    {
        QSet<QString> trackerHosts;
        trackerHosts.reserve(trackerEntries.size());
        for (const auto &trackerEntry : trackerEntries)
            trackerHosts.insert(getTrackerHost(trackerEntry));

        return trackerHosts;
    }

    template <typename T>
    QSet<QString> extractTrackerURLs(const T &trackerEntries)
    {
        QSet<QString> trackerURLs;
        trackerURLs.reserve(trackerEntries.size());
        for (const auto &trackerEntry : trackerEntries)
            trackerURLs.insert(trackerEntry.url);

        return trackerURLs;
    }

    QString getFaviconHost(const QString &trackerHost)
    {
        if (!QHostAddress(trackerHost).isNull())
            return trackerHost;

        return trackerHost.section(u'.', -2, -1);
    }

    QString getFormatStringForRow(const int row)
    {
        switch (row)
        {
        case ALL_ROW:
            return TrackersFilterWidget::tr("All (%1)", "this is for the tracker filter");
        case TRACKERLESS_ROW:
            return TrackersFilterWidget::tr("Trackerless (%1)");
        case TRACKERERROR_ROW:
            return TrackersFilterWidget::tr("Tracker error (%1)");
        case OTHERERROR_ROW:
            return TrackersFilterWidget::tr("Other error (%1)");
        case WARNING_ROW:
            return TrackersFilterWidget::tr("Warning (%1)");
        default:
            return {};
        }
    }

    QString formatItemText(const int row, const int torrentsCount)
    {
        return getFormatStringForRow(row).arg(torrentsCount);
    }

    QString formatItemText(const QString &host, const int torrentsCount)
    {
        return (host == NULL_HOST)
            ? formatItemText(TRACKERLESS_ROW, torrentsCount)
            : u"%1 (%2)"_s.arg(host, QString::number(torrentsCount));
    }
}

TrackersFilterWidget::TrackersFilterWidget(QWidget *parent, TransferListWidget *transferList, const bool downloadFavicon)
    : BaseFilterWidget(parent, transferList)
    , m_downloadTrackerFavicon {downloadFavicon}
{
    auto *allTrackersItem = new QListWidgetItem(this);
    allTrackersItem->setData(Qt::DisplayRole, formatItemText(ALL_ROW, 0));
    allTrackersItem->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"trackers"_s, u"network-server"_s));
    auto *trackerlessItem = new QListWidgetItem(this);
    trackerlessItem->setData(Qt::DisplayRole, formatItemText(TRACKERLESS_ROW, 0));
    trackerlessItem->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"trackerless"_s, u"network-server"_s));

    m_trackers[NULL_HOST] = {0, trackerlessItem};

    const auto *pref = Preferences::instance();
    const bool useSeparateTrackerStatusFilter = pref->useSeparateTrackerStatusFilter();
    if (useSeparateTrackerStatusFilter == m_handleTrackerStatuses)
        enableTrackerStatusItems(!useSeparateTrackerStatusFilter);
    connect(pref, &Preferences::changed, this, [this, pref]
    {
        const bool useSeparateTrackerStatusFilter = pref->useSeparateTrackerStatusFilter();
        if (useSeparateTrackerStatusFilter == m_handleTrackerStatuses)
        {
            enableTrackerStatusItems(!useSeparateTrackerStatusFilter);
            updateGeometry();
            if (m_handleTrackerStatuses)
                applyFilter(currentRow());
        }
    });

    const auto *btSession = BitTorrent::Session::instance();
    handleTorrentsLoaded(btSession->torrents());

    connect(btSession, &BitTorrent::Session::trackersAdded, this, &TrackersFilterWidget::handleTorrentTrackersAdded);
    connect(btSession, &BitTorrent::Session::trackersRemoved, this, &TrackersFilterWidget::handleTorrentTrackersRemoved);
    connect(btSession, &BitTorrent::Session::trackersReset, this, &TrackersFilterWidget::handleTorrentTrackersReset);
    connect(btSession, &BitTorrent::Session::trackerEntryStatusesUpdated, this, &TrackersFilterWidget::handleTorrentTrackerStatusesUpdated);

    setCurrentRow(0, QItemSelectionModel::SelectCurrent);
}

TrackersFilterWidget::~TrackersFilterWidget()
{
    for (const Path &iconPath : asConst(m_iconPaths))
        Utils::Fs::removeFile(iconPath);
}

void TrackersFilterWidget::handleTorrentTrackersAdded(const BitTorrent::Torrent *torrent, const QList<BitTorrent::TrackerEntry> &trackers)
{
    const QSet<QString> prevTrackerURLs = extractTrackerURLs(torrent->trackers()).subtract(extractTrackerURLs(trackers));
    const QSet<QString> addedTrackerHosts = extractTrackerHosts(trackers).subtract(extractTrackerHosts(prevTrackerURLs));

    for (const QString &trackerHost : addedTrackerHosts)
        increaseTorrentsCount(trackerHost, 1);

    if (prevTrackerURLs.isEmpty())
        decreaseTorrentsCount(NULL_HOST); // torrent was trackerless previously
}

void TrackersFilterWidget::handleTorrentTrackersRemoved(const BitTorrent::Torrent *torrent, const QStringList &trackers)
{
    const QList<BitTorrent::TrackerEntryStatus> currentTrackerEntries = torrent->trackers();
    const QSet<QString> removedTrackerHosts = extractTrackerHosts(trackers).subtract(extractTrackerHosts(currentTrackerEntries));
    for (const QString &trackerHost : removedTrackerHosts)
        decreaseTorrentsCount(trackerHost);

    if (currentTrackerEntries.isEmpty())
        increaseTorrentsCount(NULL_HOST, 1);

    if (m_handleTrackerStatuses)
        refreshStatusItems(torrent);
}

void TrackersFilterWidget::handleTorrentTrackersReset(const BitTorrent::Torrent *torrent
        , const QList<BitTorrent::TrackerEntryStatus> &oldEntries, const QList<BitTorrent::TrackerEntry> &newEntries)
{
    if (oldEntries.isEmpty())
    {
        decreaseTorrentsCount(NULL_HOST);
    }
    else
    {
        for (const QString &trackerHost : asConst(extractTrackerHosts(oldEntries)))
            decreaseTorrentsCount(trackerHost);
    }

    if (newEntries.isEmpty())
    {
        increaseTorrentsCount(NULL_HOST, 1);
    }
    else
    {
        for (const QString &trackerHost : asConst(extractTrackerHosts(newEntries)))
            increaseTorrentsCount(trackerHost, 1);
    }

    if (m_handleTrackerStatuses)
        refreshStatusItems(torrent);

    updateGeometry();
}

void TrackersFilterWidget::increaseTorrentsCount(const QString &trackerHost, const qsizetype torrentsCount)
{
    auto trackersIt = m_trackers.find(trackerHost);
    const bool exists = (trackersIt != m_trackers.end());
    QListWidgetItem *trackerItem = nullptr;

    if (exists)
    {
        trackerItem = trackersIt->item;
    }
    else
    {
        trackerItem = new QListWidgetItem();
        trackerItem->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"trackers"_s, u"network-server"_s));

        const TrackerData trackerData {0, trackerItem};
        trackersIt = m_trackers.insert(trackerHost, trackerData);

        const QString scheme = getScheme(trackerHost);
        downloadFavicon(trackerHost, u"%1://%2/favicon.ico"_s.arg((scheme.startsWith(u"http") ? scheme : u"http"_s), getFaviconHost(trackerHost)));
    }

    Q_ASSERT(trackerItem);

    trackersIt->torrentsCount += torrentsCount;

    trackerItem->setText(formatItemText(trackerHost, trackersIt->torrentsCount));
    if (exists)
        return;

    Q_ASSERT(count() >= numSpecialRows());
    const Utils::Compare::NaturalLessThan<Qt::CaseSensitive> naturalLessThan {};
    int insPos = count();
    for (int i = numSpecialRows(); i < count(); ++i)
    {
        if (naturalLessThan(trackerHost, item(i)->text()))
        {
            insPos = i;
            break;
        }
    }
    QListWidget::insertItem(insPos, trackerItem);
    updateGeometry();
}

void TrackersFilterWidget::decreaseTorrentsCount(const QString &trackerHost)
{
    const auto iter = m_trackers.find(trackerHost);
    Q_ASSERT(iter != m_trackers.end());
    if (iter == m_trackers.end()) [[unlikely]]
        return;

    TrackerData &trackerData = iter.value();
    Q_ASSERT(trackerData.torrentsCount > 0);
    if (trackerData.torrentsCount <= 0) [[unlikely]]
        return;

    --trackerData.torrentsCount;

    if ((trackerData.torrentsCount == 0) && (trackerHost != NULL_HOST))
    {
        if (currentItem() == trackerData.item)
            setCurrentRow(0, QItemSelectionModel::SelectCurrent);
        delete trackerData.item;
        m_trackers.erase(iter);
        updateGeometry();
    }
    else
    {
        trackerData.item->setText(formatItemText(trackerHost, trackerData.torrentsCount));
    }
}

void TrackersFilterWidget::refreshStatusItems(const BitTorrent::Torrent *torrent)
{
    const BitTorrent::TorrentAnnounceStatus announceStatus = torrent->announceStatus();

    if (announceStatus.testFlag(BitTorrent::TorrentAnnounceStatusFlag::HasWarning))
        m_warnings.insert(torrent);
    else
        m_warnings.remove(torrent);

    if (announceStatus.testFlag(BitTorrent::TorrentAnnounceStatusFlag::HasTrackerError))
        m_trackerErrors.insert(torrent);
    else
        m_trackerErrors.remove(torrent);

    if (announceStatus.testFlag(BitTorrent::TorrentAnnounceStatusFlag::HasOtherError))
        m_errors.insert(torrent);
    else
        m_errors.remove(torrent);

    item(OTHERERROR_ROW)->setText(formatItemText(OTHERERROR_ROW, m_errors.size()));
    item(TRACKERERROR_ROW)->setText(formatItemText(TRACKERERROR_ROW, m_trackerErrors.size()));
    item(WARNING_ROW)->setText(formatItemText(WARNING_ROW, m_warnings.size()));
}

void TrackersFilterWidget::setDownloadTrackerFavicon(const bool value)
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
                downloadFavicon(tracker, u"%1://%2/favicon.ico"_s
                        .arg((scheme.startsWith(u"http") ? scheme : u"http"_s), getFaviconHost(tracker)));
             }
        }
    }
}

void TrackersFilterWidget::handleTorrentTrackerStatusesUpdated(const BitTorrent::Torrent *torrent
        , [[maybe_unused]] const QHash<QString, BitTorrent::TrackerEntryStatus> &updatedTrackers)
{
    if (m_handleTrackerStatuses)
        refreshStatusItems(torrent);
}

void TrackersFilterWidget::downloadFavicon(const QString &trackerHost, const QString &faviconURL)
{
    if (!m_downloadTrackerFavicon)
        return;

    QSet<QString> &downloadingFaviconNode = m_downloadingFavicons[faviconURL];
    if (downloadingFaviconNode.isEmpty())
    {
        Net::DownloadManager::instance()->download(
                Net::DownloadRequest(faviconURL).saveToFile(true), Preferences::instance()->useProxyForGeneralPurposes()
                , this, &TrackersFilterWidget::handleFavicoDownloadFinished);
    }

    downloadingFaviconNode.insert(trackerHost);
}

void TrackersFilterWidget::removeTracker(const QString &trackerHost)
{
    for (BitTorrent::Torrent *torrent : asConst(BitTorrent::Session::instance()->torrents()))
    {
        QStringList trackersToRemove;
        for (const BitTorrent::TrackerEntryStatus &trackerEntryStatus : asConst(torrent->trackers()))
        {
            if (getTrackerHost(trackerEntryStatus) == trackerHost)
                trackersToRemove.append(trackerEntryStatus.url);
        }

        torrent->removeTrackers({trackersToRemove});
    }

    updateGeometry();
}

void TrackersFilterWidget::enableTrackerStatusItems(const bool value)
{
    m_handleTrackerStatuses = value;
    if (m_handleTrackerStatuses)
    {
        auto *trackerErrorItem = new QListWidgetItem;
        trackerErrorItem->setData(Qt::DisplayRole, formatItemText(TRACKERERROR_ROW, 0));
        trackerErrorItem->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"tracker-error"_s, u"dialog-error"_s));
        insertItem(TRACKERERROR_ROW, trackerErrorItem);

        auto *otherErrorItem = new QListWidgetItem;
        otherErrorItem->setData(Qt::DisplayRole, formatItemText(OTHERERROR_ROW, 0));
        otherErrorItem->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"tracker-error"_s, u"dialog-error"_s));
        insertItem(OTHERERROR_ROW, otherErrorItem);

        auto *warningItem = new QListWidgetItem;
        warningItem->setData(Qt::DisplayRole, formatItemText(WARNING_ROW, 0));
        warningItem->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"tracker-warning"_s, u"dialog-warning"_s));
        insertItem(WARNING_ROW, warningItem);

        const QList<BitTorrent::Torrent *> torrents = BitTorrent::Session::instance()->torrents();
        for (const BitTorrent::Torrent *torrent : torrents)
        {
            const BitTorrent::TorrentAnnounceStatus announceStatus = torrent->announceStatus();

            if (announceStatus.testFlag(BitTorrent::TorrentAnnounceStatusFlag::HasWarning))
                m_warnings.insert(torrent);

            if (announceStatus.testFlag(BitTorrent::TorrentAnnounceStatusFlag::HasTrackerError))
                m_trackerErrors.insert(torrent);

            if (announceStatus.testFlag(BitTorrent::TorrentAnnounceStatusFlag::HasOtherError))
                m_errors.insert(torrent);
        }

        warningItem->setText(formatItemText(WARNING_ROW, m_warnings.size()));
        trackerErrorItem->setText(formatItemText(TRACKERERROR_ROW, m_trackerErrors.size()));
        otherErrorItem->setText(formatItemText(OTHERERROR_ROW, m_errors.size()));
    }
    else
    {
        if (const int row = currentRow();
                (row == WARNING_ROW) || (row == TRACKERERROR_ROW) || (row == OTHERERROR_ROW))
        {
            setCurrentRow(0, QItemSelectionModel::ClearAndSelect);
        }

        // Need to be removed in reversed order
        takeItem(WARNING_ROW);
        takeItem(OTHERERROR_ROW);
        takeItem(TRACKERERROR_ROW);

        m_warnings.clear();
        m_trackerErrors.clear();
        m_errors.clear();
    }
}

qsizetype TrackersFilterWidget::numSpecialRows() const
{
    if (m_handleTrackerStatuses)
        return NUM_SPECIAL_ROWS;

    return NUM_SPECIAL_ROWS - 3;
}

void TrackersFilterWidget::handleFavicoDownloadFinished(const Net::DownloadResult &result)
{
    const QSet<QString> trackerHosts = m_downloadingFavicons.take(result.url);
    Q_ASSERT(!trackerHosts.isEmpty());
    if (trackerHosts.isEmpty()) [[unlikely]]
        return;

    QIcon icon;
    bool failed = false;
    if (result.status != Net::DownloadStatus::Success)
    {
        failed = true;
    }
    else
    {
        icon = QIcon(result.filePath.data());
        //Detect a non-decodable icon
        QList<QSize> sizes = icon.availableSizes();
        const bool invalid = (sizes.isEmpty() || icon.pixmap(sizes.first()).isNull());
        if (invalid)
        {
            Utils::Fs::removeFile(result.filePath);
            failed = true;
        }
    }

    if (failed)
    {
        if (result.url.endsWith(u".ico", Qt::CaseInsensitive))
        {
            const QString faviconURL = QStringView(result.url).chopped(4) + u".png";
            for (const auto &trackerHost : trackerHosts)
            {
                if (m_trackers.contains(trackerHost))
                    downloadFavicon(trackerHost, faviconURL);
            }
        }

        return;
    }

    bool matchedTrackerFound = false;
    for (const auto &trackerHost : trackerHosts)
    {
        if (!m_trackers.contains(trackerHost))
            continue;

        matchedTrackerFound = true;

        QListWidgetItem *trackerItem = item(rowFromTracker(trackerHost));
        Q_ASSERT(trackerItem);
        if (!trackerItem) [[unlikely]]
            continue;

        trackerItem->setData(Qt::DecorationRole, icon);
    }

    if (matchedTrackerFound)
        m_iconPaths.append(result.filePath);
    else
        Utils::Fs::removeFile(result.filePath);
}

void TrackersFilterWidget::showMenu()
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    if (currentRow() >= numSpecialRows())
    {
        menu->addAction(UIThemeManager::instance()->getIcon(u"edit-clear"_s, u"list-remove"_s), tr("Remove tracker")
            , this, &TrackersFilterWidget::onRemoveTrackerTriggered);
        menu->addSeparator();
    }

    menu->addAction(UIThemeManager::instance()->getIcon(u"torrent-start"_s, u"media-playback-start"_s), tr("Start torrents")
        , transferList(), &TransferListWidget::startVisibleTorrents);
    menu->addAction(UIThemeManager::instance()->getIcon(u"torrent-stop"_s, u"media-playback-pause"_s), tr("Stop torrents")
        , transferList(), &TransferListWidget::stopVisibleTorrents);
    menu->addAction(UIThemeManager::instance()->getIcon(u"list-remove"_s), tr("Remove torrents")
        , transferList(), &TransferListWidget::deleteVisibleTorrents);

    menu->popup(QCursor::pos());
}

void TrackersFilterWidget::applyFilter(const int row)
{
    if (m_handleTrackerStatuses)
    {
        switch (row)
        {
        case ALL_ROW:
            transferList()->applyTrackerFilter(std::nullopt);
            transferList()->applyAnnounceStatusFilter(std::nullopt);
            break;

        case TRACKERLESS_ROW:
            transferList()->applyTrackerFilter(NULL_HOST);
            transferList()->applyAnnounceStatusFilter(std::nullopt);
            break;

        case OTHERERROR_ROW:
            transferList()->applyAnnounceStatusFilter(BitTorrent::TorrentAnnounceStatusFlag::HasOtherError);
            transferList()->applyTrackerFilter(std::nullopt);
            break;

        case TRACKERERROR_ROW:
            transferList()->applyAnnounceStatusFilter(BitTorrent::TorrentAnnounceStatusFlag::HasTrackerError);
            transferList()->applyTrackerFilter(std::nullopt);
            break;

        case WARNING_ROW:
            transferList()->applyAnnounceStatusFilter(BitTorrent::TorrentAnnounceStatusFlag::HasWarning);
            transferList()->applyTrackerFilter(std::nullopt);
            break;

        default:
            transferList()->applyTrackerFilter(trackerFromRow(row));
            transferList()->applyAnnounceStatusFilter(std::nullopt);
            break;
        }
    }
    else
    {
        switch (row)
        {
        case ALL_ROW:
            transferList()->applyTrackerFilter(std::nullopt);
            break;

        case TRACKERLESS_ROW:
            transferList()->applyTrackerFilter(NULL_HOST);
            break;

        default:
            transferList()->applyTrackerFilter(trackerFromRow(row));
            break;
        }
    }
}

void TrackersFilterWidget::handleTorrentsLoaded(const QList<BitTorrent::Torrent *> &torrents)
{
    QHash<QString, qsizetype> torrentsPerTrackerHost;
    for (const BitTorrent::Torrent *torrent : torrents)
    {
        // Check for trackerless torrent
        if (const QList<BitTorrent::TrackerEntryStatus> trackers = torrent->trackers(); trackers.isEmpty())
        {
            ++torrentsPerTrackerHost[NULL_HOST];
        }
        else
        {
            for (const QString &trackerHost : asConst(extractTrackerHosts(trackers)))
                ++torrentsPerTrackerHost[trackerHost];
        }
    }

    for (const auto &[trackerHost, torrentsCount] : asConst(torrentsPerTrackerHost).asKeyValueRange())
    {
        increaseTorrentsCount(trackerHost, torrentsCount);
    }

    m_totalTorrents += torrents.count();
    item(ALL_ROW)->setText(formatItemText(ALL_ROW, m_totalTorrents));
}

void TrackersFilterWidget::torrentAboutToBeDeleted(BitTorrent::Torrent *const torrent)
{
    // Check for trackerless torrent
    if (const QList<BitTorrent::TrackerEntryStatus> trackers = torrent->trackers(); trackers.isEmpty())
    {
        decreaseTorrentsCount(NULL_HOST);
    }
    else
    {
        for (const QString &trackerHost : asConst(extractTrackerHosts(trackers)))
            decreaseTorrentsCount(trackerHost);
    }

    item(ALL_ROW)->setText(formatItemText(ALL_ROW, --m_totalTorrents));

    if (m_handleTrackerStatuses)
    {
        m_warnings.remove(torrent);
        m_trackerErrors.remove(torrent);
        m_errors.remove(torrent);

        item(WARNING_ROW)->setText(formatItemText(WARNING_ROW, m_warnings.size()));
        item(TRACKERERROR_ROW)->setText(formatItemText(TRACKERERROR_ROW, m_trackerErrors.size()));
        item(OTHERERROR_ROW)->setText(formatItemText(OTHERERROR_ROW, m_errors.size()));
    }
}

void TrackersFilterWidget::onRemoveTrackerTriggered()
{
    const int row = currentRow();
    if (row < numSpecialRows())
        return;

    const QString &tracker = trackerFromRow(row);
    if (!Preferences::instance()->confirmRemoveTrackerFromAllTorrents())
    {
        removeTracker(tracker);
        return;
    }

    auto *confirmBox = new QMessageBox(QMessageBox::Question, tr("Removal confirmation")
        , tr("Are you sure you want to remove tracker \"%1\" from all torrents?").arg(tracker)
        , (QMessageBox::Yes | QMessageBox::No), this);
    confirmBox->setCheckBox(new QCheckBox(tr("Don't ask me again.")));
    confirmBox->setAttribute(Qt::WA_DeleteOnClose);
    connect(confirmBox, &QDialog::accepted, this, [this, confirmBox, tracker]
    {
        removeTracker(tracker);

        if (confirmBox->checkBox()->isChecked())
            Preferences::instance()->setConfirmRemoveTrackerFromAllTorrents(false);
    });
    confirmBox->open();
}

QString TrackersFilterWidget::trackerFromRow(int row) const
{
    Q_ASSERT(row > 1);
    const QString tracker = item(row)->text();
    QStringList parts = tracker.split(u' ');
    Q_ASSERT(parts.size() >= 2);
    parts.removeLast(); // Remove trailing number
    return parts.join(u' ');
}

int TrackersFilterWidget::rowFromTracker(const QString &tracker) const
{
    Q_ASSERT(!tracker.isEmpty());
    for (int i = numSpecialRows(); i < count(); ++i)
    {
        if (tracker == trackerFromRow(i))
            return i;
    }
    return -1;
}
