/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  qBittorrent contributors
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

#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QTest>

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/entry.hpp>
#ifdef QBT_USES_LIBTORRENT2
#include <libtorrent/load_torrent.hpp>
#else
#include <libtorrent/torrent_info.hpp>
#endif

#include "base/bittorrent/archivedtorrent.h"
#include "base/bittorrent/torrent.h"
#include "base/global.h"

namespace
{
    constexpr int PIECE_SIZE = 16 * 1024;
    constexpr int PIECE_COUNT = 3;

    BitTorrent::LoadTorrentParams makeResumeData()
    {
        lt::entry torrentInfo;
        torrentInfo["name"] = "test.bin";
        torrentInfo["piece length"] = PIECE_SIZE;
        torrentInfo["length"] = static_cast<qint64>(PIECE_SIZE) * PIECE_COUNT;
        torrentInfo["pieces"] = std::string(20 * PIECE_COUNT, '\0');

        lt::entry torrentEntry;
        torrentEntry["info"] = std::move(torrentInfo);

        std::vector<char> buffer;
        lt::bencode(std::back_inserter(buffer), torrentEntry);

        BitTorrent::LoadTorrentParams resumeData;
#ifdef QBT_USES_LIBTORRENT2
        resumeData.ltAddTorrentParams = lt::load_torrent_buffer(
                lt::span<char const> {buffer.data(), static_cast<std::ptrdiff_t>(buffer.size())});
        resumeData.ltAddTorrentParams.info_hashes = resumeData.ltAddTorrentParams.ti->info_hashes();
#else
        const auto nativeInfo = std::make_shared<lt::torrent_info>(buffer.data(), static_cast<int>(buffer.size()));
        resumeData.ltAddTorrentParams.ti = nativeInfo;
        resumeData.ltAddTorrentParams.info_hash = nativeInfo->info_hash();
#endif
        return resumeData;
    }
}

class TestBittorrentArchivedTorrent final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestBittorrentArchivedTorrent)

public:
    TestBittorrentArchivedTorrent() = default;

