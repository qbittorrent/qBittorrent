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

#include <QFrame>
#include <QListWidget>
#include <QtContainerFwd>

#include "base/bittorrent/infohash.h"
#include "base/bittorrent/trackerentry.h"

class QCheckBox;
class QResizeEvent;

class TransferListWidget;

namespace BitTorrent
{
    class Torrent;
}

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
    TransferListWidget *transferList;

private slots:
    virtual void showMenu() = 0;
    virtual void applyFilter(int row) = 0;
    virtual void handleNewTorrent(BitTorrent::Torrent *const) = 0;
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
    void updateTorrentNumbers();

private:
    // These 4 methods are virtual slots in the base class.
    // No need to redeclare them here as slots.
    void showMenu() override;
    void applyFilter(int row) override;
    void handleNewTorrent(BitTorrent::Torrent *const) override;
    void torrentAboutToBeDeleted(BitTorrent::Torrent *const) override;
};

class TrackerFiltersList final : public BaseFilterWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TrackerFiltersList)

public:
    TrackerFiltersList(QWidget *parent, TransferListWidget *transferList, bool downloadFavicon);
    ~TrackerFiltersList() override;

    // Redefine addItem() to make sure the list stays sorted
    void addItem(const QString &tracker, const BitTorrent::TorrentID &id);
    void removeItem(const QString &tracker, const BitTorrent::TorrentID &id);
    void changeTrackerless(bool trackerless, const BitTorrent::TorrentID &id);
    void setDownloadTrackerFavicon(bool value);

public slots:
    void trackerSuccess(const BitTorrent::TorrentID &id, const QString &tracker);
    void trackerError(const BitTorrent::TorrentID &id, const QString &tracker);
    void trackerWarning(const BitTorrent::TorrentID &id, const QString &tracker);

private slots:
    void handleFavicoDownloadFinished(const Net::DownloadResult &result);

private:
    // These 4 methods are virtual slots in the base class.
    // No need to redeclare them here as slots.
    void showMenu() override;
    void applyFilter(int row) override;
    void handleNewTorrent(BitTorrent::Torrent *const torrent) override;
    void torrentAboutToBeDeleted(BitTorrent::Torrent *const torrent) override;
    QString trackerFromRow(int row) const;
    int rowFromTracker(const QString &tracker) const;
    QSet<BitTorrent::TorrentID> getTorrentIDs(int row) const;
    void downloadFavicon(const QString &url);

    QHash<QString, QSet<BitTorrent::TorrentID>> m_trackers;  // <tracker host, torrent IDs>
    QHash<BitTorrent::TorrentID, QSet<QString>> m_errors;  // <torrent ID, tracker hosts>
    QHash<BitTorrent::TorrentID, QSet<QString>> m_warnings;  // <torrent ID, tracker hosts>
    QStringList m_iconPaths;
    int m_totalTorrents;
    bool m_downloadTrackerFavicon;
};

class CategoryFilterWidget;
class TagFilterWidget;

class TransferListFiltersWidget final : public QFrame
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TransferListFiltersWidget)

public:
    TransferListFiltersWidget(QWidget *parent, TransferListWidget *transferList, bool downloadFavicon);
    void setDownloadTrackerFavicon(bool value);

public slots:
    void addTrackers(const BitTorrent::Torrent *torrent, const QVector<BitTorrent::TrackerEntry> &trackers);
    void removeTrackers(const BitTorrent::Torrent *torrent, const QVector<BitTorrent::TrackerEntry> &trackers);
    void changeTrackerless(const BitTorrent::Torrent *torrent, bool trackerless);
    void trackerSuccess(const BitTorrent::Torrent *torrent, const QString &tracker);
    void trackerWarning(const BitTorrent::Torrent *torrent, const QString &tracker);
    void trackerError(const BitTorrent::Torrent *torrent, const QString &tracker);

signals:
    void trackerSuccess(const BitTorrent::TorrentID &id, const QString &tracker);
    void trackerError(const BitTorrent::TorrentID &id, const QString &tracker);
    void trackerWarning(const BitTorrent::TorrentID &id, const QString &tracker);

private slots:
    void onCategoryFilterStateChanged(bool enabled);
    void onTagFilterStateChanged(bool enabled);

private:
    void toggleCategoryFilter(bool enabled);
    void toggleTagFilter(bool enabled);

    TransferListWidget *m_transferList;
    TrackerFiltersList *m_trackerFilters;
    CategoryFilterWidget *m_categoryFilterWidget;
    TagFilterWidget *m_tagFilterWidget;
};
