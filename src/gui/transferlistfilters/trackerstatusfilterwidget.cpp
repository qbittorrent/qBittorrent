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

#include "trackerstatusfilterwidget.h"

#include <QCheckBox>
#include <QIcon>
#include <QListWidgetItem>
#include <QMenu>

#include "base/bittorrent/session.h"
#include "base/global.h"
#include "base/preferences.h"
#include "gui/transferlistwidget.h"
#include "gui/uithememanager.h"

namespace
{
    enum TRACKERSTATUS_FILTER_ROW
    {
        ANY_ROW,
        WARNING_ROW,
        TRACKERERROR_ROW,
        OTHERERROR_ROW,

        NUM_SPECIAL_ROWS
    };

    QString getFormatStringForRow(const int row)
    {
        switch (row)
        {
        case ANY_ROW:
            return TrackerStatusFilterWidget::tr("All (%1)", "this is for the tracker filter");
        case WARNING_ROW:
            return TrackerStatusFilterWidget::tr("Warning (%1)");
        case TRACKERERROR_ROW:
            return TrackerStatusFilterWidget::tr("Tracker error (%1)");
        case OTHERERROR_ROW:
            return TrackerStatusFilterWidget::tr("Other error (%1)");
        default:
            return {};
        }
    }

    QString formatItemText(const int row, const int torrentsCount)
    {
        return getFormatStringForRow(row).arg(torrentsCount);
    }
}

TrackerStatusFilterWidget::TrackerStatusFilterWidget(QWidget *parent, TransferListWidget *transferList)
    : BaseFilterWidget(parent, transferList)
{
    auto *anyStatusItem = new QListWidgetItem(this);
    anyStatusItem->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"trackers"_s, u"network-server"_s));

    auto *warningItem = new QListWidgetItem(this);
    warningItem->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"tracker-warning"_s, u"dialog-warning"_s));

    auto *trackerErrorItem = new QListWidgetItem(this);
    trackerErrorItem->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"tracker-error"_s, u"dialog-error"_s));

    auto *otherErrorItem = new QListWidgetItem(this);
    otherErrorItem->setData(Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"tracker-error"_s, u"dialog-error"_s));

    const auto *btSession = BitTorrent::Session::instance();

    const QList<BitTorrent::Torrent *> torrents = btSession->torrents();
    m_totalTorrents += torrents.count();

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

    connect(btSession, &BitTorrent::Session::trackersRemoved, this, &TrackerStatusFilterWidget::handleTorrentTrackersRemoved);
    connect(btSession, &BitTorrent::Session::trackersReset, this, &TrackerStatusFilterWidget::handleTorrentTrackersReset);
    connect(btSession, &BitTorrent::Session::trackerEntryStatusesUpdated, this, &TrackerStatusFilterWidget::handleTorrentTrackerStatusesUpdated);

    anyStatusItem->setText(formatItemText(ANY_ROW, m_totalTorrents));
    warningItem->setText(formatItemText(WARNING_ROW, m_warnings.size()));
    trackerErrorItem->setText(formatItemText(TRACKERERROR_ROW, m_trackerErrors.size()));
    otherErrorItem->setText(formatItemText(OTHERERROR_ROW, m_errors.size()));

    setCurrentRow(0, QItemSelectionModel::SelectCurrent);
    setVisible(Preferences::instance()->getTrackerStatusFilterState());
}

void TrackerStatusFilterWidget::handleTorrentTrackersRemoved(const BitTorrent::Torrent *torrent)
{
    refreshItems(torrent);
}

void TrackerStatusFilterWidget::handleTorrentTrackersReset(const BitTorrent::Torrent *torrent
        , [[maybe_unused]] const QList<BitTorrent::TrackerEntryStatus> &oldEntries, [[maybe_unused]] const QList<BitTorrent::TrackerEntry> &newEntries)
{
    refreshItems(torrent);
}

void TrackerStatusFilterWidget::handleTorrentTrackerStatusesUpdated(const BitTorrent::Torrent *torrent)
{
    refreshItems(torrent);
}

void TrackerStatusFilterWidget::showMenu()
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    menu->addAction(UIThemeManager::instance()->getIcon(u"torrent-start"_s, u"media-playback-start"_s), tr("Start torrents")
        , transferList(), &TransferListWidget::startVisibleTorrents);
    menu->addAction(UIThemeManager::instance()->getIcon(u"torrent-stop"_s, u"media-playback-pause"_s), tr("Stop torrents")
        , transferList(), &TransferListWidget::stopVisibleTorrents);
    menu->addAction(UIThemeManager::instance()->getIcon(u"list-remove"_s), tr("Remove torrents")
        , transferList(), &TransferListWidget::deleteVisibleTorrents);

    menu->popup(QCursor::pos());
}

void TrackerStatusFilterWidget::applyFilter(const int row)
{
    switch (row)
    {
    case ANY_ROW:
        transferList()->applyAnnounceStatusFilter(std::nullopt);
        break;

    case WARNING_ROW:
        transferList()->applyAnnounceStatusFilter(BitTorrent::TorrentAnnounceStatusFlag::HasWarning);
        break;

    case TRACKERERROR_ROW:
        transferList()->applyAnnounceStatusFilter(BitTorrent::TorrentAnnounceStatusFlag::HasTrackerError);
        break;

    case OTHERERROR_ROW:
        transferList()->applyAnnounceStatusFilter(BitTorrent::TorrentAnnounceStatusFlag::HasOtherError);
        break;
    }
}

void TrackerStatusFilterWidget::handleTorrentsLoaded(const QList<BitTorrent::Torrent *> &torrents)
{
    m_totalTorrents += torrents.count();
    item(ANY_ROW)->setText(formatItemText(ANY_ROW, m_totalTorrents));
}

void TrackerStatusFilterWidget::refreshItems(const BitTorrent::Torrent *torrent)
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

void TrackerStatusFilterWidget::torrentAboutToBeDeleted(BitTorrent::Torrent *const torrent)
{
    m_warnings.remove(torrent);
    m_trackerErrors.remove(torrent);
    m_errors.remove(torrent);

    item(ANY_ROW)->setText(formatItemText(ANY_ROW, --m_totalTorrents));
    item(WARNING_ROW)->setText(formatItemText(WARNING_ROW, m_warnings.size()));
    item(TRACKERERROR_ROW)->setText(formatItemText(TRACKERERROR_ROW, m_trackerErrors.size()));
    item(OTHERERROR_ROW)->setText(formatItemText(OTHERERROR_ROW, m_errors.size()));
}
