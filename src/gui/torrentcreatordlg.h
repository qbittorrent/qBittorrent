/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2010  Christophe Dumez
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

#ifndef TORRENTCREATORDLG_H
#define TORRENTCREATORDLG_H

#include <QDialog>

namespace Ui
{
    class TorrentCreatorDlg;
}

namespace BitTorrent
{
    class TorrentCreatorThread;
}

class TorrentCreatorDlg: public QDialog
{
    Q_OBJECT

public:
    TorrentCreatorDlg(QWidget *parent = 0, const QString &defaultPath = QString());
    ~TorrentCreatorDlg();

private slots:
    void updateProgressBar(int progress);
    void onCreateButtonClicked();
    void onAddFileButtonClicked();
    void onAddFolderButtonClicked();
    void handleCreationFailure(const QString &msg);
    void handleCreationSuccess(const QString &path, const QString &branchPath);

private:
    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;

    void saveSettings();
    void loadSettings();
    int getPieceSize() const;
    void showProgressBar(bool show);
    void setInteractionEnabled(bool enabled);

    // settings storage
    QSize getDialogSize() const;
    void setDialogSize(const QSize &size);
    int getSettingPieceSize() const;
    void setSettingPieceSize(const int size);
    bool getSettingPrivateTorrent() const;
    void setSettingPrivateTorrent(const bool b);
    bool getSettingStartSeeding() const;
    void setSettingStartSeeding(const bool b);
    bool getSettingIgnoreRatio() const;
    void setSettingIgnoreRatio(const bool ignore);
    QString getLastAddPath() const;
    void setLastAddPath(const QString &path);
    QString getTrackerList() const;
    void setTrackerList(const QString &list);
    QString getWebSeedList() const;
    void setWebSeedList(const QString &list);
    QString getComments() const;
    void setComments(const QString &str);
    QString getLastSavePath() const;
    void setLastSavePath(const QString &path);

    Ui::TorrentCreatorDlg *m_ui;
    BitTorrent::TorrentCreatorThread *m_creatorThread;
    QString m_defaultPath;
};

#endif
