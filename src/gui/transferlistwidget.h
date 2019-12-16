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

#ifndef TRANSFERLISTWIDGET_H
#define TRANSFERLISTWIDGET_H

#include <functional>
#include <QTreeView>
#include <QVector>

class MainWindow;
class TransferListDelegate;
class TransferListModel;
class TransferListSortModel;

namespace BitTorrent
{
    class TorrentHandle;
}

class TransferListWidget : public QTreeView
{
    Q_OBJECT

    Q_PROPERTY(QColor unknownStateForeground READ unknownStateForeground WRITE setUnknownStateForeground)
    Q_PROPERTY(QColor forcedDownloadingStateForeground READ forcedDownloadingStateForeground WRITE setForcedDownloadingStateForeground)
    Q_PROPERTY(QColor downloadingStateForeground READ downloadingStateForeground WRITE setDownloadingStateForeground)
    Q_PROPERTY(QColor downloadingMetadataStateForeground READ downloadingMetadataStateForeground WRITE setDownloadingMetadataStateForeground)
    Q_PROPERTY(QColor allocatingStateForeground READ allocatingStateForeground WRITE setAllocatingStateForeground)
    Q_PROPERTY(QColor stalledDownloadingStateForeground READ stalledDownloadingStateForeground WRITE setStalledDownloadingStateForeground)
    Q_PROPERTY(QColor forcedUploadingStateForeground READ forcedUploadingStateForeground WRITE setForcedUploadingStateForeground)
    Q_PROPERTY(QColor uploadingStateForeground READ uploadingStateForeground WRITE setUploadingStateForeground)
    Q_PROPERTY(QColor stalledUploadingStateForeground READ stalledUploadingStateForeground WRITE setStalledUploadingStateForeground)
    Q_PROPERTY(QColor checkingResumeDataStateForeground READ checkingResumeDataStateForeground WRITE setCheckingResumeDataStateForeground)
    Q_PROPERTY(QColor queuedDownloadingStateForeground READ queuedDownloadingStateForeground WRITE setQueuedDownloadingStateForeground)
    Q_PROPERTY(QColor queuedUploadingStateForeground READ queuedUploadingStateForeground WRITE setQueuedUploadingStateForeground)
    Q_PROPERTY(QColor checkingUploadingStateForeground READ checkingUploadingStateForeground WRITE setCheckingUploadingStateForeground)
    Q_PROPERTY(QColor checkingDownloadingStateForeground READ checkingDownloadingStateForeground WRITE setCheckingDownloadingStateForeground)
    Q_PROPERTY(QColor pausedDownloadingStateForeground READ pausedDownloadingStateForeground WRITE setPausedDownloadingStateForeground)
    Q_PROPERTY(QColor pausedUploadingStateForeground READ pausedUploadingStateForeground WRITE setPausedUploadingStateForeground)
    Q_PROPERTY(QColor movingStateForeground READ movingStateForeground WRITE setMovingStateForeground)
    Q_PROPERTY(QColor missingFilesStateForeground READ missingFilesStateForeground WRITE setMissingFilesStateForeground)
    Q_PROPERTY(QColor errorStateForeground READ errorStateForeground WRITE setErrorStateForeground)

public:
    TransferListWidget(QWidget *parent, MainWindow *mainWindow);
    ~TransferListWidget() override;
    TransferListModel *getSourceModel() const;

public slots:
    void setSelectionCategory(const QString &category);
    void addSelectionTag(const QString &tag);
    void removeSelectionTag(const QString &tag);
    void clearSelectionTags();
    void setSelectedTorrentsLocation();
    void pauseAllTorrents();
    void resumeAllTorrents();
    void startSelectedTorrents();
    void forceStartSelectedTorrents();
    void startVisibleTorrents();
    void pauseSelectedTorrents();
    void pauseVisibleTorrents();
    void softDeleteSelectedTorrents();
    void permDeleteSelectedTorrents();
    void deleteSelectedTorrents(bool deleteLocalFiles);
    void deleteVisibleTorrents();
    void increaseQueuePosSelectedTorrents();
    void decreaseQueuePosSelectedTorrents();
    void topQueuePosSelectedTorrents();
    void bottomQueuePosSelectedTorrents();
    void copySelectedMagnetURIs() const;
    void copySelectedNames() const;
    void copySelectedHashes() const;
    void openSelectedTorrentsFolder() const;
    void recheckSelectedTorrents();
    void reannounceSelectedTorrents();
    void setDlLimitSelectedTorrents();
    void setUpLimitSelectedTorrents();
    void setMaxRatioSelectedTorrents();
    void previewSelectedTorrents();
    void hideQueuePosColumn(bool hide);
    void displayDLHoSMenu(const QPoint&);
    void applyNameFilter(const QString &name);
    void applyStatusFilter(int f);
    void applyCategoryFilter(const QString &category);
    void applyTagFilter(const QString &tag);
    void applyTrackerFilterAll();
    void applyTrackerFilter(const QStringList &hashes);
    void previewFile(const QString &filePath);
    void renameSelectedTorrent();

protected:
    QModelIndex mapToSource(const QModelIndex &index) const;
    QModelIndex mapFromSource(const QModelIndex &index) const;
    bool loadSettings();
    QVector<BitTorrent::TorrentHandle *> getSelectedTorrents() const;

protected slots:
    void torrentDoubleClicked();
    void displayListMenu(const QPoint &);
    void currentChanged(const QModelIndex &current, const QModelIndex&) override;
    void setSelectedTorrentsSuperSeeding(bool enabled) const;
    void setSelectedTorrentsSequentialDownload(bool enabled) const;
    void setSelectedFirstLastPiecePrio(bool enabled) const;
    void setSelectedAutoTMMEnabled(bool enabled) const;
    void askNewCategoryForSelection();
    void saveSettings();

signals:
    void currentTorrentChanged(BitTorrent::TorrentHandle *const torrent);

private:
    void wheelEvent(QWheelEvent *event) override;
    void askAddTagsForSelection();
    void editTorrentTrackers();
    void confirmRemoveAllTagsForSelection();
    QStringList askTagsForSelection(const QString &dialogTitle);
    void applyToSelectedTorrents(const std::function<void (BitTorrent::TorrentHandle *const)> &fn);

