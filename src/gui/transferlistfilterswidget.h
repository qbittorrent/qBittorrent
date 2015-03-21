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
class QListWidgetItem;
class QVBoxLayout;
class QDragMoveEvent;
QT_END_NAMESPACE

class TransferListWidget;
class TorrentModelItem;
class QTorrentHandle;
class DownloadThread;

class LabelFiltersList: public QListWidget
{
    Q_OBJECT

private:
    QListWidgetItem * itemHover;

public:
    LabelFiltersList(QWidget *parent);

    // Redefine addItem() to make sure the list stays sorted
    void addItem(QListWidgetItem *it);

    QString labelFromRow(int row) const;
    int rowFromLabel(QString label) const;

signals:
    void torrentDropped(int label_row);

protected:
    void dragMoveEvent(QDragMoveEvent *event);
    void dropEvent(QDropEvent *event);
    void dragLeaveEvent(QDragLeaveEvent*);
    void setItemHover(bool hover);
};

class StatusFiltersWidget: public QListWidget
{
    Q_OBJECT

public:
    StatusFiltersWidget(QWidget *parent);

protected:
    QSize sizeHint() const;

private:
    bool m_shown;
};

class TrackerFiltersList: public QListWidget
{
    Q_OBJECT

public:
    TrackerFiltersList(QWidget *parent);
    ~TrackerFiltersList();

    // Redefine addItem() to make sure the list stays sorted
    void addItem(const QString &tracker, const QString &hash);
    void addItem(const QTorrentHandle &handle);
    void removeItem(const QString &tracker, const QString &hash);
    void removeItem(const QTorrentHandle &handle);
    void setTorrentCount(int all);
    QStringList getHashes(int row);

public slots:
    void handleFavicoDownload(const QString &url, const QString &filePath);
    void handleFavicoFailure(const QString &url, const QString &reason);

private:
    QHash<QString, QStringList> m_trackers;
    QString trackerFromRow(int row) const;
    int rowFromTracker(const QString &tracker) const;
    QString getHost(const QString &trakcer) const;
    DownloadThread *m_downloader;
    QStringList m_iconPaths;
};

class TransferListFiltersWidget: public QFrame
{
    Q_OBJECT

private:
    QHash<QString, int> customLabels;
    StatusFiltersWidget* statusFilters;
    LabelFiltersList* labelFilters;
    TrackerFiltersList* trackerFilters;
    QVBoxLayout* vLayout;
    TransferListWidget *transferList;
    int nb_labeled;
    int nb_torrents;

public:
    TransferListFiltersWidget(QWidget *parent, TransferListWidget *transferList);
    ~TransferListFiltersWidget();

    StatusFiltersWidget* getStatusFilters() const;

    void saveSettings() const;
    void loadSettings();

public slots:
    void addTracker(const QString &tracker, const QString &hash);
    void removeTracker(const QString &tracker, const QString &hash);

protected slots:
    void updateTorrentNumbers();
    void torrentDropped(int row);
    void addLabel(QString& label);
    void showLabelMenu(QPoint);
    void showTrackerMenu(QPoint);
    void removeSelectedLabel();
    void removeUnusedLabels();
    void applyLabelFilter(int row);
    void applyTrackerFilter(int row);
    void torrentChangedLabel(TorrentModelItem *torrentItem, QString old_label, QString new_label);
    void handleNewTorrent(TorrentModelItem* torrentItem);
    void torrentAboutToBeDeleted(TorrentModelItem* torrentItem);
    void updateStickyLabelCounters();
};

#endif // TRANSFERLISTFILTERSWIDGET_H
