/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022  Mike Tzou (Chocobo1)
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

#include <algorithm>

#include <QTest>
#include <QVector>

#include "base/bittorrent/trackerentry.h"
#include "base/global.h"

class TestBittorrentTrackerEntry final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestBittorrentTrackerEntry)

public:
    TestBittorrentTrackerEntry() = default;

private slots:
    void testParseTrackerEntries() const
    {
        using Entries = QVector<BitTorrent::TrackerEntry>;

        const auto isEqual = [](const Entries &left, const Entries &right) -> bool
        {
            return std::equal(left.begin(), left.end(), right.begin(), right.end()
                , [](const BitTorrent::TrackerEntry &leftEntry, const BitTorrent::TrackerEntry &rightEntry)
            {
                return (leftEntry.url == rightEntry.url)
                    && (leftEntry.tier == rightEntry.tier);
            });
        };

        {
            const QString input;
            const Entries output;
            QVERIFY(isEqual(BitTorrent::parseTrackerEntries(input), output));
        }

        {
            const QString input = u"http://localhost:1234"_qs;
            const Entries output = {{u"http://localhost:1234"_qs, 0}};
            QVERIFY(isEqual(BitTorrent::parseTrackerEntries(input), output));
        }

        {
            const QString input = u"  http://localhost:1234     "_qs;
            const Entries output = {{u"http://localhost:1234"_qs, 0}};
            QVERIFY(isEqual(BitTorrent::parseTrackerEntries(input), output));
        }

        {
            const QString input = u"\nhttp://localhost:1234"_qs;
            const Entries output = {{u"http://localhost:1234"_qs, 1}};
            QVERIFY(isEqual(BitTorrent::parseTrackerEntries(input), output));
        }

        {
            const QString input = u"http://localhost:1234\n"_qs;
            const Entries output = {{u"http://localhost:1234"_qs, 0}};
            QVERIFY(isEqual(BitTorrent::parseTrackerEntries(input), output));
        }

        {
            const QString input = u"http://localhost:1234 \n http://[::1]:4567"_qs;
            const Entries output =
            {
                {u"http://localhost:1234"_qs, 0},
                {u"http://[::1]:4567"_qs, 0}
            };
            QVERIFY(isEqual(BitTorrent::parseTrackerEntries(input), output));
        }

        {
            const QString input = u"\n http://localhost:1234 \n http://[::1]:4567"_qs;
            const Entries output =
            {
                {u"http://localhost:1234"_qs, 1},
                {u"http://[::1]:4567"_qs, 1}
            };
            QVERIFY(isEqual(BitTorrent::parseTrackerEntries(input), output));
        }

        {
            const QString input = u"http://localhost:1234 \n http://[::1]:4567 \n \n \n"_qs;
            const Entries output =
            {
                {u"http://localhost:1234"_qs, 0},
                {u"http://[::1]:4567"_qs, 0}
            };
            QVERIFY(isEqual(BitTorrent::parseTrackerEntries(input), output));
        }

        {
            const QString input = u"http://localhost:1234 \n \n http://[::1]:4567"_qs;
            const Entries output =
            {
                {u"http://localhost:1234"_qs, 0},
                {u"http://[::1]:4567"_qs, 1}
            };
            QVERIFY(isEqual(BitTorrent::parseTrackerEntries(input), output));
        }

        {
            const QString input = u"\n \n \n http://localhost:1234 \n \n \n \n http://[::1]:4567 \n \n \n"_qs;
            const Entries output =
            {
                {u"http://localhost:1234"_qs, 3},
                {u"http://[::1]:4567"_qs, 6}
            };
            QVERIFY(isEqual(BitTorrent::parseTrackerEntries(input), output));
        }
    }
};

QTEST_APPLESS_MAIN(TestBittorrentTrackerEntry)
#include "testbittorrenttrackerentry.moc"
