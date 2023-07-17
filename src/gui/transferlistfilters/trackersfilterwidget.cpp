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

#include <QIcon>
#include <QListWidgetItem>
#include <QMenu>
#include <QUrl>

#include "base/algorithm.h"
#include "base/bittorrent/session.h"
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
        ERROR_ROW,
        WARNING_ROW
    };

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

    const QString NULL_HOST = u""_s;
}

TrackersFilterWidget::TrackersFilterWidget(QWidget *parent, TransferListWidget *transferList, const bool downloadFavicon)
    : BaseFilterWidget(parent, transferList)
    , m_downloadTrackerFavicon(downloadFavicon)
{
    auto *allTrackers = new QListWidgetItem(this);
    allTrackers->setData(Qt::DisplayRole, tr("All (0)", "this is for the tracker filter"));
    allTrackers->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"trackers"_s, u"network-server"_s));
    auto *noTracker = new QListWidgetItem(this);
    noTracker->setData(Qt::DisplayRole, tr("Trackerless (0)"));
    noTracker->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"trackerless"_s, u"network-server"_s));
    auto *errorTracker = new QListWidgetItem(this);
    errorTracker->setData(Qt::DisplayRole, tr("Error (0)"));
    errorTracker->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"tracker-error"_s, u"dialog-error"_s));
    auto *warningTracker = new QListWidgetItem(this);
    warningTracker->setData(Qt::DisplayRole, tr("Warning (0)"));
    warningTracker->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"tracker-warning"_s, u"dialog-warning"_s));

    m_trackers[NULL_HOST] = {{}, noTracker};

    handleTorrentsLoaded(BitTorrent::Session::instance()->torrents());

    setCurrentRow(0, QItemSelectionModel::SelectCurrent);
    toggleFilter(Preferences::instance()->getTrackerFilterState());
}

TrackersFilterWidget::~TrackersFilterWidget()
{
    for (const Path &iconPath : asConst(m_iconPaths))
        Utils::Fs::removeFile(iconPath);
}

void TrackersFilterWidget::addTrackers(const BitTorrent::Torrent *torrent, const QVector<BitTorrent::TrackerEntry> &trackers)
{
    const BitTorrent::TorrentID torrentID = torrent->id();
    for (const BitTorrent::TrackerEntry &tracker : trackers)
        addItems(tracker.url, {torrentID});
}

void TrackersFilterWidget::removeTrackers(const BitTorrent::Torrent *torrent, const QStringList &trackers)
{
    const BitTorrent::TorrentID torrentID = torrent->id();
    for (const QString &tracker : trackers)
        removeItem(tracker, torrentID);
}

void TrackersFilterWidget::refreshTrackers(const BitTorrent::Torrent *torrent)
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

        trackerItem->setText(u"%1 (%2)"_s.arg((host.isEmpty() ? tr("Trackerless") : host), QString::number(torrentIDs.size())));
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

void TrackersFilterWidget::changeTrackerless(const BitTorrent::Torrent *torrent, const bool trackerless)
{
    if (trackerless)
        addItems(NULL_HOST, {torrent->id()});
    else
        removeItem(NULL_HOST, torrent->id());
}

void TrackersFilterWidget::addItems(const QString &trackerURL, const QVector<BitTorrent::TorrentID> &torrents)
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

    trackerItem->setText(u"%1 (%2)"_s.arg(((host == NULL_HOST) ? tr("Trackerless") : host), QString::number(torrentIDs.size())));
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

void TrackersFilterWidget::removeItem(const QString &trackerURL, const BitTorrent::TorrentID &id)
{
    const QString host = getHost(trackerURL);

    QSet<BitTorrent::TorrentID> torrentIDs = m_trackers.value(host).torrents;
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
            trackerItem->setText(u"%1 (%2)"_s.arg(host, QString::number(torrentIDs.size())));
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

void TrackersFilterWidget::handleTrackerEntriesUpdated(const BitTorrent::Torrent *torrent
        , const QHash<QString, BitTorrent::TrackerEntry> &updatedTrackerEntries)
{
    const BitTorrent::TorrentID id = torrent->id();

    auto errorHashesIt = m_errors.find(id);
    auto warningHashesIt = m_warnings.find(id);

    for (const BitTorrent::TrackerEntry &trackerEntry : updatedTrackerEntries)
    {
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

    item(ERROR_ROW)->setText(tr("Error (%1)").arg(m_errors.size()));
    item(WARNING_ROW)->setText(tr("Warning (%1)").arg(m_warnings.size()));

    if (currentRow() == ERROR_ROW)
        applyFilter(ERROR_ROW);
    else if (currentRow() == WARNING_ROW)
        applyFilter(WARNING_ROW);
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

void TrackersFilterWidget::handleFavicoDownloadFinished(const Net::DownloadResult &result)
{
    const QSet<QString> trackerHosts = m_downloadingFavicons.take(result.url);
    Q_ASSERT(!trackerHosts.isEmpty());
    if (Q_UNLIKELY(trackerHosts.isEmpty()))
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
        if (Q_UNLIKELY(!trackerItem))
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

    menu->addAction(UIThemeManager::instance()->getIcon(u"torrent-start"_s, u"media-playback-start"_s), tr("Resume torrents")
        , transferList(), &TransferListWidget::startVisibleTorrents);
    menu->addAction(UIThemeManager::instance()->getIcon(u"torrent-stop"_s, u"media-playback-pause"_s), tr("Pause torrents")
        , transferList(), &TransferListWidget::pauseVisibleTorrents);
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

void TrackersFilterWidget::handleTorrentsLoaded(const QVector<BitTorrent::Torrent *> &torrents)
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

void TrackersFilterWidget::torrentAboutToBeDeleted(BitTorrent::Torrent *const torrent)
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
    for (int i = 4; i < count(); ++i)
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
    case ERROR_ROW:
        return {m_errors.keyBegin(), m_errors.keyEnd()};
    case WARNING_ROW:
        return {m_warnings.keyBegin(), m_warnings.keyEnd()};
    default:
        return m_trackers.value(trackerFromRow(row)).torrents;
    }
}
