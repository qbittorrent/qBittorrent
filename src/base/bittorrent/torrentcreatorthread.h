/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2010  Christophe Dumez <chris@qbittorrent.org>
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

#include <QStringList>
#include <QThread>

#include "base/path.h"

namespace BitTorrent
{
#ifdef QBT_USES_LIBTORRENT2
    enum class TorrentFormat
    {
        V1,
        V2,
        Hybrid
    };
#endif

    struct TorrentCreatorParams
    {
        bool isPrivate;
#ifdef QBT_USES_LIBTORRENT2
        TorrentFormat torrentFormat;
#else
        bool isAlignmentOptimized;
        int paddedFileSizeLimit;
#endif
        int pieceSize;
        Path inputPath;
        Path savePath;
        QString comment;
        QString source;
        QStringList trackers;
        QStringList urlSeeds;
    };

    class TorrentCreatorThread final : public QThread
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(TorrentCreatorThread)

    public:
        explicit TorrentCreatorThread(QObject *parent = nullptr);
        ~TorrentCreatorThread() override;

        void create(const TorrentCreatorParams &params);

#ifdef QBT_USES_LIBTORRENT2
        static int calculateTotalPieces(const Path &inputPath, const int pieceSize, const TorrentFormat torrentFormat);
#else
        static int calculateTotalPieces(const Path &inputPath
            , const int pieceSize, const bool isAlignmentOptimized, int paddedFileSizeLimit);
#endif

    signals:
        void creationFailure(const QString &msg);
        void creationSuccess(const Path &path, const Path &branchPath);
        void updateProgress(int progress);

    private:
        void run() override;
        void sendProgressSignal(int currentPieceIdx, int totalPieces);
        void checkInterruptionRequested() const;

        TorrentCreatorParams m_params;
    };
}
