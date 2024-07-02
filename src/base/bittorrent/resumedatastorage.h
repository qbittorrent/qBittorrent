/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2022  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QtContainerFwd>
#include <QList>
#include <QMutex>
#include <QObject>

#include "base/3rdparty/expected.hpp"
#include "base/path.h"
#include "infohash.h"
#include "loadtorrentparams.h"

namespace BitTorrent
{
    using LoadResumeDataResult = nonstd::expected<LoadTorrentParams, QString>;

    struct LoadedResumeData
    {
        TorrentID torrentID;
        LoadResumeDataResult result;
    };

    class ResumeDataStorage : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(ResumeDataStorage)

    public:
        explicit ResumeDataStorage(const Path &path, QObject *parent = nullptr);

        Path path() const;

        virtual QList<TorrentID> registeredTorrents() const = 0;
        virtual LoadResumeDataResult load(const TorrentID &id) const = 0;
        virtual void store(const TorrentID &id, const LoadTorrentParams &resumeData) const = 0;
        virtual void remove(const TorrentID &id) const = 0;
        virtual void storeQueue(const QList<TorrentID> &queue) const = 0;

        void loadAll() const;
        QList<LoadedResumeData> fetchLoadedResumeData() const;

    signals:
        void loadStarted(const QList<BitTorrent::TorrentID> &torrents);
        void loadFinished();

    protected:
        void onResumeDataLoaded(const TorrentID &torrentID, const LoadResumeDataResult &loadResumeDataResult) const;

    private:
        virtual void doLoadAll() const = 0;

        const Path m_path;
        mutable QList<LoadedResumeData> m_loadedResumeData;
        mutable QMutex m_loadedResumeDataMutex;
    };
}
