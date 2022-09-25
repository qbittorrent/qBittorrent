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

#pragma once

#include <bitset>

#include <QFrame>
#include <QHash>
#include <QListWidget>
#include <QtContainerFwd>

#include "base/bittorrent/infohash.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/trackerentry.h"
#include "base/torrentfilter.h"
#include "base/path.h"

class QCheckBox;
class QResizeEvent;

class CategoryFilterWidget;
class TagFilterWidget;
class TransferListWidget;

namespace Net
{
    struct DownloadResult;
}

class BaseFilterWidget : public QListWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(BaseFilterWidget)

public:
    BaseFilterWidget(QWidget *parent, TransferListWidget *transferList);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void toggleFilter(bool checked);

protected:
    TransferListWidget *transferList = nullptr;

private slots:
    virtual void showMenu() = 0;
    virtual void applyFilter(int row) = 0;
    virtual void handleTorrentsLoaded(const QVector<BitTorrent::Torrent *> &torrents) = 0;
    virtual void torrentAboutToBeDeleted(BitTorrent::Torrent *const) = 0;
};

class StatusFilterWidget final : public BaseFilterWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(StatusFilterWidget)

public:
    StatusFilterWidget(QWidget *parent, TransferListWidget *transferList);
    ~StatusFilterWidget() override;

private slots:
    void handleTorrentsUpdated(const QVector<BitTorrent::Torrent *> torrents);

private:
    // These 4 methods are virtual slots in the base class.
    // No need to redeclare them here as slots.
    void showMenu() override;
    void applyFilter(int row) override;
    void handleTorrentsLoaded(const QVector<BitTorrent::Torrent *> &torrents) override;
    void torrentAboutToBeDeleted(BitTorrent::Torrent *const) override;

    void populate();
    void updateTorrentStatus(const BitTorrent::Torrent *torrent);
    void updateTexts();

    using TorrentFilterBitset = std::bitset<32>;  // approximated size, this should be the number of TorrentFilter::Type elements
    QHash<const BitTorrent::Torrent *, TorrentFilterBitset> m_torrentsStatus;
    int m_nbDownloading = 0;
    int m_nbSeeding = 0;
    int m_nbCompleted = 0;
    int m_nbResumed = 0;
    int m_nbPaused = 0;
    int m_nbActive = 0;
    int m_nbInactive = 0;
    int m_nbStalled = 0;
    int m_nbStalledUploading = 0;
    int m_nbStalledDownloading = 0;
    int m_nbChecking = 0;
    int m_nbErrored = 0;
};

class TrackerFiltersList final : public BaseFilterWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TrackerFiltersList)

public:
    TrackerFiltersList(QWidget *parent, TransferListWidget *transferList, bool downloadFavicon);
    ~TrackerFiltersList() override;

    void addTrackers(const BitTorrent::Torrent *torrent, const QVector<BitTorrent::TrackerEntry> &trackers);
    void removeTrackers(const BitTorrent::Torrent *torrent, const QStringList &trackers);
    void refreshTrackers(const BitTorrent::Torrent *torrent);
    void changeTrackerless(const BitTorrent::Torrent *torrent, bool trackerless);
    void handleTrackerEntriesUpdated(const QHash<BitTorrent::Torrent *, QSet<QString>> &updateInfos);
    void setDownloadTrackerFavicon(bool value);

private slots:
    void handleFavicoDownloadFinished(const Net::DownloadResult &result);

private:
    // These 4 methods are virtual slots in the base class.
    // No need to redeclare them here as slots.
    void showMenu() override;
    void applyFilter(int row) override;
    void handleTorrentsLoaded(const QVector<BitTorrent::Torrent *> &torrents) override;
    void torrentAboutToBeDeleted(BitTorrent::Torrent *const torrent) override;

    void addItems(const QString &trackerURL, const QVector<BitTorrent::TorrentID> &torrents);
    void removeItem(const QString &trackerURL, const BitTorrent::TorrentID &id);
    QString trackerFromRow(int row) const;
    int rowFromTracker(const QString &tracker) const;
    QSet<BitTorrent::TorrentID> getTorrentIDs(int row) const;
    void downloadFavicon(const QString &url);

    struct TrackerData
    {
        QSet<BitTorrent::TorrentID> torrents;
        QListWidgetItem *item = nullptr;
    };

    QHash<QString, TrackerData> m_trackers;
    QHash<BitTorrent::TorrentID, QSet<QString>> m_errors;  // <torrent ID, tracker hosts>
    QHash<BitTorrent::TorrentID, QSet<QString>> m_warnings;  // <torrent ID, tracker hosts>
    PathList m_iconPaths;
    int m_totalTorrents;
    bool m_downloadTrackerFavicon;
};

class TransferListFiltersWidget final : public QFrame
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TransferListFiltersWidget)

public:
    TransferListFiltersWidget(QWidget *parent, TransferListWidget *transferList, bool downloadFavicon);
    void setDownloadTrackerFavicon(bool value);

public slots:
    void addTrackers(const BitTorrent::Torrent *torrent, const QVector<BitTorrent::TrackerEntry> &trackers);
    void removeTrackers(const BitTorrent::Torrent *torrent, const QStringList &trackers);
    void refreshTrackers(const BitTorrent::Torrent *torrent);
    void changeTrackerless(const BitTorrent::Torrent *torrent, bool trackerless);
    void trackerEntriesUpdated(const QHash<BitTorrent::Torrent *, QSet<QString>> &updateInfos);

private slots:
    void onCategoryFilterStateChanged(bool enabled);
    void onTagFilterStateChanged(bool enabled);

private:
    void toggleCategoryFilter(bool enabled);
    void toggleTagFilter(bool enabled);

    TransferListWidget *m_transferList = nullptr;
    TrackerFiltersList *m_trackerFilters = nullptr;
    CategoryFilterWidget *m_categoryFilterWidget = nullptr;
    TagFilterWidget *m_tagFilterWidget = nullptr;
};
