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

#ifndef STATUSBAR_H
#define STATUSBAR_H

#include <QObject>

class QStatusBar;
class QFrame;
class QLabel;
class QPushButton;
class QHBoxLayout;

namespace BitTorrent
{
    struct SessionStatus;
}

class StatusBar: public QObject
{
    Q_OBJECT

public:
    StatusBar(QStatusBar *bar);
    ~StatusBar();

    QPushButton* connectionStatusButton() const;

public slots:
    void showRestartRequired();
    void refreshStatusBar();
    void updateAltSpeedsBtn(bool alternative);
    void toggleAlternativeSpeeds();
    void capDownloadSpeed();
    void capUploadSpeed();

private:
    void updateConnectionStatus();
    void updateDHTNodesNumber();
    void updateSpeedLabels();
    void updateFreeSpaceOnDisk();

    QStatusBar *m_bar;
    QPushButton *m_dlSpeedLbl;
    QPushButton *m_upSpeedLbl;
    QLabel *m_DHTLbl;
    QLabel *m_freeSpaceOnDiskLbl;
    QFrame *m_statusSep1;
    QFrame *m_statusSep2;
    QFrame *m_statusSep3;
    QFrame *m_statusSep4;
    QFrame *m_statusSep5;
    QPushButton *m_connecStatusLblIcon;
    QPushButton *m_altSpeedsBtn;
    QWidget *m_container;
    QHBoxLayout *m_layout;
};

#endif // STATUSBAR_H
