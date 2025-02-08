/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023  Vladimir Golovnev <glassez@yandex.ru>
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

#include "base/algorithm.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/trackerentry.h"
#include "base/bittorrent/trackerentrystatus.h"
#include "base/global.h"
#include "base/net/downloadmanager.h"
#include "base/preferences.h"
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

    QString getHost(const QString &url)
    {
        // We want the hostname.
        // If failed to parse the domain, original input should be returned

        const QString host = QUrl(url).host();
        if (host.isEmpty())
            return url;

        return host;
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
    , m_downloadTrackerFavicon(downloadFavicon)
{
    auto *allTrackersItem = new QListWidgetItem(this);
    allTrackersItem->setData(Qt::DisplayRole, formatItemText(ALL_ROW, 0));
    allTrackersItem->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"trackers"_s, u"network-server"_s));
    auto *trackerlessItem = new QListWidgetItem(this);
    trackerlessItem->setData(Qt::DisplayRole, formatItemText(TRACKERLESS_ROW, 0));
    trackerlessItem->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"trackerless"_s, u"network-server"_s));
    auto *trackerErrorItem = new QListWidgetItem(this);
    trackerErrorItem->setData(Qt::DisplayRole, formatItemText(TRACKERERROR_ROW, 0));
    trackerErrorItem->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"tracker-error"_s, u"dialog-error"_s));
    auto *otherErrorItem = new QListWidgetItem(this);
    otherErrorItem->setData(Qt::DisplayRole, formatItemText(OTHERERROR_ROW, 0));
    otherErrorItem->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"tracker-error"_s, u"dialog-error"_s));
    auto *warningItem = new QListWidgetItem(this);
    warningItem->setData(Qt::DisplayRole, formatItemText(WARNING_ROW, 0));
    warningItem->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"tracker-warning"_s, u"dialog-warning"_s));

    m_trackers[NULL_HOST] = {{}, trackerlessItem};

    handleTorrentsLoaded(BitTorrent::Session::instance()->torrents());

    setCurrentRow(0, QItemSelectionModel::SelectCurrent);
    toggleFilter(Preferences::instance()->getTrackerFilterState());
}

TrackersFilterWidget::~TrackersFilterWidget()
{
    for (const Path &iconPath : asConst(m_iconPaths))
        Utils::Fs::removeFile(iconPath);
}

void TrackersFilterWidget::addTrackers(const BitTorrent::Torrent *torrent, const QList<BitTorrent::TrackerEntry> &trackers)
{
    const BitTorrent::TorrentID torrentID = torrent->id();

    for (const BitTorrent::TrackerEntry &tracker : trackers)
        addItems(tracker.url, {torrentID});

    removeItem(NULL_HOST, torrentID);
}

void TrackersFilterWidget::removeTrackers(const BitTorrent::Torrent *torrent, const QStringList &trackers)
{
    const BitTorrent::TorrentID torrentID = torrent->id();

    for (const QString &tracker : trackers)
        removeItem(tracker, torrentID);

    if (torrent->trackers().isEmpty())
        addItems(NULL_HOST, {torrentID});
}

void TrackersFilterWidget::refreshTrackers(const BitTorrent::Torrent *torrent)
{
    const BitTorrent::TorrentID torrentID = torrent->id();

    m_errors.remove(torrentID);
    m_trackerErrors.remove(torrentID);
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

        trackerItem->setText(formatItemText(host, torrentIDs.size()));
        return false;
    });

    const QList<BitTorrent::TrackerEntryStatus> trackers = torrent->trackers();
    if (trackers.isEmpty())
    {
        addItems(NULL_HOST, {torrentID});
    }
    else
    {
        for (const BitTorrent::TrackerEntryStatus &status : trackers)
            addItems(status.url, {torrentID});
    }

    item(OTHERERROR_ROW)->setText(formatItemText(OTHERERROR_ROW, m_errors.size()));
    item(TRACKERERROR_ROW)->setText(formatItemText(TRACKERERROR_ROW, m_trackerErrors.size()));
    item(WARNING_ROW)->setText(formatItemText(WARNING_ROW, m_warnings.size()));

    if (const int row = currentRow(); (row == OTHERERROR_ROW)
        || (row == TRACKERERROR_ROW) || (row == WARNING_ROW))
    {
        applyFilter(row);
    }

    updateGeometry();
}

