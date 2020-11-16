/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Vladimir Golovnev <glassez@yandex.ru>
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

#include "bandwidthscheduler.h"

#include <utility>

#include <QDate>
#include <QTime>
#include <QTimer>

#include "base/preferences.h"

BandwidthScheduler::BandwidthScheduler(QObject *parent)
    : QObject(parent)
    , m_lastAlternative(false)
{
    connect(&m_timer, &QTimer::timeout, this, &BandwidthScheduler::onTimeout);
}

void BandwidthScheduler::start()
{
    m_lastAlternative = isTimeForAlternative();
    emit bandwidthLimitRequested(m_lastAlternative);

    // Timeout regularly to accommodate for external system clock changes
    // eg from the user or from a timesync utility
    m_timer.start(30000);
}

bool BandwidthScheduler::isTimeForAlternative() const
{
    const Preferences *const pref = Preferences::instance();

    QTime start = pref->getSchedulerStartTime();
    QTime end = pref->getSchedulerEndTime();
    const QTime now = QTime::currentTime();
    const int schedulerDays = pref->getSchedulerDays();
    const int day = QDate::currentDate().dayOfWeek();
    bool alternative = false;

    if (start > end)
    {
        std::swap(start, end);
        alternative = true;
    }

    if ((start <= now) && (end >= now))
    {
        switch (schedulerDays)
        {
        case EVERY_DAY:
            alternative = !alternative;
            break;
        case WEEK_ENDS:
            if ((day == 6) || (day == 7))
                alternative = !alternative;
            break;
        case WEEK_DAYS:
            if ((day != 6) && (day != 7))
                alternative = !alternative;
            break;
        default:
            if (day == (schedulerDays - 2))
                alternative = !alternative;
        }
    }

    return alternative;
}

void BandwidthScheduler::onTimeout()
{
    const bool alternative = isTimeForAlternative();

    if (alternative != m_lastAlternative)
    {
        m_lastAlternative = alternative;
        emit bandwidthLimitRequested(alternative);
    }
}