private slots:
    void testWithoutMetadata() const
    {
        const BitTorrent::TorrentID torrentID = BitTorrent::TorrentID::fromString(
                u"c58645e2e922428dceb1f98f51ffa424810570f0"_s);

        BitTorrent::LoadTorrentParams resumeData;
        resumeData.name = u"Magnet name"_s;
        resumeData.category = u"Category"_s;
        resumeData.tags.insert(Tag(u"Tag"_s));
        resumeData.comment = u"Comment"_s;
        resumeData.ltAddTorrentParams.total_uploaded = 1000;
        resumeData.ltAddTorrentParams.total_downloaded = 0;
        resumeData.ltAddTorrentParams.active_time = 42;
#if LIBTORRENT_VERSION_NUM >= 20100
        resumeData.ltAddTorrentParams.creation_date = 1234567890;
#endif

        const BitTorrent::ArchivedTorrent torrent = BitTorrent::ArchivedTorrent::fromResumeData(torrentID, resumeData);

        QCOMPARE(torrent.id, torrentID);
        QCOMPARE(torrent.name, resumeData.name);
        QCOMPARE(torrent.category, resumeData.category);
        QVERIFY(torrent.tags.contains(Tag(u"Tag"_s)));
        QCOMPARE(torrent.comment, resumeData.comment);
        QCOMPARE(torrent.hasMetadata, false);
        QCOMPARE(torrent.totalSize, 0);
        QCOMPARE(torrent.completedSize, 0);
        QCOMPARE(torrent.progress, 0);
        QCOMPARE(torrent.totalDownload, 0);
        QCOMPARE(torrent.totalUpload, 1000);
        QCOMPARE(torrent.activeTime, 42);
        QCOMPARE(torrent.ratio, BitTorrent::Torrent::MAX_RATIO);
#if LIBTORRENT_VERSION_NUM >= 20100
        QCOMPARE(torrent.creationDate, QDateTime::fromSecsSinceEpoch(1234567890));
#else
        QVERIFY(!torrent.creationDate.isValid());
#endif
    }

    void testWithMetadata() const
    {
        const BitTorrent::TorrentID torrentID = BitTorrent::TorrentID::fromString(
                u"c58645e2e922428dceb1f98f51ffa424810570f0"_s);

        BitTorrent::LoadTorrentParams resumeData = makeResumeData();
        resumeData.name = u"Custom name"_s;
        resumeData.savePath = Path(u"/ignored/save/path"_s);

        lt::add_torrent_params &params = resumeData.ltAddTorrentParams;
        params.save_path = "/actual/save/path";
        params.have_pieces = decltype(params.have_pieces)(PIECE_COUNT, false);
        params.have_pieces.set_bit(lt::piece_index_t {0});
        params.have_pieces.set_bit(lt::piece_index_t {2});
        params.total_uploaded = 32768;
        params.total_downloaded = 16384;
        params.active_time = 100;
        params.finished_time = 50;
        params.added_time = 1000;
        params.completed_time = 2000;

        const BitTorrent::ArchivedTorrent torrent = BitTorrent::ArchivedTorrent::fromResumeData(torrentID, resumeData);

        QCOMPARE(torrent.name, resumeData.name);
        QCOMPARE(torrent.hasMetadata, true);
        QCOMPARE(torrent.totalSize, static_cast<qint64>(PIECE_SIZE) * PIECE_COUNT);
        QCOMPARE(torrent.completedSize, static_cast<qint64>(PIECE_SIZE) * 2);
        QVERIFY(qFuzzyCompare(torrent.progress, 2.0 / 3.0));
        QVERIFY(qFuzzyCompare(torrent.ratio, 2.0));
        QCOMPARE(torrent.addedTime, QDateTime::fromSecsSinceEpoch(1000));
        QCOMPARE(torrent.completedTime, QDateTime::fromSecsSinceEpoch(2000));
        QCOMPARE(torrent.savePath, Path(u"/actual/save/path"_s));
    }

    void testRatioWithLostDownloadStats() const
    {
        BitTorrent::LoadTorrentParams resumeData = makeResumeData();
        resumeData.ltAddTorrentParams.have_pieces = decltype(resumeData.ltAddTorrentParams.have_pieces)(PIECE_COUNT, true);

        lt::add_torrent_params &params = resumeData.ltAddTorrentParams;
        params.total_uploaded = static_cast<qint64>(PIECE_SIZE) * PIECE_COUNT;
        params.total_downloaded = 100;

        const BitTorrent::ArchivedTorrent torrent = BitTorrent::ArchivedTorrent::fromResumeData(
                BitTorrent::TorrentID::fromString(u"c58645e2e922428dceb1f98f51ffa424810570f0"_s), resumeData);

        QVERIFY(qFuzzyCompare(torrent.progress, 1.0));
        QVERIFY(qFuzzyCompare(torrent.ratio, 1.0));
    }

    void testSeedModeWithoutPieces() const
    {
        BitTorrent::LoadTorrentParams resumeData = makeResumeData();
        lt::add_torrent_params &params = resumeData.ltAddTorrentParams;
        params.flags |= lt::torrent_flags::seed_mode;
        params.total_uploaded = static_cast<qint64>(PIECE_SIZE) * PIECE_COUNT;

        const BitTorrent::ArchivedTorrent torrent = BitTorrent::ArchivedTorrent::fromResumeData(
                BitTorrent::TorrentID::fromString(u"c58645e2e922428dceb1f98f51ffa424810570f0"_s), resumeData);

        QVERIFY(qFuzzyCompare(torrent.progress, 1.0));
        QCOMPARE(torrent.completedSize, static_cast<qint64>(PIECE_SIZE) * PIECE_COUNT);
        QVERIFY(qFuzzyCompare(torrent.ratio, 1.0));
    }
};

QTEST_APPLESS_MAIN(TestBittorrentArchivedTorrent)
#include "testbittorrentarchivedtorrent.moc"
