/*
 * Bittorrent Client using Qt and libtorrent.
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

#ifndef BITTORRENT_TORRENTCREATORTHREAD_H
#define BITTORRENT_TORRENTCREATORTHREAD_H

#include <QThread>
#include <QStringList>

namespace BitTorrent
{
    class TorrentCreatorThread : public QThread
    {
        Q_OBJECT

    public:
        TorrentCreatorThread(QObject *parent = 0);
        ~TorrentCreatorThread();

        void create(const QString &inputPath, const QString &savePath, const QStringList &trackers,
                    const QStringList &urlSeeds, const QString &comment, bool isPrivate, int pieceSize);
        void abortCreation();

    protected:
        void run();

    signals:
        void creationFailure(const QString &msg);
        void creationSuccess(const QString &path, const QString &branchPath);
        void updateProgress(int progress);

    private:
        void sendProgressSignal(int numHashes, int numPieces);

        QString m_inputPath;
        QString m_savePath;
        QStringList m_trackers;
        QStringList m_urlSeeds;
        QString m_comment;
        bool m_private;
        int m_pieceSize;
        bool m_abort;
    };
}

#endif // BITTORRENT_TORRENTCREATORTHREAD_H
