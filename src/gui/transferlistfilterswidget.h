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

#ifndef TRANSFERLISTFILTERSWIDGET_H
#define TRANSFERLISTFILTERSWIDGET_H

#include <QFrame>
#include <QListWidget>

class QCheckBox;
class QResizeEvent;

class TransferListWidget;

namespace BitTorrent
{
    class TorrentHandle;
    class TrackerEntry;
}

namespace Net
{
    struct DownloadResult;
}

class BaseFilterWidget : public QListWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(BaseFilterWidget)

public:
    BaseFilterWidget(QWidget *parent, TransferListWidget *transferList);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void toggleFilter(bool checked);

protected:
    TransferListWidget *transferList;

private slots:
    virtual void showMenu(const QPoint &) = 0;
    virtual void applyFilter(int row) = 0;
    virtual void handleNewTorrent(BitTorrent::TorrentHandle *const) = 0;
    virtual void torrentAboutToBeDeleted(BitTorrent::TorrentHandle *const) = 0;
};

class StatusFilterWidget : public BaseFilterWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(StatusFilterWidget)

public:
    StatusFilterWidget(QWidget *parent, TransferListWidget *transferList);
    ~StatusFilterWidget() override;

private slots:
    void updateTorrentNumbers();

private:
    // These 4 methods are virtual slots in the base class.
    // No need to redeclare them here as slots.
    void showMenu(const QPoint &) override;
    void applyFilter(int row) override;
    void handleNewTorrent(BitTorrent::TorrentHandle *const) override;
    void torrentAboutToBeDeleted(BitTorrent::TorrentHandle *const) override;
};

class TrackerFiltersList : public BaseFilterWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(TrackerFiltersList)

public:
    TrackerFiltersList(QWidget *parent, TransferListWidget *transferList, bool downloadFavicon);
    ~TrackerFiltersList() override;

    // Redefine addItem() to make sure the list stays sorted
    void addItem(const QString &tracker, const QString &hash);
    void removeItem(const QString &tracker, const QString &hash);
    void changeTrackerless(bool trackerless, const QString &hash);
    void setDownloadTrackerFavicon(bool value);

public slots:
    void trackerSuccess(const QString &hash, const QString &tracker);
    void trackerError(const QString &hash, const QString &tracker);
    void trackerWarning(const QString &hash, const QString &tracker);

private slots:
    void handleFavicoDownloadFinished(const Net::DownloadResult &result);

private:
    // These 4 methods are virtual slots in the base class.
    // No need to redeclare them here as slots.
    void showMenu(const QPoint &) override;
    void applyFilter(int row) override;
    void handleNewTorrent(BitTorrent::TorrentHandle *const torrent) override;
    void torrentAboutToBeDeleted(BitTorrent::TorrentHandle *const torrent) override;
    QString trackerFromRow(int row) const;
    int rowFromTracker(const QString &tracker) const;
    QString getHost(const QString &tracker) const;
    QStringList getHashes(int row) const;
    void downloadFavicon(const QString &url);

    QHash<QString, QStringList> m_trackers;
    QHash<QString, QStringList> m_errors;
    QHash<QString, QStringList> m_warnings;
    QStringList m_iconPaths;
    int m_totalTorrents;
    bool m_downloadTrackerFavicon;
};

class CategoryFilterWidget;
class TagFilterWidget;

class TransferListFiltersWidget : public QFrame
{
    Q_OBJECT

public:
    TransferListFiltersWidget(QWidget *parent, TransferListWidget *transferList, bool downloadFavicon);
    void setDownloadTrackerFavicon(bool value);

public slots:
    void addTrackers(BitTorrent::TorrentHandle *const torrent, const QVector<BitTorrent::TrackerEntry> &trackers);
    void removeTrackers(BitTorrent::TorrentHandle *const torrent, const QVector<BitTorrent::TrackerEntry> &trackers);
    void changeTrackerless(BitTorrent::TorrentHandle *const torrent, bool trackerless);
    void trackerSuccess(BitTorrent::TorrentHandle *const torrent, const QString &tracker);
    void trackerWarning(BitTorrent::TorrentHandle *const torrent, const QString &tracker);
    void trackerError(BitTorrent::TorrentHandle *const torrent, const QString &tracker);

signals:
    void trackerSuccess(const QString &hash, const QString &tracker);
    void trackerError(const QString &hash, const QString &tracker);
    void trackerWarning(const QString &hash, const QString &tracker);

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

#endif // TRANSFERLISTFILTERSWIDGET_H
