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

#include <QTime>
#include <QDateTime>

#include "core/preferences.h"
#include "bandwidthscheduler.h"

BandwidthScheduler::BandwidthScheduler(QObject *parent)
    : QTimer(parent)
{
    Q_ASSERT(Preferences::instance()->isSchedulerEnabled());
    // Single shot, we call start() again manually
    setSingleShot(true);
    // Connect Signals/Slots
    connect(this, SIGNAL(timeout()), this, SLOT(start()));
}

void BandwidthScheduler::start()
{
    const Preferences* const pref = Preferences::instance();
    Q_ASSERT(pref->isSchedulerEnabled());
    bool alt_bw_enabled = pref->isAltBandwidthEnabled();

    QTime start = pref->getSchedulerStartTime();
    QTime end = pref->getSchedulerEndTime();
    QTime now = QTime::currentTime();
    int sched_days = pref->getSchedulerDays();
    int day = QDateTime::currentDateTime().toLocalTime().date().dayOfWeek();
    bool new_mode = false;
    bool reverse = false;

    if (start > end) {
        QTime temp = start;
        start = end;
        end = temp;
        reverse = true;
    }

    if ((start <= now) && (end >= now)) {
        switch(sched_days) {
        case EVERY_DAY:
            new_mode = true;
            break;
        case WEEK_ENDS:
            if ((day == 6) || (day == 7))
                new_mode = true;
            break;
        case WEEK_DAYS:
            if ((day != 6) && (day != 7))
                new_mode = true;
            break;
        default:
            if (day == (sched_days - 2))
                new_mode = true;
        }
    }

    if (reverse)
        new_mode = !new_mode;

    if (new_mode != alt_bw_enabled)
        emit switchToAlternativeMode(new_mode);

    // Timeout regularly to accommodate for external system clock changes
    // eg from the user or from a timesync utility
    QTimer::start(1500);
}
