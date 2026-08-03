/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Andy Ye
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

#include <QObject>
#include <QTest>

#include "base/bittorrent/torrentdescriptor.h"
#include "base/bittorrent/trackerentry.h"
#include "base/global.h"

class TestBittorrentTorrentDescriptor final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestBittorrentTorrentDescriptor)

public:
    TestBittorrentTorrentDescriptor() = default;

private slots:
    void testParseMagnetFiltersUnsupportedTrackers() const
    {
        const QString magnet =
                u"magnet:?xt=urn:btih:c58645e2e922428dceb1f98f51ffa424810570f0"
                "&tr=http%3A%2F%2Ftracker.example.com%2Fannounce"
                "&tr=udp%3A%2F%2Ftracker.example.com%3A1337%2Fannounce"
                "&tr=%3C!DOCTYPE%20html%3E%3Chtml%3E"
                "&tr=ftp%3A%2F%2Ftracker.example.com%2Fannounce"
                "&tr=313131"_s;

        const auto parseResult = BitTorrent::TorrentDescriptor::parse(magnet);
        QVERIFY(parseResult);

        const QList<BitTorrent::TrackerEntry> trackers = parseResult.value().trackers();
        QCOMPARE(trackers.size(), 2);
        QCOMPARE(trackers.at(0).url, u"http://tracker.example.com/announce"_s);
        QCOMPARE(trackers.at(1).url, u"udp://tracker.example.com:1337/announce"_s);

        const lt::add_torrent_params &nativeParams = parseResult.value().ltAddTorrentParams();
        QCOMPARE(nativeParams.trackers.size(), 2);
        QCOMPARE(nativeParams.tracker_tiers.size(), 2);
    }
};

QTEST_APPLESS_MAIN(TestBittorrentTorrentDescriptor)
#include "testbittorrenttorrentdescriptor.moc"
