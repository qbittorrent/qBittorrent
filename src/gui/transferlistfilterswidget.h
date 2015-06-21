/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
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
 *
 * Contact : chris@qbittorrent.org
 */

#ifndef TRANSFERLISTFILTERSWIDGET_H
#define TRANSFERLISTFILTERSWIDGET_H

#include <QListWidget>
#include <QFrame>

QT_BEGIN_NAMESPACE
class QResizeEvent;
class QCheckBox;
QT_END_NAMESPACE

class TransferListWidget;

namespace BitTorrent
{
    class TorrentHandle;
    class TrackerEntry;
    struct TorrentStatusReport;
}

class FiltersBase: public QListWidget
{
    Q_OBJECT

public:
    FiltersBase(QWidget *parent, TransferListWidget *transferList);

    virtual QSize sizeHint() const;
    virtual QSize minimumSizeHint() const;

public slots:
    void toggleFilter(bool checked);

protected:
    TransferListWidget *transferList;

private slots:
    virtual void showMenu(QPoint) = 0;
    virtual void applyFilter(int row) = 0;
    virtual void handleNewTorrent(BitTorrent::TorrentHandle *const) = 0;
    virtual void torrentAboutToBeDeleted(BitTorrent::TorrentHandle *const) = 0;
};

class StatusFiltersWidget: public FiltersBase
{
    Q_OBJECT

public:
    StatusFiltersWidget(QWidget *parent, TransferListWidget *transferList);
    ~StatusFiltersWidget();

private slots:
    void updateTorrentNumbers(const BitTorrent::TorrentStatusReport &report);

private:
    // These 4 methods are virtual slots in the base class.
    // No need to redeclare them here as slots.
    virtual void showMenu(QPoint);
    virtual void applyFilter(int row);
    virtual void handleNewTorrent(BitTorrent::TorrentHandle *const);
    virtual void torrentAboutToBeDeleted(BitTorrent::TorrentHandle *const);
};

class LabelFiltersList: public FiltersBase
{
    Q_OBJECT

public:
    LabelFiltersList(QWidget *parent, TransferListWidget *transferList);
    ~LabelFiltersList();

private slots:
    // Redefine addItem() to make sure the list stays sorted
    void addItem(QString &label, bool hasTorrent);
    void removeItem(const QString &label);
    void removeSelectedLabel();
    void removeUnusedLabels();
    void torrentChangedLabel(BitTorrent::TorrentHandle *const torrent, const QString &oldLabel);


private:
    // These 4 methods are virtual slots in the base class.
    // No need to redeclare them here as slots.
    virtual void showMenu(QPoint);
    virtual void applyFilter(int row);
    virtual void handleNewTorrent(BitTorrent::TorrentHandle *const torrent);
    virtual void torrentAboutToBeDeleted(BitTorrent::TorrentHandle *const torrent);
    QString labelFromRow(int row) const;
    int rowFromLabel(const QString &label) const;

private:
    QHash<QString, int> m_labels;
    int m_totalTorrents;
    int m_totalLabeled;
};

class TrackerFiltersList: public FiltersBase
{
    Q_OBJECT

public:
    TrackerFiltersList(QWidget *parent, TransferListWidget *transferList);
    ~TrackerFiltersList();

    // Redefine addItem() to make sure the list stays sorted
    void addItem(const QString &tracker, const QString &hash);
    void removeItem(const QString &tracker, const QString &hash);
    void changeTrackerless(bool trackerless, const QString &hash);

public slots:
    void trackerSuccess(const QString &hash, const QString &tracker);
    void trackerError(const QString &hash, const QString &tracker);
    void trackerWarning(const QString &hash, const QString &tracker);

private slots:
    void handleFavicoDownload(const QString &url, const QString &filePath);
    void handleFavicoFailure(const QString &url, const QString &reason);

private:
    // These 4 methods are virtual slots in the base class.
    // No need to redeclare them here as slots.
    virtual void showMenu(QPoint);
    virtual void applyFilter(int row);
    virtual void handleNewTorrent(BitTorrent::TorrentHandle *const torrent);
    virtual void torrentAboutToBeDeleted(BitTorrent::TorrentHandle *const torrent);
    QString trackerFromRow(int row) const;
    int rowFromTracker(const QString &tracker) const;
    QString getHost(const QString &trakcer) const;
    QStringList getHashes(int row);

private:
    QHash<QString, QStringList> m_trackers;
    QHash<QString, QStringList> m_errors;
    QHash<QString, QStringList> m_warnings;
    QStringList m_iconPaths;
    int m_totalTorrents;
};

class TransferListFiltersWidget: public QFrame
{
    Q_OBJECT

public:
    TransferListFiltersWidget(QWidget *parent, TransferListWidget *transferList);

public slots:
    void addTrackers(BitTorrent::TorrentHandle *const torrent, const QList<BitTorrent::TrackerEntry> &trackers);
    void removeTrackers(BitTorrent::TorrentHandle *const torrent, const QList<BitTorrent::TrackerEntry> &trackers);
    void changeTrackerless(BitTorrent::TorrentHandle *const torrent, bool trackerless);
    void trackerSuccess(BitTorrent::TorrentHandle *const torrent, const QString &tracker);
    void trackerWarning(BitTorrent::TorrentHandle *const torrent, const QString &tracker);
    void trackerError(BitTorrent::TorrentHandle *const torrent, const QString &tracker);

signals:
    void trackerSuccess(const QString &hash, const QString &tracker);
    void trackerError(const QString &hash, const QString &tracker);
    void trackerWarning(const QString &hash, const QString &tracker);

private:
    TrackerFiltersList *trackerFilters;
};

#endif // TRANSFERLISTFILTERSWIDGET_H
