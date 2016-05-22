/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016 Eugene Shalygin
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

#ifndef DOWNLOADINGDEADLINEDIALOG_H
#define DOWNLOADINGDEADLINEDIALOG_H

#include <chrono>
#include <QDialog>

class QTime;

namespace Ui
{
    class DownloadingDeadlineDialog;
}

// This dialog allows user to compute appropriate downloading deadline for a torrent or file.
// Give the size of that entity (torrent or file), the dialog shows correlated values of bitrate
// and playtime, allowing to change both of them.
class DownloadingDeadlineDialog: public QDialog
{
    Q_OBJECT

public:
    explicit DownloadingDeadlineDialog(const QString &fileOrTorrentName,
                                       qlonglong fileOrTorrentSize, qlonglong remainingSize,
                                       QWidget *parent = 0);
    ~DownloadingDeadlineDialog();

    std::chrono::milliseconds downloadingDeadline() const;
    std::chrono::milliseconds downloadingDelay() const;
    bool onlyUncompletedPieces() const;

    void accept() override;

private slots:
    void speedChanged(int speed);
    void deadlineChanged(const QTime &deadline);
    void uncompletedOnlyClicked();

private:
    void setupSignals();

    qlonglong sizeToDownload() const;
    //  void set

    Ui::DownloadingDeadlineDialog *m_ui;
    qlonglong m_size;
    qlonglong m_remainingSize;
};

#endif // DOWNLOADINGDEADLINEDIALOG_H
