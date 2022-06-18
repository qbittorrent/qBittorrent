/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
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

#include "statistics.h"

#include <chrono>

#include <QTimer>

#include "base/global.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/sessionstatus.h"
#include "base/profile.h"

using namespace std::chrono_literals;
using namespace BitTorrent;

const qint64 SAVE_INTERVAL = std::chrono::milliseconds(15min).count();

Statistics::Statistics(Session *session)
    : QObject(session)
    , m_session(session)
{
    load();
    m_lastUpdateTimer.start();

    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &Statistics::gather);
    timer->start(60s);
}

Statistics::~Statistics()
{
    save();
}

qint64 Statistics::getAlltimeDL() const
{
    return m_alltimeDL + m_sessionDL;
}

qint64 Statistics::getAlltimeUL() const
{
    return m_alltimeUL + m_sessionUL;
}

void Statistics::gather()
{
    const SessionStatus &ss = m_session->status();
    if (ss.totalDownload > m_sessionDL)
    {
        m_sessionDL = ss.totalDownload;
        m_dirty = true;
    }
    if (ss.totalUpload > m_sessionUL)
    {
        m_sessionUL = ss.totalUpload;
        m_dirty = true;
    }

    if (m_lastUpdateTimer.hasExpired(SAVE_INTERVAL))
        save();
}

void Statistics::save() const
{
    if (!m_dirty)
        return;

    const QVariantHash stats =
    {
        {u"AlltimeDL"_qs, (m_alltimeDL + m_sessionDL)},
        {u"AlltimeUL"_qs, (m_alltimeUL + m_sessionUL)}
    };
    SettingsPtr settings = Profile::instance()->applicationSettings(u"qBittorrent-data"_qs);
    settings->setValue(u"Stats/AllStats"_qs, stats);

    m_lastUpdateTimer.start();
    m_dirty = false;
}

void Statistics::load()
{
    const SettingsPtr s = Profile::instance()->applicationSettings(u"qBittorrent-data"_qs);
    const QVariantHash v = s->value(u"Stats/AllStats"_qs).toHash();

    m_alltimeDL = v[u"AlltimeDL"_qs].toLongLong();
    m_alltimeUL = v[u"AlltimeUL"_qs].toLongLong();
}
