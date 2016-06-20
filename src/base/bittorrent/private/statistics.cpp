#include <QDateTime>

#include <libtorrent/session.hpp>

#include "base/qinisettings.h"
#include "base/bittorrent/sessionstatus.h"
#include "base/bittorrent/session.h"
#include "statistics.h"

static const qint64 SAVE_INTERVAL = 15 * 60 * 1000;

namespace libt = libtorrent;
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
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(gather()));
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
    SessionStatus ss = m_session->status();
    if (ss.totalDownload() > m_sessionDL) {
        m_sessionDL = ss.totalDownload();
        m_dirty = true;
    }
    if (ss.totalUpload() > m_sessionUL) {
        m_sessionUL = ss.totalUpload();
        m_dirty = true;
    }

    save();
}

void Statistics::save() const
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (!m_dirty || ((now - m_lastWrite) < SAVE_INTERVAL))
        return;

    QIniSettings s("qBittorrent", "qBittorrent-data");
    QVariantHash v;
    v.insert("AlltimeDL", m_alltimeDL + m_sessionDL);
    v.insert("AlltimeUL", m_alltimeUL + m_sessionUL);
    s.setValue("Stats/AllStats", v);
    m_dirty = false;
    m_lastWrite = now;
}

void Statistics::load()
{
    QIniSettings s("qBittorrent", "qBittorrent-data");
    QVariantHash v = s.value("Stats/AllStats").toHash();

    m_alltimeDL = v["AlltimeDL"].toULongLong();
    m_alltimeUL = v["AlltimeUL"].toULongLong();
}