    // supposed to be used with qss only
    QColor unknownStateForeground() const;
    QColor forcedDownloadingStateForeground() const;
    QColor downloadingStateForeground() const;
    QColor downloadingMetadataStateForeground() const;
    QColor allocatingStateForeground() const;
    QColor stalledDownloadingStateForeground() const;
    QColor forcedUploadingStateForeground() const;
    QColor uploadingStateForeground() const;
    QColor stalledUploadingStateForeground() const;
    QColor checkingResumeDataStateForeground() const;
    QColor queuedDownloadingStateForeground() const;
    QColor queuedUploadingStateForeground() const;
    QColor checkingUploadingStateForeground() const;
    QColor checkingDownloadingStateForeground() const;
    QColor pausedDownloadingStateForeground() const;
    QColor pausedUploadingStateForeground() const;
    QColor movingStateForeground() const;
    QColor missingFilesStateForeground() const;
    QColor errorStateForeground() const;

    void setUnknownStateForeground(const QColor &color);
    void setForcedDownloadingStateForeground(const QColor &color);
    void setDownloadingStateForeground(const QColor &color);
    void setDownloadingMetadataStateForeground(const QColor &color);
    void setAllocatingStateForeground(const QColor &color);
    void setStalledDownloadingStateForeground(const QColor &color);
    void setForcedUploadingStateForeground(const QColor &color);
    void setUploadingStateForeground(const QColor &color);
    void setStalledUploadingStateForeground(const QColor &color);
    void setCheckingResumeDataStateForeground(const QColor &color);
    void setQueuedDownloadingStateForeground(const QColor &color);
    void setQueuedUploadingStateForeground(const QColor &color);
    void setCheckingUploadingStateForeground(const QColor &color);
    void setCheckingDownloadingStateForeground(const QColor &color);
    void setPausedDownloadingStateForeground(const QColor &color);
    void setPausedUploadingStateForeground(const QColor &color);
    void setMovingStateForeground(const QColor &color);
    void setMissingFilesStateForeground(const QColor &color);
    void setErrorStateForeground(const QColor &color);

    TransferListDelegate *m_listDelegate;
    TransferListModel *m_listModel;
    TransferListSortModel *m_sortFilterModel;
    MainWindow *m_mainWindow;
};

#endif // TRANSFERLISTWIDGET_H
