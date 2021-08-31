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

#include <QDateTime>

#include "base/bittorrent/session.h"
#include "base/bittorrent/sessionstatus.h"
#include "base/profile.h"

static const qint64 SAVE_INTERVAL = 15 * 60 * 1000;

using namespace BitTorrent;

Statistics::Statistics(Session *session)
    : QObject(session)
    , m_session(session)
    , m_sessionUL(0)
    , m_sessionDL(0)
    , m_lastWrite(0)
    , m_dirty(false)
{
    load();
    connect(&m_timer, &QTimer::timeout, this, &Statistics::gather);
    m_timer.start(60 * 1000);
}

Statistics::~Statistics()
{
    if (m_dirty)
        m_lastWrite = 0;
    save();
}

quint64 Statistics::getAlltimeDL() const
{
    return m_alltimeDL + m_sessionDL;
}

quint64 Statistics::getAlltimeUL() const
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

    save();
}

void Statistics::save() const
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (!m_dirty || ((now - m_lastWrite) < SAVE_INTERVAL))
        return;

    SettingsPtr s = Profile::instance()->applicationSettings(QLatin1String("qBittorrent-data"));
    QVariantHash v;
    v.insert("AlltimeDL", m_alltimeDL + m_sessionDL);
    v.insert("AlltimeUL", m_alltimeUL + m_sessionUL);
    s->setValue("Stats/AllStats", v);
    m_dirty = false;
    m_lastWrite = now;
}

void Statistics::load()
{
    const SettingsPtr s = Profile::instance()->applicationSettings(QLatin1String("qBittorrent-data"));
    const QVariantHash v = s->value("Stats/AllStats").toHash();

    m_alltimeDL = v["AlltimeDL"].toULongLong();
    m_alltimeUL = v["AlltimeUL"].toULongLong();
}
