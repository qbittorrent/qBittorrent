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

#ifndef CREATE_TORRENT_IMP_H
#define CREATE_TORRENT_IMP_H

#include "ui_torrentcreatordlg.h"

namespace BitTorrent
{
    class TorrentCreatorThread;
}

class TorrentCreatorDlg : public QDialog, private Ui::createTorrentDialog
{
    Q_OBJECT

public:
    TorrentCreatorDlg(QWidget *parent = 0);
    ~TorrentCreatorDlg();
    int getPieceSize() const;

public slots:
    void updateProgressBar(int progress);
    void on_cancelButton_clicked();

protected slots:
    void on_createButton_clicked();
    void on_addFile_button_clicked();
    void on_addFolder_button_clicked();
    void handleCreationFailure(QString msg);
    void handleCreationSuccess(QString path, QString branch_path);
    void setInteractionEnabled(bool enabled);
    void showProgressBar(bool show);
    void on_checkAutoPieceSize_clicked(bool checked);
    void updateOptimalPieceSize();
    void saveTrackerList();
    void loadTrackerList();

protected:
    void closeEvent(QCloseEvent *event);

private:
    void saveSettings();
    void loadSettings();

private:
    BitTorrent::TorrentCreatorThread *m_creatorThread;
    QList<int> m_pieceSizes;
};

#endif
