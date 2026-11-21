/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  1983lynx <149282465+1983lynx@users.noreply.github.com>
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

#include <QList>
#include <QObject>
#include <QTest>

#include "base/bittorrent/trackerentrystatus.h"
#include "base/bittorrent/trackersummary.h"
#include "base/global.h"

using BitTorrent::TrackerEndpointState;
using BitTorrent::TrackerEntryStatus;
using BitTorrent::TrackerSummaryStatus;

namespace
{
    TrackerEntryStatus makeEntry(const TrackerEndpointState state, const bool isUpdating = false, const QString &message = {})
    {
        TrackerEntryStatus entry;
        entry.url = u"http://tracker.example/announce"_s;
        entry.state = state;
        entry.isUpdating = isUpdating;
        entry.message = message;
        return entry;
    }
}

class TestBittorrentTrackerSummary final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestBittorrentTrackerSummary)

public:
    TestBittorrentTrackerSummary() = default;

private slots:
    void testTrackerSummaryStatus() const
    {
        using List = QList<TrackerEntryStatus>;

        QCOMPARE(BitTorrent::trackerSummaryStatus(List()), TrackerSummaryStatus::NoTrackers);
        QCOMPARE(BitTorrent::trackerSummaryStatus(List {makeEntry(TrackerEndpointState::NotContacted)})
                , TrackerSummaryStatus::NotContacted);
        QCOMPARE(BitTorrent::trackerSummaryStatus(List {makeEntry(TrackerEndpointState::Working)})
                , TrackerSummaryStatus::Working);
        QCOMPARE(BitTorrent::trackerSummaryStatus(List {makeEntry(TrackerEndpointState::Working, false, u"message"_s)})
                , TrackerSummaryStatus::Warning);
        QCOMPARE(BitTorrent::trackerSummaryStatus(List {makeEntry(TrackerEndpointState::NotWorking)})
                , TrackerSummaryStatus::NotWorking);
        QCOMPARE(BitTorrent::trackerSummaryStatus(List {makeEntry(TrackerEndpointState::Unreachable)})
                , TrackerSummaryStatus::Unreachable);
        QCOMPARE(BitTorrent::trackerSummaryStatus(List {makeEntry(TrackerEndpointState::TrackerError)})
                , TrackerSummaryStatus::TrackerError);

        // announce in progress is shown first
        QCOMPARE(BitTorrent::trackerSummaryStatus(List {makeEntry(TrackerEndpointState::TrackerError)
                , makeEntry(TrackerEndpointState::NotContacted, true)})
                , TrackerSummaryStatus::Updating);

        // tracker error wins over working trackers
        QCOMPARE(BitTorrent::trackerSummaryStatus(List {makeEntry(TrackerEndpointState::Working)
                , makeEntry(TrackerEndpointState::TrackerError)})
                , TrackerSummaryStatus::TrackerError);

        // unreachable/not working trackers are not reported while another tracker works
        QCOMPARE(BitTorrent::trackerSummaryStatus(List {makeEntry(TrackerEndpointState::Working)
                , makeEntry(TrackerEndpointState::Unreachable), makeEntry(TrackerEndpointState::NotWorking)})
                , TrackerSummaryStatus::Working);
        QCOMPARE(BitTorrent::trackerSummaryStatus(List {makeEntry(TrackerEndpointState::Unreachable)
                , makeEntry(TrackerEndpointState::NotWorking)})
                , TrackerSummaryStatus::Unreachable);
        QCOMPARE(BitTorrent::trackerSummaryStatus(List {makeEntry(TrackerEndpointState::NotContacted)
                , makeEntry(TrackerEndpointState::NotWorking)})
                , TrackerSummaryStatus::NotWorking);
    }

    void testTrackerSummaryStatusText() const
    {
        for (int i = static_cast<int>(TrackerSummaryStatus::NoTrackers); i <= static_cast<int>(TrackerSummaryStatus::TrackerError); ++i)
            QVERIFY(!BitTorrent::trackerSummaryStatusText(static_cast<TrackerSummaryStatus>(i)).isEmpty());
    }

    void testTrackerSummaryDetails() const
    {
        const QList<TrackerEntryStatus> trackers {makeEntry(TrackerEndpointState::Working, false, u"hello"_s)
                , makeEntry(TrackerEndpointState::Unreachable)};
        const QStringList lines = BitTorrent::trackerSummaryDetails(trackers).split(u'\n');
        QCOMPARE(lines.size(), 2);
        QVERIFY(lines[0].startsWith(u"http://tracker.example/announce"_s));
        QVERIFY(lines[0].endsWith(u"(hello)"_s));
    }
};

QTEST_APPLESS_MAIN(TestBittorrentTrackerSummary)
#include "testbittorrenttrackersummary.moc"