void TrackersFilterWidget::addItems(const QString &trackerURL, const QList<BitTorrent::TorrentID> &torrents)
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
        trackerItem->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"trackers"_s, u"network-server"_s));

        const TrackerData trackerData {{}, trackerItem};
        trackersIt = m_trackers.insert(host, trackerData);

        const QString scheme = getScheme(trackerURL);
        downloadFavicon(host, u"%1://%2/favicon.ico"_s.arg((scheme.startsWith(u"http") ? scheme : u"http"_s), getFaviconHost(host)));
    }

    Q_ASSERT(trackerItem);

    QSet<BitTorrent::TorrentID> &torrentIDs = trackersIt->torrents;
    for (const BitTorrent::TorrentID &torrentID : torrents)
        torrentIDs.insert(torrentID);

    trackerItem->setText(formatItemText(host, torrentIDs.size()));
    if (exists)
    {
        if (item(currentRow()) == trackerItem)
            applyFilter(currentRow());
        return;
    }

    Q_ASSERT(count() >= NUM_SPECIAL_ROWS);
    const Utils::Compare::NaturalLessThan<Qt::CaseSensitive> naturalLessThan {};
    int insPos = count();
    for (int i = NUM_SPECIAL_ROWS; i < count(); ++i)
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

void TrackersFilterWidget::removeItem(const QString &trackerURL, const BitTorrent::TorrentID &id)
{
    const QString host = getHost(trackerURL);

    QSet<BitTorrent::TorrentID> torrentIDs = m_trackers.value(host).torrents;
    torrentIDs.remove(id);

    QListWidgetItem *trackerItem = nullptr;

    if (!host.isEmpty())
    {
        // Remove from 'Error', 'Tracker error' and 'Warning' view
        if (const auto errorHashesIt = m_errors.find(id)
                ; errorHashesIt != m_errors.end())
        {
            QSet<QString> &errored = *errorHashesIt;
            errored.remove(trackerURL);
            if (errored.isEmpty())
            {
                m_errors.erase(errorHashesIt);
                item(OTHERERROR_ROW)->setText(formatItemText(OTHERERROR_ROW, m_errors.size()));
                if (currentRow() == OTHERERROR_ROW)
                    applyFilter(OTHERERROR_ROW);
            }
        }

        if (const auto trackerErrorHashesIt = m_trackerErrors.find(id)
                ; trackerErrorHashesIt != m_trackerErrors.end())
        {
            QSet<QString> &errored = *trackerErrorHashesIt;
            errored.remove(trackerURL);
            if (errored.isEmpty())
            {
                m_trackerErrors.erase(trackerErrorHashesIt);
                item(TRACKERERROR_ROW)->setText(formatItemText(TRACKERERROR_ROW, m_trackerErrors.size()));
                if (currentRow() == TRACKERERROR_ROW)
                    applyFilter(TRACKERERROR_ROW);
            }
        }

        if (const auto warningHashesIt = m_warnings.find(id)
                ; warningHashesIt != m_warnings.end())
        {
            QSet<QString> &warned = *warningHashesIt;
            warned.remove(trackerURL);
            if (warned.isEmpty())
            {
                m_warnings.erase(warningHashesIt);
                item(WARNING_ROW)->setText(formatItemText(WARNING_ROW, m_warnings.size()));
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
            trackerItem->setText(u"%1 (%2)"_s.arg(host, QString::number(torrentIDs.size())));
    }
    else
    {
        trackerItem = item(TRACKERLESS_ROW);
        trackerItem->setText(formatItemText(TRACKERLESS_ROW, torrentIDs.size()));
    }

    m_trackers.insert(host, {torrentIDs, trackerItem});

    if (currentItem() == trackerItem)
        applyFilter(currentRow());
}

void TrackersFilterWidget::setDownloadTrackerFavicon(bool value)
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

void TrackersFilterWidget::handleTrackerStatusesUpdated(const BitTorrent::Torrent *torrent
        , const QHash<QString, BitTorrent::TrackerEntryStatus> &updatedTrackers)
{
    const BitTorrent::TorrentID id = torrent->id();

    auto errorHashesIt = m_errors.find(id);
    auto trackerErrorHashesIt = m_trackerErrors.find(id);
    auto warningHashesIt = m_warnings.find(id);

    for (const BitTorrent::TrackerEntryStatus &trackerEntryStatus : updatedTrackers)
    {
        switch (trackerEntryStatus.state)
        {
        case BitTorrent::TrackerEndpointState::Working:
            {
                // remove tracker from "error" and "tracker error" categories
                if (errorHashesIt != m_errors.end())
                    errorHashesIt->remove(trackerEntryStatus.url);
                if (trackerErrorHashesIt != m_trackerErrors.end())
                    trackerErrorHashesIt->remove(trackerEntryStatus.url);

                const bool hasNoWarningMessages = std::all_of(trackerEntryStatus.endpoints.cbegin(), trackerEntryStatus.endpoints.cend()
                    , [](const BitTorrent::TrackerEndpointStatus &endpointEntry)
                {
                    return endpointEntry.message.isEmpty() || (endpointEntry.state != BitTorrent::TrackerEndpointState::Working);
                });
                if (hasNoWarningMessages)
                {
                    if (warningHashesIt != m_warnings.end())
                    {
                        warningHashesIt->remove(trackerEntryStatus.url);
                    }
                }
                else
                {
                    if (warningHashesIt == m_warnings.end())
                        warningHashesIt = m_warnings.insert(id, {});
                    warningHashesIt->insert(trackerEntryStatus.url);
                }
            }
            break;

        case BitTorrent::TrackerEndpointState::NotWorking:
        case BitTorrent::TrackerEndpointState::Unreachable:
            {
                // remove tracker from "tracker error" and  "warning" categories
                if (warningHashesIt != m_warnings.end())
                    warningHashesIt->remove(trackerEntryStatus.url);
                if (trackerErrorHashesIt != m_trackerErrors.end())
                    trackerErrorHashesIt->remove(trackerEntryStatus.url);

                if (errorHashesIt == m_errors.end())
                    errorHashesIt = m_errors.insert(id, {});
                errorHashesIt->insert(trackerEntryStatus.url);
            }
            break;

        case BitTorrent::TrackerEndpointState::TrackerError:
            {
                // remove tracker from "error" and  "warning" categories
                if (warningHashesIt != m_warnings.end())
                    warningHashesIt->remove(trackerEntryStatus.url);
                if (errorHashesIt != m_errors.end())
                    errorHashesIt->remove(trackerEntryStatus.url);

                if (trackerErrorHashesIt == m_trackerErrors.end())
                    trackerErrorHashesIt = m_trackerErrors.insert(id, {});
                trackerErrorHashesIt->insert(trackerEntryStatus.url);
            }
            break;

        case BitTorrent::TrackerEndpointState::NotContacted:
            {
                // remove tracker from "error", "tracker error" and  "warning" categories
                if (warningHashesIt != m_warnings.end())
                    warningHashesIt->remove(trackerEntryStatus.url);
                if (errorHashesIt != m_errors.end())
                    errorHashesIt->remove(trackerEntryStatus.url);
                if (trackerErrorHashesIt != m_trackerErrors.end())
                    trackerErrorHashesIt->remove(trackerEntryStatus.url);
            }
            break;

        case BitTorrent::TrackerEndpointState::Updating:
            break;
        };
    }

    if ((errorHashesIt != m_errors.end()) && errorHashesIt->isEmpty())
        m_errors.erase(errorHashesIt);
    if ((trackerErrorHashesIt != m_trackerErrors.end()) && trackerErrorHashesIt->isEmpty())
        m_trackerErrors.erase(trackerErrorHashesIt);
    if ((warningHashesIt != m_warnings.end()) && warningHashesIt->isEmpty())
        m_warnings.erase(warningHashesIt);

    item(OTHERERROR_ROW)->setText(formatItemText(OTHERERROR_ROW, m_errors.size()));
    item(TRACKERERROR_ROW)->setText(formatItemText(TRACKERERROR_ROW, m_trackerErrors.size()));
    item(WARNING_ROW)->setText(formatItemText(WARNING_ROW, m_warnings.size()));

    if (const int row = currentRow(); (row == OTHERERROR_ROW)
        || (row == TRACKERERROR_ROW) || (row == WARNING_ROW))
    {
        applyFilter(row);
    }
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

void TrackersFilterWidget::removeTracker(const QString &tracker)
{
    for (const BitTorrent::TorrentID &torrentID : asConst(m_trackers.value(tracker).torrents))
    {
        auto *torrent = BitTorrent::Session::instance()->getTorrent(torrentID);
        Q_ASSERT(torrent);
        if (!torrent) [[unlikely]]
            continue;

        QStringList trackersToRemove;
        for (const BitTorrent::TrackerEntryStatus &trackerEntryStatus : asConst(torrent->trackers()))
        {
            if ((trackerEntryStatus.url == tracker) || (QUrl(trackerEntryStatus.url).host() == tracker))
                trackersToRemove.append(trackerEntryStatus.url);
        }

        torrent->removeTrackers({trackersToRemove});
    }

    updateGeometry();
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
            const QString faviconURL = result.url.left(result.url.size() - 4) + u".png";
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

    if (currentRow() >= NUM_SPECIAL_ROWS)
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
    if (row == ALL_ROW)
        transferList()->applyTrackerFilterAll();
    else if (isVisible())
        transferList()->applyTrackerFilter(getTorrentIDs(row));
}

void TrackersFilterWidget::handleTorrentsLoaded(const QList<BitTorrent::Torrent *> &torrents)
{
    QHash<QString, QList<BitTorrent::TorrentID>> torrentsPerTracker;
    for (const BitTorrent::Torrent *torrent : torrents)
    {
        const BitTorrent::TorrentID torrentID = torrent->id();
        const QList<BitTorrent::TrackerEntryStatus> trackers = torrent->trackers();
        for (const BitTorrent::TrackerEntryStatus &tracker : trackers)
            torrentsPerTracker[tracker.url].append(torrentID);

        // Check for trackerless torrent
        if (trackers.isEmpty())
            torrentsPerTracker[NULL_HOST].append(torrentID);
    }

    for (const auto &[trackerURL, torrents] : torrentsPerTracker.asKeyValueRange())
    {
        addItems(trackerURL, torrents);
    }

    m_totalTorrents += torrents.count();
    item(ALL_ROW)->setText(formatItemText(ALL_ROW, m_totalTorrents));
}

void TrackersFilterWidget::torrentAboutToBeDeleted(BitTorrent::Torrent *const torrent)
{
    const BitTorrent::TorrentID torrentID = torrent->id();
    const QList<BitTorrent::TrackerEntryStatus> trackers = torrent->trackers();
    for (const BitTorrent::TrackerEntryStatus &tracker : trackers)
        removeItem(tracker.url, torrentID);

    // Check for trackerless torrent
    if (trackers.isEmpty())
        removeItem(NULL_HOST, torrentID);

    item(ALL_ROW)->setText(formatItemText(ALL_ROW, --m_totalTorrents));
}

void TrackersFilterWidget::onRemoveTrackerTriggered()
{
    const int row = currentRow();
    if (row < NUM_SPECIAL_ROWS)
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
    for (int i = NUM_SPECIAL_ROWS; i < count(); ++i)
    {
        if (tracker == trackerFromRow(i))
            return i;
    }
    return -1;
}

QSet<BitTorrent::TorrentID> TrackersFilterWidget::getTorrentIDs(const int row) const
{
    switch (row)
    {
    case TRACKERLESS_ROW:
        return m_trackers.value(NULL_HOST).torrents;
    case OTHERERROR_ROW:
        return {m_errors.keyBegin(), m_errors.keyEnd()};
    case TRACKERERROR_ROW:
        return {m_trackerErrors.keyBegin(), m_trackerErrors.keyEnd()};
    case WARNING_ROW:
        return {m_warnings.keyBegin(), m_warnings.keyEnd()};
    default:
        return m_trackers.value(trackerFromRow(row)).torrents;
    }
}
