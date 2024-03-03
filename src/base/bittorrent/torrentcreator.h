/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2024  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2024  Radu Carpa <radu.carpa@cern.ch>
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

#include <atomic>

#include <QObject>
#include <QRunnable>
#include <QStringList>

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
        bool isPrivate = false;
#ifdef QBT_USES_LIBTORRENT2
        TorrentFormat torrentFormat = TorrentFormat::Hybrid;
#else
        bool isAlignmentOptimized = false;
        int paddedFileSizeLimit = 0;
#endif
        int pieceSize = 0;
        Path sourcePath;
        Path torrentFilePath;
        QString comment;
        QString source;
        QStringList trackers;
        QStringList urlSeeds;
    };

    struct TorrentCreatorResult
    {
        Path torrentFilePath;
        Path savePath;
        int pieceSize;
    };

    class TorrentCreator final : public QObject, public QRunnable
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(TorrentCreator)

    public:
        explicit TorrentCreator(const TorrentCreatorParams &params, QObject *parent = nullptr);

        const TorrentCreatorParams &params() const;
        bool isInterruptionRequested() const;

        void run() override;

#ifdef QBT_USES_LIBTORRENT2
        static int calculateTotalPieces(const Path &inputPath, int pieceSize, TorrentFormat torrentFormat);
#else
        static int calculateTotalPieces(const Path &inputPath, const int pieceSize
                , const bool isAlignmentOptimized, int paddedFileSizeLimit);
#endif

    public slots:
        void requestInterruption();

    signals:
        void started();
        void creationFailure(const QString &msg);
        void creationSuccess(const TorrentCreatorResult &result);
        void progressUpdated(int progress);

    private:
        void sendProgressSignal(int currentPieceIdx, int totalPieces);
        void checkInterruptionRequested() const;

        TorrentCreatorParams m_params;
        std::atomic_bool m_interruptionRequested;
    };
}
